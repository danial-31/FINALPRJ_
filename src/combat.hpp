#pragma once
#include "world.hpp"
#include <glm/glm.hpp>
#include <string>
#include <vector>

// Real-time action combat in the style of an action RPG / MOBA:
// right-click to move or attack, abilities on Q W E R with mana and cooldowns,
// boss attacks announced by filling floor markers that the player must leave.
namespace combat {
using world::Cell;
inline int distance(Cell a, Cell b) { return world::manhattan(a,b); }
enum class Class { Warrior, Rogue, Mage };
enum class Weapon { Sword, Daggers, Bow, Staff };
enum class Kind {
    Hero, Rat, Slime, Skeleton, Ghost, Ogre, Wolf, Bandit, BanditArcher,
    Spiderling, VenomSpider, SpiderGuard, Miner, Golem, SkeletonArcher, KnightGuard,
    Matriarch, Warden, FallenKnight, BoneKeeper, Pylon, Count
};
enum class Phase { Explore, Battle, Defeat };
enum class Effect { None, Poison, Slow, Stun, Burn, Web };
enum Talent : int { T1=1, T2=2, T3=4 };

struct WeaponStats { const char* name; int damage; float range, interval; bool projectile; };
const WeaponStats& weaponStats(Weapon weapon);
const char* className(Class type);
const char* talentName(Class type,int tier);
const char* talentText(Class type,int tier);

enum class Target { Self, Unit, Point };
struct Ability { const char* name; const char* text; int mana; float cooldown, range, radius; Target target; int level; };
const Ability& ability(Class type,int slot);    // slot 0..3 -> Q W E R

struct KindInfo {
    const char* name;
    int hp, armor; float speed; int damage; float range, interval; int xp;
    Effect onSpecial; bool spirit, boss, ranged;
    const char* trait;
};
const KindInfo& kindInfo(Kind kind);

struct Actor {
    std::string name;
    Kind kind=Kind::Hero;
    Cell cell, home;
    glm::vec2 pos{0}, homePos{0};
    float facing=0, radius=.42f;
    int hp=1, maxHp=1, power=0, armor=0;
    float mana=0, maxMana=0, manaRegen=0;
    float speed=4.f, attackTimer=0, swing=0;
    int level=1, xp=0, skillPoints=0, talents=0;
    int damageBonus=0, rangeBonus=0, armorBonus=0, initiativeBonus=0;
    float poison=0, burn=0, slow=0, stun=0, web=0, guardTime=0, vulnerable=0, empowered=0;
    int poisonDps=4, burnDps=5, guardArmor=0;
    float cd[4]{};
    bool boss=false, spirit=false, aggro=false, participant=false;
    int spawn=-1, group=0, owner=-1, xpReward=0, target=-1;
    std::vector<glm::vec2> path;
    glm::vec2 goal{0}; bool moving=false;
    float repath=0, think=0, special=0, leash=0, hitFlash=0, dot=0;
    float timers[4]{};
    int stance=0, summonMarks=0;
    bool enraged=false, halfTriggered=false, phaseTwo=false;
    Weapon weapon=Weapon::Sword;
    bool alive() const { return hp>0; }
    int initiative() const { return initiativeBonus; }
};
struct Preview { bool allowed=false; int damage=0; std::string reason; };
// A boss attack: the marked area fills up and strikes when the timer runs out.
struct Telegraph {
    int owner=-1, shape=0;          // 0 circle, 1 line (rectangle from center along dir)
    glm::vec2 center{0}, dir{1,0};
    float radius=1, length=0, width=0, time=0, total=1;
    int damage=0; Effect effect=Effect::None; bool heavy=false, hazard=false, charge=false;
    std::string label;
    bool contains(glm::vec2 p) const;
};
struct Hazard { glm::vec2 center; float radius, time; int dps; };
struct Projectile { glm::vec2 pos; int source, target; float speed; int damage; Effect effect; float splash; bool hero; };
struct Event {
    enum Type { Hit, Death, Summon, Heal, Status, Warn, Vulnerable, PhaseTwo, BattleStart, BattleEnd, Trap, LevelUp, Miss, Step, Attack, Cast } type;
    int source=-1, target=-1, amount=0;
    Cell cell;
    std::string text;
};

class Game {
public:
    world::Map ownMap;
    world::Map* ext=nullptr;
    world::Map& M(){ return ext?*ext:ownMap; }
    const world::Map& M() const { return ext?*ext:ownMap; }
    std::vector<Actor> actors;
    std::vector<Telegraph> telegraphs;
    std::vector<Hazard> hazards;
    std::vector<Projectile> projectiles;
    std::vector<Event> events;
    Phase phase=Phase::Explore;
    Class heroClass=Class::Warrior;
    unsigned rng=12345;
    float time=0, itemCooldown=0;
    int pendingSlot=-1, pendingTarget=-1; glm::vec2 pendingPoint{0};   // cast waiting for the hero to get in range
    std::vector<std::string> log;
    Game();
    void setMap(world::Map* m){ ext=m; }
    static Actor makeHero(Class type);
    static Actor makeEnemy(Kind kind, Cell cell, const world::Map& map);
    void setupDemo(Class type);
    void place(int id, Cell c);                     // put an actor on a cell centre
    void update(float dt);
    // hero orders
    bool moveTo(glm::vec2 point);
    bool attackUnit(int target);
    void stop();
    Preview castPreview(int slot, int target, glm::vec2 point) const;
    bool cast(int slot, int target, glm::vec2 point);
    bool abilityUnlocked(int slot) const;
    void engage(int trigger);
    bool inBattle() const { return phase==Phase::Battle; }
    bool playerCanAct() const { return !actors.empty()&&actors[0].alive()&&actors[0].stun<=0; }
    int occupant(Cell p) const;
    bool walkable(int id, Cell c) const;
    bool walkablePos(int id, glm::vec2 p) const;
    std::vector<Cell> path(int actor, Cell destination, bool diagonal=true) const;
    bool lineOfSight(Cell from, Cell to) const { return M().lineOfSight(from,to); }
    int defense(int id) const;
    int attackDamage(int source, int target, float multiplier=1) const;
    float attackRange(const Actor& a) const;
    float attackInterval(const Actor& a) const;
    float moveSpeed(const Actor& a) const;
    std::string status(const Actor& a) const;
    bool shielded(int id) const;
    void clearStatuses(Actor& a);
    void grantXp(int amount);
    void message(const std::string& text);
    unsigned random(unsigned n){ rng=rng*1664525u+1013904223u; return n?(rng>>8)%n:0; }
    float randomf(){ return float(random(10000))/10000.f; }
    int summon(Kind kind, glm::vec2 near, int owner);
    void damage(int target, int amount, int source=-1, bool reflect=true);
    int enemyAt(glm::vec2 p, float radius) const;
    glm::vec2 world2(Cell c) const { auto v=M().center(c); return {v.x,v.z}; }
    Cell cellOf(glm::vec2 p) const { return M().cellAt({p.x,0,p.y}); }
private:
    void applyEffect(int target, Effect e, int source, float seconds=0);
    void updateActor(int id, float dt);
    void enemyAi(int id, float dt);
    void bossAi(int id, float dt);
    void steer(int id, float dt);
    void planPath(int id, glm::vec2 goal);
    void strike(int source, int target, float multiplier, Effect effect=Effect::None, float splash=0);
    void resolveTelegraph(const Telegraph& t);
    void telegraph(int owner, Telegraph t);
    void separate();
    void refreshPhase();
    bool clearLine(glm::vec2 a, glm::vec2 b, int id) const;
    int aliveSummons(int owner) const;
};
}
