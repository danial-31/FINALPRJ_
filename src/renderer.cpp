#include "renderer.hpp"
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <stb_image.h>
#include <algorithm>
#include <cstddef>
#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>

static std::string readFile(const fs::path& path){
    std::ifstream in(path,std::ios::binary);
    if(!in)throw std::runtime_error("Shader file not found: "+path.string());
    std::stringstream ss;ss<<in.rdbuf();return ss.str();
}
static GLuint compile(GLenum type,const fs::path& path){
    std::string source=readFile(path);const char* text=source.c_str();
    GLuint shader=glCreateShader(type);glShaderSource(shader,1,&text,nullptr);glCompileShader(shader);
    GLint ok=0;glGetShaderiv(shader,GL_COMPILE_STATUS,&ok);
    if(!ok){char log[8192];glGetShaderInfoLog(shader,sizeof(log),nullptr,log);glDeleteShader(shader);
        throw std::runtime_error("Shader compile error in "+path.filename().string()+":\n"+log);}
    return shader;
}
GLuint loadProgram(const fs::path& dir,const std::string& vertex,const std::string& fragment){
    GLuint v=compile(GL_VERTEX_SHADER,dir/vertex),f=compile(GL_FRAGMENT_SHADER,dir/fragment),p=glCreateProgram();
    glAttachShader(p,v);glAttachShader(p,f);glLinkProgram(p);glDeleteShader(v);glDeleteShader(f);
    GLint ok=0;glGetProgramiv(p,GL_LINK_STATUS,&ok);
    if(!ok){char log[8192];glGetProgramInfoLog(p,sizeof(log),nullptr,log);glDeleteProgram(p);
        throw std::runtime_error("Shader link error ("+vertex+" + "+fragment+"):\n"+log);}
    return p;
}
void setMatrix(GLuint p,const char* name,const glm::mat4& m){glUniformMatrix4fv(glGetUniformLocation(p,name),1,GL_FALSE,glm::value_ptr(m));}
void setVec3(GLuint p,const char* name,glm::vec3 v){glUniform3fv(glGetUniformLocation(p,name),1,glm::value_ptr(v));}
void setFloat(GLuint p,const char* name,float v){glUniform1f(glGetUniformLocation(p,name),v);}
void setInt(GLuint p,const char* name,int v){glUniform1i(glGetUniformLocation(p,name),v);}
void checkFramebuffer(const char* what){
    GLenum status=glCheckFramebufferStatus(GL_FRAMEBUFFER);
    if(status!=GL_FRAMEBUFFER_COMPLETE){
        std::ostringstream o;o<<what<<" framebuffer incomplete (status 0x"<<std::hex<<status<<")";
        throw std::runtime_error(o.str());
    }
}

GpuMesh::GpuMesh(){
    glGenVertexArrays(1,&vao);glGenBuffers(1,&vbo);glBindVertexArray(vao);glBindBuffer(GL_ARRAY_BUFFER,vbo);
    for(GLuint i=0;i<4;++i){glEnableVertexAttribArray(i);glVertexAttribPointer(i,i==3?1:3,GL_FLOAT,GL_FALSE,sizeof(Vertex),reinterpret_cast<void*>(size_t(i)*sizeof(glm::vec3)));}
}
GpuMesh::GpuMesh(const Mesh& m):GpuMesh(){update(m);}
GpuMesh::~GpuMesh(){glDeleteBuffers(1,&vbo);glDeleteVertexArrays(1,&vao);}
void GpuMesh::update(const Mesh& m){
    count=GLsizei(m.vertices.size());glBindBuffer(GL_ARRAY_BUFFER,vbo);
    glBufferData(GL_ARRAY_BUFFER,GLsizeiptr(m.vertices.size()*sizeof(Vertex)),m.vertices.empty()?nullptr:m.vertices.data(),GL_DYNAMIC_DRAW);
}
void GpuMesh::draw() const{if(count){glBindVertexArray(vao);glDrawArrays(GL_TRIANGLES,0,count);}}

bool Texture::load(const fs::path& path,bool repeat){
    int channels=0;auto pixels=stbi_load(path.string().c_str(),&width,&height,&channels,4);
    if(!pixels){std::cerr<<"Texture missing or unreadable: "<<path.string()<<" ("<<stbi_failure_reason()<<")\n";return false;}
    glGenTextures(1,&id);glBindTexture(GL_TEXTURE_2D,id);
    glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA,width,height,0,GL_RGBA,GL_UNSIGNED_BYTE,pixels);stbi_image_free(pixels);
    GLint wrap=repeat?GL_REPEAT:GL_CLAMP_TO_EDGE;
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,wrap);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,wrap);
    glGenerateMipmap(GL_TEXTURE_2D);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,repeat?GL_LINEAR_MIPMAP_LINEAR:GL_LINEAR);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
    return true;
}
Texture::~Texture(){if(id)glDeleteTextures(1,&id);}

static constexpr float cubeFar=18.f;
static constexpr int cubeSize=512;

Renderer::Renderer(const fs::path& r):root(r){
    fs::path dir=root/"shaders";
    scene=loadProgram(dir,"scene.vert","scene.frag");
    shadow=loadProgram(dir,"shadow.vert","shadow.frag");
    pointShadow=loadProgram(dir,"shadow.vert","point_shadow.frag");
    blur=loadProgram(dir,"screen.vert","blur.frag");
    post=loadProgram(dir,"screen.vert","post.frag");
    sky=loadProgram(dir,"screen.vert","sky.frag");
    particle=loadProgram(dir,"particle.vert","particle.frag");
    uiProgram=loadProgram(dir,"ui.vert","ui.frag");
    if(!font.load(root/"assets/combat-font.png",false))throw std::runtime_error("Missing required asset: assets/combat-font.png");
    stonePaint.load(root/"assets/stone-paint.png",true);
    glGenVertexArrays(1,&screenVao);
    glGenVertexArrays(1,&particleVao);glGenBuffers(1,&particleVbo);
    glBindVertexArray(particleVao);glBindBuffer(GL_ARRAY_BUFFER,particleVbo);
    size_t offsets[]={offsetof(Particle,position),offsetof(Particle,color),offsetof(Particle,size),offsetof(Particle,alpha),offsetof(Particle,kind)};
    for(GLuint i=0;i<5;++i){glEnableVertexAttribArray(i);glVertexAttribPointer(i,i<2?3:1,GL_FLOAT,GL_FALSE,sizeof(Particle),reinterpret_cast<void*>(offsets[i]));}
    glGenFramebuffers(1,&hdrFbo);glGenTextures(1,&hdrColor);glGenTextures(1,&hdrDepth);glGenFramebuffers(2,blurFbo);glGenTextures(2,blurTexture);
    glGenFramebuffers(1,&shadowFbo);glGenTextures(1,&shadowTexture);resizeShadow(2048);
    glGenFramebuffers(1,&cubeFbo);glGenFramebuffers(1,&cubeCacheFbo);glGenTextures(4,cubes);
    glBindFramebuffer(GL_FRAMEBUFFER,cubeCacheFbo);glDrawBuffer(GL_NONE);glReadBuffer(GL_NONE);
    glBindFramebuffer(GL_FRAMEBUFFER,cubeFbo);glDrawBuffer(GL_NONE);glReadBuffer(GL_NONE);
    for(GLuint t:cubes){
        glBindTexture(GL_TEXTURE_CUBE_MAP,t);
        for(int face=0;face<6;++face)glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X+face,0,GL_DEPTH_COMPONENT24,cubeSize,cubeSize,0,GL_DEPTH_COMPONENT,GL_FLOAT,nullptr);
        glTexParameteri(GL_TEXTURE_CUBE_MAP,GL_TEXTURE_MIN_FILTER,GL_NEAREST);glTexParameteri(GL_TEXTURE_CUBE_MAP,GL_TEXTURE_MAG_FILTER,GL_NEAREST);
        for(GLenum axis:{GL_TEXTURE_WRAP_S,GL_TEXTURE_WRAP_T,GL_TEXTURE_WRAP_R})glTexParameteri(GL_TEXTURE_CUBE_MAP,axis,GL_CLAMP_TO_EDGE);
        glFramebufferTexture2D(GL_FRAMEBUFFER,GL_DEPTH_ATTACHMENT,GL_TEXTURE_CUBE_MAP_POSITIVE_X,t,0);
        checkFramebuffer("Point shadow");
    }
    glBindFramebuffer(GL_FRAMEBUFFER,0);
    glEnable(GL_PROGRAM_POINT_SIZE);
}
Renderer::~Renderer(){
    for(GLuint p:{scene,shadow,pointShadow,blur,post,sky,particle,uiProgram})glDeleteProgram(p);
    glDeleteVertexArrays(1,&screenVao);glDeleteVertexArrays(1,&particleVao);glDeleteBuffers(1,&particleVbo);
    glDeleteFramebuffers(1,&shadowFbo);glDeleteTextures(1,&shadowTexture);
    glDeleteFramebuffers(1,&hdrFbo);glDeleteTextures(1,&hdrColor);glDeleteTextures(1,&hdrDepth);glDeleteFramebuffers(2,blurFbo);glDeleteTextures(2,blurTexture);
    glDeleteFramebuffers(1,&cubeFbo);glDeleteFramebuffers(1,&cubeCacheFbo);glDeleteTextures(4,cubes);
}
void Renderer::resizeShadow(int size){
    if(size==shadowSize)return;shadowSize=size;
    glBindTexture(GL_TEXTURE_2D,shadowTexture);glTexImage2D(GL_TEXTURE_2D,0,GL_DEPTH_COMPONENT24,size,size,0,GL_DEPTH_COMPONENT,GL_FLOAT,nullptr);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_NEAREST);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_CLAMP_TO_BORDER);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_CLAMP_TO_BORDER);
    const float border[]={1,1,1,1};glTexParameterfv(GL_TEXTURE_2D,GL_TEXTURE_BORDER_COLOR,border);
    glBindFramebuffer(GL_FRAMEBUFFER,shadowFbo);glFramebufferTexture2D(GL_FRAMEBUFFER,GL_DEPTH_ATTACHMENT,GL_TEXTURE_2D,shadowTexture,0);
    glDrawBuffer(GL_NONE);glReadBuffer(GL_NONE);checkFramebuffer("Sun shadow");glBindFramebuffer(GL_FRAMEBUFFER,0);
}
void Renderer::resizeTargets(int w,int h){
    if(w==width&&h==height)return;width=w;height=h;
    auto setup=[](GLuint id,int tw,int th,GLint format,GLenum external,GLenum filter){
        glBindTexture(GL_TEXTURE_2D,id);glTexImage2D(GL_TEXTURE_2D,0,format,tw,th,0,external,GL_FLOAT,nullptr);
        glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,filter);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,filter);
        glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_CLAMP_TO_EDGE);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_CLAMP_TO_EDGE);
    };
    setup(hdrColor,w,h,GL_RGBA16F,GL_RGBA,GL_LINEAR);setup(hdrDepth,w,h,GL_DEPTH_COMPONENT24,GL_DEPTH_COMPONENT,GL_NEAREST);
    glBindFramebuffer(GL_FRAMEBUFFER,hdrFbo);
    glFramebufferTexture2D(GL_FRAMEBUFFER,GL_COLOR_ATTACHMENT0,GL_TEXTURE_2D,hdrColor,0);
    glFramebufferTexture2D(GL_FRAMEBUFFER,GL_DEPTH_ATTACHMENT,GL_TEXTURE_2D,hdrDepth,0);
    checkFramebuffer("HDR scene");
    for(int i=0;i<2;++i){
        setup(blurTexture[i],std::max(1,w/2),std::max(1,h/2),GL_RGBA16F,GL_RGBA,GL_LINEAR);
        glBindFramebuffer(GL_FRAMEBUFFER,blurFbo[i]);glFramebufferTexture2D(GL_FRAMEBUFFER,GL_COLOR_ATTACHMENT0,GL_TEXTURE_2D,blurTexture[i],0);
        checkFramebuffer("Bloom");
    }
    glBindFramebuffer(GL_FRAMEBUFFER,0);
}
void Renderer::setZone(const ZoneVisuals& v){
    zoneMesh.update(v.mesh);zoneSolid.update(v.solid);
    lights=v.lights;emitters=v.emitters;cubeCached=false;cubeLights[0]=cubeLights[1]=-1;
}
static float fractf(float a){return a-std::floor(a);}
std::vector<Particle> Renderer::particles(float time,glm::vec3 focus,const RenderSettings& s) const{
    std::vector<Particle> out;out.reserve(2000);
    float density=s.effects==0?.35f:s.effects==1?.7f:1.f;
    int k=0;
    for(const auto& e:emitters){
        ++k;if(glm::length(glm::vec2(e.position.x-focus.x,e.position.z-focus.z))>40.f&&e.kind!=5)continue;
        float sc=e.scale;
        if(e.kind==0){
            for(int i=0;i<int(28*density);++i){float seed=float(i)*.618034f+float(k)*.123f,life=fractf(time*(1.3f+fractf(seed)*.6f)+seed);
                glm::vec3 o(std::sin(seed*83+time*3)*.075f*sc*(1-life),life*.63f*sc,std::cos(seed*41+time*2)*.075f*sc*(1-life));
                out.push_back({e.position+o,glm::mix(glm::vec3(6.5f,2.8f,.55f),glm::vec3(1.5f,.12f,.015f),life),(.30f*(1-life)+.03f)*sc,(1-life)*.45f,0});}
            for(int i=0;i<int(8*density);++i){float seed=float(i)*.381966f+float(k)*.17f,life=fractf(time*.24f+seed);
                out.push_back({e.position+glm::vec3(std::sin(seed*71+time)*life*.5f,life*2.2f,std::cos(seed*27)*life*.3f)*sc,{3.6f,1.2f,.2f},.016f*sc,(1-life)*.7f,1});}
        }else if(e.kind==1){ // chimney smoke
            for(int i=0;i<int(14*density);++i){float seed=float(i)*.618f+float(k)*.31f,life=fractf(time*.12f+seed);
                glm::vec3 p=e.position+glm::vec3(std::sin(seed*9+time*.3f)*life*.8f+life*1.2f,life*4.f,std::cos(seed*5)*life*.5f);
                out.push_back({p,glm::vec3(.30f,.30f,.32f)*(1.2f-life*.5f),.35f+life*1.1f,(1-life)*.35f,2});}
        }else if(e.kind==2){ // poison bubbles
            for(int i=0;i<int(6*density);++i){float seed=float(i)*.77f+float(k)*.21f,life=fractf(time*.6f+seed);
                out.push_back({e.position+glm::vec3(std::sin(seed*13)*.4f,life*.5f,std::cos(seed*7)*.4f),{.4f,1.6f,.25f},.06f,(1-life)*.8f,1});}
        }else if(e.kind==3){ // fireflies
            for(int i=0;i<int(12*density);++i){float seed=float(i)*1.3f+float(k);
                glm::vec3 p=e.position+glm::vec3(std::sin(time*.3f+seed)*4.f,std::sin(time*.7f+seed*2)*.6f,std::cos(time*.25f+seed*1.7f)*4.f);
                out.push_back({p,{1.6f,1.8f,.6f},.05f,.5f+.5f*std::sin(time*3+seed),1});}
        }else if(e.kind==4){ // shrine motes
            for(int i=0;i<int(16*density);++i){float seed=float(i)*.618f,life=fractf(time*.25f+seed);float a=seed*6.28f+time*.5f;
                out.push_back({e.position+glm::vec3(std::cos(a)*.7f,life*2.4f-.6f,std::sin(a)*.7f),{.4f,1.8f,1.6f},.05f,(1-life)*.8f,1});}
        }else if(e.kind==5){ // corruption wisps
            for(int i=0;i<int(18*density);++i){float seed=float(i)*.618f+float(k),life=fractf(time*.1f+seed);
                glm::vec3 p=e.position+glm::vec3(std::sin(seed*11+time*.2f)*2.f*sc,life*3.f*sc,std::cos(seed*7)*2.f*sc);
                out.push_back({p,{.9f,.3f,1.4f},.25f*sc,(1-life)*.4f,2});}
        }
    }
    // ambient dust motes around the focus
    for(int i=0;i<int(80*density);++i){float sd=float(i);
        glm::vec3 p=focus+glm::vec3(-14+28*fractf(sd*.618f),.4f+3.6f*fractf(sd*.345f+time*.016f),-10+20*fractf(sd*.723f));
        p.x+=std::sin(time*.35f+sd)*.15f;out.push_back({p,{.28f,.35f,.32f},.012f,.25f,1});}
    return out;
}
void Renderer::render(const FrameInput& f,const RenderSettings& s,const GpuMesh& dyn,const GpuMesh& hero,const glm::mat4& heroModel,const std::vector<Particle>& extra){
    resizeTargets(f.width,f.height);
    int wantShadow=s.shadowQuality==1?1024:2048;resizeShadow(wantShadow);
    glm::mat4 vp=f.projection*f.view;
    bool shadowsOn=s.technique&&s.shadowQuality>0;

    // Choose up to 16 lights near the focus; the two closest flagged lights get cube shadows.
    std::vector<int> order(lights.size());for(size_t i=0;i<order.size();++i)order[i]=int(i);
    std::sort(order.begin(),order.end(),[&](int a,int b){return glm::length(lights[size_t(a)].position-f.focus)<glm::length(lights[size_t(b)].position-f.focus);});
    if(order.size()>16)order.resize(16);
    int cube[2]={-1,-1};int found=0;
    for(size_t i=0;i<order.size()&&found<2;++i)if(lights[size_t(order[i])].shadow)cube[found++]=int(i);
    if(cube[0]!=cubeLights[0]||cube[1]!=cubeLights[1]){cubeCached=false;cubeLights[0]=cube[0];cubeLights[1]=cube[1];}
    // keep cache keyed by light identity, not by sorted slot
    static int cachedIds[2]={-2,-2};
    int ids[2]={cube[0]>=0?order[size_t(cube[0])]:-1,cube[1]>=0?order[size_t(cube[1])]:-1};
    if(ids[0]!=cachedIds[0]||ids[1]!=cachedIds[1]){cubeCached=false;cachedIds[0]=ids[0];cachedIds[1]=ids[1];}

    glDisable(GL_BLEND);glEnable(GL_DEPTH_TEST);glDepthMask(GL_TRUE);
    auto drawScene=[&](GLuint program,bool culled){
        setMatrix(program,"model",glm::mat4(1));
        if(culled){glEnable(GL_CULL_FACE);glCullFace(GL_BACK);}
        zoneSolid.draw();
        glDisable(GL_CULL_FACE);
        zoneMesh.draw();dyn.draw();
        setMatrix(program,"model",heroModel);hero.draw();
    };
    // Omnidirectional torch shadows (static scenery cached, moving figures redrawn).
    if(shadowsOn){
        const glm::vec3 dirs[]={{1,0,0},{-1,0,0},{0,1,0},{0,-1,0},{0,0,1},{0,0,-1}};
        const glm::vec3 ups[]={{0,-1,0},{0,-1,0},{0,0,1},{0,0,-1},{0,-1,0},{0,-1,0}};
        glm::mat4 proj=glm::perspective(pi*.5f,1.f,.12f,cubeFar);
        glUseProgram(pointShadow);setFloat(pointShadow,"time",f.time);setFloat(pointShadow,"farPlane",cubeFar);glViewport(0,0,cubeSize,cubeSize);
        for(int l=0;l<2;++l){
            if(ids[l]<0)continue;glm::vec3 p=lights[size_t(ids[l])].position;setVec3(pointShadow,"lightPosition",p);
            for(int face=0;face<6;++face){
                setMatrix(pointShadow,"lightMatrix",proj*glm::lookAt(p,p+dirs[face],ups[face]));
                if(!cubeCached){
                    glBindFramebuffer(GL_FRAMEBUFFER,cubeCacheFbo);
                    glFramebufferTexture2D(GL_FRAMEBUFFER,GL_DEPTH_ATTACHMENT,GL_TEXTURE_CUBE_MAP_POSITIVE_X+face,cubes[l+2],0);
                    glClear(GL_DEPTH_BUFFER_BIT);setMatrix(pointShadow,"model",glm::mat4(1));zoneSolid.draw();zoneMesh.draw();
                }
                glBindFramebuffer(GL_READ_FRAMEBUFFER,cubeCacheFbo);
                glFramebufferTexture2D(GL_READ_FRAMEBUFFER,GL_DEPTH_ATTACHMENT,GL_TEXTURE_CUBE_MAP_POSITIVE_X+face,cubes[l+2],0);
                glBindFramebuffer(GL_DRAW_FRAMEBUFFER,cubeFbo);
                glFramebufferTexture2D(GL_DRAW_FRAMEBUFFER,GL_DEPTH_ATTACHMENT,GL_TEXTURE_CUBE_MAP_POSITIVE_X+face,cubes[l],0);
                glBlitFramebuffer(0,0,cubeSize,cubeSize,0,0,cubeSize,cubeSize,GL_DEPTH_BUFFER_BIT,GL_NEAREST);
                glBindFramebuffer(GL_FRAMEBUFFER,cubeFbo);
                setMatrix(pointShadow,"model",glm::mat4(1));dyn.draw();setMatrix(pointShadow,"model",heroModel);hero.draw();
            }
        }
        cubeCached=true;
    }
    // Sun shadow map fitted around the camera focus (texel-snapped to avoid shimmering).
    const float extent=30.f,depthRange=90.f;
    glm::vec3 key=glm::normalize(f.keyDirection);
    glm::mat4 lightView=glm::lookAt(f.focus+key*45.f,f.focus,std::abs(key.y)>.99f?glm::vec3(0,0,1):glm::vec3(0,1,0));
    glm::mat4 lightProj=glm::ortho(-extent,extent,-extent,extent,.1f,depthRange);
    glm::mat4 lightMatrix=lightProj*lightView;
    {glm::vec4 o=lightMatrix*glm::vec4(0,0,0,1);float texel=2.f/float(shadowSize);
     glm::vec2 snap(std::round(o.x/texel)*texel-o.x,std::round(o.y/texel)*texel-o.y);
     lightMatrix=glm::translate(glm::mat4(1),glm::vec3(snap,0))*lightMatrix;}
    if(shadowsOn){
        glBindFramebuffer(GL_FRAMEBUFFER,shadowFbo);glViewport(0,0,shadowSize,shadowSize);glClear(GL_DEPTH_BUFFER_BIT);
        glUseProgram(shadow);setMatrix(shadow,"lightMatrix",lightMatrix);setFloat(shadow,"time",f.time);
        glEnable(GL_POLYGON_OFFSET_FILL);glPolygonOffset(2.f,3.f);drawScene(shadow,false);glDisable(GL_POLYGON_OFFSET_FILL);
    }
    // HDR scene
    glBindFramebuffer(GL_FRAMEBUFFER,hdrFbo);glViewport(0,0,f.width,f.height);
    glClearColor(.007f,.011f,.018f,1);glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);
    // Lighting presets per biome.
    glm::vec3 keyColor{.22f,.26f,.34f},ambLow{.095f,.080f,.095f},ambHigh{.15f,.16f,.17f},fog{.03f,.035f,.045f};float fogDensity=.002f;
    glm::vec3 skyTop{.18f,.34f,.62f},skyHorizon{.72f,.70f,.66f},cloud{.95f,.92f,.88f};
    if(f.surface){
        keyColor={1.12f,1.00f,.82f};ambLow={.08f,.075f,.085f};ambHigh={.19f,.23f,.30f};fog={.55f,.66f,.80f};fogDensity=.0016f;
        if(f.biome==2){keyColor*=.62f;ambHigh={.13f,.19f,.19f};fog={.22f,.30f,.30f};fogDensity=.006f;}
        if(f.biome==4){keyColor={.95f,.95f,1.02f};ambHigh={.17f,.20f,.27f};ambLow={.07f,.07f,.09f};fog={.50f,.55f,.62f};}
        if(f.biome==5){keyColor={1.0f,.82f,.92f};ambHigh={.18f,.17f,.26f};fog={.34f,.28f,.40f};}
        glm::vec3 taint{.55f,.35f,.70f};
        keyColor=glm::mix(keyColor,keyColor*glm::vec3(.85f,.7f,.95f),f.corruption*.6f);
        fog=glm::mix(fog,taint*.6f,f.corruption*.3f);
        skyTop=glm::mix(skyTop,glm::vec3(.25f,.18f,.40f),f.corruption*.55f);
        skyHorizon=glm::mix(skyHorizon,glm::vec3(.62f,.50f,.62f),f.corruption*.5f);
        // sky behind everything
        glDepthMask(GL_FALSE);glDisable(GL_DEPTH_TEST);glUseProgram(sky);
        setMatrix(sky,"inverseViewProjection",glm::inverse(vp));setVec3(sky,"sunDirection",key);
        setVec3(sky,"skyTop",skyTop);setVec3(sky,"skyHorizon",skyHorizon);setVec3(sky,"cloudColor",cloud);
        setFloat(sky,"time",f.time);setFloat(sky,"cloudAmount",s.effects>0?1.f:.6f);
        glBindVertexArray(screenVao);glDrawArrays(GL_TRIANGLES,0,3);
        glDepthMask(GL_TRUE);glEnable(GL_DEPTH_TEST);
    }else{
        if(f.biome==10){ambLow={.10f,.08f,.09f};ambHigh={.15f,.16f,.13f};}
        if(f.biome==11){ambHigh={.12f,.16f,.19f};keyColor={.18f,.26f,.34f};}
        if(f.biome==14){ambLow={.12f,.06f,.07f};ambHigh={.17f,.11f,.12f};keyColor={.35f,.14f,.12f};}
    }
    glUseProgram(scene);
    setMatrix(scene,"viewProjection",vp);setMatrix(scene,"lightMatrix",lightMatrix);
    setVec3(scene,"cameraPos",f.cameraPos);setVec3(scene,"keyDirection",key);setVec3(scene,"keyColor",keyColor);
    setVec3(scene,"ambientLow",ambLow);setVec3(scene,"ambientHigh",ambHigh);setVec3(scene,"fogColor",fog);setFloat(scene,"fogDensity",fogDensity);
    setFloat(scene,"corruption",f.corruption);setFloat(scene,"time",f.time);
    setVec3(scene,"heroPos",f.heroPos);setFloat(scene,"cutaway",f.cutaway);
    setInt(scene,"shadowsOn",shadowsOn);setFloat(scene,"shadowExtent",extent*2);setFloat(scene,"shadowDepthRange",depthRange-.1f);
    std::vector<glm::vec3> positions,colors;std::vector<float> radii;
    float flickerTime=f.time;
    for(size_t i=0;i<order.size();++i){const auto& l=lights[size_t(order[i])];
        float flicker=.94f+.045f*std::sin(flickerTime*3.1f+float(i)*2)+.035f*std::sin(flickerTime*5.3f+float(i));
        positions.push_back(l.position);colors.push_back(l.color*flicker);radii.push_back(l.radius);}
    setInt(scene,"lightCount",int(positions.size()));
    if(!positions.empty()){
        glUniform3fv(glGetUniformLocation(scene,"lightPositions"),GLsizei(positions.size()),glm::value_ptr(positions[0]));
        glUniform3fv(glGetUniformLocation(scene,"lightColors"),GLsizei(colors.size()),glm::value_ptr(colors[0]));
        glUniform1fv(glGetUniformLocation(scene,"lightRadii"),GLsizei(radii.size()),radii.data());
    }
    setInt(scene,"shadowLight0",shadowsOn?cube[0]:-1);setInt(scene,"shadowLight1",shadowsOn?cube[1]:-1);
    for(int i=0;i<2;++i){
        glActiveTexture(GL_TEXTURE4+i);glBindTexture(GL_TEXTURE_CUBE_MAP,cubes[i]);
        setInt(scene,i==0?"pointShadow0":"pointShadow1",4+i);
        setVec3(scene,i==0?"pointShadowPositions[0]":"pointShadowPositions[1]",ids[i]>=0?lights[size_t(ids[i])].position:glm::vec3(0,-100,0));
    }
    setFloat(scene,"pointShadowFar",cubeFar);
    glActiveTexture(GL_TEXTURE0);glBindTexture(GL_TEXTURE_2D,shadowTexture);setInt(scene,"shadowMap",0);
    glActiveTexture(GL_TEXTURE3);glBindTexture(GL_TEXTURE_2D,stonePaint.id);setInt(scene,"stonePaint",3);setInt(scene,"hasStonePaint",stonePaint.id!=0);
    drawScene(scene,true);
    // particles
    auto points=particles(f.time,f.focus,s);points.insert(points.end(),extra.begin(),extra.end());
    if(!points.empty()){
        glUseProgram(particle);setMatrix(particle,"viewProjection",vp);
        float scale=s.perspective?float(f.height)/(2.f*std::tan(f.fovY*.5f)):float(f.height)/(2.f*f.orthoHalf);
        setFloat(particle,"pointScale",scale);setInt(particle,"perspectiveCamera",s.perspective);
        glBindVertexArray(particleVao);glBindBuffer(GL_ARRAY_BUFFER,particleVbo);
        glBufferData(GL_ARRAY_BUFFER,GLsizeiptr(points.size()*sizeof(Particle)),points.data(),GL_STREAM_DRAW);
        glEnable(GL_BLEND);glBlendFunc(GL_SRC_ALPHA,GL_ONE);glDepthMask(GL_FALSE);
        glDrawArrays(GL_POINTS,0,GLsizei(points.size()));glDepthMask(GL_TRUE);glDisable(GL_BLEND);
    }
    glDisable(GL_DEPTH_TEST);
    // bloom
    glBindVertexArray(screenVao);glUseProgram(blur);setInt(blur,"source",0);glActiveTexture(GL_TEXTURE0);
    int passes=s.effects==0?2:6;
    for(int i=0;i<passes;++i){
        glBindFramebuffer(GL_FRAMEBUFFER,blurFbo[i%2]);glViewport(0,0,std::max(1,f.width/2),std::max(1,f.height/2));
        glBindTexture(GL_TEXTURE_2D,i==0?hdrColor:blurTexture[(i-1)%2]);
        setInt(blur,"extract",i==0);glUniform2f(glGetUniformLocation(blur,"direction"),i%2==0?1.8f:0,i%2==0?0:1.8f);glDrawArrays(GL_TRIANGLES,0,3);
    }
    glBindFramebuffer(GL_FRAMEBUFFER,0);glViewport(0,0,f.width,f.height);glUseProgram(post);
    glActiveTexture(GL_TEXTURE0);glBindTexture(GL_TEXTURE_2D,hdrColor);setInt(post,"scene",0);
    glActiveTexture(GL_TEXTURE1);glBindTexture(GL_TEXTURE_2D,blurTexture[(passes-1)%2]);setInt(post,"bloom",1);
    glActiveTexture(GL_TEXTURE2);glBindTexture(GL_TEXTURE_2D,hdrDepth);setInt(post,"depth",2);
    setFloat(post,"time",f.time);setFloat(post,"nearPlane",s.perspective?f.near:0.1f);setFloat(post,"farPlane",f.far);
    setInt(post,"perspectiveCamera",s.perspective);setInt(post,"tiltShift",s.tiltShift);setInt(post,"effects",s.effects>0);
    setFloat(post,"bloomStrength",s.effects==0?.05f:.10f);setFloat(post,"exposure",f.surface?.92f:1.4f);setFloat(post,"saturation",f.surface?1.35f:1.12f);setFloat(post,"fade",f.fade);
    glDrawArrays(GL_TRIANGLES,0,3);
    glActiveTexture(GL_TEXTURE0);
}
