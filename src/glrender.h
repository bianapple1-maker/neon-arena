// glrender.h — OpenGL 3.3 renderer: shaders, meshes, dynamic batches, bitmap font
#pragma once
#include "core.h"

struct Mesh {
    unsigned vao = 0, vbo = 0, ibo = 0;
    int count = 0; // index count
    bool ok = false;
};

struct Vertex {
    Vec3 p, n;
    Vec4 c;
};

// Mesh builder helpers -------------------------------------------------
struct MeshBuilder {
    std::vector<Vertex> verts;
    std::vector<uint32_t> idx;
    void clear() { verts.clear(); idx.clear(); }
    void quad(Vec3 a, Vec3 b, Vec3 c, Vec3 d, Vec3 col, float alpha = 1); // CCW facing +Z, auto normal
    void quadNormal(Vec3 a, Vec3 b, Vec3 c, Vec3 d, Vec3 n, Vec3 col, float alpha = 1);
    void box(Vec3 center, Vec3 half, Vec3 col, float alpha = 1);
    void boxFaces(Vec3 center, Vec3 half, Vec3 cTop, Vec3 cSide, Vec3 cBottom);
    void cylinder(Vec3 center, float radius, float height, int sides, Vec3 col, Vec3 capCol);
    void taperedCylinder(Vec3 center, float rBottom, float rTop, float height, int sides, Vec3 col);
    void sphere(Vec3 center, float radius, int stacks, int slices, Vec3 col);
    void grid(Vec3 origin, int cells, float cell, Vec3 colA, Vec3 colB, Vec3 colEdge);
};

namespace R {
bool init();                                  // GL loader + shaders + font
void resize(int w, int h);

// Post-processing: scene renders into an offscreen buffer, then gets drawn
// with fisheye/barrel distortion, grain, chromatic aberration & vignette.
void beginScene();                            // binds the FBO (then clear as usual)
void endScene(float time);                    // draws the distorted image to screen

// 3D pass ------------------------------------------------------------
void begin3D(const Mat4& proj, const Mat4& view, Vec3 camPos, Vec3 fogColor, Vec2 fogRange);
void drawMesh(const Mesh& m, const Mat4& model, Vec3 tint = {1, 1, 1},
              Vec3 emissive = {0, 0, 0}, float alpha = 1);

// Dynamic 3D batch (same shader, world-space quads, per-vertex alpha) --
void dynClear();
void dynQuad(Vec3 a, Vec3 b, Vec3 c, Vec3 d, Vec4 color); // CCW, faces viewer-ish
void dynFlush();

// 2D/HUD pass ----------------------------------------------------------
void begin2D(int w, int h);
void quad2D(float x, float y, float w, float h, Vec4 color);
void quadUV(float x, float y, float w, float h, Vec4 color, Vec2 uv0, Vec2 uv1);
void end2D();

// Text (5x7 bitmap font, white alpha atlas) ----------------------------
void text(float x, float y, float size, Vec4 color, const std::string& s, int align = 0); // align: 0 left, 1 center, 2 right
float textW(float size, const std::string& s);

// Mesh factories -------------------------------------------------------
Mesh buildMesh(const MeshBuilder& mb);
Mesh makeBoxMesh(Vec3 half, Vec3 col);
void freeMesh(Mesh& m);

extern int vpW, vpH;
}
