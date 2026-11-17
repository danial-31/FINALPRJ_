#include "app.hpp"
#include "hero_animation.hpp"
#include <glm/gtc/matrix_transform.hpp>
#include <chrono>
#include <cmath>
#include <fstream>
#include <functional>
#include <iostream>
#include <iomanip>
#include <numeric>
#include <sstream>

using world::Cell;
using combat::Phase;

static bool automated(const LaunchOptions& o){return o.smoke||o.benchmark||!o.capture.empty();}

App::App(GLFWwindow* w,const fs::path& r,const LaunchOptions& o):window(w),root(r),options(o){
    gpuName=reinterpret_cast<const char*>(glGetString(GL_RENDERER));
    saveDir=root/(automated(o)?"test-saves":"saves");   // automated runs never touch player saves
    settings.load(root/"settings.ini");
    if(automated(o)){settings.vsync=false;settings.fullscreen=false;}
    if(!o.technique)settings.technique=false;
    renderer=std::make_unique<Renderer>(root);
    ui.init(renderer->uiProgram,renderer->font.id,renderer->actionIcons.id,renderer->interfaceEmblems.id,renderer->coverArt.id);
    audio=std::make_unique<Audio>();
    dynamicGpu=std::make_unique<GpuMesh>();heroGpu=std::make_unique<GpuMesh>();
    {std::ifstream t(root/"assets/team.txt");std::string a,b;if(std::getline(t,a)&&!a.empty())team=a;if(std::getline(t,b)&&!b.empty())topic=b;}
    menuWorld.newGame(combat::Class::Warrior);
    menuWorld.enterZone("village",'@');
    menuWorld.flags.insert("menu_preview");
    applySettings(true);
    glfwSetWindowUserPointer(window,this);
    glfwSetScrollCallback(window,[](GLFWwindow* wnd,double,double dy){static_cast<App*>(glfwGetWindowUserPointer(wnd))->scroll+=float(dy);});
    lastTime=glfwGetTime();
    if(!o.zone.empty()&&!o.menu){
        combat::Class c=o.cls=="rogue"?combat::Class::Rogue:o.cls=="mage"?combat::Class::Mage:combat::Class::Warrior;
        startNewGame(c);
        if(o.zone!=game.zoneId)game.enterZone(o.zone,'@');
        game.requests.clear();syncActors(true);refreshVisuals(true);
        if(o.battle){
            int target=-1;float best=1e9f;
            for(size_t i=1;i<game.game.actors.size();++i){const auto& a=game.game.actors[i];if(!a.alive()||a.kind==combat::Kind::Pylon)continue;
                float d=glm::length(a.pos-game.hero().pos)-(a.boss?100.f:0.f);if(d<best){best=d;target=int(i);}}
            if(target>0){
                Cell t=game.game.actors[size_t(target)].cell;
                for(int k=2;k<6;++k){bool done=false;for(Cell d:{Cell{0,k},Cell{k,0},Cell{-k,0},Cell{0,-k}}){Cell q{t.x+d.x,t.y+d.y};
                    if(!game.map.blocked(q.x,q.y)&&game.game.occupant(q)<0&&game.game.lineOfSight(q,t)){game.game.place(0,q);done=true;break;}}if(done)break;}
                syncActors(true);game.game.engage(target);
            }
        }
        screen=Screen::Game;
    }
    // Development aid for screenshots: open a specific window (--window name).
    const std::string& wnd=o.window;
    if(!wnd.empty()){
        if(screen!=Screen::Game&&wnd!="classes"){
            startNewGame(combat::Class::Warrior);
            if(wnd=="dialog"||wnd=="shrine"){
                game.enterZone("village",'@');game.requests.clear();syncActors(true);refreshVisuals(true);
            }
        }
        if(wnd=="classes"){screen=Screen::ClassSelect;}
        auto talkTo=[&](const char* npc){for(size_t i=0;i<game.entities.size();++i)if(std::string(game.entities[i].def.id)==npc){game.interact(int(i));handleRequests();return;}};
        if(wnd=="inventory"||wnd=="shop"){for(auto id:{"potion","greater_potion","ore","herb","silk","militia_sword","leather_armor","wolf_tooth","mine_key"})game.addItem(id,1,0,false);selected=1;}
        if(wnd=="inventory")win=Window::Inventory;
        else if(wnd=="shop"){shopNpc="merchant";win=Window::Shop;}
        else if(wnd=="character"){game.hero().level=4;game.hero().skillPoints=2;game.hero().talents=1;win=Window::Character;}
        else if(wnd=="journal"){game.quests["cargo"]=1;game.quests["antidote"]=1;game.notes.push_back("guard_journal");win=Window::Journal;}
        else if(wnd=="map")win=Window::Map;
        else if(wnd=="dialog")talkTo("elder");
        else if(wnd=="craft")win=Window::Craft;
        else if(wnd=="smith")win=Window::Smith;
        else if(wnd=="shrine")talkTo("shrine_village");
        else if(wnd=="pause")win=Window::Pause;
        else if(wnd=="settings")win=Window::Settings;
        else if(wnd=="help")win=Window::Help;
        else if(wnd=="load")win=Window::Load;
        else if(wnd=="defeat")game.game.damage(0,game.hero().hp);
        else if(wnd=="reward"){game.rewardChoices={"chitin_blade","queen_fangs","silkweaver_staff"};win=Window::Reward;}
        else if(wnd=="epilogue")win=Window::Epilogue;
        else if(wnd=="ending")win=Window::Ending;
    }
}
App::~App(){if(!automated(options))settings.save(root/"settings.ini");}

rpg::Campaign& App::world(){return screen==Screen::Game?game:menuWorld;}
bool App::held(int k) const{return !options.benchmark&&glfwGetKey(window,k)==GLFW_PRESS;}
bool App::key(int k){bool down=held(k);bool edge=down&&!keys[size_t(k)];keys[size_t(k)]=down;return edge;}
fs::path App::slotPath(int slot) const{return saveDir/(slot==0?std::string("autosave.sav"):"slot"+std::to_string(slot)+".sav");}

void App::applySettings(bool windowMode){
    if(windowMode&&!automated(options)){
        GLFWmonitor* monitor=glfwGetPrimaryMonitor();
        if(settings.fullscreen&&monitor){const GLFWvidmode* vm=glfwGetVideoMode(monitor);glfwSetWindowMonitor(window,monitor,0,0,vm->width,vm->height,vm->refreshRate);}
        else{if(glfwGetWindowMonitor(window))glfwSetWindowMonitor(window,nullptr,80,80,settings.width,settings.height,0);
             else glfwSetWindowSize(window,settings.width,settings.height);}
    }
    glfwSwapInterval(settings.vsync?1:0);
    audio->volumes(settings.master,settings.music,settings.sfx);
}
void App::applyMusic(){
    if(screen!=Screen::Game){audio->music(Music::Menu);return;}
    if(game.game.inBattle()){bool boss=false;for(const auto& a:game.game.actors)boss|=a.alive()&&a.boss&&a.aggro;audio->music(boss?Music::Boss:Music::Combat);return;}
    if(game.zoneId=="village")audio->music(Music::Village);
    else if(game.zone().surface)audio->music(Music::Explore);
    else audio->music(Music::Dungeon);
}
void App::startNewGame(combat::Class c){
    game.newGame(c);screen=Screen::Game;win=Window::None;stack.clear();floaters.clear();pendingInteract=-1;
    game.requests.clear();syncActors(true);refreshVisuals(true);fade=0;fadeTarget=1;
    yaw=camYaw=.62f;pitch=camPitch=.86f;zoom=camZoom=10.5f;
    lastToastCount=int(game.toasts.size());
}
bool App::loadSlot(const fs::path& path){
    rpg::Campaign loaded;std::string error;
    if(!loaded.load(path,error)){status="Не удалось загрузить: "+error;std::cerr<<status<<'\n';return false;}
    game=std::move(loaded);game.game.setMap(&game.map);
    screen=Screen::Game;win=Window::None;stack.clear();floaters.clear();pendingInteract=-1;
    syncActors(true);refreshVisuals(true);fade=0;fadeTarget=1;
    handleRequests();
    game.toast("Игра загружена.",0);lastToastCount=int(game.toasts.size());
    return true;
}
void App::autosave(){
    if(!game.autosavePending||game.game.inBattle()||game.game.phase==Phase::Defeat)return;
    game.autosavePending=false;std::string error;
    if(game.save(slotPath(0),error))game.toast("Автосохранение",0);
    else{game.toast("Ошибка автосохранения: "+error,1);std::cerr<<"Autosave failed: "<<error<<'\n';}
}
void App::syncActors(bool snap,float dt){
    if(snap){combatVisuals.clear();floaters.clear();}
    auto& g=world().game;
    const size_t oldCount=actorYaw.size();
    actorPos.resize(g.actors.size());actorYaw.resize(g.actors.size(),0);
    actorGait.resize(g.actors.size());
    for(size_t i=0;i<g.actors.size();++i){
        const auto& actor=g.actors[i];
        const glm::vec3 nextPos{actor.pos.x,0,actor.pos.y};
        auto& walk=actorGait[i];
        if(snap||i>=oldCount){
            actorYaw[i]=actor.facing;
            // Loading a zone must not replay the deaths recorded in the campaign.
            walk={float(i)*1.71f,0.f,actor.alive()?-1.f:enemyRemainsSeconds};
        }
        else if(i>0&&!actor.alive()){
            if(walk.deathSeconds<0.f)walk.deathSeconds=0.f;
            else if(!animPaused)walk.deathSeconds=std::min(enemyRemainsSeconds,walk.deathSeconds+
                std::clamp(animTime-walk.impactAt,0.f,dt));
        }
        else {
            walk.deathSeconds=-1.f;
            const float turn=std::remainder(actor.facing-actorYaw[i],2*pi);
            actorYaw[i]+=turn*(1.f-std::exp(-(actor.boss?8.f:15.f)*dt));
            if(!animPaused&&dt>0.f){
                float speed=glm::length(nextPos-actorPos[i])/dt;
                float target=std::clamp(speed/1.5f,0.f,1.f);
                walk.weight+=(target-walk.weight)*(1.f-std::exp(-12.f*dt));
                walk.phase+=dt*(2.1f+std::min(speed,5.f)*(actor.boss?1.55f:2.85f));
            }
        }
        actorPos[i]=nextPos;
    }
    if(g.actors.empty())return;
    heroPos=actorPos[0];heroYaw=actorYaw[0]+pi;
    if(snap){
        pendingBossWindow=Window::None;
        heroFall={};
        lastHeroPos=heroPos;focus=camFocus=heroPos+glm::vec3(0,1,0);
        gait=moveBlend=0;heroAction={};
    }
}
void App::refreshVisuals(bool force){
    auto& c=world();
    std::uint32_t h=entityStateHash(c);
    const bool full=force||visualZone!=c.zoneId||visuals.cleansed!=(c.flags.count("boss_keeper")!=0);
    if(!full&&h==visualHash)return;
    if(!force&&visualZone==c.zoneId&&screen==Screen::Game)audio->play(Sfx::Door,.6f);
    visualHash=h;visualZone=c.zoneId;
    if(full){buildZoneVisuals(c,visuals);renderer->setZone(visuals);}
    else{updateZoneObjects(c,visuals);renderer->setZoneObjects(visuals);}
}

// ------------------------------------------------------------------ camera and picking
void App::updateCamera(float dt){
    float sens=settings.cameraSensitivity;
    double mx,my;glfwGetCursorPos(window,&mx,&my);
    bool middle=!options.benchmark&&glfwGetMouseButton(window,GLFW_MOUSE_BUTTON_MIDDLE)==GLFW_PRESS;
    bool altDrag=held(GLFW_KEY_LEFT_ALT)&&glfwGetMouseButton(window,GLFW_MOUSE_BUTTON_LEFT)==GLFW_PRESS;
    if(freeCamera){
        bool look=glfwGetMouseButton(window,GLFW_MOUSE_BUTTON_RIGHT)==GLFW_PRESS||middle;
        if(look){freeYaw-=float(mx-lastMouseX)*.004f*sens;freePitch=std::clamp(freePitch-float(my-lastMouseY)*.004f*sens,-1.5f,1.5f);}
        glm::vec3 forward(std::cos(freePitch)*std::sin(freeYaw),std::sin(freePitch),std::cos(freePitch)*std::cos(freeYaw));
        glm::vec3 right=glm::normalize(glm::cross(forward,glm::vec3(0,1,0)));
        float speed=(held(GLFW_KEY_LEFT_SHIFT)?24.f:10.f)*dt;
        if(held(GLFW_KEY_W))freePos+=forward*speed;if(held(GLFW_KEY_S))freePos-=forward*speed;
        if(held(GLFW_KEY_D))freePos+=right*speed;if(held(GLFW_KEY_A))freePos-=right*speed;
        if(held(GLFW_KEY_E))freePos.y+=speed;if(held(GLFW_KEY_Q))freePos.y-=speed;
        cameraPos=freePos;view=glm::lookAt(freePos,freePos+forward,glm::vec3(0,1,0));
    }else{
        if((middle||altDrag)&&win==Window::None){yaw-=float(mx-lastMouseX)*.006f*sens;pitch=std::clamp(pitch+float(my-lastMouseY)*.004f*sens,.35f,1.35f);}
        if(held(GLFW_KEY_LEFT))yaw-=dt*1.2f*sens;if(held(GLFW_KEY_RIGHT))yaw+=dt*1.2f*sens;
        if(held(GLFW_KEY_UP))pitch=std::min(1.35f,pitch+dt*.8f*sens);if(held(GLFW_KEY_DOWN))pitch=std::max(.35f,pitch-dt*.8f*sens);
        if(scroll!=0&&!ui.overUi&&(win==Window::None||screen!=Screen::Game)){zoom=std::clamp(zoom-scroll*.9f,5.f,20.f);}
        float ease=1.f-std::exp(-8.f*dt);
        camYaw+=std::atan2(std::sin(yaw-camYaw),std::cos(yaw-camYaw))*ease;
        camPitch=glm::mix(camPitch,pitch,ease);camZoom=glm::mix(camZoom,zoom,ease);camFocus=glm::mix(camFocus,focus,1.f-std::exp(-6.f*dt));
        glm::vec3 dir(std::sin(camYaw)*std::cos(camPitch),std::sin(camPitch),std::cos(camYaw)*std::cos(camPitch));
        float distance=settings.perspective?camZoom/std::tan(glm::radians(30.f)*.5f):60.f;
        cameraPos=camFocus+dir*distance;view=glm::lookAt(cameraPos,camFocus,glm::vec3(0,1,0));
    }
    scroll=0;
    lastMouseX=mx;lastMouseY=my;
    int w,h;glfwGetFramebufferSize(window,&w,&h);float aspect=float(std::max(w,1))/float(std::max(h,1));
    if(settings.perspective||freeCamera)projection=glm::perspective(glm::radians(30.f),aspect,.5f,420.f);
    else projection=glm::ortho(-camZoom*aspect,camZoom*aspect,-camZoom,camZoom,.1f,200.f);
}
glm::vec2 App::project(glm::vec3 p) const{
    glm::vec4 c=projection*view*glm::vec4(p,1);if(c.w<=0.001f)return {-9999,-9999};c/=c.w;
    return {(c.x*.5f+.5f)*ui.width,(.5f-c.y*.5f)*ui.height};
}
void App::pick(){
    hover={-1,-1};hoverValid=false;hoverActor=-1;hoverEntity=-1;
    if(ui.overUi||screen!=Screen::Game)return;
    auto& g=game.game;
    glm::mat4 inv=glm::inverse(projection*view);
    float nx=ui.mouse.x/ui.width*2-1,ny=1-ui.mouse.y/ui.height*2;
    glm::vec4 a=inv*glm::vec4(nx,ny,-1,1),b=inv*glm::vec4(nx,ny,1,1);a/=a.w;b/=b.w;
    if(std::abs(b.y-a.y)>1e-5f){float t=-a.y/(b.y-a.y);if(t>0){hoverPoint=glm::vec3(a)+(glm::vec3(b)-glm::vec3(a))*t;hover=game.map.cellAt(hoverPoint);hoverValid=true;}}
    float best=48;
    for(size_t i=1;i<g.actors.size()&&i<actorPos.size();++i){if(!g.actors[i].alive())continue;
        float h=g.actors[i].boss?2.4f:1.f;auto s=project(actorPos[i]+glm::vec3(0,h,0));float d=glm::length(s-ui.mouse)-(g.actors[i].boss?30.f:0.f);
        if(d<best){best=d;hoverActor=int(i);}}
    if(hoverActor<0&&hoverValid){int e=g.enemyAt({hoverPoint.x,hoverPoint.z},.6f);if(e>0)hoverActor=e;}
    if(hoverActor<0){
        float bestE=38;
        for(size_t i=0;i<game.entities.size();++i){if(!game.interactable(int(i)))continue;
            float h=game.entities[i].def.type==rpg::EntityType::Npc?1.4f:.6f;
            auto s=project(game.map.center(game.entities[i].cell)+glm::vec3(0,h,0));float d=glm::length(s-ui.mouse);
            if(d<bestE){bestE=d;hoverEntity=int(i);}}
        if(hoverEntity<0){int e=game.entityAt(hover);if(e>=0&&game.interactable(e))hoverEntity=e;}
    }
}

// ------------------------------------------------------------------ input
void App::interactAt(int entity){
    auto& g=game.game;Cell ec=game.entities[size_t(entity)].cell;
    glm::vec2 c=g.world2(ec);
    if(glm::length(c-g.actors[0].pos)<2.1f){g.stop();game.interact(entity);return;}
    auto cells=game.interactCells(entity);if(cells.empty())return;
    Cell best=cells[0];float bd=1e9f;for(Cell q:cells){float d=glm::length(g.world2(q)-g.actors[0].pos);if(d<bd){bd=d;best=q;}}
    if(!g.moveTo(g.world2(best))){game.toast("Туда не пройти.",1);return;}
    pendingInteract=entity;clickPoint=game.map.center(best);clickTimer=.6f;clickAttack=false;
}
void App::rightClick(){
    auto& g=game.game;
    if(g.phase==Phase::Defeat)return;
    pendingInteract=-1;
    if(hoverActor>0){g.attackUnit(hoverActor);clickPoint=actorPos[size_t(hoverActor)];clickTimer=.5f;clickAttack=true;return;}
    if(hoverEntity>=0){
        if(g.inBattle()&&game.entities[size_t(hoverEntity)].def.type==rpg::EntityType::Npc){game.toast("Сначала закончите бой",1);return;}
        interactAt(hoverEntity);return;
    }
    if(!hoverValid)return;
    if(g.moveTo({hoverPoint.x,hoverPoint.z})){clickPoint=hoverPoint;clickTimer=.6f;clickAttack=false;}
}
void App::castSlot(int slot){
    auto& g=game.game;const auto& ab=combat::ability(game.heroClass,slot);
    int target=hoverActor;
    if(ab.target==combat::Target::Unit&&target<0){ // forgiving quick-cast: nearest enemy to the cursor, else current target
        if(hoverValid)target=g.enemyAt({hoverPoint.x,hoverPoint.z},2.5f);
        if(target<0)target=g.actors[0].target;
    }
    glm::vec2 point=hoverValid?glm::vec2(hoverPoint.x,hoverPoint.z):g.actors[0].pos;
    auto p=g.castPreview(slot,target,point);
    if(!p.allowed){game.toast(p.reason,1);return;}
    g.cast(slot,target,point);
}
void App::drinkPotion(){
    int best=-1,heal=0;
    for(size_t i=0;i<game.inventory.size();++i){auto d=rpg::item(game.inventory[i].id);if(d&&d->type==rpg::ItemType::Consumable&&d->heal>=35&&(best<0||d->heal<heal)){best=int(i);heal=d->heal;}}
    std::string why;
    if(best<0)game.toast("Нет зелий лечения",1);
    else if(game.useItem(best,why))audio->play(Sfx::Loot,.5f);else game.toast(why,1);
}
void App::gameInput(float dt){
    auto& g=game.game;
    bool right=glfwGetMouseButton(window,GLFW_MOUSE_BUTTON_RIGHT)==GLFW_PRESS;
    bool left=glfwGetMouseButton(window,GLFW_MOUSE_BUTTON_LEFT)==GLFW_PRESS;
    bool rightEdge=right&&!lastRight,leftEdge=left&&!lastLeft&&!held(GLFW_KEY_LEFT_ALT);
    if(win==Window::None&&!freeCamera&&g.phase!=Phase::Defeat){
        if((rightEdge||leftEdge)&&!ui.overUi&&!overActionBar()){rightClick();rightHold=0;}
        // holding the right button keeps steering towards the cursor
        if(right&&!rightEdge&&!ui.overUi&&!overActionBar()&&hoverValid&&hoverActor<0&&hoverEntity<0){rightHold+=dt;
            if(rightHold>.15f){rightHold=0;g.moveTo({hoverPoint.x,hoverPoint.z});clickPoint=hoverPoint;clickTimer=.3f;clickAttack=false;}}
        const int slots[]={GLFW_KEY_Q,GLFW_KEY_W,GLFW_KEY_E,GLFW_KEY_R};
        for(int s=0;s<4;++s)if(key(slots[s]))castSlot(s);
        if(key(GLFW_KEY_H)||key(GLFW_KEY_1))drinkPotion();
        if(key(GLFW_KEY_S)){g.stop();pendingInteract=-1;}
        if(key(GLFW_KEY_F)){ // interact with the nearest object in reach
            int best=-1;float bd=2.2f;
            for(size_t i=0;i<game.entities.size();++i)if(game.interactable(int(i))){float d=glm::length(g.world2(game.entities[i].cell)-g.actors[0].pos);if(d<bd){bd=d;best=int(i);}}
            if(best>=0){g.stop();game.interact(best);}else game.toast("Рядом не с чем взаимодействовать",0);
        }
        if(key(GLFW_KEY_SPACE)){focus=heroPos+glm::vec3(0,1,0);camFocus=focus;}
    }
    lastRight=right;
}
void App::followHero(float dt){
    auto& g=game.game;
    syncActors(false,dt);
    const auto& hero=g.actors[0];
    if(!hero.alive()){
        if(heroFall.advance(dt))setWindow(Window::Defeat);
        return;
    }
    const float distance=glm::length(heroPos-lastHeroPos);lastHeroPos=heroPos;
    const float visualDt=animPaused?0.f:dt;
    // A blink or a dash can cross a whole room in one update.
    const float travel=distance>hero.speed*dt+.12f?0.f:distance;
    const float targetBlend=std::clamp(travel/(std::max(dt,.0001f)*3.25f),0.f,1.f);
    moveBlend=glm::mix(moveBlend,targetBlend,1.f-std::exp(-(targetBlend>moveBlend?14.f:9.f)*visualDt));
    gait=std::fmod(gait+std::min(17.f*visualDt,heroAnimation::gaitAdvance(travel,1.45f,moveBlend)),2*pi);
    heroAction.advance(visualDt);
    // reached the object we were walking to
    if(pendingInteract>=0&&pendingInteract<int(game.entities.size())&&!g.actors[0].moving){
        int e=pendingInteract;pendingInteract=-1;
        if(glm::length(g.world2(game.entities[size_t(e)].cell)-g.actors[0].pos)<2.3f)game.interact(e);
    }
}
void App::consumeEvents(){
    auto& g=game.game;
    int hits=0;
    for(const auto& e:g.events){
        if(!animPaused)combatVisuals.observe(e,g,animTime);
        if((e.type==combat::Event::Hit||e.type==combat::Event::Death)&&
           e.target>0&&size_t(e.target)<actorGait.size()){
            actorGait[size_t(e.target)].impactAt=animTime+(e.directHeroImpact?combat::heroImpactLeadSeconds:0.f);
        }
        auto pos=[&](int id){return id>=0&&id<int(actorPos.size())?actorPos[size_t(id)]:game.map.center(e.cell);};
        switch(e.type){
        case combat::Event::Cast:
            if(e.source==0&&g.actors[0].alive()){
                heroAction.start(g.actors[0].weapon,e.amount,game.heroClass);
                audio->play(Sfx::Magic,.7f);
                if(combat::ability(game.heroClass,e.amount).target==combat::Target::Unit&&g.actors[0].target<=0)
                    g.attackUnit(e.target);
            }
            break;
        case combat::Event::Attack:
            if(e.source==0&&g.actors[0].alive()&&(heroAction.slot<0||heroAction.phase>=.8f))
                heroAction.start(g.actors[0].weapon);
            break;
        case combat::Event::Hit:{
            if(e.target<0){audio->play(Sfx::Magic);break;}
            const bool audible=hits++<3;
            float delay=e.directHeroImpact&&!animPaused?combat::heroImpactLeadSeconds:0.f;
            floaters.push_back({std::to_string(e.amount),pos(e.target)+glm::vec3((float(frame%7)-3.f)*.08f,2.2f,0),
                -delay,e.target==0?ui::red:(e.source==0?ui::gold:ui::text),audible&&delay>0});
            if(audible&&delay==0)audio->play(e.target==0?Sfx::HeroHit:Sfx::Hit,.55f);
            break;}
        case combat::Event::Heal:floaters.push_back({"+"+std::to_string(e.amount),heroPos+glm::vec3(0,2.2f,0),0,ui::mint});break;
        case combat::Event::Warn:audio->play(Sfx::Warn,.7f);break;
        case combat::Event::Vulnerable:floaters.push_back({"УЯЗВИМ!",pos(e.source)+glm::vec3(0,3.4f,0),0,ui::gold});audio->play(Sfx::Victory,.4f);break;
        case combat::Event::Miss:floaters.push_back({"Уклонение",heroPos+glm::vec3(0,2.4f,0),0,ui::mint});break;
        case combat::Event::LevelUp:audio->play(Sfx::LevelUp);floaters.push_back({"Новый уровень!",heroPos+glm::vec3(0,2.8f,0),0,ui::gold});break;
        case combat::Event::BattleEnd:audio->play(Sfx::Victory,.6f);break;
        case combat::Event::Summon:floaters.push_back({e.text,game.map.center(e.cell)+glm::vec3(0,2.f,0),0,ui::red});break;
        case combat::Event::PhaseTwo:floaters.push_back({e.text,pos(e.source)+glm::vec3(0,3.6f,0),0,ui::red});audio->play(Sfx::Warn);break;
        case combat::Event::Trap:floaters.push_back({"Ловушка!",pos(e.target)+glm::vec3(0,2.f,0),0,ui::red});break;
        case combat::Event::Step:if(frame%2==0)audio->play(Sfx::Step,.25f);break;
        default:break;
        }
    }
    if(floaters.size()>40)floaters.erase(floaters.begin(),floaters.begin()+long(floaters.size()-40));
}
void App::handleRequests(){
    auto requests=std::move(game.requests);game.requests.clear();
    for(const auto& [type,arg]:requests){
        switch(type){
        case rpg::Request::ZoneChanged:fade=0;fadeTarget=1;syncActors(true);pendingInteract=-1;refreshVisuals(true);break;
        case rpg::Request::Dialog:dialogLine=0;setWindow(Window::Dialog);break;
        case rpg::Request::Shop:shopNpc=arg;selected=-1;setWindow(Window::Shop);break;
        case rpg::Request::Craft:setWindow(Window::Craft);break;
        case rpg::Request::Smith:setWindow(Window::Smith);break;
        case rpg::Request::Shrine:audio->play(Sfx::Shrine,.7f);setWindow(Window::Shrine);break;
        case rpg::Request::Note:setWindow(Window::Note);break;
        case rpg::Request::RewardChoice:
        case rpg::Request::Epilogue:
            if(game.hero().alive()){
                // Keep the same pause in gameplay while the boss lands in view.
                pendingBossWindow=type==rpg::Request::Epilogue?Window::Epilogue:Window::Reward;
                bossWindowAt=realTime+1.26f;pendingInteract=-1;setWindow(Window::None);
            }
            break;
        case rpg::Request::Credits:setWindow(Window::Ending);break;
        case rpg::Request::Defeat:
            pendingBossWindow=Window::None;
            audio->play(Sfx::Defeat);pendingInteract=-1;clickTimer=0;
            heroFall.start(animTime,game.game,heroYaw);setWindow(Window::None);break;
        }
    }
    if(pendingBossWindow!=Window::None&&realTime>=bossWindowAt){
        setWindow(pendingBossWindow);
        pendingBossWindow=Window::None;
    }
}
void App::setWindow(Window w){win=w;stack.clear();selected=-1;slotConfirm=-1;}
void App::closeWindow(){if(!stack.empty()){win=stack.back();stack.pop_back();}else win=Window::None;}

// ------------------------------------------------------------------ dynamic geometry
static void ring(Mesh& m,glm::vec3 c,float r,float thick,glm::vec3 color,float lift=.06f){
    if(r<.01f)return;
    thick=std::min(thick,r);
    int seg=std::max(16,int(r*14));
    for(int i=0;i<seg;++i){float a=float(i)/float(seg)*6.2832f,b=float(i+1)/float(seg)*6.2832f;
        glm::vec3 da(std::cos(a),0,std::sin(a)),db(std::cos(b),0,std::sin(b));
        glm::vec3 y(0,lift,0);
        m.quad(c+y+da*(r-thick),c+y+db*(r-thick),c+y+db*r,c+y+da*r,color,mat::Glow);}
}
static void warningCircle(Mesh& m,glm::vec3 center,float radius,glm::vec3 color){
    ring(m,center,radius,.065f,color);
    center.y+=.061f;
    for(int i=0;i<8;++i){
        float angle=float(i)*pi*.25f;
        glm::vec3 dir(std::cos(angle),0,std::sin(angle)),side(-dir.z,0,dir.x);
        glm::vec3 edge=center+dir*(radius-.065f);
        m.triangle(edge-side*.075f,edge+side*.075f,edge-dir*.23f,color,mat::Glow);
    }
}
static void rect(Mesh& m,glm::vec2 c,glm::vec2 dir,float length,float width,glm::vec3 color,float lift){
    glm::vec2 d=glm::normalize(dir),s(-d.y,d.x);
    auto P=[&](float a,float b){glm::vec2 p=c+d*a+s*b;return glm::vec3(p.x,lift,p.y);};
    m.quad(P(-length*.5f,-width*.5f),P(-length*.5f,width*.5f),P(length*.5f,width*.5f),P(length*.5f,-width*.5f),color,mat::Glow);
}
void App::buildDynamic(float time){
    dynamicMesh.vertices.clear();
    auto& c=world();auto& g=c.game;const auto& map=c.map;
    for(size_t i=1;i<g.actors.size()&&i<actorPos.size();++i){
        const auto& a=g.actors[i];const auto& pose=actorGait[i];
        if(!a.alive()&&(pose.deathSeconds<0.f||pose.deathSeconds>=enemyRemainsSeconds))continue;
        buildEnemy(dynamicMesh,a,actorPos[i],actorYaw[i],pose.phase,pose.weight,pose.deathSeconds,animTime-pose.impactAt);
    }
    for(const auto& e:c.entities){
        if(e.def.type!=rpg::EntityType::Npc||e.hidden)continue;
        glm::vec3 p=map.center(e.cell);glm::vec3 toHero=heroPos-p;float yawNpc=glm::length(toHero)<6.f?std::atan2(toHero.x,toHero.z):0.f;
        buildNpc(dynamicMesh,e.def.id,p,yawNpc,time);
    }
    for(const auto& b:c.loot)buildLootBag(dynamicMesh,map.center(b.cell),time);
    if(screen!=Screen::Game)return;
    combatVisuals.draw(dynamicMesh,combatParticles,g,time);
    // The outer rim marks the actual damage boundary; the moving stroke shows time to impact.
    for(const auto& hz:g.hazards){
        glm::vec3 center(hz.center.x,0,hz.center.y);
        warningCircle(dynamicMesh,center,hz.radius,{.20f,.35f,.08f});
        ring(dynamicMesh,center,hz.radius*.88f,.035f,{.08f,.20f,.06f});
    }
    for(const auto& t:g.telegraphs){
        float k=1.f-std::clamp(t.time/t.total,0.f,1.f);
        glm::vec3 base=t.owner==0?glm::vec3(.64f,.31f,.09f):t.hazard?glm::vec3(.20f,.43f,.08f):glm::vec3(.61f,.095f,.045f);
        if(t.shape==0){
            glm::vec3 center(t.center.x,0,t.center.y);
            warningCircle(dynamicMesh,center,t.radius,base);
            ring(dynamicMesh,center,t.radius*k,.045f,base*.65f,.055f);
        }else{
            glm::vec2 d=glm::normalize(t.dir),side(-d.y,d.x);
            for(float sign:{-1.f,1.f}){
                rect(dynamicMesh,t.center+side*sign*(t.width-.07f)*.5f,d,t.length,.07f,base,.06f);
                rect(dynamicMesh,t.center+d*sign*(t.length-.07f)*.5f,d,.07f,t.width,base,.06f);
            }
            rect(dynamicMesh,t.center+d*(k-.5f)*(t.length-.10f),d,.10f,t.width,base*.65f,.055f);
        }
    }
    // selection rings
    const auto& h=g.actors[0];
    if(h.alive())ring(dynamicMesh,heroPos,.62f,.05f,{.15f,.8f,.55f});
    if(h.target>0&&h.target<int(g.actors.size())&&g.actors[size_t(h.target)].alive())ring(dynamicMesh,actorPos[size_t(h.target)],g.actors[size_t(h.target)].radius+.35f,.07f,{1.f,.25f,.1f});
    if(hoverActor>0&&hoverActor<int(g.actors.size())&&g.actors[size_t(hoverActor)].alive())
        ring(dynamicMesh,actorPos[size_t(hoverActor)],g.actors[size_t(hoverActor)].radius+.3f,.04f,{.9f,.5f,.2f});
    if(hoverEntity>=0)ring(dynamicMesh,map.center(c.entities[size_t(hoverEntity)].cell),.75f,.05f,{.9f,.75f,.25f});
    // ability range when the cursor rests on an ability button
    if(hoverAbility>=0){const auto& ab=combat::ability(game.heroClass,hoverAbility);float r=ab.target==combat::Target::Self?ab.radius:ab.range;
        if(r>0)ring(dynamicMesh,heroPos,r,.05f,{.3f,.6f,1.f});}
    if(clickTimer>0){float k=clickTimer/.6f;glm::vec3 col=clickAttack?glm::vec3(1.f,.2f,.1f):glm::vec3(.2f,1.f,.4f);
        for(int s=0;s<4;++s){float a=float(s)*1.5708f+time*2;glm::vec3 o(std::cos(a),0,std::sin(a));
            dynamicMesh.beam(clickPoint+o*(.25f+.3f*k)+glm::vec3(0,.08f,0),clickPoint+o*(.05f+.3f*k)+glm::vec3(0,.08f,0),.04f,col*k,mat::Glow,4);}}
}

// ------------------------------------------------------------------ frame
void App::capture(const fs::path& path,int w,int h){
    std::vector<unsigned char> pixels(size_t(w)*size_t(h)*3);
    glPixelStorei(GL_PACK_ALIGNMENT,1);glReadPixels(0,0,w,h,GL_RGB,GL_UNSIGNED_BYTE,pixels.data());
    std::error_code ec;if(path.has_parent_path())fs::create_directories(path.parent_path(),ec);
    std::ofstream out(path,std::ios::binary);out<<"P6\n"<<w<<' '<<h<<"\n255\n";
    for(int y=h-1;y>=0;--y)out.write(reinterpret_cast<const char*>(pixels.data()+size_t(y)*size_t(w)*3),std::streamsize(w)*3);
    std::cout<<"Capture: "<<path.string()<<'\n';
}
void App::frameStep(float dt){
    int w,h;glfwGetFramebufferSize(window,&w,&h);
    double mx,my;glfwGetCursorPos(window,&mx,&my);int ww,wh;glfwGetWindowSize(window,&ww,&wh);
    glm::vec2 cursor(float(mx)*float(w)/float(std::max(1,ww)),float(my)*float(h)/float(std::max(1,wh)));
    bool left=!options.benchmark&&glfwGetMouseButton(window,GLFW_MOUSE_BUTTON_LEFT)==GLFW_PRESS;
    bool right=!options.benchmark&&glfwGetMouseButton(window,GLFW_MOUSE_BUTTON_RIGHT)==GLFW_PRESS;
    if(options.benchmark)cursor={-1000,-1000};
    ui.begin(w,h,cursor,left,left&&!lastLeft,!left&&lastLeft,right&&!lastRight);
    realTime+=dt;
    RenderSettings rs;
    // global keys
    if(key(GLFW_KEY_T)){settings.technique=!settings.technique;audio->play(Sfx::Click);}
    if(key(GLFW_KEY_Y)){settings.tiltShift=!settings.tiltShift;audio->play(Sfx::Click);}
    if(key(GLFW_KEY_P))animPaused=!animPaused;
    if(!animPaused)animTime+=dt;
    if(key(GLFW_KEY_O)){settings.perspective=!settings.perspective;}
    if(held(GLFW_KEY_LEFT_BRACKET))lightAngle-=dt*.9f;if(held(GLFW_KEY_RIGHT_BRACKET))lightAngle+=dt*.9f;
    if(held(GLFW_KEY_SEMICOLON))lightElevation=std::max(.25f,lightElevation-dt*.6f);if(held(GLFW_KEY_APOSTROPHE))lightElevation=std::min(1.45f,lightElevation+dt*.6f);
    if(key(GLFW_KEY_TAB))hudVisible=!hudVisible;
    if(key(GLFW_KEY_F4)){freeCamera=!freeCamera;if(freeCamera){freePos=cameraPos;glm::vec3 f=glm::normalize(camFocus-cameraPos);freeYaw=std::atan2(f.x,f.z);freePitch=std::asin(f.y);}}
    bool shot=key(GLFW_KEY_F12);
    if(screen==Screen::Game){
        const bool transition=heroFall.pending()||pendingBossWindow!=Window::None;
        if(key(GLFW_KEY_ESCAPE)&&!transition){
            if(freeCamera)freeCamera=false;
            else if(win==Window::None)setWindow(Window::Pause);
            else if(win!=Window::Defeat&&win!=Window::Epilogue&&win!=Window::Reward)closeWindow();
        }
        if(key(GLFW_KEY_F1)&&!transition){if(win==Window::Help)closeWindow();else{stack.push_back(win);win=Window::Help;}}
        if(!transition&&(win==Window::None||win==Window::Inventory||win==Window::Character||win==Window::Journal||win==Window::Map)){
            auto toggle=[&](int k,Window target){if(key(k))win=win==target?Window::None:target;};
            toggle(GLFW_KEY_I,Window::Inventory);toggle(GLFW_KEY_C,Window::Character);toggle(GLFW_KEY_J,Window::Journal);toggle(GLFW_KEY_M,Window::Map);
        }
    }else{
        if(key(GLFW_KEY_ESCAPE)){if(win!=Window::None)closeWindow();else if(screen==Screen::ClassSelect)screen=Screen::Menu;}
        if(key(GLFW_KEY_F1)){win=win==Window::Help?Window::None:Window::Help;}
    }
    // world update: the simulation runs while HUD windows are open, and pauses for menus and dialogues
    auto& c=world();
    if(screen==Screen::Game){
        focus=heroPos+glm::vec3(0,1.f,0);
        pick();
        bool live=win==Window::None||win==Window::Inventory||win==Window::Character||win==Window::Journal||win==Window::Map;
        if(win==Window::None&&pendingBossWindow==Window::None&&!options.benchmark)gameInput(dt);else lastRight=right;
        if(live&&pendingBossWindow==Window::None)game.game.update(dt);
        followHero(dt);
        consumeEvents();
        game.update(dt);
        handleRequests();
        autosave();
        int toastCount=int(game.toasts.size());
        if(toastCount>lastToastCount&&!game.toasts.empty()){int r=game.toasts.back().rarity;if(game.toasts.back().text.rfind("+",0)==0)audio->play(Sfx::Loot,r>=2?1.f:.6f);}
        lastToastCount=toastCount;
        clickTimer=std::max(0.f,clickTimer-dt);
    }else{
        lastRight=right;
        yaw+=dt*.04f;focus=c.map.center({17,11})+glm::vec3(0,1.5f,0);zoom=std::min(zoom,13.f);
        c.game.update(dt);
        syncActors(false,dt);
    }
    for(auto& f:floaters){
        f.time+=dt;
        if(f.impactSound&&f.time>=0){audio->play(Sfx::Hit,.55f);f.impactSound=false;}
    }
    floaters.erase(std::remove_if(floaters.begin(),floaters.end(),[](const Floater& f){return f.time>1.2f;}),floaters.end());
    fade=glm::mix(fade,fadeTarget,1.f-std::exp(-5.f*dt));
    updateCamera(dt);
    refreshVisuals(false);
    applyMusic();
    // geometry
    combatParticles.clear();
    buildDynamic(animTime);dynamicGpu->update(dynamicMesh);
    heroMesh.vertices.clear();
    glm::mat4 heroModel(1);
    if(screen==Screen::Game){
        float poseTime=heroFall.active()?heroFall.startTime+std::min(heroFall.age,heroAnimation::Fall::settled):animTime;
        float strideWeight=heroFall.active()?moveBlend*heroFall.retainStride():moveBlend;
        const auto pose=heroFall.active()?heroFall.pose(heroAction.current,game.hero().weapon):heroAction.current;
        buildAnimatedHero(heroMesh,poseTime,gait,strideWeight,pose,game.heroClass,game.hero().weapon);
        heroModel=glm::scale(glm::rotate(glm::translate(glm::mat4(1),heroPos),heroYaw,glm::vec3(0,1,0)),glm::vec3(1.45f));
        if(heroFall.active())heroModel*=heroFall.groundTransform(heroMesh);
    }
    heroGpu->update(heroMesh);
    FrameInput f;f.view=view;f.projection=projection;f.cameraPos=cameraPos;f.focus=camFocus;f.heroPos=heroPos;f.time=animTime;
    f.near=.5f;f.far=420.f;f.fovY=glm::radians(30.f);f.orthoHalf=camZoom;f.width=w;f.height=h;f.fade=fade;
    f.keyDirection=glm::vec3(std::cos(lightElevation)*std::cos(lightAngle),std::sin(lightElevation),std::cos(lightElevation)*std::sin(lightAngle));
    const auto& zone=c.zone();f.surface=zone.surface;f.biome=zone.biome;
    float corruption=c.flags.count("boss_keeper")?0.f:float(3-c.seals())/3.f*.45f+.1f;
    f.corruption=zone.surface?corruption:(zone.biome==14&&!c.flags.count("boss_keeper")?.5f:0.f);
    f.cutaway=screen==Screen::Game&&!freeCamera?(zone.surface?2.6f:3.4f):0.f;
    if(screen!=Screen::Game)f.heroPos=focus;
    rs.pauseAnimation=animPaused;rs.technique=settings.technique;rs.perspective=settings.perspective||freeCamera;rs.shadowQuality=settings.shadowQuality;rs.effects=settings.effects;
    rs.tiltShift=settings.tiltShift;
    renderer->render(f,rs,*dynamicGpu,*heroGpu,heroModel,combatParticles);
    // interface
    hoverAbility=-1;
    if(screen==Screen::Menu)drawMenu();
    else if(screen==Screen::ClassSelect)drawClassSelect();
    else{
        if(hudVisible&&win==Window::None&&!heroFall.active()&&pendingBossWindow==Window::None)drawHud();
        drawWindows();
        if(win==Window::Inventory||win==Window::Character||win==Window::Journal||win==Window::Map)
            drawWindowShortcuts(ui.width*.5f-302,ui.height-52,false);
    }
    if(screen!=Screen::Game&&win!=Window::None)drawWindows();
    drawTechniqueBadge();
    ui.drawTooltip();
    if(ui.clicks>0)audio->play(Sfx::Click,.6f);
    ui.draw();
    GLenum error=glGetError();
    if(error!=GL_NO_ERROR){std::cerr<<"OpenGL error 0x"<<std::hex<<error<<std::dec<<" in frame "<<frame<<'\n';if(options.smoke)throw std::runtime_error("OpenGL error during smoke test");}
    if(shot)capture(root/"captures"/("shot-"+std::to_string(std::chrono::system_clock::now().time_since_epoch().count())+".ppm"),w,h);
    lastLeft=left;
    ++frame;
}
int App::runBenchmark(){
    // Reset the complete scenario for each mode/repeat. Fixed simulation steps make
    // combat identical even when one GPU renders faster. glFinish is benchmark-only:
    // samples measure a completed frame, not merely the CPU's command submission.
    const char* zones[]={"village","road","lair","throne","castle"};
    constexpr int repeats=3,warmup=30,frames=90;
    settings=Settings{};settings.vsync=false;settings.fullscreen=false;
    glfwSwapInterval(0);glfwSetWindowSize(window,1280,720);
    audio->volumes(0,0,0);
    renderer->profileGpu=true;
    std::ofstream csv(root/"benchmark.csv");
    if(!csv)throw std::runtime_error("Cannot write benchmark.csv");
    csv<<"gpu,zone,shadows,repeat,width,height,frames,fps,mean_ms,median_ms,p95_ms,gpu_point_ms,gpu_sun_ms,gpu_scene_ms,gpu_post_ms\n";
    auto state=[&]{
        std::ostringstream out;out<<std::setprecision(9)<<game.zoneId<<' '<<game.game.rng<<' '<<game.game.time;
        for(const auto& a:game.game.actors)out<<' '<<int(a.kind)<<':'<<a.hp<<':'<<a.pos.x<<':'<<a.pos.y<<':'<<a.mana;
        return out.str();
    };
    for(const char* zone:zones){
        std::string reference;
        for(int mode=1;mode>=0;--mode)for(int repeat=0;repeat<repeats;++repeat){
            game=rpg::Campaign{};animTime=realTime=0;freeCamera=false;animPaused=false;
            heroAction={};heroFall={};actorGait.clear();actorYaw.clear();floaters.clear();
            clickTimer=0;hoverActor=hoverEntity=hoverAbility=-1;scroll=0;keys.fill(false);
            settings.technique=mode!=0;
            startNewGame(combat::Class::Warrior);game.enterZone(zone,'@');
            if(std::string(zone)=="road"||std::string(zone)=="lair"||std::string(zone)=="throne"){
                for(size_t i=1;i<game.game.actors.size();++i){
                    const auto& enemy=game.game.actors[i];
                    if(!enemy.alive()||enemy.kind==combat::Kind::Pylon)continue;
                    Cell at=enemy.cell;
                    for(Cell d:{Cell{0,2},Cell{2,0},Cell{-2,0},Cell{0,-2}}){Cell q{at.x+d.x,at.y+d.y};
                        if(!game.map.blocked(q.x,q.y)&&game.game.occupant(q)<0){game.game.place(0,q);break;}}
                    game.game.engage(int(i));game.game.attackUnit(int(i));break;
                }
            }
            game.requests.clear();game.autosavePending=false;syncActors(true);refreshVisuals(true);
            win=Window::None;fade=fadeTarget=1;lightAngle=.95f;lightElevation=.95f;updateCamera(0);
            for(int i=0;i<warmup;++i){glfwPollEvents();frameStep(1.f/60);glfwSwapBuffers(window);}
            glFinish();renderer->collectGpuTimings();
            const auto gpuBefore=renderer->gpuTotals;
            std::vector<double> samples;samples.reserve(frames);
            for(int i=0;i<frames;++i){
                glfwPollEvents();double begin=glfwGetTime();
                frameStep(1.f/60);glfwSwapBuffers(window);glFinish();
                samples.push_back((glfwGetTime()-begin)*1000.);
            }
            renderer->collectGpuTimings();
            const auto gpuAfter=renderer->gpuTotals;
            const double gpuSamples=double(gpuAfter.samples-gpuBefore.samples);
            if(gpuSamples<=0)throw std::runtime_error("GPU timing queries returned no samples");
            const auto endState=state();
            if(reference.empty())reference=endState;
            else if(reference!=endState)throw std::runtime_error(std::string("Benchmark scenario diverged in ")+zone);
            double mean=std::accumulate(samples.begin(),samples.end(),0.)/samples.size();
            std::sort(samples.begin(),samples.end());
            double median=(samples[frames/2-1]+samples[frames/2])*.5;
            double p95=samples[size_t(std::ceil(frames*.95))-1];
            int width,height;glfwGetFramebufferSize(window,&width,&height);
            csv<<std::quoted(gpuName)<<','<<zone<<','<<(mode?"on":"off")<<','<<repeat+1<<','<<width<<','<<height<<','<<frames<<','
               <<1000./mean<<','<<mean<<','<<median<<','<<p95<<','
               <<(gpuAfter.pointShadowsMs-gpuBefore.pointShadowsMs)/gpuSamples<<','
               <<(gpuAfter.sunShadowMs-gpuBefore.sunShadowMs)/gpuSamples<<','
               <<(gpuAfter.sceneMs-gpuBefore.sceneMs)/gpuSamples<<','
               <<(gpuAfter.postMs-gpuBefore.postMs)/gpuSamples<<'\n';csv.flush();
            std::cout<<"Benchmark "<<zone<<" shadows "<<(mode?"on":"off")<<" repeat "<<repeat+1
                     <<": "<<int(1000./mean)<<" FPS, median "<<median<<" ms, p95 "<<p95<<" ms\n"<<std::flush;
        }
    }
    std::cout<<"Benchmark complete: identical simulation states; 1280x720, high quality, synchronized completed frames.\n";
    return 0;
}

int App::run(){
    if(options.benchmark)return runBenchmark();
    if(options.smoke){
        struct Case{const char* zone;bool battle;};
        std::vector<Case> cases={{"village",false},{"road",true},{"grove",false},{"lair",true},{"mine",false},{"flooded",false},{"pass",false},{"crypt",true},{"throne",true},{"castle",false},{"heart",true}};
        startNewGame(combat::Class::Warrior);
        if(options.smoke){
            // Scripted session through the real input code: talk, activate the shrine, leave the village, fight.
            auto frames=[&](int n,const std::function<bool()>& until){for(int i=0;i<n;++i){glfwPollEvents();frameStep(1.f/30);glfwSwapBuffers(window);if(until&&until())return true;}return false;};
            auto require=[](bool ok,const char* what){if(!ok)throw std::runtime_error(std::string("Smoke scenario failed: ")+what);std::cout<<"  ok: "<<what<<'\n';};
            auto clickEntity=[&](const char* id){for(size_t i=0;i<game.entities.size();++i)if(std::string(game.entities[i].def.id)==id){hoverEntity=int(i);hoverActor=-1;rightClick();return;}};
            auto clickAt=[&](Cell c){hoverEntity=-1;hoverActor=-1;hoverPoint=game.map.center(c);hoverValid=true;rightClick();};
            require(game.zoneId=="road"&&game.zone().surface,"new hero starts on the Old Road");
            for(const auto& e:game.entities)if(std::string(e.def.id)=="to_village"){clickAt(e.cell);break;}
            require(frames(180,[&]{return game.zoneId=="village";}),"entered the village from the forest");
            clickEntity("elder");
            require(frames(900,[&]{return win==Window::Dialog;}),"walked to the elder and opened the dialog");
            win=Window::None;
            clickEntity("shrine_village");
            require(frames(400,[&]{return win==Window::Shrine;})&&game.shrineActive("shrine_village"),"shrine activated by click");
            win=Window::None;
            for(const auto& e:game.entities)if(std::string(e.def.id)=="to_road"){clickAt(e.cell);break;}
            require(frames(900,[&]{return game.zoneId=="road";}),"walked through the east exit to the old road");
            require(rpg::Campaign::inspect(slotPath(0)).valid,"autosave written on zone change");
            int wolf=-1;for(size_t i=1;i<game.game.actors.size();++i)if(game.game.actors[i].kind==combat::Kind::Wolf){wolf=int(i);break;}
            if(wolf>0)clickAt(game.game.actors[size_t(wolf)].cell);
            require(frames(900,[&]{return game.game.inBattle();}),"enemy noticed the hero and combat started");
            frames(3600,[&]{
                    auto& g=game.game;
                    if(g.actors[0].target<=0||!g.actors[size_t(g.actors[0].target)].alive()){
                        for(size_t i=1;i<g.actors.size();++i)if(g.actors[i].alive()&&g.actors[i].aggro){hoverActor=int(i);hoverEntity=-1;rightClick();break;}}
                    if(g.actors[0].target>0&&g.castPreview(0,g.actors[0].target,{}).allowed){hoverActor=g.actors[0].target;castSlot(0);}
                    if(g.actors[0].hp<g.actors[0].maxHp/3)drinkPotion();
                    return !g.inBattle();});
            require(!game.game.inBattle(),"battle resolved in real time");
            if(game.hero().alive())game.game.damage(0,game.hero().hp);
            require(frames(3,[&]{return heroFall.pending()&&win==Window::None;}),"fall began before defeat screen");
            require(frames(65,[&]{return win==Window::Defeat;}),"defeat screen followed the settled pose");
            game.respawnAtShrine();win=Window::None;handleRequests();
            require(game.hero().alive()&&!heroFall.active(),"revived at the shrine with the standing pose");
            std::cout<<"Scripted session finished in zone "<<game.zoneId<<", hero HP "<<game.hero().hp<<'\n';

            // Exercise presentation of recovered rewards and the final dialog,
            // which the headless campaign tests cannot observe.
            startNewGame(combat::Class::Mage);game.game.grantXp(100);game.game.events.clear();
            game.flags.insert("boss_matriarch");
            std::string error;const auto recovery=saveDir/"reward-recovery.sav";
            require(game.save(recovery,error),"saved pending boss reward");
            require(loadSlot(recovery)&&game.hero().maxMana==168.f,"load preserves level mana");
            require(frames(60,[&]{return win==Window::Reward;}),"load restores reward window");
            game.chooseReward(2);win=Window::None;autosave();
            rpg::Campaign restored;
            require(restored.load(slotPath(0),error)&&restored.flags.count("reward_spider")&&restored.rewardChoices.empty(),"claimed reward autosaved once");
            game.quests["main"]=10;game.enterZone("village",'@');handleRequests();
            clickEntity("elder");
            require(frames(900,[&]{return win==Window::Dialog;}),"final conversation opens before ending");
            require(game.stage("main")==10,"final quest waits for dialog completion");
            game.dialogAction("finish_campaign");handleRequests();
            require(win==Window::Ending&&game.stage("main")==11,"final dialog opens ending screen");
        }
        for(const auto& cs:cases){
            game.enterZone(cs.zone,'@');if(game.hero().cell.x<0)game.enterZone(cs.zone,'[');
            game.requests.clear();syncActors(true);refreshVisuals(true);fade=1;win=Window::None;
            if(cs.battle){int t=-1;for(size_t i=1;i<game.game.actors.size();++i)if(game.game.actors[i].alive()&&game.game.actors[i].kind!=combat::Kind::Pylon){t=int(i);break;}
                if(t>0){Cell tc=game.game.actors[size_t(t)].cell;for(Cell d:{Cell{0,2},Cell{2,0},Cell{-2,0},Cell{0,-2}}){Cell q{tc.x+d.x,tc.y+d.y};if(!game.map.blocked(q.x,q.y)&&game.game.occupant(q)<0){game.game.place(0,q);break;}}
                    syncActors(true);game.game.engage(t);}}
            {
                settings.technique=true;
                int frames=12;
                for(int i=0;i<10;++i){glfwPollEvents();frameStep(1.f/60);glfwSwapBuffers(window);}
                glFinish();double t0=glfwGetTime();
                for(int i=0;i<frames;++i){glfwPollEvents();frameStep(1.f/60);glfwSwapBuffers(window);}
                glFinish();double t1=glfwGetTime();double fpsMeasured=frames/(t1-t0);
                std::cout<<"Zone "<<cs.zone<<(cs.battle?" (battle)":"")<<" shadows on: "<<int(fpsMeasured)<<" FPS\n";
            }
            if(options.smoke){int w,h;glfwGetFramebufferSize(window,&w,&h);capture(root/"captures"/(std::string("smoke-")+cs.zone+".ppm"),w,h);}
        }
        if(options.smoke){
            for(auto size:{std::pair{1024,640},std::pair{1280,720},std::pair{800,900}}){glfwSetWindowSize(window,size.first,size.second);for(int i=0;i<3;++i){glfwPollEvents();frameStep(1.f/60);glfwSwapBuffers(window);}}
            for(int quality=0;quality<3;++quality){
                settings.shadowQuality=quality;settings.effects=quality;
                for(int i=0;i<3;++i){glfwPollEvents();frameStep(1.f/60);glfwSwapBuffers(window);}
            }
            std::cout<<"Smoke test: all zones rendered, resize and quality checks completed.\n";
        }
        return 0;
    }
    if(!options.capture.empty()){
        if(options.menu&&options.window.empty()){screen=Screen::Menu;}
        animTime=options.time;
        if(options.view=="close"){zoom=camZoom=6.5f;}
        if(options.view=="wide"){zoom=camZoom=15.f;}
        if(options.window=="defeat"){
            // Sample the real lethal-hit sequence at the requested time for motion review.
            fade=1;frameStep(0);
            float remaining=std::clamp(options.time,0.f,heroAnimation::Fall::reveal+.1f);
            while(remaining>0.f){
                float dt=std::min(1.f/30,remaining);
                glfwPollEvents();frameStep(dt);glfwSwapBuffers(window);remaining-=dt;
            }
        }else for(int i=0;i<40;++i){glfwPollEvents();frameStep(i<38?1.f/30:0.f);glfwSwapBuffers(window);}
        int w,h;glfwGetFramebufferSize(window,&w,&h);frameStep(0);capture(options.capture,w,h);glfwSwapBuffers(window);
        return 0;
    }
    while(!glfwWindowShouldClose(window)){
        glfwPollEvents();
        double now=glfwGetTime();float elapsed=float(now-lastTime);lastTime=now;float dt=std::min(elapsed,.05f);
        int w,h;glfwGetFramebufferSize(window,&w,&h);if(w<=0||h<=0){glfwWaitEvents();continue;}
        frameStep(dt);
        glfwSwapBuffers(window);
        fpsTimer+=elapsed;++fpsFrames;if(fpsTimer>=.5f){fps=float(fpsFrames)/fpsTimer;fpsTimer=0;fpsFrames=0;
            std::string title="THE OSSUARY: Эшвальд | OpenGL 3.3 | "+std::to_string(int(fps))+" FPS";glfwSetWindowTitle(window,title.c_str());}
    }
    return 0;
}
