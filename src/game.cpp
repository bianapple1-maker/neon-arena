// game.cpp — simulation: player, weapons, enemy AI, waves, scoring, juice
#include "game.h"
#include "world.h"
#include "audio.h"
#include <GLFW/glfw3.h>

std::mt19937 g_rng{12345};

static Vec3 playerHalf() { return {0.35f, 0.9f, 0.35f}; }

// ----------------------------------------------------------- FX helpers
void game::spawnParticles(Game& g, Vec3 pos, int n, Vec3 color, float speed, float size,
                          float life, float gravity, float drag) {
    for (int i = 0; i < n; i++) {
        Particle p;
        p.pos = pos;
        Vec3 d = norm(Vec3(randf(-1, 1), randf(-1, 1), randf(-1, 1)));
        p.vel = d * speed * randf(0.3f, 1.0f);
        p.maxLife = p.life = life * randf(0.6f, 1.4f);
        p.size = size * randf(0.6f, 1.3f);
        p.color = color;
        p.gravity = gravity;
        p.drag = drag;
        g.particles.push_back(p);
    }
}

static void popup(Game& g, Vec3 pos, const std::string& text, Vec3 color) {
    if ((int)g.popups.size() < 24) g.popups.push_back({pos, text, color, 1.0f});
}

static void feed(Game& g, const std::string& s) {
    g.feed.push_back({s, 4.0f});
    if (g.feed.size() > 5) g.feed.erase(g.feed.begin());
}

// ----------------------------------------------------------- enemy factory
static Enemy makeEnemy(EnemyType t, Vec3 pos) {
    Enemy e;
    e.type = t;
    switch (t) {
        case E_GRUNT:
            e.hp = e.maxHp = 30; e.radius = 0.45f; e.height = 1.9f;
            break;
        case E_SHOOTER:
            e.hp = e.maxHp = 40; e.radius = 0.42f; e.height = 1.95f;
            break;
        case E_HEAVY:
            e.hp = e.maxHp = 150; e.radius = 0.75f; e.height = 2.5f;
            break;
        default: break;
    }
    e.pos = pos + Vec3(0, e.height * 0.5f, 0); // pos = AABB center
    e.spawnT = 0.6f;
    e.attackCd = randf(0.8f, 1.6f);
    e.strafeDir = randi(0, 1) ? 1.0f : -1.0f;
    e.strafeT = randf(1, 3);
    switch (t) {
        case E_GRUNT:
            e.hp = e.maxHp = 30; e.radius = 0.45f; e.height = 1.9f;
            e.magSize = 5;
            break;
        case E_SHOOTER:
            e.hp = e.maxHp = 40; e.radius = 0.42f; e.height = 1.95f;
            e.magSize = 12;
            break;
        case E_HEAVY:
            e.hp = e.maxHp = 150; e.radius = 0.75f; e.height = 2.5f;
            e.magSize = 8;
            break;
        default: break;
    }
    e.mag = e.magSize;
    return e;
}

// ----------------------------------------------------------- scoring
static void addScore(Game& g, int base, bool headshot, Vec3 at) {
    float mult = 1.0f + std::min(g.combo, 10) * 0.1f;
    int pts = (int)(base * (headshot ? 1.5f : 1.0f) * mult);
    g.score += pts;
    char buf[16];
    snprintf(buf, sizeof(buf), "+%d", pts);
    popup(g, at + Vec3(0, 0.4f, 0), buf, headshot ? Color::yellow() : Color::white());
}

// ----------------------------------------------------------- damage
static void damagePlayer(Game& g, float dmg, Vec3 from) {
    if (!g.player.alive || g.state != GState::Playing) return;
    g.player.hp -= dmg;
    g.player.lastDamageT = 0;
    g.damageFlash = std::min(1.0f, g.damageFlash + dmg / 40.0f);
    g.shake = std::min(1.0f, g.shake + dmg / 60.0f);
    audio::play(SFX_HURT, nullptr, 0.9f, randf(0.9f, 1.1f));
    // knockback
    Vec3 dir = norm(Vec3(g.player.pos.x - from.x, 0, g.player.pos.z - from.z));
    g.player.vel += dir * std::min(dmg * 0.15f, 4.0f);

    if (g.player.hp <= 0) {
        g.player.hp = 0;
        g.player.alive = false;
        g.state = GState::GameOver;
        g.deathT = 0;
        g.combo = 0;
        audio::play(SFX_GAMEOVER, nullptr, 1.0f);
    }
}

static void damageEnemy(Game& g, Enemy& e, float dmg, bool headshot, Vec3 hitPos) {
    if (e.dying || e.spawnT > 0) return;
    e.hp -= dmg;
    e.hitFlash = 0.15f;
    e.flinchT = 0.35f; // staggers their aim and interrupts firing
    g.shotsHit++;
    g.hitMarkerT = 0.18f;
    g.hitMarkerHead = headshot;
    audio::play(headshot ? SFX_HEADSHOT : SFX_HIT, &hitPos, 0.8f, randf(0.95f, 1.05f));
    Vec3 col = headshot ? Color::yellow() : Color::red();
    game::spawnParticles(g, hitPos, headshot ? 10 : 5, col, 4, 0.06f, 0.3f, 6, 0.85f);

    if (e.hp <= 0) {
        e.dying = true;
        e.deathT = 0.9f;
        g.combo++;
        g.comboT = 3.0f;
        g.kills++;
        if (headshot) g.headshots++;
        int base = e.type == E_GRUNT ? 100 : e.type == E_SHOOTER ? 150 : 400;
        addScore(g, base, headshot, e.pos);
        audio::play(SFX_EDEATH, &e.pos, 1.0f, e.type == E_HEAVY ? 0.6f : 1.0f);
        game::spawnParticles(g, e.pos + Vec3(0, 1, 0), e.type == E_HEAVY ? 40 : 18,
                             Color::magenta() * 0.6f, 6, 0.12f, 0.6f, 8, 0.86f);
        if (e.type == E_HEAVY) {
            g.shake = std::min(1.0f, g.shake + 0.3f);
            feed(g, "HEAVY DESTROYED");
        } else if (headshot) {
            feed(g, "HEADSHOT BONUS");
        }
    } else {
        audio::play(SFX_EHURT, &e.pos, 0.5f, randf(0.9f, 1.1f));
        // small knockback
        Vec3 dir = norm(Vec3(e.pos.x - g.player.pos.x, 0, e.pos.z - g.player.pos.z));
        e.vel += dir * 1.5f;
    }
}

// ----------------------------------------------------------- weapon
static void fireWeapon(Game& g) {
    Player& p = g.player;
    if (p.ammo <= 0) {
        audio::play(SFX_DRY, nullptr, 0.5f);
        p.fireCd = 0.25f;
        return;
    }
    p.ammo--;
    p.fireCd = 0.115f;
    g.shotsFired++;
    p.muzzleT = 0.06f;
    p.gunKick = 1.0f;
    p.recoil += 0.013f;
    g.shake = std::min(1.0f, g.shake + 0.03f);
    audio::play(SFX_SHOOT, nullptr, 0.85f, randf(0.94f, 1.06f));

    Vec3 eye = eyeOf(p);
    Vec3 dir = forwardOf(p);
    // spread: tight when aiming, grows while moving / in air at the hip
    float hipSpread = 0.009f + (p.grounded ? 0.0f : 0.02f) + len(Vec2(p.vel.x, p.vel.z)) * 0.0022f;
    float spread = lerpf(hipSpread, 0.0018f, p.adsT);
    dir = norm(dir + rightOf(p) * randf(-spread, spread) +
               Vec3(0, 1, 0) * randf(-spread, spread));

    // muzzle position: hip offset blends to center line when aiming
    Vec3 muzzle = eye + dir * 0.55f + rightOf(p) * lerpf(0.25f, 0.02f, p.adsT) +
                  Vec3(0, lerpf(-0.12f, -0.02f, p.adsT), 0);

    // ejected brass casing
    {
        Particle c;
        c.pos = muzzle - dir * 0.35f + rightOf(p) * 0.06f;
        c.vel = rightOf(p) * randf(1.8f, 2.6f) + Vec3(0, randf(1.6f, 2.4f), 0) + dir * randf(0.2f, 0.6f);
        c.maxLife = c.life = randf(0.6f, 1.0f);
        c.size = 0.03f;
        c.color = Vec3(0.85f, 0.66f, 0.28f);
        c.gravity = 9.0f;
        c.drag = 0.92f;
        g.particles.push_back(c);
    }

    float maxRange = 80.0f;
    float tWorld = maxRange;
    Vec3 wallN;
    bool hitWall = world::raycast(eye, dir, maxRange, &tWorld, &wallN);

    // enemy hits
    float bestT = hitWall ? tWorld : maxRange;
    Enemy* bestE = nullptr;
    bool bestHead = false;
    for (Enemy& e : g.enemies) {
        if (e.dying || e.spawnT > 0) continue;
        // head sphere test
        Vec3 headC = e.pos + Vec3(0, e.height * 0.35f, 0); // head sphere (center pos is AABB mid)
        float headR = e.type == E_HEAVY ? 0.38f : 0.28f;
        Vec3 oc = headC - eye;
        float tca = dot(oc, dir);
        if (tca > 0 && tca < bestT) {
            float d2 = dot(oc, oc) - tca * tca;
            if (d2 < headR * headR) {
                bestT = tca;
                bestE = &e;
                bestHead = true;
                continue;
            }
        }
        // body AABB
        Vec3 mn = e.pos - Vec3(e.radius, e.height * 0.45f, e.radius);
        Vec3 mx = e.pos + Vec3(e.radius, e.height * 0.55f, e.radius);
        float tmin = 0, tmax = bestT;
        bool ok = true;
        float om[3] = {eye.x, eye.y, eye.z}, dm[3] = {dir.x, dir.y, dir.z};
        float lom[3] = {mn.x, mn.y, mn.z}, him[3] = {mx.x, mx.y, mx.z};
        for (int a = 0; a < 3 && ok; a++) {
            if (fabsf(dm[a]) < 1e-8f) {
                if (om[a] < lom[a] || om[a] > him[a]) ok = false;
                continue;
            }
            float inv = 1.0f / dm[a];
            float t1 = (lom[a] - om[a]) * inv, t2 = (him[a] - om[a]) * inv;
            if (t1 > t2) std::swap(t1, t2);
            tmin = std::max(tmin, t1);
            tmax = std::min(tmax, t2);
            if (tmin > tmax) ok = false;
        }
        if (ok && tmin > 0.01f && tmin < bestT) {
            bestT = tmin;
            bestE = &e;
            bestHead = false;
        }
    }

    Vec3 hitPoint = eye + dir * bestT;
    g.tracers.push_back({muzzle, hitPoint, 0.07f});

    if (bestE) {
        damageEnemy(g, *bestE, bestHead ? 26.0f : 13.0f, bestHead, hitPoint);
    } else if (hitWall) {
        // sparks + decal-ish puff
        game::spawnParticles(g, hitPoint + wallN * 0.05f, 6, Color::amber(), 3.5f, 0.05f, 0.35f, 7, 0.85f);
        game::spawnParticles(g, hitPoint + wallN * 0.1f, 3, Vec3(0.6f, 0.6f, 0.6f), 1.0f, 0.15f, 0.5f, -0.4f, 0.9f);
    }
    // muzzle smoke
    game::spawnParticles(g, muzzle, 2, Vec3(0.75f, 0.75f, 0.75f), 1.2f, 0.08f, 0.3f, -0.8f, 0.9f);
}

// ----------------------------------------------------------- enemy gunfire
// Shared rifle shot for all armed hostiles.
static void fireEnemyShot(Game& g, const Enemy& e, float speed, float dmg, float spreadRad) {
    Vec3 muzzle = e.pos + Vec3(0, e.height * 0.72f, 0);
    Vec3 dirToEye = norm(eyeOf(g.player) - muzzle);
    // wobble the shot so bursts feel like bodycam gunfire, not laser beams
    Vec3 side = norm(cross(dirToEye, Vec3(0, 1, 0)));
    Vec3 up = cross(side, dirToEye);
    dirToEye = norm(dirToEye + side * randf(-spreadRad, spreadRad) +
                    up * randf(-spreadRad, spreadRad));
    Projectile pr;
    pr.pos = muzzle + dirToEye * 0.7f;
    pr.vel = dirToEye * speed;
    pr.ttl = 3.0f;
    g.projectiles.push_back(pr);
    audio::play(SFX_ZAP, &e.pos, 0.75f, randf(0.85f, 1.15f));
    game::spawnParticles(g, muzzle + dirToEye * 0.8f, 2, Vec3(0.9f, 0.75f, 0.45f), 2.5f, 0.06f, 0.25f, 2, 0.85f);
}

// ----------------------------------------------------------- enemy AI
static void updateEnemy(Game& g, Enemy& e, float dt) {
    Player& p = g.player;

    if (e.spawnT > 0) {
        e.spawnT -= dt;
        if (e.spawnT <= 0) audio::play(SFX_SPAWN, &e.pos, 0.7f);
        return;
    }
    if (e.dying) {
        e.deathT -= dt;
        e.pos.y -= dt * 0.6f;
        return;
    }

    e.hitFlash = std::max(0.0f, e.hitFlash - dt);
    e.attackCd -= dt;
    e.strafeT -= dt;
    if (e.strafeT <= 0) {
        e.strafeDir = -e.strafeDir;
        e.strafeT = randf(1.2f, 3.0f);
    }

    Vec3 toP = p.pos - e.pos;
    float dist = len(Vec3(toP.x, 0, toP.z));
    Vec3 toward = norm(Vec3(toP.x, 0, toP.z));
    float speed = 0;
    Vec3 desired = Vec3(0, e.vel.y, 0);

    e.flinchT = std::max(0.0f, e.flinchT - dt);

    // tactical reload: can't fire, backpedals & strafes for cover-ish motion
    if (e.reloadT > 0) {
        e.reloadT -= dt;
        speed = 1.6f;
        Vec3 side = Vec3(-toward.z, 0, toward.x);
        desired += -toward * speed + side * e.strafeDir * speed * 0.8f;
        if (e.reloadT <= 0) {
            e.mag = e.magSize;
            audio::play(SFX_RELOAD, &e.pos, 0.7f, 1.25f); // bolt release
        }
    } else if (!p.alive) {
        // idle wander when player dead
        desired.x = sinf(g.time * 0.7f + (float)(intptr_t)&e) * 0.5f;
    } else if (e.type == E_GRUNT) {
        speed = 3.4f;
        desired += toward * speed;
        if (dist < 1.7f && e.attackCd <= 0) {
            e.attackCd = 1.1f;
            damagePlayer(g, 11, e.pos);
            e.vel += toward * 2.0f;
        }
        // pot-shots from the hip while advancing (5-round mag, then reload)
        e.burstCd -= dt;
        if (e.mag <= 0) {
            e.reloadT = randf(1.9f, 2.4f);
            audio::play(SFX_RELOAD, &e.pos, 0.7f, 0.9f);
        } else if (dist > 3.0f && dist < 17.0f && e.burstCd <= 0 && e.flinchT <= 0) {
            e.burstCd = randf(1.3f, 2.1f);
            e.mag--;
            fireEnemyShot(g, e, 19.0f, 7.0f, 0.055f);
        }
    } else if (e.type == E_SHOOTER) {
        speed = 2.6f;
        if (dist > 13) desired += toward * speed;
        else if (dist < 8) desired -= toward * speed;
        Vec3 side = Vec3(-toward.z, 0, toward.x);
        desired += side * e.strafeDir * speed * 0.7f;
        // burst fire with telegraph (12-round mag, 3-round bursts, then reload)
        if (e.mag <= 0) {
            e.reloadT = randf(2.2f, 2.7f);
            e.burstLeft = 0;
            e.telegraphT = 0;
            audio::play(SFX_RELOAD, &e.pos, 0.7f, 0.9f);
        } else if (e.burstLeft > 0) {
            e.burstCd -= dt;
            if (e.burstCd <= 0 && e.flinchT <= 0) {
                e.burstCd = 0.18f;
                e.burstLeft--;
                e.mag--;
                fireEnemyShot(g, e, 16.0f, 12.0f, 0.02f);
            }
        } else if (e.attackCd <= 0 && dist < 20) {
            e.telegraphT += dt;
            if (e.telegraphT > 0.45f) {
                e.telegraphT = 0;
                e.attackCd = randf(1.8f, 2.6f);
                e.burstLeft = 3;
                e.burstCd = 0;
            }
        } else {
            e.telegraphT = 0;
        }
    } else { // HEAVY
        speed = 1.8f;
        desired += toward * speed;
        if (dist < 2.6f && e.attackCd <= 0) {
            e.attackCd = 2.4f;
            // AoE slam
            float d = len(p.pos - e.pos);
            if (d < 4.0f) {
                damagePlayer(g, 30, e.pos);
                Vec3 kd = norm(Vec3(p.pos.x - e.pos.x, 0, p.pos.z - e.pos.z));
                p.vel += kd * 9.0f + Vec3(0, 4.0f, 0);
            }
            g.shake = std::min(1.0f, g.shake + 0.5f);
            audio::play(SFX_SLAM, &e.pos, 1.0f);
            game::spawnParticles(g, e.pos + Vec3(0, 0.2f, 0), 24, Color::amber(), 7, 0.14f, 0.5f, 9, 0.85f);
        }
    }

    // separation from other enemies
    for (Enemy& o : g.enemies) {
        if (&o == &e || o.dying || o.spawnT > 0) continue;
        Vec3 d = e.pos - o.pos;
        float l = len(Vec3(d.x, 0, d.z));
        float minD = e.radius + o.radius + 0.2f;
        if (l > 0.001f && l < minD) desired += norm(Vec3(d.x, 0, d.z)) * (minD - l) * 6.0f;
    }

    // integrate + collide (e.pos is the AABB center)
    Vec3 vel = e.vel;
    vel.x = lerpf(vel.x, desired.x, clampf(dt * 8, 0, 1));
    vel.z = lerpf(vel.z, desired.z, clampf(dt * 8, 0, 1));
    vel.y -= 20.0f * dt; // gravity

    Vec3 half(e.radius, e.height * 0.5f, e.radius);
    auto mr = world::moveAABB(e.pos, vel, half, dt);
    e.vel = mr.vel;
    e.pos = mr.pos;

    // face player
    float targetYaw = atan2f(toP.x, -toP.z);
    e.yaw = lerpAngle(e.yaw, targetYaw, clampf(dt * 10, 0, 1));

    // walk animation phase
    float hspeed = len(Vec2(e.vel.x, e.vel.z));
    e.animSpeed = lerpf(e.animSpeed, hspeed, clampf(dt * 6, 0, 1));
    e.walkPhase += hspeed * dt * 2.4f + dt * 0.5f;
}

// ----------------------------------------------------------- waves
void game::startNextWave(Game& g) {
    g.wave++;
    g.waveActive = true;
    int n = g.wave;
    int grunts = 3 + n + n / 2;
    int shooters = n >= 2 ? (n - 1) : 0;
    int heavies = (n % 4 == 0) ? n / 4 : 0;
    heavies = std::min(heavies, 4);
    shooters = std::min(shooters, 8);
    grunts = std::min(grunts, 14);

    g.spawnQueue.clear();
    for (int i = 0; i < grunts; i++) g.spawnQueue.push_back(E_GRUNT);
    for (int i = 0; i < shooters; i++) g.spawnQueue.push_back(E_SHOOTER);
    for (int i = 0; i < heavies; i++) g.spawnQueue.push_back(E_HEAVY);
    // shuffle
    for (int i = (int)g.spawnQueue.size() - 1; i > 0; i--)
        std::swap(g.spawnQueue[i], g.spawnQueue[randi(0, i)]);
    g.spawnRemaining = (int)g.spawnQueue.size();
    g.waveTotal = g.spawnRemaining;
    g.waveAlive = g.spawnRemaining;
    g.spawnCd = 0.3f;

    char buf[48];
    if (heavies > 0) snprintf(buf, sizeof(buf), "WAVE %d - HEAVY DETECTED", n);
    else snprintf(buf, sizeof(buf), "WAVE %d", n);
    g.banner = buf;
    g.bannerT = 2.5f;
    audio::play(SFX_WAVE, nullptr, 1.0f);
    g.shake = std::min(1.0f, g.shake + 0.15f);
}

static void updateWave(Game& g, float dt) {
    if (!g.waveActive) {
        g.waveStateT -= dt;
        if (g.waveStateT <= 0) game::startNextWave(g);
        return;
    }
    // trickle spawns
    if (g.spawnRemaining > 0) {
        g.spawnCd -= dt;
        if (g.spawnCd <= 0) {
            g.spawnCd = randf(0.5f, 1.0f);
            EnemyType t = g.spawnQueue.back();
            g.spawnQueue.pop_back();
            g.spawnRemaining--;
            Vec3 base = world::spawnPads[randi(0, 7)];
            Vec3 pos = base + Vec3(randf(-1.5f, 1.5f), 0, randf(-1.5f, 1.5f));
            g.enemies.push_back(makeEnemy(t, pos));
            game::spawnParticles(g, pos + Vec3(0, 0.3f, 0), 14, Color::cyan(), 3.5f, 0.1f, 0.7f, -2.0f, 0.9f);
            audio::play(SFX_SPAWN, &pos, 0.6f, 0.8f);
        }
    }
    // wave clear check
    if (g.spawnRemaining == 0) {
        int alive = 0;
        for (Enemy& e : g.enemies) if (!e.dying && e.spawnT <= 0) alive++;
        g.waveAlive = alive;
        if (alive == 0) {
            g.waveActive = false;
            g.waveStateT = 5.0f;
            g.banner = "WAVE CLEARED";
            g.bannerT = 2.5f;
            int bonus = 250 + g.wave * 100;
            g.score += bonus;
            char buf[40];
            snprintf(buf, sizeof(buf), "WAVE BONUS +%d", bonus);
            popup(g, eyeOf(g.player) + forwardOf(g.player) * 4.0f, buf, Color::cyan());
            audio::play(SFX_PICKUP, nullptr, 0.9f, 0.7f);
        }
    }
}

// ----------------------------------------------------------- pickups
static void updatePickups(Game& g, float dt) {
    Player& p = g.player;
    for (Pickup& pk : g.pickups) {
        pk.spin += dt * 2.5f;
        if (!pk.active) {
            pk.respawnT -= dt;
            if (pk.respawnT <= 0) {
                pk.active = true;
                game::spawnParticles(g, pk.pos + Vec3(0, 0.5f, 0), 10, Color::cyan(), 2.5f, 0.08f, 0.5f, -1.0f, 0.9f);
            }
            continue;
        }
        float d = len(pk.pos + Vec3(0, 0.5f, 0) - p.pos);
        if (d < 1.2f && p.alive) {
            bool used = false;
            if (pk.health && p.hp < p.maxHp) {
                p.hp = std::min(p.maxHp, p.hp + 30.0f);
                popup(g, pk.pos + Vec3(0, 1, 0), "+30 HP", Color::green());
                used = true;
            } else if (!pk.health && p.reserve < 300) {
                p.reserve = std::min(300, p.reserve + 60);
                popup(g, pk.pos + Vec3(0, 1, 0), "+60 AMMO", Color::amber());
                used = true;
            }
            if (used) {
                pk.active = false;
                pk.respawnT = 14.0f;
                audio::play(SFX_PICKUP, &pk.pos, 0.9f);
                game::spawnParticles(g, pk.pos + Vec3(0, 0.5f, 0), 12,
                                     pk.health ? Color::green() : Color::amber(), 3, 0.08f, 0.4f, 3, 0.88f);
            }
        }
    }
}

// ----------------------------------------------------------- player
static void updatePlayer(Game& g, const Input& in, float dt) {
    Player& p = g.player;
    if (!p.alive) return;

    // bodycam aim: hold RMB to raise the rifle to the eye (not while reloading)
    bool wantAds = in.mouseDown[GLFW_MOUSE_BUTTON_RIGHT] && p.reloadT <= 0;
    p.adsT = lerpf(p.adsT, wantAds ? 1.0f : 0.0f, clampf(dt * 9, 0, 1));

    // look (aiming tightens the camera)
    p.yaw += in.mouseDX * g.sens * (1.0f - 0.55f * p.adsT);
    p.pitch -= in.mouseDY * g.sens * (1.0f - 0.55f * p.adsT);
    p.pitch = clampf(p.pitch, -1.52f, 1.52f);

    // movement
    Vec3 fwd = flatForward(p);
    Vec3 right = rightOf(p);
    Vec3 wish(0, p.vel.y, 0);
    float speed = (in.key[GLFW_KEY_LEFT_SHIFT] ? 9.2f : 6.4f) * (1.0f - 0.45f * p.adsT);
    if (in.key[GLFW_KEY_W]) wish += fwd * speed;
    if (in.key[GLFW_KEY_S]) wish -= fwd * speed;
    if (in.key[GLFW_KEY_D]) wish += right * speed;
    if (in.key[GLFW_KEY_A]) wish -= right * speed;
    if (in.key[GLFW_KEY_SPACE] && p.grounded) {
        p.vel.y = 8.2f;
        p.grounded = false;
        audio::play(SFX_JUMP, nullptr, 0.5f, randf(0.95f, 1.05f));
    }

    float accel = p.grounded ? 14.0f : 5.0f;
    p.vel.x = lerpf(p.vel.x, wish.x, clampf(accel * dt, 0, 1));
    p.vel.z = lerpf(p.vel.z, wish.z, clampf(accel * dt, 0, 1));
    p.vel.y -= 20.0f * dt;

    Vec3 half = playerHalf();
    float oldVy = p.vel.y;
    auto mr = world::moveAABB(p.pos, p.vel, half, dt);
    bool wasGrounded = p.grounded;
    p.pos = mr.pos;
    p.vel = mr.vel;
    p.grounded = mr.hitGround;
    if (!wasGrounded && p.grounded && oldVy < -6.0f) {
        audio::play(SFX_LAND, nullptr, 0.5f, clampf(-oldVy / 12.0f, 0.6f, 1.2f));
        game::spawnParticles(g, p.pos - Vec3(0, half.y, 0), 5, Vec3(0.5f, 0.5f, 0.5f), 1.5f, 0.07f, 0.3f, 3, 0.85f);
    }

    // head bob & footsteps
    float hspeed = len(Vec2(p.vel.x, p.vel.z));
    if (p.grounded && hspeed > 0.5f) {
        p.walkDist += hspeed * dt;
        p.bobPhase += hspeed * dt * 1.6f;
        p.stepDist += hspeed * dt;
        if (p.stepDist > 2.4f) {
            p.stepDist = 0;
            audio::play(SFX_STEP, nullptr, 0.35f, randf(0.85f, 1.15f) * (hspeed > 7 ? 1.15f : 1.0f));
        }
    } else {
        p.bobPhase = lerpf(p.bobPhase, roundf(p.bobPhase / PI) * PI, clampf(dt * 8, 0, 1));
    }

    // bodycam: camera rolls with lateral velocity (strafing tilt)
    float latVel = dot(p.vel, rightOf(p));
    float targetRoll = clampf(latVel * 0.012f, -0.075f, 0.075f);
    p.roll = lerpf(p.roll, targetRoll, clampf(dt * 5, 0, 1));

    // regen
    p.lastDamageT += dt;
    if (p.lastDamageT > 4.0f && p.hp < p.maxHp)
        p.hp = std::min(p.maxHp, p.hp + 9.0f * dt);

    // weapon
    p.fireCd -= dt;
    p.recoil = lerpf(p.recoil, 0, clampf(dt * 9, 0, 1));
    p.gunKick = std::max(0.0f, p.gunKick - dt * 9.0f);
    p.muzzleT = std::max(0.0f, p.muzzleT - dt);
    if (p.reloadT > 0) {
        p.reloadT -= dt;
        // mag-out click near the start, bolt-release slap near the end
        if (!p.reloadClick1 && p.reloadT < 1.35f) {
            p.reloadClick1 = true;
            audio::play(SFX_RELOAD, nullptr, 0.7f, 1.2f);
        }
        if (!p.reloadClick2 && p.reloadT < 0.28f) {
            p.reloadClick2 = true;
            audio::play(SFX_RELOAD, nullptr, 0.8f, 0.75f);
        }
        if (p.reloadT <= 0) {
            int need = p.magSize - p.ammo;
            int take = std::min(need, p.reserve);
            p.ammo += take;
            p.reserve -= take;
            p.reloadClick1 = p.reloadClick2 = false;
        }
    } else {
        if (in.keyPressed[GLFW_KEY_R] && p.ammo < p.magSize && p.reserve > 0) {
            p.reloadT = 1.6f;
            p.reloadClick1 = p.reloadClick2 = false;
            audio::play(SFX_RELOAD, nullptr, 0.7f, 0.95f);
        }
        if (in.mouseDown[GLFW_MOUSE_BUTTON_LEFT] && p.fireCd <= 0 && p.ammo > 0)
            fireWeapon(g);
        else if (in.mousePressed[GLFW_MOUSE_BUTTON_LEFT] && p.ammo <= 0)
            fireWeapon(g); // play dry click
    }
}

// ----------------------------------------------------------- misc updates
static void updateProjectiles(Game& g, float dt) {
    Player& p = g.player;
    for (size_t i = 0; i < g.projectiles.size(); i++) {
        Projectile& pr = g.projectiles[i];
        pr.ttl -= dt;
        Vec3 step = pr.vel * dt;
        pr.pos += step;

        bool dead = pr.ttl <= 0;
        // world hit
        float t;
        if (!dead && world::raycast(pr.pos, norm(pr.vel), len(step) + 0.1f, &t)) {
            dead = true;
            game::spawnParticles(g, pr.pos, 6, Color::magenta(), 3, 0.07f, 0.3f, 4, 0.85f);
        }
        // player hit
        if (!dead && p.alive) {
            Vec3 half = playerHalf();
            if (fabsf(pr.pos.x - p.pos.x) < half.x + 0.15f &&
                fabsf(pr.pos.y - p.pos.y) < half.y + 0.15f &&
                fabsf(pr.pos.z - p.pos.z) < half.z + 0.15f) {
                damagePlayer(g, 12, pr.pos);
                dead = true;
            }
        }
        if (dead) {
            g.projectiles.erase(g.projectiles.begin() + i);
            i--;
            continue;
        }
        // trail
        if (randi(0, 1)) {
            Particle pt;
            pt.pos = pr.pos;
            pt.vel = Vec3(randf(-0.3f, 0.3f), randf(-0.3f, 0.3f), randf(-0.3f, 0.3f));
            pt.maxLife = pt.life = 0.25f;
            pt.size = 0.09f;
            pt.color = Color::magenta();
            pt.gravity = 0;
            pt.drag = 0.9f;
            g.particles.push_back(pt);
        }
    }
}

static void updateFX(Game& g, float dt) {
    for (size_t i = 0; i < g.particles.size(); i++) {
        Particle& pt = g.particles[i];
        pt.life -= dt;
        if (pt.life <= 0) {
            g.particles.erase(g.particles.begin() + i);
            i--;
            continue;
        }
        pt.vel.y -= pt.gravity * dt;
        pt.vel = pt.vel * powf(pt.drag, dt * 60.0f);
        pt.pos += pt.vel * dt;
        if (pt.pos.y < 0.03f && pt.gravity > 0) {
            pt.pos.y = 0.03f;
            pt.vel.y = -pt.vel.y * 0.4f;
        }
    }
    if (g.particles.size() > 800) g.particles.erase(g.particles.begin(), g.particles.begin() + (g.particles.size() - 800));

    for (size_t i = 0; i < g.tracers.size(); i++) {
        g.tracers[i].life -= dt;
        if (g.tracers[i].life <= 0) {
            g.tracers.erase(g.tracers.begin() + i);
            i--;
        }
    }
    for (size_t i = 0; i < g.popups.size(); i++) {
        Popup& pp = g.popups[i];
        pp.ttl -= dt;
        pp.pos.y += dt * 1.2f;
        if (pp.ttl <= 0) {
            g.popups.erase(g.popups.begin() + i);
            i--;
        }
    }
    for (size_t i = 0; i < g.feed.size(); i++) {
        g.feed[i].ttl -= dt;
        if (g.feed[i].ttl <= 0) {
            g.feed.erase(g.feed.begin() + i);
            i--;
        }
    }

    g.hitMarkerT = std::max(0.0f, g.hitMarkerT - dt);
    g.damageFlash = std::max(0.0f, g.damageFlash - dt * 1.6f);
    g.shake = std::max(0.0f, g.shake - dt * 2.2f);
    g.bannerT = std::max(0.0f, g.bannerT - dt);
    g.comboT -= dt;
    if (g.comboT <= 0) g.combo = 0;
}

// ----------------------------------------------------------- top-level
void game::reset(Game& g) {
    g.enemies.clear();
    g.projectiles.clear();
    g.particles.clear();
    g.tracers.clear();
    g.popups.clear();
    g.feed.clear();
    g.player = Player();
    g.player.fireCd = 0.35f; // ignore the menu click that started the run
    g.wave = 0;
    g.waveActive = false;
    g.waveStateT = 2.5f;
    g.spawnRemaining = 0;
    g.spawnQueue.clear();
    g.score = 0;
    g.combo = 0;
    g.comboT = 0;
    g.kills = g.headshots = g.shotsFired = g.shotsHit = 0;
    g.hitMarkerT = g.damageFlash = g.shake = 0;
    g.deathT = 0;
    g.banner = "";
    g.bannerT = 0;
    g.pickups.clear();
    for (int i = 0; i < 6; i++)
        g.pickups.push_back({world::pickupPads[i], i % 2 == 0, true, 0, 0});
    g.state = GState::Playing;
    g.banner = "SURVIVE";
    g.bannerT = 2.0f;
    audio::play(SFX_START, nullptr, 0.9f);
}

void game::update(Game& g, const Input& in, float dt) {
    g.time += dt;
    if (g.state != GState::Playing && g.state != GState::GameOver) return;

    if (g.state == GState::Playing) {
        updatePlayer(g, in, dt);
        updateWave(g, dt);
        updatePickups(g, dt);
    } else {
        g.deathT += dt;
        // death cam: sink to ground level
        g.player.pitch = lerpf(g.player.pitch, -0.5f, clampf(dt * 2, 0, 1));
    }

    for (Enemy& e : g.enemies) updateEnemy(g, e, dt);
    for (size_t i = 0; i < g.enemies.size(); i++) {
        if (g.enemies[i].dying && g.enemies[i].deathT <= 0) {
            g.enemies.erase(g.enemies.begin() + i);
            i--;
        }
    }
    updateProjectiles(g, dt);
    updateFX(g, dt);
    audio::updateListener(eyeOf(g.player), forwardOf(g.player));
}
