#include <glad/gl.h>
#include <GLFW/glfw3.h>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <stb_image.h>
#include "scene.hpp"
#include "material_shader.hpp"
#include "collision.hpp"
#include "hero_animation.hpp"
#include "scene_layout.hpp"
#include <chrono>
#include <cstddef>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
namespace fs = std::filesystem;
void buildHero(Mesh&);
static float zoom=17.5f,yaw=.57f,pitch=.74f;
static glm::vec3 focus(0,1.8f,0);
static float previewTime=4.25f;
static bool previewMotion=false,followHero=false;
static bool showOccupancy=false;
static bool interfaceVisible=true;
static constexpr const char* vertexShader=R"GLSL(
#version 330 core
layout(location=0) in vec3 position;
layout(location=1) in vec3 normal;
layout(location=2) in vec3 color;
layout(location=3) in float material;
uniform mat4 viewProjection,model,lightMatrix;
uniform float time;
out vec3 worldPos,worldNormal,vertexColor;
flat out int materialId;
out vec4 shadowPos;
void main(){
 vec3 local=position;
 if(material>10.5){
   float weight=clamp((5.2-local.y)/2.8,0.,1.);
   local.z+=weight*weight*(sin(time*1.5+local.y*2.+local.x*1.2)*.10+sin(time*2.3+local.x*3.)*.035);
 }
 vec4 p=model*vec4(local,1);
 worldPos=p.xyz;worldNormal=mat3(model)*normal;vertexColor=color;
 materialId=int(material+.5);shadowPos=lightMatrix*p;
 gl_Position=viewProjection*p;
}
)GLSL";
static constexpr const char* shadowVertex=R"GLSL(
#version 330 core
layout(location=0) in vec3 position;
layout(location=3) in float material;
uniform mat4 lightMatrix,model;
uniform float time;
out vec3 lightWorldPos;
void main(){
 vec3 p=position;
 if(material>10.5){float w=clamp((5.2-p.y)/2.8,0.,1.);p.z+=w*w*(sin(time*1.5+p.y*2.+p.x*1.2)*.10+sin(time*2.3+p.x*3.)*.035);}
 vec4 world=model*vec4(p,1);lightWorldPos=world.xyz;
 gl_Position=lightMatrix*world;
}
)GLSL";
static constexpr const char* shadowFragment=R"GLSL(
#version 330 core
void main(){}
)GLSL";
static constexpr const char* pointShadowFragment=R"GLSL(
#version 330 core
in vec3 lightWorldPos;
uniform vec3 lightPosition;
uniform float farPlane;
void main(){gl_FragDepth=length(lightWorldPos-lightPosition)/farPlane;}
)GLSL";
static constexpr const char* screenVertex=R"GLSL(
#version 330 core
out vec2 uv;
void main(){
 vec2 p=vec2((gl_VertexID<<1)&2,gl_VertexID&2);
 uv=p;gl_Position=vec4(p*2.-1.,0,1);
}
)GLSL";
static constexpr const char* blurFragment=R"GLSL(
#version 330 core
in vec2 uv;out vec4 fragColor;
uniform sampler2D source;uniform vec2 direction;uniform bool extract;
vec3 sampleColor(vec2 p){
 vec3 c=texture(source,p).rgb;
 return extract?c*smoothstep(.7,1.8,max(c.r,max(c.g,c.b))):c;
}
void main(){
 vec2 d=direction/vec2(textureSize(source,0));
 vec3 c=sampleColor(uv)*.227027;
 c+=(sampleColor(uv+d*1.384615)+sampleColor(uv-d*1.384615))*.316216;
 c+=(sampleColor(uv+d*3.230769)+sampleColor(uv-d*3.230769))*.070270;
 fragColor=vec4(c,1);
}
)GLSL";
static constexpr const char* postFragment=R"GLSL(
#version 330 core
in vec2 uv;out vec4 fragColor;
uniform sampler2D scene,bloom,depth;uniform float time;
vec3 aces(vec3 x){return clamp((x*(2.51*x+.03))/(x*(2.43*x+.59)+.14),0.,1.);}
void main(){
 vec3 c=texture(scene,uv).rgb;
 float z=texture(depth,uv).r;
 if(z<.9999){
   vec2 texel=1./vec2(textureSize(depth,0));float ao=0.;
   for(int i=0;i<8;i++){
     float a=float(i)*.785398;
     float dz=(z-texture(depth,uv+vec2(cos(a),sin(a))*texel*5.).r)*65.;
     ao+=smoothstep(.025,.20,dz)*(1.-smoothstep(.35,1.4,dz));
   }
   c*=1.-ao*.025;
   // Thin ink accents at silhouettes and depth creases; flat floor stays quiet.
   float l=texture(depth,uv-vec2(texel.x,0)).r;
   float r=texture(depth,uv+vec2(texel.x,0)).r;
   float u=texture(depth,uv-vec2(0,texel.y)).r;
   float d=texture(depth,uv+vec2(0,texel.y)).r;
   float crease=max(abs(l+r-2.*z),abs(u+d-2.*z));
   float ink=smoothstep(.00035,.0018,crease);
   c=mix(c,c*.28+vec3(.004,.005,.012),ink*.65);
 }
 c+=texture(bloom,uv).rgb*.10;
 c=aces(c*1.1);
 float vignette=1.-.32*pow(length((uv-.5)*vec2(1.15,1.)),1.5);
 c*=vignette;
 c=pow(c,vec3(1./2.2));
 float grain=fract(sin(dot(gl_FragCoord.xy,vec2(12.9898,78.233)))*43758.5453)-.5;
 fragColor=vec4(c+grain*.0022,1);
}
)GLSL";
static constexpr const char* particleVertex=R"GLSL(
#version 330 core
layout(location=0) in vec3 position;layout(location=1) in vec3 color;
layout(location=2) in float size;layout(location=3) in float alpha;
layout(location=4) in float kind;
uniform mat4 viewProjection;uniform float pointScale;
out vec3 particleColor;out float opacity;flat out int type;
void main(){
 gl_Position=viewProjection*vec4(position,1);
 gl_PointSize=max(1.,size*pointScale);particleColor=color;opacity=alpha;type=int(kind);
}
)GLSL";
static constexpr const char* particleFragment=R"GLSL(
#version 330 core
in vec3 particleColor;in float opacity;flat in int type;out vec4 fragColor;
void main(){
 vec2 p=gl_PointCoord*2.-1.;float r=length(p);
 if(r>1.)discard;
 float a=pow(max(0.,1.-r*r),type==2?3.:1.8)*opacity;
 fragColor=vec4(particleColor,a);
}
)GLSL";
static constexpr const char* overlayVertex=R"GLSL(
#version 330 core
uniform vec4 rect;uniform vec2 viewport;
out vec2 uv;
void main(){
 vec2 positions[6]=vec2[6](vec2(0,0),vec2(1,0),vec2(1,1),vec2(0,0),vec2(1,1),vec2(0,1));
 vec2 p=positions[gl_VertexID];uv=p;
 vec2 pixel=rect.xy+p*rect.zw;
 gl_Position=vec4(pixel.x/viewport.x*2.-1.,1.-pixel.y/viewport.y*2.,0,1);
}
)GLSL";
static constexpr const char* overlayFragment=R"GLSL(
#version 330 core
in vec2 uv;out vec4 fragColor;uniform sampler2D image;
void main(){fragColor=texture(image,uv);}
)GLSL";
GLuint program(const char* vertex,const char* fragment){
    auto compile=[](GLenum type,const char* source){
        GLuint shader=glCreateShader(type);glShaderSource(shader,1,&source,nullptr);glCompileShader(shader);
        GLint ok=0;glGetShaderiv(shader,GL_COMPILE_STATUS,&ok);
        if(!ok){char log[8192];glGetShaderInfoLog(shader,sizeof(log),nullptr,log);throw std::runtime_error(log);}
        return shader;
    };
    GLuint v=compile(GL_VERTEX_SHADER,vertex),f=compile(GL_FRAGMENT_SHADER,fragment),p=glCreateProgram();
    glAttachShader(p,v);glAttachShader(p,f);glLinkProgram(p);glDeleteShader(v);glDeleteShader(f);
    GLint ok=0;glGetProgramiv(p,GL_LINK_STATUS,&ok);
    if(!ok){char log[8192];glGetProgramInfoLog(p,sizeof(log),nullptr,log);throw std::runtime_error(log);}return p;
}
void matrix(GLuint p,const char* name,const glm::mat4& m){glUniformMatrix4fv(glGetUniformLocation(p,name),1,GL_FALSE,glm::value_ptr(m));}
void vector(GLuint p,const char* name,glm::vec3 v){glUniform3fv(glGetUniformLocation(p,name),1,glm::value_ptr(v));}
void scalar(GLuint p,const char* name,float v){glUniform1f(glGetUniformLocation(p,name),v);}
void integer(GLuint p,const char* name,int v){glUniform1i(glGetUniformLocation(p,name),v);}
struct GpuMesh{
    GLuint vao=0,vbo=0;GLsizei count=0;
    explicit GpuMesh(const Mesh& m){
        count=static_cast<GLsizei>(m.vertices.size());glGenVertexArrays(1,&vao);glGenBuffers(1,&vbo);
        glBindVertexArray(vao);glBindBuffer(GL_ARRAY_BUFFER,vbo);
        glBufferData(GL_ARRAY_BUFFER,static_cast<GLsizeiptr>(m.vertices.size()*sizeof(Vertex)),m.vertices.data(),GL_STATIC_DRAW);
        for(GLuint i=0;i<4;++i){
            glEnableVertexAttribArray(i);glVertexAttribPointer(i,i==3?1:3,GL_FLOAT,GL_FALSE,sizeof(Vertex),reinterpret_cast<void*>(size_t(i)*sizeof(glm::vec3)));
        }
    }
    ~GpuMesh(){glDeleteBuffers(1,&vbo);glDeleteVertexArrays(1,&vao);}
    void draw()const{glBindVertexArray(vao);glDrawArrays(GL_TRIANGLES,0,count);}
    void update(const Mesh& m){
        count=static_cast<GLsizei>(m.vertices.size());glBindBuffer(GL_ARRAY_BUFFER,vbo);
        glBufferData(GL_ARRAY_BUFFER,GLsizeiptr(m.vertices.size()*sizeof(Vertex)),m.vertices.data(),GL_STREAM_DRAW);
    }
};
struct Targets{
    GLuint fbo=0,color=0,depth=0,blurFbo[2]{},blurTexture[2]{};
    int width=0,height=0;
    Targets(){glGenFramebuffers(1,&fbo);glGenTextures(1,&color);glGenTextures(1,&depth);glGenFramebuffers(2,blurFbo);glGenTextures(2,blurTexture);}
    ~Targets(){glDeleteFramebuffers(1,&fbo);glDeleteTextures(1,&color);glDeleteTextures(1,&depth);glDeleteFramebuffers(2,blurFbo);glDeleteTextures(2,blurTexture);}
    static void setupTexture(GLuint id,int w,int h,GLint format,GLenum external){
        glBindTexture(GL_TEXTURE_2D,id);glTexImage2D(GL_TEXTURE_2D,0,format,w,h,0,external,GL_FLOAT,nullptr);
        glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_CLAMP_TO_EDGE);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_CLAMP_TO_EDGE);
    }
    void resize(int w,int h){
        if(w==width&&h==height)return;width=w;height=h;
        setupTexture(color,w,h,GL_RGBA16F,GL_RGBA);setupTexture(depth,w,h,GL_DEPTH_COMPONENT24,GL_DEPTH_COMPONENT);
        glBindTexture(GL_TEXTURE_2D,depth);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_NEAREST);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_NEAREST);
        glBindFramebuffer(GL_FRAMEBUFFER,fbo);glFramebufferTexture2D(GL_FRAMEBUFFER,GL_COLOR_ATTACHMENT0,GL_TEXTURE_2D,color,0);
        glFramebufferTexture2D(GL_FRAMEBUFFER,GL_DEPTH_ATTACHMENT,GL_TEXTURE_2D,depth,0);
        if(glCheckFramebufferStatus(GL_FRAMEBUFFER)!=GL_FRAMEBUFFER_COMPLETE)throw std::runtime_error("HDR framebuffer incomplete");
        for(int i=0;i<2;++i){
            setupTexture(blurTexture[i],std::max(1,w/2),std::max(1,h/2),GL_RGBA16F,GL_RGBA);
            glBindFramebuffer(GL_FRAMEBUFFER,blurFbo[i]);glFramebufferTexture2D(GL_FRAMEBUFFER,GL_COLOR_ATTACHMENT0,GL_TEXTURE_2D,blurTexture[i],0);
            if(glCheckFramebufferStatus(GL_FRAMEBUFFER)!=GL_FRAMEBUFFER_COMPLETE)throw std::runtime_error("Bloom framebuffer incomplete");
        }glBindFramebuffer(GL_FRAMEBUFFER,0);
    }
};
// Omnidirectional depth maps: the altar fires cast real shadows on all six sides.
struct PointShadows {
    GLuint fbo=0,cacheFbo=0,textures[4]{};
    bool cached=false;
    static constexpr int size=512;
    static constexpr float farPlane=18.f;
    PointShadows(){
        glGenFramebuffers(1,&fbo);glGenFramebuffers(1,&cacheFbo);glGenTextures(4,textures);
        glBindFramebuffer(GL_FRAMEBUFFER,cacheFbo);glDrawBuffer(GL_NONE);glReadBuffer(GL_NONE);
        glBindFramebuffer(GL_FRAMEBUFFER,fbo);glDrawBuffer(GL_NONE);glReadBuffer(GL_NONE);
        for(GLuint texture:textures){
            glBindTexture(GL_TEXTURE_CUBE_MAP,texture);
            for(int face=0;face<6;++face)glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X+face,0,GL_DEPTH_COMPONENT24,size,size,0,GL_DEPTH_COMPONENT,GL_FLOAT,nullptr);
            glTexParameteri(GL_TEXTURE_CUBE_MAP,GL_TEXTURE_MIN_FILTER,GL_NEAREST);glTexParameteri(GL_TEXTURE_CUBE_MAP,GL_TEXTURE_MAG_FILTER,GL_NEAREST);
            for(GLenum axis:{GL_TEXTURE_WRAP_S,GL_TEXTURE_WRAP_T,GL_TEXTURE_WRAP_R})glTexParameteri(GL_TEXTURE_CUBE_MAP,axis,GL_CLAMP_TO_EDGE);
            glFramebufferTexture2D(GL_FRAMEBUFFER,GL_DEPTH_ATTACHMENT,GL_TEXTURE_CUBE_MAP_POSITIVE_X,texture,0);
            if(glCheckFramebufferStatus(GL_FRAMEBUFFER)!=GL_FRAMEBUFFER_COMPLETE)throw std::runtime_error("Point shadow framebuffer incomplete");
        }
        glBindFramebuffer(GL_FRAMEBUFFER,0);
    }
    ~PointShadows(){glDeleteFramebuffers(1,&fbo);glDeleteFramebuffers(1,&cacheFbo);glDeleteTextures(4,textures);}
    void draw(GLuint shader,const GpuMesh& scene,const GpuMesh& hero,const glm::mat4& heroModel,const std::vector<Torch>& torches,float time){
        const glm::vec3 directions[]={{1,0,0},{-1,0,0},{0,1,0},{0,-1,0},{0,0,1},{0,0,-1}};
        const glm::vec3 up[]={{0,-1,0},{0,-1,0},{0,0,1},{0,0,-1},{0,-1,0},{0,-1,0}};
        const glm::mat4 projection=glm::perspective(pi*.5f,1.f,.12f,farPlane);
        glBindFramebuffer(GL_FRAMEBUFFER,fbo);glViewport(0,0,size,size);glUseProgram(shader);
        scalar(shader,"time",time);scalar(shader,"farPlane",farPlane);
        for(int light=0;light<2;++light){
            glm::vec3 p=torches[size_t(light)+2].position+glm::vec3(0,.38f,0);
            vector(shader,"lightPosition",p);
            for(int face=0;face<6;++face){
                matrix(shader,"lightMatrix",projection*glm::lookAt(p,p+directions[face],up[face]));
                // Static room/light positions: reuse their depth, then draw the moving hero.
                // Rebuild this cache if movable scenery or moving emitters are introduced.
                if(!cached){
                    glBindFramebuffer(GL_FRAMEBUFFER,cacheFbo);
                    glFramebufferTexture2D(GL_FRAMEBUFFER,GL_DEPTH_ATTACHMENT,GL_TEXTURE_CUBE_MAP_POSITIVE_X+face,textures[light+2],0);
                    glClear(GL_DEPTH_BUFFER_BIT);matrix(shader,"model",glm::mat4(1));scene.draw();
                }
                glBindFramebuffer(GL_READ_FRAMEBUFFER,cacheFbo);
                glFramebufferTexture2D(GL_READ_FRAMEBUFFER,GL_DEPTH_ATTACHMENT,GL_TEXTURE_CUBE_MAP_POSITIVE_X+face,textures[light+2],0);
                glBindFramebuffer(GL_DRAW_FRAMEBUFFER,fbo);
                glFramebufferTexture2D(GL_DRAW_FRAMEBUFFER,GL_DEPTH_ATTACHMENT,GL_TEXTURE_CUBE_MAP_POSITIVE_X+face,textures[light],0);
                glBlitFramebuffer(0,0,size,size,0,0,size,size,GL_DEPTH_BUFFER_BIT,GL_NEAREST);
                matrix(shader,"model",heroModel);hero.draw();
            }
        }
        cached=true;
    }
};
struct Overlay {
    GLuint texture=0;int width=0,height=0;
    explicit Overlay(const fs::path& path){
        int channels=0;auto pixels=stbi_load(path.string().c_str(),&width,&height,&channels,4);
        if(!pixels){std::cerr<<"Optional interface missing: "<<path<<'\n';return;}
        glGenTextures(1,&texture);glBindTexture(GL_TEXTURE_2D,texture);
        glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA,width,height,0,GL_RGBA,GL_UNSIGNED_BYTE,pixels);stbi_image_free(pixels);
        glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_CLAMP_TO_EDGE);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_CLAMP_TO_EDGE);
    }
    ~Overlay(){if(texture)glDeleteTextures(1,&texture);}
    void draw(GLuint p,float x,float y,float scale,int w,int h)const{
        if(!texture)return;glUseProgram(p);integer(p,"image",0);
        glUniform2f(glGetUniformLocation(p,"viewport"),float(w),float(h));
        glUniform4f(glGetUniformLocation(p,"rect"),x,y,float(width)*scale,float(height)*scale);
        glActiveTexture(GL_TEXTURE0);glBindTexture(GL_TEXTURE_2D,texture);glDrawArrays(GL_TRIANGLES,0,6);
    }
};
struct Particle { glm::vec3 position,color;float size,alpha,kind; };
float fract(float a){return a-std::floor(a);}
std::vector<Particle> particles(float time,const std::vector<Torch>& torches,glm::vec3 hero,float pulse){
    std::vector<Particle> result;result.reserve(800);int torchIndex=0;
    for(const auto& torch:torches){
        float s=torch.scale;int k=torchIndex++;
        for(int i=0;i<32;++i){
            float seed=float(i)*.618034f+float(k)*.123f,life=fract(time*(1.3f+fract(seed)*.6f)+seed);
            glm::vec3 offset(std::sin(seed*83+time*3)*.075f*s*(1-life),life*.63f*s,std::cos(seed*41+time*2)*.075f*s*(1-life));
            glm::vec3 color=glm::mix(glm::vec3(6.5f,2.8f,.55f),glm::vec3(1.5f,.12f,.015f),life);
            result.push_back({torch.position+offset,color,(.30f*(1-life)+.03f)*s,(1-life)*.45f,0});
        }
        for(int i=0;i<10;++i){
            float seed=float(i)*.381966f+float(k)*.17f,life=fract(time*.24f+seed);
            result.push_back({torch.position+glm::vec3(std::sin(seed*71+time)*life*.5f,life*2.2f,std::cos(seed*27)*life*.3f)*s,
                glm::vec3(3.6f,1.2f,.20f),.016f*s,(1-life)*.7f,1});
        }
    }
    for(int i=0;i<45;++i){
        float s=float(i);glm::vec3 p(-12+24*fract(s*.754877f),.12f+fract(s*.51f)*.25f,-9+18*fract(s*.56984f));
        p.x+=std::sin(time*.12f+s)*.7f;p.z+=std::cos(time*.1f+s)*.5f;
        result.push_back({p,glm::vec3(.055f,.095f,.115f),1.2f+fract(s*.41f)*1.6f,.10f,2});
    }
    for(int i=0;i<100;++i){
        float s=float(i);glm::vec3 p(-12+24*fract(s*.618f),.4f+3.6f*fract(s*.345f+time*.016f),-9+18*fract(s*.723f));
        p.x+=std::sin(time*.35f+s)*.15f;result.push_back({p,glm::vec3(.28f,.35f,.32f),.012f,.25f,1});
    }
    if(pulse>0)for(int i=0;i<90;++i){
        float a=float(i)*2*pi/90,r=(1-pulse)*3.0f+.2f;
        result.push_back({hero+glm::vec3(std::cos(a)*r,.18f+std::sin(float(i)*2.4f)*.12f,std::sin(a)*r),glm::vec3(.15f,1.8f,1.3f),.14f,pulse,1});
    }
    return result;
}
bool capture(const fs::path& path,int w,int h){
    std::vector<unsigned char> pixels(size_t(w)*size_t(h)*3);
    glPixelStorei(GL_PACK_ALIGNMENT,1);glReadPixels(0,0,w,h,GL_RGB,GL_UNSIGNED_BYTE,pixels.data());
    if(path.has_parent_path())fs::create_directories(path.parent_path());
    std::ofstream out(path,std::ios::binary);out<<"P6\n"<<w<<' '<<h<<"\n255\n";
    for(int y=h-1;y>=0;--y)out.write(reinterpret_cast<const char*>(pixels.data()+size_t(y)*size_t(w)*3),std::streamsize(w)*3);
    double mean=0,squares=0;size_t lit=0;
    for(size_t i=0;i<pixels.size();i+=3){double v=(double(pixels[i])+pixels[i+1]+pixels[i+2])/3;mean+=v;squares+=v*v;if(v>30)++lit;}
    double count=double(w)*h;mean/=count;squares=squares/count-mean*mean;
    std::cout<<"Frame verification: mean="<<mean<<" variance="<<squares<<" lit="<<double(lit)/count<<"\nCapture: "<<path<<'\n';
    return out.good()&&mean>8&&squares>120&&double(lit)/count>.08;
}
int run(GLFWwindow* window,bool smoke,const fs::path& capturePath,const fs::path& executableDir){
    Mesh sceneMesh,heroMesh;std::vector<Torch> torches;
    buildArchitecture(sceneMesh);buildProps(sceneMesh,torches);buildHero(heroMesh);
    std::cout<<"Scene: "<<sceneMesh.vertices.size()/3<<" triangles, hero "<<heroMesh.vertices.size()/3<<", torches "<<torches.size()<<'\n';
    GpuMesh scene(sceneMesh),hero(heroMesh);
    Mesh cellMesh;
    for(int row=0;row<layout::rows;++row)for(int col=0;col<layout::columns;++col){
        if(!layout::isBlocked(col,row))continue;
        auto c=layout::center({col,row,1,1});float h=layout::cellSize*.5f;
        glm::vec3 color(.19f,.075f,.015f);
        auto edge=[&](glm::vec3 a,glm::vec3 b){
            glm::vec3 side=glm::normalize(glm::cross(b-a,glm::vec3(0,1,0)))*.022f;
            cellMesh.quad(a-side,a+side,b+side,b-side,color,7);
        };
        glm::vec3 a(c.x-h,.04f,c.z-h),b(c.x+h,.04f,c.z-h),d(c.x-h,.04f,c.z+h),e(c.x+h,.04f,c.z+h);
        edge(a,b);edge(b,e);edge(e,d);edge(d,a);
    }
    GpuMesh cells(cellMesh);
    GLuint shader=program(vertexShader,sceneFragmentShader),shadowShader=program(shadowVertex,shadowFragment);
    GLuint pointShadowShader=program(shadowVertex,pointShadowFragment);PointShadows pointShadows;
    GLuint blur=program(screenVertex,blurFragment),post=program(screenVertex,postFragment);
    GLuint particleProgram=program(particleVertex,particleFragment),overlayProgram=program(overlayVertex,overlayFragment);
    GLuint screenVao=0;glGenVertexArrays(1,&screenVao);
    GLuint shadowFbo=0,shadowTexture=0;glGenFramebuffers(1,&shadowFbo);glGenTextures(1,&shadowTexture);
    glBindTexture(GL_TEXTURE_2D,shadowTexture);glTexImage2D(GL_TEXTURE_2D,0,GL_DEPTH_COMPONENT24,2048,2048,0,GL_DEPTH_COMPONENT,GL_FLOAT,nullptr);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_NEAREST);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_CLAMP_TO_BORDER);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_CLAMP_TO_BORDER);
    const float border[]={1,1,1,1};glTexParameterfv(GL_TEXTURE_2D,GL_TEXTURE_BORDER_COLOR,border);
    glBindFramebuffer(GL_FRAMEBUFFER,shadowFbo);glFramebufferTexture2D(GL_FRAMEBUFFER,GL_DEPTH_ATTACHMENT,GL_TEXTURE_2D,shadowTexture,0);glDrawBuffer(GL_NONE);glReadBuffer(GL_NONE);
    if(glCheckFramebufferStatus(GL_FRAMEBUFFER)!=GL_FRAMEBUFFER_COMPLETE)throw std::runtime_error("Shadow framebuffer incomplete");
    glBindFramebuffer(GL_FRAMEBUFFER,0);
    GLuint particleVao=0,particleVbo=0;glGenVertexArrays(1,&particleVao);glGenBuffers(1,&particleVbo);
    glBindVertexArray(particleVao);glBindBuffer(GL_ARRAY_BUFFER,particleVbo);
    size_t offsets[]={offsetof(Particle,position),offsetof(Particle,color),offsetof(Particle,size),offsetof(Particle,alpha),offsetof(Particle,kind)};
    for(GLuint i=0;i<5;++i){glEnableVertexAttribArray(i);glVertexAttribPointer(i,i<2?3:1,GL_FLOAT,GL_FALSE,sizeof(Particle),reinterpret_cast<void*>(offsets[i]));}
    glEnable(GL_PROGRAM_POINT_SIZE);
    Targets targets;Overlay footer(executableDir/"assets/controls.png"),stonePaint(executableDir/"assets/stone-paint.png");
    if(stonePaint.texture){
        glBindTexture(GL_TEXTURE_2D,stonePaint.texture);
        glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_REPEAT);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_REPEAT);
        glGenerateMipmap(GL_TEXTURE_2D);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR_MIPMAP_LINEAR);
    }
    glm::vec3 heroPosition=layout::center({10,11,1,1});float heroYaw=0,pulse=0,gait=0,moveBlend=0,castBlend=0;glm::vec3 velocity(0);
    float cameraYaw=yaw,cameraPitch=pitch,cameraZoom=zoom;glm::vec3 cameraFocus=focus;double previous=glfwGetTime(),oldX=0,oldY=0;
    bool dragging=false,lastTab=false,lastReset=false,lastCapture=false,lastSpace=false,lastGrid=false;
    int frame=0;bool success=!smoke&&capturePath.empty();float fpsTimer=0;int fpsFrames=0;
    double benchmarkStart=0;
    glm::vec3 keyDirection=glm::normalize(glm::vec3(.35f,1,.55f));
    glm::mat4 lightMatrix=glm::ortho(-23.f,23.f,-22.f,22.f,.1f,80.f)*glm::lookAt(keyDirection*40.f,glm::vec3(0),glm::vec3(0,1,0));
    while(!glfwWindowShouldClose(window)){
        glfwPollEvents();double now=glfwGetTime();float elapsed=float(now-previous),dt=std::min(elapsed,.05f);previous=now;
        float time=smoke?float(frame)/60.f:(!capturePath.empty()?previewTime:float(now));int w,h;glfwGetFramebufferSize(window,&w,&h);
        if(w<=0||h<=0){glfwWaitEvents();continue;}
        if(glfwGetKey(window,GLFW_KEY_ESCAPE)==GLFW_PRESS)glfwSetWindowShouldClose(window,GLFW_TRUE);
        auto pressed=[&](int key){return glfwGetKey(window,key)==GLFW_PRESS;};
        bool tab=pressed(GLFW_KEY_TAB),reset=pressed(GLFW_KEY_R),shot=pressed(GLFW_KEY_F12),space=pressed(GLFW_KEY_SPACE);
        if(tab&&!lastTab)interfaceVisible=!interfaceVisible;
        bool grid=pressed(GLFW_KEY_G);if(grid&&!lastGrid)showOccupancy=!showOccupancy;lastGrid=grid;
        if(reset&&!lastReset){followHero=false;yaw=.57f;pitch=.74f;zoom=17.5f;focus={0,1.8f,0};heroPosition=layout::center({10,11,1,1});heroYaw=0;velocity=glm::vec3(0);gait=0;}
        if(pressed(GLFW_KEY_1)){followHero=false;yaw=.57f;pitch=.74f;zoom=17.5f;focus={0,1.8f,0};}
        if(pressed(GLFW_KEY_2)){followHero=false;yaw=.30f;pitch=.60f;zoom=6.5f;focus={0,2.5f,-8.0f};}
        if(pressed(GLFW_KEY_3)){followHero=true;yaw=-2.45f;pitch=.48f;zoom=3.8f;}
        if(followHero)focus=heroPosition+glm::vec3(0,1.25f,0);
        if((space&&!lastSpace)||(smoke&&frame==8))pulse=1;
        lastTab=tab;lastReset=reset;lastSpace=space;pulse=std::max(0.f,pulse-dt*.9f);
        double mx,my;glfwGetCursorPos(window,&mx,&my);
        bool mouse=glfwGetMouseButton(window,GLFW_MOUSE_BUTTON_RIGHT)==GLFW_PRESS;
        if(mouse&&dragging){yaw-=float(mx-oldX)*.006f;pitch=std::clamp(pitch+float(my-oldY)*.004f,.35f,1.25f);}
        dragging=mouse;oldX=mx;oldY=my;
        if(pressed(GLFW_KEY_LEFT))yaw-=dt*.8f;if(pressed(GLFW_KEY_RIGHT))yaw+=dt*.8f;
        glm::vec3 right(std::cos(yaw),0,-std::sin(yaw)),forward(-std::sin(yaw),0,-std::cos(yaw)),movement(0);
        if(pressed(GLFW_KEY_W))movement+=forward;if(pressed(GLFW_KEY_S))movement-=forward;
        if(pressed(GLFW_KEY_D))movement+=right;if(pressed(GLFW_KEY_A))movement-=right;
        // Smooth velocity, wrapped turning and distance-driven gait avoid abrupt start/stop poses.
        if(smoke)movement=frame<28?forward:glm::vec3(0);
        float requestedSpeed=pressed(GLFW_KEY_LEFT_SHIFT)?2.6f:1.6f;
        glm::vec3 desired=glm::length(movement)>.01f?glm::normalize(movement)*requestedSpeed:glm::vec3(0);
        velocity=glm::mix(velocity,desired,1.f-std::exp(-12.f*dt));
        glm::vec3 oldPosition=heroPosition,next=heroPosition+velocity*dt;
        if(canMove({next.x,0,heroPosition.z}))heroPosition.x=next.x;else velocity.x=0;
        if(canMove({heroPosition.x,0,next.z}))heroPosition.z=next.z;else velocity.z=0;
        float distance=glm::length(heroPosition-oldPosition);
        float actualSpeed=dt>0?distance/dt:0;
        moveBlend=glm::mix(moveBlend,std::clamp(actualSpeed/1.6f,0.f,1.f),1.f-std::exp(-10.f*dt));
        gait+=heroAnimation::gaitAdvance(distance,1.45f,moveBlend);
        if(glm::length(velocity)>.08f){
            float wanted=std::atan2(-velocity.x,-velocity.z);
            heroYaw+=std::atan2(std::sin(wanted-heroYaw),std::cos(wanted-heroYaw))*(1.f-std::exp(-14.f*dt));
        }
        if(previewMotion&&!capturePath.empty()){moveBlend=1.f;gait=previewTime*2*pi/(heroAnimation::cycleDistance*1.45f/1.6f);}
        castBlend=glm::mix(castBlend,pulse,1.f-std::exp(-12.f*dt));
        heroMesh.vertices.clear();buildAnimatedHero(heroMesh,time,gait,moveBlend,castBlend);hero.update(heroMesh);
        glm::mat4 heroModel=glm::rotate(glm::translate(glm::mat4(1),heroPosition),heroYaw,glm::vec3(0,1,0));
        heroModel=glm::scale(heroModel,glm::vec3(1.45f));
        float ease=1.f-std::exp(-9.f*dt);
        cameraYaw+=std::atan2(std::sin(yaw-cameraYaw),std::cos(yaw-cameraYaw))*ease;
        cameraPitch=glm::mix(cameraPitch,pitch,ease);cameraZoom=glm::mix(cameraZoom,zoom,ease);cameraFocus=glm::mix(cameraFocus,focus,ease);
        glm::vec3 target=cameraFocus,camera=target+glm::vec3(std::sin(cameraYaw)*std::cos(cameraPitch),std::sin(cameraPitch),std::cos(cameraYaw)*std::cos(cameraPitch))*45.f;
        float aspect=float(w)/float(h);glm::mat4 vp=glm::ortho(-cameraZoom*aspect,cameraZoom*aspect,-cameraZoom,cameraZoom,.1f,100.f)*glm::lookAt(camera,target,glm::vec3(0,1,0));
        targets.resize(w,h);glDisable(GL_BLEND);glEnable(GL_DEPTH_TEST);glDepthMask(GL_TRUE);glDisable(GL_CULL_FACE);
        pointShadows.draw(pointShadowShader,scene,hero,heroModel,torches,time);
        glBindFramebuffer(GL_FRAMEBUFFER,shadowFbo);glViewport(0,0,2048,2048);glClear(GL_DEPTH_BUFFER_BIT);
        glUseProgram(shadowShader);matrix(shadowShader,"lightMatrix",lightMatrix);scalar(shadowShader,"time",time);glEnable(GL_POLYGON_OFFSET_FILL);glPolygonOffset(2.f,3.f);
        matrix(shadowShader,"model",glm::mat4(1));scene.draw();matrix(shadowShader,"model",heroModel);hero.draw();glDisable(GL_POLYGON_OFFSET_FILL);
        glBindFramebuffer(GL_FRAMEBUFFER,targets.fbo);glViewport(0,0,w,h);
        glClearColor(.007f,.011f,.018f,1);glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);
        glUseProgram(shader);matrix(shader,"viewProjection",vp);matrix(shader,"lightMatrix",lightMatrix);
        vector(shader,"cameraPos",camera);vector(shader,"keyDirection",keyDirection);vector(shader,"keyColor",{.13f,.18f,.25f});scalar(shader,"time",time);
        std::vector<glm::vec3> positions,colors;std::vector<float> radii;
        for(size_t i=0;i<torches.size()&&i<12;++i){
            positions.push_back(torches[i].position+glm::vec3(0,i==2||i==3?.38f:.17f,0));
            float flicker=.94f+.045f*std::sin(time*3.1f+float(i)*2)+.035f*std::sin(time*5.3f+float(i));
            colors.push_back(glm::vec3(5.0f,1.85f,.40f)*flicker*torches[i].scale);radii.push_back(7.0f);
        }
        positions.push_back({-8,2.4f,-9.6f});colors.push_back({.08f,.7f,.65f});radii.push_back(5);

        integer(shader,"lightCount",int(positions.size()));
        glUniform3fv(glGetUniformLocation(shader,"lightPositions"),GLsizei(positions.size()),glm::value_ptr(positions[0]));
        glUniform3fv(glGetUniformLocation(shader,"lightColors"),GLsizei(colors.size()),glm::value_ptr(colors[0]));
        glUniform1fv(glGetUniformLocation(shader,"lightRadii"),GLsizei(radii.size()),radii.data());
        glActiveTexture(GL_TEXTURE0);glBindTexture(GL_TEXTURE_2D,shadowTexture);integer(shader,"shadowMap",0);
        glActiveTexture(GL_TEXTURE3);glBindTexture(GL_TEXTURE_2D,stonePaint.texture);integer(shader,"stonePaint",3);integer(shader,"hasStonePaint",stonePaint.texture!=0);
        for(int i=0;i<2;++i){
            glActiveTexture(GL_TEXTURE4+i);glBindTexture(GL_TEXTURE_CUBE_MAP,pointShadows.textures[i]);
            integer(shader,i==0?"pointShadow0":"pointShadow1",4+i);
            vector(shader,i==0?"pointShadowPositions[0]":"pointShadowPositions[1]",torches[size_t(i)+2].position+glm::vec3(0,.38f,0));
        }
        scalar(shader,"pointShadowFar",PointShadows::farPlane);
        matrix(shader,"model",glm::mat4(1));scene.draw();if(showOccupancy)cells.draw();matrix(shader,"model",heroModel);hero.draw();
        auto points=particles(time,torches,heroPosition,pulse);
        glUseProgram(particleProgram);matrix(particleProgram,"viewProjection",vp);scalar(particleProgram,"pointScale",float(h)/(2*cameraZoom));
        glBindVertexArray(particleVao);glBindBuffer(GL_ARRAY_BUFFER,particleVbo);
        glBufferData(GL_ARRAY_BUFFER,GLsizeiptr(points.size()*sizeof(Particle)),points.data(),GL_STREAM_DRAW);
        glEnable(GL_BLEND);glBlendFunc(GL_SRC_ALPHA,GL_ONE);glDepthMask(GL_FALSE);
        glDrawArrays(GL_POINTS,0,GLsizei(points.size()));glDepthMask(GL_TRUE);glDisable(GL_BLEND);glDisable(GL_DEPTH_TEST);
        glBindVertexArray(screenVao);glUseProgram(blur);integer(blur,"source",0);glActiveTexture(GL_TEXTURE0);
        for(int i=0;i<6;++i){
            glBindFramebuffer(GL_FRAMEBUFFER,targets.blurFbo[i%2]);glViewport(0,0,std::max(1,w/2),std::max(1,h/2));
            glBindTexture(GL_TEXTURE_2D,i==0?targets.color:targets.blurTexture[(i-1)%2]);
            integer(blur,"extract",i==0);glUniform2f(glGetUniformLocation(blur,"direction"),i%2==0?1.8f:0,i%2==0?0:1.8f);glDrawArrays(GL_TRIANGLES,0,3);
        }
        glBindFramebuffer(GL_FRAMEBUFFER,0);glViewport(0,0,w,h);glUseProgram(post);
        glActiveTexture(GL_TEXTURE0);glBindTexture(GL_TEXTURE_2D,targets.color);integer(post,"scene",0);
        glActiveTexture(GL_TEXTURE1);glBindTexture(GL_TEXTURE_2D,targets.blurTexture[1]);integer(post,"bloom",1);
        glActiveTexture(GL_TEXTURE2);glBindTexture(GL_TEXTURE_2D,targets.depth);integer(post,"depth",2);scalar(post,"time",time);glDrawArrays(GL_TRIANGLES,0,3);
        if(interfaceVisible){
            glEnable(GL_BLEND);glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);float scale=std::clamp(float(w)/1600.f,.65f,1.5f);
            footer.draw(overlayProgram,36*scale,float(h)-58*scale,scale,w,h);glDisable(GL_BLEND);
        }
        GLenum error=glGetError();if(error!=GL_NO_ERROR)throw std::runtime_error("OpenGL error "+std::to_string(error));
        if(shot&&!lastCapture)capture(executableDir/"captures"/("room-"+std::to_string(std::chrono::system_clock::now().time_since_epoch().count())+".ppm"),w,h);
        lastCapture=shot;++frame;
        if(smoke&&frame==1)glfwSetWindowSize(window,1024,640);
        if(smoke&&frame==2)glfwSetWindowSize(window,1280,800);
        if(smoke&&frame==3)glfwSetWindowSize(window,1440,900);
        if(smoke&&frame==5){glFinish();benchmarkStart=glfwGetTime();}
        if((smoke&&frame==45)||(!smoke&&!capturePath.empty()&&frame==3)){
            success=capture(capturePath.empty()?executableDir/"captures/smoke.ppm":capturePath,w,h);glfwSwapBuffers(window);break;
        }
        glfwSwapBuffers(window);fpsTimer+=elapsed;++fpsFrames;
        if(fpsTimer>=1){
            std::string titleText="THE OSSUARY | OpenGL 3.3 | "+std::to_string(int(float(fpsFrames)/fpsTimer))+" FPS | WASD move - RMB orbit - wheel zoom";
            glfwSetWindowTitle(window,titleText.c_str());fpsTimer=0;fpsFrames=0;
        }
    }
    if(smoke&&frame==45)std::cout<<"Average render speed: "<<40.0/(glfwGetTime()-benchmarkStart)<<" FPS; resize checks completed.\n";
    glDeleteProgram(shader);glDeleteProgram(shadowShader);glDeleteProgram(pointShadowShader);glDeleteProgram(blur);glDeleteProgram(post);glDeleteProgram(particleProgram);glDeleteProgram(overlayProgram);
    glDeleteTextures(1,&shadowTexture);glDeleteFramebuffers(1,&shadowFbo);glDeleteVertexArrays(1,&screenVao);glDeleteVertexArrays(1,&particleVao);glDeleteBuffers(1,&particleVbo);
    return success?0:1;
}
int main(int argc,char** argv){
    bool smoke=false;fs::path capturePath;
    for(int i=1;i<argc;++i){
        std::string arg=argv[i];
        if(arg=="--smoke-test")smoke=true;
        else if(arg=="--grid")showOccupancy=true;
        else if(arg=="--no-ui")interfaceVisible=false;
        else if(arg=="--motion")previewMotion=true;
        else if(arg=="--time"&&i+1<argc)previewTime=std::stof(argv[++i]);
        else if(arg=="--capture"&&i+1<argc)capturePath=fs::absolute(argv[++i]);
        else if(arg=="--view"&&i+1<argc){
            std::string view=argv[++i];
            if(view=="altar"){yaw=.30f;pitch=.60f;zoom=6.5f;focus={0,2.5f,-8.0f};}
            else if(view=="hero"){followHero=true;yaw=-2.45f;pitch=.48f;zoom=3.8f;focus=layout::center({10,11,1,1})+glm::vec3(0,1.25f,0);}
            else if(view!="overview"){std::cerr<<"Unknown view\n";return 1;}
        }
        else{std::cerr<<"Usage: finalprj [--smoke-test] [--capture output.ppm] [--view overview|altar|hero] [--time seconds] [--motion]\n";return 1;}
    }
    glfwSetErrorCallback([](int code,const char* message){std::cerr<<"GLFW "<<code<<": "<<message<<'\n';});
    if(!glfwInit())return 1;
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR,3);glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR,3);glfwWindowHint(GLFW_OPENGL_PROFILE,GLFW_OPENGL_CORE_PROFILE);
#ifdef __APPLE__
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT,GL_TRUE);
#endif
    if(smoke||!capturePath.empty())glfwWindowHint(GLFW_VISIBLE,GL_FALSE);
    GLFWwindow* window=glfwCreateWindow(capturePath.empty()?1440:1920,capturePath.empty()?900:1200,"THE OSSUARY | OpenGL",nullptr,nullptr);
    if(!window){glfwTerminate();return 1;}
    glfwMakeContextCurrent(window);
    if(!gladLoadGL(glfwGetProcAddress)||!GLAD_GL_VERSION_3_3){std::cerr<<"OpenGL 3.3 required\n";glfwDestroyWindow(window);glfwTerminate();return 1;}
    std::cout<<"OpenGL: "<<glGetString(GL_VERSION)<<"\nGPU: "<<glGetString(GL_RENDERER)<<'\n';
    glfwSwapInterval(smoke||!capturePath.empty()?0:1);
    glfwSetScrollCallback(window,[](GLFWwindow*,double,double dy){zoom=std::clamp(zoom-float(dy)*.8f,3.0f,24.f);});
    int result=1;try{result=run(window,smoke,capturePath,fs::absolute(argv[0]).parent_path());}catch(const std::exception& e){std::cerr<<e.what()<<'\n';}
    glfwDestroyWindow(window);glfwTerminate();return result;
}
