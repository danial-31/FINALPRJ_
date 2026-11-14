#include "rpg.hpp"
#include <algorithm>
#include <chrono>
#include <ctime>
#include <fstream>
#include <sstream>
#include <cstdio>
#include <cstdlib>

namespace rpg {
using combat::Kind;
using combat::Weapon;
namespace fs = std::filesystem;

// ------------------------------------------------------------------ items
const std::vector<ItemDef>& items() {
    using T=ItemType;using R=Rarity;const Weapon S=Weapon::Sword,D=Weapon::Daggers,B=Weapon::Bow,F=Weapon::Staff;
    static const std::vector<ItemDef> data={
        // id, name, type, rarity, class mask (1 warrior, 2 rogue, 4 mage), weapon, dmg, armor, hp, init, range, price, heal, description
        {"rusty_sword","Ржавый меч",T::Weapon,R::Common,1,S,0,0,0,0,0,10,0,"Старый, но надёжный клинок ополчения."},
        {"militia_sword","Меч ополченца",T::Weapon,R::Uncommon,1,S,3,0,0,0,0,80,0,"Сбалансированный меч из кузни Борна."},
        {"chitin_blade","Хитиновый клинок",T::Weapon,R::Rare,1,S,6,0,0,1,0,220,0,"Выточен из панциря Матки. Лёгок и остр."},
        {"worn_daggers","Потёртые кинжалы",T::Weapon,R::Common,2,D,0,0,0,0,0,10,0,"Пара коротких клинков с выщербленными лезвиями."},
        {"steel_daggers","Стальные кинжалы",T::Weapon,R::Uncommon,2,D,3,0,0,0,0,80,0,"Хорошая сталь, удобные рукояти."},
        {"queen_fangs","Жала Матки",T::Weapon,R::Rare,2,D,5,0,0,2,0,220,0,"Ядовитые жвала, превращённые в кинжалы."},
        {"hunting_bow","Охотничий лук",T::Weapon,R::Uncommon,2,B,2,0,0,0,0,70,0,"Стреляет на 6 клеток. Навык замедляет цель."},
        {"apprentice_staff","Посох ученика",T::Weapon,R::Common,4,F,0,0,0,0,0,10,0,"Ясеневый посох с тусклым кристаллом."},
        {"oak_staff","Дубовый посох",T::Weapon,R::Uncommon,4,F,3,0,0,0,0,80,0,"Резной посох, хранящий тепло очага."},
        {"silkweaver_staff","Посох шелкопряда",T::Weapon,R::Rare,4,F,5,0,0,0,1,220,0,"Обвит шёлком Матки. +1 к дальности."},
        {"padded_armor","Стёганка",T::Armor,R::Common,7,S,0,1,0,0,0,15,0,"Простая стёганая куртка."},
        {"leather_armor","Кожаный доспех",T::Armor,R::Uncommon,7,S,0,2,0,1,0,70,0,"Лёгкий доспех, не стесняет движений."},
        {"chitin_vest","Хитиновая куртка",T::Armor,R::Uncommon,7,S,0,2,10,0,0,90,0,"Пластины паучьего панциря на коже."},
        {"chainmail","Кольчуга",T::Armor,R::Uncommon,3,S,0,3,0,-1,0,120,0,"Прочная, но тяжёлая. Только воин и разбойник."},
        {"mage_robe","Мантия чародея",T::Armor,R::Uncommon,4,S,1,1,15,0,0,100,0,"Ткань с вплетёнными рунами."},
        {"valden_plate","Латы Вальдена",T::Armor,R::Unique,7,S,0,5,20,-1,0,0,0,"Доспех павшего лорда. Уникальный."},
        {"wolf_tooth","Волчий клык",T::Talisman,R::Common,7,S,0,0,0,2,0,30,0,"Оберег охотника: +2 инициативы."},
        {"herb_charm","Травяной оберег",T::Talisman,R::Uncommon,7,S,0,0,12,0,0,60,0,"Пахнет полынью: +12 здоровья."},
        {"iron_ring","Железное кольцо",T::Talisman,R::Uncommon,7,S,1,1,0,0,0,70,0,"Грубое кольцо из шахтёрского железа."},
        {"spirit_charm","Призрачный амулет",T::Talisman,R::Rare,7,S,2,0,0,2,0,140,0,"Холодный камень, в котором шепчут тени."},
        {"guard_amulet","Амулет старого стража",T::Talisman,R::Unique,7,S,0,2,15,0,0,0,0,"Наследие Хальварда, стража дороги."},
        {"iron_heart","Железное сердце",T::Talisman,R::Unique,7,S,1,3,0,0,0,0,0,"Остывший сердечник Смотрителя."},
        {"ashen_crown","Пепельный венец",T::Talisman,R::Unique,7,S,3,0,20,0,0,0,0,"Остаток власти Хранителя Костей."},
        {"potion","Зелье лечения",T::Consumable,R::Common,7,S,0,0,0,0,0,25,35,"Восстанавливает 35 здоровья. В бою: 2 ОД."},
        {"greater_potion","Большое зелье",T::Consumable,R::Uncommon,7,S,0,0,0,0,0,60,70,"Восстанавливает 70 здоровья. В бою: 2 ОД."},
        {"antidote","Противоядие",T::Consumable,R::Common,7,S,0,0,0,0,0,20,10,"Снимает яд, горение, паутину и замедление; +10 здоровья."},
        {"ore","Железная руда",T::Resource,R::Common,7,S,0,0,0,0,0,8,0,"Кузнец использует руду для улучшений."},
        {"herb","Целебная трава",T::Resource,R::Common,7,S,0,0,0,0,0,5,0,"Основа зелий травницы."},
        {"silk","Паучий шёлк",T::Resource,R::Common,7,S,0,0,0,0,0,6,0,"Прочная нить для оберегов и зелий."},
        {"venom","Ядовитая железа",T::Resource,R::Common,7,S,0,0,0,0,0,7,0,"Нужна для противоядий."},
        {"bone","Костяной осколок",T::Resource,R::Common,7,S,0,0,0,0,0,6,0,"Пропитан тёмной силой."},
        {"matriarch_silk","Шёлк Матки",T::Resource,R::Rare,7,S,0,0,0,0,0,50,0,"Редкий материал для улучшения оружия."},
        {"iron_core","Сердечник Смотрителя",T::Resource,R::Rare,7,S,0,0,0,0,0,60,0,"Редкий металл для высшей закалки."},
        {"knight_sigil","Печатка рыцаря",T::Resource,R::Rare,7,S,0,0,0,0,0,70,0,"Знак дома Вальден. Ценится торговцами."},
        {"smith_cargo","Груз кузнеца",T::Quest,R::Uncommon,7,S,0,0,0,0,0,0,0,"Ящик стали с клеймом кузни Борна."},
        {"climbing_gear","Альпинистские крюки",T::Quest,R::Uncommon,7,S,0,0,0,0,0,0,0,"Позволяют пройти через обвал на перевал."},
        {"mine_key","Ключ от шахты",T::Quest,R::Uncommon,7,S,0,0,0,0,0,0,0,"Открывает ствол затопленной шахты."},
        {"crypt_key","Ключ склепа",T::Quest,R::Uncommon,7,S,0,0,0,0,0,0,0,"Бронзовый ключ с гербом Вальденов."},
        {"guard_key","Ключ старого стража",T::Quest,R::Uncommon,7,S,0,0,0,0,0,0,0,"Открывает сундук в сторожевой башне."},
        {"seal_spider","Печать Паутины",T::Quest,R::Unique,7,S,0,0,0,0,0,0,0,"Первая из трёх древних печатей."},
        {"seal_iron","Печать Железа",T::Quest,R::Unique,7,S,0,0,0,0,0,0,0,"Вторая древняя печать."},
        {"seal_crown","Печать Короны",T::Quest,R::Unique,7,S,0,0,0,0,0,0,0,"Третья древняя печать."},
    };
    return data;
}
const ItemDef* item(const std::string& id){for(const auto& d:items())if(id==d.id)return &d;return nullptr;}
const char* rarityName(Rarity r){static const char* n[]={"обычный","необычный","редкий","уникальный"};return n[int(r)];}
const char* typeName(ItemType t){static const char* n[]={"оружие","броня","талисман","расходник","ресурс","сюжетный"};return n[int(t)];}
bool canEquip(const ItemDef& d,combat::Class c){return (d.classMask&(1<<int(c)))!=0;}
int sellPrice(const ItemDef& d,int upgrade){return d.type==ItemType::Quest?0:std::max(1,d.price*2/5)+upgrade*15;}

const std::vector<Recipe>& recipes(){
    static const std::vector<Recipe> data={
        {"potion","potion",1,{{"herb",2}},5,""},
        {"antidote","antidote",1,{{"herb",1},{"venom",1}},0,""},
        {"greater_potion","greater_potion",1,{{"herb",2},{"silk",1},{"bone",1}},10,"flag:recipe_greater"},
        {"herb_charm","herb_charm",1,{{"herb",4},{"silk",2}},20,""},
    };
    return data;
}
UpgradeCost upgradeCost(int level){
    static const UpgradeCost c[]={{0,0,"",0},{50,3,"",0},{100,4,"matriarch_silk",1},{160,5,"iron_core",1}};
    return c[std::clamp(level,0,3)];
}

// ------------------------------------------------------------------ zones
const std::vector<ZoneDef>& zones(){
    static const std::vector<ZoneDef> data={
#include "zone_data.inc"
    };
    return data;
}
const ZoneDef* zoneDef(const std::string& id){for(const auto& z:zones())if(id==z.id)return &z;return nullptr;}
std::string zoneName(const std::string& id){auto z=zoneDef(id);return z?z->name:id;}

static world::Tile terrain(char c){
    using world::Tile;
    switch(c){
    case '#':case 'K':return Tile::Wall;
    case ',':return Tile::Path;
    case 'T':case 'H':case 'r':case 's':case 'l':case 'P':case 'W':case 'B':case 'm':case 'n':case 'y':case 'v':return Tile::Prop;
    case 'c':case 'x':case 'f':case 'g':return Tile::LowProp;
    case '~':return Tile::Water;
    case '=':return Tile::Shallow;
    case 'w':return Tile::Web;
    case 'p':return Tile::Poison;
    case '^':return Tile::Trap;
    case 'k':case 'b':case 'F':case 'd':case 'u':return Tile::Decor;
    default:return Tile::Floor;
    }
}
static bool isTerrain(char c){return std::string("#.,THK~=wp^rcxfskglPbFWBmnydu v").find(c)!=std::string::npos&&c!=' ';}
world::Map buildMap(const ZoneDef& z){
    if(z.rows.empty()){
        world::Map m=world::layoutRoom();
        m.glyphs[size_t(13*m.width+10)]='@';m.glyphs[size_t(14*m.width+10)]='D';m.glyphs[size_t(6*m.width+10)]='9';
        return m;
    }
    world::Map m;int w=int(std::string(z.rows[0]).size()),h=int(z.rows.size());m.resize(w,h);
    for(int y=0;y<h;++y)for(int x=0;x<w;++x){
        char c=z.rows[size_t(y)][x];m.glyphs[size_t(y*w+x)]=c;
        if(isTerrain(c)){m.set(x,y,terrain(c));continue;}
        m.set(x,y,world::Tile::Floor);
        for(const auto& e:z.entities)if(e.anchor==c){
            switch(e.type){
            case EntityType::Npc:case EntityType::Shrine:case EntityType::Lever:case EntityType::Ore:case EntityType::Survivor:
            case EntityType::Rune:case EntityType::KeyCache:case EntityType::Cocoon:m.set(x,y,world::Tile::Prop);break;
            case EntityType::Chest:case EntityType::Note:case EntityType::Cargo:case EntityType::Clue:m.set(x,y,world::Tile::LowProp);break;
            case EntityType::Gate:case EntityType::RuneDoor:case EntityType::LockDoor:m.set(x,y,world::Tile::Door);break;
            default:break;
            }
        }
    }
    return m;
}

// ------------------------------------------------------------------ text
std::string noteTitle(const std::string& id){
    if(id=="guard_journal")return "Дневник стража";
    if(id=="lair_note")return "Записка шахтёра";
    if(id=="foreman_note")return "Записка бригадира";
    if(id=="mage_note_1")return "Обгоревший лист";
    if(id=="rune_note")return "Надпись на плите";
    if(id=="mage_diary")return "Дневник придворного мага";
    return id;
}
std::string noteText(const std::string& id){
    if(id=="guard_journal")return "«Если со мной что-то случится — ключ от моего сундука спрятан в дупле старого дуба в Паучьей роще. Сам сундук остался в руинах сторожевой башни на перевале.»\n— Хальвард, страж дороги";
    if(id=="lair_note")return "«Нас утащили в коконы. Тоби ещё дышит — я слышу его. Матка сторожит камень с древними знаками, он светится сквозь паутину. Кто найдёт это — уходите через боковой тоннель.»\n— шахтёр Рен";
    if(id=="foreman_note")return "«Мы пробили старую стену и нашли зал с печатью. Потом хлынула вода, а железный страж в глубине открыл глаза. Насосы ещё можно запустить — западный и восточный.»\n— бригадир Гарт";
    if(id=="mage_note_1")return "«Вторая печать поддаётся. Ещё немного, мой лорд, и я верну вас из праха. Пусть долина заплатит — вы стоите любой цены.»\n— М.";
    if(id=="rune_note")return "«Луна восходит прежде звёзд, и лишь за звёздами встаёт солнце.»\nПод надписью — три пустых гнезда: Луна, Звезда, Солнце.";
    if(id=="mage_diary")return "«Я разбил три печати, чтобы поднять лорда Вальдена. Он встал — но без души, и теперь стережёт свой склеп. А под замком проснулся Хранитель Костей, которого печати держали веками. Я впустил его в мир. Пусть кто-нибудь исправит то, что сделал я.»\n— Мэйдрик, придворный маг";
    return "";
}

// ------------------------------------------------------------------ campaign core
void Campaign::toast(const std::string& text,int rarity){toasts.push_back({text,rarity,0});if(toasts.size()>6)toasts.pop_front();}
void Campaign::newGame(combat::Class type){
    *this=Campaign{};started=true;heroClass=type;
    combat::Actor h=combat::Game::makeHero(type);game.heroClass=type;
    game.actors={h};
    const char* weapon[]={"rusty_sword","worn_daggers","apprentice_staff"};
    equip[0]={weapon[int(type)],1,0};equip[1]={"padded_armor",1,0};
    addItem("potion",1,0,false);
    gold=20;quests["main"]=0;
    applyEquipment();
    enterZone("village",'@');
    toast("Правая кнопка мыши — идти. Подойдите к старосте Ольвину.",0);
}
int Campaign::seals() const{return int(flags.count("boss_matriarch")+flags.count("boss_warden")+flags.count("boss_knight"));}
bool Campaign::check(const std::string& cond) const{
    if(cond.empty())return true;
    std::stringstream ss(cond);std::string part;
    while(std::getline(ss,part,',')){
        if(part.rfind("flag:",0)==0){if(!flags.count(part.substr(5)))return false;}
        else if(part.rfind("item:",0)==0){if(count(part.substr(5))<=0)return false;}
        else if(part.rfind("stage:",0)==0){auto p=part.find(">=");if(stage(part.substr(6,p-6))<std::stoi(part.substr(p+2)))return false;}
        else if(part.rfind("seals>=",0)==0){if(seals()<std::stoi(part.substr(7)))return false;}
    }
    return true;
}
void Campaign::enterZone(const std::string& id,char arrive,Cell exact){
    const ZoneDef* z=zoneDef(id);if(!z)return;
    zoneId=id;map=buildMap(*z);game.setMap(&map);
    combat::Actor h=game.actors.empty()?combat::Game::makeHero(heroClass):game.actors[0];
    game.clearStatuses(h);h.target=-1;h.path.clear();h.moving=false;
    game.actors={h};game.phase=combat::Phase::Explore;game.telegraphs.clear();game.hazards.clear();game.events.clear();
    entities.clear();loot.clear();runeProgress=0;
    Cell arrival{-1,-1};int spawnIndex=0;
    for(int y=0;y<map.height;++y)for(int x=0;x<map.width;++x){
        char c=map.glyph(x,y);
        for(const auto& s:z->spawns)if(s.anchor==c){
            int index=spawnIndex++;
            bool boss=combat::kindInfo(s.kind).boss;
            const char* bossFlag=s.kind==Kind::Matriarch?"boss_matriarch":s.kind==Kind::Warden?"boss_warden":s.kind==Kind::FallenKnight?"boss_knight":s.kind==Kind::BoneKeeper?"boss_keeper":"";
            if(boss&&flags.count(bossFlag))continue;
            if(s.kind==Kind::Pylon&&flags.count("boss_keeper"))continue;
            if(!boss&&s.kind!=Kind::Pylon&&killed[zoneId].count(index))continue;
            auto e=combat::Game::makeEnemy(s.kind,{x,y},map);e.group=s.group;e.spawn=index;game.actors.push_back(e);
        }
        for(const auto& d:z->entities)if(d.anchor==c){
            Entity e{d,{x,y}};
            std::string key="open:"+zoneId+":"+std::to_string(x)+":"+std::to_string(y);
            switch(d.type){
            case EntityType::Arrive:if(c==arrive)arrival={x,y};break;
            case EntityType::Npc:e.hidden=!check(d.cond);if(e.hidden)map.set(x,y,world::Tile::Floor);break;
            case EntityType::Chest:case EntityType::Cocoon:case EntityType::KeyCache:case EntityType::Survivor:case EntityType::Cargo:case EntityType::Clue:
                e.done=flags.count(key)>0;break;
            case EntityType::Lever:e.done=flags.count(d.id)>0;break;
            case EntityType::Ore:case EntityType::Herb:e.done=gathered.count(key)>0;break;
            case EntityType::Gate:case EntityType::LockDoor:case EntityType::RuneDoor:e.done=flags.count(key)>0||(d.type!=EntityType::LockDoor&&check(d.cond));break;
            default:break;
            }
            if(e.done&&(d.type==EntityType::Gate||d.type==EntityType::LockDoor||d.type==EntityType::RuneDoor||d.type==EntityType::Cocoon||d.type==EntityType::Survivor||d.type==EntityType::Cargo))map.set(x,y,world::Tile::Floor);
            if(d.type==EntityType::Herb&&e.done)e.hidden=true;
            entities.push_back(e);
        }
    }
    if(exact.x>=0&&map.inside(exact.x,exact.y)&&!map.blocked(exact.x,exact.y))arrival=exact;
    if(arrival.x<0)for(const auto& e:entities)if(e.def.type==EntityType::Arrive){arrival=e.cell;break;}
    game.place(0,arrival);game.projectiles.clear();game.pendingSlot=-1;
    flags.insert("visited:"+zoneId);
    requests.push_back({Request::ZoneChanged,zoneId});
}
std::string Campaign::shrineDungeon(const std::string& shrine) const{
    for(const auto& z:zones())for(const auto& e:z.entities)if(e.type==EntityType::Shrine&&shrine==e.id)return e.arg;
    return "";
}
Cell Campaign::shrineCell(const std::string& shrine,std::string& zoneOut) const{
    for(const auto& z:zones())for(const auto& e:z.entities)if(e.type==EntityType::Shrine&&shrine==e.id){
        zoneOut=z.id;world::Map m=buildMap(z);
        for(int y=0;y<m.height;++y)for(int x=0;x<m.width;++x)if(m.glyph(x,y)==e.anchor){
            for(Cell d:{Cell{0,1},Cell{1,0},Cell{-1,0},Cell{0,-1},Cell{1,1},Cell{-1,1}}){Cell n{x+d.x,y+d.y};if(!m.blocked(n.x,n.y))return n;}
        }
    }
    return {-1,-1};
}
void Campaign::enterAtShrine(const std::string& shrine){
    std::string z;Cell c=shrineCell(shrine,z);if(z.empty())return;
    enterZone(z,'\0',c);
}
void Campaign::update(float dt){
    playTime+=dt;
    for(auto& t:toasts)t.time+=dt;
    while(!toasts.empty()&&toasts.front().time>5.f)toasts.pop_front();
    for(auto& b:loot)b.age+=dt;
    processEvents();
}
void Campaign::processEvents(){
    auto events=std::move(game.events);game.events.clear();
    for(const auto& e:events){
        if(e.type==combat::Event::Death&&e.target>0&&e.target<int(game.actors.size())){
            const auto& dead=game.actors[size_t(e.target)];
            if(dead.spawn>=0&&!dead.boss&&dead.kind!=Kind::Pylon)killed[zoneId].insert(dead.spawn);
            if(dead.xpReward>0||dead.boss)dropLoot(dead);
            if(dead.boss)bossDefeated(dead.kind);
        }
        if(e.type==combat::Event::LevelUp)toast("Новый уровень "+std::to_string(e.amount)+"! Очко развития (C).",2);
        if(e.type==combat::Event::BattleStart&&!flags.count("tutorial_battle")){
            flags.insert("tutorial_battle");
            toast("Бой! Красные круги на земле — удар босса: уходите с них.",1);
            toast("ПКМ по врагу — атаковать, Q W E R — навыки, H — зелье.",1);
        }
        if(e.type==combat::Event::BattleEnd||e.type==combat::Event::Step)onHeroStep();
    }
    if(game.phase==combat::Phase::Defeat){
        bool already=false;for(auto& r:requests)already|=r.first==Request::Defeat;
        if(!already&&!flags.count("defeat_pending")){flags.insert("defeat_pending");requests.push_back({Request::Defeat,""});}
    }
}
struct LootEntry { const char* id; int chance, min, max; };
void Campaign::dropLoot(const combat::Actor& dead){
    LootBag bag;bag.cell=dead.cell;
    auto roll=[&](int pct){return int(game.random(100))<pct;};
    auto range=[&](int a,int b){return a+int(game.random(unsigned(b-a+1)));};
    std::vector<LootEntry> table;int g0=0,g1=0;
    switch(dead.kind){
    case Kind::Wolf:table={{"bone",40,1,1},{"potion",10,1,1},{"wolf_tooth",6,1,1}};g1=4;break;
    case Kind::Bandit:table={{"potion",25,1,1},{"ore",30,1,2},{"leather_armor",7,1,1},{"iron_ring",4,1,1}};g0=8;g1=18;break;
    case Kind::BanditArcher:table={{"potion",20,1,1},{"herb",30,1,2},{"hunting_bow",8,1,1}};g0=6;g1=14;break;
    case Kind::Spiderling:table={{"silk",50,1,1},{"venom",20,1,1}};break;
    case Kind::VenomSpider:table={{"venom",70,1,2},{"silk",40,1,1},{"antidote",10,1,1}};g1=6;break;
    case Kind::SpiderGuard:table={{"silk",80,1,2},{"venom",40,1,1},{"chitin_vest",12,1,1}};g0=5;g1=12;break;
    case Kind::Rat:table={{"herb",25,1,1}};g1=5;break;
    case Kind::Slime:table={{"potion",15,1,1},{"herb",30,1,1}};g0=2;g1=8;break;
    case Kind::Miner:table={{"ore",70,1,2},{"potion",15,1,1}};g0=6;g1=15;break;
    case Kind::Golem:table={{"ore",100,2,3},{"iron_ring",8,1,1}};g0=10;g1=20;break;
    case Kind::Skeleton:table={{"bone",70,1,2},{"potion",15,1,1}};g0=5;g1=12;break;
    case Kind::SkeletonArcher:table={{"bone",60,1,1},{"antidote",10,1,1}};g0=5;g1=12;break;
    case Kind::Ghost:table={{"bone",30,1,1},{"spirit_charm",6,1,1}};g0=6;g1=14;break;
    case Kind::KnightGuard:table={{"bone",60,1,2},{"potion",25,1,1},{"chainmail",8,1,1}};g0=15;g1=25;break;
    case Kind::Pylon:table={{"bone",100,1,1}};break;
    default:break;
    }
    for(const auto& t:table)if(roll(t.chance))bag.items.push_back({t.id,range(t.min,t.max),0});
    if(g1>0)bag.gold=range(g0,g1);
    if(bag.items.empty()&&bag.gold==0)return;
    for(auto& b:loot)if(b.cell==bag.cell){b.gold+=bag.gold;b.items.insert(b.items.end(),bag.items.begin(),bag.items.end());return;}
    loot.push_back(bag);
}
void Campaign::bossDefeated(Kind kind){
    autosavePending=true;
    auto unique=[&](const char* id){if(!flags.count(std::string("unique:")+id)){flags.insert(std::string("unique:")+id);addItem(id);}};
    switch(kind){
    case Kind::Matriarch:
        flags.insert("boss_matriarch");addItem("seal_spider");addItem("matriarch_silk");gold+=80;toast("+80 золота",0);
        if(!flags.count("reward_spider")){rewardChoices={"chitin_blade","queen_fangs","silkweaver_staff"};requests.push_back({Request::RewardChoice,""});}
        toast("Печать Паутины восстановлена (1/3).",3);break;
    case Kind::Warden:
        flags.insert("boss_warden");flags.insert("smith_upgrades");addItem("seal_iron");addItem("iron_core");unique("iron_heart");gold+=120;
        toast("Печать Железа восстановлена (2/3). Кузнец может улучшать оружие.",3);break;
    case Kind::FallenKnight:
        flags.insert("boss_knight");addItem("seal_crown");addItem("knight_sigil");unique("valden_plate");gold+=150;
        toast("Печать Короны восстановлена (3/3). Ворота замка открыты!",3);break;
    case Kind::BoneKeeper:
        flags.insert("boss_keeper");unique("ashen_crown");gold+=300;
        requests.push_back({Request::Epilogue,""});
        toast("Хранитель Костей повержен!",3);break;
    default:break;
    }
    advanceMain();
}
int Campaign::entityAt(Cell c) const{
    for(size_t i=0;i<entities.size();++i)if(entities[i].cell==c&&!entities[i].hidden)return int(i);
    return -1;
}
bool Campaign::interactable(int index) const{
    if(index<0||index>=int(entities.size()))return false;
    const auto& e=entities[size_t(index)];if(e.hidden)return false;
    switch(e.def.type){
    case EntityType::Arrive:case EntityType::Exit:case EntityType::Entrance:return false;
    case EntityType::Gate:case EntityType::LockDoor:case EntityType::RuneDoor:return !e.done;
    case EntityType::Chest:case EntityType::Cocoon:case EntityType::Ore:case EntityType::Herb:case EntityType::Survivor:
    case EntityType::Cargo:case EntityType::KeyCache:case EntityType::Clue:return !e.done;
    default:return true;
    }
}
std::vector<Cell> Campaign::interactCells(int index) const{
    std::vector<Cell> out;if(index<0)return out;Cell c=entities[size_t(index)].cell;
    if(!map.blocked(c.x,c.y)){out.push_back(c);return out;}
    for(Cell d:{Cell{0,1},Cell{1,0},Cell{-1,0},Cell{0,-1},Cell{1,1},Cell{-1,1},Cell{1,-1},Cell{-1,-1}}){Cell n{c.x+d.x,c.y+d.y};if(!map.blocked(n.x,n.y))out.push_back(n);}
    return out;
}
void Campaign::grant(const std::string& spec,Cell where){
    std::stringstream ss(spec);std::string part;
    while(std::getline(ss,part,';')){
        auto colon=part.find(':');std::string id=part.substr(0,colon);int n=colon==std::string::npos?1:std::stoi(part.substr(colon+1));
        if(id=="gold"){gold+=n;toast("+"+std::to_string(n)+" золота",0);}
        else if(id=="enemy"){
            combat::Kind kind=Kind::Spiderling;
            int last=-1;
            for(int m=0;m<3;++m)for(Cell d:{Cell{0,1},Cell{1,0},Cell{-1,0},Cell{0,-1},Cell{1,1},Cell{-1,-1}}){Cell c{where.x+d.x,where.y+d.y};
                if(!map.blocked(c.x,c.y)&&game.occupant(c)<0){auto e=combat::Game::makeEnemy(kind,c,map);e.pos+=glm::vec2(float(m)*.2f,0);game.actors.push_back(e);last=int(game.actors.size())-1;break;}}
            toast("Из кокона вырываются пауки!",1);
            if(last>0)game.engage(last);
        }else{auto d=item(id);if(d){if(d->rarity==Rarity::Unique){if(flags.count("unique:"+id))continue;flags.insert("unique:"+id);}addItem(id,n);}}
    }
}
void Campaign::onHeroStep(){
    Cell c=hero().cell;
    for(size_t i=0;i<loot.size();++i)if(loot[i].cell==c){
        auto bag=loot[i];loot.erase(loot.begin()+long(i));
        if(bag.gold){gold+=bag.gold;toast("+"+std::to_string(bag.gold)+" золота",0);}
        for(const auto& s:bag.items)addItem(s.id,s.count);
        break;
    }
    if(game.phase!=combat::Phase::Explore)return;
    int index=entityAt(c);if(index<0)return;
    auto& e=entities[size_t(index)];
    if(e.def.type==EntityType::Herb&&!e.done){interact(index);return;}
    if(e.def.type==EntityType::Exit||e.def.type==EntityType::Entrance){
        if(!check(e.def.cond)){toast(e.def.msg[0]?e.def.msg:"Путь закрыт.",1);return;}
        std::string target=e.def.arg;auto at=target.find('@');std::string zone=target.substr(0,at);char anchor=target[at+1];
        if(e.def.type==EntityType::Entrance&&!flags.count("found:"+zone)){flags.insert("found:"+zone);toast(std::string("Найден вход: ")+zoneName(zone),2);}
        if(e.def.type==EntityType::Entrance||zoneDef(zone)->surface==false)flags.insert("found:"+zone);
        enterZone(zone,anchor);autosavePending=true;advanceMain();
    }
}
void Campaign::openChest(Entity& e){
    std::string key="open:"+zoneId+":"+std::to_string(e.cell.x)+":"+std::to_string(e.cell.y);
    flags.insert(key);e.done=true;grant(e.def.arg,e.cell);
    if(std::string(e.def.id)=="guard_cache"){setStage("guard",3);game.grantXp(60);toast("Задание выполнено: Тайник старого стража (+60 опыта)",2);}
}
void Campaign::interact(int index){
    if(index<0||index>=int(entities.size()))return;
    auto& e=entities[size_t(index)];const auto& d=e.def;std::string id=d.id;
    std::string key="open:"+zoneId+":"+std::to_string(e.cell.x)+":"+std::to_string(e.cell.y);
    switch(d.type){
    case EntityType::Npc:talk(id);break;
    case EntityType::Shrine:
        if(!shrineActive(id)){flags.insert("shrine:"+id);toast(std::string("Святилище активировано: ")+d.label,2);autosavePending=true;}
        lastShrine=id;advanceMain();requests.push_back({Request::Shrine,id});break;
    case EntityType::Chest:
        if(e.done){toast("Пусто.");break;}
        if(!check(d.cond)){toast(d.msg,1);break;}
        openChest(e);break;
    case EntityType::Cocoon:
        if(e.done)break;
        flags.insert(key);e.done=true;map.set(e.cell.x,e.cell.y,world::Tile::Floor);grant(d.arg,e.cell);break;
    case EntityType::Ore:case EntityType::Herb:
        if(e.done)break;
        gathered.insert(key);e.done=true;if(d.type==EntityType::Herb)e.hidden=true;grant(d.type==EntityType::Herb?"herb:1":d.arg,e.cell);break;
    case EntityType::Note:
        noteId=id;if(std::find(notes.begin(),notes.end(),id)==notes.end())notes.push_back(id);
        if(id=="guard_journal"&&stage("guard")==0){setStage("guard",count("guard_key")?2:1);toast("Новое задание: Тайник старого стража",2);}
        requests.push_back({Request::Note,id});break;
    case EntityType::Survivor:
        if(e.done)break;
        flags.insert(key);flags.insert("rescued_miner");e.done=true;map.set(e.cell.x,e.cell.y,world::Tile::Floor);
        toast("Шахтёр Тоби освобождён! Он доберётся до деревни сам.",2);game.grantXp(50);advanceMain();break;
    case EntityType::Cargo:
        if(e.done)break;
        flags.insert(key);e.done=true;map.set(e.cell.x,e.cell.y,world::Tile::Floor);addItem("smith_cargo");
        if(stage("cargo")==0){setStage("cargo",1);toast("Новое задание: Пропавший груз (отнесите кузнецу)",2);}
        break;
    case EntityType::Clue:
        if(e.done)break;
        flags.insert(key);e.done=true;
        if(id=="miner_traces"){toast("Каска пробита жвалами. Следы ведут в логово пауков.",1);flags.insert("clue_miners");if(stage("main")==2)setStage("main",3);}
        else toast("Следы волочения уходят в ствол шахты.",1);
        game.grantXp(20);break;
    case EntityType::KeyCache:
        if(e.done)break;
        flags.insert(key);e.done=true;addItem("guard_key");
        if(stage("guard")<2){setStage("guard",2);}
        toast("В дупле — ключ старого стража.",1);break;
    case EntityType::Lever:
        if(e.done){toast("Насос уже работает.");break;}
        flags.insert(id);e.done=true;toast(std::string(d.label)+": запущен",1);
        for(auto& g:entities)if(g.def.type==EntityType::Gate&&!g.done){
            if(check(g.def.cond)){g.done=true;map.set(g.cell.x,g.cell.y,world::Tile::Floor);toast("Затвор поднимается — путь к печати открыт!",2);}
            else toast("Нужен второй насос.",0);
        }
        break;
    case EntityType::Gate:
        if(e.done)break;
        if(check(d.cond)){e.done=true;flags.insert(key);map.set(e.cell.x,e.cell.y,world::Tile::Floor);toast("Путь расчищен.",1);}
        else toast(d.msg,1);
        break;
    case EntityType::LockDoor:
        if(e.done)break;
        if(check(d.cond)){e.done=true;flags.insert(key);removeItem("crypt_key");map.set(e.cell.x,e.cell.y,world::Tile::Floor);toast("Ключ поворачивается. Дверь открыта.",1);}
        else toast(d.msg,1);
        break;
    case EntityType::RuneDoor:
        toast(e.done?"Дверь открыта.":d.msg,1);break;
    case EntityType::Rune:{
        static const int order[]={1,2,0};
        int rune=std::stoi(d.arg);
        if(flags.count("runes_solved")){toast("Руны уже горят.");break;}
        if(order[runeProgress]==rune){
            ++runeProgress;toast(std::string(d.label)+" вспыхивает ("+std::to_string(runeProgress)+"/3)",1);
            if(runeProgress==3){flags.insert("runes_solved");game.grantXp(40);
                for(auto& g:entities)if(g.def.type==EntityType::RuneDoor){g.done=true;map.set(g.cell.x,g.cell.y,world::Tile::Floor);}
                toast("Рунная дверь отворяется!",2);}
        }else{runeProgress=0;toast("Руны гаснут. Порядок неверен.",1);}
        break;}
    default:break;
    }
}

// ------------------------------------------------------------------ dialogues
void Campaign::talk(const std::string& npc){
    dialog=Dialog{};int s=stage("main");
    auto say=[&](std::string line){dialog.lines.push_back(std::move(line));};
    auto option=[&](std::string label,std::string action){dialog.options.push_back({std::move(label),std::move(action)});};
    if(npc=="elder"){
        dialog.speaker="Староста Ольвин";
        if(s==0){
            say("Странник? Хвала Свету, что дорога тебя пощадила.");
            say("После обвала в старой шахте из разломов полезли твари. Пропал отряд шахтёров — шестеро, и мой племянник Тоби с ними.");
            say("Возьми припасы у торговки Миры и коснись святилища на площади: его руны запомнят тебя. Следы шахтёров вели на восток, к Паучьей роще.");
            setStage("main",1);
        }else if(s<=3){
            say(s==1?"Мира у своего прилавка, святилище — в центре площади.":"Шахтёры ждут. Дорога на восток, за бродом, затем на север — в рощу.");
        }else if(s==4){
            say("Тоби жив! Ты вернул надежду Серому Броду.");
            say("Он говорит, Матка стерегла светящийся камень — Печать Паутины. Таких печатей было три: они держали Хранителя Костей глубоко под замком.");
            say("Их разбил Мэйдрик, придворный маг, когда пытался вернуть погибшего лорда Вальдена. Вторая печать — в затопленных шахтах. Вот ключ от ствола.");
            addItem("mine_key");setStage("main",5);
        }else if(s==5)say("Насосы в шахте давно стоят. Запусти оба — западный и восточный, тогда откроется путь к печати.");
        else if(s==6)say("Перевал завалило. Борн ковал крюки для шахтёров — спроси его.");
        else if(s<=8)say("Королевский склеп лежит за перевалом. Там покоится сам лорд Вальден. Будь осторожен.");
        else if(s==9)say("Все три печати целы. Ворота замка на севере открыты. Скверна слабеет, но её сердце ещё бьётся под замком.");
        else if(s==10){
            say("Ты вернулся! Скверна ушла из долины, и наши люди возвращаются домой.");
            say("Серый Брод в долгу перед тобой. Пусть этот дом будет и твоим, странник.");
            setStage("main",11);requests.push_back({Request::Credits,""});
        }else say("Долина спокойна. Говорят, в горах ещё остались нетронутые тайники…");
        option("До встречи","close");
    }else if(npc=="merchant"){
        dialog.speaker="Торговка Мира";
        if(!flags.count("got_supplies")){
            say("Ты — тот странник? Староста просил помочь. Держи два зелья и противоядие — за счёт деревни.");
            say("Открой сумку клавишей I, чтобы осмотреть вещи. Зелья можно пить и в бою — это стоит очков действия.");
            flags.insert("got_supplies");addItem("potion",2);addItem("antidote",1);advanceMain();
        }else say(s>=11?"Праздник в деревне — и торговля идёт! Что возьмёшь?":"Зелья, броня, обереги. Продам и куплю — без обмана.");
        option("Торговать","shop:merchant");option("Уйти","close");
    }else if(npc=="smith"){
        dialog.speaker="Кузнец Борн";
        if(count("smith_cargo")>0&&stage("cargo")<2){
            say("Мой ящик! Клеймо целое — значит, и сталь цела.");
            say("Как обещал: вот оружие под твою руку и немного монет.");
            removeItem("smith_cargo");
            const char* reward[]={"militia_sword","steel_daggers","oak_staff"};addItem(reward[int(heroClass)]);gold+=80;toast("+80 золота",0);
            setStage("cargo",2);game.grantXp(60);toast("Задание выполнено: Пропавший груз (+60 опыта)",2);
        }else if(s==6){
            say("Перевал, говоришь? Вот крюки и верёвка — ковал их для шахтёров.");
            say("А сердечник Смотрителя — редкий металл. Теперь я могу закалить твоё оружие. Неси руду.");
            addItem("climbing_gear");setStage("main",7);
        }else if(stage("cargo")==0&&s>=1){
            say("Разбойники разграбили мою повозку на Старой дороге. В ящике — лучшая сталь, что у меня была.");
            say("Найдёшь ящик с клеймом кузни — сделаю тебе оружие под руку.");
            setStage("cargo",1);toast("Новое задание: Пропавший груз",2);
        }else say(flags.count("smith_upgrades")?"Горн горячий. Руда при тебе?":"Куплю, продам, подлатаю. Для закалки нужен особый металл — такой есть только в глубинах шахты.");
        option("Торговать","shop:smith");option("Улучшить оружие","smith");option("Уйти","close");
    }else if(npc=="herbalist"){
        dialog.speaker="Травница Ива";
        int q=stage("antidote");
        if(q==0){
            say("Паучий яд губит людей быстрее, чем я успеваю варить противоядие.");
            say("Принеси три целебные травы и две ядовитые железы — и я научу тебя большому зелью.");
            setStage("antidote",1);toast("Новое задание: Противоядие для деревни",2);
        }else if(q==1&&count("herb")>=3&&count("venom")>=2){
            say("Всё, что нужно! Держи противоядия, а рецепт большого зелья теперь твой.");
            removeItem("herb",3);removeItem("venom",2);addItem("antidote",3);gold+=40;flags.insert("recipe_greater");
            setStage("antidote",2);game.grantXp(60);toast("Задание выполнено: Противоядие (+60 опыта)",2);
        }else say(q==1?"Травы растут в рощах и на перевале, железы — у ядовитых пауков.":"Из трав и шёлка сварю что пожелаешь — неси ингредиенты.");
        option("Ремесло","craft");option("Уйти","close");
    }else if(npc=="survivor"){
        dialog.speaker="Шахтёр Тоби";
        say(s<=5?"Спасибо, что вытащил меня из кокона. Остальные… не дождались. Под шахтой есть ещё одна печать — я видел её свет.":
            s<=9?"Насосы, печати, склеп… Ты делаешь то, на что у нас не хватило бы смелости.":"Я снова спускаюсь в шахту — теперь там только руда и тишина.");
        option("Береги себя","close");
    }
    requests.push_back({Request::Dialog,npc});
}
void Campaign::dialogAction(const std::string& action){
    if(action.rfind("shop:",0)==0)requests.push_back({Request::Shop,action.substr(5)});
    else if(action=="craft")requests.push_back({Request::Craft,""});
    else if(action=="smith")requests.push_back({Request::Smith,""});
}
void Campaign::setStage(const std::string& q,int s){
    int before=stage(q);quests[q]=s;
    if(q=="main"&&s>before&&before>=1&&s<=10){game.grantXp(40+before*10);}
    if(q=="main"&&s!=before)toast("Цель: "+objective(),1);
}
void Campaign::advanceMain(){
    for(int guard=0;guard<4;++guard){
        int s=stage("main");
        if(s==1&&flags.count("got_supplies")&&shrineActive("shrine_village"))setStage("main",2);
        else if(s==2&&(flags.count("found:lair")||flags.count("clue_miners")))setStage("main",3);
        else if(s==3&&flags.count("boss_matriarch")&&flags.count("rescued_miner"))setStage("main",4);
        else if(s==5&&flags.count("boss_warden"))setStage("main",6);
        else if(s==7&&flags.count("found:crypt"))setStage("main",8);
        else if(s==8&&flags.count("boss_knight"))setStage("main",9);
        else if(s==9&&flags.count("boss_keeper"))setStage("main",10);
        else break;
    }
}
std::string Campaign::objective() const{
    static const char* text[]={
        "Поговорить со старостой Ольвином",
        "Получить припасы у торговки Миры и активировать святилище",
        "Найти следы шахтёров у Паучьей рощи",
        "Паучье логово: спасти шахтёра и победить Паучью Матку",
        "Вернуться в Серый Брод к старосте",
        "Затопленные шахты: запустить два насоса и победить Смотрителя",
        "Подготовиться к походу: поговорить с кузнецом Борном",
        "Пройти через перевал и найти вход в Королевский склеп",
        "Пройти склеп и победить Павшего рыцаря Вальдена",
        "Войти в замок Вальден и остановить Хранителя Костей",
        "Вернуться в Серый Брод",
        "История завершена. Исследуйте оставшиеся тайники",
    };
    return text[std::clamp(stage("main"),0,11)];
}
std::vector<QuestView> Campaign::journal() const{
    std::vector<QuestView> out;int s=stage("main");
    QuestView m;m.main=true;m.title="Пепел Эшвальда";m.objective=objective();m.done=s>=11;
    if(s==1)m.progress=std::string("Припасы: ")+(flags.count("got_supplies")?"да":"нет")+"   Святилище: "+(shrineActive("shrine_village")?"да":"нет");
    if(s==3)m.progress=std::string("Шахтёр спасён: ")+(flags.count("rescued_miner")?"да":"нет")+"   Матка: "+(flags.count("boss_matriarch")?"побеждена":"жива");
    if(s==5)m.progress="Насосы: "+std::to_string(int(flags.count("lever_a")+flags.count("lever_b")))+"/2   Смотритель: "+(flags.count("boss_warden")?"побеждён":"жив");
    m.progress+=(m.progress.empty()?"":"   ")+std::string("Печати: ")+std::to_string(seals())+"/3";
    m.reward="Опыт за каждый этап, печати, уникальные трофеи боссов";out.push_back(m);
    if(int q=stage("cargo")){QuestView v;v.title="Пропавший груз";v.done=q>=2;
        v.objective=v.done?"Выполнено":count("smith_cargo")?"Отнести ящик кузнецу Борну":"Найти ящик кузни в лагере разбойников на Старой дороге";
        v.reward="80 золота, оружие для класса, 60 опыта";out.push_back(v);}
    if(int q=stage("antidote")){QuestView v;v.title="Противоядие для деревни";v.done=q>=2;
        v.objective=v.done?"Выполнено":"Принести травнице Иве травы и ядовитые железы";
        if(!v.done)v.progress="Травы: "+std::to_string(std::min(3,count("herb")))+"/3   Железы: "+std::to_string(std::min(2,count("venom")))+"/2";
        v.reward="3 противоядия, рецепт большого зелья, 40 золота";out.push_back(v);}
    if(int q=stage("guard")){QuestView v;v.title="Тайник старого стража";v.done=q>=3;
        v.objective=v.done?"Выполнено":q==1?"Найти ключ в дупле старого дуба (Паучья роща)":"Открыть сундук в руинах сторожевой башни (Горный перевал)";
        v.reward="Уникальный амулет стража, 120 золота";out.push_back(v);}
    return out;
}

// ------------------------------------------------------------------ inventory
int Campaign::count(const std::string& id) const{int n=0;for(const auto& s:inventory)if(s.id==id)n+=s.count;for(const auto& e:equip)if(e.id==id)n+=1;return n;}
void Campaign::addItem(const std::string& id,int amount,int upgrade,bool notify){
    auto d=item(id);if(!d||amount<=0)return;
    bool stackable=d->type==ItemType::Consumable||d->type==ItemType::Resource||d->type==ItemType::Quest;
    if(stackable){bool merged=false;for(auto& s:inventory)if(s.id==id){s.count+=amount;merged=true;break;}if(!merged)inventory.push_back({id,amount,0});}
    else for(int i=0;i<amount;++i)inventory.push_back({id,1,upgrade});
    if(notify)toast("+ "+std::string(d->name)+(amount>1?" ×"+std::to_string(amount):"")+"  ("+rarityName(d->rarity)+")",int(d->rarity));
}
bool Campaign::removeItem(const std::string& id,int amount){
    if(count(id)<amount)return false;
    for(size_t i=0;i<inventory.size()&&amount>0;){
        auto& s=inventory[i];if(s.id!=id){++i;continue;}
        int take=std::min(amount,s.count);s.count-=take;amount-=take;
        if(s.count<=0)inventory.erase(inventory.begin()+long(i));else ++i;
    }
    return amount==0;
}
void Campaign::applyEquipment(){
    auto& h=hero();int dmg=0,arm=0,init=0,range=0,hp=0;
    for(const auto& e:equip){auto d=item(e.id);if(!d)continue;dmg+=d->damage+e.upgrade*2;arm+=d->armor;init+=d->initiative;range+=d->range;hp+=d->hp;}
    auto w=item(equip[0].id);if(w)h.weapon=w->weapon;
    h.damageBonus=dmg;h.armorBonus=arm;h.initiativeBonus=init;h.rangeBonus=range;
    h.maxHp+=hp-itemHp;itemHp=hp;h.hp=std::clamp(h.hp,1,h.maxHp);

}
bool Campaign::equipItem(int index,std::string& reason){
    if(index<0||index>=int(inventory.size())){reason="Нет предмета";return false;}
    auto d=item(inventory[size_t(index)].id);if(!d){reason="Неизвестный предмет";return false;}
    int slot=d->type==ItemType::Weapon?0:d->type==ItemType::Armor?1:d->type==ItemType::Talisman?2:-1;
    if(slot<0){reason="Этот предмет нельзя надеть";return false;}
    if(!canEquip(*d,heroClass)){reason=std::string("Не подходит классу ")+combat::className(heroClass);return false;}
    if(game.inBattle()){
        if(slot==1){reason="Броню нельзя менять в бою";return false;}
    }
    Stack incoming=inventory[size_t(index)];inventory.erase(inventory.begin()+index);
    if(!equip[slot].id.empty())inventory.push_back(equip[slot]);
    equip[slot]=incoming;applyEquipment();toast(std::string("Надето: ")+d->name);return true;
}
bool Campaign::unequip(int slot,std::string& reason){
    if(slot<0||slot>2||equip[slot].id.empty()){reason="Слот пуст";return false;}
    if(slot==0){reason="Нельзя остаться без оружия — наденьте другое";return false;}
    if(game.inBattle()){reason="Снимать снаряжение можно вне боя";return false;}
    inventory.push_back(equip[slot]);equip[slot]={"",0,0};applyEquipment();return true;
}
bool Campaign::useItem(int index,std::string& reason){
    if(index<0||index>=int(inventory.size())){reason="Нет предмета";return false;}
    auto d=item(inventory[size_t(index)].id);if(!d)return false;
    if(d->type!=ItemType::Consumable)return equipItem(index,reason);
    auto& h=hero();
    bool cures=std::string(d->id)=="antidote";
    if(!cures&&h.hp>=h.maxHp){reason="Здоровье уже полное";return false;}
    if(cures&&h.hp>=h.maxHp&&!h.poison&&!h.burn&&!h.web&&!h.slow){reason="Нечего лечить";return false;}
    if(game.itemCooldown>0){char buf[48];std::snprintf(buf,sizeof(buf),"Зелья перезаряжаются: %.1f с",double(game.itemCooldown));reason=buf;return false;}
    game.itemCooldown=(heroClass==combat::Class::Rogue&&(h.talents&combat::T3))?.75f:1.5f;
    h.hp=std::min(h.maxHp,h.hp+d->heal);
    if(cures){h.poison=h.burn=h.web=h.slow=0;}
    game.message(std::string(d->name)+": +"+std::to_string(d->heal)+" HP");
    game.events.push_back({combat::Event::Heal,0,0,d->heal,h.cell,""});
    removeItem(d->id,1);return true;
}
bool Campaign::sell(int index,std::string& reason){
    if(index<0||index>=int(inventory.size())){reason="Нет предмета";return false;}
    auto& s=inventory[size_t(index)];auto d=item(s.id);
    if(!d||d->type==ItemType::Quest){reason="Сюжетный предмет нельзя продать";return false;}
    if(d->rarity==Rarity::Unique){reason="Уникальную вещь лучше сохранить — продажа запрещена";return false;}
    int price=sellPrice(*d,s.upgrade);gold+=price;
    if(--s.count<=0)inventory.erase(inventory.begin()+index);
    toast("Продано: "+std::string(d->name)+" +"+std::to_string(price)+" з.");return true;
}
std::vector<std::pair<std::string,int>> Campaign::shopStock(const std::string& npc) const{
    std::vector<std::pair<std::string,int>> out;
    std::vector<const char*> ids=npc=="smith"?std::vector<const char*>{"militia_sword","steel_daggers","oak_staff","hunting_bow","chainmail"}:
        std::vector<const char*>{"potion","greater_potion","antidote","leather_armor","mage_robe","wolf_tooth","iron_ring"};
    for(auto id:ids){auto d=item(id);if(d)out.push_back({id,d->price});}
    return out;
}
bool Campaign::buy(const std::string& npc,const std::string& id,std::string& reason){
    for(const auto& [sid,price]:shopStock(npc))if(sid==id){
        if(gold<price){reason="Не хватает золота ("+std::to_string(price)+")";return false;}
        gold-=price;addItem(id);return true;
    }
    reason="Нет в продаже";return false;
}
bool Campaign::canCraft(const Recipe& r,std::string& reason) const{
    if(!check(r.cond)){reason="Рецепт ещё не изучен";return false;}
    if(gold<r.gold){reason="Нужно "+std::to_string(r.gold)+" золота";return false;}
    for(const auto& [id,n]:r.needs)if(count(id)<n){reason=std::string("Не хватает: ")+item(id)->name+" ("+std::to_string(count(id))+"/"+std::to_string(n)+")";return false;}
    return true;
}
bool Campaign::craft(const Recipe& r,std::string& reason){
    if(!canCraft(r,reason))return false;
    gold-=r.gold;for(const auto& [id,n]:r.needs)removeItem(id,n);addItem(r.result,r.count);return true;
}
bool Campaign::canUpgrade(std::string& reason) const{
    if(!flags.count("smith_upgrades")){reason="Кузнецу нужен особый металл из глубин шахты (победите Смотрителя)";return false;}
    int next=equip[0].upgrade+1;if(next>maxUpgrade){reason="Оружие закалено до предела (+3)";return false;}
    auto c=upgradeCost(next);
    if(gold<c.gold){reason="Нужно "+std::to_string(c.gold)+" золота";return false;}
    if(count("ore")<c.ore){reason="Нужно руды: "+std::to_string(c.ore)+" (есть "+std::to_string(count("ore"))+")";return false;}
    if(c.extraCount&&count(c.extra)<c.extraCount){reason=std::string("Нужен материал: ")+item(c.extra)->name;return false;}
    return true;
}
bool Campaign::upgradeWeapon(std::string& reason){
    if(!canUpgrade(reason))return false;
    int next=equip[0].upgrade+1;auto c=upgradeCost(next);
    gold-=c.gold;removeItem("ore",c.ore);if(c.extraCount)removeItem(c.extra,c.extraCount);
    equip[0].upgrade=next;applyEquipment();toast(std::string(item(equip[0].id)->name)+" закалён до +"+std::to_string(next),2);return true;
}
bool Campaign::learnTalent(int tier,std::string& reason){
    auto& h=hero();
    if(tier>=3){ // spare points: attributes
        if(h.skillPoints<1){reason="Нет очков развития";return false;}
        --h.skillPoints;if(tier==3){h.maxHp+=8;h.hp+=8;toast("Закалка: +8 здоровья");}else{h.power+=1;toast("Мастерство: +1 сила");}
        return true;
    }
    static const int levelReq[]={2,4,6};
    if(h.talents&(1<<tier)){reason="Уже изучено";return false;}
    if(tier>0&&!(h.talents&(1<<(tier-1)))){reason="Сначала изучите предыдущее улучшение";return false;}
    if(h.level<levelReq[tier]){reason="Нужен уровень "+std::to_string(levelReq[tier]);return false;}
    if(h.skillPoints<1){reason="Нет очков развития";return false;}
    --h.skillPoints;h.talents|=1<<tier;applyEquipment();
    toast(std::string("Изучено: ")+combat::talentName(heroClass,tier),2);return true;
}
void Campaign::chooseReward(int index){
    if(index<0||index>=int(rewardChoices.size()))return;
    std::string id=rewardChoices[size_t(index)];flags.insert("reward_spider");flags.insert("unique:"+id);addItem(id);rewardChoices.clear();
}

// ------------------------------------------------------------------ shrines and defeat
std::vector<std::string> Campaign::activeShrines() const{
    std::vector<std::string> out;
    for(const auto& z:zones())for(const auto& e:z.entities)if(e.type==EntityType::Shrine&&shrineActive(e.id))out.push_back(e.id);
    return out;
}
void Campaign::rest(){
    auto& h=hero();h.hp=h.maxHp;game.clearStatuses(h);
    killed.clear();gathered.clear();
    Cell here=h.cell;std::string zone=zoneId;
    enterZone(zone,'\0',here);
    toast("Вы отдохнули. Здоровье восстановлено; обычные враги и травы вернулись.",1);
}
bool Campaign::travel(const std::string& shrine,std::string& reason){
    if(game.inBattle()){reason="Нельзя перемещаться во время боя";return false;}
    if(!shrineActive(shrine)){reason="Святилище ещё не активировано";return false;}
    lastShrine=shrine;enterAtShrine(shrine);autosavePending=true;return true;
}
bool Campaign::descend(const std::string& shrine,std::string& reason){
    std::string target=shrineDungeon(shrine);
    if(target.empty()){reason="У этого святилища нет подземелья";return false;}
    std::string zone=target.substr(0,target.find('@'));
    if(!flags.count("found:"+zone)){reason="Сначала найдите вход в подземелье на поверхности";return false;}
    if(game.inBattle()){reason="Нельзя спуститься во время боя";return false;}
    enterZone(zone,target[target.find('@')+1]);autosavePending=true;return true;
}
void Campaign::respawnAtShrine(){
    int lost=gold/10;gold-=lost;
    auto& h=hero();h.hp=h.maxHp;game.clearStatuses(h);game.phase=combat::Phase::Explore;game.telegraphs.clear();game.hazards.clear();game.projectiles.clear();for(auto& a:game.actors){a.aggro=false;a.participant=false;}
    killed.clear();gathered.clear();flags.erase("defeat_pending");
    enterAtShrine(lastShrine);
    toast("Вы очнулись у святилища. Потеряно "+std::to_string(lost)+" золота.",1);
}

// ------------------------------------------------------------------ saves
static std::string nowText(long long stamp){
    std::time_t t=std::time_t(stamp);std::tm tm{};
#ifdef _WIN32
    localtime_s(&tm,&t);
#else
    localtime_r(&t,&tm);
#endif
    char buf[32];std::strftime(buf,sizeof(buf),"%d.%m.%Y %H:%M",&tm);return buf;
}
bool Campaign::save(const fs::path& path,std::string& error) const{
    if(game.inBattle()){error="Нельзя сохраняться во время боя";return false;}
    std::ostringstream o;const auto& h=hero();
    long long stamp=std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch()).count();
    o<<"OSSUARY-SAVE 1\n";
    o<<"stamp="<<stamp<<"\nclass="<<int(heroClass)<<"\nzone="<<zoneId<<"\ncell="<<h.cell.x<<","<<h.cell.y<<"\n";
    o<<"level="<<h.level<<"\nxp="<<h.xp<<"\npoints="<<h.skillPoints<<"\nmaxhp="<<h.maxHp-itemHp<<"\nhp="<<h.hp<<"\npower="<<h.power<<"\narmor="<<h.armor<<"\ntalents="<<h.talents<<"\n";
    o<<"gold="<<gold<<"\nshrine="<<lastShrine<<"\nplaytime="<<int(playTime)<<"\n";
    for(int i=0;i<3;++i)if(!equip[i].id.empty())o<<"equip="<<i<<","<<equip[i].id<<","<<equip[i].upgrade<<"\n";
    for(const auto& s:inventory)o<<"item="<<s.id<<","<<s.count<<","<<s.upgrade<<"\n";
    for(const auto& f:flags)if(f!="defeat_pending")o<<"flag="<<f<<"\n";
    for(const auto& g:gathered)o<<"gathered="<<g<<"\n";
    for(const auto& [q,s]:quests)o<<"quest="<<q<<","<<s<<"\n";
    for(const auto& n:notes)o<<"note="<<n<<"\n";
    o<<"end=1\n";
    std::error_code ec;if(path.has_parent_path())fs::create_directories(path.parent_path(),ec);
    fs::path temp=path;temp+=".tmp";
    {std::ofstream out(temp,std::ios::binary|std::ios::trunc);if(!out){error="Не удалось открыть файл для записи: "+temp.string();return false;}
     out<<o.str();out.flush();if(!out){error="Ошибка записи сохранения";return false;}}
    if(fs::exists(path,ec)){fs::path backup=path;backup+=".bak";fs::copy_file(path,backup,fs::copy_options::overwrite_existing,ec);}
    fs::rename(temp,path,ec);
    if(ec){error="Не удалось заменить файл сохранения: "+ec.message();return false;}
    return true;
}
static std::vector<std::string> split(const std::string& s,char c){std::vector<std::string> out;std::stringstream ss(s);std::string p;while(std::getline(ss,p,c))out.push_back(p);return out;}
bool Campaign::load(const fs::path& path,std::string& error){
    std::ifstream in(path,std::ios::binary);if(!in){error="Файл сохранения не найден";return false;}
    std::string line;std::getline(in,line);if(line.rfind("OSSUARY-SAVE 1",0)!=0){error="Неизвестный формат сохранения";return false;}
    Campaign c;c.started=true;std::map<std::string,std::string> kv;bool complete=false;
    std::vector<Stack> inv;Stack eq[3];std::set<std::string> fl,ga;std::map<std::string,int> qs;std::vector<std::string> ns;
    try{
        while(std::getline(in,line)){
            if(!line.empty()&&line.back()=='\r')line.pop_back();
            auto eqPos=line.find('=');if(eqPos==std::string::npos)continue;
            std::string k=line.substr(0,eqPos),v=line.substr(eqPos+1);
            if(k=="equip"){auto p=split(v,',');if(p.size()==3&&item(p[1]))eq[std::stoi(p[0])%3]={p[1],1,std::clamp(std::stoi(p[2]),0,maxUpgrade)};}
            else if(k=="item"){auto p=split(v,',');if(p.size()==3&&item(p[0]))inv.push_back({p[0],std::max(1,std::stoi(p[1])),std::clamp(std::stoi(p[2]),0,maxUpgrade)});}
            else if(k=="flag")fl.insert(v);
            else if(k=="gathered")ga.insert(v);
            else if(k=="quest"){auto p=split(v,',');if(p.size()==2)qs[p[0]]=std::stoi(p[1]);}
            else if(k=="note")ns.push_back(v);
            else if(k=="end")complete=true;
            else kv[k]=v;
        }
        if(!complete){error="Сохранение повреждено (файл обрезан)";return false;}
        int cls=std::stoi(kv.at("class"));if(cls<0||cls>2){error="Неверный класс героя";return false;}
        if(!zoneDef(kv.at("zone"))){error="Неизвестная зона в сохранении";return false;}
        c.heroClass=combat::Class(cls);
        combat::Actor h=combat::Game::makeHero(c.heroClass);c.game.heroClass=c.heroClass;
        h.level=std::stoi(kv.at("level"));h.xp=std::stoi(kv.at("xp"));h.skillPoints=std::stoi(kv.at("points"));
        h.maxHp=std::stoi(kv.at("maxhp"));h.hp=std::stoi(kv.at("hp"));h.power=std::stoi(kv.at("power"));h.armor=std::stoi(kv.at("armor"));
        h.talents=std::stoi(kv.at("talents"))&7;
        c.game.actors={h};
        c.gold=std::stoi(kv.at("gold"));c.lastShrine=kv.at("shrine");c.playTime=std::stod(kv["playtime"].empty()?"0":kv["playtime"]);
        c.inventory=inv;for(int i=0;i<3;++i)c.equip[i]=eq[i];c.flags=fl;c.gathered=ga;c.quests=qs;c.notes=ns;
        if(c.equip[0].id.empty()){const char* weapon[]={"rusty_sword","worn_daggers","apprentice_staff"};c.equip[0]={weapon[cls],1,0};}
        c.applyEquipment();
        c.hero().hp=std::clamp(h.hp,1,c.hero().maxHp);
        auto cell=split(kv.at("cell"),',');
        c.enterZone(kv.at("zone"),'\0',{std::stoi(cell.at(0)),std::stoi(cell.at(1))});
    }catch(const std::exception&){error="Сохранение повреждено (неверные данные)";return false;}
    *this=std::move(c);
    // Re-point the combat map to this object's map after the move.
    game.setMap(&map);
    return true;
}
SaveInfo Campaign::inspect(const fs::path& path){
    SaveInfo info;std::error_code ec;if(!fs::exists(path,ec))return info;info.exists=true;
    std::ifstream in(path);std::string line;std::getline(in,line);
    if(line.rfind("OSSUARY-SAVE 1",0)!=0){info.error="Неизвестный формат";return info;}
    bool end=false;
    while(std::getline(in,line)){
        if(!line.empty()&&line.back()=='\r')line.pop_back();
        auto p=line.find('=');if(p==std::string::npos)continue;std::string k=line.substr(0,p),v=line.substr(p+1);
        try{
            if(k=="stamp"){info.stamp=std::stoll(v);info.time=nowText(info.stamp);}
            else if(k=="level")info.level=std::stoi(v);
            else if(k=="zone"){info.zone=v;info.zoneName=zoneName(v);}
            else if(k=="class"){int c=std::stoi(v);if(c>=0&&c<=2)info.className=combat::className(combat::Class(c));}
            else if(k=="end")end=true;
        }catch(const std::exception&){}
    }
    info.valid=end&&!info.zone.empty()&&info.level>0;
    if(!info.valid&&info.error.empty())info.error="Файл повреждён";
    return info;
}
}
