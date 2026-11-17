#include "combat.hpp"
#include "scene_layout.hpp"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <queue>

namespace world {
Map layoutRoom() {
    Map m;m.resize(layout::columns,layout::rows);
    for(int y=0;y<m.height;++y)for(int x=0;x<m.width;++x){
        bool edge=x<=0||y<=0||x>=m.width-1||y>=m.height-1;
        if(edge){m.set(x,y,Tile::Wall);m.glyphs[size_t(y*m.width+x)]='#';}
        else if(layout::isBlocked(x,y)){m.set(x,y,Tile::Prop);m.glyphs[size_t(y*m.width+x)]='R';}
    }
    return m;
}
}

namespace combat {
using glm::vec2;
static float len(vec2 v){return glm::length(v);}

const WeaponStats& weaponStats(Weapon weapon){
    static const WeaponStats data[]={{"Меч",11,1.8f,1.0f,false},{"Кинжалы",8,1.7f,.72f,false},{"Лук",10,8.f,1.1f,true},{"Посох",11,7.5f,1.2f,true}};
    return data[int(weapon)];
}
const char* className(Class type){static const char* n[]={"Воин","Разбойник","Маг"};return n[int(type)];}
const Ability& ability(Class type,int slot){
    using T=Target;
    static const Ability data[3][4]={
        {{"Удар щитом","Оглушает цель на 1,5 с, урон оружия +8.",30,7,2.4f,0,T::Unit,1},
         {"Вихрь","Бьёт всех врагов вокруг героя (радиус 2,8 м).",40,9,0,2.8f,T::Self,1},
         {"Бастион","+6 брони на 5 секунд.",25,14,0,0,T::Self,1},
         {"Сокрушение земли","Урон и оглушение на 2 с всем вокруг (4 м).",80,40,0,4.f,T::Self,6}},
        {{"Ядовитый укол","Урон +6 и яд на 4 секунды.",25,5,2.2f,0,T::Unit,1},
         {"Шаг тени","Рывок в точку (до 7 м); следующая атака вдвое сильнее.",30,10,7,0,T::Point,1},
         {"Веер кинжалов","Урон и яд всем врагам вокруг (3,2 м).",40,9,0,3.2f,T::Self,1},
         {"Метка смерти","Огромный урон цели; вдвое больше по раненым (<40% HP).",80,45,3.f,0,T::Unit,6}},
        {{"Огненная стрела","Снаряд: урон и горение на 3 секунды.",20,3,9.5f,0,T::Unit,1},
         {"Ледяная волна","Урон и сильное замедление врагам вокруг (3,5 м).",35,10,0,3.5f,T::Self,1},
         {"Скачок","Телепорт в точку (до 8 м).",30,12,8,0,T::Point,1},
         {"Метеор","Через 1 с падает в точку: огромный урон и горение (3 м).",90,40,12,3.f,T::Point,6}},
    };
    return data[int(type)][std::clamp(slot,0,3)];
}
const char* talentName(Class type,int tier){
    static const char* names[3][3]={{"Несокрушимый","Рассекающий удар","Сотрясение"},
        {"Глубокий яд","Удар в спину","Ловкие руки"},{"Огненный вал","Тлеющие угли","Поток маны"}};
    return names[int(type)][std::clamp(tier,0,2)];
}
const char* talentText(Class type,int tier){
    static const char* text[3][3]={
        {"Бастион даёт +10 брони и сразу лечит 25 здоровья.",
         "Обычные атаки задевают врагов рядом с целью (50% урона).",
         "Удар щитом оглушает и всех врагов в 2 м от цели."},
        {"Яд длится 6 секунд и наносит 7 урона в секунду.",
         "+50% урона по отравленным целям.",
         "+20% скорости атаки, зелья перезаряжаются вдвое быстрее."},
        {"Огненная стрела взрывается: урон врагам в 1,8 м от цели.",
         "Горение наносит 8 урона в секунду вместо 5.",
         "Перезарядка навыков −25%, +1,5 маны в секунду."}};
    return text[int(type)][std::clamp(tier,0,2)];
}
const KindInfo& kindInfo(Kind kind){
    using E=Effect;
    // name, hp, armor, speed m/s, damage, range m, attack interval s, xp, special, spirit, boss, ranged, trait
    static const KindInfo data[]={
        {"Герой",1,0,4.6f,0,1.8f,1,0,E::None,false,false,false,""},
        {"Крыса",22,0,3.8f,5,1.4f,1.0f,16,E::Poison,false,false,false,"Быстрая, кусает с ядом"},
        {"Слизень",36,1,2.2f,7,1.5f,1.4f,22,E::Slow,false,false,false,"Липкий удар замедляет"},
        {"Скелет",36,2,2.8f,8,1.6f,1.3f,28,E::Stun,false,false,false,"Строй: +2 брони рядом с союзником"},
        {"Призрак",30,0,3.4f,7,3.0f,1.4f,30,E::Slow,true,false,false,"Проходит сквозь препятствия"},
        {"Огр",90,4,2.6f,13,1.9f,1.7f,70,E::Stun,false,false,false,"Оглушающий удар"},
        {"Волк",26,0,4.4f,6,1.5f,1.0f,18,E::None,false,false,false,"Быстрый, нападает стаей"},
        {"Разбойник",38,1,3.1f,8,1.6f,1.3f,26,E::Stun,false,false,false,"Оглушающий удар дубиной"},
        {"Лучник",28,0,3.0f,7,7.0f,1.6f,26,E::Slow,false,false,true,"Стреляет издалека"},
        {"Паучок",14,0,4.2f,4,1.3f,.9f,10,E::Poison,false,false,false,"Ядовитый укус"},
        {"Ядовитый паук",30,0,3.2f,6,5.0f,1.5f,26,E::Poison,false,false,true,"Плюётся ядом"},
        {"Паук-защитник",56,3,2.6f,9,1.7f,1.4f,40,E::Web,false,false,false,"Опутывает паутиной"},
        {"Заражённый шахтёр",40,1,2.9f,8,1.6f,1.3f,30,E::Poison,false,false,false,"Заражённая кирка"},
        {"Каменный голем",70,5,2.0f,12,1.9f,1.8f,45,E::Stun,false,false,false,"Медленный и крепкий"},
        {"Скелет-лучник",30,1,2.8f,8,7.5f,1.6f,30,E::None,false,false,true,"Строй и дальний бой"},
        {"Рыцарь-страж",56,4,2.8f,10,1.7f,1.4f,45,E::Stun,false,false,false,"Строй, оглушение"},
        {"Паучья Матка",440,2,3.0f,12,2.4f,1.3f,220,E::Poison,false,true,false,"Паутина, призыв, ядовитый плевок"},
        {"Железный Смотритель",500,4,2.5f,15,2.6f,1.6f,320,E::Stun,false,true,false,"Удар, рывок, броня. Уязвим после тяжёлой атаки"},
        {"Павший рыцарь Вальден",540,4,3.1f,13,2.4f,1.25f,420,E::None,false,true,false,"Стойки, круговой удар, стражи"},
        {"Хранитель Костей",700,3,2.6f,14,7.0f,1.6f,650,E::None,false,true,true,"Щит пилонов, шипы, призыв"},
        {"Костяной пилон",110,2,0,0,0,99,30,E::None,false,false,false,"Питает щит Хранителя"},
    };
    return data[int(kind)];
}

bool Telegraph::contains(vec2 p) const{
    if(shape==0)return len(p-center)<=radius;
    vec2 d=glm::normalize(dir);vec2 rel=p-center;float along=glm::dot(rel,d),side=std::abs(rel.x*d.y-rel.y*d.x);
    return along>=-length*.5f&&along<=length*.5f&&side<=width*.5f;
}

Game::Game(){ownMap=world::layoutRoom();}
void Game::message(const std::string& text){log.push_back(text);if(log.size()>40)log.erase(log.begin());}
Actor Game::makeHero(Class type){
    Actor h;h.name=className(type);h.kind=Kind::Hero;h.radius=.42f;h.speed=4.6f;
    if(type==Class::Warrior){h.maxHp=150;h.power=3;h.armor=3;h.weapon=Weapon::Sword;h.maxMana=90;h.manaRegen=2.2f;}
    if(type==Class::Rogue){h.maxHp=120;h.power=3;h.armor=1;h.weapon=Weapon::Daggers;h.maxMana=110;h.manaRegen=2.8f;h.speed=5.0f;}
    if(type==Class::Mage){h.maxHp=105;h.power=4;h.armor=1;h.weapon=Weapon::Staff;h.maxMana=160;h.manaRegen=3.6f;}
    h.hp=h.maxHp;h.mana=h.maxMana;return h;
}
Actor Game::makeEnemy(Kind kind,Cell cell,const world::Map& map){
    const auto& k=kindInfo(kind);Actor e;e.kind=kind;e.name=k.name;e.cell=cell;e.home=cell;
    auto p=map.center(cell);e.pos=e.homePos={p.x,p.z};
    e.maxHp=e.hp=k.hp;e.armor=k.armor;e.power=k.damage;e.speed=k.speed;e.boss=k.boss;e.spirit=k.spirit;e.xpReward=k.xp;
    e.radius=k.boss?.85f:(kind==Kind::Spiderling||kind==Kind::Rat?.3f:.42f);
    e.attackTimer=.5f;e.special=3.f;
    return e;
}
void Game::place(int id,Cell c){auto& a=actors[size_t(id)];a.cell=c;a.pos=world2(c);a.path.clear();a.moving=false;if(id>0)a.homePos=a.pos;}
void Game::setupDemo(Class type){
    setMap(nullptr);phase=Phase::Explore;log.clear();telegraphs.clear();hazards.clear();projectiles.clear();heroClass=type;
    actors={makeHero(type)};place(0,{10,12});
    actors.push_back(makeEnemy(Kind::Rat,{8,8},M()));actors.push_back(makeEnemy(Kind::Slime,{12,7},M()));actors.push_back(makeEnemy(Kind::SkeletonArcher,{15,6},M()));
}
int Game::occupant(Cell p) const{for(size_t i=0;i<actors.size();++i)if(actors[i].alive()&&actors[i].cell==p)return int(i);return -1;}
bool Game::walkable(int id,Cell c) const{
    if(!M().inside(c.x,c.y))return false;
    bool spirit=id>=0&&id<int(actors.size())&&actors[size_t(id)].spirit;
    return spirit?!M().blockedForSpirit(c.x,c.y):!M().blocked(c.x,c.y);
}
bool Game::walkablePos(int id,vec2 p) const{
    float r=id>=0&&id<int(actors.size())?actors[size_t(id)].radius*.6f:.25f;
    for(vec2 o:{vec2(0),vec2(r,0),vec2(-r,0),vec2(0,r),vec2(0,-r)})if(!walkable(id,cellOf(p+o)))return false;
    return true;
}
std::vector<Cell> Game::path(int id,Cell destination,bool diagonal) const{
    if(id<0||id>=int(actors.size())||!walkable(id,destination))return {};
    Cell start=actors[size_t(id)].cell;if(start==destination||!M().inside(start.x,start.y))return {};
    const int w=M().width,count=w*M().height;
    std::vector<float> cost(size_t(count),1e9f);std::vector<int> prev(size_t(count),-1);
    using Node=std::pair<float,int>;std::priority_queue<Node,std::vector<Node>,std::greater<Node>> open;
    auto idx=[w](Cell c){return c.y*w+c.x;};
    auto h=[&](Cell c){float dx=float(std::abs(c.x-destination.x)),dy=float(std::abs(c.y-destination.y));return dx+dy-.586f*std::min(dx,dy);};
    cost[size_t(idx(start))]=0;open.push({h(start),idx(start)});
    static const Cell dirs[]={{1,0},{-1,0},{0,1},{0,-1},{1,1},{-1,1},{1,-1},{-1,-1}};
    while(!open.empty()){
        auto [f,i]=open.top();open.pop();Cell c{i%w,i/w};
        if(c==destination)break;
        for(int k=0;k<(diagonal?8:4);++k){Cell d=dirs[k];Cell n{c.x+d.x,c.y+d.y};
            if(!walkable(id,n))continue;
            if(d.x&&d.y&&(!walkable(id,{c.x+d.x,c.y})||!walkable(id,{c.x,c.y+d.y})))continue;
            float step=(d.x&&d.y?1.414f:1.f)+(M().at(n.x,n.y)==world::Tile::Web?1.5f:0)+(M().at(n.x,n.y)==world::Tile::Trap&&id>0?4.f:0);
            float nc=cost[size_t(i)]+step;
            if(nc<cost[size_t(idx(n))]){cost[size_t(idx(n))]=nc;prev[size_t(idx(n))]=i;open.push({nc+h(n),idx(n)});}
        }
    }
    if(prev[size_t(idx(destination))]<0)return {};
    std::vector<Cell> out;for(int i=idx(destination);i!=idx(start);i=prev[size_t(i)])out.push_back({i%w,i/w});
    std::reverse(out.begin(),out.end());return out;
}
bool Game::clearLine(vec2 a,vec2 b,int id) const{
    float l=len(b-a);int steps=int(l/.3f)+1;
    for(int s=1;s<=steps;++s)if(!walkablePos(id,glm::mix(a,b,float(s)/float(steps))))return false;
    return true;
}
void Game::planPath(int id,vec2 goal){
    auto& a=actors[size_t(id)];a.path.clear();a.goal=goal;a.moving=true;
    Cell dest=cellOf(goal);
    if(!walkable(id,dest)){ // nearest walkable cell around the goal
        Cell best{-1,-1};float bd=1e9f;
        for(int r=1;r<=3;++r)for(int dy=-r;dy<=r;++dy)for(int dx=-r;dx<=r;++dx){Cell c{dest.x+dx,dest.y+dy};
            if(!walkable(id,c))continue;float d=len(world2(c)-goal);if(d<bd){bd=d;best=c;}}
        if(best.x<0){a.moving=false;return;}dest=best;goal=world2(best);a.goal=goal;
    }
    if(clearLine(a.pos,goal,id)){a.path={goal};return;}
    auto cells=path(id,dest);
    if(cells.empty()){a.moving=false;return;}
    for(Cell c:cells)a.path.push_back(world2(c));
    a.path.back()=goal;
    // string pulling: drop waypoints that can be reached in a straight line
    std::vector<vec2> smooth;vec2 from=a.pos;size_t i=0;
    while(i<a.path.size()){size_t far=i;for(size_t j=a.path.size();j-->i;)if(clearLine(from,a.path[j],id)){far=j;break;}
        smooth.push_back(a.path[far]);from=a.path[far];i=far+1;}
    a.path=smooth;
}
float Game::moveSpeed(const Actor& a) const{
    float s=a.speed;if(a.slow>0)s*=.55f;if(a.web>0)s*=.4f;
    auto t=M().at(a.cell.x,a.cell.y);if(!a.spirit&&t==world::Tile::Web)s*=.5f;if(!a.spirit&&t==world::Tile::Shallow)s*=.75f;
    if(a.enraged)s*=1.25f;if(a.kind==Kind::FallenKnight&&a.stance==1)s*=.75f;
    return s;
}
void Game::steer(int id,float dt){
    auto& a=actors[size_t(id)];
    if(!a.moving||a.path.empty()||a.stun>0){if(a.path.empty())a.moving=false;return;}
    vec2 target=a.path.front();vec2 d=target-a.pos;float l=len(d),step=moveSpeed(a)*dt;
    if(l<=step){a.pos=target;a.path.erase(a.path.begin());if(a.path.empty())a.moving=false;return;}
    vec2 dir=d/l;vec2 next=a.pos+dir*step;
    if(walkablePos(id,next))a.pos=next;
    else if(walkablePos(id,{next.x,a.pos.y}))a.pos.x=next.x;
    else if(walkablePos(id,{a.pos.x,next.y}))a.pos.y=next.y;
    else{a.repath=0;planPath(id,a.goal);}
    a.facing=std::atan2(dir.x,dir.y);
}
void Game::separate(){
    for(size_t i=0;i<actors.size();++i)for(size_t j=i+1;j<actors.size();++j){
        auto& a=actors[i];auto& b=actors[j];if(!a.alive()||!b.alive())continue;
        vec2 d=b.pos-a.pos;float l=len(d),minD=a.radius+b.radius;
        if(l>=minD||l<1e-4f){if(l<1e-4f)b.pos.x+=.01f;continue;}
        vec2 push=d/l*(minD-l);
        bool fixedA=a.kind==Kind::Pylon,fixedB=b.kind==Kind::Pylon;
        float wa=fixedA?0:fixedB?1:.5f,wb=fixedB?0:fixedA?1:.5f;
        vec2 na=a.pos-push*wa,nb=b.pos+push*wb;
        if(walkablePos(int(i),na))a.pos=na;if(walkablePos(int(j),nb))b.pos=nb;
    }
}
// ------------------------------------------------------------------ damage
bool Game::shielded(int id) const{
    if(id<=0||actors[size_t(id)].kind!=Kind::BoneKeeper)return false;
    for(const auto& a:actors)if(a.alive()&&a.kind==Kind::Pylon)return true;
    return false;
}
int Game::defense(int id) const{
    const auto& a=actors[size_t(id)];if(a.vulnerable>0)return 0;
    int d=a.armor+a.armorBonus+(a.guardTime>0?a.guardArmor:0);
    bool formation=a.kind==Kind::Skeleton||a.kind==Kind::SkeletonArcher||a.kind==Kind::KnightGuard;
    if(formation)for(size_t i=1;i<actors.size();++i){const auto& b=actors[i];
        if(int(i)!=id&&b.alive()&&len(b.pos-a.pos)<2.2f&&(b.kind==Kind::Skeleton||b.kind==Kind::SkeletonArcher||b.kind==Kind::KnightGuard||b.kind==Kind::FallenKnight)){d+=2;break;}}
    return d;
}
float Game::attackRange(const Actor& a) const{
    if(a.kind!=Kind::Hero)return kindInfo(a.kind).range+a.radius*.5f;
    const auto& w=weaponStats(a.weapon);return w.range+(w.projectile?float(a.rangeBonus)*.8f:0);
}
float Game::attackInterval(const Actor& a) const{
    if(a.kind!=Kind::Hero){float t=kindInfo(a.kind).interval;if(a.kind==Kind::FallenKnight&&a.stance==0)t*=.7f;if(a.enraged)t*=.8f;return t;}
    float speedUp=1.f+.03f*float(a.initiativeBonus)+((a.talents&T3)&&heroClass==Class::Rogue?.2f:0);
    return weaponStats(a.weapon).interval/speedUp;
}
int Game::attackDamage(int source,int target,float multiplier) const{
    const auto& a=actors[size_t(source)];const auto& b=actors[size_t(target)];
    int base=source==0?weaponStats(a.weapon).damage+a.damageBonus+a.power:a.power+a.damageBonus;
    float dmg=float(std::max(1,int(std::round(float(base)*multiplier))-defense(target)));
    if(b.vulnerable>0)dmg*=1.5f;
    if(source==0&&(a.talents&T2)&&heroClass==Class::Rogue&&b.poison>0)dmg*=1.5f;
    if(b.kind==Kind::Warden&&b.stance==1)dmg*=.4f;
    if(b.kind==Kind::FallenKnight&&b.stance==1)dmg*=.55f;
    if(shielded(target))dmg=0;
    return int(std::round(dmg));
}
void Game::damage(int id,int amount,int source,bool){
    auto& a=actors[size_t(id)];if(!a.alive()||amount<=0)return;
    a.hp=std::max(0,a.hp-amount);a.hitFlash=.18f;
    events.push_back({Event::Hit,source,id,amount,a.cell,""});
    if(id>0&&!a.aggro&&source==0)engage(id);
    if(a.alive())return;
    message(a.name+" повержен.");
    events.push_back({Event::Death,source,id,0,a.cell,a.name});
    if(id==0){phase=Phase::Defeat;telegraphs.clear();projectiles.clear();message("Вы пали.");return;}
    grantXp(a.xpReward);
    if(a.boss)for(size_t i=1;i<actors.size();++i)if(actors[i].alive()&&(actors[i].owner==id||actors[i].kind==Kind::Pylon)){
        actors[i].hp=0;actors[i].xpReward=0;events.push_back({Event::Death,id,int(i),0,actors[i].cell,actors[i].name});}
    telegraphs.erase(std::remove_if(telegraphs.begin(),telegraphs.end(),[&](const Telegraph& t){return t.owner==id;}),telegraphs.end());
    if(a.kind==Kind::Pylon){bool any=false;for(const auto& b:actors)any|=b.alive()&&b.kind==Kind::Pylon;
        if(!any){message("Последний пилон рухнул — щит Хранителя пал!");events.push_back({Event::PhaseTwo,id,0,0,a.cell,"Щит разрушен"});}}
}
void Game::applyEffect(int id,Effect e,int source,float seconds){
    auto& v=actors[size_t(id)];if(!v.alive()||e==Effect::None)return;
    bool hero=source==0;bool rogueT1=hero&&heroClass==Class::Rogue&&(actors[0].talents&T1);
    switch(e){
    case Effect::Poison:v.poison=std::max(v.poison,seconds>0?seconds:(rogueT1?6.f:4.f));v.poisonDps=rogueT1?7:(hero?5:4);break;
    case Effect::Burn:v.burn=std::max(v.burn,seconds>0?seconds:3.f);v.burnDps=(hero&&heroClass==Class::Mage&&(actors[0].talents&T2))?8:5;break;
    case Effect::Slow:v.slow=std::max(v.slow,seconds>0?seconds:2.5f);break;
    case Effect::Web:v.web=std::max(v.web,seconds>0?seconds:2.f);break;
    case Effect::Stun:{float s=seconds>0?seconds:1.f;if(v.boss)s*=.35f;if(v.kind==Kind::Pylon)s=0;v.stun=std::max(v.stun,s);
        if(s>0){v.moving=false;}break;}
    default:break;
    }
    events.push_back({Event::Status,source,id,int(e),v.cell,""});
}
void Game::clearStatuses(Actor& a){a.poison=a.burn=a.slow=a.stun=a.web=a.guardTime=a.vulnerable=a.empowered=0;for(float& c:a.cd)c=0;}
void Game::grantXp(int amount){
    if(amount<=0||actors.empty())return;
    auto& h=actors[0];h.xp+=amount;
    while(h.xp>=h.level*100){
        h.xp-=h.level*100;++h.level;h.maxHp+=12;h.hp=std::min(h.maxHp,h.hp+30);h.power+=1;h.maxMana+=8;h.mana=h.maxMana;h.skillPoints+=1;
        if(h.level%2==0)++h.armor;
        message("Уровень "+std::to_string(h.level)+"!");
        events.push_back({Event::LevelUp,0,0,h.level,h.cell,"Новый уровень"});
    }
}
int Game::enemyAt(vec2 p,float radius) const{
    int best=-1;float bd=radius;
    for(size_t i=1;i<actors.size();++i){if(!actors[i].alive())continue;float d=len(actors[i].pos-p)-actors[i].radius;if(d<bd){bd=d;best=int(i);}}
    return best;
}
// Applies a melee hit now or launches a projectile.
static void markDirectHeroImpacts(std::vector<Event>& events,size_t first){
    for(size_t i=first;i<events.size();++i){
        auto& e=events[i];
        if(e.source==0&&(e.type==Event::Hit||e.type==Event::Death))e.directHeroImpact=true;
    }
}
void Game::strike(int source,int target,float multiplier,Effect effect,float splash){
    auto& a=actors[size_t(source)];
    int dmg=attackDamage(source,target,multiplier);
    bool projectile=source==0?weaponStats(a.weapon).projectile:kindInfo(a.kind).ranged;
    events.push_back({Event::Attack,source,target,0,a.cell,""});
    if(projectile&&len(actors[size_t(target)].pos-a.pos)>2.f){projectiles.push_back({a.pos,source,target,source==0?16.f:11.f,dmg,effect,splash,source==0});return;}
    size_t firstHit=events.size();
    damage(target,dmg,source);applyEffect(target,effect,source);
    if(splash>0)for(size_t i=1;i<actors.size();++i)if(int(i)!=target&&actors[i].alive()&&(source==0)&&len(actors[i].pos-actors[size_t(target)].pos)<=splash){
        damage(int(i),std::max(1,dmg/2),source);applyEffect(int(i),effect,source);}
    if(source==0&&!projectile)markDirectHeroImpacts(events,firstHit);
}
// ------------------------------------------------------------------ orders
bool Game::moveTo(vec2 point){
    if(actors.empty()||!actors[0].alive())return false;
    auto& h=actors[0];h.target=-1;pendingSlot=-1;
    planPath(0,point);return h.moving;
}
bool Game::attackUnit(int target){
    if(target<=0||target>=int(actors.size())||!actors[size_t(target)].alive())return false;
    actors[0].target=target;pendingSlot=-1;actors[0].repath=0;return true;
}
void Game::stop(){if(actors.empty())return;actors[0].target=-1;actors[0].path.clear();actors[0].moving=false;pendingSlot=-1;}
bool Game::abilityUnlocked(int slot) const{return actors[0].level>=ability(heroClass,slot).level;}
Preview Game::castPreview(int slot,int target,vec2 point) const{
    Preview p;if(actors.empty())return p;const auto& h=actors[0];const auto& ab=ability(heroClass,slot);
    if(!h.alive()){p.reason="Герой пал";return p;}
    if(!abilityUnlocked(slot)){p.reason="Откроется на уровне "+std::to_string(ab.level);return p;}
    float cd=h.cd[slot];
    if(cd>0){char buf[32];std::snprintf(buf,sizeof(buf),"%.1f",double(cd));p.reason=std::string("Перезарядка: ")+buf+" с";return p;}
    if(h.mana<float(ab.mana)){p.reason="Не хватает маны ("+std::to_string(ab.mana)+")";return p;}
    if(h.stun>0){p.reason="Герой оглушён";return p;}
    if(ab.target==Target::Unit){
        if(target<=0||target>=int(actors.size())||!actors[size_t(target)].alive()){p.reason="Наведите курсор на врага";return p;}
        p.damage=attackDamage(0,target,1)+8;
    }
    (void)point;p.allowed=true;p.reason=ab.name;return p;
}
bool Game::cast(int slot,int target,vec2 point){
    auto p=castPreview(slot,target,point);if(!p.allowed){message(p.reason);return false;}
    auto& h=actors[0];const auto& ab=ability(heroClass,slot);
    vec2 at=ab.target==Target::Unit?actors[size_t(target)].pos:point;
    float dist=len(at-h.pos);
    if(ab.target!=Target::Self&&dist>ab.range+(ab.target==Target::Unit?actors[size_t(target)].radius:0.f)){
        if(ab.target==Target::Point&&(slot==1||slot==2)){at=h.pos+(at-h.pos)/dist*ab.range;point=at;}   // blinks go as far as they can
        else{pendingSlot=slot;pendingTarget=target;pendingPoint=point;planPath(0,at);return true;}
    }
    pendingSlot=-1;
    float cdScale=(heroClass==Class::Mage&&(h.talents&T3))?.75f:1.f;
    h.mana-=float(ab.mana);h.cd[slot]=ab.cooldown*cdScale;h.path.clear();h.moving=false;
    if(dist>.1f)h.facing=std::atan2(at.x-h.pos.x,at.y-h.pos.y);
    events.push_back({Event::Cast,0,target,slot,h.cell,ab.name,h.pos});
    size_t firstHit=events.size();
    auto around=[&](vec2 c,float r,auto fn){for(size_t i=1;i<actors.size();++i)if(actors[i].alive()&&len(actors[i].pos-c)<=r+actors[i].radius)fn(int(i));};
    switch(heroClass){
    case Class::Warrior:
        if(slot==0){damage(target,attackDamage(0,target,1)+8,0);applyEffect(target,Effect::Stun,0,1.5f);
            if(h.talents&T3)around(actors[size_t(target)].pos,2.f,[&](int i){if(i!=target){damage(i,attackDamage(0,i,.6f),0);applyEffect(i,Effect::Stun,0,1.2f);}});}
        if(slot==1)around(h.pos,ab.radius,[&](int i){damage(i,attackDamage(0,i,1.3f),0);});
        if(slot==2){h.guardTime=5;h.guardArmor=(h.talents&T1)?10:6;if(h.talents&T1){h.hp=std::min(h.maxHp,h.hp+25);events.push_back({Event::Heal,0,0,25,h.cell,""});}}
        if(slot==3)around(h.pos,ab.radius,[&](int i){damage(i,40+h.power*2,0);applyEffect(i,Effect::Stun,0,2.f);});
        break;
    case Class::Rogue:
        if(slot==0){damage(target,attackDamage(0,target,1)+6,0);applyEffect(target,Effect::Poison,0);}
        if(slot==1){vec2 dest=at;for(int k=0;k<20&&!walkablePos(0,dest);++k)dest=glm::mix(dest,h.pos,.15f);if(walkablePos(0,dest))h.pos=dest;h.empowered=4;}
        if(slot==2)around(h.pos,ab.radius,[&](int i){damage(i,18+h.power,0);applyEffect(i,Effect::Poison,0,3.f);});
        if(slot==3){const auto& t=actors[size_t(target)];int dmg=60+h.power*3;if(t.hp*10<t.maxHp*4)dmg*=2;damage(target,dmg,0);}
        break;
    case Class::Mage:
        if(slot==0)projectiles.push_back({h.pos,0,target,15.f,16+int(float(h.power)*1.5f)+h.damageBonus,Effect::Burn,(h.talents&T1)?1.8f:0.f,true});
        if(slot==1)around(h.pos,ab.radius,[&](int i){damage(i,14+h.power,0);applyEffect(i,Effect::Slow,0,3.f);});
        if(slot==2){vec2 dest=at;for(int k=0;k<20&&!walkablePos(0,dest);++k)dest=glm::mix(dest,h.pos,.15f);if(walkablePos(0,dest))h.pos=dest;}
        if(slot==3){Telegraph t;t.owner=0;t.shape=0;t.center=at;t.radius=ab.radius;t.total=t.time=1.f;t.damage=70+h.power*3;t.effect=Effect::Burn;t.label="Метеор";telegraphs.push_back(t);}
        break;
    }
    markDirectHeroImpacts(events,firstHit);
    return true;
}
void Game::engage(int trigger){
    if(trigger<=0||trigger>=int(actors.size()))return;
    auto& t=actors[size_t(trigger)];if(!t.alive())return;
    for(size_t i=1;i<actors.size();++i){auto& a=actors[i];if(!a.alive()||a.aggro||(a.kind==Kind::Pylon&&!t.boss))continue;
        bool join=int(i)==trigger||(len(a.pos-t.pos)<6.5f&&lineOfSight(a.cell,t.cell))||(t.group!=0&&a.group==t.group);
        if(join){a.aggro=true;a.participant=true;a.think=0;}}
    refreshPhase();
}
void Game::refreshPhase(){
    if(phase==Phase::Defeat)return;
    bool any=false;for(size_t i=1;i<actors.size();++i)any|=actors[i].alive()&&actors[i].aggro;
    if(any&&phase!=Phase::Battle){phase=Phase::Battle;events.push_back({Event::BattleStart,0,0,0,actors[0].cell,""});message("Бой!");}
    if(!any&&phase==Phase::Battle){phase=Phase::Explore;telegraphs.erase(std::remove_if(telegraphs.begin(),telegraphs.end(),[](const Telegraph& t){return t.owner!=0;}),telegraphs.end());
        hazards.clear();events.push_back({Event::BattleEnd,0,0,0,actors[0].cell,""});message("Бой окончен.");
        for(auto& a:actors)a.participant=false;}
}
int Game::aliveSummons(int owner) const{int n=0;for(const auto& a:actors)n+=a.alive()&&a.owner==owner;return n;}
int Game::summon(Kind kind,vec2 near,int owner){
    for(int k=0;k<12;++k){float ang=randomf()*6.283f,r=1.5f+randomf()*1.5f;vec2 p=near+vec2(std::cos(ang),std::sin(ang))*r;
        if(!walkablePos(-1,p))continue;
        Actor e=makeEnemy(kind,cellOf(p),M());e.pos=p;e.homePos=actors[size_t(owner)].homePos;e.owner=owner;e.aggro=e.participant=true;e.xpReward/=3;e.group=actors[size_t(owner)].group;
        actors.push_back(e);int id=int(actors.size())-1;
        events.push_back({Event::Summon,owner,id,0,e.cell,e.name});return id;}
    return -1;
}
void Game::telegraph(int owner,Telegraph t){
    t.owner=owner;t.time=t.total;telegraphs.push_back(t);
    message(actors[size_t(owner)].name+": "+t.label+"!");
    events.push_back({Event::Warn,owner,0,t.damage,cellOf(t.center),t.label});
}
void Game::resolveTelegraph(const Telegraph& t){
    if(t.owner==0){ // hero meteor
        for(size_t i=1;i<actors.size();++i)if(actors[i].alive()&&t.contains(actors[i].pos)){damage(int(i),t.damage,0);applyEffect(int(i),t.effect,0);}
        events.push_back({Event::Hit,0,-1,0,cellOf(t.center),"meteor",t.center});return;
    }
    if(t.owner<0||t.owner>=int(actors.size())||!actors[size_t(t.owner)].alive())return;
    auto& boss=actors[size_t(t.owner)];auto& h=actors[0];
    if(t.hazard){hazards.push_back({t.center,t.radius,18.f,6});message("Ядовитый туман растекается по полу.");return;}
    if(t.charge){
        vec2 d=glm::normalize(t.dir);vec2 end=t.center+d*t.length*.5f;vec2 p=boss.pos;
        for(int k=0;k<60;++k){vec2 n=p+d*.25f;if(len(n-boss.pos)>len(end-boss.pos)||!walkablePos(t.owner,n))break;p=n;}
        boss.pos=p;boss.path.clear();
    }
    if(h.alive()&&t.contains(h.pos)){damage(0,std::max(1,t.damage-defense(0)/2),t.owner);applyEffect(0,t.effect,t.owner);}
    else{events.push_back({Event::Miss,t.owner,0,0,h.cell,"Уклонение"});}
    if(t.heavy&&boss.alive()){boss.vulnerable=2.5f;message(boss.name+" открыт для удара!");events.push_back({Event::Vulnerable,t.owner,t.owner,0,boss.cell,"Уязвим"});}
}
// ------------------------------------------------------------------ AI
void Game::enemyAi(int id,float dt){
    auto& a=actors[size_t(id)];auto& h=actors[0];const auto& info=kindInfo(a.kind);
    if(a.kind==Kind::Pylon)return;
    float dh=len(h.pos-a.pos);
    if(!a.aggro){
        if(h.alive()&&phase!=Phase::Defeat&&dh<(a.boss?6.f:7.f)&&lineOfSight(a.cell,h.cell)){engage(id);return;}
        a.think-=dt;
        if(a.think<=0&&!a.boss){a.think=2.5f+randomf()*3.f;
            vec2 p=a.homePos+vec2(randomf()-.5f,randomf()-.5f)*4.f;if(walkablePos(id,p))planPath(id,p);}
        return;
    }
    // leash: give up when dragged far from home
    if(len(a.pos-a.homePos)>(a.boss?14.f:20.f)&&dh>6.f){a.aggro=false;a.participant=false;a.hp=a.maxHp;planPath(id,a.homePos);refreshPhase();return;}
    if(!h.alive())return;
    if(a.boss){bossAi(id,dt);return;}
    float range=attackRange(a);
    bool sight=lineOfSight(a.cell,h.cell);
    if(dh<=range+h.radius&&sight){
        a.path.clear();a.moving=false;a.facing=std::atan2(h.pos.x-a.pos.x,h.pos.y-a.pos.y);
        if(a.attackTimer<=0){
            a.attackTimer=attackInterval(a);a.swing=.25f;
            bool special=a.special<=0&&info.onSpecial!=Effect::None;
            if(special)a.special=6.f+randomf()*3.f;
            strike(id,0,special?1.35f:1.f,special?info.onSpecial:Effect::None);
        }
    }else{
        a.repath-=dt;
        if(a.repath<=0){a.repath=.35f;
            vec2 goal=h.pos;
            if(info.ranged&&dh<range*.6f&&sight)goal=a.pos;   // archers hold position when close enough
            planPath(id,goal);}
        // cannot reach the hero (behind a locked door, across water): give up after a while
        if(!a.moving)a.leash+=dt;else a.leash=0;
        if(a.leash>5.f){a.leash=0;a.aggro=false;a.participant=false;a.hp=a.maxHp;planPath(id,a.homePos);refreshPhase();}
        return;
    }
    a.leash=0;
}
void Game::bossAi(int id,float dt){
    auto& b=actors[size_t(id)];auto& h=actors[0];float dh=len(h.pos-b.pos);
    for(float& t:b.timers)t-=dt;
    bool winding=false;for(const auto& t:telegraphs)winding|=t.owner==id&&!t.hazard;
    auto chaseAndHit=[&](float range){
        if(winding)return;
        if(dh<=range+h.radius&&lineOfSight(b.cell,h.cell)){b.path.clear();b.moving=false;b.facing=std::atan2(h.pos.x-b.pos.x,h.pos.y-b.pos.y);
            if(b.attackTimer<=0){b.attackTimer=attackInterval(b);b.swing=.3f;strike(id,0,1.f);}}
        else{b.repath-=dt;if(b.repath<=0){b.repath=.4f;planPath(id,h.pos);}}
    };
    auto stopForCast=[&]{b.path.clear();b.moving=false;};
    vec2 toHero=dh>.01f?(h.pos-b.pos)/dh:vec2(1,0);
    switch(b.kind){
    case Kind::Matriarch:{
        if(!b.halfTriggered&&b.hp*2<=b.maxHp){b.halfTriggered=true;events.push_back({Event::PhaseTwo,id,0,0,b.cell,"Матка в ярости"});
            for(int k=0;k<4;++k){Telegraph t;t.shape=0;t.center=h.pos+vec2(randomf()-.5f,randomf()-.5f)*7.f;t.radius=1.7f;t.total=1.5f;t.hazard=true;t.label="Ядовитый туман";telegraph(id,t);}}
        if(!winding&&b.timers[0]<=0&&dh<9.f&&lineOfSight(b.cell,h.cell)){b.timers[0]=7.f;projectiles.push_back({b.pos,id,0,10.f,6,Effect::Web,0,false});message("Матка выстреливает паутиной!");}
        if(b.timers[1]<=0&&aliveSummons(id)<3){b.timers[1]=10.f;vec2 at=b.pos;summon(Kind::Spiderling,at,id);summon(Kind::Spiderling,at,id);return;}
        if(!winding&&b.timers[2]<=0&&dh<8.f){b.timers[2]=6.f;stopForCast();Telegraph t;t.shape=0;t.center=h.pos;t.radius=1.9f;t.total=1.4f;t.damage=24;t.effect=Effect::Poison;t.label="Ядовитый плевок";telegraph(id,t);return;}
        chaseAndHit(2.4f);break;}
    case Kind::Warden:{
        if(b.stance==1&&b.timers[3]<=0)b.stance=0;
        if(b.hp*10<b.maxHp*6&&b.stance==0&&b.timers[3]<-10.f){b.stance=1;b.timers[3]=5.f;message("Смотритель смыкает броню!");}
        if(!winding&&b.timers[0]<=0&&dh<3.8f){b.timers[0]=6.5f;stopForCast();Telegraph t;t.shape=1;t.dir=toHero;t.length=4.2f;t.width=3.f;t.center=b.pos+toHero*2.3f;t.total=1.3f;t.damage=36;t.effect=Effect::Stun;t.heavy=true;t.label="Сокрушающий удар";telegraph(id,t);return;}
        if(!winding&&b.timers[1]<=0&&dh>4.f&&dh<12.f&&lineOfSight(b.cell,h.cell)){b.timers[1]=10.f;stopForCast();float l=std::min(dh+2.f,11.f);
            Telegraph t;t.shape=1;t.dir=toHero;t.length=l;t.width=1.8f;t.center=b.pos+toHero*l*.5f;t.total=1.1f;t.damage=28;t.effect=Effect::Slow;t.heavy=true;t.charge=true;t.label="Рывок";telegraph(id,t);return;}
        chaseAndHit(2.6f);break;}
    case Kind::FallenKnight:{
        if(!b.enraged&&b.hp*10<b.maxHp*3){b.enraged=true;message("Павший рыцарь впадает в ярость!");events.push_back({Event::PhaseTwo,id,0,0,b.cell,"Ярость"});}
        if(b.timers[3]<=0){b.timers[3]=6.f;b.stance=1-b.stance;message(b.stance?"Рыцарь поднимает щит (урон по нему −45%)":"Рыцарь переходит в натиск");}
        int marks[]={66,33};
        for(int m=0;m<2;++m)if(!(b.summonMarks&(1<<m))&&b.hp*100<=b.maxHp*marks[m]){b.summonMarks|=1<<m;vec2 at=b.pos;summon(Kind::KnightGuard,at,id);summon(Kind::KnightGuard,at,id);return;}
        if(!winding&&b.timers[0]<=0&&dh<4.5f){b.timers[0]=b.enraged?5.f:8.f;stopForCast();Telegraph t;t.shape=0;t.center=b.pos;t.radius=3.2f;t.total=1.25f;t.damage=34;t.heavy=true;t.label="Круговой удар";telegraph(id,t);return;}
        chaseAndHit(2.4f);break;}
    case Kind::BoneKeeper:{
        bool shield=shielded(id);
        if(!shield&&!b.phaseTwo){b.phaseTwo=true;message("Хранитель Костей лишён щита!");}
        if(b.timers[1]<=0&&aliveSummons(id)<3){b.timers[1]=12.f;summon(shield?Kind::Skeleton:Kind::SkeletonArcher,b.pos,id);return;}
        if(!winding&&b.timers[0]<=0){b.timers[0]=shield?4.5f:6.f;
            for(int k=0;k<3;++k){Telegraph t;t.shape=0;t.center=h.pos+(k==0?vec2(0):vec2(randomf()-.5f,randomf()-.5f)*4.f);t.radius=1.4f;t.total=1.3f;t.damage=18;t.label="Костяные шипы";telegraph(id,t);}}
        if(!shield&&!winding&&b.timers[2]<=0){b.timers[2]=9.f;stopForCast();
            for(int k=0;k<2;++k){Telegraph t;t.shape=1;t.dir=k?vec2(1,0):vec2(0,1);t.center=b.pos;t.length=18.f;t.width=2.f;t.total=1.6f;t.damage=40;t.heavy=k==0;t.label="Крест погибели";telegraph(id,t);}return;}
        if(shield){ // stays among its pylons and shoots
            if(dh<=7.f+h.radius&&lineOfSight(b.cell,h.cell)&&b.attackTimer<=0){b.attackTimer=attackInterval(b);b.swing=.3f;strike(id,0,1.f);}
            if(len(b.pos-b.homePos)>2.f&&!b.moving)planPath(id,b.homePos);
        }else chaseAndHit(7.f);
        break;}
    default:chaseAndHit(2.f);break;
    }
}
void Game::updateActor(int id,float dt){
    auto& a=actors[size_t(id)];if(!a.alive())return;
    // statuses
    a.attackTimer-=dt;a.special-=dt;a.swing=std::max(0.f,a.swing-dt);a.hitFlash=std::max(0.f,a.hitFlash-dt);
    for(float& c:a.cd)c=std::max(0.f,c-dt);
    a.slow=std::max(0.f,a.slow-dt);a.web=std::max(0.f,a.web-dt);a.stun=std::max(0.f,a.stun-dt);
    a.guardTime=std::max(0.f,a.guardTime-dt);a.vulnerable=std::max(0.f,a.vulnerable-dt);a.empowered=std::max(0.f,a.empowered-dt);
    a.timers[3]-=0;   // boss timers handled in bossAi
    float dps=0;
    if(a.poison>0){a.poison-=dt;dps+=float(a.poisonDps);}
    if(a.burn>0){a.burn-=dt;dps+=float(a.burnDps);}
    for(const auto& hz:hazards)if(len(a.pos-hz.center)<=hz.radius&&(id==0||(a.kind!=Kind::Matriarch&&a.kind!=Kind::Spiderling)))dps+=float(hz.dps);
    if(!a.spirit&&M().at(a.cell.x,a.cell.y)==world::Tile::Poison&&a.kind!=Kind::Spiderling&&a.kind!=Kind::VenomSpider&&a.kind!=Kind::SpiderGuard&&a.kind!=Kind::Matriarch)
        {a.poison=std::max(a.poison,1.f);if(id==0&&phase==Phase::Explore)a.poisonDps=3;}
    if(dps>0){a.dot+=dps*dt;if(a.dot>=1.f){int d=int(a.dot);a.dot-=float(d);
        if(id==0&&phase==Phase::Explore)a.hp=std::max(1,a.hp-d);else damage(id,d,-1);}}
    if(!a.alive())return;
    if(id==0){a.mana=std::min(a.maxMana,a.mana+(a.manaRegen+((heroClass==Class::Mage&&(a.talents&T3))?1.5f:0.f))*dt);
        if(phase==Phase::Explore&&a.hp<a.maxHp){a.dot-=dt*1.5f;if(a.dot<=-1.f){a.dot+=1.f;a.hp=std::min(a.maxHp,a.hp+1);}}}
    if(a.stun>0)return;
    if(id==0){
        // pending ability waits until in range
        if(pendingSlot>=0){const auto& ab=ability(heroClass,pendingSlot);
            bool valid=ab.target!=Target::Unit||(pendingTarget>0&&pendingTarget<int(actors.size())&&actors[size_t(pendingTarget)].alive());
            if(!valid)pendingSlot=-1;
            else{vec2 at=ab.target==Target::Unit?actors[size_t(pendingTarget)].pos:pendingPoint;
                if(len(at-a.pos)<=ab.range+(ab.target==Target::Unit?actors[size_t(pendingTarget)].radius:0.f)){int s=pendingSlot;pendingSlot=-1;cast(s,pendingTarget,pendingPoint);}
                else{a.repath-=dt;if(a.repath<=0){a.repath=.3f;planPath(0,at);}}}
        }else if(a.target>0){
            auto& t=actors[size_t(a.target)];
            if(!t.alive()){a.target=-1;}
            else{float d=len(t.pos-a.pos)-t.radius;
                if(d<=attackRange(a)&&lineOfSight(a.cell,t.cell)){a.path.clear();a.moving=false;a.facing=std::atan2(t.pos.x-a.pos.x,t.pos.y-a.pos.y);
                    if(a.attackTimer<=0){a.attackTimer=attackInterval(a);a.swing=.25f;
                        float mult=a.empowered>0?2.f:1.f;a.empowered=0;
                        float splash=(heroClass==Class::Warrior&&(a.talents&T2))?1.8f:0.f;
                        strike(0,a.target,mult,Effect::None,splash);}}
                else{a.repath-=dt;if(a.repath<=0||!a.moving){a.repath=.3f;planPath(0,t.pos);}}}
        }
    }else enemyAi(id,dt);
    steer(id,dt);
}
void Game::update(float dt){
    if(actors.empty())return;
    time+=dt;itemCooldown=std::max(0.f,itemCooldown-dt);
    if(phase==Phase::Defeat)return;
    for(size_t i=0;i<actors.size();++i)updateActor(int(i),dt);
    separate();
    // cells, traps and hero steps
    for(size_t i=0;i<actors.size();++i){auto& a=actors[i];if(!a.alive())continue;
        Cell c=cellOf(a.pos);
        if(c!=a.cell){a.cell=c;
            if(!a.spirit&&M().at(c.x,c.y)==world::Tile::Trap){M().set(c.x,c.y,world::Tile::Floor);int amount=i==0?12:8;
                message(a.name+": ловушка! -"+std::to_string(amount));events.push_back({Event::Trap,-1,int(i),amount,c,"Ловушка"});
                if(i==0&&phase==Phase::Explore)a.hp=std::max(1,a.hp-amount);else damage(int(i),amount);}
            if(i==0)events.push_back({Event::Step,0,0,0,c,""});}}
    // projectiles
    for(size_t k=0;k<projectiles.size();){
        auto& p=projectiles[k];bool done=false;
        if(p.target<0||p.target>=int(actors.size())||!actors[size_t(p.target)].alive())done=true;
        else{vec2 d=actors[size_t(p.target)].pos-p.pos;float l=len(d),step=p.speed*dt;
            if(l<=step+.3f){int tgt=p.target;damage(tgt,p.damage,p.source);
                if(p.effect!=Effect::None)applyEffect(tgt,p.effect,p.source);
                if(p.splash>0)for(size_t i=1;i<actors.size();++i)if(int(i)!=tgt&&actors[i].alive()&&len(actors[i].pos-actors[size_t(tgt)].pos)<=p.splash){damage(int(i),std::max(1,p.damage/2),p.source);applyEffect(int(i),p.effect,p.source);}
                done=true;}
            else p.pos+=d/l*step;}
        if(done)projectiles.erase(projectiles.begin()+long(k));else ++k;
    }
    // marked attacks
    for(auto& t:telegraphs)t.time-=dt;
    std::vector<Telegraph> fire;
    for(size_t k=0;k<telegraphs.size();){if(telegraphs[k].time<=0){fire.push_back(telegraphs[k]);telegraphs.erase(telegraphs.begin()+long(k));}else ++k;}
    for(const auto& t:fire)resolveTelegraph(t);
    for(auto& hz:hazards)hz.time-=dt;
    hazards.erase(std::remove_if(hazards.begin(),hazards.end(),[](const Hazard& h){return h.time<=0;}),hazards.end());
    refreshPhase();
}
std::string Game::status(const Actor& a) const{
    std::string s;
    if(a.poison>0)s+="Яд  ";if(a.burn>0)s+="Горение  ";if(a.stun>0)s+="Оглушён  ";if(a.slow>0)s+="Замедлен  ";
    if(a.web>0)s+="Паутина  ";if(a.guardTime>0)s+="Бастион  ";if(a.vulnerable>0)s+="УЯЗВИМ  ";if(a.empowered>0)s+="Удар из тени  ";
    if(a.kind==Kind::Warden&&a.stance==1)s+="Броня сомкнута  ";
    if(a.kind==Kind::FallenKnight)s+=a.stance?"Стойка защиты  ":"Стойка натиска  ";
    if(a.enraged)s+="Ярость  ";
    for(size_t i=0;i<actors.size();++i)if(&actors[i]==&a&&shielded(int(i)))s+="Щит пилонов  ";
    return s.empty()?"Нет эффектов":s;
}
}
