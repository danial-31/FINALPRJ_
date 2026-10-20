#pragma once
#include <glm/glm.hpp>
#include <array>

// One shared layout connects floor cells, rendered objects and movement.
namespace layout {
inline constexpr float cellSize = 1.4f;
inline constexpr int columns = 20, rows = 16;
inline constexpr float originX = -14.0f, originZ = -11.2f;
struct Footprint { int col, row, width, depth; };
inline constexpr Footprint altar{8,1,4,3}, statueLeft{6,2,1,1}, statueRight{13,2,1,1};
inline constexpr Footprint chest{16,4,2,1}, tomb{2,3,1,3};
inline constexpr Footprint coverLeft{4,9,3,1}, coverRight{13,11,3,1};
inline constexpr Footprint brazier0{4,3,1,1}, brazier1{15,3,1,1};
inline constexpr Footprint brazier2{2,9,1,1}, brazier3{17,9,1,1};
inline constexpr Footprint barrel0{2,12,1,1}, barrel1{3,13,1,1}, crates{4,12,1,1};
inline constexpr Footprint urn0{1,7,1,1}, urn1{18,2,1,1}, books{5,13,1,1};
inline constexpr std::array<Footprint,17> footprints = {altar,statueLeft,statueRight,
    chest,tomb,coverLeft,coverRight,brazier0,brazier1,brazier2,brazier3,
    barrel0,barrel1,crates,urn0,urn1,books};
inline glm::vec3 center(Footprint f) {
    return {originX + (float(f.col) + float(f.width)*.5f)*cellSize, 0,
            originZ + (float(f.row) + float(f.depth)*.5f)*cellSize};
}
inline constexpr bool contains(Footprint f, int col, int row) {
    return col >= f.col && col < f.col+f.width && row >= f.row && row < f.row+f.depth;
}
inline constexpr bool isBlocked(int col, int row) {
    if (col <= 0 || row <= 0 || col >= columns-1 || row >= rows-1) return true;
    for (const auto& f : footprints) if (contains(f,col,row)) return true;
    return false;
}
}

