#include "combat.hpp"
#include <cmath>
#include <iostream>
#include <stdexcept>

using namespace combat;
static void check(bool ok,const char* name){if(!ok)throw std::runtime_error(name);}
static void run(Game& g,float seconds){for(float t=0;t<seconds;t+=.05f)g.update(.05f);}
static Game duel(Class c,Kind kind,Cell enemy={10,9}){
    Game g;g.setupDemo(c);g.actors.resize(1);g.place(0,{10,11});
    g.actors.push_back(Game::makeEnemy(kind,enemy,g.M()));
    return g;
}
int main(){
    try{
        Game g;g.setupDemo(Class::Warrior);
        check(g.actors.size()==4&&g.phase==Phase::Explore,"demo setup");
        check(!g.path(0,{10,8}).empty(),"path exists");
        check(g.path(0,{9,2}).empty(),"no path into the altar");
        // movement goes around obstacles and reaches the goal
        glm::vec2 goal=g.world2({3,12});g.actors.resize(1);
        check(g.moveTo(goal),"move order accepted");run(g,8);
        check(glm::length(g.actors[0].pos-goal)<.3f,"hero walked to the clicked point");

        // aggro starts combat, auto-attacks kill a weak enemy
        g=duel(Class::Warrior,Kind::Rat);run(g,.2f);
        check(g.inBattle()&&g.actors[1].aggro,"enemy notices the hero");
        g.attackUnit(1);run(g,6);
        check(!g.actors[1].alive()&&g.phase==Phase::Explore,"auto attacks kill the rat and end the battle");
        check(g.actors[0].xp>0,"xp granted");

        // abilities: cost mana, start cooldown, apply effects
        g=duel(Class::Warrior,Kind::Golem);g.actors[1].maxHp=g.actors[1].hp=500;
        float mana=g.actors[0].mana;
        check(g.cast(0,1,{}),"shield bash cast");
        check(g.actors[0].mana<mana&&g.actors[0].cd[0]>0&&g.actors[1].stun>0,"bash costs mana, cools down, stuns");
        check(!g.castPreview(0,1,{}).allowed,"cooldown blocks a second cast");
        check(!g.castPreview(3,1,{}).allowed,"ultimate locked before level 6");
        g=duel(Class::Rogue,Kind::Golem,{10,10});g.actors[1].maxHp=g.actors[1].hp=500;
        check(g.cast(0,1,{})&&g.actors[1].poison>0,"venom strike poisons");
        int hp=g.actors[1].hp;run(g,1.f);check(g.actors[1].hp<hp,"poison ticks over time");
        g=duel(Class::Mage,Kind::Golem,{10,5});g.actors[1].maxHp=g.actors[1].hp=500;
        check(g.cast(0,1,{}),"fire bolt cast at range");run(g,1.5f);
        check(g.actors[1].hp<500&&g.actors[1].burn>0,"projectile hits and burns");

        // walls and tall props block sight
        check(!g.lineOfSight({9,5},{9,0}),"walls block sight");

        // boss marked attack: stepping out of the area avoids it, the boss is left open
        g=duel(Class::Warrior,Kind::Warden,{10,8});g.actors[1].maxHp=g.actors[1].hp=2000;
        for(int i=0;i<60&&g.telegraphs.empty();++i)run(g,.1f);
        check(!g.telegraphs.empty(),"warden marks an attack on the floor");
        auto t=g.telegraphs.front();
        glm::vec2 away=g.actors[0].pos+glm::normalize(g.actors[0].pos-g.actors[1].pos)*4.f;
        g.moveTo(away);int before=g.actors[0].hp;run(g,t.time+.1f);
        check(g.actors[0].hp>=before-g.actors[1].power||!t.contains(g.actors[0].pos),"leaving the marked area avoids the big hit");
        check(g.actors[1].vulnerable>0,"heavy attack leaves the boss vulnerable");

        // ghosts drift through props
        g=Game{};g.setupDemo(Class::Mage);g.actors.resize(1);g.actors.push_back(Game::makeEnemy(Kind::Ghost,{10,5},g.M()));
        check(g.walkable(1,{9,2})&&!g.walkable(0,{9,2}),"ghost passes the altar, hero does not");

        // leash: an enemy dragged far from home gives up and heals
        g=duel(Class::Rogue,Kind::Wolf,{3,12});g.engage(1);g.actors[1].hp=5;g.actors[1].homePos=g.world2({18,1});
        g.place(0,{17,12});run(g,.5f);check(!g.actors[1].aggro&&g.actors[1].hp==g.actors[1].maxHp,"leashed enemy resets");

        std::cout<<"Real-time combat: all checks passed\n";
        return 0;
    }catch(const std::exception& e){std::cerr<<"FAILED: "<<e.what()<<'\n';return 1;}
}
