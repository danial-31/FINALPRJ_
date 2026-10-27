#pragma once
#include "scene_layout.hpp"
#include <string>
#include <vector>

namespace combat {
struct Cell {
    int x=-1, y=-1;
    bool operator==(Cell b) const { return x==b.x && y==b.y; }
    bool operator!=(Cell b) const { return !(*this==b); }
};
inline int distance(Cell a, Cell b) { return std::abs(a.x-b.x)+std::abs(a.y-b.y); }
inline glm::vec3 world(Cell p) { return layout::center({p.x,p.y,1,1}); }
enum class Class { Warrior, Rogue, Mage };
enum class Weapon { Sword, Daggers, Bow, Staff };
enum class Kind { Hero, Rat, Slime, Skeleton, Ghost, Ogre };
enum class Phase { ChooseClass, Explore, Battle, Victory, Defeat, Complete };
struct WeaponStats { const char* name; const char* skill; int damage, range, cost; };
const WeaponStats& weaponStats(Weapon weapon);
const char* className(Class type);
struct Actor {
    std::string name;
    Kind kind=Kind::Hero;
    Cell cell;
    int hp=1, maxHp=1, power=0, armor=0, speed=0, maxAp=8, ap=8, moveCost=2;
    int level=1, xp=0, potions=3, cooldown=0, poison=0, burn=0, stun=0, slow=0, guard=0;
    bool plated=false;
    Weapon weapon=Weapon::Sword;
    bool alive() const { return hp>0; }
    int defense() const { return armor+(plated?2:0)+guard; }
    int initiative() const { return speed-(plated?2:0); }
    int stepCost() const { return moveCost+(slow>0?1:0); }
};
struct Preview {
    bool allowed=false;
    int cost=0, damage=0;
    std::string reason;
};

// Pure turn rules; the renderer only requests actions and reads this state.
class Game {
public:
    std::vector<Actor> actors;
    Phase phase=Phase::ChooseClass;
    Class heroClass=Class::Warrior;
    int floor=1, round=0;
    std::vector<std::string> log;
    void choose(Class type);
    void engage();
    int active() const;
    bool playerCanAct() const;
    int occupant(Cell p) const;
    std::vector<Cell> path(int actor, Cell destination) const;
    bool lineOfSight(Cell from, Cell to) const;
    Preview movePreview(Cell destination) const;
    Preview attackPreview(int target, bool skill=false) const;
    bool move(Cell destination);
    bool attack(int target, bool skill=false);
    bool potion();
    bool guard();
    bool swapWeapon();
    bool swapArmor();
    void endTurn();
    void aiStep();
    void nextFloor();
    void restoreCheckpoint();
    std::vector<int> turnOrder() const;
    std::string status(const Actor& actor) const;
    void message(const std::string& text);
private:
    std::vector<int> order;
    int cursor=0;
    Actor checkpoint;
    void spawnFloor();
    void beginTurn();
    void advanceTurn();
    void resolve();
    void damage(int target, int amount);
    Preview attackFor(int source, int target, bool skill) const;
};
}
