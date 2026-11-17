// Sculpted, low-poly campaign figures. Model space faces -Z.
#include "zone_mesh.hpp"
#include <glm/gtc/matrix_transform.hpp>
#include <array>
#include <cmath>
#include <initializer_list>

using glm::vec3;
using combat::Kind;

namespace {
const vec3 ink{.112f,.103f,.128f}, iron{.25f,.31f,.35f}, bronze{.68f,.45f,.24f};
const vec3 leather{.29f,.17f,.125f}, oxblood{.42f,.135f,.16f}, jade{.16f,.39f,.37f};
const vec3 ivory{.82f,.74f,.54f}, silver{.64f,.72f,.71f}, ember{.75f,.26f,.09f};
const vec3 skin{.68f,.46f,.33f};
float ease(float begin,float end,float time){
    float u=std::clamp((time-begin)/(end-begin),0.f,1.f);
    return u*u*(3.f-2.f*u);
}
const std::array<glm::vec2,8> octagon = {{
    {1,0},{.7071f,.7071f},{0,1},{-.7071f,.7071f},
    {-1,0},{-.7071f,-.7071f},{0,-1},{.7071f,-.7071f}
}};

// Each section is a horizontal ellipse. Off-axis sections give the anatomy
// its profile in the overhead view, including swept ears, brows and hems.
struct Section { float y, width, depth, z=0, x=0; };

struct Fig {
    Mesh& mesh;
    vec3 base;
    float cs,sn,scale;
    Fig(Mesh& m, vec3 p, float yaw, float size=1)
        :mesh(m),base(p),cs(std::cos(yaw)),sn(std::sin(yaw)),scale(size){}
    vec3 at(vec3 v) const {
        return base+vec3(cs*v.x+sn*v.z,v.y,-sn*v.x+cs*v.z)*scale;
    }
    Fig offset(vec3 v) const {
        Fig copy=*this;copy.base=at(v);return copy;
    }
    void face(vec3 a,vec3 b,vec3 c,vec3 color,float material) const {
        mesh.triangle(at(a),at(b),at(c),color,material);
    }
    void panel(vec3 a,vec3 b,vec3 c,vec3 d,vec3 color,float material) const {
        mesh.quad(at(a),at(b),at(c),at(d),color,material);
    }
    void form(std::initializer_list<Section> profile,vec3 color,float material) const {
        std::array<vec3,8> lower{},upper{};
        bool first=true;
        for(const auto& section:profile){
            vec3 center=at({section.x,section.y,section.z});
            for(int i=0;i<8;++i){
                auto uv=octagon[i];
                upper[i]=at({section.x+section.width*uv.x,section.y,section.z+section.depth*uv.y});
            }
            for(int i=0;i<8;++i){
                int j=(i+1)%8;
                if(first)mesh.triangle(center,upper[i],upper[j],color*.92f,material);
                else mesh.quad(lower[i],upper[i],upper[j],lower[j],color,material);
                if(&section==profile.end()-1)mesh.triangle(center,upper[j],upper[i],color,material);
            }
            lower=upper;first=false;
        }
    }
    void limb(vec3 a,vec3 b,float root,float tip,vec3 color,float material) const {
        a=at(a);b=at(b);
        const vec3 delta=b-a;
        if(glm::dot(delta,delta)<1.e-8f)return;
        vec3 axis=glm::normalize(delta);
        vec3 side=glm::normalize(glm::cross(axis,std::abs(axis.y)>.8f?vec3(1,0,0):vec3(0,1,0)));
        vec3 front=glm::cross(side,axis);
        vec3 lastSide=side*scale;
        for(int i=1;i<=8;++i){
            auto uv=octagon[i%8];
            vec3 nextSide=(side*uv.x+front*uv.y)*scale;
            mesh.quad(a+lastSide*root,b+lastSide*tip,b+nextSide*tip,a+nextSide*root,color,material);
            mesh.triangle(a,a+lastSide*root,a+nextSide*root,color,material);
            mesh.triangle(b,b+nextSide*tip,b+lastSide*tip,color,material);
            lastSide=nextSide;
        }
    }
    void jewel(vec3 p,vec3 r,vec3 color,float material) const {
        offset(p).form({{-r.y,.13f*r.x,.13f*r.z},{0,r.x,r.z},{r.y,.13f*r.x,.13f*r.z}},color,material);
    }
};

void ring(const Fig& f, float y,float radius,float width,vec3 color,float material){
    constexpr int segments=28;
    for(int i=0;i<segments;++i){
        float a=2*pi*i/segments,b=2*pi*(i+1)/segments;
        vec3 u{std::cos(a),0,std::sin(a)},v{std::cos(b),0,std::sin(b)},up{0,y,0};
        f.panel(up+u*radius,up+u*(radius-width),up+v*(radius-width),up+v*radius,color,material);
    }
}
void scarf(const Fig& f,vec3 cloth,float phase){
    f.form({{1.31f,.28f,.225f},{1.43f,.35f,.247f},{1.53f,.17f,.16f}},cloth,mat::Cloth);
    float fold=.033f*std::sin(phase);
    f.panel({-.23f,1.42f,.12f},{.05f,1.43f,.20f},{.12f,.72f,.29f+fold},{-.16f,.67f,.25f+fold},cloth*.77f,mat::Banner);
    f.face({.05f,1.43f,.20f},{.24f,1.4f,.13f},{.12f,.72f,.29f+fold},cloth,mat::Banner);
    f.jewel({-.18f,1.43f,-.18f},{.055f,.06f,.022f},bronze,mat::Brass);
}
void hood(const Fig& f,vec3 color){
    f.form({{1.45f,.20f,.19f,.025f},{1.64f,.245f,.225f,.025f},{1.83f,.21f,.20f,.045f},
        {1.98f,.055f,.09f,.09f}},color,mat::Cloth);
    f.jewel({0,1.65f,-.184f},{.16f,.17f,.055f},ink,mat::Cloth);
    for(float s:{-1.f,1.f})
        f.face({s*.18f,1.8f,-.17f},{s*.08f,1.77f,-.241f},{s*.19f,1.54f,-.185f},color*1.16f,mat::Cloth);
}
void head(const Fig& f,vec3 flesh){
    f.form({{1.49f,.1f,.11f},{1.55f,.13f,.15f,-.035f},{1.67f,.178f,.17f,-.015f},
        {1.80f,.16f,.15f},{1.85f,.10f,.1f}},flesh,mat::Plaster);
    f.jewel({0,1.65f,-.16f},{.043f,.07f,.057f},flesh*.90f,mat::Plaster);
    for(float s:{-1.f,1.f}){
        f.face({s*.022f,1.709f,-.171f},{s*.129f,1.709f,-.117f},{s*.081f,1.674f,-.155f},leather,mat::Cloth);
        f.jewel({s*.084f,1.682f,-.153f},{.021f,.015f,.013f},ink,mat::Cloth);
    }
}
void bow(const Fig& f, vec3 grip,float draw){
    Fig bow=f.offset(grip);
    for(float sign:{-1.f,1.f}){
        vec3 mid{-.028f,sign*.30f,.06f},tip{-.07f,sign*.54f,.24f};
        bow.limb({0,0,0},mid,.041f,.028f,bronze,mat::Wood);
        bow.limb(mid,tip,.032f,.012f,leather,mat::Wood);
        bow.limb(tip,{-.06f,0,.23f+draw},.008f,.008f,ivory,mat::Cloth);
    }
}
void sword(const Fig& hand,float attack){
    vec3 near{0,.14f,-.045f},tip{.13f,.68f-.32f*attack,-.32f-.46f*attack};
    hand.limb({0,-.09f,.03f},near,.037f,.032f,leather,mat::Cloth);
    hand.limb(near-vec3(.13f,0,0),near+vec3(.14f,.02f,0),.028f,.019f,bronze,mat::Brass);
    vec3 left=near-vec3(.055f,0,0),right=near+vec3(.065f,.02f,0);
    vec3 ridge=glm::mix(near,tip,.22f)+vec3(.005f,0,-.045f);
    hand.face(left,tip,ridge,iron,mat::Steel);hand.face(ridge,tip,right,silver,mat::Steel);
    hand.face(left,ridge,right,silver*.82f,mat::Steel);
}
void shield(const Fig& hand,bool heavy){
    float w=heavy?.235f:.19f;
    vec3 upper{0,.37f,-.085f}, bottom{0,-.43f,-.08f};
    for(float side:{-1.f,1.f}){
        vec3 corner{side*w,.25f,-.045f},edge{side*w*.88f,-.19f,-.045f},boss{0,.035f,-.17f};
        hand.face(upper,corner,boss,iron*1.16f,mat::Steel);
        hand.panel(boss,corner,edge,bottom,side<0?oxblood:oxblood*.82f,mat::Banner);
        hand.limb(upper,corner,.018f,.017f,bronze,mat::Brass);
        hand.limb(corner,edge,.017f,.017f,bronze,mat::Brass);
        hand.limb(edge,bottom,.017f,.012f,bronze,mat::Brass);
    }
    hand.jewel({0,.035f,-.17f},{.07f,.09f,.035f},bronze,mat::Brass);
}
void legs(const Fig& f,vec3 trouser,float stride,float weight,bool bony=false,float fold=0){
    for(float side:{-1.f,1.f}){
        float step=side*stride;
        vec3 hip{side*.135f,.84f+weight,0},knee{side*.148f,.46f+weight*.6f,-.055f+step*.48f};
        vec3 heel{side*.15f,.10f+std::max(0.f,-step)*.32f,.03f+step};
        knee+=vec3(side*.043f,-.21f,-.24f)*fold;
        heel+=vec3(side*.066f,0,.10f)*fold;
        f.limb(hip,knee,bony?.063f:.13f,bony?.042f:.09f,trouser,bony?mat::Plaster:mat::Cloth);
        f.limb(knee,heel,bony?.057f:.107f,bony?.032f:.077f,bony?trouser:leather,bony?mat::Plaster:mat::Cloth);
        auto foot=f.offset(heel);
        foot.form({{-.085f,.10f,.155f,-.056f},{-.02f,.11f,.16f,-.06f},{.055f,.082f,.084f}},bony?iron:leather,mat::Cloth);
    }
}
struct Hands { vec3 left,right; };
Hands arms(const Fig& f,vec3 cloth,vec3 hands,float stride,float attack,bool bony){
    Hands result{};
    for(float s:{-1.f,1.f}){
        float extend=s>0?attack:0.f;
        vec3 shoulder{s*.265f,1.39f,0},elbow{s*.343f,1.15f+extend*.10f,.04f+s*stride*.55f};
        vec3 wrist{s*.37f,1.03f+extend*.18f,-.165f+s*stride*.6f-extend*.28f};
        f.limb(shoulder,elbow,bony?.052f:.115f,bony?.035f:.084f,cloth,bony?mat::Plaster:mat::Cloth);
        f.limb(elbow,wrist,bony?.043f:.084f,bony?.029f:.06f,bony?hands:leather,bony?mat::Plaster:mat::Cloth);
        f.jewel(wrist,{.072f,.082f,.073f},hands,bony?mat::Plaster:mat::Cloth);
        (s>0?result.right:result.left)=wrist;
    }
    return result;
}
void humanoid(const Fig& f,Kind kind,float phase,float stride,float attack,float fold){
    bool archer=kind==Kind::BanditArcher,miner=kind==Kind::Miner,ogre=kind==Kind::Ogre;
    vec3 cloth=archer?jade:miner?iron:oxblood;
    vec3 flesh=ogre?vec3(.56f,.51f,.30f):miner?vec3(.57f,.54f,.41f):skin;
    float weight=.016f*std::sin(phase)+std::abs(stride)*.14f-.32f*fold;
    legs(f,iron*.78f,stride,weight,false,fold);
    Fig torso=f.offset({0,weight,-.05f*attack});
    torso.form({{.68f,.29f,.20f,.015f},{.87f,.24f,.17f},{1.02f,.209f,.159f},
        {1.30f,ogre?.40f:.295f,.215f},{1.41f,.27f,.18f},{1.49f,.105f,.10f}},cloth,mat::Cloth);
    torso.form({{.90f,.236f,.18f},{.997f,.23f,.175f}},leather,mat::Cloth);
    torso.jewel({.02f,.95f,-.178f},{.058f,.044f,.02f},bronze,mat::Brass);
    torso.panel({-.21f,1.44f,-.12f},{-.11f,1.46f,-.17f},{.16f,.97f,-.15f},{.09f,.96f,-.18f},leather,mat::Cloth);
    Hands grip=arms(torso,cloth,flesh,stride,attack,false);
    if(archer){
        hood(torso,cloth*.93f);scarf(torso,jade,phase);
        bow(torso,grip.left,attack*.12f);
        torso.limb({-.16f,.92f,.18f},{.13f,1.62f,.22f},.081f,.093f,leather,mat::Wood);
        for(int i=0;i<3;++i){
            vec3 nock{.14f+i*.039f,1.79f+.022f*(i%2),.215f+i*.012f};
            torso.limb(nock-vec3(.075f,.18f,0),nock,.012f,.018f,ivory,mat::Wood);
        }
    }else{
        Fig face=torso.offset(ogre?vec3(0,-.025f,-.14f):vec3(0));
        head(face,flesh);
        if(miner){
            torso.form({{1.72f,.232f,.228f},{1.76f,.235f,.23f},{1.765f,.191f,.181f},
                {1.87f,.16f,.15f},{1.91f,.075f,.073f}},bronze,mat::Brass);
            torso.jewel({0,1.79f,-.187f},{.058f,.056f,.026f},{.42f,.66f,.48f},mat::Glow);
            Fig tool=torso.offset(grip.right);
            tool.limb({0,-.20f,.04f},{0,.44f,-.18f},.028f,.034f,leather,mat::Wood);
            for(float s:{-1.f,1.f})tool.limb({0,.44f,-.18f},{s*.21f,.34f,-.16f},.052f,.008f,silver,mat::Steel);
        }else{
            face.form({{1.731f,.184f,.173f,.02f},{1.82f,.183f,.168f,.025f},{1.89f,.112f,.124f,.043f}},ink,mat::Cloth);
            scarf(torso,ogre?leather:oxblood,phase);
            torso.jewel({-.278f,1.38f,0},{.16f,.115f,.19f},iron,mat::Steel);
            if(ogre){
                face.form({{1.469f,.114f,.119f,-.044f},{1.538f,.191f,.193f,-.056f},{1.6f,.174f,.176f,-.043f}},flesh*.86f,mat::Plaster);
                for(float s:{-1.f,1.f}){
                    face.limb({s*.123f,1.545f,-.209f},{s*.165f,1.702f,-.234f},.035f,.004f,ivory,mat::Plaster);
                    torso.offset({s*.304f,1.32f,0}).form({{-.10f,.13f,.166f},{.022f,.188f,.206f},{.151f,.13f,.156f}},
                        s<0?bronze*.76f:flesh, s<0?mat::Brass:mat::Plaster);
                }
                Fig club=torso.offset(grip.right);
                club.limb({0,-.10f,.04f},{.09f,.49f,-.24f},.047f,.062f,leather,mat::Wood);
                club.limb({.07f,.41f,-.2f},{.14f,.64f,-.32f},.112f,.13f,iron,mat::Steel);
                club.limb({.105f,.529f,-.264f},{.123f,.568f,-.286f},.129f,.133f,bronze,mat::Brass);
            }else sword(torso.offset(grip.right),attack);
        }
    }
}

void skull(const Fig& f,float y,vec3 bone){
    Fig h=f.offset({0,y,0});
    h.form({{-.19f,.094f,.116f,-.043f},{-.12f,.15f,.16f,-.015f},{.015f,.195f,.188f},
        {.143f,.177f,.169f,.017f},{.193f,.104f,.118f,.024f}},bone,mat::Plaster);
    for(float s:{-1.f,1.f}){
        h.jewel({s*.088f,.005f,-.151f},{.064f,.057f,.035f},ink,mat::Cloth);
        h.jewel({s*.084f,.009f,-.185f},{.022f,.019f,.009f},ember,mat::Glow);
    }
    h.face({-.027f,-.075f,-.181f},{0,-.015f,-.195f},{.028f,-.077f,-.180f},ink,mat::Cloth);
}
void skeleton(const Fig& f,Kind kind,float phase,float stride,float attack,float fold){
    bool archer=kind==Kind::SkeletonArcher,knight=kind==Kind::KnightGuard||kind==Kind::FallenKnight;
    const float weight=.014f*std::sin(phase)+std::abs(stride)*.12f-.36f*fold;
    legs(f,ivory,stride,weight,true,fold);
    Fig upper=f.offset({0,weight,0});
    upper.limb({0,.79f,.014f},{0,1.54f,.04f},.071f,.061f,ivory,mat::Plaster);
    upper.form({{.76f,.17f,.098f},{.84f,.205f,.12f},{.91f,.152f,.093f}},ivory*.83f,mat::Plaster);
    upper.limb({-.255f,1.37f,0},{.24f,1.37f,0},.054f,.055f,ivory,mat::Plaster);
    for(int rib=0;rib<3;++rib){
        float y=1.06f+rib*.098f,w=.14f+rib*.033f;
        for(float s:{-1.f,1.f}){
            vec3 edge{s*w,y+.038f,-.014f};
            upper.limb({s*.03f,y+.055f,.07f},edge,.036f,.035f,ivory,mat::Plaster);
            upper.limb(edge,{s*.036f,y,-.127f},.037f,.017f,ivory,mat::Plaster);
        }
    }
    Hands grip=arms(upper,ivory,ivory,stride,attack,true);
    skull(upper,1.65f,ivory);
    if(archer){
        bow(upper,grip.left,attack*.14f);
        scarf(upper,jade*.83f,phase);
    }else{
        sword(upper.offset(grip.right),attack);
        shield(upper.offset(grip.left),knight);
    }
    if(knight){
        upper.form({{1.03f,.175f,.14f},{1.25f,.266f,.193f},{1.37f,.28f,.177f},{1.44f,.13f,.118f}},iron,mat::Steel);
        upper.form({{1.72f,.211f,.21f},{1.79f,.203f,.201f},{1.91f,.13f,.134f},{1.95f,.044f,.082f}},iron,mat::Steel);
        for(float s:{-1.f,1.f}){
            upper.face({s*.195f,1.76f,-.089f},{s*.07f,1.755f,-.197f},{s*.133f,1.58f,-.162f},iron*.88f,mat::Steel);
            upper.jewel({s*.273f,1.366f,0},{.158f,.14f,.185f},bronze*.8f,mat::Brass);
        }
    }
}
void wolf(const Fig& f,float phase,float motion,float attack){
    const vec3 pelt{.38f,.40f,.42f},undercoat{.65f,.58f,.48f};
    float breath=.018f*std::sin(phase*1.2f),lift=motion*.036f*std::abs(std::sin(phase*2));
    Fig body=f.offset({0,breath+lift,-.075f*attack});
    body.form({{.42f,.125f,.36f},{.56f,.252f,.55f,.032f},{.75f,.269f,.575f,.04f},
        {.89f,.207f,.476f,.012f},{.95f,.095f,.26f,-.04f}},pelt,mat::Cloth);
    body.form({{.56f,.117f,.14f,-.32f},{.75f,.27f,.245f,-.315f},
        {.99f,.25f,.233f,-.37f},{1.115f,.141f,.14f,-.405f}},undercoat,mat::Cloth);
    Fig head=body.offset({0,.98f-attack*.037f,-.53f-attack*.07f});
    head.form({{-.165f,.11f,.149f,-.063f},{-.07f,.199f,.224f,-.03f},{.074f,.191f,.213f,.016f},
        {.15f,.122f,.14f,.023f}},pelt,mat::Cloth);
    head.form({{-.131f,.068f,.145f,-.211f},{-.052f,.112f,.188f,-.206f},{.02f,.092f,.104f,-.152f}},undercoat,mat::Cloth);
    head.jewel({0,-.057f,-.37f},{.068f,.044f,.045f},ink,mat::Cloth);
    for(float s:{-1.f,1.f}){
        head.offset({s*.132f,.081f,.035f}).form({{0,.077f,.084f},{.14f,.048f,.056f,.02f,s*.018f},
            {.263f,.009f,.012f,.04f,s*.045f}},pelt*.85f,mat::Cloth);
        head.face({s*.10f,.13f,-.034f},{s*.17f,.15f,-.03f},{s*.173f,.285f,.035f},leather,mat::Cloth);
        head.jewel({s*.135f,.037f,-.158f},{.032f,.026f,.029f},ember,mat::Glow);
        body.face({s*.177f,.97f,-.57f},{s*.255f,1.f,-.325f},{s*.292f,.695f,-.236f},pelt,mat::Cloth);
        for(float row:{-1.f,1.f}){
            float stride=.19f*motion*std::sin(phase)*s*row;
            float footLift=motion*.075f*std::max(0.f,std::cos(phase)*s*row);
            float z=row*.35f;
            vec3 hip{s*.162f,.67f+lift,z},knee{s*.19f,.39f+footLift*.6f,z+row*.09f+stride*.4f};
            vec3 hock{s*.177f,.18f+footLift,z+.09f+stride*.76f},paw{s*.18f,.06f+footLift,z+stride};
            f.limb(hip,knee,row>0?.132f:.094f,.067f,pelt,mat::Cloth);
            f.limb(knee,hock,.071f,.047f,pelt*.86f,mat::Cloth);
            f.limb(hock,paw,.047f,.052f,undercoat,mat::Cloth);
            f.offset(paw).form({{-.051f,.071f,.093f,-.035f},{.017f,.072f,.101f,-.037f},{.05f,.043f,.055f}},ink*1.6f,mat::Cloth);
        }
    }
    float sway=.065f*std::sin(phase*.7f);
    vec3 mid{sway,.565f,.76f},end{sway*1.7f,.41f,.99f};
    body.limb({0,.77f,.46f},mid,.098f,.131f,pelt,mat::Cloth);
    body.limb(mid,end,.138f,.063f,pelt*.85f,mat::Cloth);
    body.limb(end,{sway*1.95f,.40f,1.10f},.063f,.005f,undercoat,mat::Cloth);
}
void spider(const Fig& f,Kind kind,float phase,float motion,float attack,float fall){
    bool armored=kind==Kind::SpiderGuard||kind==Kind::Matriarch;
    vec3 carapace=armored?vec3(.28f,.17f,.23f):vec3(.21f,.27f,.255f);
    vec3 sigil=kind==Kind::VenomSpider?jade*1.4f:armored?oxblood*1.4f:bronze;
    float lift=.018f*std::sin(phase*.61f)-.23f*ease(.05f,.55f,fall);
    Fig body=f.offset({0,lift,-attack*.08f});
    body.form({{.28f,.14f,.21f,.24f},{.40f,.306f,.36f,.239f},{.61f,.345f,.395f,.282f},
        {.773f,.224f,.274f,.32f},{.81f,.107f,.146f,.34f}},carapace,mat::Steel);
    body.form({{.267f,.115f,.15f,-.23f},{.38f,.258f,.23f,-.195f},{.52f,.22f,.22f,-.205f},
        {.588f,.09f,.116f,-.19f}},carapace*.82f,mat::Steel);
    for(float s:{-1.f,1.f}){
        body.panel({0,.818f,.21f},{s*.153f,.799f,.31f},{s*.215f,.752f,.476f},{s*.112f,.805f,.362f},sigil,mat::Brass);
        body.jewel({s*.103f,.458f,-.381f},{.04f,.037f,.042f},sigil*.73f,mat::Glow);
        body.limb({s*.119f,.395f,-.34f},{s*.142f,.27f,-.47f-attack*.10f},.052f,.01f,ivory*.78f,mat::Plaster);
        for(int i=0;i<4;++i){
            float z=-.23f+.14f*i;
            float swing=phase+pi*float((i+(s>0?1:0))%2),reach=.71f+.09f*std::sin(pi*(i+.5f)/4);
            vec3 hip{s*.188f,.445f+lift,z},knee{s*reach*.70f,.62f+lift+.028f*std::sin(phase*.6f+i),z*1.6f};
            vec3 toe{s*reach,.018f+.081f*motion*std::max(0.f,std::sin(swing)),z*2.1f+.10f*motion*std::cos(swing)};
            // The distal joints curl under the abdomen after the thorax loses its support.
            float curl=ease(.12f+i*.035f,.77f+i*.035f,fall);
            knee=glm::mix(knee,vec3(s*.43f,.19f,z*1.33f),curl);
            toe=glm::mix(toe,vec3(s*.17f,.091f,z*.89f+.08f),curl);
            f.limb(hip,knee,armored?.066f:.052f,.037f,carapace,mat::Steel);
            f.limb(knee,toe,.041f,.008f,leather,mat::Cloth);
            f.limb(knee,glm::mix(knee,toe,.20f),.042f,.036f,bronze*.72f,mat::Brass);
        }
    }
    if(armored){
        body.form({{.686f,.22f,.227f,.28f},{.755f,.247f,.235f,.295f},
            {.82f,.17f,.20f,.30f},{.85f,.06f,.12f,.31f}},bronze*.65f,mat::Brass);
        if(kind==Kind::Matriarch){
            for(float s:{-1.f,1.f})body.limb({s*.14f,.79f,.37f},{s*.236f,.982f,.54f},.061f,.006f,ivory,mat::Plaster);
            body.jewel({0,.853f,.307f},{.057f,.055f,.105f},oxblood*1.2f,mat::Glow);
        }
    }else{
        body.jewel({0,.807f,.348f},{.078f,.022f,.143f},sigil,mat::Brass);
    }
}
void rat(const Fig& f,float phase,float motion,float attack){
    vec3 fur{.38f,.28f,.26f},muzzle{.65f,.49f,.405f};
    Fig body=f.offset({0,.012f*std::sin(phase),-attack*.09f});
    body.form({{.145f,.17f,.286f},{.26f,.283f,.447f,.05f},{.414f,.29f,.420f,.079f},
        {.55f,.213f,.313f,.092f},{.60f,.109f,.14f,.081f}},fur,mat::Cloth);
    body.form({{.209f,.096f,.154f,-.438f},{.309f,.178f,.25f,-.415f},{.44f,.195f,.215f,-.333f},
        {.494f,.124f,.109f,-.282f}},fur*1.13f,mat::Cloth);
    body.jewel({0,.30f,-.642f},{.075f,.06f,.084f},muzzle,mat::Plaster);
    for(float s:{-1.f,1.f}){
        body.jewel({s*.142f,.48f,-.315f},{.094f,.119f,.045f},muzzle,mat::Cloth);
        body.jewel({s*.142f,.49f,-.347f},{.06f,.076f,.02f},oxblood*.85f,mat::Cloth);
        body.jewel({s*.134f,.379f,-.48f},{.036f,.028f,.028f},ember*.85f,mat::Glow);
        body.limb({s*.043f,.285f,-.605f},{s*.044f,.216f,-.624f},.025f,.014f,ivory,mat::Plaster);
        for(float row:{-1.f,1.f}){
            float stride=motion*.072f*std::sin(phase)*s*row;
            vec3 knee{s*.20f,.185f,row*.25f},toe{s*.235f,.046f,row*.28f+stride};
            body.limb(knee,toe,.073f,.044f,fur,mat::Cloth);
            body.jewel(toe,{.063f,.032f,.082f},muzzle,mat::Plaster);
        }
    }
    vec3 mid{.11f,.126f,.687f},end{.20f+.072f*std::sin(phase*.72f),.086f,.858f};
    body.limb({0,.273f,.418f},mid,.061f,.039f,muzzle,mat::Cloth);
    body.limb(mid,end,.039f,.018f,muzzle,mat::Cloth);
    body.limb(end,end+vec3(-.055f,.039f,.133f),.019f,.004f,muzzle,mat::Cloth);
}

void slime(const Fig& f,float phase,float motion,float attack){
    float squash=.07f*std::sin(phase*(.8f+motion*.3f));
    Fig body=f.offset({0,0,-attack*.12f});
    body.form({{.018f,.42f-squash,.39f-squash},{.115f,.485f-squash,.46f-squash},
        {.35f+squash,.427f,.39f,.027f},{.51f+squash,.284f,.266f,.045f},
        {.59f+squash,.13f,.15f,.064f}},{.20f,.44f,.365f},mat::Moss);
    body.form({{.54f+squash,.126f,.099f,.06f},{.67f+squash,.073f,.073f,.075f},
        {.735f+squash,.012f,.015f,.091f,-.02f}},jade*1.25f,mat::Moss);
    for(float s:{-1.f,1.f}){
        body.jewel({s*.14f,.30f+squash,-.365f},{.055f,.035f,.033f},ink,mat::Cloth);
        body.jewel({s*.14f,.307f+squash,-.392f},{.023f,.018f,.012f},ivory*.8f,mat::Glow);
    }
    body.jewel({-.21f,.434f+squash,-.203f},{.08f,.029f,.034f},{.44f,.62f,.48f},mat::Moss);
    body.offset({-.30f,.01f,.19f}).form({{.01f,.142f,.123f},{.09f,.165f,.133f},{.18f,.096f,.105f}},jade,mat::Moss);
    body.offset({.34f,.01f,-.115f}).form({{.015f,.14f,.116f},{.081f,.152f,.133f},{.16f,.08f,.071f}},jade,mat::Moss);
}
void ghost(const Fig& f,float phase){
    float drift=.095f*std::sin(phase*.78f);
    Fig g=f.offset({0,drift,.027f*std::sin(phase)});
    const vec3 veil{.22f,.39f,.415f};
    hood(g,veil*.92f);
    g.form({{.60f,.23f,.19f,.056f},{1.01f,.22f,.19f},{1.37f,.33f,.21f},
        {1.51f,.165f,.159f}},veil,mat::Cloth);
    for(int i=0;i<8;++i){
        auto u=octagon[i],v=octagon[(i+1)%8];
        float flutter=.07f*std::sin(phase*.9f+i);
        vec3 a{u.x*.25f,.81f,u.y*.206f+.045f},b{v.x*.25f,.81f,v.y*.206f+.045f};
        g.face(a,b,{(u.x+v.x)*.21f+flutter,.17f+.21f*(i%2)+flutter,(u.y+v.y)*.177f+.11f},i%2?veil*.92f:jade,mat::Banner);
    }
    for(float s:{-1.f,1.f}){
        g.jewel({s*.069f,1.66f,-.23f},{.038f,.02f,.014f},{.36f,.65f,.60f},mat::Glow);
        vec3 elbow{s*.39f,1.19f,.02f},wrist{s*.446f,1.155f,-.193f+.058f*std::sin(phase+s)};
        g.limb({s*.23f,1.39f,0},elbow,.092f,.06f,veil,mat::Cloth);
        g.limb(elbow,wrist,.073f,.042f,jade,mat::Cloth);g.jewel(wrist,{.057f,.074f,.058f},ivory*.68f,mat::Plaster);
    }
}
void construct(const Fig& f,bool warden,const combat::Actor& a,float phase,float stride,float attack){
    vec3 armor=warden?iron:vec3(.40f,.39f,.36f),edge=warden?bronze:vec3(.57f,.49f,.36f);
    float material=warden?mat::Steel:mat::Stone;
    for(float s:{-1.f,1.f}){
        vec3 hip{s*.235f,.87f,0},knee{s*.248f,.5f,-.042f+stride*s*.52f},heel{s*.25f,.14f,.04f+stride*s};
        f.limb(hip,knee,.17f,.136f,armor,material);
        f.limb(knee,heel,.158f,.116f,armor*.80f,material);
        f.offset(heel).form({{-.125f,.17f,.223f,-.065f},{-.042f,.196f,.23f,-.065f},{.093f,.133f,.137f}},armor,material);
    }
    Fig body=f.offset({0,.019f*std::sin(phase)+std::abs(stride)*.12f,-.066f*attack});
    body.form({{.737f,.218f,.19f},{.87f,.326f,.24f},{1.1f,.286f,.231f},{1.4f,.451f,.325f},
        {1.55f,.397f,.272f},{1.64f,.215f,.163f}},armor,material);
    body.form({{1.54f,.192f,.154f},{1.70f,.255f,.201f,-.029f},{1.91f,.228f,.185f},
        {1.99f,.128f,.117f,.024f}},armor*.84f,material);
    vec3 signal=a.vulnerable>0?ivory:ember;
    body.jewel({0,1.78f,-.198f},{.148f,.036f,.027f},signal,mat::Glow);
    body.jewel({0,1.287f,-.269f},{.192f,.215f,.085f},edge,warden?mat::Brass:mat::Stone);
    body.jewel({0,1.281f,-.334f},{.106f,.127f,.040f},signal,mat::Glow);
    for(float s:{-1.f,1.f}){
        body.jewel({s*.425f,1.44f,.015f},{.245f,.219f,.245f},edge,warden?mat::Brass:mat::Stone);
        vec3 elbow{s*.55f,1.17f,.056f+stride*s*.34f},hand{s*.58f,.827f,-.14f+stride*s*.55f-(s>0?attack*.32f:0)};
        body.limb({s*.44f,1.422f,0},elbow,.172f,.125f,armor,material);
        body.limb(elbow,hand,.151f,.182f,armor*.86f,material);
        body.jewel(hand,{.189f,.157f,.18f},edge,warden?mat::Brass:mat::Stone);
    }
    if(warden&&a.alive()&&a.stance==1){
        ring(f,.095f,.89f,.037f,jade,mat::Glow);
        ring(f,1.56f,.82f,.021f,jade*.88f,mat::Glow);
    }
}
void keeper(const Fig& f,const combat::Actor& a,float phase){
    Fig body=f.offset({0,.055f*std::sin(phase),0});
    body.form({{.095f,.427f,.34f},{.30f,.468f,.36f,.03f},{.99f,.246f,.202f},
        {1.46f,.367f,.235f},{1.63f,.175f,.153f}},oxblood*.63f,mat::Banner);
    body.form({{1.36f,.348f,.221f},{1.44f,.414f,.255f},{1.60f,.23f,.176f}},bronze*.73f,mat::Brass);
    skull(body,1.78f,ivory);
    body.jewel({0,1.34f,-.226f},{.092f,.126f,.039f},ember,mat::Glow);
    for(float s:{-1.f,1.f}){
        body.limb({s*.308f,1.45f,.018f},{s*.481f,1.24f,.05f},.09f,.061f,oxblood,mat::Cloth);
        body.limb({s*.481f,1.24f,.05f},{s*.61f,1.19f,-.27f},.063f,.043f,ivory,mat::Plaster);
        vec3 root{s*.145f,1.89f,.055f},fork{s*.33f,2.09f,.094f},tip{s*.39f,2.38f,.095f};
        body.limb(root,fork,.067f,.045f,ivory*.81f,mat::Plaster);
        body.limb(fork,tip,.045f,.007f,ivory,mat::Plaster);
        body.limb(glm::mix(fork,tip,.42f),{s*.51f,2.32f,.13f},.034f,.004f,ivory,mat::Plaster);
        body.limb(fork,{s*.26f,2.26f,.067f},.029f,.004f,ivory,mat::Plaster);
    }
    if(a.alive()&&a.vulnerable<=0&&!a.phaseTwo){
        ring(f,.105f,.85f,.036f,jade,mat::Glow);
        for(int i=0;i<4;++i){
            float angle=phase*.31f+i*pi*.5f;
            body.jewel({std::cos(angle)*.80f,.90f+.16f*std::sin(phase+i),std::sin(angle)*.80f},
                {.063f,.122f,.061f},jade*1.40f,mat::Glow);
        }
    }
}
void pylon(const Fig& f,float phase){
    f.form({{.013f,.49f,.49f},{.114f,.52f,.52f},{.262f,.387f,.387f},
        {.359f,.33f,.33f}},iron,mat::Stone);
    for(int i=0;i<4;++i){
        float a=i*pi*.5f;
        vec3 side{std::cos(a),0,std::sin(a)},knee=side*.32f+vec3(0,1.36f,0);
        f.limb(side*.25f+vec3(0,.27f,0),knee,.087f,.061f,ivory,mat::Plaster);
        f.limb(knee,side*.17f+vec3(0,1.76f,0),.062f,.01f,bronze,mat::Brass);
    }
    f.jewel({0,1.66f+.064f*std::sin(phase),0},{.175f,.319f,.175f},oxblood*1.58f,mat::Glow);
}

// Work in model space so the fall follows the last facing even on diagonal tiles.
// The geometry pass is limited to figures that are recoiling or falling.
void finishEnemyPose(Mesh& m,size_t first,const combat::Actor& actor,vec3 position,float yaw,
                     float fall,float age,float recoil,float side){
    const bool dead=age>=0;
    const glm::mat3 facing=glm::mat3(glm::rotate(glm::mat4(1),yaw,vec3(0,1,0)));
    const glm::mat3 toLocal=glm::transpose(facing);
    float pitch=0,roll=0,lift=0,settle=0;
    vec3 stretch{1};
    if(dead){
        float impact=ease(.05f,.20f,fall)*(1-ease(.2f,.40f,fall));
        float tip=ease(.17f,.80f,fall);
        float bounce=ease(.80f,.87f,fall)*(1-ease(.87f,1.08f,fall));
        settle=tip;
        switch(actor.kind){
        case Kind::Rat:case Kind::Wolf:
            roll=side*(1.34f*tip-.14f*bounce);
            pitch=.12f*impact-.13f*tip;
            break;
        case Kind::Spiderling:case Kind::VenomSpider:case Kind::SpiderGuard:case Kind::Matriarch:
            roll=side*(.13f*impact+.20f*tip-.05f*bounce);
            pitch=-.13f*tip;
            break;
        case Kind::Slime:{
            float burst=ease(.06f,.41f,fall);
            stretch={1+.68f*burst,1+.19f*impact-.86f*burst+.10f*bounce,1+.46f*burst};
            break;}
        case Kind::Ghost:{
            float wisp=ease(.08f,1.47f,fall);
            stretch={1-.81f*wisp,1+.22f*wisp,1-.77f*wisp};
            yaw+=side*.88f*wisp;lift=.8f*wisp;settle=0;
            break;}
        case Kind::Pylon:{
            float crumble=ease(.18f,.8f,fall);
            stretch={1+.14f*crumble,1-.82f*crumble,1+.12f*crumble};
            pitch=.12f*impact;roll=.19f*side*tip;break;}
        case Kind::Golem:case Kind::Warden:
            pitch=-1.44f*tip+.07f*bounce;
            roll=side*(.07f*impact+.18f*tip);break;
        default:
            pitch=.16f*impact-1.43f*tip+.12f*bounce;
            roll=side*(.08f*impact+.32f*tip);break;
        }
    }
    glm::mat4 rotation=glm::rotate(glm::mat4(1),yaw,vec3(0,1,0));
    rotation=glm::rotate(rotation,roll,vec3(0,0,1));
    rotation=glm::rotate(rotation,pitch,vec3(1,0,0));
    glm::mat3 basis(rotation);
    float shear=recoil*(actor.boss?.08f:.18f);
    float ground=0;
    for(size_t i=first;i<m.vertices.size();++i){
        Vertex& v=m.vertices[i];
        v.position=toLocal*(v.position-position);
        v.normal=toLocal*v.normal;
        v.position.z+=v.position.y*shear;
        v.normal.y-=v.normal.z*shear;
        v.position=basis*(v.position*stretch);
        v.normal=glm::normalize(basis*(v.normal/stretch));
        ground=std::min(ground,v.position.y);
    }
    position.y+=lift-ground*settle;
    if(dead){
        // Late in the hold, painted facets separate into small motes and disappear.
        // Each triangle contracts continuously and casts a matching shadow.
        float ash=actor.kind==Kind::Ghost?ease(.78f,2.28f,age):ease(1.82f,enemyRemainsSeconds,age);
        if(ash>0.f){
            size_t output=first;
            for(size_t i=first;i<m.vertices.size();i+=3){
                std::uint32_t hash=std::uint32_t((i-first)/3+1)*0x9e3779b9u;
                hash^=hash>>16;hash*=0x85ebca6bu;hash^=hash>>13;
                float seed=float(hash&65535u)/65535.f;
                float vanish=ease(seed*.65f,seed*.65f+.35f,ash);
                if(vanish>=1)continue;
                vec3 center=(m.vertices[i].position+m.vertices[i+1].position+m.vertices[i+2].position)/3.f;
                vec3 drift{(seed-.5f)*.24f,.16f+.20f*seed,.045f};
                for(size_t k=0;k<3;++k){
                    Vertex v=m.vertices[i+k];
                    v.position=position+glm::mix(v.position,center+drift*vanish,vanish);
                    v.color*=1-.38f*vanish;
                    m.vertices[output++]=v;
                }
            }
            m.vertices.resize(output);
            return;
        }
    }
    for(size_t i=first;i<m.vertices.size();++i)m.vertices[i].position+=position;
}
} // namespace

void buildEnemy(Mesh& mesh,const combat::Actor& a,vec3 p,float yaw,float phase,float motion,float deathSeconds,float hitSeconds){
    if(deathSeconds>=enemyRemainsSeconds||(a.kind==Kind::Ghost&&deathSeconds>=2.28f))return;
    const size_t first=mesh.vertices.size();
    const bool dead=deathSeconds>=0;
    const float fall=deathSeconds/(a.boss?1.24f:.89f);
    const float fold=ease(.01f,.50f,fall);
    const float recoil=dead?0:ease(0,.043f,hitSeconds)*(1-ease(.043f,.18f,hitSeconds));
    const bool posed=dead||recoil>0;
    Fig f(mesh,p,yaw+pi);
    motion*=1-ease(0,.46f,fall);
    float stride=.21f*std::sin(phase)*motion;
    float duration=a.boss?.30f:.25f;
    float progress=std::clamp(1.f-a.swing/duration,0.f,1.f);
    float attack=a.swing>0.f?std::sin(pi*progress)*std::sin(pi*progress)*(1-fold):0.f;
    switch(a.kind){
    case Kind::Wolf:wolf(f,phase,motion,attack);break;
    case Kind::Rat:rat(f,phase,motion,attack);break;
    case Kind::Slime:slime(f,phase,motion,attack);break;
    case Kind::Spiderling:case Kind::VenomSpider:case Kind::SpiderGuard:case Kind::Matriarch:
        f.scale=a.kind==Kind::Matriarch?1.83f:a.kind==Kind::SpiderGuard?1.15f:a.kind==Kind::Spiderling?.68f:.89f;
        spider(f,a.kind,phase,motion,attack,fall);break;
    case Kind::Ogre:case Kind::Bandit:case Kind::BanditArcher:case Kind::Miner:
        if(a.kind==Kind::Ogre)f.scale=1.42f;
        humanoid(f,a.kind,phase,stride,attack,fold);break;
    case Kind::Ghost:ghost(f,phase);break;
    case Kind::Golem:case Kind::Warden:
        if(a.kind==Kind::Warden)f.scale=1.51f;
        construct(f,a.kind==Kind::Warden,a,phase,stride,attack);break;
    case Kind::BoneKeeper:f.scale=1.79f;keeper(f,a,phase);break;
    case Kind::Pylon:pylon(f,phase);break;
    case Kind::FallenKnight:{
        f.scale=1.32f;skeleton(f,a.kind,phase,stride,attack,fold);
        Fig drape=f.offset({0,.014f*std::sin(phase)+std::abs(stride)*.12f-.36f*fold,0});
        vec3 cloth=a.enraged?oxblood*1.5f:oxblood*.72f;
        float flutter=.042f*std::sin(phase*.75f);
        drape.panel({-.23f,1.42f,.13f},{0,1.48f,.19f},{.08f,.27f,.34f+flutter},{-.32f,.40f,.29f+flutter},cloth,mat::Banner);
        drape.face({0,1.48f,.19f},{.24f,1.42f,.13f},{.08f,.27f,.34f+flutter},cloth*.85f,mat::Banner);
        for(float s:{-1.f,1.f})drape.limb({s*.16f,1.82f,.012f},{s*.20f,2.034f,.062f},.047f,.009f,bronze,mat::Brass);
        if(!dead&&a.stance==1)ring(f,.079f,.58f,.025f,bronze,mat::Glow);
        break;}
    case Kind::Skeleton:case Kind::SkeletonArcher:case Kind::KnightGuard:
    default:skeleton(f,a.kind,phase,stride,attack,fold);break;
    }
    if(posed){
        float side=(a.spawn>=0?a.spawn:int(std::abs(p.x)+std::abs(p.z)))%2?1.f:-1.f;
        finishEnemyPose(mesh,first,a,p,yaw+pi,fall,deathSeconds,recoil,side);
    }
    if(!dead&&a.vulnerable>0)ring(Fig(mesh,p,0),.055f,.87f,.042f,bronze*1.28f,mat::Glow);
}

void buildNpc(Mesh& mesh,const std::string& id,vec3 p,float yaw,float time){
    Fig f(mesh,p,yaw+pi);
    bool elder=id=="elder",smith=id=="smith",merchant=id=="merchant",herbalist=id=="herbalist";
    vec3 cloth=elder?vec3(.34f,.23f,.35f):smith?iron:merchant?oxblood:herbalist?jade:leather;
    float phase=time*1.7f+p.x*.53f+p.z*.28f;
    float breath=.016f*std::sin(phase);
    Fig body=f.offset({0,breath,0});
    legs(f,iron*.87f,0,breath);
    if(elder||merchant||herbalist){
        body.form({{.13f,.302f,.236f,.022f},{.23f,.339f,.25f,.014f},{.77f,.237f,.175f},
            {1.f,.194f,.158f},{1.28f,.267f,.19f},{1.42f,.247f,.169f},{1.50f,.11f,.104f}},cloth,mat::Cloth);
        for(float s:{-1.f,1.f})
            body.face({s*.06f,.98f,-.166f},{s*.123f,.161f,-.207f},{s*.045f,.16f,-.232f},cloth*.86f,mat::Banner);
    }else{
        body.form({{.71f,.267f,.193f},{.96f,.21f,.17f},{1.27f,.32f,.211f},
            {1.41f,.275f,.181f},{1.48f,.119f,.106f}},cloth,mat::Cloth);
    }
    body.form({{.91f,.226f,.175f},{.994f,.211f,.17f}},leather,mat::Cloth);
    head(body,skin);
    Hands grip=arms(body,cloth,skin,.02f*std::sin(phase*.8f),0,false);
    if(elder){
        scarf(body,bronze*.62f,phase);
        body.form({{1.71f,.181f,.17f,.025f},{1.8f,.19f,.172f,.02f},{1.88f,.103f,.099f,.025f}},ivory*.81f,mat::Cloth);
        body.offset({0,1.54f,-.125f}).form({{-.19f,.035f,.034f,-.03f},{-.06f,.086f,.071f,-.025f},{.056f,.128f,.087f}},ivory,mat::Cloth);
        vec3 staff=grip.right;
        f.limb({staff.x,.03f,staff.z-.06f},staff+vec3(0,.76f,0),.035f,.03f,leather,mat::Wood);
        f.jewel(staff+vec3(0,.81f,0),{.065f,.10f,.06f},bronze,mat::Brass);
    }else if(smith){
        body.panel({-.19f,1.31f,-.172f},{.16f,1.31f,-.184f},{.193f,.51f,-.219f},{-.195f,.51f,-.22f},leather,mat::Cloth);
        body.form({{1.72f,.184f,.176f},{1.81f,.176f,.161f},{1.865f,.091f,.088f}},ink,mat::Cloth);
        body.jewel({0,1.553f,-.14f},{.125f,.106f,.069f},ink,mat::Cloth);
        Fig tool=body.offset(grip.right);
        tool.limb({0,-.09f,.058f},{0,.227f,-.224f},.029f,.033f,leather,mat::Wood);
        tool.limb({-.139f,.223f,-.224f},{.151f,.223f,-.224f},.087f,.075f,silver*.69f,mat::Steel);
        Fig anvil(mesh,p,pi);
        Fig stand=anvil.offset({.77f,0,-.39f});
        stand.form({{0,.21f,.19f},{.33f,.18f,.17f}},leather,mat::Wood);
        stand.form({{.306f,.244f,.181f},{.38f,.177f,.126f},{.45f,.194f,.132f},{.52f,.32f,.142f}},iron,mat::Steel);
        stand.limb({.219f,.49f,0},{.43f,.49f,0},.068f,.014f,silver,mat::Steel);
    }else if(merchant||herbalist){
        body.form({{1.72f,.195f,.18f,.019f},{1.82f,.183f,.171f,.026f},{1.88f,.092f,.11f,.025f}},merchant?leather:ink,mat::Cloth);
        body.form({{1.755f,.191f,.182f,.025f},{1.80f,.191f,.18f,.025f}},merchant?bronze:jade,mat::Banner);
        if(herbalist){
            for(float s:{-1.f,1.f}){
                body.limb({s*.147f,1.771f,.077f},{s*.16f,1.51f,.088f},.061f,.039f,leather,mat::Cloth);
                body.jewel({s*.16f,1.53f,.088f},{.041f,.032f,.043f},bronze,mat::Brass);
            }
        }else body.jewel({0,1.804f,.158f},{.112f,.101f,.095f},leather,mat::Cloth);
        scarf(body,merchant?bronze*.72f:jade*1.16f,phase);
        if(herbalist){
            Fig basket=body.offset(grip.left+vec3(0,-.10f,0));
            basket.form({{-.15f,.09f,.078f},{-.08f,.141f,.115f},{.044f,.149f,.121f}},bronze*.65f,mat::Wood);
            for(int i=0;i<3;++i){
                vec3 root{-.057f+i*.061f,.02f,0},leaf=root+vec3((i-1)*.046f,.18f-(i%2)*.059f,.027f);
                basket.limb(root,leaf,.014f,.007f,jade,mat::Foliage);
                basket.jewel(leaf,{.04f,.058f,.025f},i==1?ivory:jade*1.35f,mat::Foliage);
            }
        }else{
            // The stall stays planted while the merchant turns to greet the hero.
            Fig stall(mesh,p,pi);
            stall.offset({0,0,-.86f}).form({{.06f,.63f,.25f},{.16f,.66f,.27f},{.76f,.61f,.255f},
                {.82f,.73f,.31f}},leather,mat::Wood);
            for(float s:{-1.f,1.f})stall.limb({s*.70f,.04f,-1.08f},{s*.70f,2.14f,-1.08f},.042f,.039f,leather,mat::Wood);
            for(int i=0;i<4;++i){
                float x=-.76f+i*.38f;
                vec3 fabric=(i%2?oxblood:oxblood*1.17f);
                stall.panel({x,2.18f,-1.16f},{x+.38f,2.18f,-1.16f},{x+.38f,2.04f,-.68f},{x,2.04f,-.68f},fabric,mat::Banner);
                stall.face({x,2.04f,-.68f},{x+.38f,2.04f,-.68f},{x+.19f,1.94f,-.68f},fabric*.80f,mat::Banner);
            }
            for(int i=0;i<3;++i){
                Fig pot=stall.offset({-.36f+.31f*i,.82f,-.91f});
                pot.form({{0,.065f,.067f},{.109f,.101f,.094f},{.166f,.067f,.07f},
                    {.20f,.046f,.05f}},i%2?jade:bronze,mat::Plaster);
            }
        }
    }else{
        body.form({{1.73f,.21f,.195f},{1.77f,.221f,.20f},{1.82f,.169f,.16f},{1.89f,.093f,.085f}},bronze*.79f,mat::Brass);
    }
    ring(f,.026f,.68f,.027f,bronze*.84f,mat::Glow);
}
