// All interface windows. Every visible button performs an action or explains why it cannot.
#include "app.hpp"
#include <algorithm>
#include <cmath>

using world::Cell;
using combat::Phase;
using rpg::ItemType;

namespace {
std::string plus(int v){return (v>=0?"+":"")+std::to_string(v);}
const char* slotName(int s){static const char* n[]={"Оружие","Броня","Талисман"};return n[s];}
std::string classLine(combat::Class c){
    auto h=combat::Game::makeHero(c);
    return std::to_string(h.maxHp)+" HP   броня "+std::to_string(h.armor)+"   мана "+std::to_string(int(h.maxMana));
}
}

void App::drawTechniqueBadge(){
    float w=360,x=ui.width-w-14,y=12;
    ui.quad(x,y,w,96,{.02f,.03f,.04f,.82f});ui.quad(x,y,3,96,settings.technique?ui::mint:ui::red);
    ui.text(x+14,y+8,std::string("T  Мягкие тени PCSS: ")+(settings.technique?"ВКЛ":"ВЫКЛ"),16,settings.technique?ui::mint:ui::red);
    std::string line=std::to_string(int(fps))+" FPS   "+(settings.perspective||freeCamera?"перспектива":"ортография")+(freeCamera?" (свободная, F4)":"");
    ui.text(x+14,y+32,line,13,ui::muted);
    int deg=int(std::fmod(lightAngle*57.2958f+3600.f,360.f));
    ui.text(x+14,y+52,std::string("[ ] свет ")+std::to_string(deg)+"°   P анимация: "+(animPaused?"ПАУЗА":"идёт"),13,animPaused?ui::gold:ui::muted);
    ui.text(x+14,y+72,team+" · "+topic,12,ui::dim);
}

// ------------------------------------------------------------------ main menu
void App::drawMenu(){
    float cx=ui.width*.5f;
    ui.quad(0,0,ui.width,ui.height,{.0f,.0f,.0f,.12f});
    ui.quad(cx-300,90,600,120,{.02f,.03f,.04f,.55f});
    ui.textCentered(cx,104,"THE OSSUARY",50,ui::gold);
    ui.textCentered(cx,170,"Эшвальд: Хранитель Костей",22,ui::text);
    if(win!=Window::None)return;
    // newest valid save for "Continue"
    int newest=-1;long long stamp=0;
    for(int s=0;s<4;++s){auto info=rpg::Campaign::inspect(slotPath(s));if(info.valid&&info.stamp>stamp){stamp=info.stamp;newest=s;}}
    float bx=cx-170,by=270,bw=340,bh=50,gap=60;
    ui.panel(bx-24,by-24,bw+48,gap*6+40);
    if(ui.button(bx,by,bw,bh,"Продолжить",newest>=0,"Сохранений пока нет. Начните новую игру."))loadSlot(slotPath(newest));
    if(ui.button(bx,by+gap,bw,bh,"Новая игра"))screen=Screen::ClassSelect;
    if(ui.button(bx,by+gap*2,bw,bh,"Загрузить игру")){setWindow(Window::Load);}
    if(ui.button(bx,by+gap*3,bw,bh,"Настройки")){setWindow(Window::Settings);}
    if(ui.button(bx,by+gap*4,bw,bh,"Авторы и управление")){setWindow(Window::Credits);}
    if(ui.button(bx,by+gap*5,bw,bh,"Выход"))glfwSetWindowShouldClose(window,GLFW_TRUE);
    if(!status.empty())ui.textCentered(cx,by+gap*6+30,status,15,ui::red);
    ui.textCentered(cx,ui.height-40,"Колесо — приближение · стрелки — вращение камеры · F1 — справка",14,ui::muted);
}
void App::drawClassSelect(){
    float cx=ui.width*.5f,cy=ui.height*.5f;
    ui.quad(0,0,ui.width,ui.height,{.01f,.02f,.03f,.62f});
    ui.textCentered(cx,cy-300,"Выберите героя",34,ui::gold);
    ui.textCentered(cx,cy-250,"Каждый класс способен пройти игру в одиночку",17,ui::muted);
    if(win==Window::ConfirmNew){drawConfirm(true);return;}
    const combat::Class classes[]={combat::Class::Warrior,combat::Class::Rogue,combat::Class::Mage};
    const char* weapon[]={"Меч · Удар щитом, Вихрь, Бастион","Кинжалы · Яд, Шаг тени, Веер кинжалов","Посох · Огненная стрела, Ледяная волна, Скачок"};
    const char* text[]={"Ближний бой, много здоровья и брони, оглушения.","Быстрые атаки, яд и рывки за спину врагу.","Дальние атаки, поджог, контроль толпы и метеор."};
    for(int i=0;i<3;++i){
        float x=cx-540+float(i)*365,y=cy-200;
        ui.panel(x,y,340,400);
        ui.text(x+20,y+18,combat::className(classes[i]),28,ui::gold);
        ui.paragraph(x+20,y+70,300,classLine(classes[i]),14,ui::text);
        ui.paragraph(x+20,y+120,300,weapon[i],15,ui::mint);
        ui.paragraph(x+20,y+170,300,text[i],15,ui::muted);
        ui.text(x+20,y+240,"Ветка развития:",14,ui::gold);
        for(int t=0;t<3;++t)ui.text(x+28,y+266+float(t)*24,std::string("• ")+combat::talentName(classes[i],t),14,ui::text);
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
    ui.textCentered(cx,20,game.zone().name,18,ui::gold);
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
    {auto p=project(heroPos+glm::vec3(0,2.9f,0));ui.quad(p.x-41,p.y-1,82,8,{0,0,0,.75f});ui.bar(p.x-40,p.y,80,6,float(h.hp)/float(h.maxHp),ui::mint);}
    for(const auto& f:floaters){auto p=project(f.position+glm::vec3(0,f.time*1.2f,0));glm::vec4 c=f.color;c.a=std::clamp(1.4f-f.time,0.f,1.f);
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
    float cx=ui.width*.5f,pw=860,px=cx-pw*.5f,py=ui.height-150;
    ui.panel(px,py,pw,140);
    // portrait
    ui.quad(px+12,py+12,116,116,{.07f,.10f,.11f,1});
    glm::vec4 classColor=game.heroClass==combat::Class::Warrior?glm::vec4(.85f,.35f,.25f,1):game.heroClass==combat::Class::Rogue?glm::vec4(.35f,.8f,.45f,1):glm::vec4(.45f,.55f,1.f,1);
    ui.quad(px+12,py+12,116,4,classColor);
    ui.textCentered(px+70,py+28,combat::className(game.heroClass),17,classColor);
    ui.textCentered(px+70,py+58,std::to_string(h.level),34,ui::gold);
    ui.bar(px+22,py+108,96,6,float(h.xp)/float(h.level*100),ui::gold);
    if(h.skillPoints>0){ui.quad(px+100,py+16,24,24,ui::gold);ui.textCentered(px+112,py+18,"+",18,{0,0,0,1});
        if(ui.inside(px+100,py+16,24,24)){ui.tooltip="Есть очки развития — откройте окно героя (C)";ui.overUi=true;if(ui.pressed){win=Window::Character;ui.pressed=false;}}}
    // abilities
    float ax=px+146,ay=py+12,asz=78,agap=8;
    const char* keysQ[]={"Q","W","E","R"};
    for(int s=0;s<4;++s){
        const auto& ab=combat::ability(game.heroClass,s);float x=ax+float(s)*(asz+agap);
        bool unlocked=g.abilityUnlocked(s);bool ready=unlocked&&h.cd[s]<=0&&h.mana>=float(ab.mana);
        ui.quad(x,ay,asz,asz,unlocked?glm::vec4(.12f,.16f,.2f,1):glm::vec4(.05f,.06f,.07f,1));
        ui.quad(x,ay,asz,3,ready?classColor:ui::dim);
        std::string first=ab.name;auto sp=first.find(' ');if(sp!=std::string::npos)first=first.substr(0,sp);
        ui.textCentered(x+asz*.5f,ay+22,ui::clip(first,9),13,unlocked?ui::text:ui::dim);
        ui.text(x+5,ay+4,keysQ[s],14,ui::gold);
        ui.text(x+5,ay+asz-20,std::to_string(ab.mana),12,h.mana>=float(ab.mana)?glm::vec4(.45f,.65f,1.f,1):ui::red);
        if(h.cd[s]>0){float k=h.cd[s]/ab.cooldown;ui.quad(x,ay+asz*(1-k),asz,asz*k,{0,0,0,.6f});
            char buf[16];std::snprintf(buf,sizeof(buf),"%.0f",double(std::ceil(h.cd[s])));ui.textCentered(x+asz*.5f,ay+asz*.5f-6,buf,22,ui::text);}
        if(!unlocked){ui.quad(x,ay,asz,asz,{0,0,0,.5f});ui.textCentered(x+asz*.5f,ay+asz-22,"ур. "+std::to_string(ab.level),12,ui::muted);}
        if(ui.inside(x,ay,asz,asz)){ui.overUi=true;hoverAbility=s;
            char cdText[16];std::snprintf(cdText,sizeof(cdText),"%.0f",double(ab.cooldown));
            ui.tooltip=std::string(ab.name)+" ["+keysQ[s]+"] — "+ab.text+"  Мана "+std::to_string(ab.mana)+", перезарядка "+cdText+" с."+(unlocked?"":"  Откроется на уровне "+std::to_string(ab.level)+".");
            if(ui.pressed){ui.pressed=false;castSlot(s);}}
    }
    // potions
    float ix=ax+4*(asz+agap)+6;int potions=game.count("potion")+game.count("greater_potion");
    ui.quad(ix,ay,asz,asz,{.12f,.10f,.12f,1});
    ui.textCentered(ix+asz*.5f,ay+18,"Зелье",13,ui::text);ui.text(ix+5,ay+4,"H",14,ui::gold);
    ui.textCentered(ix+asz*.5f,ay+44,"×"+std::to_string(potions),18,potions?ui::mint:ui::red);
    if(g.itemCooldown>0){float k=g.itemCooldown/1.5f;ui.quad(ix,ay+asz*(1-k),asz,asz*k,{0,0,0,.6f});}
    if(ui.inside(ix,ay,asz,asz)){ui.overUi=true;ui.tooltip="Выпить зелье лечения (H). Перезарядка 1,5 с.";if(ui.pressed){ui.pressed=false;drinkPotion();}}
    // health and mana
    float bx=px+146,bw=pw-146-14;
    ui.quad(bx,py+98,bw,16,{.08f,.1f,.08f,1});ui.quad(bx,py+98,bw*std::clamp(float(h.hp)/float(h.maxHp),0.f,1.f),16,h.hp*3<h.maxHp?ui::red:glm::vec4(.25f,.72f,.3f,1));
    ui.textCentered(bx+bw*.5f,py+97,std::to_string(h.hp)+" / "+std::to_string(h.maxHp),13,ui::text);
    ui.quad(bx,py+117,bw,13,{.06f,.07f,.12f,1});ui.quad(bx,py+117,bw*std::clamp(h.mana/std::max(1.f,h.maxMana),0.f,1.f),13,{.25f,.45f,.95f,1});
    ui.textCentered(bx+bw*.5f,py+115,std::to_string(int(h.mana))+" / "+std::to_string(int(h.maxMana)),12,ui::text);
    // gold, status and window buttons
    float rx=ix+asz+14;
    ui.text(rx,ay+2,"Золото "+std::to_string(game.gold),14,ui::gold);
    std::string st=g.status(h);ui.paragraph(rx,ay+24,px+pw-rx-10,st=="Нет эффектов"?"":st,12,ui::red);
    const char* names[]={"I","C","J","M"};const char* tips[]={"Сумка (I)","Герой и развитие (C)","Журнал заданий (J)","Карта (M)"};
    Window targets[]={Window::Inventory,Window::Character,Window::Journal,Window::Map};
    for(int i=0;i<4;++i){float bxx=rx+float(i)*34;if(ui.button(bxx,ay+52,30,28,names[i],true,"",win==targets[i],14))win=win==targets[i]?Window::None:targets[i];
        if(ui.inside(bxx,ay+52,30,28))ui.tooltip=tips[i];}
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

// ------------------------------------------------------------------ window dispatcher
void App::drawWindows(){
    if(win!=Window::None&&screen==Screen::Game&&win!=Window::Inventory&&win!=Window::Character&&win!=Window::Journal&&win!=Window::Map)
        ui.quad(0,0,ui.width,ui.height,{0,0,0,.35f});
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
}

// ------------------------------------------------------------------ inventory and trade
void App::itemDetails(float x,float y,float w,const rpg::Stack& s,bool compare){
    auto d=rpg::item(s.id);if(!d)return;
    ui.text(x,y,std::string(d->name)+(s.upgrade?" +"+std::to_string(s.upgrade):""),19,ui::rarity(int(d->rarity)));
    ui.text(x,y+28,std::string(rpg::rarityName(d->rarity))+" · "+rpg::typeName(d->type)+(s.count>1?" · ×"+std::to_string(s.count):""),14,ui::muted);
    float yy=y+54;
    auto stat=[&](const std::string& t){ui.text(x,yy,t,15,ui::text);yy+=22;};
    if(d->type==ItemType::Weapon){auto ws=combat::weaponStats(d->weapon);
        char buf[64];std::snprintf(buf,sizeof(buf)," · дальность %.1f м · удар раз в %.2f с",double(ws.range+float(d->range)*.8f),double(ws.interval));
        stat(std::string("Тип: ")+ws.name+" · урон "+std::to_string(ws.damage+d->damage+s.upgrade*2));stat(buf+3);}
    if(d->armor)stat("Броня "+plus(d->armor));
    if(d->hp)stat("Здоровье "+plus(d->hp));
    if(d->initiative)stat("Инициатива "+plus(d->initiative));
    if(d->type!=ItemType::Weapon&&d->damage)stat("Урон "+plus(d->damage));
    if(d->heal)stat("Лечение "+std::to_string(d->heal));
    if(d->type==ItemType::Weapon||d->type==ItemType::Armor||d->type==ItemType::Talisman){
        std::string who;const char* names[]={"воин","разбойник","маг"};for(int c=0;c<3;++c)if(d->classMask&(1<<c))who+=(who.empty()?"":", ")+std::string(names[c]);
        stat("Для: "+who);
        if(!rpg::canEquip(*d,game.heroClass))ui.text(x,yy,"Не подходит вашему классу",14,ui::red),yy+=22;
    }
    yy+=ui.paragraph(x,yy+4,w,d->desc,14,ui::muted)+8;
    if(compare&&(d->type==ItemType::Weapon||d->type==ItemType::Armor||d->type==ItemType::Talisman)){
        int slot=d->type==ItemType::Weapon?0:d->type==ItemType::Armor?1:2;auto cur=rpg::item(game.equip[slot].id);
        ui.text(x,yy,"Сравнение с надетым:",14,ui::gold);yy+=22;
        if(!cur)ui.text(x,yy,"слот пуст",14,ui::muted);
        else{
            int dd=(d->damage+s.upgrade*2+(d->type==ItemType::Weapon?combat::weaponStats(d->weapon).damage:0))-(cur->damage+game.equip[slot].upgrade*2+(cur->type==ItemType::Weapon?combat::weaponStats(cur->weapon).damage:0));
            std::string line="урон "+plus(dd)+"  броня "+plus(d->armor-cur->armor)+"  HP "+plus(d->hp-cur->hp)+"  иниц. "+plus(d->initiative-cur->initiative);
            ui.text(x,yy,line,14,dd>0||d->armor>cur->armor?ui::mint:ui::text);
        }
    }
}
void App::drawInventory(bool shop){
    float x=40,y=110,w=ui.width-80,h=ui.height-200;
    ui.panel(x,y,w,h,shop?(shopNpc=="smith"?"Кузница Борна — торговля":"Лавка Миры — торговля"):"Сумка героя");
    if(ui.button(x+w-150,y+10,136,36,"Закрыть"))closeWindow();
    ui.text(x+w-420,y+18,"Золото: "+std::to_string(game.gold),18,ui::gold);
    // equipment
    float ex=x+20,ey=y+64;
    ui.text(ex,ey,"Снаряжение",16,ui::gold);
    for(int s=0;s<3;++s){
        auto d=rpg::item(game.equip[s].id);float yy=ey+30+float(s)*64;
        ui.quad(ex,yy,320,56,{.07f,.09f,.1f,1});ui.text(ex+10,yy+4,slotName(s),12,ui::muted);
        if(d){ui.text(ex+10,yy+24,std::string(d->name)+(game.equip[s].upgrade?" +"+std::to_string(game.equip[s].upgrade):""),15,ui::rarity(int(d->rarity)));
            std::string why;bool can=s!=0&&!game.game.inBattle();
            if(ui.button(ex+236,yy+14,76,30,"Снять",can,s==0?"Нельзя остаться без оружия — наденьте другое":"Снимать снаряжение можно вне боя",false,13)){if(!game.unequip(s,why))game.toast(why,1);}}
        else ui.text(ex+10,yy+24,"— пусто —",15,ui::dim);
        if(ui.inside(ex,yy,230,56)&&d){ui.tooltip=std::string(d->name)+": "+d->desc;}
    }
    // item list
    float lx=x+360,ly=y+64,lw=420;
    ui.text(lx,ly,"Предметы ("+std::to_string(game.inventory.size())+")",16,ui::gold);
    std::vector<int> order(game.inventory.size());for(size_t i=0;i<order.size();++i)order[i]=int(i);
    std::stable_sort(order.begin(),order.end(),[&](int a,int b){auto da=rpg::item(game.inventory[size_t(a)].id),db=rpg::item(game.inventory[size_t(b)].id);return int(da->type)<int(db->type);});
    int rows=int((h-120)/30);
    for(int k=0;k<int(order.size())&&k<rows;++k){int i=order[size_t(k)];const auto& s=game.inventory[size_t(i)];auto d=rpg::item(s.id);
        float yy=ly+30+float(k)*30;bool sel=selected==i;
        if(ui.inside(lx,yy,lw,28)){ui.overUi=true;if(ui.pressed){selected=i;ui.pressed=false;ui.clicks++;}}
        ui.quad(lx,yy,lw,28,sel?glm::vec4(.15f,.22f,.23f,1):glm::vec4(.06f,.08f,.09f,1));
        ui.text(lx+10,yy+4,std::string(d->name)+(s.upgrade?" +"+std::to_string(s.upgrade):"")+(s.count>1?"  ×"+std::to_string(s.count):""),15,ui::rarity(int(d->rarity)));
        ui.text(lx+lw-120,yy+6,rpg::typeName(d->type),12,ui::muted);
    }
    // details and actions
    float dx=lx+lw+24,dw=shop?300:x+w-dx-20;
    if(selected>=0&&selected<int(game.inventory.size())){
        auto s=game.inventory[size_t(selected)];auto d=rpg::item(s.id);
        itemDetails(dx,ly,dw,s,true);
        float by=y+h-70;std::string why;
        if(d->type==ItemType::Consumable){
            bool ok=game.game.itemCooldown<=0;
            if(ui.button(dx,by,150,42,"Использовать",ok,"Зелья перезаряжаются")){if(!game.useItem(selected,why))game.toast(why,1);selected=-1;}
        }else if(d->type==ItemType::Weapon||d->type==ItemType::Armor||d->type==ItemType::Talisman){
            bool ok=rpg::canEquip(*d,game.heroClass);
            if(ui.button(dx,by,150,42,"Надеть",ok,"Не подходит вашему классу")){if(!game.equipItem(selected,why))game.toast(why,1);selected=-1;}
        }
        if(shop&&selected>=0){
            bool sellable=d->type!=ItemType::Quest&&d->rarity!=rpg::Rarity::Unique;
            std::string label="Продать +"+std::to_string(rpg::sellPrice(*d,s.upgrade));
            if(ui.button(dx+160,by,170,42,label,sellable,d->type==ItemType::Quest?"Сюжетный предмет нельзя продать":"Уникальные вещи продавать нельзя")){if(game.sell(selected,why))audio->play(Sfx::Loot,.6f);else game.toast(why,1);selected=-1;}
        }
    }else ui.paragraph(dx,ly+30,std::max(200.f,dw),"Выберите предмет, чтобы увидеть описание, характеристики и сравнение с надетым.",15,ui::muted);
    // shop stock
    if(shop){
        float sx=x+w-360;ui.quad(sx-12,y+60,1,h-80,{.25f,.28f,.29f,1});
        ui.text(sx,ly,"Товары",16,ui::gold);
        auto stock=game.shopStock(shopNpc);
        for(size_t k=0;k<stock.size();++k){auto d=rpg::item(stock[k].first);float yy=ly+30+float(k)*58;
            ui.quad(sx,yy,330,52,{.06f,.08f,.09f,1});
            ui.text(sx+10,yy+4,d->name,15,ui::rarity(int(d->rarity)));ui.text(sx+10,yy+28,std::to_string(stock[k].second)+" золота",13,ui::gold);
            if(ui.inside(sx,yy,220,52)){std::string tip=std::string(d->name)+": "+d->desc;if(!rpg::canEquip(*d,game.heroClass)&&d->type!=ItemType::Consumable)tip+=" (не для вашего класса)";ui.tooltip=tip;}
            std::string why;bool afford=game.gold>=stock[k].second;
            if(ui.button(sx+238,yy+10,84,32,"Купить",afford,"Не хватает золота",false,13)){if(game.buy(shopNpc,stock[k].first,why))audio->play(Sfx::Loot,.7f);else game.toast(why,1);}
        }
    }
}

// ------------------------------------------------------------------ character and talents
void App::drawCharacter(){
    auto& h=game.hero();auto& g=game.game;
    float x=ui.width*.5f-560,y=110,w=1120,hgt=640;
    ui.panel(x,y,w,hgt,"Герой: "+std::string(combat::className(game.heroClass)));
    if(ui.button(x+w-150,y+10,136,36,"Закрыть"))closeWindow();
    auto ws=combat::weaponStats(h.weapon);
    std::vector<std::string> lines={
        "Уровень "+std::to_string(h.level)+"   опыт "+std::to_string(h.xp)+" / "+std::to_string(h.level*100),
        "Здоровье "+std::to_string(h.hp)+" / "+std::to_string(h.maxHp),
        "Сила "+std::to_string(h.power)+"   (прибавляется к урону)",
        "Броня "+std::to_string(h.armor+h.armorBonus),
        "Мана "+std::to_string(int(h.mana))+" / "+std::to_string(int(h.maxMana))+"   восстановление "+std::to_string(int(h.manaRegen*10)/10)+"/с",
        std::string("Оружие: ")+ws.name+"   урон "+std::to_string(ws.damage+h.damageBonus+h.power)+"   дальность "+std::to_string(int(g.attackRange(h)*10)/10)+" м",
        "Атак в секунду: "+std::to_string(int(100.f/g.attackInterval(h))/100)+"."+std::to_string(int(100.f/g.attackInterval(h))%100),
        std::string("Навыки: ")+combat::ability(game.heroClass,0).name+", "+combat::ability(game.heroClass,1).name+", "+combat::ability(game.heroClass,2).name+", "+combat::ability(game.heroClass,3).name+" (ур. 6)",
        "Золото "+std::to_string(game.gold),
    };
    for(size_t i=0;i<lines.size();++i)ui.text(x+24,y+64+float(i)*30,lines[i],16,i==0?ui::gold:ui::text);
    ui.text(x+24,y+350,"Урон = урон оружия + сила − броня цели (минимум 1)",13,ui::muted);
    ui.text(x+24,y+372,"Уязвимая цель: броня 0 и +50% урона",13,ui::muted);
    // talents
    float tx=x+560;
    ui.text(tx,y+64,"Ветка развития · очков: "+std::to_string(h.skillPoints),18,h.skillPoints?ui::gold:ui::text);
    static const int levelReq[]={2,4,6};
    for(int t=0;t<3;++t){
        float yy=y+100+float(t)*120;bool learned=h.talents&(1<<t);
        ui.quad(tx,yy,520,108,learned?glm::vec4(.10f,.20f,.17f,1):glm::vec4(.06f,.08f,.09f,1));
        ui.text(tx+14,yy+8,std::to_string(t+1)+". "+combat::talentName(game.heroClass,t),17,learned?ui::mint:ui::gold);
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
    ui.panel(x,y,w,h,"Журнал заданий");
    if(ui.button(x+w-150,y+10,136,36,"Закрыть"))closeWindow();
    float yy=y+60;
    for(const auto& q:game.journal()){
        ui.quad(x+20,yy,700,q.progress.empty()?96.f:118.f,{.06f,.08f,.09f,1});
        ui.text(x+34,yy+8,(q.main?"★ ":"• ")+q.title+(q.done?"  (выполнено)":""),17,q.done?ui::muted:(q.main?ui::gold:ui::text));
        ui.paragraph(x+34,yy+36,670,"Цель: "+q.objective,14,ui::text);
        float extra=0;if(!q.progress.empty()){ui.text(x+34,yy+64,q.progress,13,ui::mint);extra=22;}
        ui.text(x+34,yy+66+extra,"Награда: "+q.reward,13,ui::muted);
        yy+=(q.progress.empty()?106:128);
    }
    // notes
    float nx=x+750;ui.text(nx,y+60,"Найденные записки",17,ui::gold);
    if(game.notes.empty())ui.paragraph(nx,y+92,340,"Пока пусто. Записки, дневники и надписи раскрывают историю падения замка.",14,ui::muted);
    for(size_t i=0;i<game.notes.size();++i){
        if(ui.button(nx,y+92+float(i)*48,340,40,rpg::noteTitle(game.notes[i]),true,"",false,14)){game.noteId=game.notes[i];stack.push_back(win);win=Window::Note;}
    }
}

// ------------------------------------------------------------------ map
void App::drawMap(){
    float x=40,y=90,w=ui.width-80,h=ui.height-150;
    ui.panel(x,y,w,h,"Карта долины Эшвальд");
    if(ui.button(x+w-150,y+10,136,36,"Закрыть"))closeWindow();
    struct Node{const char* id;float px,py;};
    const Node nodes[]={{"castle",.20f,.20f},{"heart",.07f,.08f},{"village",.20f,.55f},{"road",.42f,.55f},{"grove",.42f,.25f},{"lair",.54f,.12f},
                        {"mine",.64f,.65f},{"flooded",.76f,.80f},{"pass",.64f,.30f},{"crypt",.78f,.18f},{"throne",.90f,.10f}};
    const std::pair<int,int> links[]={{0,1},{0,2},{2,3},{3,4},{4,5},{3,6},{6,7},{3,8},{8,9},{9,10}};
    float mx=x+20,my=y+60,mw=w*.62f,mh=h-90;
    ui.quad(mx,my,mw,mh,{.05f,.07f,.06f,1});
    auto at=[&](int i){return glm::vec2(mx+nodes[i].px*mw,my+nodes[i].py*mh);};
    for(auto [a,b]:links){glm::vec2 p=at(a),q=at(b);int steps=int(glm::length(q-p)/8);
        for(int s=0;s<steps;s+=2){glm::vec2 r=glm::mix(p,q,float(s)/float(std::max(1,steps)));ui.quad(r.x-2,r.y-2,4,4,{.35f,.32f,.25f,1});}}
    std::string objectiveZone;int st=game.stage("main");
    const char* objZones[]={"village","village","grove","lair","village","flooded","village","pass","crypt","heart","village","village"};
    objectiveZone=objZones[std::clamp(st,0,11)];
    for(int i=0;i<11;++i){
        glm::vec2 p=at(i);bool visited=game.flags.count(std::string("visited:")+nodes[i].id)>0;
        bool dungeon=!rpg::zoneDef(nodes[i].id)->surface;bool here=game.zoneId==nodes[i].id;
        bool known=visited||game.flags.count(std::string("found:")+nodes[i].id)||!dungeon;
        glm::vec4 col=here?ui::mint:visited?ui::gold:glm::vec4(.35f,.38f,.38f,1);
        float r=dungeon?11.f:15.f;
        ui.quad(p.x-r,p.y-r,r*2,r*2,col*glm::vec4(1,1,1,.9f));ui.quad(p.x-r+3,p.y-r+3,r*2-6,r*2-6,{.05f,.07f,.06f,1});
        if(dungeon)ui.quad(p.x-4,p.y-4,8,8,col);
        std::string name=known?rpg::zoneName(nodes[i].id):"???";
        ui.textCentered(p.x,p.y+r+4,name,13,col);
        if(objectiveZone==nodes[i].id&&st<11){float b=.6f+.4f*std::sin(realTime*4);ui.textCentered(p.x,p.y-r-26,"! цель",15,ui::gold*glm::vec4(1,1,1,b));}
        for(const auto& z:rpg::zones())if(std::string(z.id)==nodes[i].id)for(const auto& e:z.entities)if(e.type==rpg::EntityType::Shrine){
            bool on=game.shrineActive(e.id);ui.quad(p.x+r+2,p.y-6,10,12,on?glm::vec4(.3f,.95f,.85f,1):glm::vec4(.3f,.3f,.3f,1));}
    }
    // legend and local mini-map
    float lx=mx+mw+24,ly=my;
    ui.text(lx,ly,"Обозначения",15,ui::gold);
    ui.quad(lx,ly+30,12,12,ui::mint);ui.text(lx+20,ly+26,"вы здесь",13,ui::text);
    ui.quad(lx,ly+52,12,12,ui::gold);ui.text(lx+20,ly+48,"посещено",13,ui::text);
    ui.quad(lx,ly+74,10,12,{.3f,.95f,.85f,1});ui.text(lx+20,ly+70,"святилище активно",13,ui::text);
    ui.quad(lx+150,ly+30,8,8,ui::gold);ui.text(lx+164,ly+26,"подземелье",13,ui::text);
    ui.text(lx+150,ly+48,"! цель задания",13,ui::gold);
    const auto& m=game.map;float cell=std::min((x+w-lx-20)/float(m.width),(h-170)/float(m.height));
    float ox=lx,oy=ly+110;
    ui.text(ox,oy-4,game.zone().name,14,ui::gold);oy+=20;
    for(int yy=0;yy<m.height;++yy)for(int xx=0;xx<m.width;++xx){
        auto t=m.at(xx,yy);glm::vec4 c{.12f,.15f,.12f,1};
        if(t==world::Tile::Wall)c={.03f,.03f,.04f,1};else if(t==world::Tile::Water)c={.10f,.22f,.32f,1};else if(t==world::Tile::Prop||t==world::Tile::LowProp)c={.08f,.12f,.08f,1};
        else if(t==world::Tile::Path)c={.28f,.24f,.16f,1};else if(t==world::Tile::Door)c={.45f,.25f,.1f,1};
        ui.quad(ox+float(xx)*cell,oy+float(yy)*cell,cell,cell,c);
    }
    for(const auto& e:game.entities){if(e.hidden)continue;glm::vec4 c{0,0,0,0};
        switch(e.def.type){case rpg::EntityType::Shrine:c=game.shrineActive(e.def.id)?glm::vec4(.3f,.95f,.85f,1):glm::vec4(.5f,.5f,.5f,1);break;
            case rpg::EntityType::Exit:case rpg::EntityType::Entrance:c={.9f,.8f,.3f,1};break;
            case rpg::EntityType::Npc:c={.9f,.6f,.9f,1};break;case rpg::EntityType::Chest:c=e.done?glm::vec4(.3f,.3f,.3f,1):glm::vec4(1,.6f,.2f,1);break;default:break;}
        if(c.a>0)ui.quad(ox+float(e.cell.x)*cell-1,oy+float(e.cell.y)*cell-1,cell+2,cell+2,c);}
    for(size_t i=1;i<game.game.actors.size();++i)if(game.game.actors[i].alive()){auto c=game.game.actors[i].cell;ui.quad(ox+float(c.x)*cell,oy+float(c.y)*cell,cell,cell,{.9f,.2f,.15f,1});}
    Cell hc=game.hero().cell;ui.quad(ox+float(hc.x)*cell-2,oy+float(hc.y)*cell-2,cell+4,cell+4,ui::mint);
}

// ------------------------------------------------------------------ crafting and smith
void App::drawCraft(){
    float x=ui.width*.5f-460,y=140,w=920,h=560;
    ui.panel(x,y,w,h,"Травница Ива — ремесло");
    if(ui.button(x+w-150,y+10,136,36,"Закрыть"))closeWindow();
    const char* names[]={"Зелье лечения","Противоядие","Большое зелье","Травяной оберег"};
    int i=0;
    for(const auto& r:rpg::recipes()){
        float yy=y+70+float(i)*110;
        ui.quad(x+20,yy,w-40,100,{.06f,.08f,.09f,1});
        auto d=rpg::item(r.result);
        ui.text(x+36,yy+10,names[i],18,ui::rarity(int(d->rarity)));
        ui.text(x+36,yy+38,d->desc,13,ui::muted);
        std::string need;for(const auto& [id,n]:r.needs)need+=std::string(rpg::item(id)->name)+" "+std::to_string(game.count(id))+"/"+std::to_string(n)+"   ";
        if(r.gold)need+="золото "+std::to_string(r.gold);
        ui.text(x+36,yy+64,need,14,ui::text);
        std::string why;bool ok=game.canCraft(r,why);
        if(ui.button(x+w-200,yy+28,160,44,"Сварить",ok,why)){if(game.craft(r,why))audio->play(Sfx::Loot,.7f);else game.toast(why,1);}
        ++i;
    }
}
void App::drawSmith(){
    float x=ui.width*.5f-420,y=180,w=840,h=420;
    ui.panel(x,y,w,h,"Кузнец Борн — закалка оружия");
    if(ui.button(x+w-150,y+10,136,36,"Закрыть"))closeWindow();
    const auto& s=game.equip[0];auto d=rpg::item(s.id);
    ui.text(x+30,y+70,std::string("Оружие: ")+(d?d->name:"—")+"  +"+std::to_string(s.upgrade)+" / +"+std::to_string(rpg::maxUpgrade),18,ui::text);
    ui.text(x+30,y+102,"Каждая ступень закалки даёт +2 к урону.",14,ui::muted);
    if(s.upgrade<rpg::maxUpgrade){
        auto c=rpg::upgradeCost(s.upgrade+1);
        std::string cost="Стоимость +"+std::to_string(s.upgrade+1)+": "+std::to_string(c.gold)+" золота, руда "+std::to_string(game.count("ore"))+"/"+std::to_string(c.ore);
        if(c.extraCount)cost+=", "+std::string(rpg::item(c.extra)->name)+" "+std::to_string(game.count(c.extra))+"/"+std::to_string(c.extraCount);
        ui.paragraph(x+30,y+150,w-60,cost,16,ui::gold);
    }
    std::string why;bool ok=game.canUpgrade(why);
    if(ui.button(x+30,y+240,260,48,"Закалить",ok,why)){if(game.upgradeWeapon(why))audio->play(Sfx::Hit);else game.toast(why,1);}
    if(!ok)ui.paragraph(x+310,y+246,w-340,why,14,ui::red);
    if(ui.button(x+30,y+320,260,44,"Торговать")){shopNpc="smith";win=Window::Shop;}
}

// ------------------------------------------------------------------ shrine and travel
void App::drawShrine(){
    float x=ui.width*.5f-360,y=150,w=720,h=560;
    std::string id=game.lastShrine;std::string label="Святилище";
    for(const auto& e:game.entities)if(e.def.type==rpg::EntityType::Shrine&&id==e.def.id)label=e.def.label;
    ui.panel(x,y,w,h,label);
    ui.paragraph(x+30,y+56,w-60,"Руны горят: это святилище запомнило вас. Если вы падёте, то очнётесь здесь, потеряв 10% золота. Сюжетный прогресс, святилища и уникальные трофеи сохраняются.",14,ui::muted);
    float by=y+150;std::string why;
    if(ui.button(x+30,by,w-60,48,"Отдохнуть — восстановить здоровье")){game.rest();syncActors(true);refreshVisuals(true);audio->play(Sfx::Shrine,.5f);}
    ui.text(x+40,by+52,"Отдых возрождает обычных врагов и травы. Боссы и сундуки не возвращаются.",12,ui::muted);
    bool battle=game.game.inBattle();
    if(ui.button(x+30,by+80,w-60,48,"Сохранить игру",!battle,"Нельзя сохраняться в бою")){stack.push_back(win);win=Window::Save;}
    auto shrines=game.activeShrines();
    if(ui.button(x+30,by+140,w-60,48,"Переместиться к святилищу",shrines.size()>1,"Активируйте другие святилища, чтобы перемещаться между ними")){stack.push_back(win);win=Window::Travel;}
    std::string dungeon=game.shrineDungeon(id);
    if(!dungeon.empty()){
        std::string zone=dungeon.substr(0,dungeon.find('@'));
        bool found=game.flags.count("found:"+zone)>0;
        if(ui.button(x+30,by+200,w-60,48,"Спуститься: "+rpg::zoneName(zone),found,"Сначала найдите вход в подземелье на поверхности")){
            if(game.descend(id,why)){win=Window::None;}else game.toast(why,1);}
    }else ui.text(x+40,by+212,"У этого святилища нет входа в подземелье.",13,ui::dim);
    if(ui.button(x+30,by+300,w-60,46,"Закрыть"))win=Window::None;
}
void App::drawTravel(){
    float x=ui.width*.5f-360,y=150,w=720,h=520;
    ui.panel(x,y,w,h,"Быстрое перемещение");
    if(ui.button(x+w-150,y+10,136,36,"Назад"))closeWindow();
    int i=0;std::string why;
    for(const auto& z:rpg::zones())for(const auto& e:z.entities)if(e.type==rpg::EntityType::Shrine){
        bool on=game.shrineActive(e.id);bool here=game.lastShrine==e.id&&game.zoneId==z.id;
        std::string reason=!on?"Святилище ещё не найдено и не активировано":here?"Вы уже здесь":"";
        if(ui.button(x+30,y+70+float(i)*70,w-60,56,std::string(e.label)+" · "+z.name,reason.empty(),reason)){
            if(game.travel(e.id,why)){win=Window::None;stack.clear();audio->play(Sfx::Shrine,.5f);}else game.toast(why,1);}
        ++i;
    }
}

// ------------------------------------------------------------------ dialog, notes, rewards
void App::drawDialog(){
    const auto& d=game.dialog;
    float w=900,x=ui.width*.5f-w*.5f,y=ui.height-330,h=300;
    ui.panel(x,y,w,h,d.speaker);
    if(dialogLine<int(d.lines.size())){
        ui.paragraph(x+30,y+60,w-60,d.lines[size_t(dialogLine)],18,ui::text,1.5f);
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
        if(!d.lines.empty())ui.paragraph(x+30,y+60,w-60,d.lines.back(),16,ui::muted);
        if(key(GLFW_KEY_ESCAPE))win=Window::None;
    }
}
void App::drawNote(){
    float w=760,x=ui.width*.5f-w*.5f,y=170,h=480;
    ui.panel(x,y,w,h,rpg::noteTitle(game.noteId));
    ui.quad(x+24,y+56,w-48,h-140,{.20f,.17f,.12f,.6f});
    ui.paragraph(x+44,y+76,w-88,rpg::noteText(game.noteId),17,{.92f,.86f,.72f,1},1.55f);
    if(ui.button(x+w*.5f-100,y+h-68,200,46,"Закрыть"))closeWindow();
}
void App::drawReward(){
    float cx=ui.width*.5f,y=170;
    ui.panel(cx-560,y,1120,520,"Награда Паучьей Матки — выберите одну вещь");
    ui.paragraph(cx-530,y+50,1060,"Печать Паутины восстановлена. Из логова можно унести лишь один трофей — каждый создан под свой класс, но выбирать можно любой.",15,ui::muted);
    for(size_t i=0;i<game.rewardChoices.size();++i){
        float x=cx-530+float(i)*360;rpg::Stack s{game.rewardChoices[i],1,0};
        ui.quad(x,y+110,340,330,{.06f,.08f,.09f,1});
        itemDetails(x+16,y+124,308,s,true);
        auto d=rpg::item(s.id);bool fits=rpg::canEquip(*d,game.heroClass);
        if(ui.button(x+16,y+380,308,46,fits?"Взять":"Взять (не ваш класс)")){game.chooseReward(int(i));win=Window::None;audio->play(Sfx::LevelUp);}
    }
}

// ------------------------------------------------------------------ pause, settings, help, slots
void App::drawPause(){
    float cx=ui.width*.5f,y=220;
    ui.panel(cx-230,y,460,400,"Пауза");
    if(ui.button(cx-200,y+70,400,50,"Продолжить"))win=Window::None;
    if(ui.button(cx-200,y+135,400,50,"Настройки")){stack.push_back(win);win=Window::Settings;}
    if(ui.button(cx-200,y+200,400,50,"Управление")){stack.push_back(win);win=Window::Help;}
    if(ui.button(cx-200,y+265,400,50,"Выйти в главное меню")){win=Window::ConfirmMenu;}
    ui.textCentered(cx,y+340,"Сохранение — у святилищ; автосохранение при переходах",13,ui::muted);
}
void App::drawSettings(){
    float x=ui.width*.5f-420,y=110,w=840,h=690;
    ui.panel(x,y,w,h,"Настройки");
    if(ui.button(x+w-150,y+10,136,36,"Назад")){if(!(options.smoke||options.benchmark||!options.capture.empty()))settings.save(root/"settings.ini");if(screen==Screen::Game)closeWindow();else win=Window::None;}
    static const std::pair<int,int> res[]={{1280,720},{1440,900},{1600,900},{1920,1080},{2560,1440}};
    float yy=y+64;auto row=[&](const std::string& label){ui.text(x+30,yy+10,label,16,ui::text);};
    auto changed=[&]{if(!(options.smoke||options.benchmark||!options.capture.empty()))settings.save(root/"settings.ini");};
    row("Разрешение окна");
    int current=0;for(int i=0;i<5;++i)if(res[i].first==settings.width&&res[i].second==settings.height)current=i;
    if(ui.button(x+400,yy,60,38,"<",!settings.fullscreen,"В полноэкранном режиме используется разрешение монитора")){current=(current+4)%5;settings.width=res[current].first;settings.height=res[current].second;applySettings(true);changed();}
    ui.textCentered(x+560,yy+9,std::to_string(settings.width)+" × "+std::to_string(settings.height),16,ui::gold);
    if(ui.button(x+660,yy,60,38,">",!settings.fullscreen,"В полноэкранном режиме используется разрешение монитора")){current=(current+1)%5;settings.width=res[current].first;settings.height=res[current].second;applySettings(true);changed();}
    yy+=52;row("Режим");
    if(ui.button(x+400,yy,320,38,settings.fullscreen?"Полный экран":"Оконный")){settings.fullscreen=!settings.fullscreen;applySettings(true);changed();}
    yy+=52;row("Вертикальная синхронизация");
    if(ui.button(x+400,yy,320,38,settings.vsync?"Вкл":"Выкл")){settings.vsync=!settings.vsync;applySettings(false);changed();}
    yy+=52;row("Качество теней");
    const char* q[]={"Выкл","Низкое (1024)","Высокое (2048)"};
    if(ui.button(x+400,yy,320,38,q[settings.shadowQuality])){settings.shadowQuality=(settings.shadowQuality+1)%3;changed();}
    yy+=52;row("Качество эффектов");
    const char* e[]={"Низкое","Среднее","Высокое"};
    if(ui.button(x+400,yy,320,38,e[settings.effects])){settings.effects=(settings.effects+1)%3;changed();}
    yy+=52;row("Основная техника (T): мягкие тени");
    if(ui.button(x+400,yy,320,38,settings.technique?"Включена":"Выключена",settings.shadowQuality>0,"Сначала включите тени в «Качестве теней»")){settings.technique=!settings.technique;changed();}
    yy+=52;row("Проекция камеры (O)");
    if(ui.button(x+400,yy,320,38,settings.perspective?"Перспектива":"Ортография (изометрия)")){settings.perspective=!settings.perspective;changed();}
    auto slider=[&](const std::string& label,float& v,float lo,float hi,float step,bool audioSetting){
        yy+=52;row(label);
        if(ui.button(x+400,yy,60,38,"-",v>lo+1e-4f,"Минимум")){v=std::max(lo,v-step);if(audioSetting)applySettings(false);changed();}
        ui.bar(x+470,yy+16,180,8,(v-lo)/(hi-lo),ui::mint);ui.text(x+470,yy-4,std::to_string(int(std::round(v*100)))+"%",13,ui::muted);
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
    ui.panel(x,y,w,h,credits?"Авторы и управление":"Управление");
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
    for(const auto& l:lines){ui.text(x+30,yy,l[0],15,ui::gold);ui.paragraph(x+380,yy,w-420,l[1],15,ui::text);yy+=30;}
    yy+=10;
    ui.paragraph(x+30,yy,w-60,"Бой идёт в реальном времени. Герой атакует сам, навыки тратят ману и перезаряжаются. Красная зона на земле заполняется перед ударом босса — успейте выйти. После тяжёлого удара босс уязвим (+50% урона). Враги, которых увели далеко от логова, возвращаются и лечатся.",14,ui::muted);
    if(credits){
        yy+=96;
        ui.text(x+30,yy,"Авторы",17,ui::gold);yy+=28;
        ui.paragraph(x+30,yy,w-60,"Команда: "+team+". Тема: "+topic+".",15,ui::text);yy+=28;
        ui.paragraph(x+30,yy,w-60,"Код, шейдеры, модели (процедурная геометрия), звуки и музыка (синтез в коде) созданы для проекта. Библиотеки: GLFW 3.5.1 (zlib), GLAD 2.0.8, GLM 1.0.3 (MIT), stb_image 2.30 (MIT/PD), miniaudio 0.11.21 (MIT-0/PD). Шрифт интерфейса — растр Consolas, сгенерированный локально. При разработке использовалась существенная помощь ИИ (Claude).",13,ui::muted);
    }
}
void App::drawSlots(bool saving){
    float x=ui.width*.5f-480,y=130,w=960,h=600;
    ui.panel(x,y,w,h,saving?"Сохранить игру":"Загрузить игру");
    if(ui.button(x+w-150,y+10,136,36,"Назад")){if(screen==Screen::Game)closeWindow();else win=Window::None;}
    for(int s=saving?1:0;s<4;++s){
        auto info=rpg::Campaign::inspect(slotPath(s));float yy=y+70+float(s-(saving?1:0))*120;
        ui.quad(x+24,yy,w-48,108,{.06f,.08f,.09f,1});
        ui.text(x+40,yy+10,s==0?"Автосохранение":"Слот "+std::to_string(s),18,ui::gold);
        if(!info.exists)ui.text(x+40,yy+44,"Пусто",15,ui::dim);
        else if(!info.valid)ui.text(x+40,yy+44,"Повреждено: "+info.error,15,ui::red);
        else{ui.text(x+40,yy+44,info.className+" · уровень "+std::to_string(info.level)+" · "+info.zoneName,15,ui::text);ui.text(x+40,yy+72,info.time,13,ui::muted);}
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
    if(!saving&&screen==Screen::Game)ui.text(x+30,y+h-34,"Загрузка заменит текущий прогресс.",13,ui::muted);
}
void App::drawDefeat(){
    float cx=ui.width*.5f,y=200;
    ui.quad(0,0,ui.width,ui.height,{.15f,0,0,.35f});
    ui.panel(cx-380,y,760,420,"Вы пали");
    ui.paragraph(cx-350,y+60,700,"Восстав у последнего святилища, вы сохраните сюжетный прогресс, открытые святилища и уникальные награды. Цена — 10% золота ("+std::to_string(game.gold/10)+"). Обычные враги вернутся на свои места, боссы — восстановят силы.",16);
    if(ui.button(cx-340,y+200,680,50,"Восстать у святилища")){game.respawnAtShrine();win=Window::None;syncActors(true);refreshVisuals(true);}
    bool any=false;for(int s=0;s<4;++s)any|=rpg::Campaign::inspect(slotPath(s)).valid;
    if(ui.button(cx-340,y+265,680,50,"Загрузить сохранение",any,"Нет сохранений")){stack.push_back(win);win=Window::Load;}
    if(ui.button(cx-340,y+330,680,50,"Главное меню")){game.flags.erase("defeat_pending");screen=Screen::Menu;win=Window::None;syncActors(true);refreshVisuals(true);}
}
void App::drawEpilogue(bool ending){
    float cx=ui.width*.5f,y=140;
    ui.quad(0,0,ui.width,ui.height,{0,0,0,.55f});
    ui.panel(cx-460,y,920,560,ending?"Конец истории":"Хранитель Костей повержен");
    std::string text=ending?
        "Серый Брод празднует. Над площадью висят фонари, у кузницы снова звенит молот, а шахтёры спускаются в штольни без страха.\n\nТри печати целы, скверна ушла из долины, и даже над замком Вальден светлеет небо. Староста вручил вам ключ от дома у брода — теперь это и ваш дом.\n\nСпасибо за игру! Вы можете продолжить исследование: в долине остались нетронутые тайники.":
        "Кости рассыпаются в пыль, и подземный гул стихает. Пурпурная скверна отступает от стен замка, словно отлив.\n\nПо горным тропам возвращаются беглецы, в окнах Серого Брода снова зажигается свет.\n\nВернитесь в деревню и поговорите со старостой — история ещё не окончена.";
    ui.paragraph(cx-420,y+70,840,text,18,ui::text,1.6f);
    if(ending){ui.text(cx-420,y+430,"Команда: "+team,14,ui::muted);ui.text(cx-420,y+454,"Время в игре: "+std::to_string(int(game.playTime/60))+" мин",14,ui::muted);}
    if(ui.button(cx-150,y+490,300,50,ending?"Продолжить исследование":"Продолжить"))win=Window::None;
}
