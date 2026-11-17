#include "zone_mesh.hpp"
#include <iostream>
#include <stdexcept>

static void check(bool ok,const char* message){if(!ok)throw std::runtime_error(message);}
static bool sameMesh(const Mesh& a,const Mesh& b){
    if(a.vertices.size()!=b.vertices.size())return false;
    for(size_t i=0;i<a.vertices.size();++i){const auto& x=a.vertices[i];const auto& y=b.vertices[i];
        if(x.position!=y.position||x.normal!=y.normal||x.color!=y.color||x.material!=y.material)return false;}
    return true;
}
static bool sameLights(const std::vector<ZoneLight>& a,const std::vector<ZoneLight>& b){
    if(a.size()!=b.size())return false;
    for(size_t i=0;i<a.size();++i)
        if(a[i].position!=b[i].position||a[i].color!=b[i].color||a[i].radius!=b[i].radius||a[i].shadow!=b[i].shadow)return false;
    return true;
}
int main(){
    try{
        int changed=0;size_t animated=0;
        for(const auto& zone:rpg::zones()){
            rpg::Campaign c;c.newGame(combat::Class::Warrior);c.enterZone(zone.id,'@');
            ZoneVisuals v;buildZoneVisuals(c,v);
            const auto* terrainStorage=v.solid.vertices.data();
            const Mesh terrain=v.solid,scenery=v.mesh,wind=v.animated;
            animated+=wind.vertices.size();
            for(const Mesh* mesh:{&v.solid,&v.mesh})for(const auto& vertex:mesh->vertices)
                check(vertex.material!=mat::Banner&&vertex.material!=mat::Foliage,"deforming geometry remains in static shadow cache");
            for(auto& e:c.entities){
                using E=rpg::EntityType;
                if(e.def.type!=E::Chest&&e.def.type!=E::Herb&&e.def.type!=E::Gate&&e.def.type!=E::LockDoor&&e.def.type!=E::RuneDoor)continue;
                e.done=true;
                if(e.def.type==E::Herb)e.hidden=true;
                if(e.def.type==E::Gate||e.def.type==E::LockDoor||e.def.type==E::RuneDoor)c.map.set(e.cell.x,e.cell.y,world::Tile::Floor);
                ++changed;
            }
            for(int y=0;y<c.map.height;++y)for(int x=0;x<c.map.width;++x)
                if(c.map.at(x,y)==world::Tile::Trap)c.map.set(x,y,world::Tile::Floor);
            updateZoneObjects(c,v);
            check(v.solid.vertices.data()==terrainStorage,"object change reallocates terrain");
            check(sameMesh(v.solid,terrain)&&sameMesh(v.mesh,scenery)&&sameMesh(v.animated,wind),"object change modifies static scenery");
            ZoneVisuals rebuilt;buildZoneVisuals(c,rebuilt);
            check(sameMesh(v.solid,rebuilt.solid)&&sameMesh(v.mesh,rebuilt.mesh)&&sameMesh(v.animated,rebuilt.animated),"incremental scenery differs from a fresh zone");
            check(sameMesh(v.objects,rebuilt.objects)&&sameMesh(v.objectSolid,rebuilt.objectSolid),"interactive geometry is stale after opening/gathering");
            check(sameLights(v.objectLights,rebuilt.objectLights),"interactive lighting is stale");
        }
        check(changed>0&&animated>0,"missing interactive or animated test coverage");
        std::cout<<"Zone visuals: incremental geometry and animated shadow separation passed\n";
        return 0;
    }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
}
