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
static Game arena(Class c){
    Game g;g.ownMap.resize(20,20);g.heroClass=c;g.actors={Game::makeHero(c)};g.place(0,{5,5});return g;
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
        // Isolate the announced attack from ordinary swings and subsequent boss abilities.
        g.actors[1].stun=10.f;g.events.clear();
        glm::vec2 away=g.actors[0].pos+glm::normalize(g.actors[0].pos-g.actors[1].pos)*4.f;
        g.moveTo(away);int before=g.actors[0].hp;run(g,t.time+.1f);
        check(!t.contains(g.actors[0].pos),"hero actually left the marked area");
        check(g.actors[0].hp==before,"dodged attack deals no damage");
        bool missed=false;for(const auto& e:g.events)missed|=e.type==Event::Miss&&e.source==1;
        check(missed,"dodged attack emits a miss");
        check(g.actors[1].vulnerable>0,"heavy attack leaves the boss vulnerable");

        // Lethal impacts clear the remaining projectiles without invalidating the update loop.
        for(Effect effect:{Effect::None,Effect::Burn,Effect::Stun}){
            g=arena(Class::Warrior);g.actors[0].hp=1;g.phase=Phase::Battle;
            g.actors.push_back(Game::makeEnemy(Kind::BanditArcher,{15,15},g.M()));g.actors[1].stun=100;
            g.projectiles.push_back({g.actors[0].pos,1,0,11,10,effect,0,false});
            g.projectiles.push_back({g.world2({10,5}),1,0,11,10,Effect::None,0,false});
            g.update(.05f);
            check(g.phase==Phase::Defeat&&g.actors[0].hp==0&&g.projectiles.empty(),"lethal projectile resolves safely");
            int deaths=0;for(const auto& e:g.events)deaths+=e.type==Event::Death&&e.target==0;
            check(deaths==1&&g.actors[0].burn==0&&g.actors[0].stun==0,"death emitted once with no post-death effects");
        }

        // The pylon shield applies to every damage entry point, including skill bonuses and DOT.
        for(Class cls:{Class::Warrior,Class::Rogue,Class::Mage}){
            g=arena(cls);g.actors.push_back(Game::makeEnemy(Kind::BoneKeeper,{7,5},g.M()));
            g.actors.push_back(Game::makeEnemy(Kind::Pylon,{15,15},g.M()));g.actors[1].stun=100;
            int bossHp=g.actors[1].hp;
            check(g.shielded(1)&&g.attackDamage(0,1)==0,"pylon blocks ordinary attack");
            check(g.cast(0,1,{}),"skill can target the shielded boss");run(g,1.f);
            check(g.actors[1].hp==bossHp&&g.actors[1].burn==0&&g.actors[1].poison==0,"shield blocks skill damage and effects");
            g.actors[1].poison=g.actors[1].burn=2;run(g,.5f);
            check(g.actors[1].hp==bossHp,"shield blocks existing damage over time");
            g.damage(2,g.actors[2].hp,0);g.damage(1,10,0);
            check(!g.shielded(1)&&g.actors[1].hp==bossHp-10,"destroying the last pylon permits damage");
        }

        // A direct skill approaches around a wall, spending resources only at a valid attack position.
        g=arena(Class::Warrior);g.actors.push_back(Game::makeEnemy(Kind::Golem,{7,5},g.M()));g.actors[1].stun=100;
        g.M().set(6,5,world::Tile::Wall);hp=g.actors[1].hp;mana=g.actors[0].mana;
        check(!g.lineOfSight({5,5},{7,5})&&g.cast(0,1,{}),"obstructed skill queues an approach");
        check(g.actors[1].hp==hp&&g.actors[0].mana==mana&&g.pendingSlot==0,"no damage or mana cost through a wall");
        run(g,4.f);
        check(g.pendingSlot<0&&g.actors[1].hp<hp&&g.actors[0].cd[0]>0,"queued skill finds a visible reachable position");

        // An unreachable target must neither consume mana nor trigger a failed A* search every frame.
        g=arena(Class::Warrior);g.actors.push_back(Game::makeEnemy(Kind::Golem,{7,5},g.M()));g.actors[1].stun=100;
        for(int y=0;y<g.M().height;++y)g.M().set(6,y,world::Tile::Wall);
        mana=g.actors[0].mana;
        check(!g.cast(0,1,{})&&g.pendingSlot<0&&g.actors[0].mana==mana,"unreachable skill cancels without spending mana");
        g.attackUnit(1);g.update(.01f);float retry=g.actors[0].repath;g.update(.01f);
        check(!g.actors[0].moving&&g.actors[0].repath<retry,"failed chase respects the retry timer");

        // Obstacles are checked along a projectile's whole movement, even if both endpoints are free.
        g=arena(Class::Mage);g.actors.push_back(Game::makeEnemy(Kind::Golem,{9,5},g.M()));g.actors[1].stun=100;
        g.M().set(7,5,world::Tile::Wall);hp=g.actors[1].hp;
        g.projectiles.push_back({g.actors[0].pos,0,1,1000,50,Effect::Burn,0,true});g.update(.05f);
        check(g.projectiles.empty()&&g.actors[1].hp==hp&&g.actors[1].burn==0,"fast projectile cannot tunnel through a wall");
        g.M().set(7,5,world::Tile::Water);
        g.projectiles.push_back({g.actors[0].pos,0,1,1000,10,Effect::None,0,true});g.update(.05f);
        check(g.actors[1].hp==hp-10,"water blocks movement but not projectiles");

        // A clear ray skimming a corner must stay clear when split into frame-sized segments.
        g=arena(Class::Mage);g.actors[0].pos+=glm::vec2(.63f,.42f);
        g.actors.push_back(Game::makeEnemy(Kind::Golem,{7,6},g.M()));g.actors[1].stun=100;
        g.M().set(5,6,world::Tile::Wall);hp=g.actors[1].hp;
        check(g.cast(0,1,{})&&g.pendingSlot<0,"clear corner shot launches without an unnecessary approach");run(g,.5f);
        check(g.actors[1].hp<hp,"projectile does not collide with an untraversed adjacent cell");

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
