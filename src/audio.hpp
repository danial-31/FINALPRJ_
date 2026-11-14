#pragma once
#include <memory>
#include <string>

// Procedurally synthesised sound effects and ambient music, mixed on the miniaudio device thread.
// No third-party audio files are used, so there are no asset licences to track.
enum class Sfx { Step, Hit, HeroHit, Loot, Chest, Shrine, Click, LevelUp, Warn, Magic, Defeat, Door, Victory };
enum class Music { None, Menu, Village, Explore, Dungeon, Combat, Boss };

class Audio {
public:
    Audio();
    ~Audio();
    bool ok() const;
    void play(Sfx s, float volume=1.f);
    void music(Music m);
    void volumes(float master, float music, float sfx);
private:
    struct Impl;
    std::unique_ptr<Impl> impl;
};
