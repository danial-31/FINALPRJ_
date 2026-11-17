#pragma once
#include <glad/gl.h>
#include <GLFW/glfw3.h>
#include "audio.hpp"
#include "combat_visuals.hpp"
#include "hero_animation.hpp"
#include "hero_fall.hpp"
#include "renderer.hpp"
#include "rpg.hpp"
#include "settings.hpp"
#include "ui.hpp"
#include <array>
#include <deque>
#include <memory>

enum class Screen { Menu, ClassSelect, Game };
enum class Window { None, Inventory, Character, Journal, Map, Shop, Craft, Smith, Shrine, Travel, Dialog, Note, Reward,
                    Pause, Settings, Help, Load, Save, Defeat, Epilogue, Ending, Credits, ConfirmNew, ConfirmMenu };

struct Floater { std::string text; glm::vec3 position; float time; glm::vec4 color; bool impactSound=false; };

struct LaunchOptions {
    bool smoke=false, benchmark=false;
    std::filesystem::path capture;
    std::string zone, cls="warrior";
    bool battle=false, menu=false, technique=true, hideUi=false;
    float time=4.f;
    std::string view, window;
};

class App {
public:
    App(GLFWwindow* window, const fs::path& root, const LaunchOptions& options);
    ~App();
    int run();
private:
    GLFWwindow* window;
    fs::path root, saveDir;
    LaunchOptions options;
    Settings settings;
    std::unique_ptr<Renderer> renderer;
    std::unique_ptr<Audio> audio;
    Ui ui;
    Screen screen=Screen::Menu;
    Window win=Window::None;
    Window displayedWindow=Window::None;
    float windowOpenedAt=0;
    std::vector<Window> stack;           // windows to return to (settings from pause, etc.)
    rpg::Campaign game;
    rpg::Campaign menuWorld;
    ZoneVisuals visuals;
    std::uint32_t visualHash=0;
    std::string visualZone;
    std::unique_ptr<GpuMesh> dynamicGpu, heroGpu;
    Mesh dynamicMesh, heroMesh;
    CombatVisuals combatVisuals;
    std::vector<Particle> combatParticles;
    // presentation state
    glm::vec3 heroPos{0}; float heroYaw=0, gait=0, moveBlend=0;
    heroAnimation::Action heroAction;
    heroAnimation::Fall heroFall;
    std::vector<glm::vec3> actorPos; std::vector<float> actorYaw;
    struct ActorGait { float phase=0, weight=0, deathSeconds=-1.f, impactAt=-1000.f; };
    std::vector<ActorGait> actorGait;
    Window pendingBossWindow=Window::None;
    float bossWindowAt=0;
    int pendingInteract=-1;
    glm::vec3 clickPoint{0}; float clickTimer=0; bool clickAttack=false;
    world::Cell hover{-1,-1}; glm::vec3 hoverPoint{0}; bool hoverValid=false;
    int hoverActor=-1, hoverEntity=-1, hoverAbility=-1;
    float rightHold=0;
    glm::vec3 lastHeroPos{0};
    std::vector<Floater> floaters;
    std::vector<std::string> battleLog;
    // camera
    float yaw=.62f, pitch=.86f, zoom=10.5f, camYaw=.62f, camPitch=.86f, camZoom=10.5f;
    glm::vec3 focus{0}, camFocus{0};
    bool freeCamera=false; glm::vec3 freePos{0,12,20}; float freeYaw=0, freePitch=-.5f;
    float lightAngle=.95f, lightElevation=.95f;
    glm::mat4 view{1}, projection{1};
    glm::vec3 cameraPos{0};
    // timing
    double lastTime=0; float animTime=0, realTime=0, fps=0; int fpsFrames=0; float fpsTimer=0;
    float fade=1, fadeTarget=1;
    bool hudVisible=true, showGrid=true, animPaused=false;
    // input edges
    std::array<bool,GLFW_KEY_LAST+1> keys{};
    bool lastLeft=false, lastRight=false, lastMiddle=false;
    double lastMouseX=0, lastMouseY=0;
    float scroll=0;
    // window-specific state
    int selected=-1, dialogLine=0, slotConfirm=-1, settingsResolution=-1;
    int inventoryPage=0;
    std::string shopNpc, status, team="Daniyal Tapishev", topic="Тема уточняется (см. assets/team.txt)";
    std::string defeatMessage;
    int lastToastCount=0;
    std::uint32_t lastEntityHash=0;
    int frame=0;

    // core
    void frameStep(float dt);
    bool key(int k);
    bool held(int k) const;
    void startNewGame(combat::Class c);
    bool loadSlot(const fs::path& path);
    fs::path slotPath(int slot) const;
    void autosave();
    void syncActors(bool snap,float dt=0);
    void refreshVisuals(bool force);
    void updateCamera(float dt);
    void pick();
    void gameInput(float dt);
    void followHero(float dt);
    void handleRequests();
    void consumeEvents();
    void buildDynamic(float time);
    void rightClick();
    void castSlot(int slot);
    void drinkPotion();
    void interactAt(int entity);
    void setWindow(Window w);
    void closeWindow();
    void applySettings(bool windowMode);
    void applyMusic();
    rpg::Campaign& world();
    // ui
    void drawMenu();
    void drawClassSelect();
    void drawHud();
    void drawBattleHud();
    void drawWindowShortcuts(float x,float y,bool compact);
    bool overActionBar() const;
    void drawWindows();
    void drawInventory(bool shop);
    void drawCharacter();
    void drawJournal();
    void drawMap();
    void drawCraft();
    void drawSmith();
    void drawShrine();
    void drawTravel();
    void drawDialog();
    void drawNote();
    void drawReward();
    void drawPause();
    void drawSettings();
    void drawHelp(bool credits);
    void drawSlots(bool saving);
    void drawDefeat();
    void drawEpilogue(bool ending);
    void drawConfirm(bool newGame);
    void drawTechniqueBadge();
    void itemDetails(float x, float y, float w, const rpg::Stack& s, bool compare);
    glm::vec2 project(glm::vec3 p) const;
    void capture(const fs::path& path, int w, int h);
};
