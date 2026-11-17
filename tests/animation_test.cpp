#include "scene.hpp"
#include "hero_animation.hpp"
#include <cmath>
#include <iostream>

namespace {
bool near(float a,float b,float tolerance=.0001f){return std::abs(a-b)<tolerance;}
bool checkPose(float phase,float blend)
{
    const auto p=heroAnimation::sample(0,phase,blend);
    bool supported=false;
    for(const auto& leg:p.legs) {
        if(!near(glm::length(leg.knee-leg.hip),heroAnimation::thighLength)||
           !near(glm::length(leg.ankle-leg.knee),heroAnimation::shinLength)) {
            std::cerr<<"IK reach or bone-length failure at phase "<<phase<<" blend "<<blend<<'\n';return false;
        }
        float sole=100;
        for(float z:{-.21f,.10f})
            sole=std::min(sole,leg.ankle.y-.22f*std::cos(leg.pitch)-z*std::sin(leg.pitch));
        if(sole<heroAnimation::floorHeight-.0001f||
           !near(sole,heroAnimation::floorHeight+leg.clearance)) {
            std::cerr<<"Foot penetrates floor or misses target clearance\n";return false;
        }
        if(leg.stance) {
            supported=true;
            if(!near(sole,heroAnimation::floorHeight)) {std::cerr<<"Supporting sole is airborne\n";return false;}
        }
    }
    if(!supported){std::cerr<<"Walk contains unsupported flight phase\n";return false;}
    return true;
}
}
int main()
{
    for(int blend=0;blend<=20;++blend)for(int phase=0;phase<720;++phase)
        if(!checkPose(float(phase)*2*pi/720,float(blend)/20))return 1;
    // Position and first derivative are continuous across heel strike/toe-off,
    // including the opposite leg's phase offset and the heel/toe contact switch.
    for(float boundary:{0.f,heroAnimation::stanceFraction*2*pi,pi,
            heroAnimation::stanceFraction*2*pi-pi,.5f*(1+heroAnimation::stanceFraction)*2*pi}) {
        constexpr float dt=.0005f;
        auto a=heroAnimation::sample(0,boundary-dt,1),b=heroAnimation::sample(0,boundary,1),c=heroAnimation::sample(0,boundary+dt,1);
        for(size_t i=0;i<2;++i) {
            if(glm::length(c.legs[i].ankle-a.legs[i].ankle)>.002f||
               glm::length((c.legs[i].ankle-b.legs[i].ankle)/dt-(b.legs[i].ankle-a.legs[i].ankle)/dt)>.04f) {
                std::cerr<<"Foot trajectory discontinuity at boundary "<<boundary<<'\n';return 1;
            }
        }
    }
    // At flat-foot stance, root travel exactly cancels foot travel in world space.
    for(float blend:{.25f,.5f,1.f}) {
        float phase=.30f*heroAnimation::stanceFraction*2*pi,distance=.0001f,scale=1.45f;
        auto a=heroAnimation::sample(0,phase,blend);
        auto b=heroAnimation::sample(0,phase+heroAnimation::gaitAdvance(distance,scale,blend),blend);
        if(std::abs((b.legs[1].ankle.z-a.legs[1].ankle.z)*scale-distance)>.00001f) {
            std::cerr<<"Planted foot slides relative to declared stride\n";return 1;
        }
    }
    Mesh previous,current;
    buildAnimatedHero(previous,0,0,0,0);
    float totalMotion=0;
    for(int frame=1;frame<=2400;++frame) {
        float time=float(frame)/240,blend=heroAnimation::smooth(std::min(time,4-time));
        current.vertices.clear();buildAnimatedHero(current,time,time*7,blend,.15f*std::sin(time)*std::sin(time));
        if(current.vertices.size()!=previous.vertices.size()){std::cerr<<"Topology changed\n";return 1;}
        for(size_t i=0;i<current.vertices.size();++i) {
            const auto& v=current.vertices[i];float step=glm::length(v.position-previous.vertices[i].position);
            if(!std::isfinite(step)||!near(glm::length(v.normal),1,.002f)||step>.08f||glm::length(v.position)>4) {
                std::cerr<<"Mesh discontinuity/nonfinite normal at frame "<<frame<<'\n';return 1;
            }
            totalMotion+=step;
        }
        previous.vertices.swap(current.vertices);
    }
    if(totalMotion<1){std::cerr<<"No animation\n";return 1;}
    for(auto weapon:{combat::Weapon::Sword,combat::Weapon::Daggers,combat::Weapon::Bow,combat::Weapon::Staff}) {
        const auto style=weapon==combat::Weapon::Sword?combat::Class::Warrior:
                         weapon==combat::Weapon::Staff?combat::Class::Mage:combat::Class::Rogue;
        heroAnimation::Action action;
        previous.vertices.clear();
        buildAnimatedHero(previous,0,0,0,action.current,style,weapon);
        action.start(weapon);
        for(int frame=1;frame<200;++frame){
            // Retrigger a skill in mid-swing, as can happen during auto-attack.
            if(frame==50)action.start(weapon,0);
            action.advance(1.f/240);
            current.vertices.clear();
            buildAnimatedHero(current,float(frame)/240,0,0,action.current,style,weapon);
            if(previous.vertices.size()!=current.vertices.size())return 1;
            for(size_t i=0;i<current.vertices.size();++i){
                const auto& v=current.vertices[i];
                const float step=glm::length(v.position-previous.vertices[i].position);
                if(!std::isfinite(step)||!near(glm::length(v.normal),1,.002f)||step>.24f) {
                    std::cerr<<"Weapon pose discontinuity: weapon "<<int(weapon)<<" frame "<<frame<<"\n";return 1;
                }
            }
            previous.vertices.swap(current.vertices);
        }
        // Each slot must have a distinct contact pose, release to neutral and
        // preserve leg lengths even when cast while finishing a long stride.
        glm::vec3 handAtContact[4];
        for(int slot=0;slot<4;++slot){
            const auto& clip=heroAnimation::combatClip(weapon,style,slot);
            const auto contact=heroAnimation::attackPose(weapon,slot,clip.contact,style);
            handAtContact[slot]=contact.right-contact.left;
            for(int other=0;other<slot;++other){
                if(glm::distance(handAtContact[slot],handAtContact[other])<.18f)return 1;
            }
            for(int frame=0;frame<=240;++frame){
                float t=float(frame)/240;
                auto gesture=heroAnimation::attackPose(weapon,slot,t,style);
                for(float speed:{0.f,.5f,1.f}){
                    auto pose=heroAnimation::sample(t,t*2*pi,speed,gesture.weight,gesture.pivot);
                    for(const auto& leg:pose.legs){
                        if(!near(glm::distance(leg.hip,leg.knee),heroAnimation::thighLength,.001f)||
                           !near(glm::distance(leg.knee,leg.ankle),heroAnimation::shinLength,.001f)){
                            std::cerr<<"Unreachable action stance, weapon "<<int(weapon)<<" slot "<<slot<<" t "<<t<<'\n';
                            return 1;
                        }
                    }
                }
                current.vertices.clear();buildAnimatedHero(current,t,0,0,gesture,style,weapon);
                for(size_t i=0;i<current.vertices.size();++i){
                    const auto& v=current.vertices[i];
                    if(!std::isfinite(v.position.y)||v.position.y<-.05f||
                       (frame>0&&glm::distance(v.position,previous.vertices[i].position)>.24f)){
                        std::cerr<<"Action mesh discontinuity/ground contact, weapon "<<int(weapon)
                                 <<" slot "<<slot<<" frame "<<frame<<" vertex "<<i<<" height "<<v.position.y
                                 <<" step "<<(frame>0?glm::distance(v.position,previous.vertices[i].position):0.f)<<'\n';
                        return 1;
                    }
                }
                previous.vertices.swap(current.vertices);
                if(frame==240&&(glm::length(gesture.right)+glm::length(gesture.left)+glm::length(gesture.weight)>.0001f))return 1;
            }
        }
    }
    std::cout<<"IK reach, grounded support, swing clearance, boundary continuity, stride matching and mesh transitions pass.\n";
}
