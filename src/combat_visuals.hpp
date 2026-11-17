#pragma once
#include "renderer.hpp"

// Short-lived marks are fixed in world space and use the animation clock.
// Replaying a mark never feeds back into simulation, collision or damage.
class CombatVisuals {
public:
    void clear();
    void observe(const combat::Event& event,const combat::Game& game,float now);
    void draw(Mesh& mesh,std::vector<Particle>& particles,const combat::Game& game,float now);
private:
    enum class Mark { Impact, Whirl, Quake, Blink, Knives, Frost, Meteor };
    struct Stroke {
        Mark kind;
        glm::vec3 from,to,color;
        float yaw, start, duration, reach;
    };
    std::vector<Stroke> strokes;
};
