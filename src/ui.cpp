// ui.cpp — 2D overlay: HUD, menus, radar, banners, damage vignette
#include "ui.h"
#include "glrender.h"

static Vec4 rgba(Vec3 c, float a) { return {c.x, c.y, c.z, a}; }

static void bar(float x, float y, float w, float h, float frac, Vec4 back, Vec4 fill) {
    R::quad2D(x, y, w, h, back);
    if (frac > 0.001f) {
        float fw = w * clampf(frac, 0, 1);
        R::quad2D(x, y, fw, h, fill);
    }
    // outline
    Vec4 line(0.05f, 0.05f, 0.08f, 0.9f);
    R::quad2D(x, y, w, 1.5f, line);
    R::quad2D(x, y + h - 1.5f, w, 1.5f, line);
    R::quad2D(x, y, 1.5f, h, line);
    R::quad2D(x + w - 1.5f, y, 1.5f, h, line);
}

// project world point to screen; returns false if behind camera
static bool project(const Mat4& vp, Vec3 p, int w, int h, float* sx, float* sy) {
    float m[16] = {vp.m[0], vp.m[1], vp.m[2], vp.m[3], vp.m[4], vp.m[5], vp.m[6], vp.m[7],
                   vp.m[8], vp.m[9], vp.m[10], vp.m[11], vp.m[12], vp.m[13], vp.m[14], vp.m[15]};
    float x = p.x, y = p.y, z = p.z;
    float cx = m[0] * x + m[4] * y + m[8] * z + m[12];
    float cy = m[1] * x + m[5] * y + m[9] * z + m[13];
    float cw = m[3] * x + m[7] * y + m[11] * z + m[15];
    if (cw <= 0.05f) return false;
    float nx = cx / cw, ny = cy / cw;
    *sx = (nx * 0.5f + 0.5f) * w;
    *sy = (1.0f - (ny * 0.5f + 0.5f)) * h;
    return true;
}

// --------------------------------------------------------------- crosshair
static void drawCrosshair(Game& g) {
    Player& p = g.player;
    float cx = (float)R::vpW / 2, cy = (float)R::vpH / 2;
    float gap = 7.0f + p.gunKick * 10.0f + len(Vec2(p.vel.x, p.vel.z)) * 0.8f;
    float lenA = 9.0f;
    float th = 2.0f;
    Vec4 c(0.9f, 0.95f, 1.0f, 0.85f);
    Vec4 hot(1.0f, 0.85f, 0.3f, 0.95f);
    if (g.hitMarkerT > 0) {
        c = g.hitMarkerHead ? Vec4(1.0f, 0.9f, 0.2f, 1) : Vec4(1.0f, 0.35f, 0.4f, 1);
        lenA += 3.0f;
    }
    R::quad2D(cx - th / 2, cy - gap - lenA, th, lenA, c);
    R::quad2D(cx - th / 2, cy + gap, th, lenA, c);
    R::quad2D(cx - gap - lenA, cy - th / 2, lenA, th, c);
    R::quad2D(cx + gap, cy - th / 2, lenA, th, c);
    R::quad2D(cx - 1.5f, cy - 1.5f, 3, 3, c);
}

// --------------------------------------------------------------- radar
static void drawRadar(Game& g) {
    float cx = (float)R::vpW - 82, cy = 82, r = 62;
    Vec4 bg(0.05f, 0.06f, 0.10f, 0.55f);
    Vec4 ring(0.3f, 0.85f, 0.9f, 0.35f);
    // approximate circle with quads (diamond fill looks fine at small size)
    R::quad2D(cx - r * 0.72f, cy - r * 0.72f, r * 1.44f, r * 1.44f, bg);
    // ring: 4 edge bars
    R::quad2D(cx - r, cy - 1.5f, r * 2, 3, ring);
    R::quad2D(cx - r, cy + r - 1.5f, r * 2, 3, ring);
    R::quad2D(cx - r, cy - r, 3, r * 2, ring);
    R::quad2D(cx + r - 3, cy - r, 3, r * 2, ring);
    // crosshair lines
    Vec4 grid(0.3f, 0.85f, 0.9f, 0.15f);
    R::quad2D(cx - r, cy - 0.75f, r * 2, 1.5f, grid);
    R::quad2D(cx - 0.75f, cy - r, 1.5f, r * 2, grid);

    Player& p = g.player;
    float cosY = cosf(-p.yaw), sinY = sinf(-p.yaw);
    auto rel = [&](Vec3 wp, float* rx, float* ry, float maxD) {
        float dx = wp.x - p.pos.x, dz = wp.z - p.pos.z;
        // rotate so player forward (-Z at yaw 0) points up on radar
        float tx = dx * cosY - dz * sinY;
        float tz = dx * sinY + dz * cosY;
        float d = sqrtf(tx * tx + tz * tz);
        if (d > maxD) { tx *= maxD / d; tz *= maxD / d; }
        *rx = cx + tx * (r / maxD);
        *ry = cy + tz * (r / maxD);
    };
    for (Enemy& e : g.enemies) {
        if (e.dying || e.spawnT > 0) continue;
        float rx, ry;
        rel(e.pos, &rx, &ry, 40.0f);
        Vec3 c = e.type == E_HEAVY ? Vec3(1.0f, 0.4f, 0.2f) : (e.type == E_SHOOTER ? Color::magenta() : Color::red());
        float s = e.type == E_HEAVY ? 7.0f : 5.0f;
        R::quad2D(rx - s / 2, ry - s / 2, s, s, rgba(c, 0.95f));
    }
    for (Pickup& pk : g.pickups) {
        if (!pk.active) continue;
        float rx, ry;
        rel(pk.pos, &rx, &ry, 40.0f);
        R::quad2D(rx - 2, ry - 2, 4, 4, rgba(pk.health ? Color::green() : Color::amber(), 0.9f));
    }
    // player marker
    R::quad2D(cx - 2.5f, cy - 2.5f, 5, 5, Vec4(0.4f, 0.95f, 1.0f, 1.0f));
}

// --------------------------------------------------------------- HUD (bodycam overlay)
void ui::drawHUD(Game& g, const Mat4& vp, int w, int h) {
    Player& p = g.player;
    float sw = (float)w, sh = (float)h;
    Vec4 white(0.92f, 0.94f, 0.92f, 0.9f);
    Vec4 dim(0.92f, 0.94f, 0.92f, 0.55f);
    Vec4 amber(1.0f, 0.75f, 0.35f, 0.9f);

    // damage vignette
    if (g.damageFlash > 0.01f) {
        float a = clampf(g.damageFlash, 0, 0.8f) * 0.5f;
        Vec4 red(0.7f, 0.05f, 0.08f, a);
        float m = 52.0f;
        R::quad2D(0, 0, sw, m, red);
        R::quad2D(0, sh - m, sw, m, red);
        R::quad2D(0, 0, m, sh, red);
        R::quad2D(sw - m, 0, m, sh, red);
        R::quad2D(0, 0, sw, sh, Vec4(0.7f, 0.05f, 0.08f, a * 0.22f));
    }

    if (!p.alive) return;

    // --- REC indicator (blinking) + cam id
    float blink = 0.5f + 0.5f * sinf((float)g.time * 4.5f);
    R::quad2D(30, 30, 13, 13, Vec4(0.9f, 0.12f, 0.12f, 0.35f + 0.65f * blink));
    R::text(52, 28, 17, white, "REC");
    R::text(52, 50, 11, dim, "CAM 01 - CHEST MOUNT");

    // --- battery + signal (top right)
    float bx = sw - 96;
    R::quad2D(bx, 34, 40, 18, Vec4(0, 0, 0, 0));
    Vec4 shell(0.92f, 0.94f, 0.92f, 0.75f);
    R::quad2D(bx, 34, 34, 16, Vec4(0, 0, 0, 0)); // bg
    R::quad2D(bx, 34, 34, 16, Vec4(0, 0, 0, 0));
    R::quad2D(bx, 34, 2, 16, shell);
    R::quad2D(bx + 32, 34, 2, 16, shell);
    R::quad2D(bx, 34, 34, 2, shell);
    R::quad2D(bx, 48, 34, 2, shell);
    R::quad2D(bx + 36, 39, 3, 6, shell);
    float batt = 0.72f + 0.05f * sinf((float)g.time * 0.2f);
    R::quad2D(bx + 4, 36, 28 * batt, 12, shell);
    R::text(bx - 4, 56, 11, dim, "BATT 87%", 2);
    // signal bars
    for (int i = 0; i < 4; i++)
        R::quad2D(bx - 60 + i * 10, 48 - (i + 1) * 4, 6, (i + 1) * 4, shell);
    R::text(bx - 20, 56, 11, dim, "SIGNAL", 2);

    // --- wave (top center, small)
    char buf[64];
    snprintf(buf, sizeof(buf), "OP %02d", g.wave);
    R::text(sw / 2, 26, 15, amber, buf, 1);
    if (g.waveActive) {
        snprintf(buf, sizeof(buf), "HOSTILES %d", g.waveAlive + g.spawnRemaining);
        R::text(sw / 2, 46, 11, dim, buf, 1);
    } else {
        snprintf(buf, sizeof(buf), "NEXT OP IN %d", (int)ceilf(g.waveStateT));
        R::text(sw / 2, 46, 11, amber, buf, 1);
    }

    // --- timestamp (bottom left)
    int t = (int)g.time + 4 * 3600 + 17 * 60 + 33;
    snprintf(buf, sizeof(buf), "2026-11-22  %02d:%02d:%02d", (t / 3600) % 24, (t / 60) % 60, t % 60);
    R::text(30, sh - 30, 13, white, buf);

    // --- vitals (bottom left, above timestamp)
    snprintf(buf, sizeof(buf), "VITALS %d%%", (int)ceilf(p.hp / p.maxHp * 100));
    R::text(30, sh - 84, 13, p.hp < 30 ? Vec4(1.0f, 0.3f, 0.25f, 0.95f) : white, buf);
    bar(30, sh - 64, 190, 7, p.hp / p.maxHp, Vec4(0.05f, 0.05f, 0.06f, 0.6f),
        rgba(Color::cyan() * (0.5f + 0.5f * p.hp / p.maxHp), 0.9f));

    // --- ammo (bottom right)
    snprintf(buf, sizeof(buf), "%d / %d", p.ammo, p.reserve);
    R::text(sw - 30, sh - 84, 20, p.ammo == 0 ? Vec4(1.0f, 0.3f, 0.25f, 0.95f) : white, buf, 2);
    if (p.reloadT > 0) {
        float frac = 1.0f - p.reloadT / 1.6f;
        bar(sw - 220, sh - 58, 190, 6, frac, Vec4(0.05f, 0.05f, 0.06f, 0.6f), rgba(Color::amber(), 0.9f));
        float bl = 0.6f + 0.4f * sinf((float)g.time * 9.0f);
        R::text(sw - 125, sh - 78, 12, rgba(Color::amber(), bl), "RELOADING", 1);
    }

    // --- score strip (bottom center)
    snprintf(buf, sizeof(buf), "SCORE %d", g.score);
    R::text(sw / 2, sh - 30, 13, dim, buf, 1);
    if (g.combo > 1) {
        snprintf(buf, sizeof(buf), "X%.1f", 1.0f + std::min(g.combo, 10) * 0.1f);
        R::text(sw / 2 + 130, sh - 30, 13, rgba(Color::yellow(), 0.9f), buf);
    }

    // --- UAV feed (radar, right side under signal)
    float cx = sw - 92, cy = 168, r = 56;
    Vec4 bg(0.02f, 0.03f, 0.03f, 0.45f);
    Vec4 ring(0.7f, 0.85f, 0.8f, 0.30f);
    R::quad2D(cx - r * 0.72f, cy - r * 0.72f, r * 1.44f, r * 1.44f, bg);
    R::quad2D(cx - r, cy - 1.5f, r * 2, 3, ring);
    R::quad2D(cx - r, cy + r - 1.5f, r * 2, 3, ring);
    R::quad2D(cx - r, cy - r, 3, r * 2, ring);
    R::quad2D(cx + r - 3, cy - r, 3, r * 2, ring);
    Vec4 grid(0.7f, 0.85f, 0.8f, 0.12f);
    R::quad2D(cx - r, cy - 0.75f, r * 2, 1.5f, grid);
    R::quad2D(cx - 0.75f, cy - r, 1.5f, r * 2, grid);
    R::text(cx, cy - r - 16, 11, dim, "UAV FEED", 1);

    float cosY = cosf(-p.yaw), sinY = sinf(-p.yaw);
    auto rel = [&](Vec3 wp, float* rx, float* ry, float maxD) {
        float dx = wp.x - p.pos.x, dz = wp.z - p.pos.z;
        float tx = dx * cosY - dz * sinY;
        float tz = dx * sinY + dz * cosY;
        float d = sqrtf(tx * tx + tz * tz);
        if (d > maxD) { tx *= maxD / d; tz *= maxD / d; }
        *rx = cx + tx * (r / maxD);
        *ry = cy + tz * (r / maxD);
    };
    for (Enemy& e : g.enemies) {
        if (e.dying || e.spawnT > 0) continue;
        float rx, ry;
        rel(e.pos, &rx, &ry, 40.0f);
        Vec3 c = e.type == E_HEAVY ? Vec3(1.0f, 0.45f, 0.25f) : (e.type == E_SHOOTER ? Vec3(0.9f, 0.8f, 0.3f) : Vec3(1.0f, 0.35f, 0.3f));
        float sz = e.type == E_HEAVY ? 7.0f : 5.0f;
        R::quad2D(rx - sz / 2, ry - sz / 2, sz, sz, rgba(c, 0.9f));
    }
    for (Pickup& pk : g.pickups) {
        if (!pk.active) continue;
        float rx, ry;
        rel(pk.pos, &rx, &ry, 40.0f);
        R::quad2D(rx - 2, ry - 2, 4, 4, rgba(pk.health ? Color::green() : Color::amber(), 0.9f));
    }
    R::quad2D(cx - 2.5f, cy - 2.5f, 5, 5, Vec4(0.85f, 0.95f, 0.9f, 1.0f));

    // --- kill feed (below radar)
    float fy = 244;
    for (size_t i = g.feed.size(); i-- > 0;) {
        FeedItem& f = g.feed[i];
        float a2 = clampf(f.ttl, 0, 1);
        R::text(sw - 30, fy, 11, rgba(Color::white(), a2 * 0.8f), f.text, 2);
        fy += 16;
    }

    // --- score popups (world -> screen)
    for (Popup& pp : g.popups) {
        float sx, sy;
        if (project(vp, pp.pos, w, h, &sx, &sy)) {
            float a2 = clampf(pp.ttl, 0, 1);
            R::text(sx, sy, 12, rgba(pp.color, a2), pp.text, 1);
        }
    }

    // --- banner
    if (g.bannerT > 0) {
        float a2 = clampf(g.bannerT, 0, 1);
        float slide = (1.0f - a2) * 30.0f;
        R::text(sw / 2, sh * 0.30f + slide, 30, Vec4(0.92f, 0.94f, 0.92f, a2), g.banner, 1);
    }

    // --- low HP warning frame
    if (p.hp < 30) {
        float pulse = 0.25f + 0.25f * sinf((float)g.time * 6.0f);
        R::quad2D(0, 0, sw, 5, rgba(Color::red(), pulse));
        R::quad2D(0, sh - 5, sw, 5, rgba(Color::red(), pulse));
        R::quad2D(0, 0, 5, sh, rgba(Color::red(), pulse));
        R::quad2D(sw - 5, 0, 5, sh, rgba(Color::red(), pulse));
    }

    // --- minimal dot crosshair (bodycam style), fades out when aiming down sights
    float cxp = sw / 2, cyp = sh / 2;
    R::quad2D(cxp - 2, cyp - 2, 4, 4, Vec4(0.95f, 0.97f, 0.95f, 0.8f * (1.0f - p.adsT)));
    if (g.hitMarkerT > 0) {
        float lm = 13.0f;
        Vec4 hc = g.hitMarkerHead ? Vec4(1.0f, 0.85f, 0.25f, 1) : Vec4(1.0f, 0.35f, 0.4f, 1);
        R::quad2D(cxp - lm, cyp - lm, 4, 4, hc);
        R::quad2D(cxp + lm - 4, cyp - lm, 4, 4, hc);
        R::quad2D(cxp - lm, cyp + lm - 4, 4, 4, hc);
        R::quad2D(cxp + lm - 4, cyp + lm - 4, 4, 4, hc);
    }
}

// --------------------------------------------------------------- menus
void ui::drawMenu(Game& g, int w, int h) {
    float sw = (float)w, sh = (float)h;
    R::quad2D(0, 0, sw, sh, Vec4(0.02f, 0.03f, 0.06f, 0.65f));

    float pulse = 0.8f + 0.2f * sinf((float)g.time * 3.0f);
    R::text(sw / 2, sh * 0.18f, 58, rgba(Color::cyan(), pulse), "NEON ARENA", 1);
    R::text(sw / 2, sh * 0.18f + 72, 18, rgba(Color::magenta(), 0.9f), "A LOW-POLY WAVE SURVIVAL FPS", 1);

    R::text(sw / 2, sh * 0.44f, 24, rgba(Color::white(), 0.95f), "CLICK TO PLAY", 1);
    R::text(sw / 2, sh * 0.44f + 34, 13, rgba(Color::amber(), 0.8f),
            "MOUSE WHEEL ADJUSTS SENSITIVITY", 1);

    float cy = sh * 0.58f;
    R::text(sw / 2, cy, 14, rgba(Color::cyan(), 0.85f), "CONTROLS", 1);
    const char* lines[] = {
        "WASD MOVE    SHIFT SPRINT    SPACE JUMP",
        "MOUSE LOOK    LEFT CLICK FIRE    R RELOAD",
        "ESC PAUSE",
    };
    for (int i = 0; i < 3; i++)
        R::text(sw / 2, cy + 24 + i * 22, 12, rgba(Color::white(), 0.75f), lines[i], 1);

    char buf[64];
    snprintf(buf, sizeof(buf), "SENSITIVITY %.2f", g.sens * 400.0f);
    R::text(sw / 2, sh * 0.82f, 12, rgba(Color::amber(), 0.8f), buf, 1);
    if (g.wave > 0) {
        snprintf(buf, sizeof(buf), "BEST RUN - WAVE %d, SCORE %d", g.wave, g.score);
        R::text(sw / 2, sh * 0.86f, 12, rgba(Color::white(), 0.6f), buf, 1);
    }
}

void ui::drawPause(Game& g, int w, int h) {
    float sw = (float)w, sh = (float)h;
    R::quad2D(0, 0, sw, sh, Vec4(0.02f, 0.03f, 0.06f, 0.6f));
    R::text(sw / 2, sh * 0.35f, 42, rgba(Color::cyan(), 0.95f), "PAUSED", 1);
    R::text(sw / 2, sh * 0.35f + 60, 16, rgba(Color::white(), 0.85f), "ESC RESUME    ENTER RESTART", 1);
    char buf[64];
    snprintf(buf, sizeof(buf), "SENSITIVITY %.2f (MOUSE WHEEL)", g.sens * 400.0f);
    R::text(sw / 2, sh * 0.35f + 92, 12, rgba(Color::amber(), 0.8f), buf, 1);
}

void ui::drawGameOver(Game& g, int w, int h) {
    float sw = (float)w, sh = (float)h;
    float fade = clampf(g.deathT * 1.5f, 0, 1);
    R::quad2D(0, 0, sw, sh, Vec4(0.08f, 0.01f, 0.02f, fade * 0.72f));

    float pulse = 0.8f + 0.2f * sinf((float)g.time * 3.0f);
    R::text(sw / 2, sh * 0.20f, 46, rgba(Color::red(), fade * pulse), "YOU DIED", 1);

    if (g.deathT > 0.6f) {
        float a = clampf((g.deathT - 0.6f) * 2, 0, 1);
        char buf[80];
        snprintf(buf, sizeof(buf), "SCORE %d", g.score);
        R::text(sw / 2, sh * 0.38f, 30, rgba(Color::white(), a), buf, 1);
        snprintf(buf, sizeof(buf), "REACHED WAVE %d", g.wave);
        R::text(sw / 2, sh * 0.38f + 44, 18, rgba(Color::cyan(), a * 0.95f), buf, 1);

        snprintf(buf, sizeof(buf), "KILLS %d    HEADSHOTS %d", g.kills, g.headshots);
        R::text(sw / 2, sh * 0.52f, 15, rgba(Color::white(), a * 0.85f), buf, 1);
        float acc = g.shotsFired > 0 ? 100.0f * g.shotsHit / g.shotsFired : 0;
        snprintf(buf, sizeof(buf), "ACCURACY %.0f%%", acc);
        R::text(sw / 2, sh * 0.52f + 26, 15, rgba(Color::white(), a * 0.85f), buf, 1);

        float blink = 0.55f + 0.45f * sinf((float)g.time * 4.0f);
        R::text(sw / 2, sh * 0.72f, 20, rgba(Color::amber(), a * blink), "ENTER - RETRY", 1);
        R::text(sw / 2, sh * 0.72f + 30, 13, rgba(Color::white(), a * 0.6f), "ESC - MAIN MENU", 1);
    }
}
