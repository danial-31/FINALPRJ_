#pragma once
#include <glm/glm.hpp>
#include <cstdint>
#include <cstdlib>
#include <string>
#include <vector>

// Runtime tile grid shared by exploration, combat, rendering and pathfinding.
namespace world {
struct Cell {
    int x=-1, y=-1;
    bool operator==(Cell b) const { return x==b.x && y==b.y; }
    bool operator!=(Cell b) const { return !(*this==b); }
};
inline int manhattan(Cell a, Cell b) { return std::abs(a.x-b.x)+std::abs(a.y-b.y); }
inline int chebyshev(Cell a, Cell b) { return std::max(std::abs(a.x-b.x),std::abs(a.y-b.y)); }

enum class Tile : std::uint8_t {
    Floor, Path, Wall, Water, Shallow, Web, Poison, Trap, Prop, LowProp, Door, Decor
};

struct Map {
    static constexpr float cellSize=1.4f;
    int width=0, height=0;
    float originX=0, originZ=0;
    std::vector<Tile> tiles;
    std::vector<char> glyphs;      // source character, used by the mesh builder

    void resize(int w,int h,Tile fill=Tile::Floor){
        width=w;height=h;tiles.assign(size_t(w)*size_t(h),fill);glyphs.assign(size_t(w)*size_t(h),'.');
        originX=-float(w)*cellSize*.5f;originZ=-float(h)*cellSize*.5f;
    }
    bool inside(int x,int y) const { return x>=0&&y>=0&&x<width&&y<height; }
    Tile at(int x,int y) const { return inside(x,y)?tiles[size_t(y*width+x)]:Tile::Wall; }
    void set(int x,int y,Tile t){ if(inside(x,y))tiles[size_t(y*width+x)]=t; }
    char glyph(int x,int y) const { return inside(x,y)?glyphs[size_t(y*width+x)]:'#'; }
    bool blocked(int x,int y) const {
        Tile t=at(x,y);
        return t==Tile::Wall||t==Tile::Water||t==Tile::Prop||t==Tile::LowProp||t==Tile::Door;
    }
    // Water and low props (barrels, carts) do not stop arrows or sight; walls and tall props do.
    bool opaque(int x,int y) const {
        Tile t=at(x,y);
        return t==Tile::Wall||t==Tile::Prop||t==Tile::Door;
    }
    // Ghosts drift through props and doors, but never leave the map or cross solid rock.
    bool blockedForSpirit(int x,int y) const { Tile t=at(x,y); return !inside(x,y)||t==Tile::Wall||t==Tile::Water; }
    int extraStepCost(int x,int y) const {
        Tile t=at(x,y);
        return t==Tile::Web?2:(t==Tile::Shallow?1:0);
    }
    glm::vec3 center(Cell c) const { return {originX+(float(c.x)+.5f)*cellSize,0,originZ+(float(c.y)+.5f)*cellSize}; }
    Cell cellAt(glm::vec3 p) const {
        return {int(std::floor((p.x-originX)/cellSize)),int(std::floor((p.z-originZ)/cellSize))};
    }
    bool lineOfSight(Cell from, Cell to) const {
        if(opaque(from.x,from.y)||opaque(to.x,to.y))return false;
        // Supercover traversal: a ray through a corner cannot pass between two blockers.
        int dx=to.x-from.x,dy=to.y-from.y,nx=std::abs(dx),ny=std::abs(dy);
        int sx=dx>0?1:-1,sy=dy>0?1:-1,ix=0,iy=0;Cell p=from;
        while(ix<nx||iy<ny){
            int decision=(1+2*ix)*ny-(1+2*iy)*nx;
            if(decision==0){
                if(opaque(p.x+sx,p.y)||opaque(p.x,p.y+sy))return false;
                p.x+=sx;p.y+=sy;++ix;++iy;
            }else if(decision<0){p.x+=sx;++ix;}else{p.y+=sy;++iy;}
            if(p!=to&&opaque(p.x,p.y))return false;
        }
        return true;
    }
};

// Builds the original 20 x 16 crypt room from scene_layout (used by the throne crypt and tests).
Map layoutRoom();
}
