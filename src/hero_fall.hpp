#pragma once
#include "hero_animation.hpp"
#include "zone_mesh.hpp"
#include <glm/gtc/matrix_transform.hpp>

namespace heroAnimation {
// Local-space defeat clip. Keep the live attack and planted feet at the first sample.
struct Fall {
    static constexpr float contact=.96f, settled=1.30f, reveal=1.65f;
    float age=-1, startTime=0, direction=0;

    bool active() const { return age>=0; }
    bool pending() const { return active()&&age<reveal; }
    void start(float time,const combat::Game& game,float yaw){
        age=0;startTime=time;direction=0;
        // Prefer falling backwards, then towards the nearest open side of the tile.
        int best=-1;
        for(float angle:{0.f,pi*.25f,-pi*.25f,pi*.5f,-pi*.5f,pi*.75f,-pi*.75f,pi}){
            glm::vec2 along(std::sin(yaw+angle),std::cos(yaw+angle));
            int steps=0;
            while(steps<6&&game.walkablePos(0,game.actors[0].pos+along*(.45f*float(steps+1))))++steps;
            if(steps>best){best=steps;direction=angle;}
            if(steps==6)break;
        }
    }
    bool advance(float dt){
        if(!pending())return false;
        age=std::min(reveal,age+dt);
        return age==reveal;
    }
    float retainStride() const {return 1.f-smooth(age/.58f);}

    Gesture pose(const Gesture& initial,combat::Weapon weapon) const{
        float buckle=smooth((age-.09f)/.46f);
        float reach=smooth((age-.03f)/.26f);
        float release=smooth((age-.51f)/.54f);
        Gesture p{};
        p.spin=initial.spin;
        p.weight={-.078f*buckle,-.32f*buckle+.15f*release,.045f*buckle};
        p.pivot=-.19f*buckle;
        p.torso={.23f*reach-.33f*buckle+.15f*release,.23f*buckle,-.11f*reach};
        p.right={.67f*reach-.83f*release,.28f*buckle,.56f*reach-.12f*release};
        p.left={.39f*reach-.47f*release,-.14f*buckle,-.78f*reach+.12f*release};
        p.bend={.64f*reach-.42f*release,.41f*reach-.35f*release,.35f*(buckle-release)};
        if(weapon==combat::Weapon::Sword||weapon==combat::Weapon::Daggers){
            p.wrist.x=-.98f*release;
            p.wrist.y=weapon==combat::Weapon::Daggers?-.82f*release:0.f;
        }
        return blendGesture(initial,p,smooth(age/.25f));
    }

    glm::mat4 groundTransform(Mesh& mesh) const{
        float tip=std::clamp((age-.33f)/(contact-.33f),0.f,1.f);
        // Acceleration continues until contact; the shoulder then rebounds and settles.
        float rebound=std::clamp((age-contact)/(settled-contact),0.f,1.f);
        float pitch=.13f*smooth(age/.20f)+1.36f*tip*tip-.10f*std::sin(pi*rebound)*(1.f-rebound);
        float roll=.39f*smooth((age-.10f)/.80f);
        glm::mat4 rotation=glm::rotate(glm::mat4(1),direction,glm::vec3(0,1,0));
        rotation=glm::rotate(rotation,roll,glm::vec3(0,0,1));
        rotation=glm::rotate(rotation,pitch,glm::vec3(1,0,0));
        rotation=glm::rotate(rotation,-direction,glm::vec3(0,1,0));
        // Equipment length, class proportions and the pose at impact all change the support point.
        glm::vec3 vertical{rotation[0].y,rotation[1].y,rotation[2].y};
        float lowest=0;
        for(const auto& v:mesh.vertices)
            if(v.material!=mat::Cloth)lowest=std::min(lowest,glm::dot(vertical,v.position));
        rotation[3].y=-lowest*smooth(age/.22f);
        // Loose fabric settles against the floor instead of supporting the torso on its hem.
        for(size_t i=0;i+2<mesh.vertices.size();i+=3){
            auto* tri=&mesh.vertices[i];
            if(tri[0].material!=mat::Cloth)continue;
            bool touched=false;
            for(int j=0;j<3;++j){
                float height=glm::dot(vertical,tri[j].position)+rotation[3].y;
                if(height<.01f){tri[j].position+=vertical*(.01f-height);touched=true;}
            }
            if(touched){
                glm::vec3 n=glm::cross(tri[1].position-tri[0].position,tri[2].position-tri[0].position);
                if(glm::dot(n,n)>1.e-12f)tri[0].normal=tri[1].normal=tri[2].normal=glm::normalize(n);
            }
        }
        return rotation;
    }
};
}
