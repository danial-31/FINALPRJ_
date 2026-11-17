#pragma once
#include <glad/gl.h>
#include "scene.hpp"
#include "zone_mesh.hpp"
#include <filesystem>
#include <array>
#include <memory>
#include <string>
#include <vector>

namespace fs = std::filesystem;

// Shader programs are read from shaders/*.vert|frag next to the executable.
// Any missing file, compile or link error throws with the file name and the driver log.
GLuint loadProgram(const fs::path& dir, const std::string& vertex, const std::string& fragment);
void setMatrix(GLuint p, const char* name, const glm::mat4& m);
void setVec3(GLuint p, const char* name, glm::vec3 v);
void setFloat(GLuint p, const char* name, float v);
void setInt(GLuint p, const char* name, int v);
void checkFramebuffer(const char* what);

struct GpuMesh {
    GLuint vao=0, vbo=0; GLsizei count=0;
    GpuMesh();
    explicit GpuMesh(const Mesh& m);
    ~GpuMesh();
    GpuMesh(const GpuMesh&)=delete; GpuMesh& operator=(const GpuMesh&)=delete;
    void update(const Mesh& m);
    void draw() const;
};

struct Texture {
    GLuint id=0; int width=0, height=0;
    bool load(const fs::path& path, bool repeat);
    ~Texture();
};

struct Particle { glm::vec3 position, color; float size, alpha, kind; };

struct RenderSettings {
    bool technique=true;        // headline technique (soft shadows) on/off
    bool perspective=true;
    int shadowQuality=2;        // 0 off, 1 low (1024), 2 high (2048)
    int effects=2;              // 0 low, 1 medium, 2 high (bloom, ink, particles)
    bool pauseAnimation=false;
    bool tiltShift=true;
};

struct FrameInput {
    glm::mat4 view, projection;
    glm::vec3 cameraPos, focus, heroPos;
    float time;                 // animation time (frozen when paused)
    float near, far;
    float fovY=.5236f, orthoHalf=10.f;
    int width, height;
    float fade=1;
    glm::vec3 keyDirection;
    bool surface=true;
    int biome=0;
    float corruption=0;
    float cutaway=0;
};

struct GpuTimings {
    double pointShadowsMs=0, sunShadowMs=0, sceneMs=0, postMs=0;
    unsigned samples=0;
};

class Renderer {
public:
    explicit Renderer(const fs::path& root);
    ~Renderer();
    void setZone(const ZoneVisuals& v);
    void setZoneObjects(const ZoneVisuals& v);
    bool profileGpu=false;
    GpuTimings gpuTimings, gpuTotals; // latest sample and cumulative sums (divide by samples)
    void collectGpuTimings();         // polls available queries only; never stalls rendering
    // Draws sky, shadows, scene, dynamic meshes, particles and post-processing into the default framebuffer.
    void render(const FrameInput& f, const RenderSettings& s, const GpuMesh& dynamicMesh, const GpuMesh& heroMesh, const glm::mat4& heroModel,
                const std::vector<Particle>& extraParticles);
    std::vector<Particle> particles(float time, glm::vec3 focus, const RenderSettings& s) const;
    GLuint uiProgram=0;
    Texture font, stonePaint, actionIcons, interfaceEmblems, coverArt;
    fs::path root;
private:
    GLuint scene=0, shadow=0, pointShadow=0, blur=0, post=0, sky=0, particle=0;
    GLuint screenVao=0, particleVao=0, particleVbo=0;
    GLuint shadowFbo=0, shadowTexture=0; int shadowSize=0;
    GLuint hdrFbo=0, hdrColor=0, hdrDepth=0, blurFbo[2]{}, blurTexture[2]{}; int width=0, height=0;
    GLuint cubeFbo=0, cubeCacheFbo=0, cubes[4]{}; bool cubeCached=false;
    GpuMesh zoneMesh, zoneSolid, zoneAnimated, zoneObjects, zoneObjectSolid;
    std::vector<std::unique_ptr<GpuMesh>> animatedShadowMeshes;
    std::vector<ZoneLight> terrainLights;
    std::vector<Emitter> terrainEmitters;
    int cachedLightIds[2]{-2,-2};
    struct QueryFrame { std::array<GLuint,5> stamps{}; bool pending=false; };
    std::array<QueryFrame,4> gpuQueries{};
    unsigned queryFrame=0;
    std::vector<ZoneLight> lights;
    std::vector<Emitter> emitters;
    void resizeTargets(int w, int h);
    void resizeShadow(int size);
};
