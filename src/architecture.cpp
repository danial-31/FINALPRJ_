#include "scene.hpp"
#include "scene_layout.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <random>
#include <utility>
#include <vector>

namespace {
constexpr float pi = 3.14159265359f;

struct Opening {
    float center;
    float radius;
    float spring;
};

glm::vec3 wallPoint(float u, float y, float depth, bool left)
{
    return left ? glm::vec3(-14.0f + depth, y, u)
                : glm::vec3(u, y, -11.2f + depth);
}

// Each voussoir is a real six-sided stone, including its recessed inner surface.
void prism(Mesh& mesh, const std::array<glm::vec3, 8>& p, glm::vec3 color, float material)
{
    constexpr int faces[6][4] = {
        {0, 1, 2, 3}, {7, 6, 5, 4}, {0, 4, 5, 1},
        {1, 5, 6, 2}, {2, 6, 7, 3}, {3, 7, 4, 0}
    };
    glm::vec3 center(0.0f);
    for (const auto& point : p) center += point / 8.0f;
    for (const auto& f : faces) {
        auto a = p[f[0]], b = p[f[1]], c = p[f[2]], d = p[f[3]];
        const auto middle = (a + b + c + d) * 0.25f;
        if (glm::dot(glm::cross(b - a, c - a), middle - center) < 0.0f)
            std::swap(b, d);
        mesh.quad(a, b, c, d, color, material);
    }
}

glm::vec2 archPoint(float radius, float t)
{
    if (t <= 0.5f) {
        const float angle = pi - t * (2.0f * pi / 3.0f);
        return {radius + 2.0f * radius * std::cos(angle),
                2.0f * radius * std::sin(angle)};
    }
    const float angle = (1.0f - t) * (2.0f * pi / 3.0f);
    return {-radius + 2.0f * radius * std::cos(angle),
            2.0f * radius * std::sin(angle)};
}

void archBand(Mesh& mesh, const Opening& opening, bool left,
              float thickness, float depth, float front, glm::vec3 color)
{
    constexpr int stones = 18;
    for (int i = 0; i < stones; ++i) {
        const float a = (static_cast<float>(i) + 0.028f) / stones;
        const float b = (static_cast<float>(i + 1) - 0.028f) / stones;
        const auto i0 = archPoint(opening.radius, a);
        const auto i1 = archPoint(opening.radius, b);
        const auto o0 = archPoint(opening.radius + thickness, a);
        const auto o1 = archPoint(opening.radius + thickness, b);
        std::array<glm::vec3, 8> points;
        const glm::vec2 contour[4] = {i0, i1, o1, o0};
        for (int j = 0; j < 4; ++j) {
            points[j] = wallPoint(opening.center + contour[j].x,
                                  opening.spring + contour[j].y, front, left);
            points[j + 4] = wallPoint(opening.center + contour[j].x,
                                      opening.spring + contour[j].y, front - depth, left);
        }
        const float variation = 0.94f + 0.08f * std::sin(i * 2.71f);
        prism(mesh, points, color * variation, 0.0f);
    }
}

float openingWidthAt(const Opening& opening, float y)
{
    if (y < opening.spring) return opening.radius;
    const float h = y - opening.spring;
    if (h >= std::sqrt(3.0f) * opening.radius) return 0.0f;
    return std::sqrt(4.0f * opening.radius * opening.radius - h * h) - opening.radius;
}

void masonry(Mesh& mesh, bool left, float lo, float hi,
             const std::vector<Opening>& openings, std::mt19937& random)
{
    std::uniform_real_distribution<float> shade(0.85f, 1.15f);
    constexpr float rowHeight = 0.72f;
    constexpr float stoneWidth = 1.68f;
    for (int row = 0; row < 9; ++row) {
        const float y = rowHeight * (static_cast<float>(row) + 0.5f);
        const float shift = (row % 2) * stoneWidth * 0.5f;
        for (float x = lo - stoneWidth + shift; x < hi; x += stoneWidth) {
            std::vector<std::pair<float, float>> pieces = {
                {std::max(lo, x + 0.019f), std::min(hi, x + stoneWidth - 0.019f)}
            };
            for (const auto& opening : openings) {
                const float half = openingWidthAt(opening, std::max(0.0f, y - rowHeight * 0.2f));
                if (half <= 0.0f) continue;
                std::vector<std::pair<float, float>> clipped;
                for (auto piece : pieces) {
                    const float cutA = opening.center - half;
                    const float cutB = opening.center + half;
                    if (piece.second <= cutA || piece.first >= cutB) clipped.push_back(piece);
                    else {
                        if (piece.first < cutA) clipped.emplace_back(piece.first, cutA);
                        if (piece.second > cutB) clipped.emplace_back(cutB, piece.second);
                    }
                }
                pieces = std::move(clipped);
            }
            for (auto piece : pieces) {
                const float width = piece.second - piece.first;
                if (width < 0.08f) continue;
                const float middle = (piece.first + piece.second) * 0.5f;
                const float loss = row == 8 && std::sin(middle * 1.7f) > 0.4f ? 0.16f : 0.0f;
                const auto size = left ? glm::vec3(0.56f, rowHeight - 0.035f - loss, width)
                                       : glm::vec3(width, rowHeight - 0.035f - loss, 0.56f);
                mesh.block(wallPoint(middle, y - loss * 0.5f, 0.0f, left), size, 0.035f,
                           glm::vec3(0.30f, 0.31f, 0.39f) * shade(random), 0.0f);
            }
        }
    }
}

void framedOpening(Mesh& mesh, const Opening& opening, bool left, bool shrine)
{
    const glm::vec3 stone(0.44f, 0.43f, 0.49f);
    const glm::vec3 darkStone(0.22f, 0.24f, 0.32f);
    const float r = opening.radius;
    const float top = opening.spring + std::sqrt(3.0f) * r;
    // Dark backing is recessed behind the wall, keeping the opening visibly deep.
    mesh.box(wallPoint(opening.center, top * 0.5f, -0.34f, left),
             left ? glm::vec3(0.08f, top, r * 2.0f + 0.12f)
                  : glm::vec3(r * 2.0f + 0.12f, top, 0.08f),
             glm::vec3(0.065f, 0.083f, 0.095f), 0.0f);
    archBand(mesh, opening, left, 0.29f, 0.72f, 0.42f, stone);
    archBand(mesh, {opening.center, r + 0.32f, opening.spring}, left,
             0.08f, 0.15f, 0.47f, darkStone);
    for (float sign : {-1.0f, 1.0f}) {
        const float u = opening.center + sign * (r + 0.145f);
        for (int j = 0; j < 5; ++j) {
            const float h = opening.spring / 5.0f;
            mesh.block(wallPoint(u, h * (j + 0.5f), 0.12f, left),
                       left ? glm::vec3(0.62f, h - 0.025f, 0.29f)
                            : glm::vec3(0.29f, h - 0.025f, 0.62f),
                       0.018f, stone * (j % 2 ? 0.93f : 1.0f), 0.0f);
        }
        mesh.block(wallPoint(u, 0.11f, 0.22f, left),
                   left ? glm::vec3(0.79f, 0.22f, 0.51f)
                        : glm::vec3(0.51f, 0.22f, 0.79f),
                   0.025f, darkStone, 0.0f);
        mesh.block(wallPoint(u, opening.spring - 0.05f, 0.22f, left),
                   left ? glm::vec3(0.77f, 0.18f, 0.45f)
                        : glm::vec3(0.45f, 0.18f, 0.77f),
                   0.025f, stone * 1.06f, 0.0f);
        // Narrow paired shafts reinforce the Gothic silhouette.
        mesh.cylinder(wallPoint(u + sign * 0.30f, opening.spring * 0.5f, 0.36f, left),
                      0.09f, 0.09f, opening.spring, darkStone, 0.0f, 10);
    }
    const glm::vec3 metal(0.21f, 0.23f, 0.22f);
    if (!shrine) {
        for (int i = -2; i <= 2; ++i) {
            const float u = opening.center + i * r * 0.30f;
            const float barTop = opening.spring + r * (1.25f - 0.17f * std::abs(i));
            mesh.beam(wallPoint(u, 0.18f, -0.10f, left), wallPoint(u, barTop, -0.10f, left),
                      0.025f, metal, 3.0f, 6);
        }
        for (float y : {0.60f, 1.35f, 2.05f}) {
            if (y < top - 0.4f)
                mesh.beam(wallPoint(opening.center - r + 0.08f, y, -0.08f, left),
                          wallPoint(opening.center + r - 0.08f, y, -0.08f, left),
                          0.022f, metal, 3.0f, 6);
        }
    }
}

void pilaster(Mesh& mesh, float x, float z, float height)
{
    const glm::vec3 stone(0.36f, 0.37f, 0.44f);
    mesh.block({x, 0.12f, z}, {0.90f, 0.24f, 0.90f}, 0.045f, stone * 0.82f, 0.0f);
    mesh.block({x, 0.33f, z}, {0.72f, 0.18f, 0.72f}, 0.035f, stone, 0.0f);
    const float shaftHeight = height - 1.0f;
    for (int i = 0; i < 6; ++i) {
        const float h = shaftHeight / 6.0f;
        mesh.block({x, 0.45f + h * (i + 0.5f), z}, {0.52f, h - 0.022f, 0.52f},
                   0.025f, stone * (0.92f + (i % 3) * 0.045f), 0.0f);
    }
    mesh.cylinder({x - 0.25f, height * 0.5f - 0.10f, z + 0.25f},
                  0.09f, 0.09f, height - 0.95f, stone * 1.12f, 0.0f, 10);
    mesh.cylinder({x + 0.25f, height * 0.5f - 0.10f, z + 0.25f},
                  0.09f, 0.09f, height - 0.95f, stone * 1.12f, 0.0f, 10);
    mesh.block({x, height - 0.43f, z}, {0.69f, 0.20f, 0.69f}, 0.025f, stone, 0.0f);
    mesh.block({x, height - 0.22f, z}, {0.89f, 0.20f, 0.89f}, 0.035f, stone * 1.08f, 0.0f);
    mesh.block({x, height - 0.06f, z}, {0.98f, 0.12f, 0.98f}, 0.025f, stone * 0.78f, 0.0f);
}
} // namespace

void buildArchitecture(Mesh& mesh)
{
    std::mt19937 random(71231);
    std::uniform_real_distribution<float> shade(0.87f, 1.13f);
    std::uniform_real_distribution<float> unit(0.0f, 1.0f);

    // Broad, bevelled forms and restrained joints keep the room readable at game scale.
    mesh.block({0.0f, -0.83f, 0.0f}, {29.0f, 0.42f, 23.4f}, 0.20f,
               {0.12f, 0.13f, 0.20f}, 0.0f);
    mesh.block({0.0f, -0.47f, 0.0f}, {28.65f, 0.34f, 23.05f}, 0.14f,
               {0.24f, 0.24f, 0.32f}, 0.0f);
    mesh.box({0.0f, -0.22f, 0.0f}, {28.0f, 0.30f, 22.4f},
             {0.12f, 0.13f, 0.19f}, 0.0f);
    const glm::vec3 floorPalette[] = {
        {0.31f, 0.34f, 0.43f}, {0.32f, 0.30f, 0.39f},
        {0.35f, 0.33f, 0.39f}, {0.28f, 0.32f, 0.41f},
        {0.34f, 0.35f, 0.41f}
    };
    for (int row = 0; row < 16; ++row) {
        for (int col = 0; col < 20; ++col) {
            const float x = -13.3f + float(col) * 1.4f;
            const float z = -10.5f + float(row) * 1.4f;
            const float bevel = 0.045f + unit(random) * 0.025f;
            const auto color = floorPalette[(row * 7 + col * 3 + int(random() % 5)) % 5] * shade(random);
            mesh.block({x, -0.078f, z}, {1.36f, 0.156f, 1.36f}, bevel, color, 1.0f);
            // Occasional broad edge chips and short fractures break the tiled regularity.
            if ((row * 13 + col * 7) % 10 == 0) {
                const float sign = (col % 2 == 0) ? 1.0f : -1.0f;
                const glm::vec3 a{x - .48f, .004f, z + sign * .58f};
                const glm::vec3 b{x - .12f, .004f, z + sign * .58f};
                const glm::vec3 c{x - .36f, .004f, z + sign * .35f};
                mesh.triangle(a, sign > 0 ? b : c, sign > 0 ? c : b,
                              color * .65f, 1.0f);
                mesh.beam({x+.61f,.006f,z-.20f}, {x+.30f,.006f,z-.04f},
                          .012f, color*.58f, 1.0f, 4);
                mesh.beam({x+.30f,.006f,z-.04f}, {x+.24f,.006f,z+.20f},
                          .009f, color*.58f, 1.0f, 4);
            }
        }
    }

    // Muted bronze and dark inset borders frame the play area without busy ornament.
    for (float x : {-13.35f, 13.35f}) {
        mesh.box({x, 0.012f, 0}, {0.055f, 0.025f, 20.7f}, {0.35f, 0.28f, 0.19f}, 4.0f);
        mesh.box({x * 0.976f, 0.008f, 0}, {0.10f, 0.018f, 20.15f}, {0.17f, 0.18f, 0.25f}, 1.0f);
    }
    for (float z : {-10.35f, 10.35f}) {
        mesh.box({0, 0.012f, z}, {26.75f, 0.025f, 0.055f}, {0.35f, 0.28f, 0.19f}, 4.0f);
        mesh.box({0, 0.008f, z * 0.97f}, {26.1f, 0.018f, 0.10f}, {0.17f, 0.18f, 0.25f}, 1.0f);
    }

    const std::vector<Opening> back = {
        {-9.1f, 1.18f, 2.50f}, {0.0f, 1.90f, 2.60f}, {9.1f, 1.18f, 2.50f}
    };
    const std::vector<Opening> left = {
        {-5.0f, 1.35f, 2.40f}, {3.6f, 1.35f, 2.40f}
    };
    masonry(mesh, false, -14.0f, 14.0f, back, random);
    masonry(mesh, true, -10.92f, 11.2f, left, random);
    for (const auto& opening : back)
        framedOpening(mesh, opening, false, opening.center == 0.0f);
    for (const auto& opening : left)
        framedOpening(mesh, opening, true, false);

    for (int i = 0; i < 20; ++i) {
        const float x = -13.3f + float(i) * 1.4f;
        if (i == 3 || i == 12 || i == 16) {
            mesh.block({x-.20f, 6.39f, -11.2f}, {.83f,.19f,.77f}, .075f,
                       {.38f,.38f,.45f},0.0f,.07f);
            continue;
        }
        mesh.block({x, 6.49f, -11.2f}, {1.37f, 0.20f, 0.88f}, 0.045f,
                   {0.38f, 0.38f, 0.45f}, 0.0f);
        mesh.block({x, 6.67f, -11.2f}, {1.37f, 0.16f, 1.02f}, 0.035f,
                   {0.26f, 0.26f, 0.35f}, 0.0f);
        if (i == 1 || i == 7 || i == 14 || i == 18)
            mesh.block({x+.16f, 6.86f, -11.2f}, {.82f,.30f,.61f}, .09f,
                       {.30f,.31f,.39f},0.0f,-.06f);
    }
    for (int i = 0; i < 16; ++i) {
        const float z = -10.5f + float(i) * 1.4f;
        if (i == 2 || i == 8 || i == 13) {
            mesh.block({-14.0f, 6.37f, z+.18f}, {.77f,.20f,.75f}, .07f,
                       {.38f,.38f,.45f},0.0f,-.09f);
            continue;
        }
        mesh.block({-14.0f, 6.49f, z}, {0.88f, 0.20f, 1.37f}, 0.045f,
                   {0.38f, 0.38f, 0.45f}, 0.0f);
        mesh.block({-14.0f, 6.67f, z}, {1.02f, 0.16f, 1.37f}, 0.035f,
                   {0.26f, 0.26f, 0.35f}, 0.0f);
        if (i == 4 || i == 10 || i == 15)
            mesh.block({-14.0f, 6.89f, z-.1f}, {.61f,.35f,.90f}, .09f,
                       {.30f,.31f,.39f},0.0f,.06f);
    }
    for (float x : {-13.50f, -5.80f, 5.80f, 13.50f})
        pilaster(mesh, x, -10.55f, 6.75f);
    for (float z : {-0.75f, 10.45f})
        pilaster(mesh, -13.50f, z, 6.75f);

    // All three steps remain inside the four-by-three-cell altar footprint.
    const glm::vec3 altar = layout::center(layout::altar);
    for (int step=0; step<3; ++step) {
        const float inset = .10f + float(step)*.24f;
        mesh.block(altar + glm::vec3(0,.07f + float(step)*.14f,0),
                   {layout::altar.width*layout::cellSize-inset,.14f,
                    layout::altar.depth*layout::cellSize-inset},.055f,
                   glm::vec3(.31f,.32f,.39f)+glm::vec3(float(step)*.035f),1.0f);
    }
    auto brokenWall = [&](float cx, float z, bool reverse) {
        const glm::vec3 color(0.34f, 0.32f, 0.40f);
        // Solid broad lower course and stepped upper fragments form useful low cover.
        for (int i = 0; i < 4; ++i) {
            const float x = cx - 1.575f + float(i)*1.05f;
            mesh.block({x, 0.25f, z}, {0.95f, 0.50f, 1.30f}, 0.07f,
                       color * shade(random), 0.0f);
            const int index = reverse ? 3 - i : i;
            if (index == 0) {
                // The surviving end pier gives the broken wall a recognizable silhouette.
                mesh.block({x,.69f,z}, {.88f,.36f,.84f},.07f,color*.91f,0.0f);
                mesh.block({x+.035f,1.12f,z}, {.73f,.50f,.73f},.07f,color,0.0f,-.03f);
                mesh.block({x-.025f,1.53f,z}, {.77f,.28f,.76f},.08f,color*.94f,0.0f,.025f);
                mesh.block({x,1.75f,z}, {.89f,.18f,.87f},.07f,color*1.06f,0.0f);
                mesh.beam({x-.12f,1.30f,z+.38f},{x+.11f,1.06f,z+.38f},
                          .018f,color*.56f,0.0f,4);
                continue;
            }
            if (index < 3) {
                const float height = index == 1 ? 0.63f : 0.32f;
                mesh.block({x, 0.50f + height * 0.5f, z}, {0.94f, height, 0.83f}, 0.08f,
                           color * shade(random), 0.0f, (unit(random) - 0.5f) * 0.07f);
            }
        }
    };
    const auto leftCover = layout::center(layout::coverLeft);
    const auto rightCover = layout::center(layout::coverRight);
    brokenWall(leftCover.x, leftCover.z, false);
    brokenWall(rightCover.x, rightCover.z, true);
    // Ancient roots hug the closed wall, outside the hero's existing boundary.
    const glm::vec3 bark(.20f,.18f,.19f);
    for (float z : {-8.2f, 7.5f}) {
        const glm::vec3 points[] = {{-13.68f,.08f,z-.65f},{-13.56f,.55f,z-.28f},
            {-13.60f,1.35f,z},{-13.68f,2.3f,z+.19f},{-13.69f,3.25f,z-.12f},
            {-13.71f,4.2f,z+.22f},{-13.72f,5.25f,z+.38f}};
        for (int j=0;j<6;++j)
            mesh.beam(points[j],points[j+1],.16f-float(j)*.019f,bark,2.0f,7);
        mesh.beam(points[1],{-13.61f,.04f,z+.85f},.095f,bark,2.0f,7);
        mesh.beam(points[3],{-13.68f,2.95f,z-.66f},.068f,bark,2.0f,6);
        mesh.beam({-13.68f,2.95f,z-.66f},{-13.71f,3.70f,z-.93f},.043f,bark,2.0f,6);
    }

    // Short wall ends preserve the open foreground and show the room's thickness.
    mesh.block({13.75f, 0.30f, -10.4f}, {0.70f, 0.60f, 1.20f}, 0.08f,
               {0.30f, 0.30f, 0.38f}, 0.0f);
    mesh.block({13.75f, 0.17f, -9.35f}, {0.70f, 0.34f, 0.82f}, 0.07f,
               {0.27f, 0.28f, 0.35f}, 0.0f);

    // Sparse, broad moss patches sit at damp wall seams, leaving play space clean.
    for (int i = 0; i < 26; ++i) {
        const float x = -12.8f + unit(random) * 25.6f;
        if (std::abs(x) < 3.7f) continue;
        const float r = 0.08f + unit(random) * 0.15f;
        mesh.sphere({x, 0.018f, -10.59f + unit(random) * 0.13f},
                    {r * 1.8f, 0.025f, r}, {0.16f, 0.20f, 0.15f}, 9.0f, 6, 3);
    }
    for (int i = 0; i < 20; ++i) {
        const float z = -9.7f + unit(random) * 19.4f;
        const float r = 0.08f + unit(random) * 0.13f;
        mesh.sphere({-13.60f + unit(random) * 0.12f, 0.018f, z},
                    {r, 0.025f, r * 1.8f}, {0.15f, 0.19f, 0.14f}, 9.0f, 6, 3);
    }
}



