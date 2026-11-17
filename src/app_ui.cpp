// All interface windows. Every visible button performs an action or explains why it cannot.
#include "app.hpp"
#include <algorithm>
#include <cmath>
#include <string_view>

using world::Cell;
using combat::Phase;
using rpg::ItemType;

namespace {
std::string plus(int v){return (v>=0?"+":"")+std::to_string(v);}
constexpr glm::vec4 bronze{.50f,.38f,.23f,1};
constexpr glm::vec4 ink{.055f,.065f,.073f,.95f};
constexpr int potionIcon=12;
constexpr float actionBarWidth=860,actionBarBottom=150,actionBarHeight=140;
int portraitIcon(combat::Class c){return 13+int(c);}
int abilityIcon(combat::Class c,int slot){return 4*int(c)+slot;}
void framedIcon(Ui& ui,float x,float y,float size,int icon,glm::vec4 frame,float brightness=1){
    ui.quad(x,y,size,size,frame);
    ui.icon(x+2,y+2,size-4,icon,brightness);
}
void keyLabel(Ui& ui,float x,float y,const char* key){
    ui.quad(x,y,17,19,ink);
    ui.textCentered(x+8.5f,y,key,13,ui::gold);
}
const char* slotName(int s){static const char* n[]={"Оружие","Броня","Талисман"};return n[s];}
std::string classLine(combat::Class c){
    auto h=combat::Game::makeHero(c);
    return std::to_string(h.maxHp)+" HP   броня "+std::to_string(h.armor)+"   мана "+std::to_string(int(h.maxMana));
}
}

void App::drawTechniqueBadge(){
    const bool compact=screen!=Screen::Game||win!=Window::None;
    float w=compact?218.f:308.f,x=ui.width-w-14,y=12;
    ui.gradient(x,y,w,compact?29.f:86.f,{.105f,.12f,.122f,.95f},{.048f,.056f,.06f,.91f});
    ui.frame(x,y,w,compact?29.f:86.f,ui::bevel);
    ui.quad(x,y,2,compact?29.f:86.f,settings.technique?ui::mint:ui::red);
    ui.text(x+12,y+5,std::string("T  ·  Мягкие тени PCSS  ")+(settings.technique?"Вкл":"Выкл"),15,settings.technique?ui::mint:ui::red);
    if(compact)return;
    std::string line=(fps>0?std::to_string(int(fps))+" FPS   ·   ":"")+(settings.perspective||freeCamera?std::string("Перспектива"):std::string("Ортография"))+(freeCamera?"  ·  F4":"");
    ui.text(x+12,y+26,line,14,ui::text);
    int deg=int(std::fmod(lightAngle*57.2958f+3600.f,360.f));
    ui.text(x+12,y+44,"[ ]  Свет "+std::to_string(deg)+"°     P  Анимация "+(animPaused?"на паузе":"включена"),13,ui::muted);
    ui.text(x+12,y+63,team+" · "+topic,12,ui::muted);
}

void App::drawMenu(){
    if(win!=Window::None)return;
    using E=ui::Emblem;
    const float x=ui.width*.5f-484,y=ui.height*.5f-300;
    ui.quad(0,0,ui.width,ui.height,{.022f,.03f,.033f,.59f});
    ui.panel(x,y,968,600);
    ui.quad(x+2,y+2,476,596,{1,1,1,1},{0,0},{1,1},3);
    ui.gradient(x+2,y+388,476,210,{.025f,.03f,.033f,0},{.025f,.03f,.033f,.96f});
    ui.quad(x+478,y+2,1,596,ui::brass);
    ui.heading(x+32,y+472,"THE OSSUARY",49,ui::text);
    ui.text(x+34,y+541,"Эшвальд: Хранитель Костей",22,ui::gold);
    int newest=-1;long long stamp=0;
    for(int slot=0;slot<4;++slot){
        auto info=rpg::Campaign::inspect(slotPath(slot));
        if(info.valid&&info.stamp>stamp){stamp=info.stamp;newest=slot;}
    }
    const float bx=x+518,bw=408,by=y+107,bh=54,gap=64;
    ui.heading(bx,y+37,"Главное меню",32);
    ui.quad(bx,y+88,bw,1,ui::brass);
    if(ui.button(bx,by,bw,bh,"Продолжить",newest>=0,"Сначала начните новую игру.",newest>=0,20,E::Hourglass))loadSlot(slotPath(newest));
    if(ui.button(bx,by+gap,bw,bh,"Новая игра",true,"",newest<0,20,E::Sword))screen=Screen::ClassSelect;
    if(ui.button(bx,by+gap*2,bw,bh,"Загрузить игру",true,"",false,19,E::Chest))setWindow(Window::Load);
    if(ui.button(bx,by+gap*3,bw,bh,"Настройки",true,"",false,19,E::Gear))setWindow(Window::Settings);
    if(ui.button(bx,by+gap*4,bw,bh,"Авторы и управление",true,"",false,19,E::Journal))setWindow(Window::Credits);
    ui.quad(bx,y+448,bw,1,ui::bevel);
    if(ui.button(bx,y+466,bw,46,"Выход из игры"))glfwSetWindowShouldClose(window,GLFW_TRUE);
    if(!status.empty())ui.paragraph(bx,y+523,bw,status,15,ui::red);
    ui.textCentered(ui.width*.5f,y+623,"Колесо — приближение   ·   Стрелки — вращение камеры   ·   F1 — справка",15,ui::text);
}
void App::drawClassSelect(){
    float cx=ui.width*.5f,cy=ui.height*.5f;
    ui.quad(0,0,ui.width,ui.height,{.01f,.02f,.03f,.62f});
    ui.headingCentered(cx,cy-300,"Выберите героя",38);
    ui.textCentered(cx,cy-250,"Каждый класс способен пройти игру в одиночку",17,ui::muted);
    if(win==Window::ConfirmNew)return;
    const combat::Class classes[]={combat::Class::Warrior,combat::Class::Rogue,combat::Class::Mage};
    const char* weapon[]={"Меч и щит","Парные кинжалы","Посох"};
    const char* text[]={"Ближний бой, много здоровья и брони, оглушения.","Быстрые атаки, яд и рывки за спину врагу.","Дальние атаки, поджог, контроль толпы и метеор."};
    for(int i=0;i<3;++i){
        float x=cx-540+float(i)*365,y=cy-200;
        ui.panel(x,y,340,400);
        auto cls=classes[i];
        framedIcon(ui,x+20,y+18,88,portraitIcon(cls),bronze);
        ui.heading(x+122,y+16,combat::className(cls),26);
        ui.text(x+122,y+52,weapon[i],16,ui::text);
        ui.paragraph(x+122,y+76,208,classLine(cls),12,ui::muted);
        ui.paragraph(x+20,y+116,300,text[i],15,ui::text);
        for(int slot=0;slot<4;++slot){
            float sx=x+20+float(slot)*76;
            bool hot=ui.inside(sx,y+172,68,68);
            framedIcon(ui,sx,y+172,68,abilityIcon(cls,slot),hot?ui::gold:bronze);
            if(hot){
                const auto& ab=combat::ability(cls,slot);
                ui.tooltip=std::string(ab.name)+" — "+ab.text+"  Уровень "+std::to_string(ab.level)+".";
            }
        }
        ui.text(x+20,y+246,"Ветка развития",13,ui::gold);
        for(int t=0;t<3;++t)ui.text(x+20,y+267+float(t)*21,combat::talentName(cls,t),14,ui::text);
        if(ui.button(x+20,y+340,300,44,"Выбрать")){
            bool any=false;for(int s=0;s<4;++s)any|=rpg::Campaign::inspect(slotPath(s)).exists;
            selected=i;
            if(rpg::Campaign::inspect(slotPath(0)).exists){win=Window::ConfirmNew;}
            else startNewGame(classes[i]);
            (void)any;
        }
    }
    if(ui.button(cx-120,cy+230,240,44,"Назад"))screen=Screen::Menu;
}
void App::drawConfirm(bool newGame){
    float cx=ui.width*.5f,cy=ui.height*.5f;
    ui.panel(cx-330,cy-120,660,240,newGame?"Новая игра":"Выйти в главное меню?");
    if(newGame){
        ui.paragraph(cx-300,cy-70,600,"Автосохранение будет заменено новой игрой при первом же переходе. Ручные слоты 1–3 останутся нетронутыми.",16);
        if(ui.button(cx-300,cy+50,280,46,"Начать заново")){win=Window::None;startNewGame(combat::Class(std::max(0,selected)));}
        if(ui.button(cx+20,cy+50,280,46,"Отмена"))win=Window::None;
    }else{
        ui.paragraph(cx-300,cy-70,600,"Прогресс после последнего сохранения (автосохранение или святилище) будет потерян.",16);
        if(ui.button(cx-300,cy+50,280,46,"В главное меню")){screen=Screen::Menu;win=Window::None;stack.clear();syncActors(true);refreshVisuals(true);}
        if(ui.button(cx+20,cy+50,280,46,"Отмена"))win=Window::Pause;
    }
}

// ------------------------------------------------------------------ HUD (action-RPG layout)
void App::drawHud(){
    auto& g=game.game;const auto& h=game.hero();
    float cx=ui.width*.5f;
    // objective
    ui.panel(cx-330,12,660,72);
    ui.headingCentered(cx,17,game.zone().name,23);
    ui.textCentered(cx,50,"Цель: "+game.objective(),15,ui::text);
    // boss bar
    for(size_t i=1;i<g.actors.size();++i){const auto& a=g.actors[i];if(!a.alive()||!a.boss||!a.aggro)continue;
        float bw=620,bx=cx-bw*.5f,by=96;
        ui.quad(bx-6,by-4,bw+12,48,{.02f,.03f,.04f,.85f});
        ui.textCentered(cx,by-2,a.name+(a.vulnerable>0?"  —  УЯЗВИМ":""),16,a.vulnerable>0?ui::gold:ui::red);
        ui.bar(bx,by+24,bw,12,float(a.hp)/float(a.maxHp),a.vulnerable>0?ui::gold:glm::vec4(.85f,.18f,.12f,1));
        std::string st=g.status(a);if(st!="Нет эффектов")ui.textCentered(cx,by+44,st,13,ui::muted);
        break;}
    // nameplates over heads
    for(size_t i=1;i<g.actors.size()&&i<actorPos.size();++i){const auto& a=g.actors[i];if(!a.alive()||a.boss)continue;
        if(glm::length(a.pos-h.pos)>16.f)continue;
        float top=a.kind==combat::Kind::Pylon?2.9f:2.1f;auto p=project(actorPos[i]+glm::vec3(0,top,0));
        float w=70;bool hot=int(i)==hoverActor||int(i)==h.target;
        ui.quad(p.x-w*.5f-1,p.y-1,w+2,8,{0,0,0,.75f});ui.bar(p.x-w*.5f,p.y,w,6,float(a.hp)/float(a.maxHp),a.aggro?glm::vec4(.9f,.25f,.18f,1):glm::vec4(.75f,.55f,.2f,1));
        if(hot)ui.textCentered(p.x,p.y-24,a.name,14,ui::gold);
        if(a.stun>0)ui.textCentered(p.x,p.y+8,"оглушён",11,ui::gold);
    }
    {auto p=project(heroPos+glm::vec3(0,3.55f,0));ui.quad(p.x-41,p.y-1,82,8,{0,0,0,.75f});ui.bar(p.x-40,p.y,80,6,float(h.hp)/float(h.maxHp),ui::mint);}
    for(const auto& f:floaters){if(f.time<0)continue;auto p=project(f.position+glm::vec3(0,f.time*1.2f,0));glm::vec4 c=f.color;c.a=std::clamp(1.4f-f.time,0.f,1.f);
        ui.textCentered(p.x,p.y,f.text,f.text.size()<5?24.f:18.f,c);}
    // hovered interactable
    if(hoverEntity>=0&&win==Window::None){
        const auto& e=game.entities[size_t(hoverEntity)];auto p=project(game.map.center(e.cell)+glm::vec3(0,e.def.type==rpg::EntityType::Npc?2.6f:1.6f,0));
        std::string label=e.def.label[0]?e.def.label:"Объект";std::string hint="ПКМ — подойти и взаимодействовать";
        float w=std::max(ui.textWidth(label,15),ui.textWidth(hint,12))+20;
        ui.quad(p.x-w*.5f,p.y-6,w,46,{.02f,.03f,.04f,.85f});ui.textCentered(p.x,p.y-2,label,15,ui::gold);ui.textCentered(p.x,p.y+20,hint,12,ui::muted);
    }
    // toasts
    float ty=128;
    for(const auto& t:game.toasts){float a=std::clamp(5.f-t.time,0.f,1.f);float w=std::min(560.f,ui.textWidth(t.text,15)+30);float x=ui.width-w-14;
        ui.quad(x,ty,w,30,{.02f,.03f,.04f,.8f*a});ui.quad(x,ty,3,30,ui::rarity(t.rarity)*glm::vec4(1,1,1,a));
        glm::vec4 c=ui::rarity(t.rarity);c.a=a;ui.text(x+14,ty+6,t.text,15,c);ty+=34;}
    drawBattleHud();
}
void App::drawBattleHud(){
    auto& g=game.game;auto& h=g.actors[0];
    float cx=ui.width*.5f,pw=actionBarWidth,px=cx-pw*.5f,py=ui.height-actionBarBottom;
    ui.panel(px,py,pw,actionBarHeight);
    // portrait
    framedIcon(ui,px+12,py+12,116,portraitIcon(game.heroClass),bronze);
    ui.quad(px+14,py+99,112,27,ink);
    ui.textCentered(px+70,py+100,std::string(combat::className(game.heroClass))+" · "+std::to_string(h.level),14,ui::gold);
    ui.bar(px+14,py+123,112,3,float(h.xp)/float(h.level*100),ui::gold);
    if(h.skillPoints>0){ui.quad(px+100,py+16,24,24,ui::gold);ui.textCentered(px+112,py+18,"+",18,{0,0,0,1});
        if(ui.inside(px+100,py+16,24,24)){ui.tooltip="Есть очки развития — откройте окно героя (C)";ui.overUi=true;if(ui.pressed){win=Window::Character;ui.pressed=false;}}}
    // abilities
    float ax=px+146,ay=py+12,asz=78,agap=8;
    const char* keysQ[]={"Q","W","E","R"};
    for(int s=0;s<4;++s){
        const auto& ab=combat::ability(game.heroClass,s);float x=ax+float(s)*(asz+agap);
        bool unlocked=g.abilityUnlocked(s);bool ready=unlocked&&h.cd[s]<=0&&h.mana>=float(ab.mana);
        bool hot=ui.inside(x,ay,asz,asz);
        framedIcon(ui,x,ay,asz,abilityIcon(game.heroClass,s),hot?ui::gold:bronze,ready?1.f:unlocked?.70f:.43f);
        if(h.cd[s]>0){
            float k=std::clamp(h.cd[s]/ab.cooldown,0.f,1.f);
            ui.quad(x+2,ay+2+(asz-4)*(1-k),asz-4,(asz-4)*k,{.02f,.04f,.05f,.68f});
            ui.quad(x+asz*.5f-17,ay+asz*.5f-13,34,28,ink);
            ui.textCentered(x+asz*.5f,ay+asz*.5f-14,std::to_string(int(std::ceil(h.cd[s]))),21,ui::text);
        }
        keyLabel(ui,x+2,ay+2,keysQ[s]);
        float labelWidth=unlocked?25.f:44.f;
        ui.quad(x+asz-labelWidth-2,ay+asz-20,labelWidth,18,ink);
        std::string cost=unlocked?std::to_string(ab.mana):"ур. "+std::to_string(ab.level);
        ui.textCentered(x+asz-2-labelWidth*.5f,ay+asz-20,cost,12,unlocked&&h.mana<float(ab.mana)?ui::red:unlocked?ui::mint:ui::text);
        if(ui.inside(x,ay,asz,asz)){ui.overUi=true;hoverAbility=s;
            char cdText[16];std::snprintf(cdText,sizeof(cdText),"%.0f",double(ab.cooldown));
            ui.tooltip=std::string(ab.name)+" ["+keysQ[s]+"] — "+ab.text+"  Мана "+std::to_string(ab.mana)+", перезарядка "+cdText+" с."+(unlocked?"":"  Откроется на уровне "+std::to_string(ab.level)+".");
            if(ui.pressed){ui.pressed=false;castSlot(s);}}
    }
    // potions
    float ix=ax+4*(asz+agap)+6;int potions=game.count("potion")+game.count("greater_potion");
    framedIcon(ui,ix,ay,asz,potionIcon,ui.inside(ix,ay,asz,asz)?ui::gold:bronze,potions?1.f:.45f);
    if(g.itemCooldown>0){float k=std::clamp(g.itemCooldown/1.5f,0.f,1.f);ui.quad(ix+2,ay+2+(asz-4)*(1-k),asz-4,(asz-4)*k,{0,0,0,.6f});}
    keyLabel(ui,ix+2,ay+2,"H");
    std::string count="×"+std::to_string(potions);
    float countWidth=ui.textWidth(count,14)+8;
    ui.quad(ix+asz-2-countWidth,ay+asz-22,countWidth,20,ink);
    ui.text(ix+asz+2-countWidth,ay+asz-22,count,14,potions?ui::text:ui::red);
    if(ui.inside(ix,ay,asz,asz)){ui.overUi=true;ui.tooltip="Выпить зелье лечения (H). Перезарядка 1,5 с.";if(ui.pressed){ui.pressed=false;drinkPotion();}}
    // health and mana
    float bx=px+146,bw=pw-146-14;
    ui.bar(bx,py+98,bw,16,float(h.hp)/float(h.maxHp),h.hp*3<h.maxHp?ui::red:glm::vec4(.51f,.17f,.19f,1));
    ui.textCentered(bx+bw*.5f,py+97,std::to_string(h.hp)+" / "+std::to_string(h.maxHp),13,ui::text);
    ui.bar(bx,py+117,bw,13,h.mana/std::max(1.f,h.maxMana),{.17f,.39f,.43f,1});
    ui.textCentered(bx+bw*.5f,py+115,std::to_string(int(h.mana))+" / "+std::to_string(int(h.maxMana)),12,ui::text);
    // gold, status and window buttons
    float rx=ix+asz+14;
    ui.emblem(rx,ay+1,22,ui::Emblem::Coins);
    ui.text(rx+27,ay+3,std::to_string(game.gold),16,ui::gold);
    std::string st=g.status(h);ui.paragraph(rx,ay+24,px+pw-rx-10,st=="Нет эффектов"?"":st,12,ui::red);
    drawWindowShortcuts(rx,ay+45,true);
    // target card
    int target=hoverActor>0?hoverActor:h.target;
    if(target>0&&target<int(g.actors.size())&&g.actors[size_t(target)].alive()&&!g.actors[size_t(target)].boss){
        const auto& a=g.actors[size_t(target)];float x=14,y=ui.height-300;
        ui.panel(x,y,330,140);
        ui.text(x+14,y+10,a.name,17,ui::gold);
        ui.text(x+14,y+38,"HP "+std::to_string(a.hp)+" / "+std::to_string(a.maxHp)+"   броня "+std::to_string(g.defense(target)),14,ui::text);
        ui.bar(x+14,y+60,300,6,float(a.hp)/float(a.maxHp),ui::red);
        ui.paragraph(x+14,y+74,300,combat::kindInfo(a.kind).trait,13,ui::muted);
        std::string st2=g.status(a);if(st2!="Нет эффектов")ui.paragraph(x+14,y+100,300,st2,13,ui::mint);
        if(a.kind!=combat::Kind::Pylon)ui.text(x+14,y+118,"Ваш удар: ~"+std::to_string(g.attackDamage(0,target,1)),13,ui::gold);
    }
    if(!g.inBattle()&&!ui.overUi&&win==Window::None)
        ui.textCentered(cx,py-26,"ПКМ — идти / атаковать / говорить   ·   Q W E R — навыки   ·   H — зелье   ·   F1 — справка",13,ui::muted);
}

void App::drawWindowShortcuts(float x,float y,bool compact){
    using E=ui::Emblem;
    struct Tab {Window window;E icon;const char* name;const char* key;};
    static const Tab tabs[]={
        {Window::Inventory,E::Pack,"Сумка","I"},{Window::Character,E::Helm,"Герой","C"},
        {Window::Journal,E::Journal,"Журнал","J"},{Window::Map,E::Map,"Карта","M"}
    };
    float w=compact?43.f:146.f,h=compact?36.f:42.f;
    for(const auto& tab:tabs){
        bool active=win==tab.window;
        if(ui.button(x,y,w,h,compact?"":tab.name,true,"",active,17,compact?E::None:tab.icon)){
            if(active)closeWindow();else setWindow(tab.window);
        }
        if(compact)ui.emblem(x+3,y+2,h-4,tab.icon);
        ui.quad(x+w-14,y+h-16,13,15,ink);
        ui.textCentered(x+w-7.5f,y+h-16,tab.key,12,ui::gold);
        if(compact&&ui.inside(x,y,w,h))ui.tooltip=std::string(tab.name)+" ("+tab.key+")";
        x+=w+7;
    }
}

bool App::overActionBar() const{
    return hudVisible&&ui.inside((ui.width-actionBarWidth)*.5f,ui.height-actionBarBottom,actionBarWidth,actionBarHeight);
}

// ------------------------------------------------------------------ window dispatcher
void App::drawWindows(){
    if(win!=displayedWindow){displayedWindow=win;windowOpenedAt=realTime;}
    if(win!=Window::None)ui.quad(0,0,ui.width,ui.height,{.018f,.023f,.027f,.48f});
    switch(win){
    case Window::Inventory:drawInventory(false);break;
    case Window::Shop:drawInventory(true);break;
    case Window::Character:drawCharacter();break;
    case Window::Journal:drawJournal();break;
    case Window::Map:drawMap();break;
    case Window::Craft:drawCraft();break;
    case Window::Smith:drawSmith();break;
    case Window::Shrine:drawShrine();break;
    case Window::Travel:drawTravel();break;
    case Window::Dialog:drawDialog();break;
    case Window::Note:drawNote();break;
    case Window::Reward:drawReward();break;
    case Window::Pause:drawPause();break;
    case Window::Settings:drawSettings();break;
    case Window::Help:drawHelp(false);break;
    case Window::Credits:drawHelp(true);break;
    case Window::Load:drawSlots(false);break;
    case Window::Save:drawSlots(true);break;
    case Window::Defeat:drawDefeat();break;
    case Window::Epilogue:drawEpilogue(false);break;
    case Window::Ending:drawEpilogue(true);break;
    case Window::ConfirmNew:drawConfirm(true);break;
    case Window::ConfirmMenu:drawConfirm(false);break;
    default:break;
    }
    if(screen!=Screen::Game||win==Window::None)return;
    bool live=win==Window::Inventory||win==Window::Character||win==Window::Journal||win==Window::Map;
    if(live&&game.game.inBattle()){
        const auto& h=game.hero();
        std::string statusLine="В бою   ·   Здоровье "+std::to_string(h.hp)+" / "+std::to_string(h.maxHp);
        float width=ui.textWidth(statusLine,16)+58,x=(ui.width-width)*.5f;
        ui.quad(x,12,width,33,ink);ui.frame(x,12,width,33,ui::brass);
        ui.emblem(x+5,15,27,ui::Emblem::Sword);ui.text(x+40,18,statusLine,16,ui::text);
    }else if((win==Window::Shop||win==Window::Craft||win==Window::Smith||win==Window::Save||
              win==Window::Shrine||win==Window::Inventory)&&!game.toasts.empty()&&
              game.toasts.back().time<=realTime-windowOpenedAt){
        const auto& notice=game.toasts.back();
        float width=std::min(670.f,ui.textWidth(notice.text,15)+26),x=(ui.width-width)*.5f;
        float height=ui.paragraph(0,0,width-22,notice.text,15,ui::text,1.45f,true)+10;
        ui.quad(x,12,width,height,ink);ui.quad(x,12,2,height,ui::rarity(notice.rarity));
        ui.paragraph(x+11,16,width-22,notice.text,15,ui::text);
    }
}

// ------------------------------------------------------------------ inventory and trade
namespace {
bool equipment(ItemType type){
    return type==ItemType::Weapon || type==ItemType::Armor || type==ItemType::Talisman;
}

ui::Emblem itemEmblem(const rpg::ItemDef& item){
    using E=ui::Emblem;
    std::string_view id=item.id;
    if(item.type==ItemType::Weapon){
        switch(item.weapon){
        case combat::Weapon::Sword: return E::Sword;
        case combat::Weapon::Daggers: return E::Daggers;
        case combat::Weapon::Bow: return E::Bow;
        case combat::Weapon::Staff: return E::Staff;
        }
    }
    if(item.type==ItemType::Armor)return E::Armor;
    if(id=="wolf_tooth")return E::Fang;
    if(id=="ashen_crown")return E::Helm;
    if(id=="knight_sigil" || item.type==ItemType::Talisman)return E::Charm;
    if(id=="venom" || item.type==ItemType::Consumable)return E::Vial;
    if(id=="ore" || id=="iron_core")return E::Ore;
    if(id=="herb")return E::Herb;
    if(id=="silk" || id=="matriarch_silk")return E::Silk;
    if(id=="bone")return E::Bone;
    if(id=="smith_cargo")return E::Chest;
    if(id=="climbing_gear")return E::Gear;
    if(id=="mine_key" || id=="crypt_key" || id=="guard_key")return E::Key;
    return item.type==ItemType::Quest?E::Shrine:E::Pack;
}

void itemImage(Ui& ui,float x,float y,float size,const rpg::ItemDef& item){
    ui.quad(x,y,size,size,glm::mix(bronze,ui::rarity(int(item.rarity)),.24f));
    if(item.heal>=35)ui.icon(x+1,y+1,size-2,potionIcon);
    else ui.emblem(x+1,y+1,size-2,itemEmblem(item));
}

std::string stackName(const rpg::ItemDef& item,int upgrade){
    return std::string(item.name)+(upgrade?" +"+std::to_string(upgrade):"");
}

void fitItemName(Ui& ui,float x,float y,float width,const std::string& name,float size,glm::vec4 color){
    if(ui.textWidth(name,size)<=width){ui.text(x,y,name,size,color);return;}
    float occupied=ui.textWidth("…",size);
    size_t length=0;
    while(length<name.size()){
        size_t next=length;
        float advance=Ui::glyphAdvance(glyphIndex(nextGlyph(name,next)),size,Ui::Face::Body);
        if(occupied+advance>width)break;
        occupied+=advance;length=next;
    }
    ui.text(x,y,name.substr(0,length)+"…",size,color);
}

void sectionName(Ui& ui,float x,float y,float width,const std::string& name){
    ui.heading(x,y,name,19,ui::gold);
    ui.quad(x,y+30,width,1,{.38f,.31f,.22f,.82f});
}
}

void App::itemDetails(float x,float y,float w,const rpg::Stack& s,bool compare){
    const auto* d=rpg::item(s.id);
    if(!d)return;
    auto color=ui::rarity(int(d->rarity));
    const float imageSize=w>=380?98.f:68.f;
    const float titleX=x+imageSize+15;
    itemImage(ui,x,y,imageSize,*d);
    ui.text(titleX,y+8,rpg::rarityName(d->rarity),15,color);
    ui.paragraph(titleX,y+32,w-imageSize-15,stackName(*d,s.upgrade),21,ui::text,1.12f);
    std::string category=rpg::typeName(d->type);
    if(s.count>1)category+="  ×"+std::to_string(s.count);
    ui.text(x,y+imageSize+11,category,15,ui::muted);
    ui.quad(x,y+imageSize+37,w,1,{.40f,.32f,.22f,.85f});
    float yy=y+imageSize+49;
    auto stat=[&](const std::string& label,const std::string& value){
        ui.text(x,yy,label,15,ui::muted);
        ui.text(x+w-ui.textWidth(value,15),yy,value,15,ui::text);
        yy+=23;
    };
    if(d->type==ItemType::Weapon){
        const auto& ws=combat::weaponStats(d->weapon);
        stat(ws.name,std::to_string(ws.damage+d->damage+s.upgrade*2)+" урона");
        char reach[32],interval[32];
        std::snprintf(reach,sizeof(reach),"%.1f м",double(ws.range+float(d->range)*.8f));
        std::snprintf(interval,sizeof(interval),"%.2f с",double(ws.interval));
        stat("Дальность",reach);stat("Интервал атак",interval);
    }
    if(d->armor)stat("Броня",plus(d->armor));
    if(d->hp)stat("Здоровье",plus(d->hp));
    if(d->initiative)stat("Инициатива",plus(d->initiative));
    if(d->type!=ItemType::Weapon&&d->damage)stat("Урон",plus(d->damage));
    if(d->heal)stat("Лечение",std::to_string(d->heal));
    if(equipment(d->type)){
        std::string who;
        const char* names[]={"воин","разбойник","маг"};
        for(int c=0;c<3;++c)if(d->classMask&(1<<c))who+=(who.empty()?"":", ")+std::string(names[c]);
        stat("Класс",who);
        if(!rpg::canEquip(*d,game.heroClass)){ui.text(x,yy,"Для другого класса",14,ui::red);yy+=22;}
    }
    yy+=8+ui.paragraph(x,yy+8,w,d->desc,15,ui::text);
    if(compare&&equipment(d->type)){
        int slot=d->type==ItemType::Weapon?0:d->type==ItemType::Armor?1:2;
        const auto& equipped=game.equip[slot];
        const auto* cur=rpg::item(equipped.id);
        yy+=14;
        ui.text(x,yy,"Сравнение с надетым",15,ui::gold);
        yy+=23;
        if(!cur){ui.text(x,yy,"Слот пуст",14,ui::muted);return;}
        auto damage=[](const rpg::ItemDef& it,int upgrade){
            return it.damage+(it.type==ItemType::Weapon?combat::weaponStats(it.weapon).damage+upgrade*2:0);
        };
        int delta=damage(*d,s.upgrade)-damage(*cur,equipped.upgrade);
        std::string line="Урон "+plus(delta)+"    Броня "+plus(d->armor-cur->armor)+"    Здоровье "+plus(d->hp-cur->hp)+"    Инициатива "+plus(d->initiative-cur->initiative);
        ui.paragraph(x,yy,w,line,14,ui::text);
    }
}

void App::drawInventory(bool shop){
    const float w=std::min(ui.width-72.f,1540.f),h=std::min(ui.height-158.f,770.f);
    const float x=(ui.width-w)*.5f,y=(ui.height-h)*.5f;
    ui.panel(x,y,w,h,shop?(shopNpc=="smith"?"Торговля у кузнеца Борна":"Лавка Миры"):"Сумка героя",shop?ui::Emblem::Coins:ui::Emblem::Pack);
    if(ui.button(x+w-152,y+11,134,34,"Закрыть"))closeWindow();
    ui.emblem(x+w-310,y+13,28,ui::Emblem::Coins);
    ui.text(x+w-273,y+15,std::to_string(game.gold),18,ui::gold);

    const float lx=x+24,top=y+65,lw=std::floor(w*.28f);
    const float rightW=std::floor(w*(shop?.30f:.27f)),rx=x+w-24-rightW;
    const float dx=lx+lw+26,dw=rx-dx-26,base=top+42,bottom=y+h-20;
    sectionName(ui,lx,top,lw,"Предметы · "+std::to_string(game.inventory.size()));
    ui.quad(dx-13,top,1,bottom-top,{.32f,.27f,.20f,.86f});
    ui.quad(rx-13,top,1,bottom-top,{.32f,.27f,.20f,.86f});
    sectionName(ui,dx,top,dw,"Сведения о предмете");

    std::vector<int> order(game.inventory.size());
    for(size_t i=0;i<order.size();++i)order[i]=int(i);
    std::stable_sort(order.begin(),order.end(),[&](int a,int b){
        return rpg::item(game.inventory[a].id)->type<rpg::item(game.inventory[b].id)->type;
    });
    constexpr float row=46;
    const int rows=std::max(1,int((bottom-base-38)/row));
    const int pages=std::max(1,(int(order.size())+rows-1)/rows);
    inventoryPage=std::clamp(inventoryPage,0,pages-1);
    if(!order.empty()&&(selected<0||selected>=int(game.inventory.size())))selected=order[inventoryPage*rows];
    if(pages>1){
        int previousPage=inventoryPage;
        if(ui.button(lx,bottom-30,38,30,"←",inventoryPage>0,"Первая страница"))--inventoryPage;
        ui.textCentered(lx+lw*.5f,bottom-26,std::to_string(inventoryPage+1)+" / "+std::to_string(pages),15,ui::muted);
        if(ui.button(lx+lw-38,bottom-30,38,30,"→",inventoryPage+1<pages,"Последняя страница"))++inventoryPage;
        if(previousPage!=inventoryPage)selected=order[inventoryPage*rows];
    }
    for(int k=inventoryPage*rows;k<std::min(int(order.size()),(inventoryPage+1)*rows);++k){
        int index=order[k];
        const auto& itemStack=game.inventory[index];
        const auto& item=*rpg::item(itemStack.id);
        float yy=base+float(k%rows)*row;
        bool hot=ui.inside(lx,yy,lw,row-2);
        if(hot&&ui.pressed){selected=index;ui.pressed=false;++ui.clicks;}
        bool active=selected==index;
        ui.quad(lx,yy,lw,row-2,active?glm::vec4(.15f,.20f,.20f,1):hot?glm::vec4(.12f,.15f,.15f,1):glm::vec4(.08f,.10f,.11f,1));
        if(active)ui.quad(lx,yy,3,row-2,ui::gold);
        itemImage(ui,lx+7,yy+4,36,item);
        float end=lx+lw-9;
        if(itemStack.count>1){
            std::string qty="×"+std::to_string(itemStack.count);
            end-=ui.textWidth(qty,15);
            ui.text(end,yy+11,qty,15,ui::muted);
            end-=8;
        }
        fitItemName(ui,lx+54,yy+4,end-lx-54,stackName(item,itemStack.upgrade),16,ui::rarity(int(item.rarity)));
        ui.text(lx+54,yy+24,rpg::typeName(item.type),13,ui::muted);
    }
    if(order.empty())ui.text(lx,base+8,"Сумка пуста",16,ui::muted);
    if(selected>=0&&selected<int(game.inventory.size())){
        auto itemStack=game.inventory[selected];
        const auto& item=*rpg::item(itemStack.id);
        itemDetails(dx,base+4,dw,itemStack,true);
        float btnW=shop?(dw-10)*.5f:dw;
        std::string why;
        if(item.type==ItemType::Consumable){
            if(ui.button(dx,bottom-46,btnW,46,"Использовать",game.game.itemCooldown<=0,"Зелья перезаряжаются")){
                if(!game.useItem(selected,why))game.toast(why,1);
                selected=-1;
            }
        }else if(equipment(item.type)){
            bool fits=rpg::canEquip(item,game.heroClass);
            bool armorLocked=item.type==ItemType::Armor&&game.game.inBattle();
            if(ui.button(dx,bottom-46,btnW,46,"Надеть",fits&&!armorLocked,armorLocked?"Броню нельзя менять в бою":"Не подходит вашему классу")){
                if(!game.equipItem(selected,why))game.toast(why,1);
                selected=-1;
            }
        }
        if(shop&&selected>=0){
            bool sellable=item.type!=ItemType::Quest&&item.rarity!=rpg::Rarity::Unique;
            std::string label="Продать · "+std::to_string(rpg::sellPrice(item,itemStack.upgrade));
            if(ui.button(dx+btnW+10,bottom-46,btnW,46,label,sellable,item.type==ItemType::Quest?"Сюжетный предмет нельзя продать":"Уникальные вещи продавать нельзя")){
                if(game.sell(selected,why))audio->play(Sfx::Loot,.6f);else game.toast(why,1);
                selected=-1;
            }
        }
    }
    float equipmentY=top;
    if(shop){
        sectionName(ui,rx,top,rightW,"Товары");
        auto stock=game.shopStock(shopNpc);
        for(size_t k=0;k<stock.size();++k){
            const auto& item=*rpg::item(stock[k].first);
            float yy=base+float(k)*48;
            ui.quad(rx,yy,rightW,45,{.08f,.10f,.11f,1});
            itemImage(ui,rx+5,yy+4,36,item);
            fitItemName(ui,rx+52,yy+3,rightW-142,item.name,16,ui::rarity(int(item.rarity)));
            ui.text(rx+52,yy+23,std::to_string(stock[k].second)+" золота",14,ui::gold);
            if(ui.inside(rx,yy,rightW-84,45)){
                ui.tooltip=std::string(item.name)+" — "+item.desc;
                if(!rpg::canEquip(item,game.heroClass)&&equipment(item.type))ui.tooltip+=" (Для другого класса)";
            }
            std::string why;
            if(ui.button(rx+rightW-83,yy+6,77,33,"Купить",game.gold>=stock[k].second,"Не хватает золота",false,14)){
                if(game.buy(shopNpc,stock[k].first,why))audio->play(Sfx::Loot,.7f);else game.toast(why,1);
            }
        }
        equipmentY=base+float(stock.size())*48+16;
    }
    sectionName(ui,rx,equipmentY,rightW,"Снаряжение");
    const ui::Emblem emptySlots[]={ui::Emblem::Sword,ui::Emblem::Armor,ui::Emblem::Charm};
    for(int slot=0;slot<3;++slot){
        float yy=equipmentY+41+float(slot)*61;
        const auto& itemStack=game.equip[slot];
        const auto* item=rpg::item(itemStack.id);
        ui.quad(rx,yy,rightW,57,{.08f,.10f,.11f,1});
        if(item)itemImage(ui,rx+6,yy+6,45,*item);
        else ui.emblem(rx+6,yy+6,45,emptySlots[slot],.43f);
        ui.text(rx+61,yy+5,slotName(slot),13,ui::muted);
        if(!item){ui.text(rx+61,yy+25,"Слот пуст",16,ui::muted);continue;}
        fitItemName(ui,rx+61,yy+24,rightW-150,stackName(*item,itemStack.upgrade),16,ui::rarity(int(item->rarity)));
        if(ui.inside(rx,yy,rightW-81,57))ui.tooltip=std::string(item->name)+" — "+item->desc;
        if(ui.button(rx+rightW-76,yy+11,70,34,"Снять",slot!=0&&!game.game.inBattle(),slot==0?"Чтобы сменить оружие, выберите другое в сумке":"Снимать снаряжение можно вне боя",false,14)){
            std::string why;if(!game.unequip(slot,why))game.toast(why,1);
        }
    }
}

// ------------------------------------------------------------------ character and talents
void App::drawCharacter(){
    auto& h=game.hero();auto& g=game.game;
    float x=ui.width*.5f-560,y=110,w=1120,hgt=640;
    ui.panel(x,y,w,hgt,"Лист героя",ui::Emblem::Helm);
    if(ui.button(x+w-150,y+10,136,36,"Закрыть"))closeWindow();
    float left=x+28,tx=x+558;
    auto decimal=[](float v){char buffer[24];std::snprintf(buffer,sizeof(buffer),"%.1f",double(v));return std::string(buffer);};
    framedIcon(ui,left,y+68,88,portraitIcon(game.heroClass),bronze);
    ui.heading(left+106,y+68,combat::className(game.heroClass),27,ui::gold);
    ui.text(left+106,y+110,"Уровень "+std::to_string(h.level)+"   ·   Опыт "+std::to_string(h.xp)+" / "+std::to_string(h.level*100),16,ui::text);
    ui.bar(left+106,y+140,320,6,float(h.xp)/float(h.level*100),ui::gold);
    auto stat=[&](float offset,const std::string& name,const std::string& value){
        ui.text(left,y+offset,name,16,ui::muted);
        ui.text(left+195,y+offset,value,16,ui::text);
    };
    auto ws=combat::weaponStats(h.weapon);
    stat(182,"Здоровье",std::to_string(h.hp)+" / "+std::to_string(h.maxHp));
    stat(211,"Мана",std::to_string(int(h.mana))+" / "+std::to_string(int(h.maxMana))+"  (+"+decimal(h.manaRegen)+"/с)");
    stat(240,"Сила / броня",std::to_string(h.power)+" / "+std::to_string(h.armor+h.armorBonus));
    stat(269,"Золото",std::to_string(game.gold));
    ui.quad(left,y+309,494,1,bronze);
    ui.heading(left,y+323,ws.name,21,ui::gold);
    stat(360,"Урон / дальность",std::to_string(ws.damage+h.damageBonus+h.power)+" / "+decimal(g.attackRange(h))+" м");
    stat(389,"Атак в секунду",decimal(1.f/g.attackInterval(h)));
    for(int i=0;i<4;++i){
        float sx=left+float(i)*125.f;
        bool unlocked=g.abilityUnlocked(i);
        const auto& ab=combat::ability(game.heroClass,i);
        framedIcon(ui,sx,y+435,52,abilityIcon(game.heroClass,i),bronze,unlocked?1.f:.50f);
        ui.paragraph(sx,y+492,117,ab.name,14,unlocked?ui::text:ui::muted);
        if(ui.inside(sx,y+435,117,92))ui.tooltip=std::string(ab.text)+"  Уровень "+std::to_string(ab.level)+".";
    }
    ui.paragraph(left,y+559,495,"Сила прибавляется к урону оружия. Броня цели уменьшает урон до минимума в 1. Уязвимая цель теряет броню и получает на 50% больше урона.",14,ui::muted);
    ui.quad(tx-22,y+68,1,hgt-102,bronze);
    ui.heading(tx,y+64,"Развитие",23,ui::gold);
    ui.text(tx+290,y+70,"Свободных очков: "+std::to_string(h.skillPoints),15,h.skillPoints?ui::mint:ui::muted);
    static const int levelReq[]={2,4,6};
    for(int t=0;t<3;++t){
        float yy=y+100+float(t)*120;bool learned=h.talents&(1<<t);
        ui.quad(tx,yy,520,108,learned?glm::vec4(.10f,.20f,.17f,1):glm::vec4(.06f,.08f,.09f,1));
        ui.quad(tx,yy,3,108,learned?ui::mint:bronze);
        ui.heading(tx+14,yy+8,combat::talentName(game.heroClass,t),18,learned?ui::mint:ui::gold);
        ui.paragraph(tx+14,yy+36,360,combat::talentText(game.heroClass,t),14,ui::text);
        ui.text(tx+14,yy+84,"Требуется уровень "+std::to_string(levelReq[t]),12,ui::muted);
        std::string why;
        std::string reason=learned?"Уже изучено":(t>0&&!(h.talents&(1<<(t-1))))?"Сначала изучите предыдущее улучшение":h.level<levelReq[t]?"Нужен уровень "+std::to_string(levelReq[t]):h.skillPoints<1?"Нет очков развития":"";
        if(ui.button(tx+390,yy+34,116,40,learned?"Изучено":"Изучить",reason.empty(),reason,learned,14)){if(!game.learnTalent(t,why))game.toast(why,1);else audio->play(Sfx::LevelUp,.6f);}
    }
    ui.text(tx,y+470,"Свободные очки можно вложить в характеристики:",14,ui::muted);
    std::string none=h.skillPoints<1?"Нет очков развития":"";std::string why;
    if(ui.button(tx,y+500,250,42,"Закалка: +8 здоровья",none.empty(),none,false,14))game.learnTalent(3,why);
    if(ui.button(tx+270,y+500,250,42,"Мастерство: +1 сила",none.empty(),none,false,14))game.learnTalent(4,why);
}

// ------------------------------------------------------------------ journal
void App::drawJournal(){
    float x=ui.width*.5f-560,y=100,w=1120,h=680;
    ui.panel(x,y,w,h,"Журнал заданий",ui::Emblem::Journal);
    if(ui.button(x+w-150,y+10,136,36,"Закрыть"))closeWindow();
    float yy=y+64;
    for(const auto& q:game.journal()){
        float cardHeight=q.progress.empty()?109.f:133.f;
        ui.quad(x+20,yy,700,cardHeight,{.069f,.080f,.083f,1});
        ui.quad(x+20,yy,3,cardHeight,q.done?ui::muted:q.main?ui::gold:bronze);
        ui.emblem(x+34,yy+14,35,q.main?ui::Emblem::Journal:ui::Emblem::Scroll,q.done?.55f:1.f);
        ui.heading(x+83,yy+12,q.title,20,q.done?ui::muted:ui::gold);
        std::string state=q.done?"Выполнено":q.main?"Сюжет":"Поручение";
        ui.text(x+700-ui.textWidth(state,14),yy+15,state,14,q.done?ui::mint:ui::muted);
        float contentY=yy+45;
        contentY+=ui.paragraph(x+83,contentY,609,q.objective,15,ui::text);
        if(!q.progress.empty())ui.text(x+83,contentY+2,q.progress,14,ui::mint);
        ui.text(x+83,yy+cardHeight-28,"Награда: "+q.reward,14,ui::muted);
        yy+=cardHeight+8;
    }
    float nx=x+750;
    ui.quad(nx-14,y+65,1,h-96,bronze);
    ui.heading(nx,y+62,"Найденные записки",20,ui::gold);
    if(game.notes.empty())ui.paragraph(nx,y+92,340,"Пока пусто. Записки, дневники и надписи раскрывают историю падения замка.",14,ui::muted);
    for(size_t i=0;i<game.notes.size();++i){
        float top=y+108+float(i)*54;
        if(ui.button(nx,top,340,46,rpg::noteTitle(game.notes[i]),true,"",false,15,ui::Emblem::Scroll)){game.noteId=game.notes[i];stack.push_back(win);win=Window::Note;}
    }
}

// ------------------------------------------------------------------ map
void App::drawMap(){
    float x=40,y=90,w=ui.width-80,h=ui.height-150;
    using Icon=ui::Emblem;
    ui.panel(x,y,w,h,"Карта долины Эшвальд",Icon::Map);
    if(ui.button(x+w-150,y+10,136,36,"Закрыть"))closeWindow();
    struct Node{const char* id;float px,py;};
    const Node nodes[]={{"castle",.18f,.31f},{"heart",.12f,.12f},{"village",.16f,.58f},{"road",.41f,.58f},{"grove",.42f,.31f},{"lair",.50f,.12f},
                        {"mine",.65f,.70f},{"flooded",.82f,.83f},{"pass",.66f,.44f},{"crypt",.77f,.27f},{"throne",.87f,.10f}};
    const std::pair<int,int> links[]={{0,1},{0,2},{2,3},{3,4},{4,5},{3,6},{6,7},{3,8},{8,9},{9,10}};
    float mx=x+22,my=y+66,mw=w*.595f,mh=h-94;
    ui.quad(mx,my,mw,mh,{.066f,.091f,.093f,1});
    auto at=[&](int i){return glm::vec2(mx+nodes[i].px*mw,my+nodes[i].py*mh);};
    for(auto [a,b]:links){
        glm::vec2 p=at(a),q=at(b);
        int steps=std::max(1,int(glm::length(q-p)/9.f));
        for(int k=1;k<steps;++k){
            glm::vec2 dot=glm::mix(p,q,float(k)/float(steps));
            ui.quad(dot.x-1.5f,dot.y-1.5f,3,3,{.46f,.38f,.25f,1});
        }
    }
    const char* objZones[]={"village","village","grove","lair","village","flooded","village","pass","crypt","heart","village","village"};
    int st=game.stage("main");std::string goal=objZones[std::clamp(st,0,11)];
    auto pin=[&](glm::vec2 center,float size,Icon icon,glm::vec4 border,float light){
        ui.quad(center.x-size*.5f-1,center.y-size*.5f-1,size+2,size+2,border);
        ui.emblem(center.x-size*.5f,center.y-size*.5f,size,icon,light);
    };
    for(int i=0;i<11;++i){
        glm::vec2 p=at(i);std::string id=nodes[i].id;
        const auto& z=*rpg::zoneDef(id);
        bool visited=game.flags.count("visited:"+id)>0;
        bool known=visited||game.flags.count("found:"+id)||z.surface;
        bool here=id==game.zoneId;
        glm::vec4 color=here?ui::mint:visited?ui::gold:ui::muted;
        pin(p,here?39.f:33.f,here?Icon::Helm:z.surface?Icon::Map:Icon::Key,color,known?1.f:.48f);
        ui.textCentered(p.x,p.y+23,known?rpg::zoneName(id):"???",15,color);
        if(goal==id&&st<11){
            ui.emblem(p.x-36,p.y-47,20,Icon::Journal);
            ui.text(p.x-11,p.y-48,"Цель",15,ui::gold);
        }
        for(const auto& e:z.entities)if(e.type==rpg::EntityType::Shrine)
            ui.emblem(p.x+20,p.y-8,17,Icon::Shrine,game.shrineActive(e.id)?1.f:.46f);
    }
    float lx=mx+mw+24,ly=my+8,sideWidth=x+w-lx-24;
    ui.heading(lx,ly,"Обозначения",20,ui::gold);
    auto legend=[&](float xx,float yy,Icon icon,const char* label,glm::vec4 border){
        pin({xx+11,yy+11},20,icon,border,1);
        ui.text(xx+32,yy+1,label,15,ui::text);
    };
    legend(lx,ly+38,Icon::Helm,"Вы здесь",ui::mint);
    legend(lx,ly+72,Icon::Map,"Посещённое место",ui::gold);
    legend(lx,ly+106,Icon::Shrine,"Святилище",bronze);
    legend(lx+sideWidth*.52f,ly+38,Icon::Key,"Подземелье",ui::muted);
    legend(lx+sideWidth*.52f,ly+72,Icon::Journal,"Цель задания",ui::gold);
    ui.quad(lx,ly+145,sideWidth,1,bronze);
    ui.heading(lx,ly+160,game.zone().name,21,ui::gold);
    const auto& m=game.map;
    float oy=ly+201,cell=std::min(sideWidth/float(m.width),(y+h-62-oy)/float(m.height));
    float ox=lx+(sideWidth-cell*float(m.width))*.5f;
    for(int yy=0;yy<m.height;++yy)for(int xx=0;xx<m.width;++xx){
        auto t=m.at(xx,yy);glm::vec4 c{.12f,.15f,.12f,1};
        if(t==world::Tile::Wall)c={.03f,.03f,.04f,1};else if(t==world::Tile::Water)c={.10f,.22f,.32f,1};else if(t==world::Tile::Prop||t==world::Tile::LowProp)c={.08f,.12f,.08f,1};
        else if(t==world::Tile::Path)c={.28f,.24f,.16f,1};else if(t==world::Tile::Door)c={.45f,.25f,.1f,1};
        ui.quad(ox+float(xx)*cell,oy+float(yy)*cell,cell,cell,c);
    }
    for(const auto& e:game.entities){
        if(e.hidden)continue;
        Icon mark;float brightness=1;
        switch(e.def.type){
        case rpg::EntityType::Shrine:mark=Icon::Shrine;brightness=game.shrineActive(e.def.id)?1.f:.55f;break;
        case rpg::EntityType::Exit:case rpg::EntityType::Entrance:mark=Icon::Key;break;
        case rpg::EntityType::Npc:mark=Icon::Scroll;break;
        case rpg::EntityType::Chest:mark=Icon::Chest;brightness=e.done?.50f:1.f;break;
        default:continue;
        }
        ui.emblem(ox+float(e.cell.x)*cell-1,oy+float(e.cell.y)*cell-1,cell+2,mark,brightness);
    }
    for(size_t i=1;i<game.game.actors.size();++i)if(game.game.actors[i].alive()){
        auto c=game.game.actors[i].cell;
        ui.quad(ox+(float(c.x)+.2f)*cell,oy+(float(c.y)+.2f)*cell,cell*.6f,cell*.6f,ui::red);
    }
    Cell hc=game.hero().cell;
    pin({ox+(float(hc.x)+.5f)*cell,oy+(float(hc.y)+.5f)*cell},std::max(12.f,cell),Icon::Helm,ui::mint,1);
    float keyY=oy+float(m.height)*cell+12;
    legend(lx,keyY,Icon::Chest,"Сундук",bronze);
    legend(lx+sideWidth*.25f,keyY,Icon::Scroll,"Житель",bronze);
    legend(lx+sideWidth*.52f,keyY,Icon::Key,"Переход",bronze);
    ui.quad(lx+sideWidth*.80f,keyY+5,12,12,ui::red);
    ui.text(lx+sideWidth*.80f+20,keyY+1,"Враг",15,ui::text);
}

// ------------------------------------------------------------------ crafting and smith
void App::drawCraft(){
    const float w=1040,h=628,x=(ui.width-w)*.5f,y=(ui.height-h)*.5f;
    ui.panel(x,y,w,h,"Ремесло травницы Ивы",ui::Emblem::Herb);
    if(ui.button(x+w-150,y+10,136,36,"Закрыть"))closeWindow();
    const auto& recipes=rpg::recipes();
    for(size_t i=0;i<recipes.size();++i){
        const auto& recipe=recipes[i];
        const auto& output=*rpg::item(recipe.result);
        float yy=y+71+float(i)*135;
        ui.quad(x+20,yy,w-40,123,{.079f,.097f,.100f,1});
        itemImage(ui,x+32,yy+13,72,output);
        ui.heading(x+122,yy+11,output.name,21,ui::rarity(int(output.rarity)));
        ui.paragraph(x+122,yy+41,w-336,output.desc,15,ui::muted);
        float ix=x+122;
        auto ingredient=[&](ui::Emblem art,const std::string& label,bool enough){
            ui.emblem(ix,yy+80,24,art);
            ui.text(ix+30,yy+83,label,14,enough?ui::text:ui::red);
            ix+=ui.textWidth(label,14)+47;
        };
        for(const auto& [id,needed]:recipe.needs){
            const auto& material=*rpg::item(id);
            int held=game.count(id);
            ingredient(itemEmblem(material),std::string(material.name)+" "+std::to_string(held)+"/"+std::to_string(needed),held>=needed);
        }
        if(recipe.gold)ingredient(ui::Emblem::Coins,std::to_string(recipe.gold),game.gold>=recipe.gold);
        bool charm=output.type==ItemType::Talisman;
        std::string why;bool ready=game.canCraft(recipe,why);
        if(ui.button(x+w-188,yy+33,151,43,charm?"Сплести":"Сварить",ready,why)){
            if(game.craft(recipe,why))audio->play(Sfx::Loot,.7f);else game.toast(why,1);
        }
    }
}

void App::drawSmith(){
    const float w=840,h=488,x=(ui.width-w)*.5f,y=(ui.height-h)*.5f;
    ui.panel(x,y,w,h,"Закалка у кузнеца Борна",ui::Emblem::Forge);
    if(ui.button(x+w-150,y+10,136,36,"Закрыть"))closeWindow();
    const auto& s=game.equip[0];const auto* item=rpg::item(s.id);
    ui.quad(x+24,y+68,w-48,125,{.091f,.103f,.106f,1});
    if(item)itemImage(ui,x+40,y+82,96,*item);
    ui.text(x+154,y+80,"Экипированное оружие",14,ui::muted);
    ui.heading(x+154,y+105,item?stackName(*item,s.upgrade):"Слот пуст",24,item?ui::rarity(int(item->rarity)):ui::text);
    ui.text(x+154,y+151,"Ступень "+std::to_string(s.upgrade)+" из "+std::to_string(rpg::maxUpgrade)+"   ·   +2 к урону за закалку",16,ui::text);
    if(s.upgrade<rpg::maxUpgrade){
        auto cost=rpg::upgradeCost(s.upgrade+1);
        sectionName(ui,x+28,y+211,w-56,"Материалы для ступени +"+std::to_string(s.upgrade+1));
        float ix=x+28;
        auto costLabel=[&](ui::Emblem art,const std::string& label,int held,int need,float width){
            ui.emblem(ix,y+261,40,art);
            ui.text(ix+51,y+259,label,15,ui::muted);
            ui.text(ix+51,y+280,std::to_string(held)+" / "+std::to_string(need),17,held>=need?ui::text:ui::red);
            ix+=width;
        };
        costLabel(ui::Emblem::Coins,"Золото",game.gold,cost.gold,185);
        costLabel(ui::Emblem::Ore,"Руда",game.count("ore"),cost.ore,185);
        if(cost.extraCount){
            const auto& extra=*rpg::item(cost.extra);
            costLabel(itemEmblem(extra),extra.name,game.count(cost.extra),cost.extraCount,240);
        }
    }else ui.heading(x+28,y+218,"Оружие закалено до предела",20,ui::gold);
    std::string why;bool ready=game.canUpgrade(why);
    if(!ready)ui.paragraph(x+28,y+341,w-56,why,15,ui::muted);
    if(ui.button(x+28,y+h-72,220,48,"Закалить оружие",ready,why)){
        if(game.upgradeWeapon(why))audio->play(Sfx::Hit);else game.toast(why,1);
    }
    if(ui.button(x+w-248,y+h-72,220,48,"Торговать")){shopNpc="smith";win=Window::Shop;}
}

// ------------------------------------------------------------------ shrine and travel
void App::drawShrine(){
    float x=ui.width*.5f-360,y=150,w=720,h=560;
    std::string id=game.lastShrine;std::string label="Святилище";
    for(const auto& e:game.entities)if(e.def.type==rpg::EntityType::Shrine&&id==e.def.id)label=e.def.label;
    ui.panel(x,y,w,h,label,ui::Emblem::Shrine);
    ui.emblem(x+32,y+66,58,ui::Emblem::Shrine);
    ui.paragraph(x+108,y+66,w-138,"Руны горят: это святилище запомнило вас. Если вы падёте, то очнётесь здесь, потеряв 10% золота. Сюжетный прогресс, святилища и уникальные трофеи сохраняются.",14,ui::muted);
    float by=y+166;std::string why;
    if(ui.button(x+30,by,w-60,48,"Отдохнуть — восстановить здоровье",true,"",false,17,ui::Emblem::Vial)){game.rest();syncActors(true);refreshVisuals(true);audio->play(Sfx::Shrine,.5f);}
    ui.text(x+40,by+53,"Возвращаются травы и обычные враги. Побеждённые боссы и открытые сундуки остаются прежними.",13,ui::muted);
    bool battle=game.game.inBattle();
    if(ui.button(x+30,by+80,w-60,48,"Сохранить игру",!battle,"Нельзя сохраняться в бою",false,17,ui::Emblem::Journal)){stack.push_back(win);win=Window::Save;}
    auto shrines=game.activeShrines();
    if(ui.button(x+30,by+140,w-60,48,"Переместиться к святилищу",shrines.size()>1,"Активируйте другие святилища, чтобы перемещаться между ними",false,17,ui::Emblem::Map)){stack.push_back(win);win=Window::Travel;}
    std::string dungeon=game.shrineDungeon(id);
    if(!dungeon.empty()){
        std::string zone=dungeon.substr(0,dungeon.find('@'));
        bool found=game.flags.count("found:"+zone)>0;
        if(ui.button(x+30,by+200,w-60,48,"Спуститься: "+rpg::zoneName(zone),found,"Сначала найдите вход в подземелье на поверхности",false,17,ui::Emblem::Key)){
            if(game.descend(id,why)){win=Window::None;}else game.toast(why,1);}
    }else ui.text(x+40,by+212,"У этого святилища нет входа в подземелье.",13,ui::dim);
    if(ui.button(x+30,by+300,w-60,46,"Закрыть"))win=Window::None;
}
void App::drawTravel(){
    float x=ui.width*.5f-360,y=150,w=720,h=520;
    ui.panel(x,y,w,h,"Быстрое перемещение",ui::Emblem::Map);
    if(ui.button(x+w-150,y+10,136,36,"Назад"))closeWindow();
    int i=0;std::string why;
    for(const auto& z:rpg::zones())for(const auto& e:z.entities)if(e.type==rpg::EntityType::Shrine){
        bool on=game.shrineActive(e.id);bool here=game.lastShrine==e.id&&game.zoneId==z.id;
        std::string reason=!on?"Святилище ещё не найдено и не активировано":here?"Вы уже здесь":"";
        if(ui.button(x+30,y+70+float(i)*70,w-60,56,std::string(e.label)+" · "+z.name,reason.empty(),reason,false,16,ui::Emblem::Shrine)){
            if(game.travel(e.id,why)){win=Window::None;stack.clear();audio->play(Sfx::Shrine,.5f);}else game.toast(why,1);}
        ++i;
    }
}

// ------------------------------------------------------------------ dialog, notes, rewards
void App::drawDialog(){
    const auto& d=game.dialog;
    float w=900,x=ui.width*.5f-w*.5f,y=ui.height-330,h=300;
    ui.panel(x,y,w,h,d.speaker,ui::Emblem::Scroll);
    ui.quad(x+30,y+72,2,h-164,bronze);
    if(dialogLine<int(d.lines.size())){
        ui.paragraph(x+53,y+70,w-96,d.lines[size_t(dialogLine)],20,ui::text,1.48f);
        ui.text(x+30,y+h-40,std::to_string(dialogLine+1)+" / "+std::to_string(d.lines.size()),13,ui::muted);
        bool advance=ui.button(x+w-220,y+h-62,190,44,dialogLine+1<int(d.lines.size())?"Далее (Пробел)":"Ответить")||key(GLFW_KEY_SPACE)||key(GLFW_KEY_ENTER);
        if(advance)++dialogLine;
    }else{
        float bx=x+30;
        for(const auto& o:d.options){
            float bw=ui.textWidth(o.label,16)+50;
            std::string why;bool enabled=true;
            if(o.action=="smith"&&!game.canUpgrade(why)&&!game.flags.count("smith_upgrades")){enabled=false;}
            if(ui.button(bx,y+h-80,bw,48,o.label,enabled,why)){
                if(o.action=="close")win=Window::None;else{win=Window::None;game.dialogAction(o.action);}
            }
            bx+=bw+16;
        }
        if(!d.lines.empty())ui.paragraph(x+53,y+70,w-96,d.lines.back(),18,ui::text);
        if(key(GLFW_KEY_ESCAPE))win=Window::None;
    }
}
void App::drawNote(){
    float w=760,x=ui.width*.5f-w*.5f,y=170,h=480;
    ui.panel(x,y,w,h,rpg::noteTitle(game.noteId),ui::Emblem::Scroll);
    ui.quad(x+37,y+78,w-68,h-172,{.25f,.22f,.17f,1});
    ui.quad(x+30,y+72,w-68,h-172,{.76f,.70f,.57f,1});
    ui.quad(x+50,y+89,2,h-212,{.54f,.40f,.28f,1});
    ui.paragraph(x+80,y+106,w-150,rpg::noteText(game.noteId),20,{.155f,.135f,.105f,1},1.55f);
    if(ui.button(x+w*.5f-100,y+h-68,200,46,"Закрыть"))closeWindow();
}
void App::drawReward(){
    const float cx=ui.width*.5f,w=1160,h=645,y=(ui.height-h)*.5f;
    ui.panel(cx-w*.5f,y,w,h,"Трофеи Паучьей Матки",ui::Emblem::Chest);
    ui.paragraph(cx-w*.5f+24,y+57,w-48,"Печать Паутины восстановлена. Выберите один трофей из логова.",18,ui::text);
    const float cardW=360,gap=14;
    for(size_t i=0;i<game.rewardChoices.size();++i){
        const float x=cx-(3*cardW+2*gap)*.5f+float(i)*(cardW+gap);
        rpg::Stack itemStack{game.rewardChoices[i],1,0};
        ui.quad(x,y+102,cardW,h-126,{.081f,.10f,.106f,1});
        ui.quad(x,y+102,cardW,2,bronze);
        itemDetails(x+17,y+120,cardW-34,itemStack,true);
        bool fits=rpg::canEquip(*rpg::item(itemStack.id),game.heroClass);
        if(ui.button(x+17,y+h-88,cardW-34,46,fits?"Взять трофей":"Взять для другого класса")){
            game.chooseReward(int(i));win=Window::None;audio->play(Sfx::LevelUp);
        }
    }
}

// ------------------------------------------------------------------ pause, settings, help, slots
void App::drawPause(){
    float cx=ui.width*.5f,y=(ui.height-400)*.5f;
    ui.panel(cx-230,y,460,400,"Пауза",ui::Emblem::Hourglass);
    if(ui.button(cx-200,y+70,400,50,"Продолжить",true,"",false,17,ui::Emblem::Sword))win=Window::None;
    if(ui.button(cx-200,y+135,400,50,"Настройки",true,"",false,17,ui::Emblem::Gear)){stack.push_back(win);win=Window::Settings;}
    if(ui.button(cx-200,y+200,400,50,"Управление",true,"",false,17,ui::Emblem::Scroll)){stack.push_back(win);win=Window::Help;}
    if(ui.button(cx-200,y+265,400,50,"Выйти в главное меню",true,"",false,17,ui::Emblem::Map)){win=Window::ConfirmMenu;}
    ui.textCentered(cx,y+347,"Сохраниться можно у святилища.",15,ui::muted);
    ui.textCentered(cx,y+368,"При переходах игра сохраняется автоматически.",14,ui::muted);
}
void App::drawSettings(){
    float x=ui.width*.5f-420,y=110,w=840,h=690;
    ui.panel(x,y,w,h,"Настройки",ui::Emblem::Gear);
    if(ui.button(x+w-150,y+10,136,36,"Назад")){if(!(options.smoke||options.benchmark||!options.capture.empty()))settings.save(root/"settings.ini");if(screen==Screen::Game)closeWindow();else win=Window::None;}
    static const std::pair<int,int> res[]={{1280,720},{1440,900},{1600,900},{1920,1080},{2560,1440}};
    float yy=y+64;auto row=[&](const std::string& label){ui.quad(x+30,yy+44,w-60,1,{.21f,.22f,.20f,.8f});ui.text(x+34,yy+9,label,17,ui::text);};
    auto changed=[&]{if(!(options.smoke||options.benchmark||!options.capture.empty()))settings.save(root/"settings.ini");};
    row("Разрешение окна");
    int current=0;for(int i=0;i<5;++i)if(res[i].first==settings.width&&res[i].second==settings.height)current=i;
    if(ui.button(x+400,yy,60,38,"<",!settings.fullscreen,"В полноэкранном режиме используется разрешение монитора")){current=(current+4)%5;settings.width=res[current].first;settings.height=res[current].second;applySettings(true);changed();}
    ui.textCentered(x+560,yy+9,std::to_string(settings.width)+" × "+std::to_string(settings.height),16,ui::gold);
    if(ui.button(x+660,yy,60,38,">",!settings.fullscreen,"В полноэкранном режиме используется разрешение монитора")){current=(current+1)%5;settings.width=res[current].first;settings.height=res[current].second;applySettings(true);changed();}
    yy+=52;row("Режим экрана");
    if(ui.button(x+400,yy,320,38,settings.fullscreen?"Полный экран":"Оконный")){settings.fullscreen=!settings.fullscreen;applySettings(true);changed();}
    yy+=52;row("Вертикальная синхронизация");
    if(ui.button(x+400,yy,320,38,settings.vsync?"Вкл":"Выкл")){settings.vsync=!settings.vsync;applySettings(false);changed();}
    yy+=52;row("Качество теней");
    const char* q[]={"Выкл","Низкое (1024)","Высокое (2048)"};
    if(ui.button(x+400,yy,320,38,q[settings.shadowQuality])){settings.shadowQuality=(settings.shadowQuality+1)%3;changed();}
    yy+=52;row("Качество эффектов");
    const char* e[]={"Низкое","Среднее","Высокое"};
    if(ui.button(x+400,yy,320,38,e[settings.effects])){settings.effects=(settings.effects+1)%3;changed();}
    yy+=52;row("Мягкие тени  ·  T");
    if(ui.button(x+400,yy,320,38,settings.technique?"Включена":"Выключена",settings.shadowQuality>0,"Сначала включите тени в «Качестве теней»")){settings.technique=!settings.technique;changed();}
    yy+=52;row("Проекция камеры  ·  O");
    if(ui.button(x+400,yy,320,38,settings.perspective?"Перспектива":"Ортография (изометрия)")){settings.perspective=!settings.perspective;changed();}
    auto slider=[&](const std::string& label,float& v,float lo,float hi,float step,bool audioSetting){
        yy+=52;row(label);
        if(ui.button(x+400,yy,60,38,"-",v>lo+1e-4f,"Минимум")){v=std::max(lo,v-step);if(audioSetting)applySettings(false);changed();}
        ui.bar(x+470,yy+27,180,5,(v-lo)/(hi-lo),ui::mint);ui.textCentered(x+560,yy+2,std::to_string(int(std::round(v*100)))+"%",14,ui::text);
        if(ui.button(x+660,yy,60,38,"+",v<hi-1e-4f,"Максимум")){v=std::min(hi,v+step);if(audioSetting)applySettings(false);changed();}
    };
    slider("Общая громкость",settings.master,0,1,.1f,true);
    slider("Музыка",settings.music,0,1,.1f,true);
    slider("Эффекты",settings.sfx,0,1,.1f,true);
    slider("Чувствительность камеры",settings.cameraSensitivity,.2f,3.f,.2f,false);
    if(!audio->ok())ui.text(x+30,y+h-34,"Звуковое устройство недоступно — игра работает без звука.",13,ui::red);
}
void App::drawHelp(bool credits){
    float x=ui.width*.5f-600,y=60,w=1200,h=ui.height-120;
    ui.panel(x,y,w,h,credits?"Авторы и управление":"Управление",ui::Emblem::Scroll);
    if(ui.button(x+w-150,y+10,136,36,"Назад")){if(screen==Screen::Game)closeWindow();else win=Window::None;}
    const char* lines[][2]={
        {"ПКМ (или ЛКМ) по земле","герой идёт сам, обходя препятствия; удержание ПКМ — вести за курсором"},
        {"ПКМ по врагу","подойти и атаковать (автоатака, пока враг жив)"},
        {"ПКМ по объекту / NPC","подойти и взаимодействовать · F — взаимодействовать рядом"},
        {"Q W E R","навыки: применяются к врагу или точке под курсором (R — с 6 уровня)"},
        {"H или 1 · S · Пробел","зелье лечения · остановиться · центрировать камеру"},
        {"Y","эффект миниатюры (tilt-shift) вкл/выкл"},
        {"I / C / J / M","сумка · герой и развитие · журнал · карта"},
        {"Колесо, стрелки, СКМ или Alt+ЛКМ","приближение и вращение камеры"},
        {"F4","свободная камера: WASD, Q/E, ПКМ — обзор"},{"O","перспектива / ортография"},
        {"T","вкл/выкл основную технику: мягкие тени (PCSS + кубические карты)"},
        {"P","пауза анимации (огонь, вода, облака, листва)"},{"[ ]   ; '","повернуть солнце · изменить его высоту"},
        {"Tab · F12 · F1 · Esc","скрыть HUD · снимок кадра · справка · пауза/закрыть окно"},
    };
    float yy=y+60;
    for(const auto& l:lines){
        ui.quad(x+26,yy,317,25,{.107f,.116f,.113f,1});
        ui.text(x+37,yy+1,l[0],15,ui::gold);
        ui.paragraph(x+366,yy+1,w-400,l[1],15,ui::text);yy+=30;
    }
    yy+=10;
    ui.paragraph(x+30,yy,w-60,"Бой идёт в реальном времени. Герой атакует сам, навыки тратят ману и перезаряжаются. Перед ударом босса на земле появляется контур опасной зоны. Успейте выйти за него. После тяжёлого удара босс уязвим (+50% урона). Враги, которых увели далеко от логова, возвращаются и лечатся.",14,ui::muted);
    if(credits){
        yy+=96;
        ui.heading(x+30,yy,"Авторы",19,ui::gold);yy+=28;
        ui.paragraph(x+30,yy,w-60,"Команда: "+team+". Тема: "+topic+".",15,ui::text);yy+=28;
        ui.paragraph(x+30,yy,w-60,"Код, шейдеры, модели (процедурная геометрия), звуки и музыка (синтез в коде) созданы для проекта. Библиотеки: GLFW 3.5.1 (zlib), GLAD 2.0.8, GLM 1.0.3 (MIT), stb_image 2.30 (MIT/PD), miniaudio 0.11.21 (MIT-0/PD). Для интерфейса подготовлены растровые шрифтовые атласы с кириллицей. При разработке использовалась существенная помощь ИИ (Claude).",13,ui::muted);
    }
}
void App::drawSlots(bool saving){
    float x=ui.width*.5f-480,y=130,w=960,h=600;
    ui.panel(x,y,w,h,saving?"Сохранить игру":"Загрузить игру",ui::Emblem::Journal);
    if(ui.button(x+w-150,y+10,136,36,"Назад")){if(screen==Screen::Game)closeWindow();else win=Window::None;}
    for(int s=saving?1:0;s<4;++s){
        auto info=rpg::Campaign::inspect(slotPath(s));float yy=y+70+float(s-(saving?1:0))*120;
        ui.quad(x+24,yy,w-48,108,{.06f,.08f,.09f,1});
        ui.emblem(x+40,yy+18,64,s==0?ui::Emblem::Hourglass:ui::Emblem::Journal,info.valid?1.f:.42f);
        ui.heading(x+123,yy+10,s==0?"Автосохранение":"Слот "+std::to_string(s),18,ui::gold);
        if(!info.exists)ui.text(x+123,yy+44,"Свободная ячейка",15,ui::muted);
        else if(!info.valid)ui.paragraph(x+123,yy+44,390,"Файл повреждён: "+info.error,14,ui::red);
        else{ui.text(x+123,yy+44,info.className+" · уровень "+std::to_string(info.level)+" · "+info.zoneName,15,ui::text);ui.text(x+123,yy+72,info.time,13,ui::muted);}
        if(saving){
            if(slotConfirm==s){
                ui.text(x+w-470,yy+14,"Перезаписать?",15,ui::red);
                if(ui.button(x+w-470,yy+50,150,42,"Да")){std::string err;if(game.save(slotPath(s),err)){game.toast("Сохранено в слот "+std::to_string(s),0);slotConfirm=-1;}else game.toast(err,1);}
                if(ui.button(x+w-310,yy+50,150,42,"Нет"))slotConfirm=-1;
            }else if(ui.button(x+w-250,yy+30,200,48,"Сохранить",!game.game.inBattle(),"Нельзя сохраняться в бою")){
                if(info.exists)slotConfirm=s;else{std::string err;if(game.save(slotPath(s),err))game.toast("Сохранено в слот "+std::to_string(s),0);else game.toast(err,1);}
            }
        }else{
            std::string reason=!info.exists?"Слот пуст":!info.valid?"Файл повреждён и не может быть загружен":"";
            if(ui.button(x+w-250,yy+30,200,48,"Загрузить",reason.empty(),reason)){loadSlot(slotPath(s));}
        }
    }
    if(!status.empty()&&!saving)ui.text(x+30,y+h-34,status,14,ui::red);
    else if(!saving&&screen==Screen::Game)ui.text(x+30,y+h-34,"Загрузка заменит текущий прогресс.",13,ui::muted);
}
void App::drawDefeat(){
    float cx=ui.width*.5f,y=200;
    ui.quad(0,0,ui.width,ui.height,{.15f,0,0,.35f});
    ui.panel(cx-380,y,760,420,"Вы пали",ui::Emblem::Bone);
    ui.paragraph(cx-350,y+60,700,"Восстав у последнего святилища, вы сохраните сюжетный прогресс, открытые святилища и уникальные награды. Цена — 10% золота ("+std::to_string(game.gold/10)+"). Обычные враги вернутся на свои места, боссы — восстановят силы.",16);
    if(ui.button(cx-340,y+200,680,50,"Восстать у святилища")){game.respawnAtShrine();win=Window::None;syncActors(true);refreshVisuals(true);}
    bool any=false;for(int s=0;s<4;++s)any|=rpg::Campaign::inspect(slotPath(s)).valid;
    if(ui.button(cx-340,y+265,680,50,"Загрузить сохранение",any,"Нет сохранений")){stack.push_back(win);win=Window::Load;}
    if(ui.button(cx-340,y+330,680,50,"Главное меню")){game.flags.erase("defeat_pending");screen=Screen::Menu;win=Window::None;syncActors(true);refreshVisuals(true);}
}
void App::drawEpilogue(bool ending){
    float cx=ui.width*.5f,y=140;
    ui.quad(0,0,ui.width,ui.height,{0,0,0,.55f});
    ui.panel(cx-460,y,920,560,ending?"Конец истории":"Хранитель Костей повержен",ending?ui::Emblem::Key:ui::Emblem::Shrine);
    std::string text=ending?
        "Серый Брод празднует. Над площадью висят фонари, у кузницы снова звенит молот, а шахтёры спускаются в штольни без страха.\n\nТри печати целы, скверна ушла из долины, и даже над замком Вальден светлеет небо. Староста вручил вам ключ от дома у брода — теперь это и ваш дом.\n\nСпасибо за игру! Вы можете продолжить исследование: в долине остались нетронутые тайники.":
        "Кости рассыпаются в пыль, и подземный гул стихает. Пурпурная скверна отступает от стен замка, словно отлив.\n\nПо горным тропам возвращаются беглецы, в окнах Серого Брода снова зажигается свет.\n\nВернитесь в деревню и поговорите со старостой — история ещё не окончена.";
    ui.quad(cx-420,y+87,2,285,bronze);
    ui.paragraph(cx-384,y+81,760,text,19,ui::text,1.55f);
    if(ending){ui.text(cx-420,y+430,"Команда: "+team,14,ui::muted);ui.text(cx-420,y+454,"Время в игре: "+std::to_string(int(game.playTime/60))+" мин",14,ui::muted);}
    if(ui.button(cx-150,y+490,300,50,ending?"Продолжить исследование":"Продолжить"))win=Window::None;
}
