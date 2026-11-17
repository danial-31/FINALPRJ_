#pragma once
#include <glad/gl.h>
#include <glm/glm.hpp>
#include <algorithm>
#include <iterator>
#include <string>
#include <string_view>
#include <vector>
#include "ui_font_metrics.hpp"

// Immediate-mode interface drawn in one batched call. Coordinates are virtual pixels of a
// 1440 x 900 canvas scaled to the window, so layouts survive resizing.
inline int glyphIndex(unsigned cp){
    if(cp>=32&&cp<128)return int(cp-32);
    if(cp>=0x410&&cp<0x450)return 96+int(cp-0x410);
    if(cp==0x401)return 160; if(cp==0x451)return 161;
    for(size_t i=0;i<std::size(uiFont::extra);++i)if(uiFont::extra[i]==cp)return 162+int(i);
    return 31;                     // '?'
}
inline unsigned nextGlyph(std::string_view s,size_t& i){
    unsigned char c=static_cast<unsigned char>(s[i++]);
    if(c<0x80)return c;
    int count=c>=0xF0?3:c>=0xE0?2:1;
    unsigned cp=c&(0x7F>>(count+1));
    for(int j=0;j<count;++j){
        if(i>=s.size()||(static_cast<unsigned char>(s[i])&0xC0)!=0x80)return '?';
        cp=(cp<<6)|(static_cast<unsigned char>(s[i++])&63);
    }
    return cp;
}

namespace ui {
enum class Emblem {
    Pack, Helm, Journal, Map, Forge, Shrine, Gear, Hourglass,
    Sword, Daggers, Bow, Staff, Armor, Charm, Ore, Herb,
    Scroll, Key, Coins, Chest, Vial, Silk, Fang, Bone, None
};
// Shortens text to `n` characters without splitting a UTF-8 sequence.
inline std::string clip(const std::string& s,size_t n){
    size_t count=0;for(size_t i=0;i<s.size();++i){if((static_cast<unsigned char>(s[i])&0xC0)!=0x80){if(count==n)return s.substr(0,i)+"…";++count;}}
    return s;
}
const glm::vec4 gold{.91f,.77f,.52f,1}, mint{.50f,.81f,.73f,1}, muted{.70f,.70f,.65f,1}, text{.94f,.91f,.84f,1},
                red{.96f,.52f,.43f,1}, panelBg{.064f,.071f,.078f,.985f}, dim{.56f,.55f,.51f,1},
                brass{.45f,.35f,.22f,1}, bevel{.28f,.24f,.19f,1};
inline glm::vec4 rarity(int r){
    static const glm::vec4 c[]={{.87f,.84f,.77f,1},{.57f,.80f,.60f,1},{.55f,.75f,.93f,1},{.98f,.72f,.39f,1}};
    return c[std::clamp(r,0,3)];
}
}

struct Ui {
    struct V { glm::vec2 p, uv; glm::vec4 color; float image; };
    std::vector<V> vertices;
    GLuint program=0, vao=0, vbo=0, atlas=0, icons=0, emblems=0, cover=0;
    float width=1440, height=900, scale=1;
    glm::vec2 mouse{-1};
    bool down=false, pressed=false, released=false, rightPressed=false;
    bool overUi=false;              // the cursor is over an interface element this frame
    std::string tooltip;
    int clicks=0;                   // buttons activated this frame (for UI sounds)

    void init(GLuint prog,GLuint font,GLuint iconTexture,GLuint emblemTexture,GLuint coverTexture){
        emblems=emblemTexture;cover=coverTexture;
        program=prog;atlas=font;icons=iconTexture;glGenVertexArrays(1,&vao);glGenBuffers(1,&vbo);glBindVertexArray(vao);glBindBuffer(GL_ARRAY_BUFFER,vbo);
        glEnableVertexAttribArray(0);glVertexAttribPointer(0,2,GL_FLOAT,GL_FALSE,sizeof(V),nullptr);
        glEnableVertexAttribArray(1);glVertexAttribPointer(1,2,GL_FLOAT,GL_FALSE,sizeof(V),reinterpret_cast<void*>(sizeof(glm::vec2)));
        glEnableVertexAttribArray(2);glVertexAttribPointer(2,4,GL_FLOAT,GL_FALSE,sizeof(V),reinterpret_cast<void*>(2*sizeof(glm::vec2)));
        glEnableVertexAttribArray(3);glVertexAttribPointer(3,1,GL_FLOAT,GL_FALSE,sizeof(V),reinterpret_cast<void*>(offsetof(V,image)));
    }
    ~Ui(){if(vbo)glDeleteBuffers(1,&vbo);if(vao)glDeleteVertexArrays(1,&vao);}
    void begin(int w,int h,glm::vec2 cursorPixels,bool left,bool leftEdge,bool leftUp,bool rightEdge){
        scale=std::min(float(w)/1440.f,float(h)/900.f);width=float(w)/scale;height=float(h)/scale;
        mouse=cursorPixels/scale;down=left;pressed=leftEdge;released=leftUp;rightPressed=rightEdge;
        overUi=false;tooltip.clear();clicks=0;
    }
    bool inside(float x,float y,float w,float h) const {return mouse.x>=x&&mouse.x<x+w&&mouse.y>=y&&mouse.y<y+h;}
    void block(float x,float y,float w,float h){if(inside(x,y,w,h))overUi=true;}
    void quad(float x,float y,float w,float h,glm::vec4 color,glm::vec2 uv0={-1,-1},glm::vec2 uv1={-1,-1},float image=0){
        V a{{x*scale,y*scale},uv0,color,image},b{{(x+w)*scale,y*scale},{uv1.x,uv0.y},color,image};
        V c{{(x+w)*scale,(y+h)*scale},uv1,color,image},d{{x*scale,(y+h)*scale},{uv0.x,uv1.y},color,image};
        vertices.insert(vertices.end(),{a,b,c,a,c,d});
    }
    void icon(float x,float y,float size,int cell,float brightness=1.f){
        constexpr float inset=2.f/1024.f;
        glm::vec2 base{float(cell%4)*.25f,float(cell/4)*.25f};
        quad(x,y,size,size,{brightness,brightness,brightness,1},base+inset,base+.25f-inset,1);
    }
    void emblem(float x,float y,float size,ui::Emblem symbol,float brightness=1.f){
        if(symbol==ui::Emblem::None)return;
        int cell=static_cast<int>(symbol);
        glm::vec2 pitch{1.f/6.f,1.f/4.f};
        glm::vec2 origin=glm::vec2(cell%6,cell/6)*pitch;
        glm::vec2 inset=pitch*(2.f/256.f);
        quad(x,y,size,size,{brightness,brightness,brightness,1},origin+inset,origin+pitch-inset,2);
    }
    void gradient(float x,float y,float w,float h,glm::vec4 top,glm::vec4 bottom){
        V a{{x*scale,y*scale},{-1,-1},top,0}, b{{(x+w)*scale,y*scale},{-1,-1},top,0};
        V c{{(x+w)*scale,(y+h)*scale},{-1,-1},bottom,0}, d{{x*scale,(y+h)*scale},{-1,-1},bottom,0};
        vertices.insert(vertices.end(),{a,b,c,a,c,d});
    }
    void frame(float x,float y,float w,float h,glm::vec4 color){
        quad(x,y,w,1,color);quad(x,y+h-1,w,1,color);
        quad(x,y+1,1,h-2,color);quad(x+w-1,y+1,1,h-2,color);
    }
    enum class Face { Body, Heading };
    static float glyphAdvance(int index,float size,Face face){
        return uiFont::advances[int(face)][index]*size/(64.f*uiFont::em);
    }
    float textWidth(std::string_view s,float size,Face face=Face::Body) const{
        float widthUsed=0;
        for(size_t p=0;p<s.size();)widthUsed+=glyphAdvance(glyphIndex(nextGlyph(s,p)),size,face);
        return widthUsed;
    }
    void writeText(float x,float y,std::string_view s,float size,glm::vec4 color,Face face){
        const float ratio=size/uiFont::em,span=uiFont::cell*ratio;
        const glm::vec2 uvSize(1.f/uiFont::columns,1.f/(uiFont::rows*2));
        for(size_t p=0;p<s.size();){
            unsigned cp=nextGlyph(s,p);int i=glyphIndex(cp);
            if(cp!=' '){
                glm::vec2 uv=glm::vec2(i%uiFont::columns,i/uiFont::columns+int(face)*uiFont::rows)*uvSize;
                quad(x-uiFont::penX*ratio,y+size-uiFont::baseline*ratio,span,span,color,uv,uv+uvSize);
            }
            x+=glyphAdvance(i,size,face);
        }
    }
    void text(float x,float y,std::string_view s,float size=17,glm::vec4 color=ui::text){
        writeText(x,y,s,size,color,Face::Body);
    }
    void textCentered(float cx,float y,std::string_view s,float size,glm::vec4 color){
        text(cx-textWidth(s,size)*.5f,y,s,size,color);
    }
    void heading(float x,float y,std::string_view s,float size=24,glm::vec4 color=ui::gold){
        writeText(x,y,s,size,color,Face::Heading);
    }
    void headingCentered(float cx,float y,std::string_view s,float size,glm::vec4 color=ui::gold){
        heading(cx-textWidth(s,size,Face::Heading)*.5f,y,s,size,color);
    }
    float paragraph(float x,float y,float w,std::string_view s,float size=16,glm::vec4 color=ui::text,float lineGap=1.45f,bool measureOnly=false){
        float lineWidth=0,dy=0;
        float space=glyphAdvance(0,size,Face::Body);
        std::string line;
        auto flush=[&]{
            if(!measureOnly)text(x,y+dy,line,size,color);
            line.clear();lineWidth=0;dy+=size*lineGap;
        };
        for(size_t pos=0;pos<s.size();){
            if(s[pos]=='\n'){flush();++pos;continue;}
            if(s[pos]==' '){++pos;continue;}
            size_t end=s.find_first_of(" \n",pos);
            if(end==std::string_view::npos)end=s.size();
            std::string_view word=s.substr(pos,end-pos);
            float wordWidth=textWidth(word,size);
            if(!line.empty()&&lineWidth+space+wordWidth>w)flush();
            if(!line.empty()){line+=' ';lineWidth+=space;}
            if(wordWidth<=w){line+=word;lineWidth+=wordWidth;}
            else for(size_t p=0;p<word.size();){
                size_t start=p;
                float glyphWidth=glyphAdvance(glyphIndex(nextGlyph(word,p)),size,Face::Body);
                if(!line.empty()&&lineWidth+glyphWidth>w)flush();
                line+=word.substr(start,p-start);lineWidth+=glyphWidth;
            }
            pos=end;
        }
        if(!line.empty())flush();
        return dy;
    }
    void panel(float x,float y,float w,float h,const std::string& title="",ui::Emblem symbol=ui::Emblem::None){
        block(x,y,w,h);
        quad(x+3,y+5,w,h,{.012f,.016f,.019f,.46f});
        gradient(x,y,w,h,{.115f,.122f,.121f,.985f},ui::panelBg);
        if(!title.empty()){
            gradient(x+1,y+1,w-2,47,{.21f,.19f,.155f,.85f},{.12f,.125f,.124f,.4f});
            float tx=x+19;
            if(symbol!=ui::Emblem::None){
                emblem(x+12,y+8,32,symbol);
                frame(x+11,y+7,34,34,ui::bevel);
                tx=x+56;
            }
            heading(tx,y+7,title,25,ui::gold);
            quad(x+10,y+48,w-20,1,ui::bevel);
        }
        frame(x,y,w,h,ui::brass);
        for(float corner:{x,x+w-14}){
            quad(corner,y,14,2,ui::gold);
            quad(corner,y+h-2,14,2,ui::brass);
        }
    }
    void bar(float x,float y,float w,float h,float ratio,glm::vec4 color){quad(x,y,w,h,{.10f,.13f,.15f,1});quad(x,y,w*std::clamp(ratio,0.f,1.f),h,color);}
    bool button(float x,float y,float w,float h,const std::string& label,bool enabled=true,const std::string& reason="",bool selected=false,float size=17,ui::Emblem symbol=ui::Emblem::None){
        bool hot=inside(x,y,w,h);if(hot)overUi=true;
        glm::vec4 top=!enabled?glm::vec4(.14f,.14f,.14f,1):hot?glm::vec4(.27f,.25f,.19f,1):selected?glm::vec4(.16f,.26f,.24f,1):glm::vec4(.19f,.205f,.20f,1);
        gradient(x,y,w,h,top,top*glm::vec4(.61f,.66f,.71f,1));
        frame(x,y,w,h,(selected||hot)&&enabled?ui::gold:enabled?ui::brass:ui::bevel);
        glm::vec4 labelColor=enabled?ui::text:ui::dim;
        if(symbol==ui::Emblem::None){
            textCentered(x+w*.5f,y+h*.5f-size*.69f,label,size,labelColor);
        }else{
            float side=std::min(42.f,h-8.f);
            emblem(x+6,y+(h-side)*.5f,side,symbol,enabled?1.f:.53f);
            text(x+side+18,y+h*.5f-size*.69f,label,size,labelColor);
        }
        if(hot&&!enabled&&!reason.empty())tooltip=reason;
        if(hot&&enabled&&pressed){++clicks;pressed=false;return true;}
        if(hot&&!enabled&&pressed)pressed=false;
        return false;
    }
    void drawTooltip(){
        if(tooltip.empty())return;
        float w=std::min(520.f,textWidth(tooltip,15)+28);float x=std::min(mouse.x+18,width-w-8);
        float hgt=paragraph(0,0,w-24,tooltip,15,ui::text,1.45f,true)+18;
        float y=mouse.y+22+hgt<=height-8?mouse.y+22:std::max(8.f,mouse.y-hgt-8);
        gradient(x,y,w,hgt,{.13f,.14f,.14f,.99f},ui::panelBg);frame(x,y,w,hgt,ui::brass);
        quad(x,y,2,hgt,ui::gold);
        paragraph(x+14,y+9,w-24,tooltip,15,ui::text);
    }
    void draw(){
        if(vertices.empty())return;
        glDisable(GL_DEPTH_TEST);glDisable(GL_CULL_FACE);glEnable(GL_BLEND);glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);
        glUseProgram(program);glUniform2f(glGetUniformLocation(program,"viewport"),width*scale,height*scale);
        glActiveTexture(GL_TEXTURE0);glBindTexture(GL_TEXTURE_2D,atlas);glUniform1i(glGetUniformLocation(program,"atlas"),0);
        glActiveTexture(GL_TEXTURE1);glBindTexture(GL_TEXTURE_2D,icons);glUniform1i(glGetUniformLocation(program,"icons"),1);
        glActiveTexture(GL_TEXTURE2);glBindTexture(GL_TEXTURE_2D,emblems);glUniform1i(glGetUniformLocation(program,"emblems"),2);
        glActiveTexture(GL_TEXTURE3);glBindTexture(GL_TEXTURE_2D,cover);glUniform1i(glGetUniformLocation(program,"cover"),3);
        glActiveTexture(GL_TEXTURE0);
        glBindVertexArray(vao);glBindBuffer(GL_ARRAY_BUFFER,vbo);
        glBufferData(GL_ARRAY_BUFFER,GLsizeiptr(vertices.size()*sizeof(V)),vertices.data(),GL_STREAM_DRAW);
        glDrawArrays(GL_TRIANGLES,0,GLsizei(vertices.size()));glDisable(GL_BLEND);vertices.clear();
    }
};
