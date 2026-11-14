#pragma once
#include "combat.hpp"
#include <deque>
#include <filesystem>
#include <map>
#include <set>
#include <string>
#include <vector>

// Campaign rules: zones, items, quests, NPCs, shrines and saves. No OpenGL here.
namespace rpg {
using world::Cell;

enum class ItemType { Weapon, Armor, Talisman, Consumable, Resource, Quest };
enum class Rarity { Common, Uncommon, Rare, Unique };
struct ItemDef {
    const char* id; const char* name; ItemType type; Rarity rarity; int classMask; combat::Weapon weapon;
    int damage, armor, hp, initiative, range, price, heal;
    const char* desc;
};
const ItemDef* item(const std::string& id);
const std::vector<ItemDef>& items();
const char* rarityName(Rarity r);
const char* typeName(ItemType t);
bool canEquip(const ItemDef& d, combat::Class c);
int sellPrice(const ItemDef& d, int upgrade=0);

struct Stack { std::string id; int count=1; int upgrade=0; };

enum class EntityType { Npc, Shrine, Chest, Exit, Entrance, Arrive, Lever, Gate, Cocoon, Ore, Note, Herb,
                        Survivor, Rune, Cargo, Clue, KeyCache, RuneDoor, LockDoor };
struct EntityDef { char anchor; EntityType type; const char* id; const char* arg; const char* cond; const char* label; const char* msg; };
struct SpawnDef { char anchor; combat::Kind kind; int group; };
struct ZoneDef {
    const char* id; const char* name; int biome; bool surface; const char* music; const char* region;
    std::vector<const char*> rows; std::vector<EntityDef> entities; std::vector<SpawnDef> spawns;
};
const std::vector<ZoneDef>& zones();
const ZoneDef* zoneDef(const std::string& id);
// Builds the tile map of a zone (the throne crypt reuses the original painted room).
world::Map buildMap(const ZoneDef& z);

struct Entity { EntityDef def; Cell cell; bool done=false; bool hidden=false; };
struct LootBag { Cell cell; std::vector<Stack> items; int gold=0; float age=0; };
struct Toast { std::string text; int rarity=0; float time=0; };
struct DialogOption { std::string label, action; };
struct Dialog { std::string speaker; std::vector<std::string> lines; std::vector<DialogOption> options; };
struct Recipe { const char* id; const char* result; int count; std::vector<std::pair<const char*,int>> needs; int gold; const char* cond; };
const std::vector<Recipe>& recipes();
struct UpgradeCost { int gold, ore; const char* extra; int extraCount; };
UpgradeCost upgradeCost(int level);   // cost of reaching `level` (1..3)
constexpr int maxUpgrade=3;

enum class Request { Dialog, Shop, Craft, Smith, Shrine, Note, RewardChoice, Epilogue, Defeat, ZoneChanged, Credits };
struct QuestView { std::string title, objective, progress, reward; bool main=false, done=false; };

struct SaveInfo { bool exists=false, valid=false; std::string zone, zoneName, className, time; int level=0; long long stamp=0; std::string error; };

class Campaign {
public:
    combat::Game game;
    world::Map map;
    std::string zoneId;
    std::vector<Entity> entities;
    std::vector<LootBag> loot;
    combat::Class heroClass=combat::Class::Warrior;
    int gold=0, itemHp=0, runeProgress=0;
    std::vector<Stack> inventory;
    Stack equip[3];                          // weapon, armor, talisman
    std::set<std::string> flags;             // story, bosses, shrines, chests, unique rewards
    std::set<std::string> gathered;          // herbs/ore taken; refilled on rest
    std::map<std::string,std::set<int>> killed;   // regular enemies by zone; cleared on rest
    std::map<std::string,int> quests;
    std::string lastShrine="shrine_village";
    std::vector<std::string> notes;
    std::deque<Toast> toasts;
    std::vector<std::pair<Request,std::string>> requests;
    Dialog dialog;
    std::string noteId;
    std::vector<std::string> rewardChoices;
    double playTime=0;
    bool started=false;

    void newGame(combat::Class type);
    void enterZone(const std::string& id, char arrive, Cell exact={-1,-1});
    void enterAtShrine(const std::string& shrine);
    void update(float dt);
    void processEvents();
    void onHeroStep();                       // exits, herbs and loot pickups under the hero
    int entityAt(Cell c) const;
    bool interactable(int index) const;
    std::vector<Cell> interactCells(int index) const;
    void interact(int index);
    void dialogAction(const std::string& action);
    // inventory
    int count(const std::string& id) const;
    void addItem(const std::string& id, int amount=1, int upgrade=0, bool notify=true);
    bool removeItem(const std::string& id, int amount=1);
    bool equipItem(int inventoryIndex, std::string& reason);
    bool unequip(int slot, std::string& reason);
    bool useItem(int inventoryIndex, std::string& reason);
    bool sell(int inventoryIndex, std::string& reason);
    std::vector<std::pair<std::string,int>> shopStock(const std::string& npc) const;
    bool buy(const std::string& npc, const std::string& id, std::string& reason);
    bool canCraft(const Recipe& r, std::string& reason) const;
    bool craft(const Recipe& r, std::string& reason);
    bool canUpgrade(std::string& reason) const;
    bool upgradeWeapon(std::string& reason);
    void applyEquipment();
    bool learnTalent(int tier, std::string& reason);
    void chooseReward(int index);
    // shrines and death
    bool shrineActive(const std::string& id) const { return flags.count("shrine:"+id)>0; }
    std::vector<std::string> activeShrines() const;
    void rest();
    bool travel(const std::string& shrine, std::string& reason);
    bool descend(const std::string& shrine, std::string& reason);
    std::string shrineDungeon(const std::string& shrine) const;
    void respawnAtShrine();
    // quests
    int stage(const std::string& q) const { auto i=quests.find(q);return i==quests.end()?0:i->second; }
    void setStage(const std::string& q,int s);
    std::vector<QuestView> journal() const;
    std::string objective() const;
    int seals() const;
    bool check(const std::string& cond) const;
    void toast(const std::string& text,int rarity=0);
    // saves: text key=value, written to a temporary file and renamed into place
    bool save(const std::filesystem::path& path, std::string& error) const;
    bool load(const std::filesystem::path& path, std::string& error);
    static SaveInfo inspect(const std::filesystem::path& path);
    const ZoneDef& zone() const { return *zoneDef(zoneId); }
    combat::Actor& hero() { return game.actors[0]; }
    const combat::Actor& hero() const { return game.actors[0]; }
    bool autosavePending=false;
private:
    void dropLoot(const combat::Actor& dead);
    void bossDefeated(combat::Kind kind);
    void talk(const std::string& npc);
    void advanceMain();
    void openChest(Entity& e);
    void grant(const std::string& spec, Cell where);
    Cell shrineCell(const std::string& shrine, std::string& zone) const;
};
std::string noteTitle(const std::string& id);
std::string noteText(const std::string& id);
std::string zoneName(const std::string& id);
}
