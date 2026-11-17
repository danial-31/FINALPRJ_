// Campaign checks plus a scripted full playthrough for every class (no rendering).
#include "rpg.hpp"
#include <filesystem>
#include <fstream>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <stdexcept>

using namespace rpg;
using combat::Kind;
static void check(bool ok,const std::string& name){if(!ok)throw std::runtime_error(name);}

static int findEntity(Campaign& c,const std::string& id){
    for(size_t i=0;i<c.entities.size();++i)if(id==c.entities[i].def.id&&!c.entities[i].hidden&&!c.entities[i].done)return int(i);
    for(size_t i=0;i<c.entities.size();++i)if(id==c.entities[i].def.id)return int(i);
    return -1;
}
static void standNear(Campaign& c,world::Cell target){
    for(int r=1;r<6;++r)for(int dy=-r;dy<=r;++dy)for(int dx=-r;dx<=r;++dx){
        world::Cell n{target.x+dx,target.y+dy};
        if(!c.map.blocked(n.x,n.y)&&c.game.occupant(n)<0&&r>=3){c.game.place(0,n);return;}
    }
}
static void use(Campaign& c,const std::string& id){
    int i=findEntity(c,id);check(i>=0,"entity "+id);
    auto cells=c.interactCells(i);check(!cells.empty(),"reachable "+id);
    c.game.place(0,cells[0]);c.interact(i);c.update(0);
}
static void heal(Campaign& c){
    std::string why;
    for(int k=0;k<6&&c.hero().hp<c.hero().maxHp*55/100;++k){
        bool used=false;
        for(size_t i=0;i<c.inventory.size();++i){auto d=item(c.inventory[i].id);
            if(d&&d->type==ItemType::Consumable&&d->heal>=35){used=c.useItem(int(i),why);break;}}
        if(!used)break;
    }
}
// Real-time bot: leave marked areas, drink when hurt, use abilities, auto-attack the best target.
static bool fight(Campaign& c,int trigger,int& potionsUsed){
    auto& g=c.game;g.engage(trigger);
    const float dt=.05f;
    for(float t=0;t<300.f&&g.inBattle();t+=dt){
        auto& h=g.actors[0];
        if(h.hp<h.maxHp*40/100&&g.itemCooldown<=0){int before=h.hp;heal(c);if(h.hp>before)++potionsUsed;}
        bool danger=false;
        for(const auto& tg:g.telegraphs)if(tg.owner!=0&&tg.contains(h.pos))danger=true;
        for(const auto& hz:g.hazards)if(glm::length(hz.center-h.pos)<=hz.radius)danger=true;
        if(danger){
            if(!h.moving){
                glm::vec2 best=h.pos;float bestScore=1e9f;
                for(int k=0;k<16;++k)for(float r:{2.5f,4.f,5.5f}){float a=float(k)*0.3927f;glm::vec2 p=h.pos+glm::vec2(std::cos(a),std::sin(a))*r;
                    if(!g.walkablePos(0,p))continue;bool bad=false;
                    for(const auto& tg:g.telegraphs)if(tg.owner!=0&&tg.contains(p))bad=true;
                    for(const auto& hz:g.hazards)if(glm::length(hz.center-p)<=hz.radius+.5f)bad=true;
                    if(!bad&&r<bestScore){bestScore=r;best=p;}}
                g.moveTo(best);
            }
        }else{
            int target=-1;float score=1e9f;
            for(size_t i=1;i<g.actors.size();++i){const auto& e=g.actors[i];if(!e.alive()||!e.aggro)continue;
                float sc=glm::length(e.pos-h.pos)+(g.shielded(int(i))?1000.f:0.f)+(e.kind==Kind::Pylon?-50.f:0.f)+(e.vulnerable>0?-30.f:0.f);
                if(sc<score){score=sc;target=int(i);}}
            if(target>0){
                if(h.target!=target||(!h.moving&&h.target<=0))g.attackUnit(target);
                float d=glm::length(g.actors[size_t(target)].pos-h.pos);
                for(int s=0;s<4;++s){const auto& ab=combat::ability(c.heroClass,s);
                    if(ab.target==combat::Target::Point)continue;
                    if(ab.target==combat::Target::Self&&d>ab.radius&&!(s==2&&c.heroClass==combat::Class::Warrior))continue;
                    if(ab.target==combat::Target::Unit&&d>ab.range+1.f)continue;
                    if(g.castPreview(s,target,g.actors[size_t(target)].pos).allowed){g.cast(s,target,g.actors[size_t(target)].pos);break;}}
            }
        }
        g.update(dt);
        c.update(0);
    }
    c.update(0);
    return g.phase==combat::Phase::Explore&&g.actors[0].alive();
}
static void clearZone(Campaign& c,int& potions,bool bosses){
    for(int guard=0;guard<80;++guard){
        int target=-1;
        for(size_t i=1;i<c.game.actors.size();++i){const auto& e=c.game.actors[i];
            if(!e.alive()||e.kind==Kind::Pylon)continue;if(e.boss&&!bosses)continue;target=int(i);break;}
        if(target<0)return;
        standNear(c,c.game.actors[size_t(target)].cell);
        if(!fight(c,target,potions)){
            std::cerr<<"log:\n";size_t from=c.game.log.size()>30?c.game.log.size()-30:0;for(size_t k=from;k<c.game.log.size();++k)std::cerr<<"  "<<c.game.log[k]<<"\n";
            std::cerr<<"potions left "<<c.count("potion")+c.count("greater_potion")<<" maxhp "<<c.hero().maxHp<<" power "<<c.hero().power<<" dmgbonus "<<c.hero().damageBonus<<"\n";
            throw std::runtime_error(std::string(c.hero().alive()?"battle never ended in ":"hero died in ")+c.zoneId+" vs "+c.game.actors[size_t(target)].name+
                " at level "+std::to_string(c.hero().level));
        }
        for(auto bag=c.loot.begin();bag!=c.loot.end();bag=c.loot.begin()){c.game.place(0,bag->cell);c.onHeroStep();if(c.loot.empty())break;}
    }
}
static void travelTo(Campaign& c,const std::string& exitId){
    int i=findEntity(c,exitId);check(i>=0,"exit "+exitId);
    c.game.place(0,c.entities[size_t(i)].cell);std::string before=c.zoneId;c.onHeroStep();
    check(c.zoneId!=before,"travel through "+exitId+" from "+before);
}
static void restAt(Campaign& c){c.hero().hp=c.hero().maxHp;}
static void spendPoints(Campaign& c){std::string why;for(int t=0;t<3;++t)c.learnTalent(t,why);while(c.hero().skillPoints>0&&c.learnTalent(4,why)){}}

static void playthrough(combat::Class cls){
    Campaign c;c.newGame(cls);int potions=0;std::string why;
    check(c.zoneId=="road"&&c.zone().surface&&c.map.glyph(c.hero().cell.x,c.hero().cell.y)=='[',"starts on the forest road by the village");
    travelTo(c,"to_village");
    use(c,"elder");check(c.stage("main")==1,"elder starts the story");
    use(c,"merchant");use(c,"shrine_village");check(c.stage("main")==2,"supplies + shrine");
    use(c,"smith");check(c.stage("cargo")==1,"smith side quest");
    use(c,"herbalist");check(c.stage("antidote")==1,"herbalist side quest");
    travelTo(c,"to_road");clearZone(c,potions,false);
    use(c,"guard_journal");check(c.stage("guard")==1,"guard quest from note");
    use(c,"smith_cargo");use(c,"road_stash");
    travelTo(c,"to_grove");clearZone(c,potions,false);
    use(c,"guard_key");use(c,"miner_traces");check(c.stage("main")==3,"traces found");
    for(int k=0;k<3;++k){int h=findEntity(c,"herb");if(h>=0&&!c.entities[size_t(h)].done)use(c,"herb");}
    use(c,"shrine_grove");
    travelTo(c,"to_lair");clearZone(c,potions,false);
    use(c,"miner_survivor");use(c,"lair_cache");
    spendPoints(c);restAt(c);
    clearZone(c,potions,true);check(c.flags.count("boss_matriarch"),"matriarch defeated");
    c.chooseReward(int(cls));check(c.count(cls==combat::Class::Warrior?"chitin_blade":cls==combat::Class::Rogue?"queen_fangs":"silkweaver_staff")==1,"reward chosen");
    for(size_t i=0;i<c.inventory.size();++i)if(item(c.inventory[i].id)->rarity==Rarity::Rare&&item(c.inventory[i].id)->type==ItemType::Weapon){c.equipItem(int(i),why);break;}
    check(c.stage("main")==4,"stage 4 after rescue and boss");
    travelTo(c,"boss_exit");
    check(c.travel("shrine_village",why),"fast travel home");
    use(c,"elder");check(c.stage("main")==5,"seals explained");
    use(c,"smith");check(c.stage("cargo")==2,"cargo delivered");
    if(c.count("herb")>=3&&c.count("venom")>=2)use(c,"herbalist");
    travelTo(c,"to_road");travelTo(c,"to_mine");clearZone(c,potions,false);use(c,"shrine_mine");
    travelTo(c,"to_flooded");clearZone(c,potions,false);
    check(!c.check("flag:lever_a,flag:lever_b"),"gate closed before levers");
    use(c,"lever_a");use(c,"lever_b");
    int gate=findEntity(c,"pump_gate");check(c.entities[size_t(gate)].done,"gate opens after both levers");
    use(c,"flooded_cache");spendPoints(c);restAt(c);
    clearZone(c,potions,true);check(c.flags.count("boss_warden")&&c.stage("main")==6,"warden defeated");
    check(c.travel("shrine_village",why),"travel home 2");
    use(c,"smith");check(c.stage("main")==7&&c.count("climbing_gear")==1,"climbing gear");
    for(int k=0;k<2;++k)if(!c.upgradeWeapon(why))break;
    travelTo(c,"to_road");
    int slide=findEntity(c,"rockslide");check(c.entities[size_t(slide)].done,"rockslide cleared with gear");
    travelTo(c,"to_pass");clearZone(c,potions,false);use(c,"shrine_pass");use(c,"guard_cache");
    check(c.stage("guard")==3&&c.count("guard_amulet")==1,"guard cache opened");
    travelTo(c,"to_crypt");check(c.stage("main")==8,"crypt found");clearZone(c,potions,false);
    use(c,"crypt_key_chest");use(c,"crypt_door");
    use(c,"rune_sun");check(!c.flags.count("runes_solved"),"wrong rune order resets");
    use(c,"rune_moon");use(c,"rune_star");use(c,"rune_sun");check(c.flags.count("runes_solved"),"rune puzzle");
    use(c,"crypt_treasure");
    travelTo(c,"to_throne");spendPoints(c);restAt(c);
    clearZone(c,potions,true);check(c.flags.count("boss_knight")&&c.stage("main")==9,"knight defeated");
    check(c.seals()==3,"three seals");
    check(c.travel("shrine_village",why),"travel home 3");
    travelTo(c,"to_castle");clearZone(c,potions,false);use(c,"shrine_castle");
    travelTo(c,"to_heart");clearZone(c,potions,false);spendPoints(c);restAt(c);
    // equip the best talisman/armor
    for(size_t i=0;i<c.inventory.size();){auto d=item(c.inventory[i].id);
        if(d&&d->rarity==Rarity::Unique&&(d->type==ItemType::Armor||d->type==ItemType::Talisman)&&canEquip(*d,cls)){int slot=d->type==ItemType::Armor?1:2;auto cur=item(c.equip[slot].id);if(!cur||cur->rarity!=Rarity::Unique){c.equipItem(int(i),why);i=0;continue;}}++i;}
    clearZone(c,potions,true);check(c.flags.count("boss_keeper")&&c.stage("main")==10,"bone keeper defeated");
    check(c.travel("shrine_village",why),"travel home 4");
    use(c,"elder");check(c.stage("main")==11,"story complete");
    std::cout<<combat::className(cls)<<": level "<<c.hero().level<<", potions "<<potions<<", gold "<<c.gold
             <<", weapon +"<<c.equip[0].upgrade<<", stage "<<c.stage("main")<<'\n';
}

int main(){
    try{
        check(zones().size()==11,"eleven zones");
        for(const auto& z:zones()){auto m=buildMap(z);check(m.width>0&&m.height>0,std::string("map ")+z.id);}
        // inventory and economy rules
        Campaign c;c.newGame(combat::Class::Rogue);std::string why;
        c.addItem("mine_key");int key=-1;for(size_t i=0;i<c.inventory.size();++i)if(c.inventory[i].id=="mine_key")key=int(i);
        check(!c.sell(key,why),"quest items cannot be sold");
        c.addItem("ore",3);c.addItem("ore",2);int stacks=0;for(auto& s:c.inventory)stacks+=s.id=="ore";check(stacks==1&&c.count("ore")==5,"resources stack");
        c.addItem("chainmail");int mail=int(c.inventory.size())-1;check(c.equipItem(mail,why),"rogue wears chainmail");
        c.addItem("oak_staff");check(!c.equipItem(int(c.inventory.size())-1,why),"class restriction");
        int armor=c.hero().armor+c.hero().armorBonus;check(armor==1+3,"armor bonus applied");
        check(!c.canUpgrade(why),"upgrades locked before the warden");
        // save / load round trip
        auto dir=std::filesystem::temp_directory_path()/"ossuary_test";std::filesystem::create_directories(dir);
        c.gold=123;c.flags.insert("boss_matriarch");c.quests["main"]=4;
        check(c.save(dir/"slot.sav",why),"save: "+why);
        Campaign d;check(d.load(dir/"slot.sav",why),"load: "+why);
        check(d.gold==123&&d.flags.count("boss_matriarch")&&d.stage("main")==4&&d.count("ore")==5&&d.equip[1].id=="chainmail","round trip");
        check(&d.game.M()==&d.map,"map pointer survives load");
        auto info=Campaign::inspect(dir/"slot.sav");check(info.valid&&info.level==1&&info.zoneName=="Старая дорога","slot info");
        {std::ofstream bad(dir/"bad.sav");bad<<"OSSUARY-SAVE 1\nlevel=2\n";}
        Campaign e;check(!e.load(dir/"bad.sav",why),"truncated save rejected");
        // death rules
        c.gold=200;c.flags.insert("shrine:shrine_village");c.killed["road"].insert(1);c.respawnAtShrine();
        check(c.gold==180&&c.killed.empty()&&c.flags.count("boss_matriarch"),"respawn keeps progress, loses 10% gold");
        // saving is refused during battle
        Campaign b;b.newGame(combat::Class::Warrior);b.enterZone("road",'[');
        b.game.engage(1);
        if(b.game.inBattle())check(!b.save(dir/"x.sav",why),"no save in battle");

        playthrough(combat::Class::Warrior);
        playthrough(combat::Class::Rogue);
        playthrough(combat::Class::Mage);
        std::cout<<"Campaign: all checks passed\n";
        return 0;
    }catch(const std::exception& ex){std::cerr<<"FAILED: "<<ex.what()<<'\n';return 1;}
}
