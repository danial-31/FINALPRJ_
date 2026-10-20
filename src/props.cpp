#include "hero_animation.hpp"
#include "scene_layout.hpp"
#include <glm/gtc/matrix_transform.hpp>

#include <algorithm>
#include <cmath>
#include <random>

namespace {
using glm::vec3;
const vec3 stone{0.38f, 0.39f, 0.38f};
const vec3 iron{0.13f, 0.16f, 0.17f};
const vec3 brass{0.36f, 0.28f, 0.16f};
const vec3 bone{0.61f, 0.57f, 0.44f};
const vec3 wood{0.27f, 0.19f, 0.12f};
const vec3 cloth{0.23f, 0.035f, 0.045f};

vec3 offset(vec3 p, float x, float y, float z) { return p + vec3{x,y,z}; }

void candle(Mesh& m, vec3 p, float height, float radius = 0.052f)
{
    m.cylinder(offset(p,0,0.025f,0), radius*1.8f, radius*1.8f, 0.05f, brass, 4, 12);
    m.cylinder(offset(p,0,height*0.5f+0.05f,0), radius,radius*0.87f,height,
               vec3{0.67f,0.60f,0.42f},6,10);
    for (int i=0;i<3;++i) {
        const float a = float(i)*2.15f;
        m.cylinder(offset(p,std::cos(a)*radius*0.85f,height*0.75f+0.05f,
                          std::sin(a)*radius*0.85f),
                   radius*0.23f,radius*0.16f,height*0.28f,vec3{0.73f,0.66f,0.49f},6,6);
    }
    m.beam(offset(p,0,height+0.045f,0),offset(p,0,height+0.10f,0),0.009f,iron,3,5);
    m.sphere(offset(p,0.008f,height+0.14f,0),{radius*0.58f,0.095f,radius*0.45f},
             {1.0f,0.49f,0.075f},8,7,5);
}

void brazier(Mesh& m, std::vector<Torch>& lights, vec3 p, float height, float scale)
{
    m.block(offset(p,0,0.10f,0),{0.62f*scale,0.20f,0.62f*scale},0.055f,stone*0.8f,0);
    m.cylinder(offset(p,0,0.27f,0),0.24f*scale,0.12f*scale,0.18f,iron,3,8);
    m.cylinder(offset(p,0,height*0.5f,0),0.075f*scale,0.09f*scale,
               height-0.48f,iron*1.4f,3,8);
    for (float y : {0.40f,height-0.40f})
        m.cylinder(offset(p,0,y,0),0.14f*scale,0.14f*scale,0.09f,brass*0.65f,4,12);
    m.cylinder(offset(p,0,height-0.16f,0),0.12f*scale,0.38f*scale,0.32f,iron,3,12);
    m.cylinder(offset(p,0,height+0.005f,0),0.395f*scale,0.395f*scale,0.055f,brass*0.72f,4,12);
    m.cylinder(offset(p,0,height+0.035f,0),0.33f*scale,0.33f*scale,0.025f,
               {0.61f,0.12f,0.018f},8,12);
    for (int i=0;i<8;++i) {
        const float a=float(i)*2*pi/8;
        const vec3 radial{std::cos(a),0,std::sin(a)};
        m.beam(offset(p,0,height-0.25f,0)+radial*0.16f*scale,
               offset(p,0,height+0.21f,0)+radial*0.43f*scale,0.027f*scale,iron,3,6);
        m.sphere(offset(p,0,height+0.21f,0)+radial*0.43f*scale,
                 vec3{0.04f*scale},brass,4,6,4);
    }
    lights.push_back({offset(p,0,height+0.30f,0),scale});
}

void barrel(Mesh& m, vec3 p, float s)
{
    const float radius=0.40f*s, height=1.08f*s;
    m.cylinder(offset(p,0,height*0.5f,0),radius*0.84f,radius*0.84f,height,wood*0.60f,2,14);
    for (int i=0;i<14;++i) {
        const float a=2*pi*(float(i)+0.5f)/14;
        const vec3 color=wood*(0.84f+0.04f*float(i%5));
        m.block(offset(p,std::sin(a)*radius*0.88f,height*0.5f,std::cos(a)*radius*0.88f),
                {0.155f*s,height,0.085f*s},0.014f*s,color,2,a);
    }
    for (float y : {0.13f,0.38f,0.77f,0.97f}) {
        for (int i=0;i<14;++i) {
            const float a=2*pi*float(i)/14, b=2*pi*float(i+1)/14;
            m.quad(offset(p,std::sin(a)*radius,y*s-0.035f*s,std::cos(a)*radius),
                   offset(p,std::sin(b)*radius,y*s-0.035f*s,std::cos(b)*radius),
                   offset(p,std::sin(b)*radius,y*s+0.035f*s,std::cos(b)*radius),
                   offset(p,std::sin(a)*radius,y*s+0.035f*s,std::cos(a)*radius),iron,3);
        }
    }
    m.cylinder(offset(p,0,height+0.012f*s,0),radius*0.85f,radius*0.85f,0.035f*s,
               wood*0.95f,2,14);
    for (int i=-2;i<=2;++i) {
        const float x=float(i)*0.12f*s;
        const float half=std::sqrt(std::max(0.0f,radius*radius*0.69f-x*x));
        m.beam(offset(p,x,height+0.032f*s,-half),
               offset(p,x,height+0.032f*s,half),0.007f*s,wood*0.45f,2,4);
    }
    m.cylinder(offset(p,0.1f*s,height+0.045f*s,0.08f*s),0.045f*s,0.045f*s,
               0.018f*s,wood*0.45f,2,8);
}

void crate(Mesh& m, vec3 p, vec3 size, float yaw)
{
    const auto q = [&](vec3 v) {
        return p + vec3{std::cos(yaw)*v.x+std::sin(yaw)*v.z,v.y,
                        -std::sin(yaw)*v.x+std::cos(yaw)*v.z};
    };
    m.box(q({0,size.y*0.5f,0}),size,wood*0.55f,2,yaw);
    for(int i=0;i<5;++i) {
        float x=(float(i)-2.0f)*size.x/5.0f;
        m.block(q({x,size.y*0.5f,size.z*0.5f+0.015f}),
                {size.x/5.0f-0.014f,size.y-0.04f,0.04f},0.008f,wood*(0.8f+float(i%3)*0.1f),2,yaw);
        m.block(q({x,size.y+0.012f,0}),{size.x/5.0f-0.015f,0.04f,size.z-0.04f},
                0.008f,wood*0.9f,2,yaw);
    }
    for(float x : {-size.x*0.41f,size.x*0.41f}) {
        m.box(q({x,size.y*0.5f,size.z*0.5f+0.045f}),{0.09f,size.y+0.03f,0.07f},wood*0.65f,2,yaw);
        for(float y : {0.10f,size.y-0.10f})
            m.sphere(q({x,y,size.z*0.5f+0.085f}),{0.025f,0.025f,0.014f},iron,3,6,4);
    }
    m.beam(q({-size.x*0.36f,0.12f,size.z*0.5f+0.085f}),
           q({size.x*0.36f,size.y-0.12f,size.z*0.5f+0.085f}),0.055f,wood*1.12f,2,4);
}

void chest(Mesh& m, vec3 p)
{
    constexpr float w=1.78f,d=1.03f,y=0.73f;
    // Planked chest with a closed barrel-vault lid.
    m.block(offset(p,0,0.13f,0),{w+0.12f,0.18f,d+0.12f},0.04f,iron,3);
    m.box(offset(p,0,0.47f,-d*0.5f),{w,0.59f,0.09f},wood,2);
    m.box(offset(p,-w*0.5f,0.47f,0),{0.09f,0.59f,d},wood*0.8f,2);
    m.box(offset(p,w*0.5f,0.47f,0),{0.09f,0.59f,d},wood*0.8f,2);
    for(int i=0;i<8;++i) {
        m.block(offset(p,-w*0.5f+(float(i)+0.5f)*w/8,0.47f,d*0.5f),
                {w/8-0.012f,0.59f,0.10f},0.012f,wood*(0.76f+float(i%3)*0.13f),2);
    }
    m.box(offset(p,0,0.54f,0),{w-0.14f,0.08f,d-0.15f},cloth*0.65f,5);
    for(float x : {-w*0.5f,w*0.5f}) for(float z : {-d*0.5f,d*0.5f}) {
        m.box(offset(p,x,0.45f,z),{0.12f,0.65f,0.12f},iron,3);
        for(float h : {0.22f,0.66f})
            m.sphere(offset(p,x,h,z+0.065f),{0.032f,0.032f,0.018f},brass,4,7,4);
    }
    for(float h : {0.22f,0.75f})
        m.box(offset(p,0,h,d*0.5f+0.06f),{w+0.07f,0.07f,0.045f},brass*0.65f,4);
    // Barrel-vault lid rests on the chest body.
    const auto lid = [&](float x,float yy,float z) {
        const float angle=0.0f;
        return offset(p,x,y+std::cos(angle)*yy+std::sin(angle)*(z+d*0.5f),
                        -d*0.5f-std::sin(angle)*yy+std::cos(angle)*(z+d*0.5f));
    };
    for(int i=0;i<12;++i) {
        const float a=pi*float(i)/12,b=pi*float(i+1)/12;
        m.quad(lid(-w*0.5f,std::sin(a)*0.43f,std::cos(a)*d*0.5f),
               lid(w*0.5f,std::sin(a)*0.43f,std::cos(a)*d*0.5f),
               lid(w*0.5f,std::sin(b)*0.43f,std::cos(b)*d*0.5f),
               lid(-w*0.5f,std::sin(b)*0.43f,std::cos(b)*d*0.5f),
               wood*(0.86f+float(i%3)*0.10f),2);
        for(float x : {-0.68f,0.68f})
            m.beam(lid(x,std::sin(a)*0.45f,std::cos(a)*0.54f),
                   lid(x,std::sin(b)*0.45f,std::cos(b)*0.54f),0.035f,brass*0.7f,4,6);
        m.triangle(lid(-w*0.5f,0,0),lid(-w*0.5f,std::sin(a)*0.43f,std::cos(a)*d*0.5f),
                   lid(-w*0.5f,std::sin(b)*0.43f,std::cos(b)*d*0.5f),wood*0.76f,2);
        m.triangle(lid(w*0.5f,0,0),lid(w*0.5f,std::sin(b)*0.43f,std::cos(b)*d*0.5f),
                   lid(w*0.5f,std::sin(a)*0.43f,std::cos(a)*d*0.5f),wood*0.76f,2);
    }
    m.block(offset(p,0,0.56f,d*0.5f+0.09f),{0.24f,0.30f,0.085f},0.035f,brass,4);
    m.box(offset(p,0,0.56f,d*0.5f+0.14f),{0.035f,0.09f,0.015f},iron,3);

}

void urn(Mesh& m, vec3 p, float s)
{
    const vec3 clay{0.28f,0.19f,0.13f};
    m.cylinder(offset(p,0,0.055f*s,0),0.21f*s,0.21f*s,0.11f*s,clay*0.7f,0,12);
    m.cylinder(offset(p,0,0.24f*s,0),0.16f*s,0.32f*s,0.30f*s,clay,0,12);
    m.cylinder(offset(p,0,0.48f*s,0),0.32f*s,0.14f*s,0.22f*s,clay*1.1f,0,12);
    m.cylinder(offset(p,0,0.66f*s,0),0.14f*s,0.17f*s,0.15f*s,clay,0,12);
    m.cylinder(offset(p,0,0.745f*s,0),0.20f*s,0.20f*s,0.05f*s,clay*1.15f,0,12);
    m.cylinder(offset(p,0,0.773f*s,0),0.15f*s,0.15f*s,0.008f*s,{0.035f,0.028f,0.022f},0,12);
    for(int side : {-1,1}) {
        vec3 a=offset(p,float(side)*0.18f*s,0.62f*s,0);
        vec3 b=offset(p,float(side)*0.39f*s,0.52f*s,0);
        vec3 c=offset(p,float(side)*0.25f*s,0.33f*s,0);
        m.beam(a,b,0.045f*s,clay*0.85f,0,7);
        m.beam(b,c,0.045f*s,clay*0.85f,0,7);
    }
}


void transform(Mesh& m, size_t first, const glm::mat4& matrix)
{
    const glm::mat3 rotation(matrix);
    for(size_t i=first;i<m.vertices.size();++i) {
        m.vertices[i].position=vec3(matrix*glm::vec4(m.vertices[i].position,1));
        m.vertices[i].normal=glm::normalize(rotation*m.vertices[i].normal);
    }
}

// Scale a generated prop around its cell center, preserving correct surface normals.
void scaleProp(Mesh& m, size_t first, vec3 center, vec3 scale)
{
    for(size_t i=first;i<m.vertices.size();++i) {
        auto& v=m.vertices[i];
        v.position=center+(v.position-center)*scale;
        v.normal=glm::normalize(v.normal/scale);
    }
}

void ring(Mesh& m, vec3 center, float radius, float thickness, vec3 color, float material)
{
    for(int i=0;i<48;++i) {
        float a=2*pi*float(i)/48,b=2*pi*float(i+1)/48;
        m.beam(center+vec3{std::cos(a)*radius,std::sin(a)*radius,0},
               center+vec3{std::cos(b)*radius,std::sin(b)*radius,0},thickness,color,material,6);
    }
}

void statue(Mesh& m, vec3 p)
{
    const vec3 carved=stone*.88f;
    m.block(offset(p,0,.16f,0),{1.5f,.32f,1.5f},.07f,stone*.77f,0);
    m.block(offset(p,0,.37f,0),{1.25f,.12f,1.25f},.035f,stone,0);
    // The robe spreads at the hem; its folds sweep sideways under the mantle.
    auto robe=[&](int level,int segment) {
        const float v=float(level)/7,a=2*pi*float(segment)/40;
        const float radius=.56f*(1-v)+.29f*v;
        const float fold=(.025f+.025f*(1-v))*std::cos(a*10+v*1.7f);
        return offset(p,std::sin(a)*(radius+fold)+.045f*std::sin(v*pi),
                      .44f+v*2.22f,std::cos(a)*(radius*.83f+fold));
    };
    for(int row=0;row<7;++row) for(int i=0;i<40;++i)
        m.quad(robe(row,i),robe(row,i+1),robe(row+1,i+1),robe(row+1,i),
               carved*(.95f+.025f*float(i%4)),0);
    // Hood is an angular shell with a pointed crown, a deep open face, and a heavy lip.
    const vec3 outer[]={{0,3.80f,.02f},{.22f,3.57f,.16f},{.38f,3.21f,.28f},
        {.36f,2.91f,.32f},{.23f,2.70f,.34f},{0,2.64f,.36f},
        {-.23f,2.70f,.34f},{-.36f,2.91f,.32f},{-.38f,3.21f,.28f},{-.22f,3.57f,.16f}};
    const vec3 inner[]={{0,3.47f,.25f},{.12f,3.35f,.32f},{.22f,3.15f,.38f},
        {.20f,2.96f,.40f},{.12f,2.85f,.40f},{0,2.81f,.40f},
        {-.12f,2.85f,.40f},{-.20f,2.96f,.40f},{-.22f,3.15f,.38f},{-.12f,3.35f,.32f}};
    for(int i=0;i<10;++i) {
        int j=(i+1)%10;
        vec3 backI{outer[i].x*.78f,outer[i].y,-.26f},backJ{outer[j].x*.78f,outer[j].y,-.26f};
        m.quad(p+outer[j],p+outer[i],p+inner[i],p+inner[j],carved*1.10f,0);
        m.quad(p+outer[i],p+outer[j],p+backJ,p+backI,carved,0);
        m.triangle(p+backI,p+backJ,offset(p,0,3.18f,-.34f),carved*.92f,0);
        vec3 insetI=inner[i]-vec3{0,0,.12f},insetJ=inner[j]-vec3{0,0,.12f};
        m.quad(p+inner[j],p+inner[i],p+insetI,p+insetJ,carved*.50f,0);
        m.triangle(offset(p,0,3.12f,.16f),p+insetJ,p+insetI,carved*.25f,0);
    }
    // Layered shoulder mantle: nested hanging curves, not separate spherical shoulders.
    for(int layer=0;layer<3;++layer) for(int i=0;i<20;++i) {
        auto q=[&](int col,float v) {
            float u=float(col)/20,x=(u-.5f)*1.05f;
            float sag=std::sin(u*pi);
            return offset(p,x,2.72f-.14f*float(layer)-.25f*v-.25f*sag,
                          .08f+.28f*sag+.035f*v+.025f*float(layer));
        };
        m.quad(q(i,0),q(i,1),q(i+1,1),q(i+1,0),carved*(1.06f-.06f*float(layer)),0);
    }
    for(int side:{-1,1}) {
        float sign=float(side);
        const vec3 shoulder=offset(p,sign*.42f,2.60f,.01f);
        const vec3 elbow=offset(p,sign*.43f,2.18f,.23f);
        const vec3 cuff=offset(p,sign*.22f,2.20f,.47f);
        m.beam(shoulder,elbow,.15f,carved*.94f,0,12);
        m.beam(elbow,cuff,.12f,carved,0,12);
        // Diagonal folds lie over the sleeves, tapering into the wrists.
        for(int f=0;f<3;++f) {
            float dy=.055f*float(f);
            m.beam(elbow+vec3{0,dy,.07f},cuff+vec3{0,dy*.25f,.07f},.018f,carved*.76f,0,6);
        }
        m.sphere(offset(p,sign*.18f,2.21f,.51f),{.075f,.095f,.065f},carved*1.08f,0,10,6);
        for(int finger=0;finger<3;++finger) {
            float y=2.17f+.035f*float(finger);
            m.beam(offset(p,sign*.22f,y,.56f),offset(p,sign*.14f,y+.008f,.57f),.013f,carved*1.03f,0,6);
        }
    }
    ring(m,offset(p,0,2.22f,.54f),.235f,.023f,brass,4);
}

void book(Mesh& m, vec3 p, float yaw, vec3 cover)
{
    m.block(offset(p,0,.035f,0),{.65f,.07f,.45f},.012f,cover,5,yaw);
    m.box(offset(p,0,.11f,0),{.60f,.10f,.40f},bone*.9f,6,yaw);
    m.block(offset(p,0,.18f,0),{.65f,.04f,.45f},.009f,cover,5,yaw);
}
} // namespace

void buildProps(Mesh& m, std::vector<Torch>& torches)
{
    const vec3 altar=layout::center(layout::altar)+vec3{0,.42f,0};
    const size_t altarStart=m.vertices.size();
    m.block(offset(altar,0,.14f,0),{4.35f,.28f,1.65f},.07f,stone*.8f,0);
    for(float x:{-1.65f,1.65f}) {
        m.block(offset(altar,x,.69f,0),{.62f,.95f,1.2f},.065f,stone*.9f,0);
        m.block(offset(altar,x,1.15f,0),{.85f,.15f,1.4f},.04f,stone,0);
        for(float dx:{-.15f,.15f}) m.box(offset(altar,x+dx,.71f,.607f),{.035f,.59f,.015f},stone*.6f,0);
    }
    m.block(offset(altar,0,1.35f,0),{4.75f,.30f,1.85f},.09f,stone*1.12f,0);
    // Recessed apron panels and restrained corner reliefs break up the pale slab.
    m.block(offset(altar,0,1.13f,.68f),{4.35f,.24f,.22f},.025f,stone*.78f,0);
    for(float x:{-1.55f,1.55f}) {
        m.block(offset(altar,x,1.12f,.802f),{1.10f,.15f,.025f},.012f,stone*.53f,0);
        for(float dx:{-.40f,0.0f,.40f}) {
            vec3 center=offset(altar,x+dx,1.12f,.824f);
            m.beam(center+vec3{-.065f,0,0},center+vec3{0,.055f,0},.012f,stone*.98f,0,5);
            m.beam(center+vec3{0,.055f,0},center+vec3{.065f,0,0},.012f,stone*.98f,0,5);
            m.beam(center+vec3{.065f,0,0},center+vec3{0,-.055f,0},.012f,stone*.98f,0,5);
            m.beam(center+vec3{0,-.055f,0},center+vec3{-.065f,0,0},.012f,stone*.98f,0,5);
        }
    }
    for(float x:{-2.15f,2.15f}) for(float z:{-.73f,.73f}) {
        m.block(offset(altar,x,1.507f,z),{.25f,.018f,.24f},.018f,stone*.73f,0);
        m.block(offset(altar,x,1.52f,z),{.12f,.018f,.12f},.008f,stone*.97f,0,pi*.25f);
    }

    for(int i=0;i<12;++i) {
        float a=-.76f+float(i)*1.52f/12,b=a+1.52f/12;
        float za=.94f+.018f*std::cos(float(i)*pi),zb=.94f+.018f*std::cos(float(i+1)*pi);
        m.quad(offset(altar,a,1.505f,-.76f),offset(altar,a,1.505f,za),offset(altar,b,1.505f,zb),offset(altar,b,1.505f,-.76f),cloth,5);
        m.quad(offset(altar,a,1.505f,za),offset(altar,a,.35f,za+.05f),offset(altar,b,.35f,zb+.05f),offset(altar,b,1.505f,zb),cloth*(.88f+.06f*float(i%3)),5);
    }
    m.beam(offset(altar,-.70f,.40f,1.01f),offset(altar,.70f,.40f,1.01f),.012f,brass,4,5);
    const vec3 bp=offset(altar,0,1.55f,.1f);
    m.block(bp,{1.1f,.08f,.72f},.02f,wood*.65f,2);
    for(int side:{-1,1}) {
        float x=float(side)*.27f;
        m.block(offset(bp,x,.065f,0),{.50f,.09f,.65f},.012f,bone,6);
        for(int l=0;l<8;++l) m.box(offset(bp,x,.113f,-.23f+float(l)*.06f),{.34f-.035f*float(l%3),.004f,.009f},iron,3);
    }
    for(int side:{-1,1}) for(int i=0;i<4;++i)
        candle(m,offset(altar,float(side)*(1.35f+.22f*float(i)),1.50f,-.26f+.20f*float(i%2)),.35f+.15f*float(i%3),.065f);
    scaleProp(m,altarStart,altar,{4.2f/4.75f,1,1});
    torches.push_back({offset(altar,-1.65f*4.2f/4.75f,2.13f,-.16f),.45f});
    torches.push_back({offset(altar,1.65f*4.2f/4.75f,2.13f,-.16f),.45f});
    for(auto footprint:{layout::statueLeft,layout::statueRight}) {
        const size_t start=m.vertices.size();
        const vec3 p=layout::center(footprint);
        statue(m,p);
        scaleProp(m,start,p,{1.3f/1.5f,1,1.3f/1.5f});
    }
    // The circular seal is old stone and tarnished brass, with a carved heraldic sword.
    size_t first=m.vertices.size();
    m.cylinder({0,0,0},1.47f,1.47f,.16f,stone*.8f,0,64);
    transform(m,first,glm::translate(glm::mat4(1),{0,3.4f,-10.7f})*glm::rotate(glm::mat4(1),pi*.5f,vec3{1,0,0}));
    ring(m,{0,3.4f,-10.59f},1.47f,.045f,brass,4);
    ring(m,{0,3.4f,-10.59f},1.32f,.018f,stone*.5f,0);
    m.beam({0,2.43f,-10.57f},{0,4.27f,-10.57f},.055f,stone*.45f,0,6);
    m.beam({-.48f,3.95f,-10.57f},{.48f,3.95f,-10.57f},.045f,stone*.45f,0,6);
    for(int side:{-1,1}) {
        float x=float(side);
        m.beam({0,2.9f,-10.57f},{x*.68f,3.4f,-10.57f},.035f,stone*.45f,0,6);
        m.beam({x*.68f,3.4f,-10.57f},{x*.78f,3.92f,-10.57f},.035f,stone*.45f,0,6);
    }
    for(auto footprint:{layout::brazier0,layout::brazier1,layout::brazier2,layout::brazier3})
        brazier(m,torches,layout::center(footprint),2.05f,1.0f);
    first=m.vertices.size();
    chest(m,layout::center(layout::chest));
    scaleProp(m,first,layout::center(layout::chest),{2.4f/1.9f,1,.85f});
    barrel(m,layout::center(layout::barrel0),1.04f);
    barrel(m,layout::center(layout::barrel1),1.04f);
    const vec3 cratePosition=layout::center(layout::crates);
    crate(m,cratePosition,{1.13f,.85f,.95f},0);
    crate(m,cratePosition+vec3{0,.89f,0},{.75f,.60f,.68f},0);
    urn(m,layout::center(layout::urn0),1.10f); urn(m,layout::center(layout::urn1),1.0f);
    book(m,layout::center(layout::books)+vec3{0,.01f,0},0,cloth);
    book(m,layout::center(layout::books)+vec3{0,.22f,0},0,wood*.7f);
    // Heavy stone sarcophagus along the west aisle.
    const vec3 tomb=layout::center(layout::tomb);
    const size_t tombStart=m.vertices.size();
    m.block(offset(tomb,0,.15f,0),{2.1f,.30f,3.3f},.09f,stone*.78f,0);
    m.block(offset(tomb,0,.65f,0),{1.85f,.75f,3.05f},.07f,stone*.91f,0);
    m.block(offset(tomb,0,1.11f,0),{2.05f,.22f,3.25f},.08f,stone*1.12f,0);
    m.block(offset(tomb,0,1.25f,-.1f),{1.55f,.14f,2.7f},.06f,stone,0);
    m.beam(offset(tomb,0,1.34f,-1.0f),offset(tomb,0,1.34f,.95f),.065f,stone*.6f,0,4);
    m.beam(offset(tomb,-.5f,1.34f,-.45f),offset(tomb,.5f,1.34f,-.45f),.065f,stone*.6f,0,4);
    for(float z:{-1.1f,0.0f,1.1f}) m.block(offset(tomb,1.0f,.64f,z),{.05f,.44f,.12f},.015f,stone*.68f,0);
    scaleProp(m,tombStart,tomb,{1.25f/2.1f,1.15f/1.405f,3.9f/3.3f});
    // Segmented wall banner uses the environment cloth material (shader animation).
    m.beam({-13.35f,5.26f,-4.3f},{-13.35f,5.26f,-2.7f},.045f,iron,3,8);
    for(int row=0;row<14;++row) for(int col=0;col<8;++col) {
        auto q=[&](int r,int c) {
            float u=float(c)/8,v=float(r)/14;
            return vec3{-13.26f+.045f*std::sin(u*6*pi),5.2f-v*2.8f,-4.2f+u*1.4f};
        };
        m.quad(q(row,col+1),q(row+1,col+1),q(row+1,col),q(row,col),cloth*(.92f+.035f*float(col%3)),11);
    }
}

void buildAnimatedHero(Mesh& m, float time, float gait, float speed, float cast)
{
    speed=std::clamp(speed,0.0f,1.0f); cast=std::clamp(cast,0.0f,1.0f);
    const auto pose=heroAnimation::sample(time,gait,speed);
    const vec3 steel{.32f,.38f,.40f}, leather{.13f,.09f,.065f};
    auto T=[](vec3 p){return glm::translate(glm::mat4(1),p);};
    auto R=[](float angle){return glm::rotate(glm::mat4(1),angle,vec3{1,0,0});};
    auto boneFrame=[&](vec3 from,vec3 to) {
        vec3 y=glm::normalize(from-to),x=glm::normalize(glm::cross(y,vec3{0,0,1}));
        glm::mat4 frame(1);
        frame[0]=glm::vec4(x,0);frame[1]=glm::vec4(y,0);
        frame[2]=glm::vec4(glm::cross(x,y),0);frame[3]=glm::vec4(from,1);
        return frame;
    };
    const glm::mat4 body=T(pose.pelvisOffset)*T({0,1.10f,0})*
        glm::rotate(glm::mat4(1),pose.torsoYaw,vec3{0,1,0})*
        glm::rotate(glm::mat4(1),pose.torsoRoll,vec3{0,0,1})*T({0,-1.10f,0});
    // The feet drive the skeleton. Both bones meet the exact ankle target.
    for(const auto& leg:pose.legs) {
        size_t first=m.vertices.size();
        m.beam({0,0,0},{0,-.39f,0},.095f,leather,2,10);
        m.block({0,-.18f,-.06f},{.16f,.29f,.105f},.028f,steel,3);
        transform(m,first,boneFrame(leg.hip,leg.knee));
        first=m.vertices.size();
        m.sphere({0,0,-.025f},{.105f,.105f,.105f},steel*1.12f,3,10,6);
        m.beam({0,-.035f,0},{0,-.40f,0},.075f,leather,2,10);
        m.block({0,-.20f,-.055f},{.13f,.29f,.10f},.025f,steel*.9f,3);
        transform(m,first,boneFrame(leg.knee,leg.ankle));
        first=m.vertices.size();
        m.block({0,-.11f,-.055f},{.19f,.22f,.31f},.035f,leather,2);
        m.block({0,-.075f,-.13f},{.18f,.12f,.15f},.025f,steel*.8f,3);
        transform(m,first,T(leg.ankle)*R(leg.pitch));
    }
    size_t first=m.vertices.size();
    const float breath=.006f*std::sin(time*2.1f)*(1-speed*.6f);
    m.sphere({0,1.12f,0},{.225f,.18f,.145f},leather,2,12,8);
    m.block({0,1.32f+breath,-.005f},{.44f,.41f,.27f},.065f,steel,3);
    m.block({0,1.10f,-.02f},{.45f,.065f,.30f},.018f,leather*.8f,2);
    m.block({0,1.10f,-.178f},{.095f,.07f,.035f},.012f,brass,4);
    m.beam({-.18f,1.49f,-.15f},{.11f,1.12f,-.16f},.028f,leather,2,6);
    transform(m,first,body);
    first=m.vertices.size();
    m.cylinder({0,1.60f+breath,0},.105f,.105f,.12f,iron,3,12);
    m.sphere({0,1.80f+breath,-.008f},{.165f,.205f,.16f},steel,3,16,10);
    m.block({0,1.77f+breath,-.15f},{.25f,.16f,.055f},.02f,steel*.86f,3);
    m.box({0,1.81f+breath,-.181f},{.205f,.018f,.008f},vec3{.025f},3);
    m.beam({0,1.99f,-.08f},{0,1.99f,.07f},.015f,steel*1.3f,3,6);
    transform(m,first,T(pose.headOffset));
    for(int side:{-1,1}) {
        float phase=gait+(side<0?pi:0);
        float swing=-std::cos(phase-.22f)*(side>0?.18f:.09f)*speed+(side>0?cast*1.9f:cast*.35f);
        glm::mat4 upper=body*T({float(side)*.29f,1.48f+breath,0})*R(swing);
        first=m.vertices.size();
        m.sphere({0,.015f,0},{.145f,.105f,.16f},steel*1.12f,3,12,8);
        m.beam({0,-.05f,0},{0,-.27f,0},.068f,leather,2,10);
        m.block({0,-.16f,-.035f},{.115f,.17f,.12f},.025f,steel,3);
        transform(m,first,upper);
        glm::mat4 lower=upper*T({0,-.27f,0})*R((side>0?.22f:.40f)+.04f*speed*std::sin(phase-.5f)+cast*.4f);
        first=m.vertices.size();
        m.sphere({0,0,0},vec3{.075f},steel*.8f,3,10,6);
        m.beam({0,-.015f,0},{0,-.25f,0},.065f,steel*.85f,3,10);
        m.sphere({0,-.28f,0},{.065f,.085f,.07f},leather,2,10,6);
        transform(m,first,lower);
        glm::mat4 hand=lower*T({0,-.28f,0});
        first=m.vertices.size();
        if(side>0) {
            m.beam({0,.09f,0},{0,-.10f,0},.026f,leather,2,8);
            m.beam({-.12f,-.11f,0},{.12f,-.11f,0},.018f,brass,4,8);
            vec3 a{-.035f,-.14f,0},b{0,-.14f,.018f},c{.035f,-.14f,0},d{0,-.14f,-.018f},tip{0,-.80f,-.035f};
            m.triangle(a,b,tip,steel*1.55f,3); m.triangle(b,c,tip,steel*1.15f,3);
            m.triangle(c,d,tip,steel*1.35f,3); m.triangle(d,a,tip,steel,3);
        } else {
            vec3 corners[]={{-.19f,.28f,-.08f},{.19f,.28f,-.08f},{.21f,-.03f,-.08f},{0,-.33f,-.08f},{-.21f,-.03f,-.08f}};
            for(int i=0;i<5;++i) {
                m.triangle({0,.04f,-.13f},corners[i],corners[(i+1)%5],cloth*.7f,5);
                m.triangle({0,.04f,-.065f},corners[(i+1)%5],corners[i],wood*.7f,2);
                m.beam(corners[i],corners[(i+1)%5],.018f,steel,3,6);
            }
            m.beam({0,.21f,-.145f},{0,-.20f,-.145f},.015f,brass,4,6);
        }
        transform(m,first,hand);
    }
    first=m.vertices.size();
    auto cape=[&](int row,int col) {
        float v=float(row)/12,u=float(col)/10-.5f;
        float flutter=.008f*v*v*std::sin(time*2.5f-u*5+v*4)
            +.045f*speed*v*v*std::sin(gait-.8f-v*1.4f-u*2);
        return vec3{u*(.44f+.32f*v)+.045f*speed*v*v*std::sin(gait-.6f-v),
                    1.51f-v*1.21f+breath+.015f*v*std::cos(u*8*pi),
                    .16f+.18f*v+.025f*std::sin(u*8*pi)*v+flutter};
    };
    for(int row=0;row<12;++row) for(int col=0;col<10;++col) {
        vec3 a=cape(row,col),b=cape(row+1,col),c=cape(row+1,col+1),d=cape(row,col+1);
        vec3 color=cloth*(.88f+.035f*float(col%4));
        m.quad(a,b,c,d,color,5); m.quad(d,c,b,a,color*.85f,5);
    }
    transform(m,first,body);

}

void buildHero(Mesh& m) { buildAnimatedHero(m,0,0,0,0); }
