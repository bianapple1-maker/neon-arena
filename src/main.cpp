// main.cpp — NEON ARENA: window, input, scene composition, game loop
#include <GLFW/glfw3.h>
#include "core.h"
#include "glrender.h"
#include "world.h"
#include "game.h"
#include "ui.h"
#include "audio.h"

// ------------------------------------------------------------- scene meshes
static Mesh meshWorld;
static Mesh meshBody[3], meshHead[3], meshLimb[3], meshLeg[3], meshVisor[3];
static Mesh meshPickupHP, meshPickupAmmo;
static Mesh meshProj, meshGun, meshGunE, meshMagVM, meshHandL;

static Vec3 FOG_COLOR = {0.44f, 0.46f, 0.49f}; // pale overcast haze

static void buildEntities() {
    MeshBuilder mb;

    // cylinder along the Z axis (for barrels/gun parts), baked into the current builder
    auto cylZ = [&](Vec3 center, float r, float len, int sides, Vec3 col) {
        float z0 = center.z - len * 0.5f, z1 = center.z + len * 0.5f;
        for (int i = 0; i < sides; i++) {
            float a0 = TAU * (float)i / sides, a1 = TAU * (float)(i + 1) / sides;
            Vec3 p0{center.x + cosf(a0) * r, center.y + sinf(a0) * r, z0};
            Vec3 p1{center.x + cosf(a1) * r, center.y + sinf(a1) * r, z0};
            Vec3 p2{center.x + cosf(a1) * r, center.y + sinf(a1) * r, z1};
            Vec3 p3{center.x + cosf(a0) * r, center.y + sinf(a0) * r, z1};
            mb.quadNormal(p0, p3, p2, p1, norm(Vec3(cosf((a0+a1)*0.5f), sinf((a0+a1)*0.5f), 0)), col);
            mb.quadNormal(center + Vec3(0, 0, z1), p2, p1, p1, {0, 0, 1}, col);
            mb.quadNormal(center + Vec3(0, 0, z0), p0, p3, p3, {0, 0, -1}, col);
        }
    };

    // ---- human-proportioned hostiles: [grunt, shooter, heavy]
    Vec3 cloth[3]   = {{0.30f, 0.28f, 0.25f}, {0.26f, 0.30f, 0.22f}, {0.15f, 0.15f, 0.16f}};
    Vec3 armorC[3]  = {{0.17f, 0.17f, 0.19f}, {0.15f, 0.17f, 0.14f}, {0.09f, 0.09f, 0.10f}};
    Vec3 helmet[3]  = {{0.20f, 0.20f, 0.22f}, {0.18f, 0.20f, 0.16f}, {0.08f, 0.08f, 0.09f}};
    Vec3 skinD[3]   = {{0.25f, 0.23f, 0.20f}, {0.23f, 0.24f, 0.20f}, {0.12f, 0.12f, 0.13f}};
    Vec3 visorC[3]  = {{1.0f, 0.25f, 0.2f}, {0.2f, 0.9f, 1.0f}, {1.0f, 0.2f, 0.25f}};

    for (int i = 0; i < 3; i++) {
        float s = (i == 2) ? 1.45f : (i == 1 ? 0.98f : 1.0f);
        // torso: tapered cylinder hips->shoulders + chest plate + shoulder pads
        mb.clear();
        mb.taperedCylinder({0, 1.15f * s, 0}, 0.155f * s, 0.235f * s, 0.62f * s, 8, cloth[i]);
        mb.box({0, 1.22f * s, 0.13f * s}, {0.165f * s, 0.17f * s, 0.08f * s}, armorC[i]);      // chest plate
        mb.box({0, 1.02f * s, 0.12f * s}, {0.12f * s, 0.10f * s, 0.05f * s}, armorC[i]);       // belt pouch
        mb.sphere({0.245f * s, 1.40f * s, 0}, 0.085f * s, 5, 8, armorC[i]);                    // shoulders
        mb.sphere({-0.245f * s, 1.40f * s, 0}, 0.085f * s, 5, 8, armorC[i]);
        if (i == 1) mb.box({0, 1.30f * s, -0.20f * s}, {0.12f * s, 0.16f * s, 0.08f * s}, armorC[i]); // shooter pack
        if (i == 2) { // heavy shoulder plates
            mb.box({0.26f * s, 1.44f * s, 0}, {0.09f * s, 0.05f * s, 0.13f * s}, armorC[i]);
            mb.box({-0.26f * s, 1.44f * s, 0}, {0.09f * s, 0.05f * s, 0.13f * s}, armorC[i]);
        }
        meshBody[i] = R::buildMesh(mb);

        // head: helmet sphere + neck
        mb.clear();
        mb.sphere({0, 0.10f * s, 0}, 0.125f * s, 6, 8, helmet[i]);
        mb.taperedCylinder({0, -0.045f * s, 0}, 0.05f * s, 0.06f * s, 0.09f * s, 6, skinD[i]);
        if (i == 1) mb.box({0.06f * s, 0.19f * s, -0.02f * s}, {0.008f * s, 0.10f * s, 0.008f * s}, armorC[i]); // antenna
        meshHead[i] = R::buildMesh(mb);

        // visor: small glowing slit on the helmet front (+Z)
        mb.clear();
        mb.quadNormal({-0.085f * s, 0.115f * s, 0.124f * s}, {-0.085f * s, 0.065f * s, 0.124f * s},
                      {0.085f * s, 0.065f * s, 0.124f * s}, {0.085f * s, 0.115f * s, 0.124f * s},
                      {0, 0, 1}, visorC[i]);
        meshVisor[i] = R::buildMesh(mb);

        // arm: tapered cylinder hanging from shoulder pivot + hand
        mb.clear();
        mb.taperedCylinder({0, -0.26f * s, 0}, 0.038f * s, 0.058f * s, 0.52f * s, 6, cloth[i]);
        mb.sphere({0, -0.545f * s, 0.01f * s}, 0.055f * s, 4, 6, skinD[i]); // hand
        meshLimb[i] = R::buildMesh(mb);

        // leg: tapered cylinder + boot
        mb.clear();
        mb.taperedCylinder({0, -0.38f * s, 0}, 0.052f * s, 0.075f * s, 0.76f * s, 6, cloth[i]);
        mb.box({0, -0.70f * s, 0.045f * s}, {0.065f * s, 0.06f * s, 0.115f * s}, armorC[i]); // boot
        meshLeg[i] = R::buildMesh(mb);
    }

    // pickups: matte military crates
    mb.clear();
    mb.box({0, 0, 0}, {0.24f, 0.18f, 0.24f}, {0.30f, 0.36f, 0.26f});
    mb.box({0, 0.10f, 0}, {0.10f, 0.09f, 0.10f}, Color::green());
    mb.box({0, 0.215f, 0}, {0.045f, 0.035f, 0.045f}, Color::white());
    meshPickupHP = R::buildMesh(mb);

    mb.clear();
    mb.box({0, 0, 0}, {0.26f, 0.16f, 0.18f}, {0.36f, 0.30f, 0.22f});
    mb.box({0, 0.09f, 0}, {0.20f, 0.07f, 0.05f}, Color::amber());
    meshPickupAmmo = R::buildMesh(mb);

    // enemy projectile: hot tracer bolt (stretched octahedron, front = +Z)
    mb.clear();
    for (int i = 0; i < 6; i++) {
        float a0 = TAU * i / 6, a1 = TAU * (i + 1) / 6;
        Vec3 v0{cosf(a0) * 0.045f, 0, sinf(a0) * 0.045f};
        Vec3 v1{cosf(a1) * 0.045f, 0, sinf(a1) * 0.045f};
        mb.quadNormal({0, 0.045f, 0.30f}, v1, v0, v0,
                      norm(Vec3(cosf((a0+a1)*0.5f), 0.7f, sinf((a0+a1)*0.5f))), Vec3(1.0f, 0.72f, 0.30f));
        mb.quadNormal({0, -0.045f, 0.30f}, v0, v1, v1,
                      norm(Vec3(cosf((a0+a1)*0.5f), -0.7f, sinf((a0+a1)*0.5f))), Vec3(0.85f, 0.55f, 0.20f));
        mb.quadNormal({0, 0.045f, -0.10f}, v0, v1, v1,
                      norm(Vec3(cosf((a0+a1)*0.5f), 0.7f, -sinf((a0+a1)*0.5f))), Vec3(1.0f, 0.95f, 0.75f));
        mb.quadNormal({0, -0.045f, -0.10f}, v1, v0, v0,
                      norm(Vec3(cosf((a0+a1)*0.5f), -0.7f, -sinf((a0+a1)*0.5f))), Vec3(0.9f, 0.7f, 0.35f));
    }
    meshProj = R::buildMesh(mb);

    // ---- hostile rifle (shared mesh, front = +Z)
    mb.clear();
    mb.box({0, 0, 0.02f}, {0.038f, 0.055f, 0.24f}, {0.09f, 0.09f, 0.095f});   // receiver
    mb.box({0, -0.01f, 0.26f}, {0.03f, 0.045f, 0.10f}, {0.13f, 0.13f, 0.135f}); // handguard
    cylZ({0, 0.012f, 0.42f}, 0.016f, 0.24f, 6, {0.07f, 0.07f, 0.075f});       // barrel
    mb.box({0, -0.09f, -0.03f}, {0.035f, 0.085f, 0.045f}, {0.11f, 0.11f, 0.115f}); // grip
    mb.taperedCylinder({0, -0.06f, 0.14f}, 0.024f, 0.038f, 0.13f, 6, {0.08f, 0.08f, 0.085f}); // mag
    mb.box({0, 0.045f, -0.16f}, {0.03f, 0.06f, 0.10f}, {0.10f, 0.10f, 0.105f}); // stock
    meshGunE = R::buildMesh(mb);

    // ---- view-model rifle: darker, cylindrical, suppressed
    Vec3 gunMetal(0.10f, 0.10f, 0.11f);
    Vec3 gunPoly(0.16f, 0.16f, 0.17f);
    mb.clear();
    mb.box({0, 0, -0.10f}, {0.052f, 0.078f, 0.30f}, gunMetal);           // receiver
    mb.box({0, -0.015f, -0.36f}, {0.045f, 0.055f, 0.10f}, gunPoly);      // handguard
    cylZ({0, 0.005f, -0.48f}, 0.022f, 0.18f, 8, gunMetal);               // barrel
    cylZ({0, 0.005f, -0.66f}, 0.037f, 0.24f, 8, {0.07f, 0.07f, 0.075f}); // suppressor
    mb.box({0, -0.115f, 0.03f}, {0.042f, 0.105f, 0.05f}, gunPoly);       // grip
    mb.box({0, 0.088f, -0.02f}, {0.02f, 0.028f, 0.14f}, gunMetal);       // sight rail
    mb.box({0, 0.125f, -0.08f}, {0.012f, 0.03f, 0.012f}, gunMetal);      // front post
    mb.box({0, 0.125f, 0.05f}, {0.030f, 0.032f, 0.02f}, {0.05f, 0.05f, 0.055f}); // rear sight
    mb.box({0, 0.02f, 0.235f}, {0.045f, 0.065f, 0.11f}, gunPoly);        // stock
    meshGun = R::buildMesh(mb);

    // ---- detachable magazine (animated separately for the reload)
    mb.clear();
    mb.taperedCylinder({0, -0.08f, 0}, 0.030f, 0.048f, 0.16f, 6, gunMetal);
    mb.box({0, -0.155f, 0}, {0.028f, 0.012f, 0.036f}, {0.2f, 0.2f, 0.21f}); // base plate
    meshMagVM = R::buildMesh(mb);

    // ---- low-poly hands (wrist at origin, fingers toward -Z, skin + sleeve)
    Vec3 skinH(0.52f, 0.40f, 0.33f), skinHD(0.44f, 0.33f, 0.27f), sleeveH(0.22f, 0.24f, 0.19f);
    mb.clear();
    mb.box({0, 0, -0.01f}, {0.034f, 0.020f, 0.052f}, skinH);              // palm
    mb.box({0, 0.006f, -0.052f}, {0.031f, 0.022f, 0.020f}, skinHD);       // knuckles
    mb.box({-0.019f, 0.010f, -0.068f}, {0.011f, 0.009f, 0.017f}, skinH);  // fingers
    mb.box({0.0f, 0.011f, -0.072f}, {0.011f, 0.009f, 0.017f}, skinH);
    mb.box({0.019f, 0.010f, -0.068f}, {0.011f, 0.009f, 0.017f}, skinH);
    mb.box({0.036f, 0.004f, -0.026f}, {0.011f, 0.012f, 0.028f}, skinHD);  // thumb
    mb.box({0, 0, 0.055f}, {0.038f, 0.038f, 0.055f}, sleeveH);            // sleeve cuff
    mb.box({0, 0, 0.135f}, {0.05f, 0.05f, 0.085f}, sleeveH);              // forearm
    meshHandL = R::buildMesh(mb);
}

static float spawnVisual(const Enemy& e, float* rise, float* spin) {
    if (e.spawnT > 0) {
        float t = 1.0f - e.spawnT / 0.6f;
        *rise = t;
        *spin = e.spawnT * 10.0f;
        return t;
    }
    *rise = 1.0f;
    *spin = 0;
    return 1.0f;
}

static void drawEnemy(Game& g, Enemy& e) {
    int i = e.type == E_GRUNT ? 0 : (e.type == E_SHOOTER ? 1 : 2);
    float s = (i == 2) ? 1.45f : (i == 1 ? 0.98f : 1.0f);

    float rise, spin;
    float a = spawnVisual(e, &rise, &spin);
    float alpha = e.dying ? clampf(e.deathT / 0.9f, 0, 1) : a;

    Vec3 tint(1, 1, 1);
    if (e.hitFlash > 0) tint = lerp(Vec3(1, 1, 1), Vec3(3.0f, 3.0f, 3.0f), e.hitFlash / 0.15f);
    if (e.dying) tint = lerp(Vec3(0.45f, 0.35f, 0.5f), Vec3(1, 1, 1), alpha);

    float fall = e.dying ? smooth01(1.0f - e.deathT / 0.9f) * PI * 0.45f : 0.0f;
    float swing = sinf(e.walkPhase) * clampf(e.animSpeed / 4.0f, 0, 1) * 0.7f;
    float bob = fabsf(cosf(e.walkPhase)) * clampf(e.animSpeed / 4.0f, 0, 1) * 0.07f * s;
    float yOff = (rise - 1.0f) * -1.4f; // rises from below floor

    float feetY = e.pos.y - e.height * 0.5f + yOff + bob;
    // model front is +Z; with yaw convention fwd=(sin,-,-cos) the model must rotate by (PI - yaw)
    Mat4 base = matMul(matTranslate(Vec3(e.pos.x, feetY, e.pos.z)),
                       matMul(matRotateY(PI - e.yaw + spin), matRotateX(-fall)));

    // torso (baked: torso, chest plate, shoulders)
    R::drawMesh(meshBody[i], matMul(base, matTranslate(Vec3(0, 0, 0))), tint, Vec3(0,0,0), alpha);
    // head
    R::drawMesh(meshHead[i], matMul(base, matTranslate(Vec3(0, 1.55f * s, 0))), tint, Vec3(0,0,0), alpha);
    // visor (emissive; brighter when telegraphing an attack)
    Vec3 visorE(0,0,0);
    if (i == 0) visorE = Vec3(1.0f, 0.25f, 0.2f) * (0.5f + (e.attackCd < 0.3f ? 0.9f : 0.0f));
    if (i == 1) visorE = Vec3(0.2f, 0.9f, 1.0f) * (0.5f + e.telegraphT * 1.5f);
    if (i == 2) visorE = Vec3(1.0f, 0.2f, 0.25f) * (0.5f + (e.attackCd < 0.4f ? 0.8f : 0.0f));
    R::drawMesh(meshVisor[i], matMul(base, matTranslate(Vec3(0, 1.55f * s, 0))), Vec3(1, 1, 1), visorE, alpha);
    // arms (pivot at shoulders, hang from origin)
    R::drawMesh(meshLimb[i], matMul(base, matMul(matTranslate(Vec3(0.26f * s, 1.40f * s, 0)),
              matRotateX(swing))), tint, Vec3(0,0,0), alpha);
    R::drawMesh(meshLimb[i], matMul(base, matMul(matTranslate(Vec3(-0.26f * s, 1.40f * s, 0)),
              matRotateX(-swing))), tint, Vec3(0,0,0), alpha);
    // rifle held aimed at the player's eye (pitch tracks the target)
    float dx = g.player.pos.x - e.pos.x, dz = g.player.pos.z - e.pos.z;
    float distH = std::max(sqrtf(dx * dx + dz * dz), 0.1f);
    float dyAim = (g.player.pos.y + 0.72f) - (feetY + 1.06f * s);
    float aimPitch = atan2f(dyAim, distH);
    float reloadAnim = e.reloadT > 0 ? sinf(clampf(1.0f - e.reloadT / 2.4f, 0, 1) * PI) : 0.0f;
    float flinchJerk = e.flinchT > 0 ? sinf(e.flinchT * 42.0f) * 0.06f : 0.0f;
    Mat4 gunBase = matMul(base, matMul(matTranslate(Vec3(0.17f * s, 1.06f * s - reloadAnim * 0.07f, 0.24f * s)),
                          matMul(matRotateX(-aimPitch - 0.10f + swing * 0.15f + reloadAnim * 0.5f),
                                 matRotateZ(flinchJerk))));
    R::drawMesh(meshGunE, gunBase, tint, e.spawnT > 0 ? Vec3(0,0,0) : Vec3(0.02f, 0.01f, 0.0f), alpha);
    // legs (pivot at hips)
    R::drawMesh(meshLeg[i], matMul(base, matMul(matTranslate(Vec3(0.115f * s, 0.84f * s, 0)),
              matRotateX(-swing * 0.8f))), tint, Vec3(0,0,0), alpha);
    R::drawMesh(meshLeg[i], matMul(base, matMul(matTranslate(Vec3(-0.115f * s, 0.84f * s, 0)),
              matRotateX(swing * 0.8f))), tint, Vec3(0,0,0), alpha);
}

static void drawScene(Game& g, int w, int h) {
    Player& p = g.player;

    R::beginScene(); // render the world into the post-process FBO

    // bodycam camera: shake + heavy bob + breathing + strafe roll + death drop
    float shakeAmp = g.shake * g.shake * 0.06f;
    float shX = randf(-1, 1) * shakeAmp, shY = randf(-1, 1) * shakeAmp;
    float bobY = p.grounded ? fabsf(sinf(p.bobPhase)) * 0.065f : 0.0f;
    float swayX = p.grounded ? sinf(p.bobPhase) * 0.035f * clampf(len(Vec2(p.vel.x, p.vel.z)) / 6.4f, 0, 1) : 0.0f;
    // aiming steadies the bodycam
    bobY *= 1.0f - 0.55f * p.adsT;
    swayX *= 1.0f - 0.7f * p.adsT;
    float breath = sinf((float)g.time * 1.25f) * 0.008f;
    float roll = p.roll + sinf((float)g.time * 1.1f) * 0.006f;
    float eyeH = 1.52f;
    if (!p.alive) eyeH = lerpf(1.52f, 0.40f, clampf(g.deathT * 1.4f, 0, 1));
    Vec3 eye = p.pos + Vec3(0, eyeH - 0.9f + bobY + shY + breath, 0);
    Vec3 fwd = forwardOf(p);
    Vec3 rgt = rightOf(p);
    eye += rgt * swayX;
    // roll: tilt the camera's up vector around the view direction
    Vec3 up = norm(Vec3(0, 1, 0) * cosf(roll) + rgt * sinf(roll));
    Vec3 target = eye + fwd + Vec3(shX, 0, 0);
    Mat4 view = matLookAt(eye, target, up);
    Mat4 proj = matPerspective(lerpf(1.30f, 0.85f, p.adsT), (float)w / (float)h, 0.05f, 120.0f);
    Mat4 vp = matMul(proj, view);

    glClearColor(FOG_COLOR.x, FOG_COLOR.y, FOG_COLOR.z, 1);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    R::begin3D(proj, view, eye, FOG_COLOR, {14.0f, 85.0f});

    // world
    R::drawMesh(meshWorld, Mat4());

    // pickups
    for (Pickup& pk : g.pickups) {
        if (!pk.active) continue;
        float y = 0.45f + sinf((float)g.time * 2.5f + pk.spin) * 0.12f;
        Mat4 m = matTRS(pk.pos + Vec3(0, y, 0), pk.spin, 0, 0, Vec3(1, 1, 1));
        R::drawMesh(pk.health ? meshPickupHP : meshPickupAmmo, m);
    }

    // enemies
    for (Enemy& e : g.enemies) drawEnemy(g, e);

    // projectiles: tracer bolts oriented along their velocity
    for (Projectile& pr : g.projectiles) {
        float spd = len(pr.vel);
        float yw = atan2f(pr.vel.x, -pr.vel.z);
        float pt2 = spd > 0.01f ? -asinf(clampf(pr.vel.y / spd, -1, 1)) : 0;
        R::drawMesh(meshProj, matTRS(pr.pos, PI - yw, pt2, 0, Vec3(1, 1, 1)),
                    Vec3(1, 1, 1), Vec3(1.0f, 0.62f, 0.25f) * 1.3f);
    }

    // dynamic FX: tracers, particles, blob shadows, muzzle flash
    R::dynClear();

    // blob shadows
    for (Enemy& e : g.enemies) {
        if (e.spawnT > 0 && e.spawnT > 0.3f) continue;
        float r = e.radius * (e.dying ? clampf(e.deathT / 0.9f, 0, 1) + 0.4f : 1.0f);
        float a = e.dying ? 0.35f * clampf(e.deathT / 0.9f, 0, 1) : 0.35f;
        Vec3 c = Vec3(e.pos.x, 0.03f, e.pos.z);
        R::dynQuad({c.x - r, c.y, c.z - r}, {c.x + r, c.y, c.z - r},
                   {c.x + r, c.y, c.z + r}, {c.x - r, c.y, c.z + r}, Vec4(0, 0, 0, a));
    }
    if (p.alive) {
        float r = 0.5f;
        Vec3 c = Vec3(p.pos.x, 0.03f, p.pos.z);
        R::dynQuad({c.x - r, c.y, c.z - r}, {c.x + r, c.y, c.z - r},
                   {c.x + r, c.y, c.z + r}, {c.x - r, c.y, c.z + r}, Vec4(0, 0, 0, 0.35f));
    }

    // tracers
    for (Tracer& t : g.tracers) {
        Vec3 dir = norm(t.to - t.from);
        Vec3 side = norm(cross(dir, fwd));
        if (len2(side) < 0.5f) side = Vec3(1, 0, 0);
        side = side * 0.025f;
        float a = clampf(t.life / 0.07f, 0, 1) * 0.75f;
        Vec4 col(1.0f, 0.85f, 0.4f, a);
        R::dynQuad(t.from + side, t.to + side, t.to - side, t.from - side, col);
    }

    // particles (camera-facing billboards)
    Vec3 upv = norm(cross(rgt, fwd));
    for (Particle& pt : g.particles) {
        float lt = clampf(pt.life / pt.maxLife, 0, 1);
        float sz = pt.size * (0.4f + 0.6f * lt);
        Vec3 X = rgt * sz, Y = upv * sz;
        Vec4 col(pt.color.x, pt.color.y, pt.color.z, powf(lt, 0.8f));
        Vec3 c = pt.pos;
        R::dynQuad(c - X + Y, c + X + Y, c + X - Y, c - X - Y, col);
    }

    // muzzle flash
    if (p.muzzleT > 0 && p.alive) {
        Vec3 dir = forwardOf(p);
        Vec3 muzz = eye + dir * 0.75f + rgt * lerpf(0.28f, 0.03f, p.adsT) + Vec3(0, lerpf(-0.14f, -0.04f, p.adsT), 0);
        float fsz = 0.25f + randf(0, 0.1f);
        Vec3 X = rgt * fsz, Y = upv * fsz;
        R::dynQuad(muzz - X + Y, muzz + X + Y, muzz + X - Y, muzz - X - Y, Vec4(1.0f, 0.8f, 0.35f, 0.9f));
        R::dynQuad(muzz - X * 0.5f + Y * 0.5f, muzz + X * 0.5f + Y * 0.5f,
                   muzz + X * 0.5f - Y * 0.5f, muzz - X * 0.5f - Y * 0.5f, Vec4(1.0f, 1.0f, 0.9f, 0.95f));
    }

    R::dynFlush();

    // ---- view model (clear depth so the gun never clips through walls)
    if (p.alive && g.state == GState::Playing) {
        glClear(GL_DEPTH_BUFFER_BIT);
        // hip carry blends to eye-aligned sights when aiming (sight sits 0.125 above the receiver)
        float kick = p.gunKick;
        Vec3 hipPos = rgt * (0.30f + kick * 0.04f) + Vec3(0, -0.34f + bobY * 0.5f + kick * 0.03f, 0) +
                      fwd * (0.55f - kick * 0.09f);
        Vec3 adsPos = rgt * (0.0f) + Vec3(0, -0.128f + kick * 0.015f, 0) + fwd * (0.34f - kick * 0.05f);
        Vec3 gunPos = eye + lerp(hipPos, adsPos, smooth01(p.adsT));
        // sway + reload dip + bodycam roll follow (sway fades out while aiming)
        float reloadDip = p.reloadT > 0 ? sinf(clampf(1.0f - p.reloadT / 1.6f, 0, 1) * PI) * 0.25f : 0.0f;
        gunPos.y -= reloadDip;
        float swayA = 1.0f - 0.75f * p.adsT;
        float roll = (sinf(p.bobPhase) * 0.02f + reloadDip * 0.5f + p.roll * 0.7f) * swayA;
        float aimYaw = sinf((float)g.time * 0.9f) * 0.004f * swayA;   // breathing drift
        float aimPitch = sinf((float)g.time * 1.3f) * 0.003f * swayA;

        // ---- procedural reload: magazine out with the left hand, fresh mag slapped in
        float r = p.reloadT > 0 ? clampf(1.0f - p.reloadT / 1.6f, 0, 1) : 0.0f;
        auto seg = [&](float a, float b) { return smooth01((r - a) / (b - a)); };
        // gun body: dips + cants while the mag is out
        float rGun = sinf(r * PI) * clampf(r * 4.0f, 0, 1);
        roll += rGun * 0.14f;
        aimPitch -= rGun * 0.20f;
        gunPos.y -= rGun * 0.05f;

        // hand animation waypoints (gun-local space; fingers point -Z)
        Vec3 restL(0.0f, -0.062f, -0.345f);
        float restRot = -0.9f;
        Vec3 gripL(0.012f, -0.185f, -0.175f);
        float gripRot = -1.35f;
        Vec3 outL(0.03f, -0.62f, 0.06f);
        // magazine travel (relative to its well at (0,-0.105,-0.20))
        Vec3 magWell(0, -0.105f, -0.20f);
        Vec3 magOff(0, 0, 0);
        float magRoll = 0;
        Vec3 handPos;
        float handRot;
        if (r < 0.22f) {                       // reach down to the mag
            float t = seg(0.0f, 0.22f);
            handPos = lerp(restL, gripL, t);
            handRot = lerpf(restRot, gripRot, t);
        } else if (r < 0.46f) {                // pull the mag out, hand follows
            float t = seg(0.22f, 0.46f);
            magOff = Vec3(0.0f, -0.17f * t, 0.03f * t);
            magRoll = 0.4f * t;
            handPos = lerp(gripL, gripL + Vec3(0.01f, -0.16f, 0.03f), t);
            handRot = lerpf(gripRot, gripRot - 0.2f, t);
        } else if (r < 0.56f) {                // mag swings out of frame
            float t = seg(0.46f, 0.56f);
            magOff = Vec3(0.02f * t, -0.17f - 0.5f * t, 0.03f + 0.2f * t);
            magRoll = 0.4f + 0.7f * t;
            handPos = lerp(gripL + Vec3(0.01f, -0.16f, 0.03f), outL, t);
            handRot = lerpf(gripRot - 0.2f, -1.1f, t);
        } else if (r < 0.78f) {                // fresh mag comes back up from below
            float t = seg(0.56f, 0.78f);
            magOff = Vec3(0.02f * (1 - t), -0.67f + 0.62f * t, 0.23f - 0.24f * t);
            magRoll = 1.1f - 1.0f * t;
            handPos = lerp(outL, gripL + Vec3(0.0f, -0.075f, 0.0f), t); // hand under the fresh mag
            handRot = lerpf(-1.1f, -1.45f, t);
        } else if (r < 0.90f) {                // insert + slap
            float t = seg(0.78f, 0.90f);
            magOff = Vec3(0, -0.05f * (1 - t), 0);
            magRoll = 0.1f * (1 - t);
            handPos = lerp(gripL + Vec3(0.0f, -0.075f, 0.0f), gripL, t);
            handRot = lerpf(-1.45f, gripRot, t);
        } else {                               // release, hand back to the guard
            float t = seg(0.90f, 1.0f);
            magOff = Vec3(0, 0, 0);
            handPos = lerp(gripL, restL, t);
            handRot = lerpf(gripRot, restRot, t);
        }
        if (p.reloadT <= 0) {
            // idle: support hand wrapped on the handguard, trigger hand on the grip
            handPos = restL;
            handRot = restRot;
            magOff = Vec3(0, 0, 0);
        }

        // gun model front is -Z; -yaw maps it onto the view direction
        Mat4 model = matTRS(gunPos, -p.yaw + aimYaw, p.pitch + p.recoil * 6.0f + aimPitch, roll, Vec3(1, 1, 1));
        auto vmPart = [&](Mesh& m, Mat4 local, Vec3 emissive) {
            R::drawMesh(m, matMul(model, local), Vec3(1, 1, 1), emissive);
        };
        vmPart(meshGun, Mat4(), p.muzzleT > 0 ? Vec3(0.4f, 0.3f, 0.1f) : Vec3(0, 0, 0));
        vmPart(meshMagVM, matMul(matTranslate(magWell + magOff), matRotateZ(magRoll)), Vec3(0, 0, 0));
        // left (support) hand
        vmPart(meshHandL, matMul(matTranslate(handPos), matRotateX(handRot)), Vec3(0, 0, 0));
        // right (trigger) hand on the grip — same mesh mirrored 180°
        vmPart(meshHandL, matMul(matTranslate(Vec3(-0.005f, -0.135f, 0.03f)),
                                 matMul(matRotateZ(PI), matRotateX(-0.55f))), Vec3(0, 0, 0));
    }

    // composite: fisheye, grain, vignette straight to the screen
    R::endScene((float)g.time);

    // ---- HUD
    if (g.state == GState::Playing || g.state == GState::GameOver) {
        R::begin2D(w, h);
        ui::drawHUD(g, vp, w, h);
        R::end2D();
    }
}

// ------------------------------------------------------------- GLFW
static Game G;
static Input input;
static bool cursorLocked = false;

static void keyCallback(GLFWwindow* win, int key, int scancode, int action, int mods) {
    (void)win; (void)scancode; (void)mods;
    if (key < 0 || key >= 350) return;
    if (action == GLFW_PRESS) {
        input.key[key] = true;
        input.keyPressed[key] = true;
    } else if (action == GLFW_RELEASE) {
        input.key[key] = false;
    }
}
static void mouseButtonCallback(GLFWwindow* win, int button, int action, int mods) {
    (void)win; (void)mods;
    if (button < 0 || button >= 8) return;
    if (action == GLFW_PRESS) {
        input.mouseDown[button] = true;
        input.mousePressed[button] = true;
    } else {
        input.mouseDown[button] = false;
    }
}
static void cursorPosCallback(GLFWwindow*, double x, double y) {
    static double lastX = 0, lastY = 0;
    static bool first = true;
    if (first) { lastX = x; lastY = y; first = false; return; }
    input.mouseDX += (float)(x - lastX);
    input.mouseDY += (float)(y - lastY);
    lastX = x;
    lastY = y;
}
static void scrollCallback(GLFWwindow*, double, double dy) {
    input.wheel += (float)dy;
}
static void resizeCallback(GLFWwindow*, int w, int h) {
    R::resize(w, h);
}

static void setCursorLock(GLFWwindow* win, bool lock) {
    if (lock == cursorLocked) return;
    cursorLocked = lock;
    glfwSetInputMode(win, GLFW_CURSOR, lock ? GLFW_CURSOR_DISABLED : GLFW_CURSOR_NORMAL);
    if (!lock) {
        // center-free: reset deltas so no jump on unlock
        input.mouseDX = input.mouseDY = 0;
    }
}

int main() {
    if (!glfwInit()) {
        fprintf(stderr, "glfwInit failed\n");
        return 1;
    }
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_SAMPLES, 4);
    GLFWwindow* win = glfwCreateWindow(1280, 720, "NEON ARENA - low poly FPS", nullptr, nullptr);
    if (!win) {
        fprintf(stderr, "Window creation failed (GL 3.3 core required)\n");
        glfwTerminate();
        return 1;
    }
    glfwMakeContextCurrent(win);
    glfwSetKeyCallback(win, keyCallback);
    glfwSetMouseButtonCallback(win, mouseButtonCallback);
    glfwSetCursorPosCallback(win, cursorPosCallback);
    glfwSetScrollCallback(win, scrollCallback);
    glfwSetFramebufferSizeCallback(win, resizeCallback);
    glfwSwapInterval(1);

    if (!R::init()) { glfwTerminate(); return 1; }
    audio::init();

    int w = 1280, h = 720;
    glfwGetFramebufferSize(win, &w, &h);
    R::resize(w, h);

    // static geometry: world module owns the visual mesh + colliders
    world::build();
    meshWorld = R::buildMesh(world::sceneBuilder());
    buildEntities();

    double lastTime = glfwGetTime();
    while (!glfwWindowShouldClose(win)) {
        double now = glfwGetTime();
        float dt = (float)std::min(now - lastTime, 0.05);
        lastTime = now;

        // clear per-frame edge triggers
        for (int i = 0; i < 350; i++) input.keyPressed[i] = false;
        for (int i = 0; i < 8; i++) input.mousePressed[i] = false;
        float mdx = input.mouseDX, mdy = input.mouseDY;
        input.mouseDX = input.mouseDY = 0;
        float wheel = input.wheel;
        input.wheel = 0;

        glfwPollEvents();

        // sensitivity adjust with wheel (menu / pause)
        if (G.state != GState::Playing && wheel != 0) {
            G.sens = clampf(G.sens + wheel * 0.0004f, 0.0004f, 0.012f);
        }

        // state machine keys
        if (G.state == GState::Menu) {
            setCursorLock(win, false);
            G.time += dt;
            if (input.mousePressed[GLFW_MOUSE_BUTTON_LEFT]) {
                game::reset(G);
            }
        } else if (G.state == GState::Playing) {
            setCursorLock(win, true);
            if (input.keyPressed[GLFW_KEY_ESCAPE]) G.state = GState::Paused;
        } else if (G.state == GState::Paused) {
            setCursorLock(win, false);
            G.time += dt;
            if (input.keyPressed[GLFW_KEY_ESCAPE]) G.state = GState::Playing;
            if (input.keyPressed[GLFW_KEY_ENTER]) game::reset(G);
        } else if (G.state == GState::GameOver) {
            setCursorLock(win, false);
            G.time += dt;
            if (G.deathT > 1.0f) {
                if (input.keyPressed[GLFW_KEY_ENTER]) game::reset(G);
                if (input.keyPressed[GLFW_KEY_ESCAPE]) G.state = GState::Menu;
            }
        }

        // simulate
        if (G.state == GState::Playing || G.state == GState::GameOver) {
            game::update(G, input, dt);
        }

        // ---- render 3D + HUD
        drawScene(G, R::vpW, R::vpH);

        // ---- overlays per state
        R::begin2D(R::vpW, R::vpH);
        if (G.state == GState::Menu) ui::drawMenu(G, R::vpW, R::vpH);
        else if (G.state == GState::Paused) ui::drawPause(G, R::vpW, R::vpH);
        else if (G.state == GState::GameOver) ui::drawGameOver(G, R::vpW, R::vpH);
        R::end2D();

        audio::updateListener(G.player.pos + Vec3(0, 0.7f, 0),
                              Vec3(sinf(G.player.yaw), 0, -cosf(G.player.yaw)));

        glfwSwapBuffers(win);
    }

    audio::shutdown();
    glfwDestroyWindow(win);
    glfwTerminate();
    return 0;
}
