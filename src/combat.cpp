#include "combat.hpp"
#include <algorithm>
#include <array>
#include <numeric>
#include <queue>

namespace combat {
const WeaponStats& weaponStats(Weapon weapon) {
    static const WeaponStats data[]={
        {"Sword", "Shield bash", 10,1,3}, {"Daggers", "Venom strike",8,1,2},
        {"Bow", "Pinning shot",9,6,3}, {"Staff", "Ember burst",10,5,3}};
    return data[static_cast<int>(weapon)];
}
const char* className(Class type) {
    static const char* names[]={"Warrior","Rogue","Mage"};
    return names[static_cast<int>(type)];
}
void Game::message(const std::string& text) {
    log.push_back(text);
    if(log.size()>8)log.erase(log.begin());
}
void Game::choose(Class type) {
    heroClass=type; floor=1; round=0; order.clear(); cursor=0; log.clear();
    Actor h; h.name=className(type); h.cell={10,12};
    if(type==Class::Warrior){h.maxHp=100;h.power=3;h.armor=3;h.speed=12;h.weapon=Weapon::Sword;}
    if(type==Class::Rogue){h.maxHp=76;h.power=2;h.armor=1;h.speed=16;h.maxAp=9;h.moveCost=1;h.weapon=Weapon::Daggers;}
    if(type==Class::Mage){h.maxHp=66;h.power=4;h.armor=1;h.speed=13;h.weapon=Weapon::Staff;}
    h.hp=h.maxHp;h.ap=h.maxAp;actors={h};checkpoint=h;spawnFloor();
    message("Click a tile to walk. B starts combat.");
}
void Game::spawnFloor() {
    actors.resize(1);actors[0].cell={10,12};
    actors[0].ap=actors[0].maxAp;actors[0].cooldown=0;actors[0].poison=0;
    actors[0].burn=0;actors[0].stun=0;actors[0].slow=0;actors[0].guard=0;
    auto add=[&](Kind kind,Cell cell,int hp,int armor,int speed,Weapon weapon,int power){
        Actor e;e.kind=kind;e.cell=cell;e.maxHp=hp;e.hp=hp;e.armor=armor;
        e.speed=speed;e.weapon=weapon;e.power=power;e.maxAp=5;e.ap=5;e.potions=0;
        const char* names[]={"Hero","Molerat","Slime","Skeleton","Ghost","Ogre"};
        e.name=names[static_cast<int>(kind)];actors.push_back(e);
    };
    if(floor==1){add(Kind::Rat,{8,8},22,0,10,Weapon::Daggers,-3);add(Kind::Slime,{12,7},28,1,6,Weapon::Sword,-4);add(Kind::Skeleton,{15,6},25,2,9,Weapon::Bow,-3);}
    if(floor==2){add(Kind::Ghost,{8,7},30,1,15,Weapon::Staff,-3);add(Kind::Skeleton,{14,6},32,3,11,Weapon::Bow,-2);add(Kind::Slime,{6,6},34,2,7,Weapon::Sword,-3);}
    if(floor==3){add(Kind::Ogre,{10,6},68,4,8,Weapon::Sword,3);add(Kind::Ghost,{6,7},34,1,15,Weapon::Staff,-2);add(Kind::Rat,{14,7},30,1,14,Weapon::Daggers,-1);}
    phase=Phase::Explore;order.clear();cursor=0;round=0;
    checkpoint=actors[0];
    message("Encounter "+std::to_string(floor)+" / 3. Checkpoint saved.");
}
int Game::active() const {
    if(phase!=Phase::Battle || order.empty())return -1;
    return order[size_t(cursor)];
}
bool Game::playerCanAct() const {
    return !actors.empty()&&actors[0].alive()&&(phase==Phase::Explore||phase==Phase::Victory||active()==0);
}
int Game::occupant(Cell p) const {
    for(size_t i=0;i<actors.size();++i)if(actors[i].alive()&&actors[i].cell==p)return int(i);
    return -1;
}
std::vector<Cell> Game::path(int id,Cell destination) const {
    if(id<0||id>=int(actors.size())||!actors[size_t(id)].alive()||layout::isBlocked(destination.x,destination.y))return {};
    const Cell start=actors[size_t(id)].cell;
    if(start==destination)return {};
    if(occupant(destination)>=0)return {};
    constexpr int count=layout::columns*layout::rows;
    std::array<int,count> previous;previous.fill(-1);
    auto index=[](Cell c){return c.y*layout::columns+c.x;};
    std::queue<Cell> queue;queue.push(start);previous[size_t(index(start))]=index(start);
    const Cell offsets[]={{1,0},{0,1},{-1,0},{0,-1}};
    while(!queue.empty()){
        Cell current=queue.front();queue.pop();
        if(current==destination)break;
        for(Cell d:offsets){
            Cell next{current.x+d.x,current.y+d.y};
            if(layout::isBlocked(next.x,next.y)||occupant(next)>=0||previous[size_t(index(next))]!=-1)continue;
            previous[size_t(index(next))]=index(current);queue.push(next);
        }
    }
    if(previous[size_t(index(destination))]<0)return {};
    std::vector<Cell> result;
    for(int i=index(destination);i!=index(start);i=previous[size_t(i)])result.push_back({i%layout::columns,i/layout::columns});
    std::reverse(result.begin(),result.end());return result;
}
bool Game::lineOfSight(Cell from,Cell to) const {
    if(layout::isBlocked(from.x,from.y)||layout::isBlocked(to.x,to.y))return false;
    // Supercover traversal: a ray through a corner cannot pass between two blockers.
    int dx=to.x-from.x,dy=to.y-from.y,nx=std::abs(dx),ny=std::abs(dy);
    int sx=dx>0?1:-1,sy=dy>0?1:-1,ix=0,iy=0;Cell p=from;
    while(ix<nx||iy<ny){
        int decision=(1+2*ix)*ny-(1+2*iy)*nx;
        if(decision==0){
            if(layout::isBlocked(p.x+sx,p.y)||layout::isBlocked(p.x,p.y+sy))return false;
            p.x+=sx;p.y+=sy;++ix;++iy;
        }else if(decision<0){p.x+=sx;++ix;}else{p.y+=sy;++iy;}
        if(layout::isBlocked(p.x,p.y))return false;
    }
    return true;
}
Preview Game::movePreview(Cell destination) const {
    Preview p;
    if(!playerCanAct()){p.reason="Wait for your turn";return p;}
    if(layout::isBlocked(destination.x,destination.y)){p.reason="Blocked by scenery";return p;}
    if(occupant(destination)>=0){p.reason="Tile occupied";return p;}
    auto route=path(0,destination);
    if(route.empty()){p.reason="No route";return p;}
    p.cost=phase==Phase::Battle?int(route.size())*actors[0].stepCost():0;
    if(p.cost>actors[0].ap){p.reason="Not enough AP";return p;}
    p.allowed=true;p.reason="Move "+std::to_string(route.size())+" tiles";return p;
}
Preview Game::attackFor(int source,int target,bool skill) const {
    Preview p;
    if(source<0||target<0||source>=int(actors.size())||target>=int(actors.size())||source==target){p.reason="Choose an enemy";return p;}
    const auto& a=actors[size_t(source)];const auto& b=actors[size_t(target)];
    if(!a.alive()||!b.alive()||(source!=0&&target!=0)){p.reason="Invalid target";return p;}
    auto w=weaponStats(a.weapon);p.cost=skill?4:w.cost;
    p.damage=std::max(1,w.damage+a.power+(skill?4:0)-b.defense());
    if(skill&&a.cooldown>0){p.reason="Skill cooling down: "+std::to_string(a.cooldown);return p;}
    if(distance(a.cell,b.cell)>w.range){p.reason="Out of range ("+std::to_string(w.range)+")";return p;}
    if(!lineOfSight(a.cell,b.cell)){p.reason="Scenery blocks line of sight";return p;}
    if(a.ap<p.cost){p.reason="Not enough AP";return p;}
    p.allowed=true;p.reason=skill?w.skill:"Attack";return p;
}
Preview Game::attackPreview(int target,bool skill) const {
    if(active()!=0)return {false,0,0,phase==Phase::Explore?"Press B to enter combat":"Wait for your turn"};
    return attackFor(0,target,skill);
}
bool Game::move(Cell destination) {
    auto p=movePreview(destination);if(!p.allowed){message(p.reason);return false;}
    // Combat commits one complete, affordable path; exploration is submitted one tile at a time by the view.
    actors[0].cell=destination;actors[0].ap-=p.cost;
    if(phase==Phase::Explore)for(size_t i=1;i<actors.size();++i)
        if(actors[i].alive()&&distance(destination,actors[i].cell)<=5&&lineOfSight(destination,actors[i].cell)){engage();break;}
    return true;
}
void Game::engage() {
    if(phase!=Phase::Explore)return;
    order.clear();for(size_t i=0;i<actors.size();++i)if(actors[i].alive())order.push_back(int(i));
    std::stable_sort(order.begin(),order.end(),[&](int a,int b){return actors[size_t(a)].initiative()>actors[size_t(b)].initiative();});
    cursor=0;round=1;phase=Phase::Battle;message("Combat! Initiative determines turn order.");beginTurn();
}
void Game::damage(int id,int amount) {
    auto& a=actors[size_t(id)];if(!a.alive())return;
    a.hp=std::max(0,a.hp-amount);
    if(!a.alive()){
        message(a.name+" defeated.");
        if(id!=0){
            auto& h=actors[0];int reward=a.kind==Kind::Ogre?70:35;h.xp+=reward;
            message("+"+std::to_string(reward)+" XP");
            while(h.xp>=h.level*90){h.xp-=h.level*90;++h.level;h.maxHp+=12;h.hp=std::min(h.maxHp,h.hp+20);h.power+=2;++h.armor;
                message("Level "+std::to_string(h.level)+": +12 HP, +2 power, +1 armor.");}
        }
    }
}
void Game::resolve() {
    if(!actors[0].alive()){phase=Phase::Defeat;message("Defeated. R restores this encounter's checkpoint.");return;}
    bool enemies=false;for(size_t i=1;i<actors.size();++i)enemies|=actors[i].alive();
    if(!enemies){
        phase=floor==3?Phase::Complete:Phase::Victory;
        actors[0].ap=actors[0].maxAp;actors[0].poison=actors[0].burn=actors[0].slow=actors[0].stun=0;
        ++actors[0].potions;message("Room cleared! +1 potion. "+std::string(floor<3?"N: next encounter.":"All three encounters complete."));
    }
}
bool Game::attack(int target,bool skill) {
    const int source=active();
    auto p=attackFor(source,target,skill);if(!p.allowed){message(p.reason);return false;}
    auto& a=actors[size_t(source)];a.ap-=p.cost;
    message(a.name+": "+p.reason+" -> "+actors[size_t(target)].name+" -"+std::to_string(p.damage)+" HP");
    damage(target,p.damage);
    if(skill){
        a.cooldown=3;
        auto& victim=actors[size_t(target)];
        if(victim.alive()){
            if(a.weapon==Weapon::Sword)victim.stun=1;
            if(a.weapon==Weapon::Daggers)victim.poison=2;
            if(a.weapon==Weapon::Bow)victim.slow=2;
            if(a.weapon==Weapon::Staff)victim.burn=2;
        }
        if(a.weapon==Weapon::Staff){
            Cell impact=victim.cell;
            for(size_t i=0;i<actors.size();++i)if(int(i)!=target&&int(i)!=source&&((source==0&&i>0)||(source>0&&i==0))&&actors[i].alive()&&distance(impact,actors[i].cell)==1&&lineOfSight(impact,actors[i].cell)){
                damage(int(i),std::max(1,p.damage/2));if(actors[i].alive())actors[i].burn=2;
            }
        }
    }
    resolve();return true;
}
bool Game::potion() {
    if(!playerCanAct())return false;
    auto& h=actors[0];int cost=phase==Phase::Battle?2:0;
    if(h.potions<=0){message("No potions left");return false;}
    if(h.hp==h.maxHp){message("Health already full");return false;}
    if(h.ap<cost){message("Not enough AP");return false;}
    h.ap-=cost;--h.potions;h.hp=std::min(h.maxHp,h.hp+35);message("Potion: restore up to 35 HP");return true;
}
bool Game::guard() {
    if(active()!=0)return false;
    auto& h=actors[0];if(h.ap<2||h.guard){message(h.guard?"Already guarding":"Not enough AP");return false;}
    h.ap-=2;h.guard=4;message("Guard: +4 defense until your next turn");return true;
}
bool Game::swapWeapon() {
    if(!playerCanAct())return false;
    auto& h=actors[0];int cost=phase==Phase::Battle?2:0;
    if(h.ap<cost){message("Not enough AP");return false;}
    h.ap-=cost;h.weapon=Weapon((int(h.weapon)+1)%4);message(std::string("Equipped ")+weaponStats(h.weapon).name);return true;
}
bool Game::swapArmor() {
    if(!playerCanAct())return false;
    if(phase==Phase::Battle){message("Change armor outside combat");return false;}
    actors[0].plated=!actors[0].plated;message(actors[0].plated?"Plate: +2 defense, -2 initiative":"Light armor equipped");return true;
}
void Game::beginTurn() {
    // Bounded loop skips dead or stunned actors, including deaths from damage over time.
    for(size_t skipped=0;skipped<=order.size();++skipped){
        if(phase!=Phase::Battle)return;
        auto& a=actors[size_t(active())];a.ap=a.maxAp;a.guard=0;
        a.cooldown=std::max(0,a.cooldown-1);
        if(a.poison>0){--a.poison;message(a.name+": poison -4 HP");damage(active(),4);}
        if(a.burn>0){--a.burn;message(a.name+": burning -5 HP");damage(active(),5);}
        resolve();if(phase!=Phase::Battle)return;
        if(a.alive()&&a.stun==0)return;
        if(a.stun>0){--a.stun;a.ap=0;message(a.name+" is stunned: turn skipped");}
        advanceTurn();
    }
}
void Game::advanceTurn() {
    do{cursor=(cursor+1)%int(order.size());if(cursor==0)++round;}while(!actors[size_t(order[size_t(cursor)])].alive());
}
void Game::endTurn() {
    if(phase!=Phase::Battle)return;
    auto& a=actors[size_t(active())];a.ap=0;if(a.slow>0)--a.slow;
    advanceTurn();beginTurn();
}
void Game::aiStep() {
    int id=active();if(id<=0)return;
    auto& a=actors[size_t(id)];
    if(a.cooldown==0&&attackFor(id,0,true).allowed){attack(0,true);return;}
    if(attackFor(id,0,false).allowed){attack(0);return;}
    if(a.ap<a.stepCost()){endTurn();return;}
    // ponytail: a 320-cell room permits BFS per candidate; cache a distance field for larger maps.
    std::vector<Cell> best;
    auto weapon=weaponStats(a.weapon);
    for(int y=1;y<layout::rows-1;++y)for(int x=1;x<layout::columns-1;++x){
        Cell candidate{x,y};
        if(layout::isBlocked(x,y)||occupant(candidate)>=0||distance(candidate,actors[0].cell)>weapon.range||!lineOfSight(candidate,actors[0].cell))continue;
        auto route=path(id,candidate);if(!route.empty()&&(best.empty()||route.size()<best.size()))best=std::move(route);
    }
    if(best.empty()){endTurn();return;}
    a.cell=best.front();a.ap-=a.stepCost();
}
void Game::nextFloor() {
    if(phase!=Phase::Victory)return;
    ++floor;actors[0].hp=std::min(actors[0].maxHp,actors[0].hp+25);spawnFloor();
}
void Game::restoreCheckpoint() {
    if(phase!=Phase::Defeat)return;
    actors={checkpoint};spawnFloor();message("Encounter checkpoint restored.");
}
std::vector<int> Game::turnOrder() const {
    std::vector<int> result;
    if(phase!=Phase::Battle)return result;
    for(size_t i=0;i<order.size();++i){int id=order[(size_t(cursor)+i)%order.size()];if(actors[size_t(id)].alive())result.push_back(id);}
    return result;
}
std::string Game::status(const Actor& a) const {
    std::string s;
    if(a.poison)s+="POISON "+std::to_string(a.poison)+" ";
    if(a.burn)s+="BURN "+std::to_string(a.burn)+" ";
    if(a.stun)s+="STUN "+std::to_string(a.stun)+" ";
    if(a.slow)s+="SLOW "+std::to_string(a.slow)+" ";
    if(a.guard)s+="GUARD ";
    return s.empty()?"No effects":s;
}
}
