#include "combat.hpp"
#include <algorithm>
#include <iostream>
#include <stdexcept>

using namespace combat;
void check(bool ok,const char* name){if(!ok)throw std::runtime_error(name);}
Game duel(Weapon weapon=Weapon::Sword){
    Game g;g.choose(Class::Warrior);g.actors.resize(2);
    g.actors[0].cell={10,10};g.actors[0].weapon=weapon;
    g.actors[1].cell={10,9};g.actors[1].hp=g.actors[1].maxHp=100;
    g.engage();return g;
}
int main(){
    try{
        Game g;check(g.phase==Phase::ChooseClass,"class selection first");
        for(Class type:{Class::Warrior,Class::Rogue,Class::Mage}){
            g.choose(type);check(g.actors.size()==4&&g.phase==Phase::Explore,"three enemies per encounter");
            for(const auto& a:g.actors){check(!layout::isBlocked(a.cell.x,a.cell.y),"valid spawn");check(g.occupant(a.cell)>=0,"occupancy");}
            for(int y=1;y<layout::rows-1;++y)for(int x=1;x<layout::columns-1;++x){
                Cell dest{x,y};auto route=g.path(0,dest);Cell previous=g.actors[0].cell;
                for(auto step:route){check(distance(previous,step)==1&&!layout::isBlocked(step.x,step.y)&&g.occupant(step)<0,"path uses free cardinal neighbors");previous=step;}
                if(!route.empty())check(previous==dest,"path reaches requested tile");
                check(g.lineOfSight({10,12},dest)==g.lineOfSight(dest,{10,12}),"line of sight symmetric");
            }
            check(g.path(0,{4,9}).empty()&&g.path(0,{-1,2}).empty(),"scenery and outside blocked");
            check(g.path(0,g.actors[1].cell).empty(),"occupied tile blocked");
            check(!g.lineOfSight({3,9},{7,9}),"cover blocks ranged attacks");
            check(!g.lineOfSight({3,8},{5,10}),"cannot shoot through obstacle corner");
            int ap=g.actors[0].ap;check(g.move({10,13})&&g.actors[0].ap==ap,"exploration costs no AP");
            g.engage();check(g.active()==0,"hero initiative");
            auto far=g.movePreview({1,1});check(!far.allowed&&far.cost>g.actors[0].ap,"long path rejected");
            Cell original=g.actors[0].cell;check(!g.move({1,1})&&g.actors[0].cell==original,"failed movement atomic");
            auto p=g.movePreview({10,12});check(p.allowed&&g.move({10,12})&&g.actors[0].ap==ap-p.cost,"movement spends exact AP");
        }
        g=duel();int before=g.actors[1].hp;
        auto p=g.attackPreview(1);check(p.allowed&&g.attack(1)&&g.actors[1].hp==before-p.damage,"preview matches damage");
        check(g.actors[0].ap==g.actors[0].maxAp-p.cost,"attack costs AP");
        g.actors[0].ap=0;before=g.actors[1].hp;check(!g.attack(1)&&g.actors[1].hp==before,"no free attack");
        g=duel(Weapon::Bow);g.actors[0].cell={3,9};g.actors[1].cell={7,9};
        check(!g.attackPreview(1).allowed,"ranged attack blocked by cover");
        g=duel();check(g.attack(1,true)&&g.actors[1].stun==1,"bash applies stun");
        check(!g.attack(1,true),"cooldown prevents skill spam");g.endTurn();
        check(g.active()==0&&g.round==2&&g.actors[1].stun==0,"stunned enemy skipped");
        check(g.actors[0].cooldown==2,"cooldown ticks on owner's turn");
        g=duel(Weapon::Daggers);check(g.attack(1,true),"poison skill");before=g.actors[1].hp;g.endTurn();
        check(g.actors[1].hp==before-4&&g.actors[1].poison==1,"poison tick and duration");
        g.endTurn();g.endTurn();check(g.actors[1].poison==0,"poison expires");
        g=duel(Weapon::Staff);check(g.attack(1,true),"fire skill");before=g.actors[1].hp;g.endTurn();check(g.actors[1].hp==before-5,"burn tick");
        g=duel(Weapon::Bow);g.attack(1,true);g.endTurn();check(g.actors[1].stepCost()==3,"slow raises movement cost");
        g=duel();check(!g.potion()&&g.actors[0].potions==3,"full HP does not consume potion");
        g.actors[0].hp-=20;check(g.potion()&&g.actors[0].hp==g.actors[0].maxHp&&g.actors[0].potions==2,"healing capped");
        check(g.guard()&&g.actors[0].defense()==7,"guard defense");check(!g.guard(),"guard cannot stack");
        g.endTurn();g.endTurn();check(g.actors[0].guard==0,"guard expires next turn");
        g=duel();int cost=g.actors[0].ap;check(g.swapWeapon()&&g.actors[0].ap==cost-2,"equipment costs AP");check(!g.swapArmor(),"armor locked during combat");
        g=duel();g.actors[1].hp=1;check(g.attack(1)&&g.phase==Phase::Victory,"last enemy ends combat");
        check(g.turnOrder().empty()&&g.occupant(g.actors[1].cell)<0,"dead removed from queue and occupancy");
        check(g.actors[0].xp==35&&g.actors[0].potions==4,"rewards once");
        g.nextFloor();check(g.floor==2&&g.phase==Phase::Explore&&g.actors[0].xp==35,"progress persists between encounters");
        int savedHp=g.actors[0].hp;g.engage();g.actors[0].hp=1;g.actors[0].poison=1;
        // Give hero a turn so poison causes death, then reload the encounter start.
        while(g.active()!=0&&g.phase==Phase::Battle)g.endTurn();
        if(g.phase==Phase::Battle){g.endTurn();while(g.active()!=0&&g.phase==Phase::Battle)g.endTurn();}
        check(g.phase==Phase::Defeat,"status damage can defeat hero");g.restoreCheckpoint();
        check(g.phase==Phase::Explore&&g.floor==2&&g.actors[0].hp==savedHp&&g.actors[0].poison==0,"checkpoint restores progress and health");
        g=duel();g.actors[1].hp=1;g.actors[1].poison=1;g.endTurn();check(g.phase==Phase::Victory,"status kill ends battle without stuck turn");
        // Full encounter: a deterministic player policy and the actual AI must terminate.
        for(Class type:{Class::Warrior,Class::Rogue,Class::Mage}){
            g.choose(type);g.engage();int steps=0;
            while((g.phase==Phase::Battle||g.phase==Phase::Victory)&&steps++<1800){
                if(g.phase==Phase::Victory){g.nextFloor();g.engage();continue;}
                if(g.active()!=0){int actor=g.active(),ap=g.actors[size_t(actor)].ap,round=g.round;g.aiStep();
                    check(g.actors[size_t(actor)].ap>=0,"AI AP never negative");
                    if(g.active()==actor&&g.round==round)check(g.actors[size_t(actor)].ap<ap,"AI spends AP or ends turn");
                    continue;
                }
                auto& h=g.actors[0];bool acted=false;
                if(h.hp<=h.maxHp-30)acted=g.potion();
                std::vector<int> targets;for(size_t i=1;i<g.actors.size();++i)if(g.actors[i].alive())targets.push_back(int(i));
                std::stable_sort(targets.begin(),targets.end(),[&](int a,int b){return g.actors[size_t(a)].hp<g.actors[size_t(b)].hp;});
                if(!acted)for(int i:targets)if(g.attackPreview(i,true).allowed){g.attack(i,true);acted=true;break;}
                if(!acted)for(int i:targets)if(g.attackPreview(i).allowed){g.attack(i);acted=true;break;}
                if(!acted){
                    int nearest=1000;for(size_t i=1;i<g.actors.size();++i)if(g.actors[i].alive())nearest=std::min(nearest,distance(h.cell,g.actors[i].cell));
                    int best=nearest*10;Cell next;
                    for(int y=1;y<layout::rows-1;++y)for(int x=1;x<layout::columns-1;++x){Cell c{x,y};auto move=g.movePreview(c);if(!move.allowed)continue;
                        for(size_t i=1;i<g.actors.size();++i)if(g.actors[i].alive()){
                            int score=distance(c,g.actors[i].cell)*10+move.cost;
                            if(score<best){best=score;next=c;}
                        }
                    }
                    if(next.x>=0&&nearest>weaponStats(h.weapon).range)acted=g.move(next);
                }
                if(!acted&&h.ap>=2&&h.guard==0)acted=g.guard();
                if(!acted)g.endTurn();
            }
            check(steps<1800,"combat must terminate");
            if(g.phase!=Phase::Complete){std::cerr<<className(type)<<" stopped at encounter "<<g.floor<<" HP "<<g.actors[0].hp<<" level "<<g.actors[0].level<<" potions "<<g.actors[0].potions<<'\n';for(const auto& line:g.log)std::cerr<<line<<'\n';}
            check(g.phase==Phase::Complete,"each starting class can complete all encounters");
            check(g.actors[0].level>=3,"kills grant levels and stats");
        }
        std::cout<<"Combat checks passed: grid, LoS, AP, initiative, skills, effects, AI, rewards and checkpoint.\n";
        return 0;
    }catch(const std::exception& e){std::cerr<<"FAIL: "<<e.what()<<'\n';return 1;}
}
