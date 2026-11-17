#pragma once
#include "scene.hpp"
#include "combat.hpp"
#include <array>

namespace heroAnimation {
inline constexpr float stanceFraction=.62f;
inline constexpr float stride=.82f;
inline constexpr float cycleDistance=stride/stanceFraction;
inline constexpr float thighLength=.39f, shinLength=.40f, floorHeight=.012f;
struct LegPose {
    glm::vec3 hip,knee,ankle;
    float pitch=0, phase=0, clearance=0;
    bool stance=true;
};
struct Pose {
    std::array<LegPose,2> legs; // left, right
    glm::vec3 pelvisOffset{0},headOffset{0};
    float pelvisYaw=0,torsoYaw=0,torsoRoll=0;
};
inline float smooth(float t) {t=std::clamp(t,0.f,1.f);return t*t*t*(t*(t*6-15)+10);}
// Match root displacement to planted-foot travel. Blend should be the same smoothed
// value supplied to sample/build; callers supply positive horizontal distance.
inline float gaitAdvance(float distance,float heroScale,float blend) {
    return distance*2*pi/(heroScale*cycleDistance*std::max(blend,.12f));
}
inline Pose sample(float time,float gait,float blend,glm::vec3 weight={},float pivot=0)
{
    blend=std::clamp(blend,0.f,1.f);
    Pose pose;
    const float weightShift=std::sin(gait-.32f);
    pose.pelvisOffset={.035f*blend*weightShift,
        blend*(-.133f+.026f*std::cos(2*gait-2.9f)),0};
    pose.pelvisYaw=.078f*blend*std::cos(gait-.25f);
    pose.torsoYaw=-.092f*blend*std::cos(gait-.44f);
    pose.torsoRoll=-.033f*blend*weightShift;
    pose.headOffset={pose.pelvisOffset.x*.48f,blend*(-.113f+.012f*std::cos(2*gait-3.1f))+.003f*std::sin(time*2.1f),-.029f*blend};
    pose.pelvisOffset+=weight*(1.f-.65f*blend);
    pose.pelvisYaw+=pivot;
    pose.torsoYaw+=pivot*.6f;
    for(int index=0;index<2;++index) {
        auto& leg=pose.legs[size_t(index)];
        const float side=index==0?-1.f:1.f;
        float phase=(gait+(index==0?pi:0))/(2*pi);
        phase-=std::floor(phase);leg.phase=phase;
        leg.stance=phase<stanceFraction;
        float z=0,pitch=0,lift=0;
        if(leg.stance) {
            float u=phase/stanceFraction;
            z=-stride*.5f+stride*u;
            pitch=.13f*(1-smooth(u/.17f))-.20f*smooth((u-.77f)/.23f);
        } else {
            float u=(phase-stanceFraction)/(1-stanceFraction);
            // Cubic Hermite preserves stance velocity at toe-off and heel strike.
            float tangent=stride*(1-stanceFraction)/stanceFraction;
            float u2=u*u,u3=u2*u;
            z=(2*u3-3*u2+1)*stride*.5f+(u3-2*u2+u)*tangent
              +(-2*u3+3*u2)*(-stride*.5f)+(u3-u2)*tangent;
            lift=.15f*std::pow(std::sin(pi*u),2.f);
            pitch=u<.5f?-.20f*(1-smooth(u*2)):.13f*smooth((u-.5f)*2);
        }
        z*=blend;pitch*=blend;lift*=blend;
        leg.pitch=pitch;leg.clearance=lift;
        const float contactZ=pitch>=0?.10f:-.21f;
        const float sine=std::sin(pitch),cosine=std::cos(pitch);
        // Rotate around the active heel/toe contact, keeping the sole above floor.
        leg.ankle={side*.135f,floorHeight+lift+.22f*cosine+contactZ*sine,
                   z+.22f*sine-contactZ*(cosine-1)};
        leg.hip={pose.pelvisOffset.x+side*.135f*std::cos(pose.pelvisYaw),
                 1.0f+pose.pelvisOffset.y,pose.pelvisOffset.z-side*.135f*std::sin(pose.pelvisYaw)};
        const glm::vec3 delta=leg.ankle-leg.hip;
        const float distance=glm::length(delta);
        const glm::vec3 axis=delta/distance;
        // Analytic two-bone IK, with an anatomical forward-facing knee pole.
        const float along=(thighLength*thighLength-shinLength*shinLength+distance*distance)/(2*distance);
        const float bend=std::sqrt(std::max(0.f,thighLength*thighLength-along*along));
        const glm::vec3 pole{side*.08f,0,-1};
        const glm::vec3 forward=glm::normalize(pole-axis*glm::dot(pole,axis));
        leg.knee=leg.hip+axis*along+forward*bend;
    }
    return pose;
}

struct Gesture {
    glm::vec3 torso{0}, right{0}, left{0}; // shoulder pitch, yaw and roll in joint space
    glm::vec3 bend{0};                    // elbows and cloak follow-through
    glm::vec3 weight{0}, wrist{0};        // pelvis offset; wrist pitches and main-hand roll
    float pivot=0, spin=0, draw=0;        // hip yaw, full-body turn, bow draw
};
inline Gesture blendGesture(const Gesture& a,const Gesture& b,float t) {
    return {glm::mix(a.torso,b.torso,t),glm::mix(a.right,b.right,t),glm::mix(a.left,b.left,t),
            glm::mix(a.bend,b.bend,t),glm::mix(a.weight,b.weight,t),glm::mix(a.wrist,b.wrist,t),
            glm::mix(a.pivot,b.pivot,t),glm::mix(a.spin,b.spin,t),glm::mix(a.draw,b.draw,t)};
}

// The two sculpted poses are anticipation and contact. Angles are authored in
// degrees. Negative local Z is forward; the feet stay pinned while the hips shift.
inline Gesture key(glm::vec3 chest,glm::vec3 armR,glm::vec3 armL,glm::vec2 elbows,
                   glm::vec3 weight,glm::vec3 wrist,float hipYaw=0,float bowDraw=0) {
    return {glm::radians(chest),glm::radians(armR),glm::radians(armL),
            {glm::radians(elbows.x),glm::radians(elbows.y),0},weight,glm::radians(wrist),
            glm::radians(hipYaw),0,bowDraw};
}
struct Clip {
    float seconds, ready, contact, release;
    Gesture prepare, extend;
};
inline const Clip& combatClip(combat::Weapon weapon,combat::Class style,int slot) {
    static const Clip basic[]={
        // Edge cut, thrust, draw/release and staff bolt.
        {.55f,.28f,.49f,.52f,
         key({8,-29,4},{58,-35,32},{27,-5,-8},{46,14},{.048f,-.064f,.052f},{-32,0,28},-11),
         key({-14,43,-7},{74,25,-36},{15,15,-24},{-12,22},{-.052f,-.09f,-.100f},{32,0,-42},15)},
        {.39f,.21f,.43f,.46f,
         key({6,-25,-6},{-19,-12,25},{42,8,-12},{55,14},{.048f,-.065f,.065f},{-15,13,12},-10),
         key({-19,26,6},{87,11,-12},{-26,0,-29},{-24,25},{-.036f,-.113f,-.143f},{14,-16,28},12)},
        {.66f,.43f,.54f,.60f,
         key({-4,27,0},{68,-45,48},{78,29,-8},{66,-24},{.025f,-.038f,.025f},{3,0,0},13,1),
         key({5,35,-4},{62,-68,66},{75,32,-9},{44,-24},{.045f,-.046f,.052f},{-30,0,0},9)},
        {.58f,.26f,.46f,.52f,
         key({6,-24,5},{45,-32,19},{52,15,-30},{32,29},{.026f,-.045f,.03f},{-22,0,18},-8),
         key({-13,24,-3},{68,6,4},{87,-7,-11},{-12,-20},{-.044f,-.079f,-.08f},{34,12,4},9)}
    };
    static const Clip skills[3][4]={
        { // Warrior: shield bash, whirlwind, bastion, ground breaker.
            {.49f,.22f,.43f,.52f,
             key({6,22,6},{38,12,18},{19,-28,-27},{25,68},{.04f,-.074f,.062f},{-17,17,7},8),
             key({-23,-26,-9},{-15,17,24},{87,14,-10},{33,-15},{-.09f,-.135f,-.13f},{8,-12,0},-15)},
            {.80f,.21f,.50f,.67f,
             key({-10,-35,-9},{57,-51,53},{32,22,-24},{42,32},{.06f,-.135f,.04f},{-23,0,47},-18),
             key({-5,29,12},{54,-12,78},{34,21,-56},{-12,14},{-.035f,-.093f,-.048f},{28,0,-26},12)},
            {.78f,.24f,.43f,.69f,
             key({-7,4,3},{32,-3,14},{25,-20,-18},{33,47},{0,-.11f,.02f},{-11,10,0},5),
             key({-13,-19,-8},{48,15,-13},{55,24,11},{62,48},{-.06f,-.175f,-.045f},{-46,8,-23},-6)},
            {.87f,.38f,.57f,.63f,
             key({12,-25,6},{145,-18,17},{68,-15,-27},{25,41},{.03f,-.052f,.07f},{-56,0,9},-12),
             key({-34,21,-8},{34,5,-11},{-12,7,-36},{-13,29},{-.044f,-.222f,-.121f},{29,10,-24},11)}
        },
        { // Rogue: poison thrust, shadow step, fan of knives, death mark.
            {.43f,.22f,.45f,.51f,
             key({-8,-32,9},{4,-31,16},{38,12,-26},{72,28},{.07f,-.105f,.052f},{-35,12,32},-12),
             key({-23,33,-11},{92,8,-21},{-34,-3,-36},{-25,9},{-.065f,-.157f,-.15f},{28,-10,-20},13)},
            {.48f,.20f,.45f,.54f,
             key({-21,19,-14},{58,11,-20},{35,-12,5},{48,50},{-.075f,-.19f,.055f},{-22,-5,9},7),
             key({-29,-21,11},{-32,-18,36},{75,11,-36},{16,-9},{.075f,-.17f,-.125f},{16,12,38},-11)},
            {.65f,.29f,.47f,.59f,
             key({-12,-20,4},{63,29,-19},{59,-25,20},{74,65},{0,-.122f,.035f},{-28,-23,-32},-13),
             key({-7,20,-6},{62,-38,74},{61,31,-63},{-16,-12},{.021f,-.075f,-.055f},{35,34,54},10)},
            {.63f,.33f,.52f,.64f,
             key({8,27,-12},{96,-20,48},{102,20,-43},{47,29},{-.04f,-.068f,.035f},{-36,-21,28},14),
             key({-23,-21,8},{71,38,-25},{65,-26,19},{15,4},{.056f,-.16f,-.112f},{51,36,-36},-9)}
        },
        { // Mage: firebolt, ice wave, blink, meteor.
            {.53f,.23f,.43f,.52f,
             key({4,-28,6},{39,-39,27},{68,16,-7},{39,65},{.04f,-.055f,.055f},{-17,0,-14},-8),
             key({-15,27,-7},{72,10,-9},{95,4,-9},{6,-28},{-.045f,-.094f,-.096f},{41,-20,17},11)},
            {.77f,.34f,.49f,.62f,
             key({-14,9,4},{65,19,-19},{73,-22,14},{63,59},{0,-.156f,.033f},{-25,0,9},6),
             key({4,-7,-6},{51,-36,62},{62,42,-68},{-9,-18},{-.022f,-.067f,-.043f},{17,17,-26},-4)},
            {.49f,.21f,.40f,.49f,
             key({-19,27,-8},{49,24,-15},{46,-17,8},{57,72},{-.047f,-.17f,.025f},{-36,0,-19},8),
             key({-10,-32,13},{24,-53,40},{77,24,-39},{3,14},{.062f,-.09f,-.105f},{12,-16,19},-10)},
            {1.00f,.42f,.62f,.73f,
             key({10,-15,8},{119,-16,22},{131,15,-33},{21,36},{.008f,-.045f,.052f},{-35,-11,-14},-8),
             key({-28,27,-9},{58,8,7},{78,19,-23},{-8,-16},{-.045f,-.191f,-.114f},{56,24,12},11)}
        }
    };
    return slot<0?basic[int(weapon)]:skills[int(style)][std::clamp(slot,0,3)];
}
inline float ramp(float t,float start,float end){return smooth((t-start)/(end-start));}
inline Gesture sampleClip(const Clip& clip,float t) {
    float ready=ramp(t,0,clip.ready),extend=ramp(t,clip.ready,clip.contact);
    float recover=ramp(t,clip.release,1);
    return blendGesture(blendGesture(blendGesture({},clip.prepare,ready),clip.extend,extend),{},recover);
}
inline combat::Class weaponClass(combat::Weapon weapon) {
    return weapon==combat::Weapon::Sword?combat::Class::Warrior:
           weapon==combat::Weapon::Staff?combat::Class::Mage:combat::Class::Rogue;
}
inline Gesture attackPose(combat::Weapon weapon,int slot,float t,combat::Class style) {
    const auto& clip=combatClip(weapon,style,slot);
    Gesture pose=sampleClip(clip,t);
    // Hips initiate the transfer, the hands follow, then the wrists and fabric
    // catch up. Warping normalized time leaves the same exact rest endpoints.
    float offset=t*(1.f-t);
    const auto lead=sampleClip(clip,t+.18f*offset),tip=sampleClip(clip,t-.12f*offset);
    pose.weight=lead.weight;pose.pivot=lead.pivot;pose.wrist=tip.wrist;
    pose.bend.z=(ramp(t,clip.ready,clip.contact+.09f)*(1-ramp(t,clip.release+.06f,1)));
    if(style==combat::Class::Warrior&&slot==1){
        pose.spin=2*pi*ramp(t,.13f,.86f)-.38f*ramp(t,0,.13f)*(1-ramp(t,.13f,.39f));
    }
    return pose;
}
inline Gesture attackPose(combat::Weapon weapon,int slot,float t) {
    return attackPose(weapon,slot,t,weaponClass(weapon));
}
struct Action {
    float phase=1, duration=.5f;
    combat::Weapon weapon=combat::Weapon::Sword;
    combat::Class style=combat::Class::Warrior;
    int slot=-1;
    Gesture from{},current{};
    void start(combat::Weapon w,int ability,combat::Class heroClass) {
        from=current;from.spin=std::remainder(from.spin,2*pi);
        phase=0;weapon=w;slot=ability;style=heroClass;
        duration=combatClip(w,style,slot).seconds;
    }
    void start(combat::Weapon w,int ability=-1){start(w,ability,weaponClass(w));}
    void advance(float dt) {
        if(phase>=1)return;
        phase=std::min(1.f,phase+dt/duration);
        current=blendGesture(from,attackPose(weapon,slot,phase,style),smooth(phase/.22f));
        if(phase==1)current={};
    }
};
}

void buildAnimatedHero(Mesh& mesh,float time,float gait,float speed,const heroAnimation::Gesture& gesture,
                       combat::Class style,combat::Weapon weapon);
inline void buildAnimatedHero(Mesh& mesh,float time,float gait,float speed,float phase) {
    buildAnimatedHero(mesh,time,gait,speed,heroAnimation::attackPose(combat::Weapon::Sword,-1,phase),
                      combat::Class::Warrior,combat::Weapon::Sword);
}
void buildHero(Mesh& mesh);
