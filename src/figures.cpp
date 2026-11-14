// Procedural creature and villager models. Each enemy type has its own silhouette and palette.
#include "zone_mesh.hpp"
#include <cmath>

using glm::vec3;
using combat::Kind;

namespace {
struct Fig {
    Mesh& m; vec3 p; float yaw, s;
    vec3 at(float x,float y,float z) const {
        float c=std::cos(yaw),sn=std::sin(yaw);
        return p+vec3(c*x+sn*z,y,-sn*x+c*z)*s;
    }
    void ball(float x,float y,float z,vec3 r,vec3 col,float mt,int sl=9,int st=6){m.sphere(at(x,y,z),r*s,col,mt,sl,st);}
    void limb(vec3 a,vec3 b,float r,vec3 col,float mt){m.beam(at(a.x,a.y,a.z),at(b.x,b.y,b.z),r*s,col,mt,6);}
    void eyes(float y,float z,float spread,vec3 col,float size=.05f){for(float x:{-spread,spread})ball(x,y,z,vec3(size),col,mat::Glow,6,4);}
};
void spider(Fig f,vec3 body,vec3 mark,float legLen,float time,bool big){
    float bob=std::sin(time*6)*.03f;
    f.ball(0,.45f+bob,.25f,{.42f,.34f,.5f},body,mat::Cloth);
    f.ball(0,.40f+bob,-.25f,{.25f,.22f,.25f},body*1.1f,mat::Cloth);
    f.ball(0,.62f+bob,.3f,{.16f,.06f,.2f},mark,mat::Glow,6,4);
    f.eyes(.48f+bob,-.47f,.08f,{1.f,.2f,.1f},.04f);
    if(big)f.eyes(.56f+bob,-.44f,.14f,{1.f,.2f,.1f},.035f);
    for(int side=-1;side<=1;side+=2)for(int k=0;k<4;++k){
        float z=-.25f+float(k)*.17f,ph=time*8+float(k)*1.3f+(side>0?1.5f:0);
        vec3 hip(float(side)*.18f,.45f+bob,z),knee(float(side)*legLen*.55f,.75f+.08f*std::sin(ph),z*1.4f),foot(float(side)*legLen,0,z*1.8f+.06f*std::cos(ph));
        f.limb(hip,knee,.035f,body*.8f,mat::Cloth);f.limb(knee,foot,.03f,body*.7f,mat::Cloth);
    }
}
void skeletonBody(Fig f,vec3 bone,float time,bool archer,bool knight){
    float sway=std::sin(time*2.5f)*.03f;
    f.ball(0,1.62f,0,{.21f,.24f,.2f},bone,mat::Plaster);
    f.eyes(1.64f,-.17f,.08f,knight?vec3(.3f,.6f,1.f):vec3(1.f,.5f,.2f));
    f.limb({0,.75f,0},{sway,1.4f,0},.07f,bone,mat::Plaster);
    for(int r=0;r<4;++r)f.limb({-.2f,.98f+float(r)*.1f,0},{.2f,.98f+float(r)*.1f,0},.03f,bone,mat::Plaster);
    for(float side:{-1.f,1.f}){
        f.limb({side*.13f,.05f,0},{side*.15f,.75f,0},.05f,bone,mat::Plaster);
        f.limb({side*.24f,1.35f,0},{side*.36f,.85f,-.2f},.04f,bone,mat::Plaster);
    }
    if(archer){f.limb({-.4f,.7f,-.35f},{-.4f,1.7f,-.35f},.025f,{.35f,.25f,.12f},mat::Wood);f.limb({-.4f,.7f,-.35f},{-.32f,1.2f,-.3f},.008f,{.8f,.8f,.7f},mat::Plaster);}
    else f.limb({.36f,.8f,-.2f},{.36f,1.7f,-.35f},.03f,{.55f,.56f,.6f},mat::Steel);
    if(knight){
        vec3 steel{.32f,.33f,.36f};
        f.m.block(f.at(0,1.15f,0),vec3(.55f,.6f,.32f)*f.s,.08f*f.s,steel,mat::Steel,f.yaw);
        f.ball(0,1.66f,0,{.25f,.22f,.25f},steel,mat::Steel);
        f.m.block(f.at(-.42f,1.f,-.12f),vec3(.08f,.7f,.5f)*f.s,.03f*f.s,{.25f,.12f,.12f},mat::Steel,f.yaw);
    }else if(!archer)f.m.block(f.at(-.4f,1.0f,-.1f),vec3(.06f,.45f,.4f)*f.s,.02f*f.s,{.4f,.3f,.18f},mat::Wood,f.yaw);
}
void humanoid(Fig f,vec3 cloth,vec3 skin,float time,int style){
    // style 0 bandit, 1 archer, 2 infected miner
    float step=std::sin(time*3)*.04f;
    for(float side:{-1.f,1.f})f.limb({side*.13f,0,0},{side*.14f,.8f,step*side},.08f,cloth*.7f,mat::Cloth);
    f.m.cylinder(f.at(0,1.15f,0),.30f*f.s,.24f*f.s,.75f*f.s,cloth,mat::Cloth,9);
    f.ball(0,1.68f,0,{.19f,.21f,.19f},skin,mat::Cloth);
    for(float side:{-1.f,1.f})f.limb({side*.3f,1.45f,0},{side*.42f,.95f,-.15f},.065f,cloth*.9f,mat::Cloth);
    if(style==0){f.ball(0,1.72f,0,{.22f,.13f,.22f},{.25f,.12f,.1f},mat::Cloth);f.limb({.42f,.95f,-.15f},{.5f,1.6f,-.3f},.06f,{.3f,.2f,.12f},mat::Wood);
        f.eyes(1.68f,-.17f,.07f,{.1f,.05f,.05f},.03f);}
    if(style==1){f.ball(0,1.78f,.04f,{.23f,.2f,.24f},cloth*.8f,mat::Cloth);f.limb({-.42f,.6f,-.3f},{-.42f,1.7f,-.3f},.025f,{.35f,.25f,.12f},mat::Wood);}
    if(style==2){f.ball(0,1.8f,0,{.22f,.1f,.22f},{.6f,.5f,.2f},mat::Brass);f.eyes(1.67f,-.17f,.07f,{.4f,1.f,.3f});
        f.limb({.42f,.95f,-.15f},{.45f,1.5f,-.35f},.04f,{.35f,.25f,.15f},mat::Wood);f.limb({.3f,1.5f,-.4f},{.6f,1.5f,-.3f},.04f,{.4f,.42f,.45f},mat::Steel);
        f.ball(.15f,1.2f,-.25f,{.12f,.1f,.08f},{.3f,.6f,.15f},mat::Glow,6,4);}
}
}

void buildEnemy(Mesh& m,const combat::Actor& a,vec3 p,float yaw,float time,bool active){
    float bob=std::sin(time*3+float(a.spawn))*.04f;
    Fig f{m,p,yaw,1};
    switch(a.kind){
    case Kind::Slime:
        f.ball(0,.40f+bob,0,{.52f,.40f+bob,.48f},{.15f,.45f,.23f},mat::Moss);
        f.ball(0,.55f,-.1f,{.25f,.2f,.25f},{.3f,.75f,.35f},mat::Glow,8,5);
        f.eyes(.55f,-.42f,.17f,{.85f,.95f,.6f},.065f);break;
    case Kind::Rat:{
        vec3 c(.40f,.30f,.26f);
        f.ball(0,.32f+bob,0,{.34f,.26f,.48f},c,mat::Cloth);f.ball(0,.36f,-.45f,{.2f,.18f,.22f},c*1.1f,mat::Cloth);
        for(float x:{-.13f,.13f})f.ball(x,.55f,-.38f,{.1f,.12f,.05f},{.6f,.4f,.4f},mat::Cloth,6,4);
        f.eyes(.42f,-.6f,.08f,{.9f,.2f,.1f},.035f);
        f.limb({0,.25f,.45f},{.25f,.1f,.95f},.03f,{.55f,.4f,.4f},mat::Cloth);break;}
    case Kind::Wolf:{
        vec3 fur(.36f,.34f,.33f);float run=std::sin(time*7)*.08f;
        f.ball(0,.72f+bob,.05f,{.32f,.3f,.62f},fur,mat::Cloth);f.ball(0,.86f,-.58f,{.22f,.22f,.26f},fur*1.1f,mat::Cloth);
        f.ball(0,.8f,-.82f,{.1f,.09f,.16f},fur*.8f,mat::Cloth,6,4);
        for(float x:{-.1f,.1f})f.limb({x,1.f,-.55f},{x*1.3f,1.18f,-.5f},.04f,fur,mat::Cloth);
        f.eyes(.92f,-.78f,.09f,{1.f,.8f,.2f},.035f);
        for(float x:{-.17f,.17f})for(float z:{-.38f,.4f})f.limb({x,.6f,z},{x,0,z+run*(z>0?1:-1)},.06f,fur*.85f,mat::Cloth);
        f.limb({0,.85f,.62f},{0,.7f,1.05f},.06f,fur,mat::Cloth);break;}
    case Kind::Bandit:humanoid(f,{.35f,.24f,.16f},{.62f,.47f,.36f},time,0);break;
    case Kind::BanditArcher:humanoid(f,{.22f,.30f,.18f},{.62f,.47f,.36f},time,1);break;
    case Kind::Miner:humanoid(f,{.26f,.28f,.22f},{.48f,.55f,.40f},time,2);break;
    case Kind::Spiderling:{Fig g=f;g.s=.6f;spider(g,{.20f,.16f,.14f},{.6f,.5f,.1f},.75f,time,false);break;}
    case Kind::VenomSpider:{Fig g=f;g.s=.85f;spider(g,{.16f,.22f,.12f},{.3f,1.f,.2f},.8f,time,false);break;}
    case Kind::SpiderGuard:{Fig g=f;g.s=1.15f;spider(g,{.24f,.18f,.24f},{.9f,.25f,.6f},.9f,time,true);
        g.m.block(g.at(0,.8f,.1f),vec3(.6f,.12f,.7f)*g.s,.05f,{.35f,.28f,.3f},mat::Steel,g.yaw);break;}
    case Kind::Matriarch:{Fig g=f;g.s=1.9f;spider(g,{.22f,.10f,.16f},{1.f,.25f,.5f},.95f,time,true);
        g.ball(0,.95f,.45f,{.45f,.4f,.5f},{.35f,.12f,.24f},mat::Cloth);
        for(int k=0;k<5;++k)g.ball(-.3f+float(k)*.15f,1.25f,.4f+float(k%2)*.1f,vec3(.05f),{1.f,.3f,.6f},mat::Glow,5,3);break;}
    case Kind::Golem:{
        vec3 rock(.40f,.38f,.35f);float sw=std::sin(time*1.5f)*.05f;
        m.block(f.at(0,1.f+bob,0),vec3(.85f,.9f,.6f),.15f,rock,mat::Stone,yaw);
        m.block(f.at(0,1.75f,-.05f),vec3(.45f,.4f,.42f),.1f,rock*1.1f,mat::Stone,yaw+.2f);
        f.eyes(1.78f,-.27f,.1f,{1.f,.55f,.15f},.06f);
        for(float side:{-1.f,1.f}){m.block(f.at(side*.62f,1.0f+sw*side,-.1f),vec3(.35f,.9f,.35f),.1f,rock*.9f,mat::Stone,yaw);
            m.block(f.at(side*.25f,.3f,0),vec3(.35f,.6f,.35f),.08f,rock*.85f,mat::Stone,yaw);}
        f.ball(0,1.05f,-.32f,{.15f,.15f,.05f},{1.f,.5f,.15f},mat::Glow,6,4);break;}
    case Kind::Skeleton:skeletonBody(f,{.72f,.68f,.55f},time,false,false);break;
    case Kind::SkeletonArcher:skeletonBody(f,{.66f,.63f,.55f},time,true,false);break;
    case Kind::KnightGuard:skeletonBody(f,{.6f,.58f,.52f},time,false,true);break;
    case Kind::Ghost:{
        float fl=.35f+std::sin(time*2+float(a.spawn))*.15f;vec3 g(.12f,.34f,.42f);
        m.cylinder(f.at(0,.65f+fl,0),.5f,.18f,1.1f,g*.8f,mat::Glow,9);
        f.ball(0,1.32f+fl,0,{.26f,.3f,.25f},g,mat::Glow);
        for(int k=0;k<5;++k){float an=float(k)*1.256f+time;m.beam(f.at(std::cos(an)*.35f,.15f+fl,std::sin(an)*.35f),f.at(std::cos(an)*.5f,-.2f+fl,std::sin(an)*.5f),.04f,g*.6f,mat::Glow,4);}
        f.eyes(1.35f+fl,-.23f,.09f,{.9f,1.f,1.f},.05f);break;}
    case Kind::Ogre:{Fig g=f;g.s=1.45f;humanoid(g,{.35f,.40f,.21f},{.45f,.5f,.3f},time,0);break;}
    case Kind::Warden:{
        vec3 iron(.28f,.27f,.28f),rust(.45f,.26f,.14f);Fig g=f;g.s=1.55f;float sw=std::sin(time*1.4f)*.04f;
        m.block(g.at(0,1.2f+bob,0),vec3(1.0f,1.0f,.75f)*g.s,.12f,iron,mat::Steel,yaw);
        m.block(g.at(0,1.95f,0),vec3(.55f,.45f,.5f)*g.s,.08f,iron*1.1f,mat::Steel,yaw);
        g.m.block(g.at(0,1.97f,-.24f),vec3(.4f,.08f,.04f)*g.s,.01f,a.vulnerable?vec3(1.f,.9f,.3f):vec3(1.f,.3f,.1f),mat::Glow,yaw);
        for(float side:{-1.f,1.f}){m.block(g.at(side*.7f,1.1f+sw*side,-.1f),vec3(.4f,1.0f,.4f)*g.s,.1f,rust,mat::Brass,yaw);
            m.block(g.at(side*.3f,.35f,0),vec3(.4f,.7f,.45f)*g.s,.08f,iron*.9f,mat::Steel,yaw);}
        g.ball(0,1.25f,-.4f,{.18f,.18f,.06f},a.vulnerable?vec3(1.f,.9f,.3f):vec3(1.f,.45f,.15f),mat::Glow,8,5);
        if(a.stance==1)g.m.cylinder(g.at(0,1.2f,0),1.0f*g.s,1.0f*g.s,2.2f*g.s,{.2f,.4f,.7f},mat::Glow,14);
        break;}
    case Kind::FallenKnight:{
        Fig g=f;g.s=1.35f;skeletonBody(g,{.55f,.52f,.48f},time,false,true);
        vec3 cape=a.enraged?vec3(.55f,.08f,.06f):vec3(.20f,.10f,.25f);
        g.m.quad(g.at(-.3f,1.45f,.18f),g.at(.3f,1.45f,.18f),g.at(.4f,.2f,.35f),g.at(-.4f,.2f,.35f),cape,mat::Banner);
        g.ball(0,1.95f,0,{.12f,.1f,.12f},{.9f,.75f,.3f},mat::Brass,6,4);
        if(a.stance==1)g.m.block(g.at(-.45f,1.f,-.2f),vec3(.12f,1.0f,.7f)*g.s,.04f,{.35f,.3f,.4f},mat::Steel,g.yaw);
        break;}
    case Kind::BoneKeeper:{
        Fig g=f;g.s=1.8f;vec3 bone(.78f,.74f,.62f);float fl=std::sin(time*1.5f)*.1f;
        g.m.cylinder(g.at(0,1.1f+fl,0),.65f*g.s,.35f*g.s,1.6f*g.s,{.18f,.10f,.14f},mat::Cloth,10);
        g.ball(0,2.15f+fl,0,{.3f,.34f,.3f},bone,mat::Plaster);
        g.eyes(2.18f+fl,-.25f,.1f,{1.f,.2f,.15f},.06f);
        for(int k=0;k<6;++k){float an=float(k)*1.047f+time*.4f;g.limb({0,2.f+fl,0},{std::cos(an)*.9f,2.6f+fl+float(k%2)*.3f,std::sin(an)*.9f},.04f,bone,mat::Plaster);}
        for(float side:{-1.f,1.f})g.limb({side*.4f,1.7f+fl,0},{side*.85f,1.0f+fl,-.35f},.06f,bone,mat::Plaster);
        g.ball(0,1.4f+fl,-.35f,{.18f,.18f,.08f},{1.f,.15f,.1f},mat::Glow,8,5);
        if(a.vulnerable==0&&a.phaseTwo==false){g.m.sphere(g.at(0,1.5f,0),vec3(1.25f,1.6f,1.25f)*g.s,{.25f,.08f,.35f},mat::Glow,12,8);}
        break;}
    case Kind::Pylon:{
        vec3 bone(.75f,.7f,.6f);
        m.block(p+vec3(0,.2f,0),vec3(.9f,.4f,.9f),.08f,{.25f,.18f,.18f},mat::Stone);
        for(int k=0;k<5;++k){float an=float(k)*1.256f;m.beam(p+vec3(std::cos(an)*.3f,.3f,std::sin(an)*.3f),p+vec3(std::cos(an)*.1f,2.2f,std::sin(an)*.1f),.07f,bone,mat::Plaster,5);}
        m.sphere(p+vec3(0,2.4f+std::sin(time*2)*.1f,0),vec3(.28f),{1.f,.2f,.4f},mat::Glow,8,6);
        break;}
    default:{
        Fig g=f;skeletonBody(g,{.7f,.65f,.5f},time,false,false);break;}
    }
    if(a.vulnerable>0)m.cylinder(p+vec3(0,.05f,0),.9f,.9f,.02f,{1.f,.85f,.2f},mat::Glow,16);
    (void)active;
}

void buildNpc(Mesh& m,const std::string& id,vec3 p,float yaw,float time){
    Fig f{m,p,yaw,1.f};float breathe=std::sin(time*2)*.015f;
    vec3 cloth{.3f,.3f,.35f},skin{.68f,.52f,.40f},hair{.25f,.18f,.12f};
    if(id=="elder"){cloth={.30f,.22f,.40f};hair={.82f,.82f,.8f};}
    if(id=="smith"){cloth={.30f,.22f,.15f};hair={.15f,.1f,.08f};}
    if(id=="merchant"){cloth={.55f,.25f,.20f};hair={.45f,.25f,.12f};}
    if(id=="herbalist"){cloth={.25f,.42f,.25f};hair={.55f,.4f,.2f};}
    if(id=="survivor"){cloth={.30f,.32f,.28f};}
    m.cylinder(f.at(0,.62f,0),.36f,.27f,1.25f,cloth,mat::Cloth,10);
    f.ball(0,1.5f+breathe,0,{.27f,.22f,.25f},cloth*1.1f,mat::Cloth);
    f.ball(0,1.86f+breathe,0,{.18f,.21f,.18f},skin,mat::Cloth);
    f.ball(0,1.98f+breathe,.04f,{.19f,.12f,.19f},hair,mat::Cloth);
    for(float side:{-1.f,1.f})f.limb({side*.3f,1.55f,0},{side*.36f,1.05f,-.12f},.06f,cloth*.9f,mat::Cloth);
    if(id=="elder"){f.limb({.4f,0,-.2f},{.4f,1.9f,-.2f},.035f,{.35f,.25f,.15f},mat::Wood);f.ball(0,1.72f,-.15f,{.12f,.14f,.06f},hair,mat::Cloth);}
    if(id=="smith"){m.block(f.at(0,1.f,-.2f),vec3(.5f,.8f,.05f),.01f,{.2f,.15f,.12f},mat::Cloth,yaw);
        f.limb({.36f,1.05f,-.12f},{.4f,1.3f,-.45f},.03f,{.3f,.2f,.1f},mat::Wood);m.block(f.at(.4f,1.35f,-.5f),vec3(.25f,.12f,.12f),.02f,{.3f,.3f,.32f},mat::Steel,yaw);
        m.block(f.at(.9f,.4f,-.4f),vec3(.7f,.35f,.35f),.04f,{.2f,.2f,.22f},mat::Steel,yaw);
        m.block(f.at(.9f,.15f,-.4f),vec3(.35f,.3f,.3f),.03f,{.25f,.18f,.12f},mat::Wood,yaw);}
    if(id=="merchant"){m.block(f.at(0,.5f,-.85f),vec3(1.6f,1.f,.6f),.04f,{.42f,.30f,.17f},mat::Wood,yaw);
        m.quad(f.at(-.9f,2.3f,-1.2f),f.at(.9f,2.3f,-1.2f),f.at(.9f,2.f,-.3f),f.at(-.9f,2.f,-.3f),{.7f,.25f,.2f},mat::Cloth);
        for(float x:{-.85f,.85f})f.limb({x,0,-1.1f},{x,2.3f,-1.15f},.04f,{.35f,.25f,.15f},mat::Wood);
        for(int k=0;k<3;++k)f.ball(-.4f+float(k)*.4f,1.08f,-.8f,vec3(.1f),k==1?vec3(.3f,.8f,.4f):vec3(.85f,.2f,.2f),mat::Glow,6,4);}
    if(id=="herbalist"){f.ball(.4f,1.f,-.2f,{.16f,.12f,.16f},{.4f,.3f,.15f},mat::Wood,7,4);for(int k=0;k<3;++k)f.limb({.35f+float(k)*.05f,1.1f,-.2f},{.3f+float(k)*.08f,1.35f,-.25f},.015f,{.3f,.6f,.25f},mat::Foliage);}
    // a soft marker ring shows that the villager can be spoken to
    m.cylinder(p+vec3(0,.02f,0),.75f,.75f,.01f,{.6f,.5f,.2f},mat::Glow,16);
}
