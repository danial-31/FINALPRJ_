#pragma once
#include <glm/glm.hpp>
#include <algorithm>
#include <cmath>
#include <vector>

constexpr float pi = 3.14159265359f;
struct Vertex { glm::vec3 position, normal, color; float material; };
struct Torch { glm::vec3 position; float scale; };
struct Mesh {
    std::vector<Vertex> vertices;
    void triangle(glm::vec3 a, glm::vec3 b, glm::vec3 c, glm::vec3 color, float material) {
        glm::vec3 n = glm::cross(b-a,c-a);
        if (glm::length(n)<0.000001f) return;
        n=glm::normalize(n);
        vertices.insert(vertices.end(),{{a,n,color,material},{b,n,color,material},{c,n,color,material}});
    }
    void quad(glm::vec3 a,glm::vec3 b,glm::vec3 c,glm::vec3 d,glm::vec3 color,float material) {
        triangle(a,b,c,color,material); triangle(a,c,d,color,material);
    }
    void block(glm::vec3 center,glm::vec3 size,float bevel,glm::vec3 color,float material,float yaw=0) {
        glm::vec3 h=size*.5f;
        float b=std::clamp(bevel,0.0f,std::min({h.x,h.y,h.z})*.85f);
        glm::vec3 q=h-glm::vec3(b);
        auto transform=[&](glm::vec3 p) { return center+glm::vec3(std::cos(yaw)*p.x+std::sin(yaw)*p.z,p.y,-std::sin(yaw)*p.x+std::cos(yaw)*p.z); };
        auto face=[&](std::vector<glm::vec3> p) {
            glm::vec3 avg(0); for(auto v:p) avg+=v;
            if(glm::dot(glm::cross(p[1]-p[0],p[2]-p[0]),avg)<0) std::reverse(p.begin(),p.end());
            for(size_t i=1;i+1<p.size();++i) triangle(transform(p[0]),transform(p[i]),transform(p[i+1]),color,material);
        };
        for(int axis=0;axis<3;++axis) {
            int u=(axis+1)%3,v=(axis+2)%3;
            for(int sign:{-1,1}) {
                std::vector<glm::vec3> p;
                for(auto s: {glm::ivec2(-1,-1),glm::ivec2(1,-1),glm::ivec2(1,1),glm::ivec2(-1,1)}) {
                    glm::vec3 a(0);a[axis]=float(sign)*h[axis];a[u]=float(s.x)*q[u];a[v]=float(s.y)*q[v];p.push_back(a);
                } face(p);
            }
        }
        if(b<=0) return;
        for(int axis=0;axis<3;++axis) {
            int u=(axis+1)%3,v=(axis+2)%3;
            for(int su:{-1,1}) for(int sv:{-1,1}) {
                glm::vec3 a(0),bb(0),c(0),d(0);
                a[axis]=-q[axis];a[u]=float(su)*h[u];a[v]=float(sv)*q[v];
                bb=a;bb[axis]=q[axis];
                c=bb;c[u]=float(su)*q[u];c[v]=float(sv)*h[v];
                d=c;d[axis]=-q[axis];face({a,bb,c,d});
            }
        }
        for(int sx:{-1,1}) for(int sy:{-1,1}) for(int sz:{-1,1}) {
            glm::vec3 s{float(sx),float(sy),float(sz)};
            auto a=q*s,bb=a,c=a;a.x=h.x*s.x;bb.y=h.y*s.y;c.z=h.z*s.z;face({a,bb,c});
        }
    }
    void box(glm::vec3 center,glm::vec3 size,glm::vec3 color,float material,float yaw=0) { block(center,size,0,color,material,yaw); }
    void cylinder(glm::vec3 center,float rb,float rt,float height,glm::vec3 color,float material,int segments=16) {
        for(int i=0;i<segments;++i) {
            float a=2*pi*float(i)/float(segments),b=2*pi*float(i+1)/float(segments);
            glm::vec3 na(std::cos(a),0,std::sin(a)),nb(std::cos(b),0,std::sin(b));
            glm::vec3 p0=center+na*rb-glm::vec3(0,height*.5f,0),p1=center+nb*rb-glm::vec3(0,height*.5f,0);
            glm::vec3 p2=center+nb*rt+glm::vec3(0,height*.5f,0),p3=center+na*rt+glm::vec3(0,height*.5f,0);
            quad(p0,p3,p2,p1,color,material);
            triangle(center-glm::vec3(0,height*.5f,0),p0,p1,color,material);
            triangle(center+glm::vec3(0,height*.5f,0),p2,p3,color,material);
        }
    }
    void sphere(glm::vec3 center,glm::vec3 radii,glm::vec3 color,float material,int slices=12,int stacks=8) {
        auto point=[&](int i,int j) {
            float a=2*pi*float(i)/float(slices),b=pi*(float(j)/float(stacks)-.5f);
            glm::vec3 n(std::cos(b)*std::cos(a),std::sin(b),std::cos(b)*std::sin(a));
            return Vertex{center+n*radii,glm::normalize(n/radii),color,material};
        };
        for(int j=0;j<stacks;++j) for(int i=0;i<slices;++i) {
            auto a=point(i,j),b=point(i+1,j),c=point(i+1,j+1),d=point(i,j+1);
            vertices.insert(vertices.end(),{a,d,c,a,c,b});
        }
    }
    void beam(glm::vec3 a,glm::vec3 b,float radius,glm::vec3 color,float material,int sides=8) {
        float length=glm::length(b-a);if(length<.0001f)return;
        glm::vec3 up=(b-a)/length,right=glm::normalize(glm::cross(up,std::abs(up.y)<.9f?glm::vec3(0,1,0):glm::vec3(1,0,0))),forward=glm::cross(right,up);
        Mesh temp;temp.cylinder(glm::vec3(0),radius,radius,length,color,material,sides);
        for(auto v:temp.vertices) {
            v.position=(a+b)*.5f+right*v.position.x+up*v.position.y+forward*v.position.z;
            v.normal=right*v.normal.x+up*v.normal.y+forward*v.normal.z;vertices.push_back(v);
        }
    }
};
void buildArchitecture(Mesh&);
void buildProps(Mesh&,std::vector<Torch>&);
