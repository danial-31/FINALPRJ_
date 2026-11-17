#include "zone_mesh.hpp"
#include <cmath>

using glm::vec3;
using world::Cell;
using world::Tile;
using rpg::EntityType;

namespace {
float hash(int x,int y,int k=0){
    std::uint32_t h=std::uint32_t(x)*374761393u+std::uint32_t(y)*668265263u+std::uint32_t(k)*2246822519u;
    h=(h^(h>>13))*1274126177u;h^=h>>16;return float(h&0xffffff)/float(0xffffff);
}
struct Palette { vec3 ground, path, rock, wall, accent; bool surface; float wallHeight; };
Palette palette(int biome){
    switch(biome){
    case 0:return {{.36f,.52f,.20f},{.60f,.48f,.31f},{.42f,.40f,.37f},{.44f,.42f,.38f},{.85f,.55f,.25f},true,0};
    case 1:return {{.42f,.50f,.22f},{.60f,.49f,.32f},{.45f,.42f,.37f},{.46f,.43f,.38f},{.8f,.5f,.2f},true,0};
    case 2:return {{.24f,.40f,.22f},{.46f,.39f,.27f},{.33f,.33f,.32f},{.35f,.35f,.33f},{.75f,.85f,.8f},true,0};
    case 3:return {{.42f,.40f,.28f},{.54f,.45f,.32f},{.40f,.36f,.32f},{.41f,.37f,.33f},{.9f,.5f,.2f},true,0};
    case 4:return {{.28f,.31f,.24f},{.40f,.35f,.27f},{.27f,.27f,.30f},{.30f,.30f,.33f},{.8f,.85f,.9f},true,0};
    case 5:return {{.36f,.35f,.34f},{.44f,.40f,.34f},{.42f,.41f,.42f},{.45f,.44f,.45f},{.7f,.3f,.8f},true,0};
    case 10:return {{.26f,.21f,.22f},{.30f,.24f,.24f},{.30f,.24f,.26f},{.31f,.25f,.27f},{.5f,.9f,.3f},false,2.6f};
    case 11:return {{.25f,.28f,.30f},{.30f,.31f,.31f},{.29f,.32f,.34f},{.30f,.33f,.35f},{.3f,.7f,.9f},false,2.6f};
    case 12:return {{.33f,.33f,.36f},{.36f,.36f,.38f},{.36f,.36f,.40f},{.38f,.38f,.42f},{.5f,.7f,1.f},false,3.0f};
    default:return {{.30f,.21f,.21f},{.34f,.24f,.22f},{.33f,.24f,.24f},{.36f,.26f,.26f},{1.f,.25f,.15f},false,3.0f};
    }
}
struct Builder {
    const rpg::Campaign& c;
    const world::Map& map;
    ZoneVisuals& out;
    Palette pal;
    float s=world::Map::cellSize;
    std::vector<float> corner;int cornerW=0;
    float groundAt(int x,int y)const{if(corner.empty())return 0;auto h=[&](int cx,int cy){return corner[size_t(cy*(cornerW+1)+cx)];};return (h(x,y)+h(x+1,y)+h(x,y+1)+h(x+1,y+1))*.25f;}
    vec3 at(int x,int y,float h=0)const{auto p=map.center({x,y});p.y=h;return p;}
    bool wallish(int x,int y)const{Tile t=map.at(x,y);char g=map.glyph(x,y);return t==Tile::Wall&&g!='~';}
    bool walkable(int x,int y)const{return map.inside(x,y)&&!map.blocked(x,y);}
    bool nearWalkable(int x,int y)const{for(int dy=-1;dy<=1;++dy)for(int dx=-1;dx<=1;++dx)if(map.inside(x+dx,y+dy)&&!wallish(x+dx,y+dy))return true;return false;}
    void light(vec3 p,vec3 color,float radius,bool shadow=false){out.lights.push_back({p,color,radius,shadow});}
    void emit(vec3 p,int kind,float scale=1){out.emitters.push_back({p,kind,scale});}

    void ground(int x,int y){
        vec3 p=at(x,y);float r=hash(x,y,1),r2=hash(x,y,2);char g=map.glyph(x,y);
        bool path=g==','||map.at(x,y)==Tile::Path;
        if(pal.surface&&pal.wallHeight==0&&c.zone().biome!=5){
            vec3 col=path?pal.path*(0.95f+r*.08f):pal.ground*(0.95f+r*.09f);
            if(!path&&r2>.88f)col=glm::mix(col,vec3(.42f,.40f,.20f),.18f);
            if(c.zone().biome==4&&r2>.8f&&!path)col=glm::mix(col,vec3(.78f,.80f,.84f),.5f);   // snow patches on the pass
            out.solid.block(p+vec3(0,-.06f,0),vec3(s,.12f,s),0,col,mat::Ground);
            if(!path&&map.at(x,y)==Tile::Floor&&r>.55f)grassTufts(x,y);
        }else{
            // Flagstones: slightly uneven bevelled slabs, as in the original crypt.
            vec3 col=(path?pal.path:pal.ground)*(0.84f+r*.26f);
            float h=.10f+r2*.03f;
            out.solid.block(p+vec3((r-.5f)*.03f,-.10f+h*.5f,(r2-.5f)*.03f),vec3(s*.96f,h,s*.96f),.035f,col,mat::Stone);
        }
    }
    void grassTufts(int x,int y){
        vec3 base=at(x,y);int n=2+int(hash(x,y,5)*3);
        for(int i=0;i<n;++i){
            vec3 p=base+vec3((hash(x,y,10+i)-.5f)*s*.8f,0,(hash(x,y,20+i)-.5f)*s*.8f);
            float h=.18f+hash(x,y,30+i)*.2f;vec3 col=pal.ground*1.25f;
            for(int b=0;b<3;++b){float a=float(b)*2.1f+hash(x,y,40+i)*3;
                vec3 tip=p+vec3(std::cos(a)*.08f,h,std::sin(a)*.08f);
                out.mesh.triangle(p+vec3(-.03f,0,0),p+vec3(.03f,0,0),tip,col,mat::Foliage);}
        }
    }
    void flowers(int x,int y){
        vec3 base=at(x,y);vec3 colors[]={{.9f,.8f,.3f},{.8f,.35f,.45f},{.6f,.5f,.9f},{.95f,.95f,.9f}};
        for(int i=0;i<6;++i){vec3 p=base+vec3((hash(x,y,50+i)-.5f)*s*.85f,.12f,(hash(x,y,60+i)-.5f)*s*.85f);
            out.mesh.beam(p-vec3(0,.12f,0),p,.012f,{.2f,.35f,.12f},mat::Foliage,4);
            out.mesh.sphere(p,vec3(.05f),colors[int(hash(x,y,70+i)*4)%4],mat::Foliage,6,4);}
    }
    void broadleafCrown(vec3 p,vec3 size,vec3 pigment,int seed,bool flowering=false){
        // Skew the shoulder and the cap separately so adjacent crowns interlock.
        constexpr int sides=9;
        const float heights[]={-.81f,-.39f,.20f,.65f}, radii[]={.43f,.88f,1.f,.70f};
        vec3 rings[4][sides];
        for(int row=0;row<4;++row)for(int i=0;i<sides;++i){
            float a=2*pi*float(i)/sides+.13f*float(row)+.31f*float(seed);
            float r=radii[row]*(.87f+.23f*hash(i,row,seed));
            float h=heights[row]+(hash(row,i,seed)-.5f)*.22f;
            rings[row][i]=p+size*vec3(r*std::cos(a)+.06f*row,h,r*std::sin(a));
        }
        size_t first=out.mesh.vertices.size();
        const float values[]={.72f,.86f,1.01f};
        for(int i=0;i<sides;++i){
            int next=(i+1)%sides;
            float tint=.96f+.08f*hash(i,7,seed);
            for(int row=0;row<3;++row)
                out.mesh.quad(rings[row][i],rings[row+1][i],rings[row+1][next],rings[row][next],pigment*values[row]*tint,mat::Foliage);
            out.mesh.triangle(rings[3][i],p+size*vec3(.15f,.91f,-.07f),rings[3][next],pigment*1.06f,mat::Foliage);
            out.mesh.triangle(rings[0][next],p-size*vec3(.04f,.96f,0),rings[0][i],pigment*.69f,mat::Foliage);
        }
        for(size_t k=first;k<out.mesh.vertices.size();++k){
            auto& v=out.mesh.vertices[k];
            vec3 radial=glm::normalize((v.position-p)/(size*size));
            v.normal=glm::normalize(glm::mix(v.normal,radial,.58f));
        }
        if(!flowering)return;
        for(int i=seed%3;i<sides;i+=3){
            vec3 a=rings[2][i], up=rings[3][i]-a, across=rings[3][(i+1)%sides]-rings[3][i];
            vec3 lift=.007f*glm::normalize(glm::cross(up,across));
            const vec3 petal=glm::mix(pigment,vec3(.96f,.81f,.72f),.52f);
            for(int bud=0;bud<3;++bud){
                vec3 q=a+lift+up*(.46f+.14f*bud)+across*(bud==1?.31f:.16f);
                out.mesh.quad(q-across*.10f,q+up*.11f,q+across*.09f,q-up*.07f,petal,mat::Foliage);
            }
        }
    }
    void pineBoughs(vec3 p,float scale,vec3 needles,int seed,bool snowy){
        constexpr int tips=9;
        for(int tier=0;tier<3;++tier){
            float radius=(1.04f-.25f*tier)*scale, height=(1.22f+.73f*tier)*scale;
            vec3 edge[tips], ridge[tips];
            for(int i=0;i<tips;++i){
                float a=2*pi*float(i)/tips+.39f*float(tier)+float(seed)*.2f;
                vec3 ray(std::cos(a),0,std::sin(a));
                edge[i]=p+ray*radius*(.89f+.16f*hash(i,tier,seed))+vec3(0,height-(.11f+.17f*hash(tier,i,seed))*scale,0);
                ridge[i]=p+ray*radius*.58f+vec3(0,height+(.42f+.15f*hash(i,9,seed))*scale,0);
            }
            vec3 crown=p+vec3(.04f*scale,height+1.01f*scale,-.02f*scale);
            for(int i=0;i<tips;++i){
                int j=(i+1)%tips;
                vec3 notch=glm::mix(p,(edge[i]+edge[j])*.5f,.69f);
                notch.y=height+p.y+.055f*scale;
                vec3 hue=needles*(.88f+.08f*float(tier)+.10f*hash(i,1,seed));
                out.mesh.triangle(edge[i],ridge[i],notch,hue,mat::Foliage);
                out.mesh.triangle(notch,ridge[j],edge[j],hue*.83f,mat::Foliage);
                out.mesh.triangle(notch,ridge[i],ridge[j],hue*1.03f,mat::Foliage);
                bool frost=snowy&&(tier==2||hash(i,tier,seed)>.67f);
                out.mesh.triangle(ridge[i],crown,ridge[j],frost?vec3(.76f,.82f,.82f):hue*1.10f,frost?mat::Snow:mat::Foliage);
            }
        }
    }
    void tree(int x,int y){
        vec3 p=at(x,y,groundAt(x,y)-.05f);float r=hash(x,y,3),r2=hash(x,y,4);
        p+=vec3((r-.5f)*.4f,0,(r2-.5f)*.4f);
        int biome=c.zone().biome;
        if((biome==0||biome==1||biome==2)&&hash(x,y,77)>.86f){blossomTree(p,.85f+r2*.4f,hash(x,y,78)>.25f?vec3(.72f,.40f,.46f):vec3(.78f,.69f,.59f),x+7*y);return;}
        bool pine=c.zone().biome==4||c.zone().biome==2?r>.25f:r>.6f;
        float scale=.85f+r2*.5f;
        vec3 bark{.25f,.17f,.11f};
        if(c.zone().biome==2)bark={.16f,.13f,.12f};
        out.mesh.cylinder(p+vec3(0,.9f*scale,0),.16f*scale,.11f*scale,1.8f*scale,bark,mat::Wood,7);
        if(pine){
            vec3 green=c.zone().biome==2?vec3(.14f,.30f,.20f):vec3(.16f,.36f,.21f);
            pineBoughs(p,scale,green,x+7*y,c.zone().biome==4);
        }else{
            vec3 green=c.zone().biome==1?vec3(.42f,.50f,.18f):vec3(.26f,.48f,.20f);
            out.mesh.beam(p+vec3(0,1.12f,0)*scale,p+vec3(-.47f,2.24f,-.17f)*scale,.08f*scale,bark,mat::Wood,6);
            broadleafCrown(p+vec3(-.28f,2.49f,-.17f)*scale,vec3(.83f,.79f,.84f)*scale,green*.98f,x+y);
            broadleafCrown(p+vec3(.39f,2.13f,.22f)*scale,vec3(.72f,.65f,.74f)*scale,green*1.04f,x+2*y+8);
        }
        if(c.zone().biome==2&&r>.7f)web(x,y,1.2f);
    }
    void deadTree(int x,int y){
        vec3 p=at(x,y);vec3 bark{.20f,.16f,.14f};
        out.mesh.cylinder(p+vec3(0,1.1f,0),.18f,.10f,2.2f,bark,mat::Wood,7);
        for(int k=0;k<4;++k){float a=hash(x,y,80+k)*6.28f,h=1.2f+hash(x,y,90+k);
            out.mesh.beam(p+vec3(0,h,0),p+vec3(std::cos(a)*.9f,h+.6f,std::sin(a)*.9f),.05f,bark,mat::Wood,5);}
        web(x,y,1.0f);
    }
    void web(int x,int y,float height){
        vec3 p=at(x,y,height);vec3 white{.82f,.84f,.86f};
        for(int k=0;k<6;++k){float a=float(k)*1.047f+hash(x,y,7)*.5f;
            out.mesh.beam(p,p+vec3(std::cos(a)*.7f,-height*.85f+hash(x,y,100+k)*.3f,std::sin(a)*.7f),.008f,white,mat::Plaster,3);}
        for(float r:{.25f,.45f})for(int k=0;k<6;++k){float a=float(k)*1.047f,b=float(k+1)*1.047f;
            out.mesh.beam(p+vec3(std::cos(a)*r,-r*height,std::sin(a)*r),p+vec3(std::cos(b)*r,-r*height,std::sin(b)*r),.006f,white,mat::Plaster,3);}
    }
    void floorWeb(int x,int y){
        vec3 p=at(x,y,.04f);vec3 white{.78f,.80f,.82f};
        for(int k=0;k<8;++k){float a=float(k)*.785f;out.mesh.beam(p,p+vec3(std::cos(a)*.66f,0,std::sin(a)*.66f),.01f,white,mat::Plaster,3);}
        for(float r:{.2f,.4f,.6f})for(int k=0;k<8;++k){float a=float(k)*.785f,b=float(k+1)*.785f;
            out.mesh.beam(p+vec3(std::cos(a)*r,0,std::sin(a)*r),p+vec3(std::cos(b)*r,0,std::sin(b)*r),.008f,white,mat::Plaster,3);}
    }
    void rock(int x,int y,float big){
        vec3 p=at(x,y);
        for(int k=0;k<3;++k){
            vec3 size=vec3(.6f+hash(x,y,110+k)*.6f,.5f+hash(x,y,120+k)*.7f*big,.6f+hash(x,y,130+k)*.5f)*big;
            vec3 o((hash(x,y,140+k)-.5f)*.6f,size.y*.5f-.05f,(hash(x,y,150+k)-.5f)*.6f);
            out.solid.block(p+o,size,.12f,pal.rock*(0.85f+hash(x,y,160+k)*.25f),mat::Stone,hash(x,y,170+k)*3);
        }
    }
    void cliff(int x,int y){
        if(!nearWalkable(x,y)){ // fill the mass with a low cap so the void reads as rock
            vec3 p=at(x,y);float h=pal.surface?2.2f+hash(x,y,9)*1.8f:pal.wallHeight;
            out.solid.block(p+vec3(0,h*.5f,0),vec3(s,h,s),0,pal.rock*.55f,mat::Stone);return;
        }
        if(pal.surface){
            vec3 p=at(x,y);float h=1.6f+hash(x,y,11)*2.4f;
            out.solid.block(p+vec3(0,h*.5f-.1f,0),vec3(s*1.02f,h,s*1.02f),.18f,pal.rock*(0.8f+hash(x,y,12)*.3f),mat::Stone,(hash(x,y,13)-.5f)*.3f);
            if(hash(x,y,14)>.5f)out.solid.block(p+vec3((hash(x,y,15)-.5f)*.4f,h+.25f,(hash(x,y,16)-.5f)*.4f),vec3(.8f,.6f,.7f),.15f,pal.rock*.9f,mat::Stone,hash(x,y,17));
            if(c.zone().biome==4&&hash(x,y,18)>.45f)out.solid.block(p+vec3(0,h+.02f,0),vec3(s*.8f,.08f,s*.8f),.04f,{.80f,.82f,.86f},mat::Snow);
        }else{
            // Masonry courses with bevelled blocks.
            vec3 p=at(x,y);float h=pal.wallHeight;int courses=int(h/.55f);
            for(int k=0;k<courses;++k){float off=(k%2)*.35f-.17f;
                for(int b=0;b<2;++b){vec3 col=pal.wall*(0.82f+hash(x*3+b,y,200+k)*.3f);
                    out.solid.block(p+vec3((float(b)-.5f)*s*.5f+off*.2f,.275f+float(k)*.55f,0),vec3(s*.5f,.53f,s),.05f,col,mat::Masonry);}}
            out.solid.block(p+vec3(0,h+.08f,0),vec3(s*1.04f,.16f,s*1.04f),.04f,pal.wall*.75f,mat::Masonry);
        }
    }
    void castleWall(int x,int y){
        vec3 p=at(x,y);float h=4.2f;bool edge=nearWalkable(x,y);
        if(!edge){out.solid.block(p+vec3(0,h*.5f,0),vec3(s,h,s),0,pal.wall*.6f,mat::Masonry);return;}
        for(int k=0;k<7;++k){vec3 col=pal.wall*(0.8f+hash(x,y,300+k)*.3f);
            out.solid.block(p+vec3((k%2)*.1f-.05f,.3f+float(k)*.6f,0),vec3(s,.58f,s),.06f,col,mat::Masonry);}
        if(hash(x,y,310)>.0f)out.solid.block(p+vec3(0,h+.35f,0),vec3(s*.5f,.7f,s*.5f),.05f,pal.wall*.85f,mat::Masonry);
    }
    void house(int x0,int y0,int x1,int y1){
        vec3 a=map.center({x0,y0}),b=map.center({x1,y1});
        vec3 center=(a+b)*.5f;float w=(b.x-a.x)+s*.9f,d=(b.z-a.z)+s*.9f,h=2.2f;
        bool corrupt=c.seals()<3&&!c.flags.count("boss_keeper");
        vec3 plaster=vec3(.90f,.84f,.72f)*(0.92f+hash(x0,y0,1)*.1f);
        out.solid.block(center+vec3(0,.25f,0),vec3(w+.15f,.5f,d+.15f),.06f,pal.rock*1.1f,mat::Stone);
        // ground floor, then an upper floor that overhangs slightly (half-timbered)
        float lower=h*.5f;
        out.solid.block(center+vec3(0,.5f+lower*.5f,0),vec3(w,lower,d),.04f,plaster,mat::Plaster);
        out.solid.block(center+vec3(0,.5f+lower+(h-lower)*.5f,0),vec3(w+.3f,h-lower,d+.3f),.04f,plaster*1.03f,mat::Plaster);
        vec3 timber{.30f,.19f,.11f};
        float ow=w+.3f,od=d+.3f;
        for(float sx:{-1.f,1.f})for(float sz:{-1.f,1.f}){
            out.solid.block(center+vec3(sx*w*.5f,.5f+lower*.5f,sz*d*.5f),vec3(.16f,lower,.16f),.02f,timber,mat::Wood);
            out.solid.block(center+vec3(sx*ow*.5f,.5f+lower+(h-lower)*.5f,sz*od*.5f),vec3(.16f,h-lower,.16f),.02f,timber,mat::Wood);}
        for(float sz:{-1.f,1.f}){
            out.solid.block(center+vec3(0,.5f+lower,sz*(od*.5f)),vec3(ow,.14f,.14f),.02f,timber,mat::Wood);
            out.solid.block(center+vec3(0,.5f+h,sz*od*.5f),vec3(ow,.14f,.14f),.02f,timber,mat::Wood);
            for(float t:{-.33f,.33f})out.solid.block(center+vec3(t*ow,.5f+lower+(h-lower)*.5f,sz*od*.5f),vec3(.12f,h-lower,.12f),.01f,timber,mat::Wood);
            // diagonal braces on the upper floor
            for(float t:{-1.f,1.f}){vec3 a0=center+vec3(t*ow*.5f,.5f+lower+.05f,sz*(od*.5f+.02f)),a1=center+vec3(t*ow*.17f,.5f+h-.05f,sz*(od*.5f+.02f));
                out.mesh.beam(a0,a1,.05f,timber,mat::Wood,4);}
        }
        // gabled roof along x
        float ry=.5f+h,peak=ry+1.5f;
        const vec3 roofs[]={{.66f,.30f,.18f},{.36f,.40f,.50f},{.66f,.54f,.32f},{.55f,.24f,.16f}};
        vec3 roof=roofs[int(hash(x0,y0,2)*3.99f)];
        w=ow;d=od;
        vec3 p0=center+vec3(-w*.5f-.3f,ry,-d*.5f-.3f),p1=center+vec3(w*.5f+.3f,ry,-d*.5f-.3f),
             p2=center+vec3(w*.5f+.3f,ry,d*.5f+.3f),p3=center+vec3(-w*.5f-.3f,ry,d*.5f+.3f),
             r0=center+vec3(-w*.5f-.3f,peak,0),r1=center+vec3(w*.5f+.3f,peak,0);
        out.mesh.quad(p0,r0,r1,p1,roof,mat::Wood);out.mesh.quad(p3,p2,r1,r0,roof*.92f,mat::Wood);
        out.mesh.triangle(center+vec3(-w*.5f,ry,-d*.5f),center+vec3(-w*.5f,ry,d*.5f),center+vec3(-w*.5f,peak-.1f,0),plaster*.95f,mat::Plaster);
        out.mesh.triangle(center+vec3(w*.5f,ry,d*.5f),center+vec3(w*.5f,ry,-d*.5f),center+vec3(w*.5f,peak-.1f,0),plaster*.95f,mat::Plaster);
        // chimney with smoke
        vec3 chimney=center+vec3(w*.25f,peak-.2f,-d*.18f);
        out.solid.block(chimney,vec3(.42f,1.3f,.42f),.04f,pal.rock*.8f,mat::Stone);
        emit(chimney+vec3(0,.75f,0),1,1);
        // windows glow warm; door faces +z (towards the camera side)
        vec3 warm=corrupt?vec3(.9f,.45f,.15f):vec3(1.f,.65f,.25f);
        for(float sx:{-.28f,.28f}){vec3 wp=center+vec3(sx*w,.5f+h*.55f,d*.5f+.02f);
            out.mesh.quad(wp+vec3(-.22f,-.22f,0),wp+vec3(.22f,-.22f,0),wp+vec3(.22f,.22f,0),wp+vec3(-.22f,.22f,0),warm*.5f,mat::Ember);}
        vec3 door=center+vec3(0,.5f+.7f,d*.5f+.03f);
        out.mesh.quad(door+vec3(-.3f,-.7f,0),door+vec3(.3f,-.7f,0),door+vec3(.3f,.7f,0),door+vec3(-.3f,.7f,0),timber*1.3f,mat::Wood);
        light(center+vec3(0,1.6f,d*.5f+.8f),warm*1.2f,4.5f);
    }
    void cart(int x,int y){
        const vec3 p=at(x,y,groundAt(x,y));
        const float yaw=(hash(x,y,1)-.5f)*.40f, co=std::cos(yaw), si=std::sin(yaw);
        const vec3 oak{.44f,.28f,.17f}, iron{.19f,.23f,.24f}, copper{.58f,.41f,.23f};
        auto point=[&](float lx,float h,float lz){return p+vec3(co*lx+si*lz,h,-si*lx+co*lz);};
        for(int row=0;row<4;++row)
            out.solid.block(point(-.14f,.44f,(float(row)-1.5f)*.16f),{.85f,.10f,.154f},.014f,oak*(.9f+.05f*row),mat::Wood,yaw);
        for(float side:{-1.f,1.f}){
            for(int row=0;row<2;++row)
                out.solid.block(point(-.14f,.56f+.14f*row,side*.32f),{.88f,.13f,.065f},.014f,oak*(.86f+.14f*row),mat::Wood,yaw);
            for(float post:{-.52f,.23f})
                out.solid.block(point(post,.61f,side*.354f),{.064f,.38f,.046f},.009f,iron,mat::Steel,yaw);
            out.mesh.beam(point(.18f,.43f,side*.20f),point(.59f,.12f,side*.23f),.037f,oak*.8f,mat::Wood,6);
            const float wheelX=-.19f, wheelY=.32f, z=side*.425f;
            auto rim=[&](float angle,float radius,float depth){return point(wheelX+std::cos(angle)*radius,wheelY+std::sin(angle)*radius,z+side*depth);};
            for(int i=0;i<12;++i){
                float a=float(i)*pi/6, b=float(i+1)*pi/6;
                out.mesh.quad(rim(a,.238f,.036f),rim(a,.32f,.036f),rim(b,.32f,.036f),rim(b,.238f,.036f),oak*1.08f,mat::Wood);
                out.mesh.quad(rim(a,.238f,-.036f),rim(b,.238f,-.036f),rim(b,.32f,-.036f),rim(a,.32f,-.036f),oak*.79f,mat::Wood);
                out.mesh.quad(rim(a,.238f,-.036f),rim(a,.238f,.036f),rim(b,.238f,.036f),rim(b,.238f,-.036f),oak*.75f,mat::Wood);
                out.mesh.quad(rim(a,.32f,.036f),rim(a,.32f,-.036f),rim(b,.32f,-.036f),rim(b,.32f,.036f),iron,mat::Steel);
                if(i%2==0)out.mesh.beam(point(wheelX,wheelY,z),rim(a,.247f,0),.027f,oak,mat::Wood,5);
            }
            out.mesh.beam(point(wheelX,wheelY,z-side*.05f),point(wheelX,wheelY,z+side*.066f),.071f,copper,mat::Brass,8);
        }
        out.solid.block(point(-.55f,.6f,0),{.065f,.30f,.62f},.018f,oak*.9f,mat::Wood,yaw);
    }
    void crates(int x,int y){
        vec3 p=at(x,y);vec3 wood{.36f,.25f,.15f};
        if(hash(x,y,1)>.5f){
            for(int k=0;k<2;++k){vec3 q=p+vec3(k?.3f:-.25f,.35f,k?.15f:-.2f);
                out.mesh.cylinder(q,.32f,.32f,.7f,wood*.9f,mat::Wood,10);
                out.mesh.cylinder(q+vec3(0,.22f,0),.335f,.335f,.06f,{.18f,.19f,.2f},mat::Steel,10);}
        }else{
            out.solid.block(p+vec3(-.15f,.35f,0),vec3(.7f,.7f,.7f),.04f,wood,mat::Wood,.2f);
            out.solid.block(p+vec3(.25f,.3f,.3f),vec3(.55f,.6f,.55f),.04f,wood*.9f,mat::Wood,-.3f);
            out.solid.block(p+vec3(-.05f,.95f,.05f),vec3(.5f,.5f,.5f),.04f,wood*1.05f,mat::Wood,.6f);
        }
    }
    void fence(int x,int y){
        vec3 p=at(x,y);vec3 wood{.35f,.26f,.16f};
        for(float o:{-.55f,0.f,.55f})out.solid.block(p+vec3(o,.4f,0),vec3(.1f,.8f,.1f),.02f,wood,mat::Wood);
        out.solid.block(p+vec3(0,.6f,0),vec3(s,.08f,.06f),.01f,wood*.9f,mat::Wood);
        out.solid.block(p+vec3(0,.3f,0),vec3(s,.08f,.06f),.01f,wood*.9f,mat::Wood);
    }
    void brazier(int x,int y){
        vec3 p=at(x,y);
        out.solid.block(p+vec3(0,.1f,0),vec3(.62f,.2f,.62f),.05f,pal.rock*.8f,mat::Stone);
        out.mesh.cylinder(p+vec3(0,.6f,0),.08f,.1f,.9f,{.15f,.16f,.17f},mat::Steel,8);
        out.mesh.cylinder(p+vec3(0,1.12f,0),.12f,.3f,.22f,{.15f,.16f,.17f},mat::Steel,10);
        out.mesh.sphere(p+vec3(0,1.25f,0),{.2f,.12f,.2f},{1.f,.45f,.08f},mat::Ember,8,5);
        Torch t{p+vec3(0,1.3f,0),1.f};out.torches.push_back(t);
        emit(p+vec3(0,1.3f,0),0,1);light(p+vec3(0,1.6f,0),vec3(5.f,1.85f,.4f),7.f,true);
    }
    void wallTorch(vec3 p,vec3 n){
        out.mesh.beam(p,p+n*.25f+vec3(0,.2f,0),.035f,{.2f,.15f,.1f},mat::Wood,5);
        vec3 f=p+n*.3f+vec3(0,.32f,0);out.mesh.sphere(f,{.09f,.13f,.09f},{1.f,.5f,.1f},mat::Ember,6,4);
        out.torches.push_back({f,.6f});emit(f,0,.6f);light(f+n*.3f,vec3(4.f,1.5f,.35f),6.f);
    }
    void pillar(int x,int y){
        vec3 p=at(x,y);
        out.solid.block(p+vec3(0,.15f,0),vec3(.9f,.3f,.9f),.05f,pal.wall*.9f,mat::Masonry);
        out.mesh.cylinder(p+vec3(0,1.6f,0),.32f,.28f,2.7f,pal.wall,mat::Masonry,12);
        out.solid.block(p+vec3(0,3.0f,0),vec3(.85f,.25f,.85f),.05f,pal.wall*.95f,mat::Masonry);
        if(!pal.surface&&c.zone().biome==12)out.solid.block(p+vec3(0,3.3f,0),vec3(.5f,.4f,.5f),.05f,pal.wall,mat::Masonry);
        if(pal.surface&&hash(x,y,1)>.4f)out.solid.block(p+vec3(.3f,.2f,.5f),vec3(.6f,.35f,.5f),.08f,pal.wall*.8f,mat::Masonry,.5f);
    }
    void stalagmite(int x,int y){
        vec3 p=at(x,y);
        for(int k=0;k<3;++k){float h=1.f+hash(x,y,k)*1.6f;vec3 o((hash(x,y,10+k)-.5f)*.7f,h*.5f,(hash(x,y,20+k)-.5f)*.7f);
            out.mesh.cylinder(p+o,.3f-.05f*float(k),.03f,h,pal.rock*(0.9f+.1f*float(k)),mat::Stone,7);}
    }
    void bones(int x,int y){
        vec3 p=at(x,y,.06f);vec3 bone{.72f,.68f,.55f};
        for(int k=0;k<4;++k){float a=hash(x,y,k)*6.28f;vec3 o((hash(x,y,10+k)-.5f)*.9f,0,(hash(x,y,20+k)-.5f)*.9f);
            out.mesh.beam(p+o,p+o+vec3(std::cos(a)*.35f,.02f,std::sin(a)*.35f),.03f,bone,mat::Plaster,5);}
        if(hash(x,y,5)>.5f)out.mesh.sphere(p+vec3(.2f,.08f,-.1f),{.13f,.12f,.15f},bone,mat::Plaster,8,6);
    }
    void grave(int x,int y){
        vec3 p=at(x,y);vec3 col=pal.wall*(0.9f+hash(x,y,1)*.2f);
        out.solid.block(p+vec3(0,.2f,0),vec3(1.f,.4f,1.25f),.06f,col*.85f,mat::Stone);
        out.solid.block(p+vec3(0,.5f,-.05f),vec3(.85f,.25f,1.1f),.08f,col,mat::Stone);
    }
    void bush(int x,int y){vec3 p=at(x,y);vec3 g=pal.ground*1.15f;
        for(int k=0;k<3;++k)out.mesh.sphere(p+vec3((hash(x,y,k)-.5f)*.7f,.25f,(hash(x,y,5+k)-.5f)*.7f),vec3(.35f,.28f,.35f),g*(0.9f+.1f*float(k)),mat::Foliage,8,5);}
    void well(int x,int y){
        vec3 p=at(x,y);
        for(int k=0;k<10;++k){float a=float(k)*.628f;out.solid.block(p+vec3(std::cos(a)*.52f,.3f,std::sin(a)*.52f),vec3(.32f,.6f,.22f),.04f,pal.rock*(0.85f+hash(x,y,k)*.2f),mat::Stone,-a);}
        out.mesh.cylinder(p+vec3(0,.45f,0),.42f,.42f,.02f,{.05f,.12f,.16f},mat::Water,12);
        for(float sx:{-.6f,.6f})out.solid.block(p+vec3(sx,1.0f,0),vec3(.1f,1.4f,.1f),.02f,{.3f,.2f,.12f},mat::Wood);
        out.mesh.quad(p+vec3(-.8f,1.6f,-.5f),p+vec3(-.8f,2.f,0),p+vec3(.8f,2.f,0),p+vec3(.8f,1.6f,-.5f),{.40f,.22f,.13f},mat::Wood);
        out.mesh.quad(p+vec3(-.8f,1.6f,.5f),p+vec3(.8f,1.6f,.5f),p+vec3(.8f,2.f,0),p+vec3(-.8f,2.f,0),{.38f,.21f,.12f},mat::Wood);
    }
    void banner(int x,int y){
        vec3 p=at(x,y);
        if(pal.surface&&c.zone().biome!=5){ // statue on the surface
            out.solid.block(p+vec3(0,.3f,0),vec3(.8f,.6f,.8f),.06f,pal.rock,mat::Stone);
            out.mesh.cylinder(p+vec3(0,1.3f,0),.28f,.2f,1.4f,pal.rock*1.05f,mat::Stone,8);
            out.mesh.sphere(p+vec3(0,2.15f,0),vec3(.2f),pal.rock*1.05f,mat::Stone,8,6);return;
        }
        out.mesh.beam(p,p+vec3(0,3.6f,0),.05f,{.2f,.15f,.1f},mat::Wood,6);
        vec3 cloth=c.seals()>=3?vec3(.15f,.25f,.55f):vec3(.35f,.06f,.07f);
        Mesh flag;int segs=6;for(int i=0;i<segs;++i){float y0=3.4f-float(i)*.4f,y1=y0-.4f;
            flag.quad(p+vec3(.05f,y0,0),p+vec3(.85f,y0,0),p+vec3(.85f,y1,0),p+vec3(.05f,y1,0),cloth,mat::Banner);}
        out.mesh.vertices.insert(out.mesh.vertices.end(),flag.vertices.begin(),flag.vertices.end());
    }
    void mechanism(int x,int y){
        vec3 p=at(x,y);vec3 iron{.20f,.19f,.18f},rust{.42f,.24f,.14f};
        out.solid.block(p+vec3(0,.12f,0),vec3(1.1f,.24f,1.f),.03f,{.3f,.22f,.14f},mat::Wood);
        for(float sx:{-.4f,.4f})out.solid.block(p+vec3(sx,.8f,0),vec3(.12f,1.4f,.12f),.02f,{.3f,.22f,.14f},mat::Wood);
        Mesh gear;gear.cylinder(vec3(0),.45f,.45f,.12f,rust,mat::Brass,14);
        for(int k=0;k<8;++k){float a=float(k)*.785f;gear.box(vec3(std::cos(a)*.5f,0,std::sin(a)*.5f),vec3(.14f,.12f,.14f),rust,mat::Brass,-a);}
        for(auto v:gear.vertices){vec3 q=v.position;v.position=p+vec3(q.x,1.1f+q.z,q.y);v.normal=vec3(v.normal.x,v.normal.z,v.normal.y);out.mesh.vertices.push_back(v);}
        out.mesh.beam(p+vec3(-.5f,1.1f,0),p+vec3(.5f,1.1f,0),.06f,iron,mat::Steel,6);
    }
    void tent(int x,int y){
        vec3 p=at(x,y);vec3 cloth=vec3(.45f,.36f,.24f)*(0.9f+hash(x,y,1)*.2f);
        vec3 a=p+vec3(-.65f,0,-.6f),b=p+vec3(.65f,0,-.6f),cc=p+vec3(.65f,0,.6f),d=p+vec3(-.65f,0,.6f),t0=p+vec3(-.65f,1.3f,0),t1=p+vec3(.65f,1.3f,0);
        out.mesh.quad(a,t0,t1,b,cloth,mat::Cloth);out.mesh.quad(d,cc,t1,t0,cloth*.9f,mat::Cloth);
        out.mesh.beam(t0+vec3(-.1f,0,0),t1+vec3(.1f,0,0),.035f,{.3f,.2f,.1f},mat::Wood,5);
    }
    void oreRock(int x,int y,bool crystals){
        rock(x,y,.8f);
        if(!crystals)return;
        vec3 p=at(x,y);vec3 glow=c.zone().biome==4?vec3(.5f,.7f,1.f):vec3(1.f,.55f,.2f);
        for(int k=0;k<4;++k){vec3 o((hash(x,y,400+k)-.5f)*.7f,.6f+hash(x,y,410+k)*.4f,(hash(x,y,420+k)-.5f)*.7f);
            out.mesh.beam(p+o-vec3(0,.2f,0),p+o+vec3((hash(x,y,430+k)-.5f)*.3f,.25f,0),.07f,glow*.6f,mat::Glow,5);}
        light(p+vec3(0,1,0),glow*.8f,3.f);
    }
    void water(int x,int y){
        vec3 p=at(x,y,-.18f);
        out.solid.block(p+vec3(0,-.2f,0),vec3(s,.3f,s),0,vec3(.05f,.08f,.10f),mat::Stone);
        out.mesh.quad(p+vec3(-s*.5f,0,s*.5f),p+vec3(s*.5f,0,s*.5f),p+vec3(s*.5f,0,-s*.5f),p+vec3(-s*.5f,0,-s*.5f),
            pal.surface?vec3(.10f,.25f,.30f):vec3(.06f,.16f,.18f),mat::Water);
        // stones on the banks
        for(Cell d:{Cell{1,0},Cell{-1,0},Cell{0,1},Cell{0,-1}})if(walkable(x+d.x,y+d.y)&&hash(x,y,d.x*3+d.y+500)>.55f){
            vec3 q=p+vec3(float(d.x)*s*.45f,.08f,float(d.y)*s*.45f);out.solid.block(q,vec3(.35f,.25f,.3f),.08f,pal.rock,mat::Stone,hash(x,y,600));}
    }
    void shallow(int x,int y){
        if(pal.surface){ // plank bridge over the ford
            vec3 p=at(x,y,-.18f);
            out.mesh.quad(p+vec3(-s*.5f,0,s*.5f),p+vec3(s*.5f,0,s*.5f),p+vec3(s*.5f,0,-s*.5f),p+vec3(-s*.5f,0,-s*.5f),{.10f,.25f,.30f},mat::Water);
            for(int k=0;k<5;++k)out.solid.block(at(x,y)+vec3(0,.05f,(float(k)-2)*.27f),vec3(s,.08f,.24f),.02f,vec3(.38f,.27f,.16f)*(0.9f+hash(x,y,k)*.2f),mat::Wood);
        }else{
            vec3 p=at(x,y,-.06f);
            out.mesh.quad(p+vec3(-s*.5f,0,s*.5f),p+vec3(s*.5f,0,s*.5f),p+vec3(s*.5f,0,-s*.5f),p+vec3(-s*.5f,0,-s*.5f),{.08f,.18f,.2f},mat::Water);
            for(int k=0;k<2;++k)out.solid.block(at(x,y)+vec3((hash(x,y,k)-.5f)*.6f,-.02f,(hash(x,y,k+5)-.5f)*.6f),vec3(.4f,.12f,.4f),.05f,pal.rock,mat::Stone,hash(x,y,9));
        }
    }
    void poison(int x,int y){
        vec3 p=at(x,y,.02f);
        out.mesh.cylinder(p,.62f,.62f,.03f,{.10f,.30f,.06f},mat::Glow,12);
        emit(p+vec3(0,.05f,0),2,1);light(p+vec3(0,.6f,0),{.3f,1.f,.15f},3.f);
    }
    void trap(int x,int y){
        vec3 p=at(x,y,.03f);
        out.solid.block(p,vec3(1.f,.05f,1.f),.02f,{.2f,.2f,.21f},mat::Steel);
        for(int i=0;i<3;++i)for(int j=0;j<3;++j)out.mesh.cylinder(p+vec3((float(i)-1)*.28f,.08f,(float(j)-1)*.28f),.04f,.0f,.1f,{.35f,.35f,.36f},mat::Steel,5);
    }
    void mushrooms(int x,int y){
        vec3 p=at(x,y);for(int k=0;k<3;++k){vec3 q=p+vec3((hash(x,y,k)-.5f)*.8f,0,(hash(x,y,k+3)-.5f)*.8f);float h=.15f+hash(x,y,k+6)*.2f;
            out.mesh.beam(q,q+vec3(0,h,0),.025f,{.8f,.8f,.7f},mat::Plaster,5);out.mesh.sphere(q+vec3(0,h,0),{.1f,.05f,.1f},{.3f,.8f,.7f},mat::Glow,7,4);}
        light(p+vec3(0,.5f,0),{.2f,.7f,.6f},2.5f);
    }
    void rubble(int x,int y){vec3 p=at(x,y);for(int k=0;k<4;++k)out.solid.block(p+vec3((hash(x,y,k)-.5f)*.9f,.06f,(hash(x,y,k+4)-.5f)*.9f),vec3(.2f+hash(x,y,k+8)*.15f,.12f,.18f),.04f,pal.rock*.85f,mat::Stone,hash(x,y,k+12)*3);}

    // ---------------------------------------------------------- continuous terrain (surface zones)
    // Corner heightfield: walkable ground stays flat for combat, forests rise into soft hills,
    // banks slope into the water. Colours blend across cells, so there is no visible tile grid.
    bool terrainZone()const{return pal.surface&&c.zone().biome!=5;}
    static bool natureBlock(char g){return g=='T'||g=='#'||g=='r'||g=='y'||g=='b';}
    vec3 cellColor(int x,int y)const{
        char g=map.glyph(x,y);Tile t=map.at(x,y);
        vec3 grass=pal.ground;
        float n=hash(x/3,y/3,7)*.5f+hash(x,y,8)*.5f;
        grass=glm::mix(grass,grass*vec3(1.18f,1.08f,.8f),n*.5f);
        if(t==Tile::Water)return vec3(.62f,.56f,.40f);
        if(g==','||t==Tile::Path)return pal.path;
        if(g=='#'||g=='r')return pal.rock*1.05f;
        if(g=='T'||g=='y')return grass*vec3(.72f,.82f,.7f);
        if(c.zone().biome==4&&hash(x,y,2)>.8f)return glm::mix(grass,vec3(.82f,.84f,.88f),.6f);
        return grass;
    }
    void terrain(){
        const int W=map.width,H=map.height;
        auto cellAt=[&](int x,int y)->char{x=std::clamp(x,0,W-1);y=std::clamp(y,0,H-1);return map.glyph(x,y);};
        std::vector<float> height(size_t((W+1)*(H+1)),0.f);std::vector<vec3> color(size_t((W+1)*(H+1)));std::vector<float> wet(size_t((W+1)*(H+1)),0.f);
        for(int y=0;y<=H;++y)for(int x=0;x<=W;++x){
            int nature=0,water=0,walk=0;vec3 col(0);
            for(int dy=-1;dy<=0;++dy)for(int dx=-1;dx<=0;++dx){
                int cx=std::clamp(x+dx,0,W-1),cy=std::clamp(y+dy,0,H-1);char g=cellAt(cx,cy);
                if(map.at(cx,cy)==Tile::Water)++water;else if(natureBlock(g))++nature;else ++walk;
                col+=cellColor(cx,cy);
            }
            float hgt=0;
            if(water>0&&walk+nature>0)hgt=-.22f;                 // bank
            else if(water==4)hgt=-.42f;                          // river bed
            else if(nature==4){bool cliff=false;for(int dy=-1;dy<=0;++dy)for(int dx=-1;dx<=0;++dx)cliff|=cellAt(x+dx,y+dy)=='#';
                hgt=(cliff?.9f:.35f)+hash(x,y,31)*(cliff?1.4f:.75f);}
            else if(nature>0)hgt=.04f+.06f*hash(x,y,32);
            if(walk>0&&nature<4)hgt=std::min(hgt,.0f+(nature>0?.05f:0.f));
            if(walk==4)hgt=0;
            size_t i=size_t(y*(W+1)+x);height[i]=hgt;color[i]=col*.25f;
            if(water>0&&water<4)color[i]=glm::mix(color[i],vec3(.74f,.66f,.46f),.75f);   // sandy shore
            wet[i]=float(water)/4.f;
        }
        corner=height;cornerW=W;
        auto P=[&](int x,int y){return vec3(map.originX+float(x)*s,height[size_t(y*(W+1)+x)],map.originZ+float(y)*s);};
        auto N=[&](int x,int y){
            float l=height[size_t(y*(W+1)+std::max(0,x-1))],r=height[size_t(y*(W+1)+std::min(W,x+1))];
            float d=height[size_t(std::max(0,y-1)*(W+1)+x)],u=height[size_t(std::min(H,y+1)*(W+1)+x)];
            return glm::normalize(vec3(l-r,2.f*s,d-u));};
        auto V=[&](int x,int y){return Vertex{P(x,y),N(x,y),color[size_t(y*(W+1)+x)],mat::Ground};};
        for(int y=0;y<H;++y)for(int x=0;x<W;++x){
            Vertex a=V(x,y),b=V(x,y+1),cc=V(x+1,y+1),d=V(x+1,y);
            out.solid.vertices.insert(out.solid.vertices.end(),{a,b,cc,a,cc,d});
        }
        // water surface with a depth gradient and a pale rim of foam along the banks
        for(int y=0;y<H;++y)for(int x=0;x<W;++x){
            if(map.at(x,y)!=Tile::Water)continue;
            auto WV=[&](int vx,int vy){float w=wet[size_t(vy*(W+1)+vx)];
                vec3 deep(.07f,.25f,.36f),shallow(.30f,.62f,.62f),foam(.85f,.92f,.90f);
                vec3 col=w>=1.f?deep:glm::mix(foam,shallow,w);
                return Vertex{vec3(map.originX+float(vx)*s,-.14f,map.originZ+float(vy)*s),vec3(0,1,0),col,mat::Water};};
            Vertex a=WV(x,y),b=WV(x,y+1),cc=WV(x+1,y+1),d=WV(x+1,y);
            out.solid.vertices.insert(out.solid.vertices.end(),{a,b,cc,a,cc,d});
        }
        // a wide apron so the map edge blends into the land around it
        float w=float(W)*s,h=float(H)*s;
        out.solid.block(vec3(0,-.12f,0),vec3(w+180.f,.2f,h+180.f),0,pal.ground*.92f,mat::Ground);
    }
    void blossomTree(vec3 p,float scale,vec3 petals,int seed){
        vec3 bark{.30f,.20f,.15f};
        out.mesh.cylinder(p+vec3(0,.85f*scale,0),.14f*scale,.09f*scale,1.7f*scale,bark,mat::Wood,7);
        out.mesh.beam(p+vec3(0,1.3f*scale,0),p+vec3(.45f,1.9f,.2f)*scale,.06f*scale,bark,mat::Wood,5);
        broadleafCrown(p+vec3(-.25f,2.31f,-.15f)*scale,vec3(.86f,.72f,.84f)*scale,petals,seed,true);
        broadleafCrown(p+vec3(.45f,1.96f,.22f)*scale,vec3(.71f,.54f,.68f)*scale,petals*1.06f,seed+9,true);
    }
    void windmill(vec3 p,float time0){
        vec3 stone=pal.rock*1.2f,wood{.38f,.26f,.15f},roof{.45f,.22f,.14f};
        out.mesh.cylinder(p+vec3(0,2.4f,0),1.5f,1.1f,4.8f,stone,mat::Stone,12);
        out.mesh.cylinder(p+vec3(0,5.4f,0),1.25f,.05f,1.6f,roof,mat::Wood,12);
        vec3 hub=p+vec3(0,4.3f,1.25f);
        for(int k=0;k<4;++k){float a=float(k)*1.5708f+time0;vec3 d(std::cos(a),std::sin(a),0);
            out.mesh.beam(hub,hub+d*3.4f,.07f,wood,mat::Wood,5);
            vec3 side(-d.y,d.x,0);out.mesh.quad(hub+d*.8f,hub+d*3.3f,hub+d*3.3f+side*.6f,hub+d*.8f+side*.6f,vec3(.86f,.82f,.72f),mat::Cloth);}
        out.mesh.quad(p+vec3(-.35f,0,1.12f),p+vec3(.35f,0,1.12f),p+vec3(.35f,1.2f,1.1f),p+vec3(-.35f,1.2f,1.1f),wood,mat::Wood);
    }
    // ---------------------------------------------------------- entities
    void shrine(const rpg::Entity& e){
        const vec3 p=at(e.cell.x,e.cell.y,groundAt(e.cell.x,e.cell.y));
        const bool active=c.shrineActive(e.def.id);
        const vec3 shale=glm::mix(vec3(.32f,.39f,.40f),pal.rock,.24f), bronze{.57f,.39f,.21f};
        out.solid.block(p+vec3(0,.10f,0),{1.28f,.20f,1.28f},.075f,shale*.75f,mat::Stone);
        out.mesh.cylinder(p+vec3(0,.27f,0),.64f,.50f,.16f,shale*1.22f,mat::Stone,8);
        out.mesh.cylinder(p+vec3(0,.58f,0),.45f,.33f,.46f,shale,mat::Stone,8);
        out.mesh.cylinder(p+vec3(0,.83f,0),.34f,.39f,.05f,bronze,mat::Brass,8);
        out.mesh.cylinder(p+vec3(0,.99f,0),.37f,.52f,.28f,shale*1.17f,mat::Stone,8);
        out.mesh.cylinder(p+vec3(0,1.16f,0),.53f,.53f,.06f,bronze*.86f,mat::Brass,8);

        // Paired bronze leaves frame a faceted votive stone against open air.
        const float heights[]={1.13f,1.48f,1.82f,2.10f,2.46f};
        const float offsets[]={.22f,.42f,.45f,.34f,.15f};
        const float widths[] ={.11f,.13f,.095f,.07f,0.f};
        for(float side:{-1.f,1.f}){
            auto leaf=[&](int row,int face){
                float a=pi*.5f*float(face), w=widths[row];
                return p+vec3(side*(offsets[row]+w*std::cos(a)),heights[row],side*.09f+w*std::sin(a));
            };
            for(int row=0;row<4;++row)for(int face=0;face<4;++face){
                vec3 color=bronze*(face==1?1.22f:face==3?.70f:.95f);
                out.mesh.quad(leaf(row,face),leaf(row+1,face),leaf(row+1,face+1),leaf(row,face+1),color,mat::Brass);
            }
        }
        const vec3 crystal=active?vec3(.11f,.49f,.42f):vec3(.25f,.38f,.38f);
        for(int i=0;i<6;++i){
            auto girdle=[&](int index){float a=float(index)*pi/3;return p+vec3(.17f*std::cos(a),1.78f,.17f*std::sin(a));};
            vec3 a=girdle(i), b=girdle(i+1);
            float value=.72f+.15f*float((5*i)%4);
            float material=active?mat::Glow:mat::Stone;
            out.mesh.triangle(a,p+vec3(0,2.17f,0),b,crystal*value,material);
            out.mesh.triangle(b,p+vec3(0,1.40f,0),a,crystal*(value*.72f),material);
        }
        if(active){light(p+vec3(0,1.78f,0),{.4f,1.6f,1.4f},6.f);emit(p+vec3(0,1.46f,0),4,1);}
    }
    void chest(const rpg::Entity& e){
        const vec3 p=at(e.cell.x,e.cell.y,groundAt(e.cell.x,e.cell.y));
        const vec3 walnut{.42f,.245f,.16f}, iron{.19f,.23f,.24f}, brass{.68f,.48f,.26f}, lining{.27f,.10f,.14f};
        out.solid.block(p+vec3(0,.10f,0),{1.02f,.17f,.66f},.035f,walnut*.65f,mat::Wood);
        out.solid.block(p+vec3(0,.19f,0),{.94f,.08f,.57f},.015f,lining,mat::Cloth);
        for(float side:{-1.f,1.f}){
            for(int k=0;k<2;++k){
                float h=.25f+.15f*k;
                out.solid.block(p+vec3(0,h,side*.30f),{1.01f,.145f,.066f},.012f,walnut*(.84f+.18f*k),mat::Wood);
                out.solid.block(p+vec3(side*.47f,h,0),{.069f,.145f,.56f},.012f,walnut*.88f,mat::Wood);
            }
            for(float x:{-.34f,.34f})
                out.solid.block(p+vec3(x,.30f,side*.334f),{.085f,.37f,.018f},.006f,iron,mat::Steel);
        }
        // The outer staves, lining and straps rotate together about the rear hinge.
        const float angle=e.done?1.35f:0.f, co=std::cos(angle), si=std::sin(angle);
        auto lid=[&](float x,float y,float z){
            return p+vec3(x,.48f+y*co+(z+.33f)*si,-.33f-y*si+(z+.33f)*co);
        };
        auto stave=[&](float x,int index,float layer){
            float a=float(index)*pi/6;
            return lid(x,(.27f+layer)*std::sin(a),(.337f+layer)*std::cos(a));
        };
        for(int i=0;i<6;++i){
            out.mesh.quad(stave(-.51f,i,0),stave(.51f,i,0),stave(.51f,i+1,0),stave(-.51f,i+1,0),walnut*(.84f+.10f*float(i%3)),mat::Wood);
            out.mesh.quad(stave(.48f,i,-.026f),stave(-.48f,i,-.026f),stave(-.48f,i+1,-.026f),stave(.48f,i+1,-.026f),lining,mat::Cloth);
            for(float side:{-1.f,1.f}){
                float x=.51f*side;
                out.mesh.triangle(lid(x,0,0),stave(x,i,0),stave(x,i+1,0),walnut*.84f,mat::Wood);
                float strap=.34f*side;
                out.mesh.quad(stave(strap-.045f,i,.009f),stave(strap+.045f,i,.009f),stave(strap+.045f,i+1,.009f),stave(strap-.045f,i+1,.009f),iron,mat::Steel);
            }
        }
        for(float side:{-1.f,1.f}){
            float x=side*.34f;
            out.mesh.beam(lid(x-.054f,0,-.33f),lid(x+.054f,0,-.33f),.034f,brass*.8f,mat::Brass,8);
        }
        if(!e.done){
            vec3 q=p+vec3(0,.49f,.347f);
            out.mesh.quad(q+vec3(0,-.12f,0),q+vec3(.11f,0,0),q+vec3(0,.12f,0),q+vec3(-.11f,0,0),brass,mat::Brass);
            out.solid.block(q+vec3(0,-.013f,.008f),{.028f,.067f,.012f},.004f,iron*.48f,mat::Steel);
            if(e.def.cond[0])out.solid.block(p+vec3(0,.37f,0),{1.035f,.065f,.68f},.012f,iron,mat::Steel);
        }
    }
    void signpost(vec3 p,bool locked,vec3 toward){
        vec3 wood{.35f,.25f,.15f};
        out.solid.block(p+vec3(0,.7f,0),vec3(.12f,1.4f,.12f),.02f,wood,mat::Wood);
        float yaw=std::atan2(toward.x,toward.z);
        out.solid.block(p+vec3(0,1.2f,0)+toward*.25f,vec3(.12f,.25f,.75f),.02f,locked?vec3(.45f,.12f,.1f):wood*1.2f,mat::Wood,yaw);
        out.mesh.cylinder(p+vec3(0,.02f,0),.5f,.5f,.02f,locked?vec3(.3f,.06f,.04f):vec3(.15f,.35f,.25f),mat::Glow,14);
    }
    vec3 outward(Cell c0)const{ // direction off the map from an edge cell
        vec3 d(0);if(c0.x<=1)d.x=-1;if(c0.x>=map.width-2)d.x=1;if(c0.y<=1)d.z=-1;if(c0.y>=map.height-2)d.z=1;
        if(glm::length(d)<.1f)d={0,0,-1};return glm::normalize(d);
    }
    void exitMark(const rpg::Entity& e){
        vec3 p=at(e.cell.x,e.cell.y);bool locked=!c.check(e.def.cond);
        if(pal.surface&&e.def.type==EntityType::Exit){
            // road continues off the map
            vec3 d=outward(e.cell);
            for(int k=1;k<5;++k)out.solid.block(p+d*(float(k)*s)+vec3(0,-.06f,0),vec3(s,.12f,s),0,pal.path,mat::Ground);
            signpost(p+vec3(-d.z,0,d.x)*.9f,locked,d);
            if(std::string(e.def.id)=="to_castle"&&locked){
                for(int k=-1;k<=1;++k)out.mesh.beam(p+vec3(float(k)*.4f,0,0),p+vec3(float(k)*.4f,2.6f,0),.06f,{.15f,.15f,.16f},mat::Steel,6);
                out.mesh.beam(p+vec3(-.7f,1.5f,0),p+vec3(.7f,1.5f,0),.06f,{.15f,.15f,.16f},mat::Steel,6);
                emit(p+vec3(0,1.,0),5,1);light(p+vec3(0,1.5f,0),{.7f,.2f,.9f},4.f);
            }
        }else if(e.def.type==EntityType::Entrance){
            vec3 dark{.02f,.02f,.03f};
            out.mesh.cylinder(p+vec3(0,.01f,0),.62f,.62f,.02f,dark,mat::Stone,12);
            for(int k=0;k<5;++k){float a=3.14159f*float(k)/4.f;vec3 q=p+vec3(std::cos(a)*.75f,std::sin(a)*1.3f+.2f,-.6f);
                out.solid.block(q,vec3(.45f,.45f,.45f),.08f,pal.rock*.8f,mat::Stone,a);}
            light(p+vec3(0,1.,.6f),locked?vec3(.8f,.2f,.1f):vec3(.6f,.5f,.3f),3.f);
            if(locked){for(int k=-1;k<=1;++k)out.solid.block(p+vec3(float(k)*.3f,.8f,-.3f),vec3(.12f,1.4f,.1f),.02f,{.3f,.22f,.12f},mat::Wood,.1f*float(k));
                out.solid.block(p+vec3(0,.9f,-.25f),vec3(1.1f,.15f,.08f),.02f,{.3f,.22f,.12f},mat::Wood,.15f);}
        }else{
            // dungeon exit: stair landing with a soft light
            for(int k=0;k<3;++k)out.solid.block(p+vec3(0,-.05f-float(k)*.12f,-float(k)*.25f),vec3(1.2f,.1f,.4f),.02f,pal.ground*(1.1f-.15f*float(k)),mat::Stone);
            light(p+vec3(0,1.2f,0),locked?vec3(.8f,.25f,.1f):vec3(.5f,.7f,.9f),3.5f);
            out.mesh.cylinder(p+vec3(0,.02f,0),.55f,.55f,.02f,locked?vec3(.3f,.08f,.04f):vec3(.12f,.25f,.35f),mat::Glow,14);
        }
    }
    void gate(const rpg::Entity& e){
        vec3 p=at(e.cell.x,e.cell.y);
        if(std::string(e.def.id)=="rockslide"){if(!e.done)for(int k=0;k<6;++k)out.solid.block(p+vec3((hash(e.cell.x,k)-.5f)*1.,.3f+float(k%3)*.35f,(hash(k,e.cell.y)-.5f)*1.),vec3(.6f,.5f,.6f),.12f,pal.rock,mat::Stone,float(k));return;}
        vec3 iron{.16f,.17f,.18f};
        out.solid.block(p+vec3(-.62f,1.3f,0),vec3(.2f,2.6f,.4f),.03f,pal.wall,mat::Masonry);
        out.solid.block(p+vec3(.62f,1.3f,0),vec3(.2f,2.6f,.4f),.03f,pal.wall,mat::Masonry);
        out.solid.block(p+vec3(0,2.7f,0),vec3(1.5f,.3f,.45f),.03f,pal.wall*.9f,mat::Masonry);
        if(e.done)return;
        if(e.def.type==EntityType::Gate)for(int k=-2;k<=2;++k)out.mesh.beam(p+vec3(float(k)*.22f,0,0),p+vec3(float(k)*.22f,2.5f,0),.04f,iron,mat::Steel,6);
        else{out.solid.block(p+vec3(0,1.25f,0),vec3(1.05f,2.5f,.18f),.03f,e.def.type==EntityType::RuneDoor?pal.wall*1.1f:vec3(.30f,.18f,.10f),e.def.type==EntityType::RuneDoor?mat::Stone:mat::Wood);
            if(e.def.type==EntityType::RuneDoor)for(int k=0;k<3;++k){vec3 q=p+vec3((float(k)-1)*.3f,1.5f,.1f);bool lit=c.flags.count("runes_solved")||k<c.runeProgress;
                out.mesh.quad(q+vec3(-.08f,-.1f,0),q+vec3(.08f,-.1f,0),q+vec3(.08f,.1f,0),q+vec3(-.08f,.1f,0),lit?vec3(.3f,.6f,1.f):vec3(.1f),lit?mat::Glow:mat::Stone);}
            else out.solid.block(p+vec3(.3f,1.2f,.1f),vec3(.12f,.12f,.05f),.01f,{.6f,.5f,.25f},mat::Brass);}
    }
    void lever(const rpg::Entity& e){
        vec3 p=at(e.cell.x,e.cell.y);mechanism(e.cell.x,e.cell.y);
        float a=e.done?-.6f:.6f;
        out.mesh.beam(p+vec3(.55f,.4f,.3f),p+vec3(.55f+std::sin(a)*.6f,.4f+std::cos(a)*.6f,.3f),.05f,{.3f,.3f,.32f},mat::Steel,6);
        out.mesh.sphere(p+vec3(.55f+std::sin(a)*.6f,.4f+std::cos(a)*.6f,.3f),vec3(.08f),e.done?vec3(.2f,.9f,.3f):vec3(.9f,.2f,.1f),mat::Glow,6,4);
        if(e.done)light(p+vec3(0,1.5f,0),{.4f,.9f,.5f},3.f);
    }
    void cocoon(const rpg::Entity& e,bool survivor){
        vec3 p=at(e.cell.x,e.cell.y);
        if(e.done){for(int k=0;k<4;++k){float a=float(k)*1.57f;out.mesh.beam(p+vec3(0,.05f,0),p+vec3(std::cos(a)*.5f,.25f,std::sin(a)*.5f),.06f,{.75f,.75f,.7f},mat::Plaster,5);}return;}
        out.mesh.sphere(p+vec3(0,.75f,0),vec3(.42f,.8f,.42f),{.80f,.80f,.74f},mat::Plaster,10,8);
        for(int k=0;k<5;++k)out.mesh.beam(p+vec3(0,1.5f,0),p+vec3((hash(e.cell.x,k)-.5f)*1.4f,3.f,(hash(k,e.cell.y)-.5f)*1.4f),.012f,{.85f,.85f,.85f},mat::Plaster,3);
        if(survivor){out.mesh.sphere(p+vec3(0,1.35f,.2f),vec3(.17f,.19f,.17f),{.62f,.45f,.36f},mat::Cloth,8,6);light(p+vec3(0,1.6f,.6f),{.8f,.7f,.4f},2.5f);}
    }
    void runeStone(const rpg::Entity& e){
        vec3 p=at(e.cell.x,e.cell.y);int idx=std::stoi(e.def.arg);
        out.solid.block(p+vec3(0,.6f,0),vec3(.7f,1.2f,.35f),.08f,pal.wall*1.1f,mat::Stone);
        vec3 col=idx==0?vec3(1.f,.7f,.2f):idx==1?vec3(.5f,.6f,1.f):vec3(.8f,.8f,1.f);
        bool lit=c.flags.count("runes_solved")>0;
        vec3 q=p+vec3(0,.8f,.19f);
        out.mesh.quad(q+vec3(-.17f,-.2f,0),q+vec3(.17f,-.2f,0),q+vec3(.17f,.2f,0),q+vec3(-.17f,.2f,0),lit?col:col*.3f,mat::Glow);
        light(p+vec3(0,1.2f,.6f),col*(lit?1.f:.35f),2.5f);
    }
    void prop(const rpg::Entity& e){
        vec3 p=at(e.cell.x,e.cell.y);
        switch(e.def.type){
        case EntityType::Note:
            out.solid.block(p+vec3(0,.4f,0),vec3(.4f,.8f,.4f),.04f,{.30f,.22f,.14f},mat::Wood);
            out.solid.block(p+vec3(0,.84f,0),vec3(.55f,.06f,.42f),.01f,{.85f,.80f,.65f},mat::Plaster,.2f);
            light(p+vec3(0,1.2f,.4f),{.8f,.7f,.4f},2.f);break;
        case EntityType::Cargo:
            if(e.done)break;
            out.solid.block(p+vec3(0,.4f,0),vec3(.9f,.8f,.7f),.05f,{.42f,.30f,.16f},mat::Wood,.3f);
            out.solid.block(p+vec3(0,.45f,.36f),vec3(.3f,.3f,.02f),.01f,{.7f,.15f,.1f},mat::Cloth,.3f);break;
        case EntityType::Clue:
            if(e.done)break;
            out.mesh.sphere(p+vec3(0,.18f,0),{.3f,.22f,.3f},{.45f,.38f,.2f},mat::Brass,10,6);
            out.mesh.cylinder(p+vec3(.12f,.32f,.2f),.07f,.07f,.1f,{1.f,.8f,.4f},mat::Glow,8);break;
        case EntityType::KeyCache:
            out.mesh.cylinder(p+vec3(0,.7f,0),.55f,.45f,1.4f,{.25f,.18f,.12f},mat::Wood,9);
            out.mesh.cylinder(p+vec3(0,.8f,.4f),.18f,.18f,.4f,{.03f,.02f,.02f},mat::Stone,8);
            if(!e.done)light(p+vec3(0,1.,.6f),{.9f,.8f,.4f},2.f);break;
        case EntityType::Ore:oreRock(e.cell.x,e.cell.y,!e.done);break;
        case EntityType::Herb:
            if(e.done||e.hidden)break;
            for(int k=0;k<5;++k){float a=float(k)*1.25f;out.mesh.beam(p,p+vec3(std::cos(a)*.18f,.45f,std::sin(a)*.18f),.02f,{.25f,.55f,.2f},mat::Foliage,4);
                out.mesh.sphere(p+vec3(std::cos(a)*.18f,.47f,std::sin(a)*.18f),vec3(.05f),{.95f,.95f,.8f},mat::Glow,5,3);}
            break;
        default:break;
        }
    }
    void backdrop(){
        if(!pal.surface)return;
        float w=float(map.width)*s,h=float(map.height)*s;
        // A ring of painted mountains; snow caps catch the sun.
        for(int k=0;k<26;++k){
            float a=float(k)/26.f*6.2832f+hash(k,3)*.2f;
            float dist=std::max(w,h)*.5f+55.f+hash(k,7)*30.f;
            vec3 p(std::cos(a)*dist,0,std::sin(a)*dist);
            float height=30.f+hash(k,9)*34.f,radius=22.f+hash(k,11)*14.f;
            vec3 rock=vec3(.33f,.34f,.38f)*(0.8f+hash(k,13)*.3f);
            out.mesh.cylinder(p+vec3(0,height*.5f-2.f,0),radius,radius*.12f,height,rock,mat::Stone,9);
            out.mesh.cylinder(p+vec3(0,height*.82f-2.f,0),radius*.36f,radius*.06f,height*.36f,{.88f,.9f,.95f},mat::Snow,9);
        }
        // outer ground apron so the map edge does not float
        out.solid.block(vec3(0,-.25f,0),vec3(w+160.f,.2f,h+160.f),0,pal.ground*.75f,mat::Ground);
        // Castle Valden on the northern ridge; visible from the village and the road
        int b=c.zone().biome;
        if(b==0||b==1||b==5){
            vec3 base(b==5?0.f:-6.f,0,-h*.5f-(b==5?6.f:24.f));
            if(b==5)base.y=0;
            vec3 stone{.36f,.35f,.38f};
            out.solid.block(base+vec3(0,2.f,0),vec3(26.f,4.f,10.f),.2f,stone*.8f,mat::Masonry);
            out.solid.block(base+vec3(0,7.f,-1.f),vec3(10.f,10.f,7.f),.2f,stone,mat::Masonry);
            for(float sx:{-12.f,-5.f,5.f,12.f}){out.mesh.cylinder(base+vec3(sx,5.f,1.f),1.8f,1.7f,10.f+std::abs(sx)*.2f,stone*.95f,mat::Masonry,10);
                out.mesh.cylinder(base+vec3(sx,11.5f+std::abs(sx)*.1f,1.f),2.1f,.05f,3.2f,{.25f,.18f,.25f},mat::Wood,10);}
            out.mesh.cylinder(base+vec3(0,15.f,-1.f),2.4f,2.2f,6.f,stone,mat::Masonry,10);
            out.mesh.cylinder(base+vec3(0,19.5f,-1.f),2.8f,.05f,4.f,{.25f,.18f,.25f},mat::Wood,10);
            bool cursed=!c.flags.count("boss_keeper");
            if(cursed){light(base+vec3(0,12.f,6.f),{1.2f,.3f,1.6f},18.f);emit(base+vec3(0,18.f,0),5,4);}
            for(float sx:{-3.f,3.f})out.mesh.quad(base+vec3(sx-.5f,8.f,2.55f),base+vec3(sx+.5f,8.f,2.55f),base+vec3(sx+.5f,9.5f,2.55f),base+vec3(sx-.5f,9.5f,2.55f),cursed?vec3(.6f,.2f,.9f):vec3(1.f,.7f,.3f),mat::Ember);
        }
    }
    void houses(){
        std::vector<char> seen(size_t(map.width*map.height),0);
        for(int y=0;y<map.height;++y)for(int x=0;x<map.width;++x){
            if(map.glyph(x,y)!='H'||seen[size_t(y*map.width+x)])continue;
            int x1=x;while(x1+1<map.width&&map.glyph(x1+1,y)=='H')++x1;
            int y1=y;while(y1+1<map.height){bool row=true;for(int k=x;k<=x1;++k)row&=map.glyph(k,y1+1)=='H';if(!row)break;++y1;}
            for(int yy=y;yy<=y1;++yy)for(int xx=x;xx<=x1;++xx)seen[size_t(yy*map.width+xx)]=1;
            house(x,y,x1,y1);
        }
    }
    void dungeonTorches(){
        // Wall torches along corridors; deterministic spacing keeps the light count bounded.
        int placed=0;
        for(int y=1;y<map.height-1;++y)for(int x=1;x<map.width-1;++x){
            if(!wallish(x,y)||placed>=14)continue;
            for(Cell d:{Cell{0,1},Cell{1,0},Cell{-1,0}}){
                if(!walkable(x+d.x,y+d.y))continue;
                if((x*7+y*13)%11!=0)continue;
                vec3 n(float(d.x),0,float(d.y));vec3 p=at(x,y,1.7f)+n*s*.5f;wallTorch(p,n);++placed;break;
            }
        }
    }
    void build(){
        int biome=c.zone().biome;
        if(biome==13){
            buildArchitecture(out.solid);buildProps(out.mesh,out.torches);
            for(size_t i=0;i<out.torches.size();++i){
                auto& t=out.torches[i];emit(t.position,0,t.scale);
                light(t.position+vec3(0,i==2||i==3?.38f:.17f,0),vec3(5.0f,1.85f,.40f)*t.scale,7.f,i==2||i==3);
            }
            light({-8,2.4f,-9.6f},{.08f,.7f,.65f},5);
        }else{
            if(terrainZone())terrain();
            for(int y=0;y<map.height;++y)for(int x=0;x<map.width;++x){
                char g=map.glyph(x,y);
                bool wall=map.at(x,y)==Tile::Wall;
                if(!terrainZone()&&!wall&&g!='~')ground(x,y);
                else if(terrainZone()&&!wall&&map.at(x,y)!=Tile::Water&&!map.blocked(x,y)&&g!=','){
                    if(hash(x,y,1)>.25f)grassTufts(x,y);
                    if(hash(x,y,44)>.9f&&c.zone().biome!=4)flowers(x,y);
                }
                switch(g){
                case '#':cliff(x,y);break;
                case 'K':castleWall(x,y);break;
                case 'T':tree(x,y);break;
                case 'y':deadTree(x,y);break;
                case 'r':rock(x,y,1.f);break;
                case '~':if(!terrainZone())water(x,y);break;
                case '=':shallow(x,y);break;
                case 'w':floorWeb(x,y);break;
                case 'p':poison(x,y);break;
                case '^':trap(x,y);break;
                case 'c':cart(x,y);break;
                case 'x':crates(x,y);break;
                case 'f':fence(x,y);break;
                case 's':stalagmite(x,y);break;
                case 'k':bones(x,y);break;
                case 'g':grave(x,y);break;
                case 'l':brazier(x,y);break;
                case 'P':pillar(x,y);break;
                case 'b':bush(x,y);break;
                case 'F':flowers(x,y);break;
                case 'W':well(x,y);break;
                case 'B':banner(x,y);break;
                case 'm':mechanism(x,y);break;
                case 'n':tent(x,y);break;
                case 'd':rubble(x,y);break;
                case 'u':mushrooms(x,y);break;
                case 'v':oreRock(x,y,true);break;
                default:break;
                }
            }
            houses();
            if(biome==0)windmill(at(map.width+2,7),.3f);
            if(!pal.surface)dungeonTorches();
            if(biome==2)for(int k=0;k<6;++k)emit(at(int(hash(k,1)*float(map.width)),int(hash(k,2)*float(map.height)),1.2f),3,1);
            if(biome==14){for(int k=0;k<4;++k)emit(at(map.width/2+int((hash(k,3)-.5f)*10),5+int(hash(k,4)*6),.2f),5,1.5f);}
        }
        for(const auto& e:c.entities){
            if(e.hidden&&e.def.type!=EntityType::Herb)continue;
            switch(e.def.type){
            case EntityType::Shrine:shrine(e);break;
            case EntityType::Chest:chest(e);break;
            case EntityType::Exit:case EntityType::Entrance:exitMark(e);break;
            case EntityType::Gate:case EntityType::LockDoor:case EntityType::RuneDoor:gate(e);break;
            case EntityType::Lever:lever(e);break;
            case EntityType::Cocoon:cocoon(e,false);break;
            case EntityType::Survivor:cocoon(e,true);break;
            case EntityType::Rune:runeStone(e);break;
            default:prop(e);break;
            }
        }
        backdrop();
        // Village festival after the final victory.
        if(c.zoneId=="village"&&c.flags.count("boss_keeper")){
            for(int k=0;k<10;++k){vec3 p=at(9+k*2,11,3.2f);out.mesh.sphere(p,vec3(.18f),k%2?vec3(1.f,.6f,.2f):vec3(1.f,.3f,.2f),mat::Ember,7,5);
                if(k>0)out.mesh.beam(at(9+(k-1)*2,11,3.25f),p,.01f,{.2f,.15f,.1f},mat::Wood,3);if(k%3==0)light(p,{1.6f,.8f,.3f},4.f);}
        }
    }
};
}

void buildZoneVisuals(const rpg::Campaign& c,ZoneVisuals& out){
    out=ZoneVisuals{};
    Builder b{c,c.map,out,palette(c.zone().biome)};
    b.build();
}
std::uint32_t entityStateHash(const rpg::Campaign& c){
    std::uint32_t h=2166136261u;
    auto mix=[&](std::uint32_t v){h^=v;h*=16777619u;};
    for(const auto& e:c.entities){mix(e.done);mix(e.hidden);}
    mix(std::uint32_t(c.runeProgress));mix(std::uint32_t(c.flags.size()));mix(std::uint32_t(c.seals()));
    for(int i=0;i<c.map.width*c.map.height;i+=1)mix(std::uint32_t(c.map.tiles[size_t(i)]));
    return h;
}
void floorTile(Mesh& m,const world::Map& map,Cell cell,vec3 color,float thickness,float lift){
    auto p=map.center(cell);float r=.66f;
    vec3 a=p+vec3(-r,lift,-r),b=p+vec3(r,lift,-r),cc=p+vec3(r,lift,r),d=p+vec3(-r,lift,r);
    for(auto edge:{std::pair{a,b},std::pair{b,cc},std::pair{cc,d},std::pair{d,a}}){
        auto side=glm::normalize(glm::cross(edge.second-edge.first,vec3(0,1,0)))*thickness;
        m.quad(edge.first-side,edge.first+side,edge.second+side,edge.second-side,color,mat::Glow);
    }
}
void fillTile(Mesh& m,const world::Map& map,Cell cell,vec3 color,float inset,float lift){
    auto p=map.center(cell);float r=.7f-inset;
    m.quad(p+vec3(-r,lift,r),p+vec3(r,lift,r),p+vec3(r,lift,-r),p+vec3(-r,lift,-r),color,mat::Glow);
}
void buildLootBag(Mesh& m,vec3 p,float time){
    m.sphere(p+vec3(0,.28f,0),{.28f,.3f,.26f},{.48f,.36f,.20f},mat::Cloth,8,6);
    m.cylinder(p+vec3(0,.6f,0),.08f,.12f,.14f,{.40f,.30f,.16f},mat::Cloth,6);
    float b=.85f+.15f*std::sin(time*4);
    m.sphere(p+vec3(.18f,.62f+.05f*std::sin(time*3),.1f),vec3(.06f),vec3(1.f,.8f,.3f)*b,mat::Glow,6,4);
}
