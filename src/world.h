// world.h — arena geometry, AABB colliders, raycasts & swept movement
#pragma once
#include "core.h"
#include "glrender.h"

struct Collider {
    Vec3 min, max;
};

struct MoveResult {
    Vec3 pos, vel;
    bool hitGround = false;
    bool hitWall = false;
    bool hitCeiling = false;
};

namespace world {
extern std::vector<Collider> colliders;
extern const float ARENA; // half-extent

MeshBuilder& sceneBuilder(); // visual geometry appended by build()

void build(); // constructs static arena (once)

// Ray vs all colliders; returns true + t (distance) + surface normal.
bool raycast(Vec3 o, Vec3 d, float maxT, float* outT = nullptr, Vec3* outNormal = nullptr);

// Axis-separated AABB slide; vel is modified (zeroed on blocked axes).
MoveResult moveAABB(Vec3 pos, Vec3 vel, Vec3 half, float dt);

extern Vec3 spawnPads[8];
extern Vec3 pickupPads[6]; // alternating health / ammo pads
}
