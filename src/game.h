// game.h — entities, state and simulation for NEON ARENA
#pragma once
#include "core.h"

enum class GState { Menu, Playing, Paused, GameOver };

enum EnemyType { E_GRUNT = 0, E_SHOOTER, E_HEAVY, E_TYPES };

struct Player {
    Vec3 pos{0, 0.9f, 20};       // feet at pos.y - halfY... stored as center of AABB (half.y=0.9)
    Vec3 vel;
    float yaw = 0, pitch = 0;
    float hp = 100, maxHp = 100;
    int ammo = 30, reserve = 120, magSize = 30;
    float fireCd = 0, reloadT = 0;
    float bobPhase = 0, walkDist = 0;
    float roll = 0;               // bodycam strafe tilt
    float recoil = 0;             // recoil kick (visual pitch offset)
    float stepDist = 0;
    float lastDamageT = 10;       // seconds since last damage
    float muzzleT = 0;            // muzzle flash timer
    bool reloadClick1 = false, reloadClick2 = false;
    float adsT = 0;               // aim-down-sights 0..1 (bodycam aim)
    float gunKick = 0;            // viewmodel kick
    bool grounded = true;
    bool alive = true;
};

struct Enemy {
    EnemyType type = E_GRUNT;
    Vec3 pos, vel;
    float yaw = 0;
    float hp = 30, maxHp = 30;
    float radius = 0.45f, height = 1.9f;
    float attackCd = 1.0f;
    float walkPhase = 0;
    float hitFlash = 0;
    float spawnT = 0.6f;          // materialize timer
    float deathT = 0;             // >0 while dying
    float strafeDir = 1;
    float strafeT = 0;
    float telegraphT = 0;         // shooter windup
    float burstLeft = 0, burstCd = 0;
    int mag = 8, magSize = 8;     // rifle magazine for armed hostiles
    float reloadT = 0;            // >0 while performing a tactical reload
    float flinchT = 0;            // staggers aim when hit
    float animSpeed = 0;
    bool dying = false;
};

struct Projectile {
    Vec3 pos, vel;
    float ttl = 3;
};

struct Pickup {
    Vec3 pos;
    bool health = true;
    bool active = true;
    float respawnT = 0;
    float spin = 0;
};

struct Particle {
    Vec3 pos, vel;
    float life = 0, maxLife = 1;
    float size = 0.1f;
    Vec3 color;
    float drag = 0.9f;      // per-second velocity retention
    float gravity = 0;
    float fadePow = 1.0f;
};

struct Tracer {
    Vec3 from, to;
    float life = 0.07f;
};

struct Popup {                // floating score text in world space
    Vec3 pos;
    std::string text;
    Vec3 color;
    float ttl = 1.0f;
};

struct FeedItem {             // kill feed
    std::string text;
    float ttl = 4.0f;
};

struct Input {
    bool key[350] = {};
    bool keyPressed[350] = {};   // edge-triggered this frame
    bool mouseDown[8] = {};
    bool mousePressed[8] = {};
    float mouseDX = 0, mouseDY = 0;
    float wheel = 0;
};

struct Game {
    GState state = GState::Menu;
    Player player;
    std::vector<Enemy> enemies;
    std::vector<Projectile> projectiles;
    std::vector<Pickup> pickups;
    std::vector<Particle> particles;
    std::vector<Tracer> tracers;
    std::vector<Popup> popups;
    std::vector<FeedItem> feed;

    int wave = 0;
    int waveAlive = 0, waveTotal = 0;
    float waveStateT = 2.0f;         // intermission countdown before next wave
    bool waveActive = false;
    int spawnRemaining = 0;
    float spawnCd = 0;
    std::vector<EnemyType> spawnQueue;

    int score = 0;
    int combo = 0;
    float comboT = 0;
    int kills = 0, headshots = 0;
    int shotsFired = 0, shotsHit = 0;

    float hitMarkerT = 0;
    bool hitMarkerHead = false;
    float damageFlash = 0;
    float shake = 0;
    float sens = 0.0023f;
    float deathT = 0;
    float bannerT = 0;
    std::string banner;
    double time = 0;
};

namespace game {
void reset(Game& g);                          // fresh run
void update(Game& g, const Input& in, float dt);
void spawnParticles(Game& g, Vec3 pos, int n, Vec3 color, float speed, float size, float life, float gravity, float drag);
void startNextWave(Game& g);
}

// Shared player-space helpers (used by both simulation and rendering)
inline Vec3 forwardOf(const Player& p) {
    float cp = cosf(p.pitch), sp = sinf(p.pitch);
    return {sinf(p.yaw) * cp, sp, -cosf(p.yaw) * cp};
}
inline Vec3 rightOf(const Player& p) {
    return norm(cross(forwardOf(p), Vec3(0, 1, 0)));
}
inline Vec3 flatForward(const Player& p) {
    Vec3 f = forwardOf(p);
    return norm(Vec3(f.x, 0, f.z));
}
inline Vec3 eyeOf(const Player& p) { return p.pos + Vec3(0, 0.72f, 0); } // ~1.62 above feet
