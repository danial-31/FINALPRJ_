#include "combat_visuals.hpp"

namespace {
using glm::vec3;
const vec3 brass{.61f,.39f,.16f}, ice{.23f,.65f,.75f}, silver{.55f,.65f,.68f};
const vec3 ember{.91f,.31f,.07f}, jade{.16f,.48f,.38f}, lilac{.42f,.32f,.62f};
vec3 at(glm::vec2 p,float y=0){return {p.x,y,p.y};}
vec3 radial(float angle){return {std::sin(angle),0,std::cos(angle)};}

// A short ink stroke with a broad middle and two needle tips.
void shard(Mesh& mesh,vec3 tail,vec3 tip,float width,vec3 color,float material){
    vec3 d=tip-tail;
    if(glm::dot(d,d)<1.e-7f||width<=.001f)return;
    d=glm::normalize(d);
    vec3 axis=std::abs(d.y)<.9f?vec3(0,1,0):vec3(1,0,0);
    vec3 u=glm::normalize(glm::cross(d,axis))*width,v=glm::cross(d,u)*.65f;
    vec3 middle=glm::mix(tail,tip,.62f);
    vec3 corners[]={middle+u,middle+v,middle-u,middle-v};
    for(int i=0;i<4;++i){
        vec3 a=corners[i],b=corners[(i+1)%4];
        mesh.triangle(tail,b,a,color*.78f,material);
        mesh.triangle(tip,a,b,color,material);
    }
}

// Width tapers at both ends. The visible segment sweeps around the caster.
void sweep(Mesh& mesh,vec3 center,float heading,float radius,float span,float width,vec3 color){
    constexpr int segments=18;
    for(int i=0;i<segments;++i){
        float a=float(i)/segments,b=float(i+1)/segments;
        auto edge=[&](float u,float offset){
            vec3 dir=radial(heading-span*(1-u));
            return center+dir*(radius+offset*std::sin(pi*u)*width)+vec3(0,.16f*u,0);
        };
        mesh.quad(edge(a,0),edge(b,0),edge(b,1),edge(a,1),color*(.60f+.40f*b),mat::Glow);
    }
}

void missile(Mesh& mesh,std::vector<Particle>& sparks,const combat::Projectile& p,const combat::Game& g,float now){
    if(p.target<0||p.target>=int(g.actors.size()))return;
    vec3 head=at(p.pos,1.18f);
    glm::vec2 towards=g.actors[p.target].pos-p.pos;
    if(glm::dot(towards,towards)<.00001f)return;
    vec3 dir=at(glm::normalize(towards)),side{dir.z,0,-dir.x};
    bool arrow=p.source>=0&&p.source<int(g.actors.size())&&(p.hero?g.actors[p.source].weapon==combat::Weapon::Bow:
        g.actors[p.source].kind==combat::Kind::BanditArcher||g.actors[p.source].kind==combat::Kind::SkeletonArcher);
    if(arrow&&p.effect==combat::Effect::None){
        vec3 tail=head-dir*.63f;
        mesh.beam(tail,head-dir*.11f,.019f,brass,mat::Wood,5);
        shard(mesh,head-dir*.19f,head,.056f,silver,mat::Steel);
        for(float s:{-1.f,1.f})mesh.triangle(tail,tail+dir*.17f,tail+dir*.03f+side*s*.075f,silver,mat::Cloth);
        return;
    }
    bool web=p.effect==combat::Effect::Web;
    vec3 pigment=web?vec3(.69f,.65f,.54f):p.effect==combat::Effect::Burn?ember:p.effect==combat::Effect::Poison?jade:p.hero?ice:lilac;
    shard(mesh,head-dir*.51f,head+dir*.15f,.11f,pigment,mat::Glow);
    for(int i=0;i<7;++i){
        float t=float(i)/7,phase=now*16-t*4;
        vec3 bend=(side*std::cos(phase)+vec3(0,std::sin(phase),0))*(.09f*t);
        sparks.push_back({head-dir*(.12f+t*.76f)+bend,pigment*2.2f,.23f*(1-t)+.05f,1-t,1});
    }
}
}

void CombatVisuals::clear(){strokes.clear();}

void CombatVisuals::observe(const combat::Event& e,const combat::Game& g,float now){
    if(g.actors.empty())return;
    const auto& hero=g.actors.front();
    const vec3 origin=at(e.origin),destination=at(hero.pos);
    const float yaw=hero.facing;
    auto add=[&](Mark kind,vec3 from,vec3 to,vec3 color,float seconds,float reach,float delay=0.f){
        if(strokes.size()>=96)strokes.erase(strokes.begin());
        strokes.push_back({kind,from,to,color,yaw,now+delay,seconds,reach});
    };
    if(e.type==combat::Event::Hit){
        if(e.target<0){
            if(e.text=="meteor")add(Mark::Meteor,origin,origin,ember,.50f,2.4f);
        }else if(e.target<int(g.actors.size())&&e.amount>0){
            const auto& a=g.actors[e.target];
            vec3 pos=at(a.pos,a.kind==combat::Kind::Hero?1.65f:.55f+a.radius*.75f);
            add(Mark::Impact,pos,pos,e.target==0?vec3(.86f,.37f,.15f):brass,.24f,a.boss?1.35f:1.f,
                e.directHeroImpact?combat::heroImpactLeadSeconds:0.f);
        }
        return;
    }
    if(e.type!=combat::Event::Cast||e.amount<0||e.amount>3)return;
    const auto cls=g.heroClass;
    float reach=combat::ability(cls,e.amount).radius;
    if(cls==combat::Class::Warrior&&e.amount==1)add(Mark::Whirl,origin,origin,silver,.43f,1.55f,.14f);
    if(cls==combat::Class::Warrior&&e.amount==3)add(Mark::Quake,origin,origin,brass,.50f,reach,.28f);
    if(cls==combat::Class::Rogue&&e.amount==2)add(Mark::Knives,origin,origin,jade,.34f,reach,.12f);
    if(cls==combat::Class::Mage&&e.amount==1)add(Mark::Frost,origin,origin,ice,.55f,reach,.13f);
    if((cls==combat::Class::Rogue&&e.amount==1)||(cls==combat::Class::Mage&&e.amount==2))
        add(Mark::Blink,origin,destination,cls==combat::Class::Mage?lilac:jade,.40f,1);
}

void CombatVisuals::draw(Mesh& mesh,std::vector<Particle>& sparks,const combat::Game& g,float now){
    for(const auto& p:g.projectiles)missile(mesh,sparks,p,g,now);
    for(const auto& warning:g.telegraphs){
        if(warning.owner!=0||warning.time>.47f||warning.time<=0)continue;
        float t=warning.time/.47f;
        vec3 contact=at(warning.center,.12f),up{-.27f,1.f,.13f};
        vec3 head=contact+up*(7.f*t);
        shard(mesh,head+up*.86f,head,.22f,ember,mat::Ember);
        sparks.push_back({head,ember*2.f,.64f,1,1});
    }
    strokes.erase(std::remove_if(strokes.begin(),strokes.end(),[&](const Stroke& s){return now>=s.start+s.duration;}),strokes.end());
    for(const auto& s:strokes){
        if(now<s.start)continue;
        float t=(now-s.start)/s.duration,fade=1.f-t;
        float ease=1.f-fade*fade,weight=std::sin(pi*t);
        switch(s.kind){
        case Mark::Whirl:
            for(int i=0;i<2;++i)sweep(mesh,s.from+vec3(0,1.05f+.24f*i,0),s.yaw+i*pi+t*2*pi,
                s.reach-.22f*i,weight*2.2f,(.11f-.04f*i)*weight,s.color);
            break;
        case Mark::Blink:
            for(int i=0;i<15;++i){
                float u=(float(i)+.5f)/15;
                float lift=.8f+.55f*std::sin(pi*u)+.22f*std::sin(u*15+t*4);
                vec3 p=glm::mix(s.from,s.to,u)+vec3(0,lift+.4f*t,0);
                sparks.push_back({p,s.color*2.3f,.23f+weight*.13f,fade,1});
            }
            break;
        case Mark::Impact:
        case Mark::Meteor:
            for(int i=0;i<(s.kind==Mark::Meteor?13:6);++i){
                vec3 dir=radial(float(i)*2.39996f+s.yaw);
                vec3 p=s.from+dir*((.12f+.62f*ease)*s.reach)+vec3(0,.04f+weight*(.18f+.08f*(i%4))*s.reach,0);
                sparks.push_back({p,s.color*2.2f,.065f+.09f*fade,fade,1});
                shard(mesh,p-dir*.16f*fade,p,.028f*fade,s.color,mat::Glow);
            }
            break;
        case Mark::Knives:
        case Mark::Quake:
        case Mark::Frost:
            for(int i=0;i<10;++i){
                vec3 dir=radial(s.yaw+float(i)*2*pi/10),side{dir.z,0,-dir.x};
                float radius=(.18f+.82f*ease)*s.reach;
                float height=s.kind==Mark::Knives?1.03f:.08f;
                vec3 p=s.from+dir*radius+vec3(0,height,0);
                if(s.kind==Mark::Frost){
                    vec3 tip=p+dir*(.28f*weight)+vec3(0,(.52f+.11f*(i%3))*weight,0);
                    shard(mesh,p,tip,.11f*weight,s.color,mat::Glow);
                }else if(s.kind==Mark::Knives){
                    shard(mesh,p-dir*.43f*weight,p,.082f*weight,silver,mat::Steel);
                    sparks.push_back({p-dir*.35f,s.color*2.f,.18f,weight,1});
                }else{
                    mesh.triangle(p-dir*.50f*weight,p+side*.05f*weight,p-side*.05f*weight,s.color,mat::Glow);
                    sparks.push_back({p+vec3(0,weight*.27f,0),s.color,.23f,fade,2});
                }
            }
            break;
        }
    }
}
