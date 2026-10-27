#pragma once
#include "combat.hpp"
#include <array>
#include <deque>

// Small batched HUD: one atlas and one draw call, no new runtime library.
struct CombatHud {
    struct Vertex2 { glm::vec2 p,uv;glm::vec4 color; };
    std::vector<Vertex2> vertices;
    GLuint shader=0,vao=0,vbo=0;
    Overlay font;
    float width=1440,height=900,scale=1;
    explicit CombatHud(const fs::path& assets):font(assets/"combat-font.png") {
        if(!font.texture)throw std::runtime_error("Missing assets/combat-font.png");
        shader=program(R"(#version 330 core
layout(location=0) in vec2 p;layout(location=1) in vec2 tex;layout(location=2) in vec4 color;
uniform vec2 viewport;out vec2 uv;out vec4 tint;
void main(){gl_Position=vec4(p.x/viewport.x*2.-1.,1.-p.y/viewport.y*2.,0,1);uv=tex;tint=color;}
)",R"(#version 330 core
in vec2 uv;in vec4 tint;out vec4 fragColor;uniform sampler2D atlas;
void main(){fragColor=vec4(tint.rgb,tint.a*(uv.x<0.?1.:texture(atlas,uv).a));}
)");
        glGenVertexArrays(1,&vao);glGenBuffers(1,&vbo);glBindVertexArray(vao);glBindBuffer(GL_ARRAY_BUFFER,vbo);
        glEnableVertexAttribArray(0);glVertexAttribPointer(0,2,GL_FLOAT,GL_FALSE,sizeof(Vertex2),nullptr);
        glEnableVertexAttribArray(1);glVertexAttribPointer(1,2,GL_FLOAT,GL_FALSE,sizeof(Vertex2),reinterpret_cast<void*>(sizeof(glm::vec2)));
        glEnableVertexAttribArray(2);glVertexAttribPointer(2,4,GL_FLOAT,GL_FALSE,sizeof(Vertex2),reinterpret_cast<void*>(2*sizeof(glm::vec2)));
    }
    ~CombatHud(){glDeleteProgram(shader);glDeleteVertexArrays(1,&vao);glDeleteBuffers(1,&vbo);}
    void resize(int w,int h){scale=std::min(float(w)/1440.f,float(h)/900.f);width=float(w)/scale;height=float(h)/scale;}
    void quad(float x,float y,float w,float h,glm::vec4 color,glm::vec2 uv0={-1,-1},glm::vec2 uv1={-1,-1}){
        Vertex2 a{{x*scale,y*scale},uv0,color},b{{(x+w)*scale,y*scale},{uv1.x,uv0.y},color};
        Vertex2 c{{(x+w)*scale,(y+h)*scale},uv1,color},d{{x*scale,(y+h)*scale},{uv0.x,uv1.y},color};
        vertices.insert(vertices.end(),{a,b,c,a,c,d});
    }
    void text(float x,float y,const std::string& s,float size=17,glm::vec4 color={.84f,.87f,.86f,1}){
        for(unsigned char ch:s){
            if(ch>=32&&ch<128){int i=ch-32;glm::vec2 uv(float(i%16)/16.f,float(i/16)/6.f);
                quad(x,y,size,size*4/3,color,uv,uv+glm::vec2(1.f/16,1.f/6));}
            x+=size*.65f;
        }
    }
    void panel(float x,float y,float w,float h){quad(x,y,w,h,{.035f,.050f,.065f,.95f});quad(x,y,w,1,{.37f,.41f,.40f,.5f});}
    void bar(float x,float y,float w,float ratio,glm::vec4 color){quad(x,y,w,6,{.13f,.16f,.18f,1});quad(x,y,w*std::clamp(ratio,0.f,1.f),6,color);}
    void draw(){
        glDisable(GL_DEPTH_TEST);glEnable(GL_BLEND);glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);
        glUseProgram(shader);glUniform2f(glGetUniformLocation(shader,"viewport"),width*scale,height*scale);
        glActiveTexture(GL_TEXTURE0);glBindTexture(GL_TEXTURE_2D,font.texture);integer(shader,"atlas",0);
        glBindVertexArray(vao);glBindBuffer(GL_ARRAY_BUFFER,vbo);
        glBufferData(GL_ARRAY_BUFFER,GLsizeiptr(vertices.size()*sizeof(Vertex2)),vertices.data(),GL_STREAM_DRAW);
        glDrawArrays(GL_TRIANGLES,0,GLsizei(vertices.size()));glDisable(GL_BLEND);vertices.clear();
    }
};

struct CombatView {
    combat::Game game;
    CombatHud hud;
    Mesh gridMesh,enemyMesh;
    GpuMesh grid,enemy;
    std::vector<glm::vec3> positions;
    std::deque<combat::Cell> route;
    combat::Cell hover;
    int selected=-1,hoverTarget=-1,mode=0; // 0: contextual move/attack, 1: weapon skill
    std::array<bool,GLFW_KEY_LAST+1> keys{};
    bool lastClick=false,busy=false,showGrid=true;
    float aiTimer=0,hitTimer=0;
    int lastHit=-1;
    glm::vec3 effectFrom{},effectTo{};
    explicit CombatView(const fs::path& assets):hud(assets),grid(gridMesh),enemy(enemyMesh){}
    bool pressed(GLFWwindow* window,int key){bool down=glfwGetKey(window,key)==GLFW_PRESS;bool edge=down&&!keys[size_t(key)];keys[size_t(key)]=down;return edge;}
    void sync(){positions.clear();for(const auto& a:game.actors)positions.push_back(combat::world(a.cell));route.clear();selected=-1;mode=0;aiTimer=0;}
    void choose(combat::Class type){game.choose(type);sync();}
    glm::vec2 screen(glm::vec3 point,const glm::mat4& vp)const{
        glm::vec4 p=vp*glm::vec4(point,1);p/=p.w;return {(p.x*.5f+.5f)*hud.width,(.5f-p.y*.5f)*hud.height};
    }
    bool overUi(float x,float y)const{
        return game.phase==combat::Phase::ChooseClass||y<126||y>hud.height-158||
            (x>hud.width-282&&y<570)||(x<340&&y>hud.height-330);
    }
    int targetAt(float x,float y,const glm::mat4& vp)const{
        // Body picking supplements tile picking for tall models; closest projected body wins.
        int result=-1;float best=34;
        for(size_t i=1;i<game.actors.size();++i)if(game.actors[i].alive()){
            auto p=screen(positions[i]+glm::vec3(0,.9f,0),vp);float d=glm::length(p-glm::vec2(x,y));
            auto label=screen(positions[i]+glm::vec3(0,game.actors[i].kind==combat::Kind::Ogre?2.8f:2.15f,0),vp);
            if(std::abs(x-label.x)<48&&y>=label.y&&y<=label.y+28)d=0;
            if(d<best){best=d;result=int(i);}
        }
        if(result<0)result=game.occupant(hover);return result>0?result:-1;
    }
    void recordHit(int target,glm::vec3 from){
        if(target<0||target>=int(positions.size()))return;
        lastHit=target;hitTimer=.65f;effectFrom=from+glm::vec3(0,.8f,0);effectTo=positions[size_t(target)]+glm::vec3(0,.8f,0);
    }
    void action(int button){
        if(busy)return;
        switch(button){
        case 0: mode=0;break;
        case 1: mode=1;break;
        case 2: game.guard();break;
        case 3: game.potion();break;
        case 4: game.swapWeapon();mode=0;break;
        case 5: if(game.active()==0){game.endTurn();mode=0;}break;
        }
    }
    void input(GLFWwindow* window,const glm::mat4& vp,int w,int h,float dt,bool automated){
        hud.resize(w,h);hitTimer=std::max(0.f,hitTimer-dt);
        double mx,my;glfwGetCursorPos(window,&mx,&my);int ww,wh;glfwGetWindowSize(window,&ww,&wh);
        float x=float(mx)*float(w)/float(std::max(1,ww))/hud.scale,y=float(my)*float(h)/float(std::max(1,wh))/hud.scale;
        bool click=glfwGetMouseButton(window,GLFW_MOUSE_BUTTON_LEFT)==GLFW_PRESS;bool clicked=click&&!lastClick;lastClick=click;
        if(automated)clicked=false;
        hover={};
        if(!overUi(x,y)){
            glm::mat4 inverse=glm::inverse(vp);
            glm::vec4 a=inverse*glm::vec4(x/hud.width*2-1,1-y/hud.height*2,-1,1),b=inverse*glm::vec4(x/hud.width*2-1,1-y/hud.height*2,1,1);a/=a.w;b/=b.w;
            if(std::abs(b.y-a.y)>.0001f){auto p=glm::vec3(a)+(glm::vec3(b)-glm::vec3(a))*(-a.y/(b.y-a.y));
                hover={int(std::floor((p.x-layout::originX)/layout::cellSize)),int(std::floor((p.z-layout::originZ)/layout::cellSize))};}
        }
        hoverTarget=overUi(x,y)?-1:targetAt(x,y,vp);
        if(automated){hover={};hoverTarget=-1;}
        bool f1=pressed(window,GLFW_KEY_F1),f2=pressed(window,GLFW_KEY_F2),f3=pressed(window,GLFW_KEY_F3);
        if(game.phase==combat::Phase::ChooseClass){
            float start=hud.width*.5f-510;
            if(f1||(clicked&&x>=start&&x<start+320&&y>hud.height*.5f-40&&y<hud.height*.5f+200))choose(combat::Class::Warrior);
            if(f2||(clicked&&x>=start+350&&x<start+670&&y>hud.height*.5f-40&&y<hud.height*.5f+200))choose(combat::Class::Rogue);
            if(f3||(clicked&&x>=start+700&&x<start+1020&&y>hud.height*.5f-40&&y<hud.height*.5f+200))choose(combat::Class::Mage);
            return;
        }
        if(pressed(window,GLFW_KEY_R)&&game.phase==combat::Phase::Defeat){game.restoreCheckpoint();sync();}
        if(pressed(window,GLFW_KEY_N)&&game.phase==combat::Phase::Victory){game.nextFloor();sync();}
        if(pressed(window,GLFW_KEY_F5)&&game.phase==combat::Phase::Complete){game=combat::Game{};route.clear();return;}
        if(pressed(window,GLFW_KEY_G))showGrid=!showGrid;
        if(pressed(window,GLFW_KEY_B)&&!busy){route.clear();game.engage();}
        if(pressed(window,GLFW_KEY_Q))action(0);
        if(pressed(window,GLFW_KEY_E))action(1);
        if(pressed(window,GLFW_KEY_F))action(2);
        if(pressed(window,GLFW_KEY_H))action(3);
        if(pressed(window,GLFW_KEY_V))action(4);
        if(pressed(window,GLFW_KEY_SPACE))action(5);
        if(pressed(window,GLFW_KEY_I)&&!busy)game.swapArmor();
        if(pressed(window,GLFW_KEY_BACKSPACE)){if(game.phase!=combat::Phase::Battle)route.clear();mode=0;}
        if(clicked&&!busy){
            float bx=hud.width*.5f-618;
            if(y>hud.height-100&&y<hud.height-42){
                for(int i=0;i<6;++i)if(x>=bx+float(i)*208&&x<bx+float(i)*208+198)action(i);
            }else if(!overUi(x,y)){
                int target=hoverTarget;
                if(target>0){
                    selected=target;
                    if(game.phase==combat::Phase::Explore){game.engage();game.message("Enemy selected. Click again to attack.");}
                    else if(game.active()==0){if(game.attack(target,mode==1))recordHit(target,positions[0]);}
                }else if(mode==0&&game.playerCanAct()){
                    auto p=game.movePreview(hover);
                    if(p.allowed){auto steps=game.path(0,hover);route.assign(steps.begin(),steps.end());
                        if(game.phase==combat::Phase::Battle)game.move(hover);
                    }else game.message(p.reason);
                }
            }
        }
        // Logical cells update atomically; visual positions walk through every validated cell.
        busy=false;
        if(!route.empty()){
            auto goal=combat::world(route.front());auto delta=goal-positions[0];float length=glm::length(delta);
            float step=dt*5;
            if(length<=step){
                positions[0]=goal;
                if(game.phase==combat::Phase::Explore||game.phase==combat::Phase::Victory){game.move(route.front());if(game.phase==combat::Phase::Battle)route.clear();}
                if(!route.empty())route.pop_front();
            }else positions[0]+=delta/length*step;
            busy=!route.empty();
        }
        for(size_t i=1;i<positions.size();++i){auto delta=combat::world(game.actors[i].cell)-positions[i];float length=glm::length(delta);
            if(length>.005f){positions[i]+=delta/length*std::min(length,dt*4.f);busy=true;}}
        if(!busy&&game.active()>0){aiTimer+=dt;if(aiTimer>.48f){
            auto before=game.actors[0].hp;int source=game.active();game.aiStep();aiTimer=0;
            if(game.actors[0].hp<before)recordHit(0,positions[size_t(source)]);
        }}else aiTimer=0;
    }
    void tile(combat::Cell cell,glm::vec3 color,float thickness=.035f){
        auto p=combat::world(cell);float r=.66f,y=.055f;
        glm::vec3 a=p+glm::vec3(-r,y,-r),b=p+glm::vec3(r,y,-r),c=p+glm::vec3(r,y,r),d=p+glm::vec3(-r,y,r);
        for(auto edge:{std::pair{a,b},std::pair{b,c},std::pair{c,d},std::pair{d,a}}){auto side=glm::normalize(glm::cross(edge.second-edge.first,glm::vec3(0,1,0)))*thickness;gridMesh.quad(edge.first-side,edge.first+side,edge.second+side,edge.second-side,color,7);}
    }
    void geometry(float time){
        gridMesh.vertices.clear();enemyMesh.vertices.clear();
        if(game.actors.empty()){grid.update(gridMesh);enemy.update(enemyMesh);return;}
        if(showGrid){
            for(int y=1;y<layout::rows-1;++y)for(int x=1;x<layout::columns-1;++x){
                if(layout::isBlocked(x,y))continue;
                glm::vec3 color(.016f,.029f,.034f);
                if(game.active()==0&&game.movePreview({x,y}).allowed)color={.025f,.13f,.12f};
                tile({x,y},color,.012f);
            }
        }
        if(game.playerCanAct()&&hover.x>=0&&mode==0){auto p=game.movePreview(hover);if(p.allowed){for(auto c:game.path(0,hover))tile(c,{.08f,.40f,.30f});}}
        if(mode==1&&hoverTarget>0&&game.attackPreview(hoverTarget,true).allowed){
            auto impact=game.actors[size_t(hoverTarget)].cell;tile(impact,{.65f,.24f,.04f},.06f);
            if(game.actors[0].weapon==combat::Weapon::Staff)for(combat::Cell offset:std::array<combat::Cell,4>{{{1,0},{-1,0},{0,1},{0,-1}}}){
                combat::Cell c{impact.x+offset.x,impact.y+offset.y};if(game.lineOfSight(impact,c))tile(c,{.32f,.12f,.025f});
            }
        }
        for(size_t i=0;i<game.actors.size();++i){
            const auto& a=game.actors[i];if(!a.alive())continue;
            tile(a.cell,i==0?glm::vec3(.08f,.45f,.34f):glm::vec3(.45f,.07f,.035f),int(i)==game.active()?.055f:.028f);
            if(int(i)==selected)tile(a.cell,{.65f,.37f,.08f},.065f);
            if(i==0)continue;
            glm::vec3 p=positions[i],c(.48f,.25f,.19f);float bob=std::sin(time*3+float(i))*.04f;
            if(a.kind==combat::Kind::Slime){
                enemyMesh.sphere(p+glm::vec3(0,.40f+bob,0),{.52f,.40f+bob,.48f},{.15f,.45f,.23f},6);
                for(float x:{-.17f,.17f})enemyMesh.sphere(p+glm::vec3(x,.52f,-.40f),{.065f,.07f,.04f},{.3f,.6f,.1f},7);
            }else if(a.kind==combat::Kind::Rat){
                enemyMesh.sphere(p+glm::vec3(0,.38f+bob,0),{.42f,.32f,.55f},c,6);
                enemyMesh.sphere(p+glm::vec3(0,.40f,-.46f),{.25f,.23f,.26f},{.38f,.23f,.23f},6);
                for(float x:{-.19f,.19f}){enemyMesh.sphere(p+glm::vec3(x,.65f,-.38f),{.12f,.15f,.07f},c,6);enemyMesh.sphere(p+glm::vec3(x*.65f,.46f,-.66f),{.045f,.045f,.045f},{.7f,.12f,.04f},7);}
                enemyMesh.beam(p+glm::vec3(0,.2f,.5f),p+glm::vec3(.3f,.1f,.78f),.045f,{.4f,.18f,.2f},6);
            }else if(a.kind==combat::Kind::Ghost){
                p.y+=.3f+bob*3;enemyMesh.cylinder(p+glm::vec3(0,.65f,0),.47f,.20f,1.1f,{.035f,.14f,.19f},7);
                enemyMesh.sphere(p+glm::vec3(0,1.3f,0),{.25f,.29f,.24f},{.07f,.25f,.32f},7);
                for(float x:{-.10f,.10f})enemyMesh.sphere(p+glm::vec3(x,1.32f,-.22f),{.04f,.07f,.03f},{.5f,.7f,.65f},7);
            }else{
                bool ogre=a.kind==combat::Kind::Ogre;float s=ogre?1.45f:1.f;
                c=ogre?glm::vec3(.35f,.40f,.21f):glm::vec3(.7f,.65f,.49f);
                auto at=[&](float x,float y,float z){return p+glm::vec3(x,y+bob,z)*s;};
                enemyMesh.sphere(at(0,1.55f,0),glm::vec3(.24f,.28f,.23f)*s,c,6);
                if(ogre)enemyMesh.sphere(at(0,.95f,0),glm::vec3(.40f,.53f,.30f)*s,c,6);
                else{
                    enemyMesh.beam(at(0,.65f,0),at(0,1.3f,0),.07f,c,6);
                    for(int r=0;r<4;++r)enemyMesh.beam(at(-.23f,.92f+float(r)*.09f,0),at(.23f,.92f+float(r)*.09f,0),.035f,c,6);
                }
                for(float side:{-1.f,1.f}){
                    enemyMesh.beam(at(side*.16f,.15f,0),at(side*.18f,.70f,0),ogre?.14f:.065f,c,6);
                    enemyMesh.beam(at(side*.23f,1.25f,0),at(side*.44f,.80f,-.12f),ogre?.12f:.05f,c,6);
                    enemyMesh.sphere(at(side*.09f,1.60f,-.21f),glm::vec3(.045f)*s,{.5f,.10f,.025f},7);
                }
                enemyMesh.beam(at(.45f,.45f,-.12f),at(.45f,1.25f,-.12f),ogre?.09f:.025f,{.25f,.15f,.07f},2);
                if(ogre)enemyMesh.block(at(.45f,1.25f,-.12f),glm::vec3(.32f,.45f,.30f),.04f,{.27f,.24f,.22f},0);
            }
        }
        if(hitTimer>.3f)enemyMesh.beam(effectFrom,effectTo,.025f,{.6f,.23f,.05f},7);
        grid.update(gridMesh);enemy.update(enemyMesh);
    }
    void drawHud(const glm::mat4& vp){
        using combat::Phase;
        auto gold=glm::vec4(.84f,.68f,.39f,1),mint=glm::vec4(.36f,.84f,.69f,1),muted=glm::vec4(.52f,.60f,.63f,1);
        if(game.phase==Phase::ChooseClass){
            hud.quad(0,0,hud.width,hud.height,{.012f,.021f,.033f,.76f});float cx=hud.width*.5f,cy=hud.height*.5f;
            hud.text(cx-130,cy-210,"THE OSSUARY",38,gold);hud.text(cx-255,cy-145,"TACTICAL COMBAT / CHOOSE YOUR HERO",23);
            const char* names[]={"F1   WARRIOR","F2   ROGUE","F3   MAGE"};
            const char* stats[]={"100 HP   3 ARMOR   8 AP","76 HP   1 ARMOR   9 AP","66 HP   1 ARMOR   8 AP"};
            const char* weapon[]={"SWORD / SHIELD BASH","DAGGERS / VENOM STRIKE","STAFF / EMBER BURST"};
            const char* desc[]={"Stun a foe. Hold the line.","Fast steps. Poison attacks.","Ranged fire. Splash damage."};
            for(int i=0;i<3;++i){float x=cx-510+float(i)*350;hud.panel(x,cy-40,320,240);hud.quad(x,cy-40,320,3,i==1?mint:gold);
                hud.text(x+20,cy-15,names[i],25,gold);hud.text(x+20,cy+45,stats[i],16);hud.text(x+20,cy+90,weapon[i],16,mint);hud.text(x+20,cy+137,desc[i],16,muted);
                hud.text(x+20,cy+178,"CLICK TO SELECT",15,gold);}
            hud.text(cx-338,cy+249,"One hero. Shared action points. Every tile matters.",20,muted);
            hud.text(cx-292,cy+292,"Move freely until enemies detect you. B: engage.",18,muted);hud.draw();return;
        }
        if(game.actors.empty())return;
        const auto& hero=game.actors[0];auto weapon=combat::weaponStats(hero.weapon);
        hud.panel(20,18,350,104);hud.text(35,25,"THE OSSUARY",21,gold);
        hud.text(35,57,hero.name+"  LV "+std::to_string(hero.level)+"  HP "+std::to_string(hero.hp)+"/"+std::to_string(hero.maxHp),17);
        hud.bar(38,91,310,float(hero.hp)/float(hero.maxHp),mint);
        std::string state=game.phase==Phase::Battle?(game.active()==0?"YOUR TURN":"ENEMY TURN"):"EXPLORATION";
        if(game.phase==Phase::Victory)state="VICTORY / N: NEXT";
        if(game.phase==Phase::Defeat)state="DEFEAT / R: RETRY";
        if(game.phase==Phase::Complete)state="COMPLETE / F5: NEW";
        float topX=hud.width*.5f-200;hud.panel(topX,18,440,104);
        hud.text(topX+18,26,state,23,game.active()>0?gold:mint);
        hud.text(topX+18,64,"ENCOUNTER "+std::to_string(game.floor)+"/3   ROUND "+std::to_string(game.round),17,muted);
        auto order=game.turnOrder();float tx=topX+450;
        hud.text(tx,25,"TURN ORDER",15,gold);
        for(size_t i=0;i<order.size();++i)hud.text(tx,48+float(i)*17,(i==0?"> ":"  ")+game.actors[size_t(order[i])].name,14,i==0?mint:muted);
        for(size_t i=0;i<game.actors.size();++i){const auto& a=game.actors[i];if(!a.alive())continue;
            auto p=screen(positions[i]+glm::vec3(0,a.kind==combat::Kind::Ogre?2.8f:2.15f,0),vp);
            hud.quad(p.x-48,p.y,96,28,{.025f,.04f,.05f,.88f});hud.text(p.x-44,p.y,a.name,14,i==0?mint:gold);
            hud.bar(p.x-44,p.y+23,88,float(a.hp)/float(a.maxHp),i==0?mint:glm::vec4(.85f,.30f,.21f,1));
            if(int(i)==lastHit&&hitTimer>0)hud.text(p.x-30,p.y-30-(.65f-hitTimer)*30,"HIT",22,gold);
        }
        float rx=hud.width-278;hud.panel(rx,145,258,272);
        hud.text(rx+16,154,"LOADOUT",16,gold);
        hud.text(rx+16,184,std::string(weapon.name)+"  DMG "+std::to_string(weapon.damage+hero.power),18);
        hud.text(rx+16,213,"RANGE "+std::to_string(weapon.range)+"  ARMOR "+std::to_string(hero.defense()),16,muted);
        hud.text(rx+16,240,"INIT "+std::to_string(hero.initiative())+"  STEP "+std::to_string(hero.stepCost())+" AP",16,muted);
        hud.text(rx+16,270,"XP "+std::to_string(hero.xp)+" / "+std::to_string(hero.level*90),16,mint);
        hud.text(rx+16,298,"I: "+std::string(hero.plated?"PLATE":"LIGHT ARMOR"),15,muted);
        hud.text(rx+16,328,game.status(hero),14,gold);
        hud.text(rx+16,360,"B: ENGAGE   G: GRID",14,muted);
        hud.text(rx+16,384,"RMB: ORBIT / WHEEL: ZOOM",13,muted);
        hud.panel(20,hud.height-330,320,160);hud.text(35,hud.height-323,"COMBAT LOG",14,gold);
        size_t first=game.log.size()>5?game.log.size()-5:0;
        for(size_t i=first;i<game.log.size();++i)hud.text(35,hud.height-295+float(i-first)*23,game.log[i].substr(0,43),10.5f,i+1==game.log.size()?glm::vec4(.84f,.87f,.86f,1):muted);
        int target=hoverTarget;if(target<=0)target=selected;
        std::string hint="LMB: move / attack   E: skill   BACKSPACE: cancel route";
        if(target>0&&target<int(game.actors.size())&&game.actors[size_t(target)].alive()){
            const auto& a=game.actors[size_t(target)];auto p=game.attackPreview(target,mode==1);
            hint=a.name+"  HP "+std::to_string(a.hp)+"  "+p.reason+" | "+std::to_string(p.cost)+" AP / "+std::to_string(p.damage)+" damage";
            hud.panel(rx,430,258,130);hud.text(rx+16,440,a.name+" / HP "+std::to_string(a.hp),18,gold);
            hud.text(rx+16,471,"ARMOR "+std::to_string(a.defense())+"  AP "+std::to_string(a.ap),15,muted);
            hud.text(rx+16,500,game.status(a),14,mint);
            hud.text(rx+16,528,p.reason,13,muted);
        }else if(hover.x>=0){auto p=game.movePreview(hover);hint=p.reason+" | "+std::to_string(p.cost)+" AP";}
        if(busy)hint="Moving...";
        if(game.phase==Phase::Defeat)hint="R: restore encounter checkpoint (this session)";
        if(game.phase==Phase::Complete)hint="All encounters cleared. F5: choose another class.";
        hud.panel(20,hud.height-158,hud.width-40,140);hud.text(38,hud.height-149,hint,16,gold);
        hud.text(hud.width-260,hud.height-149,"AP "+std::to_string(hero.ap)+" / "+std::to_string(hero.maxAp),21,mint);
        for(int i=0;i<hero.maxAp;++i)hud.quad(hud.width-260+float(i)*21,hud.height-119,16,5,i<hero.ap?mint:glm::vec4(.16f,.21f,.23f,1));
        const std::string labels[]={"Q  ATTACK","E  SKILL","F  GUARD","H  POTION","V  WEAPON","SPACE  END TURN"};
        const char* effects[]={"stun 1 turn","poison 2 turns","slow 2 turns","burn + splash"};
        bool inBattle=game.phase==Phase::Battle;
        const std::string sub[]={std::to_string(weapon.cost)+" AP / range "+std::to_string(weapon.range),hero.cooldown?"Cooldown: "+std::to_string(hero.cooldown):"4 AP / "+std::string(effects[int(hero.weapon)]),"2 AP / +4 armor",std::string(inBattle?"2 AP / ":"Free / ")+std::to_string(hero.potions)+" left",inBattle?"2 AP / cycle loadout":"Free / cycle loadout","Refresh next turn"};
        const bool available[]={game.active()==0&&hero.ap>=weapon.cost,game.active()==0&&hero.ap>=4&&hero.cooldown==0,
            game.active()==0&&hero.ap>=2&&hero.guard==0,game.playerCanAct()&&hero.potions>0&&hero.hp<hero.maxHp&&(game.phase!=Phase::Battle||hero.ap>=2),
            game.playerCanAct()&&(game.phase!=Phase::Battle||hero.ap>=2),game.active()==0};
        float bx=hud.width*.5f-618;
        for(int i=0;i<6;++i){float x=bx+float(i)*208;hud.quad(x,hud.height-100,198,58,available[i]&&!busy?glm::vec4(.11f,.18f,.20f,1):glm::vec4(.065f,.085f,.10f,1));
            if(i==mode)hud.quad(x,hud.height-100,198,2,gold);
            hud.text(x+10,hud.height-94,labels[i],17,available[i]&&!busy?glm::vec4(.87f,.89f,.85f,1):muted);
            hud.text(x+10,hud.height-67,sub[i],12,muted);}
        hud.text(38,hud.height-35,"1 / 2 / 3: CAMERA    TAB: HUD    F12: CAPTURE",12,muted);hud.draw();
    }
};
