#pragma once
#include "scene_layout.hpp"
#include <algorithm>
#include <cmath>

// Continuous movement against the same full cells used by the tactical layout.
inline bool canMove(glm::vec3 p)
{
    constexpr float radius = .44f;
    if (!std::isfinite(p.x) || !std::isfinite(p.z)) return false;
    if (p.x < layout::originX + layout::cellSize + radius ||
        p.x > layout::originX + (layout::columns-1)*layout::cellSize - radius ||
        p.z < layout::originZ + layout::cellSize + radius ||
        p.z > layout::originZ + (layout::rows-1)*layout::cellSize - radius) return false;
    for (const auto& f : layout::footprints) {
        const float x0 = layout::originX + float(f.col)*layout::cellSize;
        const float z0 = layout::originZ + float(f.row)*layout::cellSize;
        const float dx = p.x - std::clamp(p.x,x0,x0+float(f.width)*layout::cellSize);
        const float dz = p.z - std::clamp(p.z,z0,z0+float(f.depth)*layout::cellSize);
        if (dx*dx + dz*dz < radius*radius) return false;
    }
    return true;
}
