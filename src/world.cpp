// world.cpp — the low-poly arena: geometry baked into static meshes + AABB colliders
#include "world.h"
#include "glrender.h"

namespace world {

std::vector<Collider> colliders;
const float ARENA = 30.0f; // playable half-extent

Vec3 spawnPads[8] = {
    {-26, 0, -26}, {26, 0, -26}, {-26, 0, 26}, {26, 0, 26},
    {0, 0, -27},   {0, 0, 27},   {-27, 0, 0}, {27, 0, 0},
};
Vec3 pickupPads[6] = {
    {-14, 0, 0}, {14, 0, 0}, {0, 0, -14}, {0, 0, 14}, {-20, 0, -20}, {20, 0, 20},
};

// ----------------------------------------------------------- collision
static bool overlaps(const Collider& c, Vec3 pos, Vec3 half) {
    return pos.x + half.x > c.min.x && pos.x - half.x < c.max.x &&
           pos.y + half.y > c.min.y && pos.y - half.y < c.max.y &&
           pos.z + half.z > c.min.z && pos.z - half.z < c.max.z;
}

MoveResult moveAABB(Vec3 pos, Vec3 vel, Vec3 half, float dt) {
    MoveResult r;
    r.pos = pos;
    r.vel = vel;
    const float eps = 0.001f;

    // Y axis
    r.pos.y += vel.y * dt;
    for (const Collider& c : colliders) {
        if (!overlaps(c, r.pos, half)) continue;
        if (vel.y <= 0) {
            r.pos.y = c.max.y + half.y + eps;
            r.hitGround = true;
        } else {
            r.pos.y = c.min.y - half.y - eps;
            r.hitCeiling = true;
        }
        r.vel.y = 0;
    }

    // X axis
    r.pos.x += vel.x * dt;
    for (const Collider& c : colliders) {
        if (!overlaps(c, r.pos, half)) continue;
        r.pos.x = vel.x > 0 ? c.min.x - half.x - eps : c.max.x + half.x + eps;
        r.vel.x = 0;
        r.hitWall = true;
    }

    // Z axis
    r.pos.z += vel.z * dt;
    for (const Collider& c : colliders) {
        if (!overlaps(c, r.pos, half)) continue;
        r.pos.z = vel.z > 0 ? c.min.z - half.z - eps : c.max.z + half.z + eps;
        r.vel.z = 0;
        r.hitWall = true;
    }
    return r;
}

bool raycast(Vec3 o, Vec3 d, float maxT, float* outT, Vec3* outNormal) {
    bool hit = false;
    float bestT = maxT;
    Vec3 bestN;
    float dn[3] = {d.x, d.y, d.z};
    float on[3] = {o.x, o.y, o.z};
    for (const Collider& c : colliders) {
        float cmin[3] = {c.min.x, c.min.y, c.min.z};
        float cmax[3] = {c.max.x, c.max.y, c.max.z};
        float tmin = 0.0f, tmax = bestT;
        Vec3 n;
        bool valid = true;
        for (int a = 0; a < 3 && valid; a++) {
            if (fabsf(dn[a]) < 1e-8f) {
                if (on[a] < cmin[a] || on[a] > cmax[a]) valid = false;
                continue;
            }
            float inv = 1.0f / dn[a];
            float t1 = (cmin[a] - on[a]) * inv;
            float t2 = (cmax[a] - on[a]) * inv;
            float sign = -1.0f;
            if (t1 > t2) {
                float tmp = t1; t1 = t2; t2 = tmp;
                sign = 1.0f;
            }
            if (t1 > tmin) {
                tmin = t1;
                n = Vec3(0, 0, 0);
                if (a == 0) n.x = sign;
                if (a == 1) n.y = sign;
                if (a == 2) n.z = sign;
            }
            tmax = std::min(tmax, t2);
            if (tmin > tmax) valid = false;
        }
        if (valid && tmin > 0.0001f && tmin < bestT) {
            bestT = tmin;
            bestN = n;
            hit = true;
        }
    }
    if (hit) {
        if (outT) *outT = bestT;
        if (outNormal) *outNormal = bestN;
    }
    return hit;
}

// ----------------------------------------------------------- geometry
static void addBlockMesh(MeshBuilder& mb, Vec3 center, Vec3 half, Vec3 col) {
    mb.box(center, half, col);
    colliders.push_back({center - half, center + half});
}

static MeshBuilder scene_mb;
MeshBuilder& sceneBuilder() { return scene_mb; }

void build() {
    MeshBuilder& mb = scene_mb;
    // overcast military palette
    Vec3 floorA(0.295f, 0.295f, 0.305f);  // worn concrete
    Vec3 floorB(0.325f, 0.325f, 0.335f);
    Vec3 edgeC(0.145f, 0.145f, 0.155f);
    Vec3 wallC(0.395f, 0.385f, 0.365f);   // concrete walls
    Vec3 wallTop(0.24f, 0.24f, 0.25f);
    Vec3 crateC(0.33f, 0.37f, 0.27f);     // military green
    Vec3 crateDark(0.43f, 0.37f, 0.27f);  // tan
    Vec3 pillarC(0.455f, 0.455f, 0.465f); // concrete pillars
    Vec3 coverC(0.415f, 0.415f, 0.43f);
    Vec3 barrelRust(0.47f, 0.26f, 0.20f);
    Vec3 barrelOlive(0.29f, 0.31f, 0.21f);

    // Floor collider (the visible grid mesh sits exactly at y=0)
    colliders.push_back({{-ARENA - 2, -1, -ARENA - 2}, {ARENA + 2, 0, ARENA + 2}});

    // Perimeter walls (4 slabs, height 5)
    float wh = 2.5f;
    addBlockMesh(mb, {0, wh, -ARENA - 0.5f}, {ARENA + 1.0f, wh, 0.5f}, wallC);
    addBlockMesh(mb, {0, wh, ARENA + 0.5f}, {ARENA + 1.0f, wh, 0.5f}, wallC);
    addBlockMesh(mb, {-ARENA - 0.5f, wh, 0}, {0.5f, wh, ARENA + 1.0f}, wallC);
    addBlockMesh(mb, {ARENA + 0.5f, wh, 0}, {0.5f, wh, ARENA + 1.0f}, wallC);
    // Wall trim
    mb.box({0, 5.05f, -ARENA - 0.5f}, {ARENA + 1.0f, 0.1f, 0.52f}, wallTop);
    mb.box({0, 5.05f, ARENA + 0.5f}, {ARENA + 1.0f, 0.1f, 0.52f}, wallTop);
    mb.box({-ARENA - 0.5f, 5.05f, 0}, {0.52f, 0.1f, ARENA + 1.0f}, wallTop);
    mb.box({ARENA + 0.5f, 5.05f, 0}, {0.52f, 0.1f, ARENA + 1.0f}, wallTop);

    // Four round concrete pillars with a small work light on top
    for (int i = 0; i < 4; i++) {
        float sx = (i & 1) ? 1 : -1, sz = (i & 2) ? 1 : -1;
        mb.cylinder({sx * 11.0f, 2.0f, sz * 11.0f}, 1.5f, 4.0f, 10, pillarC, pillarC * 0.9f);
        colliders.push_back({{sx * 11.0f - 1.15f, 0, sz * 11.0f - 1.15f},
                             {sx * 11.0f + 1.15f, 4.0f, sz * 11.0f + 1.15f}});
        mb.box({sx * 11.0f, 4.25f, sz * 11.0f}, {0.35f, 0.18f, 0.35f}, {0.2f, 0.2f, 0.21f});
        mb.box({sx * 11.0f, 4.12f, sz * 11.0f}, {0.28f, 0.05f, 0.28f}, Color::amber() * 0.55f);
    }

    // Central cover: jersey barriers
    addBlockMesh(mb, {0, 0.55f, -4.5f}, {2.6f, 0.55f, 0.42f}, coverC);
    addBlockMesh(mb, {0, 1.25f, -4.5f}, {2.2f, 0.15f, 0.30f}, coverC);
    addBlockMesh(mb, {0, 0.55f, 4.5f}, {2.6f, 0.55f, 0.42f}, coverC);
    addBlockMesh(mb, {0, 1.25f, 4.5f}, {2.2f, 0.15f, 0.30f}, coverC);
    addBlockMesh(mb, {-4.5f, 0.55f, 0}, {0.42f, 0.55f, 2.6f}, coverC);
    addBlockMesh(mb, {-4.5f, 1.25f, 0}, {0.30f, 0.15f, 2.2f}, coverC);
    addBlockMesh(mb, {4.5f, 0.55f, 0}, {0.42f, 0.55f, 2.6f}, coverC);
    addBlockMesh(mb, {4.5f, 1.25f, 0}, {0.30f, 0.15f, 2.2f}, coverC);

    // Supply crates (2x2 stacks & singles)
    auto crate = [&](float x, float z, float s, float y) {
        addBlockMesh(mb, {x, y + s * 0.5f, z}, {s * 0.5f, s * 0.5f, s * 0.5f},
                     ((int)(x + z) & 1) ? crateC : crateDark);
    };
    crate(18, 18, 1.8f, 0); crate(20, 18, 1.8f, 0); crate(18, 20, 1.8f, 0); crate(19, 19, 1.8f, 1.8f);
    crate(-18, -18, 1.8f, 0); crate(-20, -18, 1.8f, 0); crate(-18, -20, 1.8f, 0); crate(-19, -19, 1.8f, 1.8f);
    crate(-20, 14, 1.6f, 0); crate(20, -14, 1.6f, 0); crate(8, -22, 1.6f, 0);
    crate(-8, 22, 1.6f, 0); crate(-24, 6, 1.6f, 0); crate(24, -6, 1.6f, 0);

    // Oil barrels (round, colliders approximate)
    auto barrel = [&](float x, float z, Vec3 col) {
        mb.cylinder({x, 0.55f, z}, 0.55f, 1.1f, 10, col, col * 0.85f);
        mb.cylinder({x, 0.57f, z}, 0.57f, 0.06f, 10, col * 0.8f, col * 0.8f); // rib
        mb.cylinder({x, 0.92f, z}, 0.57f, 0.06f, 10, col * 0.8f, col * 0.8f);
        colliders.push_back({{x - 0.48f, 0, z - 0.48f}, {x + 0.48f, 1.1f, z + 0.48f}});
    };
    barrel(12.0f, -12.0f, barrelRust);
    barrel(13.1f, -11.2f, barrelOlive);
    barrel(-12.0f, 12.0f, barrelRust);
    barrel(-13.1f, 11.2f, barrelOlive);
    barrel(-6.0f, -14.0f, barrelOlive);
    barrel(6.0f, 14.0f, barrelRust);

    // Long cover walls
    addBlockMesh(mb, {-9.0f, 1.2f, -19.0f}, {4.0f, 1.2f, 0.7f}, coverC);
    addBlockMesh(mb, {9.0f, 1.2f, 19.0f}, {4.0f, 1.2f, 0.7f}, coverC);
    addBlockMesh(mb, {-19.0f, 1.2f, 9.0f}, {0.7f, 1.2f, 4.0f}, coverC);
    addBlockMesh(mb, {19.0f, 1.2f, -9.0f}, {0.7f, 1.2f, 4.0f}, coverC);

    // Corner watch posts (decor + light)
    for (int i = 0; i < 4; i++) {
        float sx = (i & 1) ? 1 : -1, sz = (i & 2) ? 1 : -1;
        addBlockMesh(mb, {sx * 27.0f, 1.6f, sz * 27.0f}, {0.5f, 1.6f, 0.5f}, pillarC);
        mb.box({sx * 27.0f, 3.4f, sz * 27.0f}, {0.55f, 0.2f, 0.55f}, Color::amber() * 0.5f);
    }

    // ================= DETAIL PASS =================
    // generic cylinder along an axis (0=X, 1=Y, 2=Z)
    auto cyl = [&](Vec3 c, float r, float len, int axis, int sides, Vec3 col) {
        for (int i = 0; i < sides; i++) {
            float a0 = TAU * (float)i / sides, a1 = TAU * (float)(i + 1) / sides;
            Vec3 d0, d1, n;
            if (axis == 0) { d0 = {0, cosf(a0) * r, sinf(a0) * r}; d1 = {0, cosf(a1) * r, sinf(a1) * r}; n = {0, cosf((a0+a1)/2), sinf((a0+a1)/2)}; }
            else if (axis == 1) { d0 = {cosf(a0) * r, 0, sinf(a0) * r}; d1 = {cosf(a1) * r, 0, sinf(a1) * r}; n = {cosf((a0+a1)/2), 0, sinf((a0+a1)/2)}; }
            else { d0 = {cosf(a0) * r, sinf(a0) * r, 0}; d1 = {cosf(a1) * r, sinf(a1) * r, 0}; n = {cosf((a0+a1)/2), sinf((a0+a1)/2), 0}; }
            Vec3 ext = axis == 0 ? Vec3(len * 0.5f, 0, 0) : axis == 1 ? Vec3(0, len * 0.5f, 0) : Vec3(0, 0, len * 0.5f);
            // (start - tangent -> start + tangent -> end + tangent -> end - tangent) winds outward
            mb.quadNormal(c - ext + d0, c - ext + d1, c + ext + d1, c + ext + d0, norm(n), col);
        }
        // end caps as triangle fans
        Vec3 eA = axis == 0 ? Vec3(len * 0.5f, 0, 0) : axis == 1 ? Vec3(0, len * 0.5f, 0) : Vec3(0, 0, len * 0.5f);
        Vec3 eB = -eA;
        uint32_t base = (uint32_t)mb.verts.size();
        Vec3 nA = axis == 0 ? Vec3(1, 0, 0) : axis == 1 ? Vec3(0, 1, 0) : Vec3(0, 0, 1);
        mb.verts.push_back({c + eA, nA, {col.x, col.y, col.z, 1}});
        mb.verts.push_back({c + eB, -nA, {col.x, col.y, col.z, 1}});
        for (int i = 0; i <= sides; i++) {
            float a = TAU * (float)i / sides;
            Vec3 d;
            if (axis == 0) d = {0, cosf(a) * r, sinf(a) * r};
            else if (axis == 1) d = {cosf(a) * r, 0, sinf(a) * r};
            else d = {cosf(a) * r, sinf(a) * r, 0};
            mb.verts.push_back({c + eA + d, nA, {col.x, col.y, col.z, 1}});
            mb.verts.push_back({c + eB + d, -nA, {col.x, col.y, col.z, 1}});
        }
        for (int i = 0; i < sides; i++) {
            uint32_t ca = base + 2 + i * 2;
            // +Y-facing cap: center, higher angle, lower angle
            mb.idx.push_back(base); mb.idx.push_back(ca + 2); mb.idx.push_back(ca);
            mb.idx.push_back(base + 1); mb.idx.push_back(ca + 1); mb.idx.push_back(ca + 3);
        }
    };
    // flat polygon on the floor (triangle fan), for stains & paint
    auto floorPoly = [&](Vec3 c, float r, int n, float rot, Vec3 col) {
        uint32_t base = (uint32_t)mb.verts.size();
        Vec3 nn(0, 1, 0);
        mb.verts.push_back({c + Vec3(0, 0.004f, 0), nn, {col.x, col.y, col.z, 1}});
        for (int i = 0; i <= n; i++) {
            float a = rot + TAU * (float)i / n + sinf((float)i * 2.7f) * 0.35f;
            float rr = r * (0.75f + 0.35f * sinf((float)i * 1.9f + rot));
            mb.verts.push_back({c + Vec3(cosf(a) * rr, 0.004f, sinf(a) * rr), nn, {col.x, col.y, col.z, 1}});
        }
        for (int i = 1; i <= n; i++) {
            mb.idx.push_back(base); mb.idx.push_back(base + i + 1); mb.idx.push_back(base + i);
        }
    };
    Vec3 paintY(0.62f, 0.52f, 0.16f), paintW(0.66f, 0.67f, 0.66f);
    Vec3 stainC(0.13f, 0.12f, 0.11f), stainB(0.16f, 0.13f, 0.10f);

    // --- painted floor markings: hazard chevrons around the center barriers
    for (int i = 0; i < 4; i++) {
        float a = TAU * (float)i / 4 + PI / 4;
        Vec3 c{cosf(a) * 7.4f, 0, sinf(a) * 7.4f};
        for (int k = 0; k < 3; k++) {
            float ca = cosf(a), sa = sinf(a);
            float off = (k - 1) * 0.8f;
            Vec3 p = c + Vec3(-sa, 0, ca) * off;
            Vec3 along = Vec3(ca, 0, sa);
            Vec3 side = Vec3(-sa, 0, ca);
            mb.quadNormal(p - along * 0.6f + side * 0.22f + Vec3(0, 0.004f, 0),
                          p + along * 0.6f + side * 0.22f + Vec3(0, 0.004f, 0),
                          p + along * 0.6f - side * 0.22f + Vec3(0, 0.004f, 0),
                          p - along * 0.6f - side * 0.22f + Vec3(0, 0.004f, 0), {0, 1, 0}, paintY);
        }
    }
    // white ring around the central cross
    for (int i = 0; i < 40; i++) {
        float a0 = TAU * (float)i / 40, a1 = TAU * (float)(i + 1) / 40;
        if (fabsf(cosf((a0 + a1) * 0.5f)) > 0.86f && fabsf(sinf((a0 + a1) * 0.5f)) > 0.86f) continue; // gaps at walkways
        mb.quadNormal({cosf(a0) * 6.2f, 0.004f, sinf(a0) * 6.2f}, {cosf(a1) * 6.2f, 0.004f, sinf(a1) * 6.2f},
                      {cosf(a1) * 6.5f, 0.004f, sinf(a1) * 6.5f}, {cosf(a0) * 6.5f, 0.004f, sinf(a0) * 6.5f},
                      {0, 1, 0}, paintW * 0.8f);
    }

    // --- oil & fluid stains
    floorPoly({5.5f, 0, 8.0f}, 1.4f, 9, 0.4f, stainC);
    floorPoly({6.9f, 0, 8.7f}, 0.8f, 8, 2.1f, stainB);
    floorPoly({-7.0f, 0, -3.0f}, 1.7f, 10, 1.2f, stainC);
    floorPoly({12.0f, 0, -17.5f}, 1.1f, 8, 3.3f, stainB);
    floorPoly({-2.0f, 0, 17.0f}, 1.3f, 9, 5.0f, stainC);
    floorPoly({23.0f, 0, 22.5f}, 0.9f, 7, 2.8f, stainB);

    // --- scattered debris: grit, rocks, shrapnel (kept clear of pads)
    for (int i = 0; i < 26; i++) {
        float x = randf(-27.0f, 27.0f), z = randf(-27.0f, 27.0f);
        bool nearPad = false;
        for (Vec3& s : spawnPads) if (len(Vec3(x - s.x, 0, z - s.z)) < 3.5f) nearPad = true;
        for (Vec3& s : pickupPads) if (len(Vec3(x - s.x, 0, z - s.z)) < 3.0f) nearPad = true;
        if (nearPad || len(Vec3(x, 0, z)) < 3.0f) continue;
        float s = randf(0.06f, 0.16f);
        mb.box({x, s * 0.4f, z}, {s, s * 0.4f, s * randf(0.7f, 1.3f)},
               Vec3(0.24f, 0.23f, 0.22f) * randf(0.8f, 1.2f));
    }

    // --- floodlight poles (4 corners)
    for (int i = 0; i < 4; i++) {
        float sx = (i & 1) ? 1 : -1, sz = (i & 2) ? 1 : -1;
        float px = sx * 23.0f, pz = sz * 23.0f;
        cyl({px, 3.0f, pz}, 0.09f, 6.0f, 1, 8, {0.30f, 0.30f, 0.32f});
        colliders.push_back({{px - 0.14f, 0, pz - 0.14f}, {px + 0.14f, 6.0f, pz + 0.14f}});
        // mast arm + lamp head aimed at the arena center
        Vec3 armEnd{px - sx * 1.1f, 5.7f, pz};
        cyl({(px + armEnd.x) * 0.5f, 5.7f, pz}, 0.06f, 1.2f, 0, 6, {0.30f, 0.30f, 0.32f});
        mb.box({armEnd.x, 5.6f, armEnd.z}, {0.28f, 0.14f, 0.28f}, {0.18f, 0.18f, 0.19f});
        mb.box({armEnd.x, 5.47f, armEnd.z}, {0.24f, 0.05f, 0.24f}, Vec3(0.95f, 0.93f, 0.85f));
        // base plate
        mb.cylinder({px, 0.10f, pz}, 0.30f, 0.2f, 8, {0.26f, 0.26f, 0.27f}, {0.22f, 0.22f, 0.23f});
    }

    // --- dumpsters (green industrial) with slightly open lids
    Vec3 dumpC(0.16f, 0.30f, 0.22f), dumpLid(0.13f, 0.24f, 0.18f);
    for (int i = 0; i < 2; i++) {
        float dx = i ? 25.0f : -25.0f, dz = i ? -8.0f : 8.0f;
        mb.box({dx, 0.65f, dz}, {1.25f, 0.65f, 0.60f}, dumpC);
        mb.box({dx, 1.34f, dz + 0.08f}, {1.25f, 0.07f, 0.58f}, dumpLid);
        colliders.push_back({{dx - 1.25f, 0, dz - 0.62f}, {dx + 1.25f, 1.3f, dz + 0.62f}});
        cyl({dx - 1.1f, 0.12f, dz - 0.68f}, 0.09f, 0.18f, 1, 6, {0.1f, 0.1f, 0.1f});
        cyl({dx + 1.1f, 0.12f, dz - 0.68f}, 0.09f, 0.18f, 1, 6, {0.1f, 0.1f, 0.1f});
    }

    // --- pallet stacks with cargo crates on top
    Vec3 palletC(0.48f, 0.38f, 0.24f);
    for (int i = 0; i < 2; i++) {
        float dx = i ? 16.0f : -16.0f, dz = i ? -24.0f : 24.0f;
        for (int lvl = 0; lvl < 2; lvl++) {
            float y = lvl * 0.17f;
            for (int sl = 0; sl < 5; sl++)
                mb.box({dx + (sl - 2) * 0.30f, 0.07f + y, dz}, {0.13f, 0.035f, 0.65f}, palletC * (sl % 2 ? 1.0f : 0.9f));
            mb.box({dx, 0.13f + y, dz - 0.58f}, {0.68f, 0.045f, 0.07f}, palletC);
            mb.box({dx, 0.13f + y, dz + 0.58f}, {0.68f, 0.045f, 0.07f}, palletC);
        }
        colliders.push_back({{dx - 0.7f, 0, dz - 0.68f}, {dx + 0.7f, 0.34f, dz + 0.68f}});
        mb.box({dx, 0.55f, dz}, {0.55f, 0.2f, 0.55f}, ((int)dx & 1) ? crateDark : crateC);
    }

    // --- sandbag nests (round bags, stacked 2 high)
    Vec3 bagC(0.42f, 0.38f, 0.28f), bagD(0.36f, 0.32f, 0.23f);
    for (int i = 0; i < 2; i++) {
        float bx = i ? 15.0f : -15.0f, bz = i ? 15.0f : -15.0f;
        for (int row = 0; row < 2; row++)
            for (int b = 0; b < 4; b++) {
                float off = (b - 1.5f) * 0.52f + row * 0.26f;
                float y = 0.17f + row * 0.28f;
                cyl({bx + off, y, bz - row * 0.22f}, 0.17f, 0.5f, 0, 7, (b + row) % 2 ? bagC : bagD);
            }
        colliders.push_back({{bx - 1.35f, 0, bz - 0.45f}, {bx + 1.35f, 0.75f, bz + 0.15f}});
    }

    // --- AC / generator units with fan grilles
    for (int i = 0; i < 2; i++) {
        float dx = i ? 10.0f : -10.0f;
        mb.box({dx, 0.55f, 0}, {0.75f, 0.55f, 0.65f}, {0.36f, 0.37f, 0.38f});
        mb.box({dx, 1.13f, 0}, {0.78f, 0.04f, 0.68f}, {0.28f, 0.29f, 0.30f});
        cyl({dx, 1.16f, 0}, 0.30f, 0.04f, 1, 10, {0.20f, 0.21f, 0.22f});
        colliders.push_back({{dx - 0.78f, 0, -0.68f}, {dx + 0.78f, 1.15f, 0.68f}});
        for (int v = 0; v < 3; v++)
            mb.box({dx + (i ? 0.77f : -0.77f), 0.5f + v * 0.28f, 0}, {0.012f, 0.015f, 0.5f}, {0.24f, 0.25f, 0.26f});
    }

    // --- pipes running along two walls with brackets
    Vec3 pipeC(0.30f, 0.31f, 0.32f), pipeR(0.42f, 0.28f, 0.20f);
    for (int wall = 0; wall < 2; wall++) {
        float wz = wall ? 29.35f : -29.35f;
        cyl({0, 2.4f, wz}, 0.13f, 56.0f, 0, 8, pipeC);
        cyl({0, 1.9f, wz}, 0.10f, 56.0f, 0, 8, pipeR);
        for (int k = 0; k < 7; k++) {
            float bx = -24.0f + k * 8.0f;
            mb.box({bx, 2.4f, wz + (wall ? -0.3f : 0.3f)}, {0.08f, 0.22f, 0.28f}, {0.2f, 0.2f, 0.21f});
            mb.box({bx, 1.9f, wz + (wall ? -0.3f : 0.3f)}, {0.08f, 0.18f, 0.28f}, {0.2f, 0.2f, 0.21f});
        }
        // valve wheels
        cyl({-12.0f, 2.4f, wz + (wall ? -0.35f : 0.35f)}, 0.22f, 0.08f, 0, 8, {0.45f, 0.3f, 0.18f});
        cyl({12.0f, 2.4f, wz + (wall ? -0.35f : 0.35f)}, 0.22f, 0.08f, 0, 8, {0.45f, 0.3f, 0.18f});
    }
}

} // namespace world
