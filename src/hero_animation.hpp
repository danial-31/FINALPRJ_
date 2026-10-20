#pragma once
#include "scene.hpp"
#include <array>

namespace heroAnimation {
inline constexpr float stanceFraction=.62f;
inline constexpr float stride=.60f;
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
inline Pose sample(float time,float gait,float blend)
{
    blend=std::clamp(blend,0.f,1.f);
    Pose pose;
    pose.pelvisOffset={.023f*blend*std::sin(gait),
        blend*(-.06f+.018f*std::cos(gait*2)),0};
    pose.pelvisYaw=.045f*blend*std::sin(gait);
    pose.torsoYaw=-.035f*blend*std::sin(gait-.16f);
    pose.torsoRoll=-.018f*blend*std::sin(gait);
    pose.headOffset={pose.pelvisOffset.x*.45f,blend*(-.06f+.006f*std::cos(gait*2))+.002f*std::sin(time*2.1f),0};
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
                 1.0f+pose.pelvisOffset.y,-side*.135f*std::sin(pose.pelvisYaw)};
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
}

// Append a pose with constant topology. time is seconds, gait is radians,
// speed is the smoothed locomotion blend, cast is a smoothed 0..1 gesture.
void buildAnimatedHero(Mesh& mesh,float time,float gait,float speed,float cast);
void buildHero(Mesh& mesh);
