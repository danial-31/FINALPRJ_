// A standalone target: include the procedural builders to inspect each prop's vertices.
#include "../src/props.cpp"
#include <iostream>

namespace {
bool fits(const Mesh& mesh, layout::Footprint footprint, const char* name)
{
    const glm::vec3 center=layout::center(footprint);
    const float halfX=float(footprint.width)*layout::cellSize*.5f-.049f;
    const float halfZ=float(footprint.depth)*layout::cellSize*.5f-.049f;
    for(const auto& vertex:mesh.vertices) {
        const auto p=vertex.position-center;
        if(!std::isfinite(p.x)||!std::isfinite(p.y)||!std::isfinite(p.z)||
           std::abs(p.x)>halfX||std::abs(p.z)>halfZ) {
            std::cerr<<name<<" exceeds its assigned footprint at "
                     <<vertex.position.x<<", "<<vertex.position.z<<'\n';
            return false;
        }
    }
    if(mesh.vertices.empty()) { std::cerr<<name<<" has no geometry\n";return false; }
    return true;
}
bool inside(glm::vec3 p,layout::Footprint f)
{
    const auto c=layout::center(f);
    return f.width>0&&f.depth>0&&std::abs(p.x-c.x)<=float(f.width)*.7f-.049f&&
           std::abs(p.z-c.z)<=float(f.depth)*.7f-.049f;
}
}
int main()
{
    bool ok=true;
    Mesh scene;std::vector<Torch> lights;
    buildProps(scene,lights);
    // All actual scene vertices must stay inside a blocked object footprint.
    // The seal and hanging banner are attached to the walls above walkable space.
    for(const auto& vertex:scene.vertices) {
        const auto p=vertex.position;
        const bool wallSeal=p.z<-10.4f&&p.y>1.7f&&std::abs(p.x)<1.7f;
        const bool wallBanner=p.x<-13.15f&&p.y>2.3f&&p.z>-4.5f&&p.z<-2.5f;
        if(wallSeal||wallBanner)continue;
        bool contained=false;
        for(const auto f:layout::footprints)contained|=inside(p,f);
        if(!contained) {
            std::cerr<<"Scene prop escapes all assigned cells at "<<p.x<<", "<<p.y<<", "<<p.z<<'\n';
            ok=false;break;
        }
    }
    if(lights.size()!=6) {std::cerr<<"Expected four braziers and two altar light clusters\n";ok=false;}
    for(const auto& light:lights) {
        bool contained=false;
        for(auto f:{layout::altar,layout::brazier0,layout::brazier1,layout::brazier2,layout::brazier3})
            contained|=inside(light.position,f);
        if(!contained){std::cerr<<"Light detached from its prop footprint\n";ok=false;}
    }
    // Test silhouettes individually so an object cannot borrow space from a neighbor.
    Mesh m;
    for(auto f:{layout::statueLeft,layout::statueRight}) {
        m.vertices.clear();statue(m,layout::center(f));
        scaleProp(m,0,layout::center(f),{1.3f/1.5f,1,1.3f/1.5f});
        ok=fits(m,f,"statue")&&ok;
    }
    m.vertices.clear();chest(m,layout::center(layout::chest));
    scaleProp(m,0,layout::center(layout::chest),{2.4f/1.9f,1,.85f});
    ok=fits(m,layout::chest,"chest")&&ok;
    for(auto f:{layout::brazier0,layout::brazier1,layout::brazier2,layout::brazier3}) {
        m.vertices.clear();brazier(m,lights,layout::center(f),2.05f,1);
        ok=fits(m,f,"brazier")&&ok;
    }
    for(auto f:{layout::barrel0,layout::barrel1}) {
        m.vertices.clear();barrel(m,layout::center(f),1.04f);ok=fits(m,f,"barrel")&&ok;
    }
    for(auto f:{layout::urn0,layout::urn1}) {
        m.vertices.clear();urn(m,layout::center(f),1.1f);ok=fits(m,f,"urn")&&ok;
    }
    m.vertices.clear();const auto c=layout::center(layout::crates);
    crate(m,c,{1.13f,.85f,.95f},0);crate(m,c+glm::vec3{0,.89f,0},{.75f,.60f,.68f},0);
    ok=fits(m,layout::crates,"stacked crates")&&ok;
    m.vertices.clear();const auto b=layout::center(layout::books);
    book(m,b+glm::vec3{0,.01f,0},0,cloth);book(m,b+glm::vec3{0,.22f,0},0,wood);
    ok=fits(m,layout::books,"stacked books")&&ok;
    if(ok)std::cout<<"All scene props and lights fit the shared floor grid.\n";
    return ok?0:1;
}
