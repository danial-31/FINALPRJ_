#pragma once
#include "rpg.hpp"
#include "scene.hpp"

// Material ids understood by shaders/scene.frag.
namespace mat {
inline constexpr float Stone=0, Masonry=1, Wood=2, Steel=3, Brass=4, Cloth=5, Plaster=6,
    Glow=7, Ember=8, Moss=9, Banner=11, Water=12, Ground=13, Foliage=14, Snow=15;
}

struct Emitter { glm::vec3 position; int kind; float scale; };   // 0 fire, 1 chimney smoke, 2 poison bubbles, 3 fireflies, 4 shrine motes, 5 corruption
struct ZoneLight { glm::vec3 position, color; float radius; bool shadow; };

struct ZoneVisuals {
    Mesh mesh;                         // two-sided organic shapes (culling off)
    Mesh solid;                        // outward-wound blocks and ground (back-face culled)
    Mesh animated;                     // shader-deformed scenery, never cached in point shadows
    Mesh objects, objectSolid;         // interactive scenery, rebuilt independently of terrain
    std::vector<ZoneLight> lights;
    std::vector<Emitter> emitters;
    std::vector<ZoneLight> objectLights;
    std::vector<Emitter> objectEmitters;
    std::vector<Torch> torches;        // used by the original throne-room props
    std::vector<float> terrainHeights;
    bool cleansed=false;
};

// Build terrain on zone changes; interactive changes only rebuild the small object layer.
void buildZoneVisuals(const rpg::Campaign& c, ZoneVisuals& out);
void updateZoneObjects(const rpg::Campaign& c, ZoneVisuals& out);
// Animated figures (enemies, NPCs) and floor markers; rebuilt every frame.
inline constexpr float enemyRemainsSeconds=3.25f;
void buildEnemy(Mesh& m, const combat::Actor& a, glm::vec3 p, float yaw, float phase, float moveWeight,
                float deathSeconds=-1.f,float hitSeconds=1.f);
void buildNpc(Mesh& m, const std::string& id, glm::vec3 p, float yaw, float time);
void buildLootBag(Mesh& m, glm::vec3 p, float time);
void floorTile(Mesh& m, const world::Map& map, world::Cell cell, glm::vec3 color, float thickness, float lift=.06f);
void fillTile(Mesh& m, const world::Map& map, world::Cell cell, glm::vec3 color, float inset, float lift=.05f);
std::uint32_t entityStateHash(const rpg::Campaign& c);
