#pragma once
#include <glad/gl.h>
#include <glm/glm.hpp>
#include <algorithm>
#include <string>
#include <vector>

// Immediate-mode interface drawn in one batched call. Coordinates are virtual pixels of a
// 1440 x 900 canvas scaled to the window, so layouts survive resizing.
inline int glyphIndex(unsigned cp){
    if(cp>=32&&cp<128)return int(cp-32);
    if(cp>=0x410&&cp<0x450)return 96+int(cp-0x410);
    if(cp==0x401)return 160; if(cp==0x451)return 161;
    static const unsigned extra[]={0x2014,0xAB,0xBB,0x2116,0x2022,0x2026,0x2190,0x2192,0x2191,0x2193,0xD7,0xB0,0xB7,0x2605};
    for(int i=0;i<14;++i)if(extra[i]==cp)return 162+i;
    if(cp==0x2013)return 13;       // en dash -> '-'
    return 31;                     // '?'
}
inline std::vector<unsigned> decodeUtf8(const std::string& s){
    std::vector<unsigned> out;
    for(size_t i=0;i<s.size();){
        unsigned char c=static_cast<unsigned char>(s[i]);unsigned cp=c;int n=0;
        if(c>=0xF0){cp=c&7;n=3;}else if(c>=0xE0){cp=c&15;n=2;}else if(c>=0xC0){cp=c&31;n=1;}
        ++i;for(int k=0;k<n&&i<s.size();++k,++i)cp=(cp<<6)|(static_cast<unsigned char>(s[i])&63);
        out.push_back(cp);
    }
    return out;
}

namespace ui {
// Shortens text to `n` characters without splitting a UTF-8 sequence.
inline std::string clip(const std::string& s,size_t n){
    size_t count=0;for(size_t i=0;i<s.size();++i){if((static_cast<unsigned char>(s[i])&0xC0)!=0x80){if(count==n)return s.substr(0,i)+"…";++count;}}
    return s;
}
const glm::vec4 gold{.86f,.70f,.40f,1}, mint{.38f,.86f,.71f,1}, muted{.56f,.63f,.66f,1}, text{.88f,.90f,.88f,1},
                red{.92f,.36f,.28f,1}, panelBg{.035f,.048f,.062f,.94f}, dim{.30f,.34f,.36f,1};
inline glm::vec4 rarity(int r){
    static const glm::vec4 c[]={{.84f,.86f,.84f,1},{.45f,.86f,.40f,1},{.40f,.62f,1.f,1},{1.f,.62f,.20f,1}};
    return c[std::clamp(r,0,3)];
}
}

struct Ui {
    struct V { glm::vec2 p, uv; glm::vec4 color; };
    std::vector<V> vertices;
    GLuint program=0, vao=0, vbo=0, atlas=0;
    float width=1440, height=900, scale=1;
    glm::vec2 mouse{-1};
    bool down=false, pressed=false, released=false, rightPressed=false;
    bool overUi=false;              // the cursor is over an interface element this frame
    std::string tooltip;
    int clicks=0;                   // buttons activated this frame (for UI sounds)

    void init(GLuint prog,GLuint font){
        program=prog;atlas=font;glGenVertexArrays(1,&vao);glGenBuffers(1,&vbo);glBindVertexArray(vao);glBindBuffer(GL_ARRAY_BUFFER,vbo);
        glEnableVertexAttribArray(0);glVertexAttribPointer(0,2,GL_FLOAT,GL_FALSE,sizeof(V),nullptr);
        glEnableVertexAttribArray(1);glVertexAttribPointer(1,2,GL_FLOAT,GL_FALSE,sizeof(V),reinterpret_cast<void*>(sizeof(glm::vec2)));
        glEnableVertexAttribArray(2);glVertexAttribPointer(2,4,GL_FLOAT,GL_FALSE,sizeof(V),reinterpret_cast<void*>(2*sizeof(glm::vec2)));
    }
    ~Ui(){if(vbo)glDeleteBuffers(1,&vbo);if(vao)glDeleteVertexArrays(1,&vao);}
    void begin(int w,int h,glm::vec2 cursorPixels,bool left,bool leftEdge,bool leftUp,bool rightEdge){
        scale=std::min(float(w)/1440.f,float(h)/900.f);width=float(w)/scale;height=float(h)/scale;
        mouse=cursorPixels/scale;down=left;pressed=leftEdge;released=leftUp;rightPressed=rightEdge;
        overUi=false;tooltip.clear();clicks=0;
    }
    bool inside(float x,float y,float w,float h) const {return mouse.x>=x&&mouse.x<x+w&&mouse.y>=y&&mouse.y<y+h;}
    void block(float x,float y,float w,float h){if(inside(x,y,w,h))overUi=true;}
    void quad(float x,float y,float w,float h,glm::vec4 color,glm::vec2 uv0={-1,-1},glm::vec2 uv1={-1,-1}){
        V a{{x*scale,y*scale},uv0,color},b{{(x+w)*scale,y*scale},{uv1.x,uv0.y},color};
        V c{{(x+w)*scale,(y+h)*scale},uv1,color},d{{x*scale,(y+h)*scale},{uv0.x,uv1.y},color};
        vertices.insert(vertices.end(),{a,b,c,a,c,d});
    }
    static float advance(float size){return size*.56f;}
    float textWidth(const std::string& s,float size) const {return float(decodeUtf8(s).size())*advance(size);}
    void text(float x,float y,const std::string& s,float size=17,glm::vec4 color=ui::text){
        float h=size*44.f/32.f;
        for(unsigned cp:decodeUtf8(s)){
            if(cp!=' '){int i=glyphIndex(cp);glm::vec2 uv(float(i%16)/16.f,float(i/16)/11.f);
                quad(x-size*.22f,y,size,h,color,uv,uv+glm::vec2(1.f/16,1.f/11));}
            x+=advance(size);
        }
    }
    void textCentered(float cx,float y,const std::string& s,float size,glm::vec4 color){text(cx-textWidth(s,size)*.5f,y,s,size,color);}
    // Word wrap; returns the height used.
    float paragraph(float x,float y,float w,const std::string& s,float size=16,glm::vec4 color=ui::text,float lineGap=1.45f){
        float line=size*lineGap,cy=y;size_t maxChars=size_t(std::max(4.f,w/advance(size)));
        std::string current,word;
        auto flush=[&]{if(!current.empty()){text(x,cy,current,size,color);cy+=line;current.clear();}};
        auto cps=decodeUtf8(s);
        auto len=[&](const std::string& t){return decodeUtf8(t).size();};
        size_t i=0;std::string all=s;
        for(size_t pos=0;pos<=all.size();++pos){
            char ch=pos<all.size()?all[pos]:' ';
            if(ch==' '||ch=='\n'||pos==all.size()){
                if(!word.empty()){
                    if(len(current)+len(word)+(current.empty()?0:1)>maxChars)flush();
                    current+=(current.empty()?"":" ")+word;word.clear();
                }
                if(ch=='\n')flush();
            }else word+=ch;
        }
        flush();(void)cps;(void)i;
        return cy-y;
    }
    void panel(float x,float y,float w,float h,const std::string& title=""){
        block(x,y,w,h);
        quad(x,y,w,h,ui::panelBg);quad(x,y,w,1.5f,{.55f,.47f,.30f,.8f});quad(x,y+h-1,w,1,{.25f,.28f,.29f,.8f});
        if(!title.empty())text(x+18,y+12,title,20,ui::gold);
    }
    void bar(float x,float y,float w,float h,float ratio,glm::vec4 color){quad(x,y,w,h,{.10f,.13f,.15f,1});quad(x,y,w*std::clamp(ratio,0.f,1.f),h,color);}
    bool button(float x,float y,float w,float h,const std::string& label,bool enabled=true,const std::string& reason="",bool selected=false,float size=17){
        bool hot=inside(x,y,w,h);if(hot)overUi=true;
        glm::vec4 bg=!enabled?glm::vec4(.06f,.075f,.085f,.95f):hot?glm::vec4(.17f,.26f,.27f,1):selected?glm::vec4(.13f,.20f,.21f,1):glm::vec4(.10f,.15f,.17f,.97f);
        quad(x,y,w,h,bg);
        if(selected||hot)quad(x,y+h-2,w,2,enabled?ui::gold:ui::dim);
        textCentered(x+w*.5f,y+h*.5f-size*.69f,label,size,enabled?ui::text:ui::dim);
        if(hot&&!enabled&&!reason.empty())tooltip=reason;
        if(hot&&enabled&&pressed){++clicks;pressed=false;return true;}
        if(hot&&!enabled&&pressed)pressed=false;
        return false;
    }
    void drawTooltip(){
        if(tooltip.empty())return;
        float w=std::min(520.f,textWidth(tooltip,15)+28);float x=std::min(mouse.x+18,width-w-8),y=std::min(mouse.y+22,height-60);
        quad(x,y,w,10,{0,0,0,0});
        float hgt=paragraph(-9999,0,w-24,tooltip,15)+18;   // measure
        quad(x,y,w,hgt,{.02f,.03f,.04f,.97f});quad(x,y,2,hgt,ui::gold);
        paragraph(x+14,y+9,w-24,tooltip,15,ui::text);
    }
    void draw(){
        if(vertices.empty())return;
        glDisable(GL_DEPTH_TEST);glDisable(GL_CULL_FACE);glEnable(GL_BLEND);glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);
        glUseProgram(program);glUniform2f(glGetUniformLocation(program,"viewport"),width*scale,height*scale);
        glActiveTexture(GL_TEXTURE0);glBindTexture(GL_TEXTURE_2D,atlas);glUniform1i(glGetUniformLocation(program,"atlas"),0);
        glBindVertexArray(vao);glBindBuffer(GL_ARRAY_BUFFER,vbo);
        glBufferData(GL_ARRAY_BUFFER,GLsizeiptr(vertices.size()*sizeof(V)),vertices.data(),GL_STREAM_DRAW);
        glDrawArrays(GL_TRIANGLES,0,GLsizei(vertices.size()));glDisable(GL_BLEND);vertices.clear();
    }
};
