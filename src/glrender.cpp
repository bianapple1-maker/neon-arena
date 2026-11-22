// glrender.cpp — OpenGL 3.3 core renderer with a tiny hand-rolled loader
// GL 1.1-era functions come from gl.h (via glfw3.h); everything 2.0+ is loaded
// through glfwGetProcAddress into the pointers below.
#include "glrender.h"
#include <GLFW/glfw3.h>

// Fallback constants in case the system gl.h predates GL 2.0
#ifndef GL_VERTEX_SHADER
#define GL_VERTEX_SHADER 0x8B31
#define GL_FRAGMENT_SHADER 0x8B30
#define GL_COMPILE_STATUS 0x8B81
#define GL_LINK_STATUS 0x8B82
#define GL_ARRAY_BUFFER 0x8892
#define GL_ELEMENT_ARRAY_BUFFER 0x8893
#define GL_STATIC_DRAW 0x88E4
#define GL_STREAM_DRAW 0x88E0
#endif
#ifndef GL_CLAMP_TO_EDGE
#define GL_CLAMP_TO_EDGE 0x812F
#endif
#ifndef GL_LEQUAL
#define GL_LEQUAL 0x0203
#endif
#ifndef GL_TEXTURE0
#define GL_TEXTURE0 0x84C0
#endif
#ifndef GL_FRAMEBUFFER
#define GL_FRAMEBUFFER 0x8D40
#define GL_RENDERBUFFER 0x8D41
#define GL_DEPTH_COMPONENT24 0x81A6
#define GL_FRAMEBUFFER_COMPLETE 0x8CD5
#define GL_COLOR_ATTACHMENT0 0x8CE0
#define GL_DEPTH_ATTACHMENT 0x8D00
#endif

namespace R {

// ---- GL 2.0+ function pointer types -----------------------------------
typedef unsigned int (*PFN_CreateShader)(unsigned int);
typedef void (*PFN_ShaderSource)(unsigned int, int, const char**, const int*);
typedef void (*PFN_CompileShader)(unsigned int);
typedef void (*PFN_GetShaderiv)(unsigned int, unsigned int, int*);
typedef void (*PFN_GetShaderInfoLog)(unsigned int, int, int*, char*);
typedef unsigned int (*PFN_CreateProgram)();
typedef void (*PFN_AttachShader)(unsigned int, unsigned int);
typedef void (*PFN_LinkProgram)(unsigned int);
typedef void (*PFN_GetProgramiv)(unsigned int, unsigned int, int*);
typedef void (*PFN_GetProgramInfoLog)(unsigned int, int, int*, char*);
typedef void (*PFN_UseProgram)(unsigned int);
typedef int (*PFN_GetUniformLocation)(unsigned int, const char*);
typedef void (*PFN_Uniform1i)(int, int);
typedef void (*PFN_Uniform1f)(int, float);
typedef void (*PFN_Uniform2f)(int, float, float);
typedef void (*PFN_Uniform3f)(int, float, float, float);
typedef void (*PFN_Uniform4f)(int, float, float, float, float);
typedef void (*PFN_UniformMatrix4fv)(int, int, unsigned char, const float*);
typedef void (*PFN_GenVertexArrays)(int, unsigned int*);
typedef void (*PFN_BindVertexArray)(unsigned int);
typedef void (*PFN_DeleteVertexArrays)(int, const unsigned int*);
typedef void (*PFN_GenBuffers)(int, unsigned int*);
typedef void (*PFN_BindBuffer)(unsigned int, unsigned int);
typedef void (*PFN_BufferData)(unsigned int, ptrdiff_t, const void*, unsigned int);
typedef void (*PFN_DeleteBuffers)(int, const unsigned int*);
typedef void (*PFN_EnableVertexAttribArray)(unsigned int);
typedef void (*PFN_DisableVertexAttribArray)(unsigned int);
typedef void (*PFN_VertexAttribPointer)(unsigned int, int, unsigned int, unsigned char, int, const void*);
typedef void (*PFN_ActiveTexture)(unsigned int);
typedef void (*PFN_GenerateMipmap)(unsigned int);
typedef void (*PFN_GenFramebuffers)(int, unsigned int*);
typedef void (*PFN_BindFramebuffer)(unsigned int, unsigned int);
typedef void (*PFN_FramebufferTexture2D)(unsigned int, unsigned int, unsigned int, unsigned int, int);
typedef unsigned int (*PFN_CheckFramebufferStatus)(unsigned int);
typedef void (*PFN_GenRenderbuffers)(int, unsigned int*);
typedef void (*PFN_BindRenderbuffer)(unsigned int, unsigned int);
typedef void (*PFN_RenderbufferStorage)(unsigned int, unsigned int, int, int);
typedef void (*PFN_FramebufferRenderbuffer)(unsigned int, unsigned int, unsigned int, unsigned int);
typedef void (*PFN_DeleteFramebuffers)(int, const unsigned int*);
typedef void (*PFN_DeleteRenderbuffers)(int, const unsigned int*);

#define GLPTR(name) PFN_##name pfn_##name
GLPTR(CreateShader); GLPTR(ShaderSource); GLPTR(CompileShader); GLPTR(GetShaderiv);
GLPTR(GetShaderInfoLog); GLPTR(CreateProgram); GLPTR(AttachShader); GLPTR(LinkProgram);
GLPTR(GetProgramiv); GLPTR(GetProgramInfoLog); GLPTR(UseProgram); GLPTR(GetUniformLocation);
GLPTR(Uniform1i); GLPTR(Uniform1f); GLPTR(Uniform2f); GLPTR(Uniform3f); GLPTR(Uniform4f);
GLPTR(UniformMatrix4fv); GLPTR(GenVertexArrays); GLPTR(BindVertexArray); GLPTR(DeleteVertexArrays);
GLPTR(GenBuffers); GLPTR(BindBuffer); GLPTR(BufferData); GLPTR(DeleteBuffers);
GLPTR(EnableVertexAttribArray); GLPTR(DisableVertexAttribArray); GLPTR(VertexAttribPointer);
GLPTR(ActiveTexture); GLPTR(GenerateMipmap);
GLPTR(GenFramebuffers); GLPTR(BindFramebuffer); GLPTR(FramebufferTexture2D);
GLPTR(CheckFramebufferStatus); GLPTR(GenRenderbuffers); GLPTR(BindRenderbuffer);
GLPTR(RenderbufferStorage); GLPTR(FramebufferRenderbuffer); GLPTR(DeleteFramebuffers);
GLPTR(DeleteRenderbuffers);

#define LOAD(VAR) pfn_##VAR = (PFN_##VAR)glfwGetProcAddress("gl" #VAR); \
    if (!pfn_##VAR) { fprintf(stderr, "Missing GL function: gl%s\n", #VAR); return false; }

#define glCreateShader pfn_CreateShader
#define glShaderSource pfn_ShaderSource
#define glCompileShader pfn_CompileShader
#define glGetShaderiv pfn_GetShaderiv
#define glGetShaderInfoLog pfn_GetShaderInfoLog
#define glCreateProgram pfn_CreateProgram
#define glAttachShader pfn_AttachShader
#define glLinkProgram pfn_LinkProgram
#define glGetProgramiv pfn_GetProgramiv
#define glGetProgramInfoLog pfn_GetProgramInfoLog
#define glUseProgram pfn_UseProgram
#define glGetUniformLocation pfn_GetUniformLocation
#define glUniform1i pfn_Uniform1i
#define glUniform1f pfn_Uniform1f
#define glUniform2f pfn_Uniform2f
#define glUniform3f pfn_Uniform3f
#define glUniform4f pfn_Uniform4f
#define glUniformMatrix4fv pfn_UniformMatrix4fv
#define glGenVertexArrays pfn_GenVertexArrays
#define glBindVertexArray pfn_BindVertexArray
#define glDeleteVertexArrays pfn_DeleteVertexArrays
#define glGenBuffers pfn_GenBuffers
#define glBindBuffer pfn_BindBuffer
#define glBufferData pfn_BufferData
#define glDeleteBuffers pfn_DeleteBuffers
#define glEnableVertexAttribArray pfn_EnableVertexAttribArray
#define glDisableVertexAttribArray pfn_DisableVertexAttribArray
#define glVertexAttribPointer pfn_VertexAttribPointer
#define glActiveTexture pfn_ActiveTexture
#define glGenerateMipmap pfn_GenerateMipmap
#define glGenFramebuffers pfn_GenFramebuffers
#define glBindFramebuffer pfn_BindFramebuffer
#define glFramebufferTexture2D pfn_FramebufferTexture2D
#define glCheckFramebufferStatus pfn_CheckFramebufferStatus
#define glGenRenderbuffers pfn_GenRenderbuffers
#define glBindRenderbuffer pfn_BindRenderbuffer
#define glRenderbufferStorage pfn_RenderbufferStorage
#define glFramebufferRenderbuffer pfn_FramebufferRenderbuffer
#define glDeleteFramebuffers pfn_DeleteFramebuffers
#define glDeleteRenderbuffers pfn_DeleteRenderbuffers
// GL 1.1 functions (glEnable, glClear, glDrawElements, ...) are called directly.
} // namespace R (loader section)

namespace R {

int vpW = 1280, vpH = 720;
void buildFont(); // defined below (font atlas)

// ------------------------------------------------------------------ state
static GLuint prog3D = 0, prog2D = 0;

struct U3D {
    GLint proj, view, model, camPos, sunDir, tint, emissive, alpha, fogColor, fogRange;
} u3;
struct U2D {
    GLint ortho, tex, useTex;
} u2;

static Mesh dynMesh;
static std::vector<Vertex> dynVerts;
static std::vector<uint32_t> dynIdx;

struct V2D { Vec2 p; Vec4 c; Vec2 uv; };
static std::vector<V2D> uiVerts;
static GLuint vao2D = 0, vbo2D = 0;
static GLuint fontTex = 0;
static int uiMode = -1; // -1 none, 0 untextured, 1 textured (font)

// ---- post-processing (bodycam) ----
static GLuint fbo = 0, fboTex = 0, depthRb = 0, progPost = 0;
static GLuint vaoPost = 0, vboPost = 0;
static GLint uPostTex = -1, uPostTime = -1, uPostRes = -1;
static int postW = 0, postH = 0;

// ------------------------------------------------------------------ shaders
static const char* VS_3D = R"(
#version 330 core
layout(location=0) in vec3 aPos;
layout(location=1) in vec3 aNorm;
layout(location=2) in vec4 aCol;
uniform mat4 uProj, uView, uModel;
out vec3 vCol; out vec3 vNorm; out vec3 vPos;
void main(){
    vec4 wp = uModel * vec4(aPos, 1.0);
    vPos = wp.xyz;
    vNorm = normalize(mat3(uModel) * aNorm);
    vCol = aCol.rgb;
    gl_Position = uProj * uView * wp;
}
)";

static const char* FS_3D = R"(
#version 330 core
in vec3 vCol; in vec3 vNorm; in vec3 vPos;
uniform vec3 uCamPos, uSunDir, uTint, uEmissive, uFogColor;
uniform vec2 uFogRange;
uniform float uAlpha;
out vec4 frag;
void main(){
    vec3 n = normalize(vNorm);
    vec3 base = vCol * uTint;
    float ndl = max(dot(n, uSunDir), 0.0);
    vec3 amb = vec3(0.60, 0.61, 0.62);          // flat overcast sky light
    vec3 sun = vec3(0.85, 0.88, 0.92) * 0.42;   // weak cool sun
    vec3 lit = base * amb + base * sun * ndl;
    vec3 vdir = normalize(uCamPos - vPos);
    float rim = pow(1.0 - max(dot(n, vdir), 0.0), 3.0) * 0.15;
    lit += base * rim + uEmissive;
    float d = length(uCamPos - vPos);
    float fog = smoothstep(uFogRange.x, uFogRange.y, d);
    frag = vec4(mix(lit, uFogColor, fog), uAlpha);
}
)";

static const char* VS_2D = R"(
#version 330 core
layout(location=0) in vec2 aPos;
layout(location=1) in vec4 aCol;
layout(location=2) in vec2 aUV;
uniform mat4 uOrtho;
out vec4 vCol; out vec2 vUV;
void main(){
    vCol = aCol; vUV = aUV;
    gl_Position = uOrtho * vec4(aPos, 0.0, 1.0);
}
)";

static const char* FS_2D = R"(
#version 330 core
in vec4 vCol; in vec2 vUV;
uniform sampler2D uTex;
uniform float uUseTex;
out vec4 frag;
void main(){
    float a = 1.0;
    if (uUseTex > 0.5) a = texture(uTex, vUV).a;
    frag = vec4(vCol.rgb, vCol.a * a);
}
)";

// Bodycam post-process: fisheye barrel distortion + chromatic aberration,
// vignette, film grain, subtle scanlines, slight desaturation.
static const char* VS_POST = R"(
#version 330 core
layout(location=0) in vec2 aPos;
out vec2 vUV;
void main(){
    vUV = aPos * 0.5 + 0.5;
    gl_Position = vec4(aPos, 0.0, 1.0);
}
)";

static const char* FS_POST = R"(
#version 330 core
in vec2 vUV;
uniform sampler2D uTex;
uniform float uTime;
uniform vec2 uRes;
out vec4 frag;

vec3 scene(vec2 uv, float ca){
    vec2 c = uv - 0.5;
    float r2 = dot(c, c);
    // pincushion sample map => displayed fisheye (no out-of-range sampling)
    vec2 d = c * (1.0 - 0.085 * r2 - 0.06 * r2 * r2 + ca);
    return texture(uTex, d + 0.5).rgb;
}

float hash(vec2 p){
    return fract(sin(dot(p, vec2(127.1, 311.7))) * 43758.5453);
}

void main(){
    vec3 col;
    col.r = scene(vUV,  0.0035).r;
    col.g = scene(vUV,  0.0000).g;
    col.b = scene(vUV, -0.0035).b;

    vec2 c = vUV - 0.5;
    float r2 = dot(c, c);
    // lens vignette
    col *= 1.0 - 0.55 * smoothstep(0.12, 0.55, r2);

    // bodycam grade: slight desaturation + lifted blacks + contrast
    float g = dot(col, vec3(0.299, 0.587, 0.114));
    col = mix(vec3(g), col, 0.82);
    col = clamp((col - 0.5) * 1.07 + 0.5 + 0.012, 0.0, 1.0);

    // animated film grain
    float n = hash(vUV * uRes * 0.75 + vec2(uTime * 61.7, uTime * 127.3)) - 0.5;
    col += n * 0.05;

    // faint scanline
    col *= 0.97 + 0.03 * sin(vUV.y * uRes.y * 1.35);

    // soft dark border of the lens
    float edge = max(abs(c.x), abs(c.y)) * 2.0;
    col *= smoothstep(1.0, 0.90, edge);

    frag = vec4(col, 1.0);
}
)";

static GLuint compile(GLenum type, const char* src) {
    GLuint s = glCreateShader(type);
    glShaderSource(s, 1, &src, nullptr);
    glCompileShader(s);
    GLint ok = 0;
    glGetShaderiv(s, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        char log[2048];
        glGetShaderInfoLog(s, 2048, nullptr, log);
        fprintf(stderr, "Shader compile error:\n%s\n%s\n", log, src);
    }
    return s;
}
static GLuint link(GLuint vs, GLuint fs) {
    GLuint p = glCreateProgram();
    glAttachShader(p, vs);
    glAttachShader(p, fs);
    glLinkProgram(p);
    GLint ok = 0;
    glGetProgramiv(p, GL_LINK_STATUS, &ok);
    if (!ok) {
        char log[2048];
        glGetProgramInfoLog(p, 2048, nullptr, log);
        fprintf(stderr, "Program link error:\n%s\n", log);
    }
    return p;
}

// ------------------------------------------------------------------ init
bool init() {
    LOAD(CreateShader) LOAD(ShaderSource) LOAD(CompileShader) LOAD(GetShaderiv)
    LOAD(GetShaderInfoLog) LOAD(CreateProgram) LOAD(AttachShader) LOAD(LinkProgram)
    LOAD(GetProgramiv) LOAD(GetProgramInfoLog) LOAD(UseProgram) LOAD(GetUniformLocation)
    LOAD(Uniform1i) LOAD(Uniform1f) LOAD(Uniform2f) LOAD(Uniform3f) LOAD(Uniform4f)
    LOAD(UniformMatrix4fv) LOAD(GenVertexArrays) LOAD(BindVertexArray) LOAD(DeleteVertexArrays)
    LOAD(GenBuffers) LOAD(BindBuffer) LOAD(BufferData) LOAD(DeleteBuffers)
    LOAD(EnableVertexAttribArray) LOAD(DisableVertexAttribArray) LOAD(VertexAttribPointer)
    LOAD(ActiveTexture) LOAD(GenerateMipmap)
    LOAD(GenFramebuffers) LOAD(BindFramebuffer) LOAD(FramebufferTexture2D)
    LOAD(CheckFramebufferStatus) LOAD(GenRenderbuffers) LOAD(BindRenderbuffer)
    LOAD(RenderbufferStorage) LOAD(FramebufferRenderbuffer) LOAD(DeleteFramebuffers)
    LOAD(DeleteRenderbuffers)

    prog3D = link(compile(GL_VERTEX_SHADER, VS_3D), compile(GL_FRAGMENT_SHADER, FS_3D));
    prog2D = link(compile(GL_VERTEX_SHADER, VS_2D), compile(GL_FRAGMENT_SHADER, FS_2D));
    progPost = link(compile(GL_VERTEX_SHADER, VS_POST), compile(GL_FRAGMENT_SHADER, FS_POST));
    u3 = {glGetUniformLocation(prog3D, "uProj"), glGetUniformLocation(prog3D, "uView"),
          glGetUniformLocation(prog3D, "uModel"), glGetUniformLocation(prog3D, "uCamPos"),
          glGetUniformLocation(prog3D, "uSunDir"), glGetUniformLocation(prog3D, "uTint"),
          glGetUniformLocation(prog3D, "uEmissive"), glGetUniformLocation(prog3D, "uAlpha"),
          glGetUniformLocation(prog3D, "uFogColor"), glGetUniformLocation(prog3D, "uFogRange")};
    u2 = {glGetUniformLocation(prog2D, "uOrtho"), glGetUniformLocation(prog2D, "uTex"),
          glGetUniformLocation(prog2D, "uUseTex")};

    glGenVertexArrays(1, &dynMesh.vao);
    glGenBuffers(1, &dynMesh.vbo);
    glGenBuffers(1, &dynMesh.ibo);
    glGenVertexArrays(1, &vao2D);
    glGenBuffers(1, &vbo2D);

    // fullscreen triangle for the post pass
    glGenVertexArrays(1, &vaoPost);
    glGenBuffers(1, &vboPost);
    glBindVertexArray(vaoPost);
    glBindBuffer(GL_ARRAY_BUFFER, vboPost);
    float tri[6] = {-1, -1, 3, -1, -1, 3};
    glBufferData(GL_ARRAY_BUFFER, sizeof(tri), tri, GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, nullptr);
    glBindVertexArray(0);
    uPostTex = glGetUniformLocation(progPost, "uTex");
    uPostTime = glGetUniformLocation(progPost, "uTime");
    uPostRes = glGetUniformLocation(progPost, "uRes");

    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LEQUAL);
    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);
    glFrontFace(GL_CCW);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    buildFont();
    return true;
}

void resize(int w, int h) {
    vpW = w;
    vpH = h > 0 ? h : 1;
    glViewport(0, 0, vpW, vpH);
}

// ------------------------------------------------------------------ meshes
Mesh buildMesh(const MeshBuilder& mb) {
    Mesh m;
    glGenVertexArrays(1, &m.vao);
    glGenBuffers(1, &m.vbo);
    glGenBuffers(1, &m.ibo);
    glBindVertexArray(m.vao);
    glBindBuffer(GL_ARRAY_BUFFER, m.vbo);
    glBufferData(GL_ARRAY_BUFFER, (long)mb.verts.size() * sizeof(Vertex), mb.verts.data(), GL_STATIC_DRAW);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m.ibo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, (long)mb.idx.size() * sizeof(uint32_t), mb.idx.data(), GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, p));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, n));
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 4, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, c));
    glBindVertexArray(0);
    m.count = (int)mb.idx.size();
    m.ok = m.count > 0;
    return m;
}

void freeMesh(Mesh& m) {
    if (m.vao) { glDeleteVertexArrays(1, &m.vao); m.vao = 0; }
    if (m.vbo) { glDeleteBuffers(1, &m.vbo); m.vbo = 0; }
    if (m.ibo) { glDeleteBuffers(1, &m.ibo); m.ibo = 0; }
    m.ok = false;
}

Mesh makeBoxMesh(Vec3 half, Vec3 col) {
    MeshBuilder mb;
    mb.box({0, 0, 0}, half, col);
    return buildMesh(mb);
}

// ------------------------------------------------------------------ post (bodycam)
static void ensurePost(int w, int h) {
    if (postW == w && postH == h && fbo) return;
    postW = w;
    postH = h;
    if (fboTex) { glDeleteTextures(1, &fboTex); fboTex = 0; }
    if (depthRb) { glDeleteRenderbuffers(1, &depthRb); depthRb = 0; }
    if (fbo) glDeleteFramebuffers(1, &fbo);

    glGenTextures(1, &fboTex);
    glBindTexture(GL_TEXTURE_2D, fboTex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    glGenRenderbuffers(1, &depthRb);
    glBindRenderbuffer(GL_RENDERBUFFER, depthRb);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, w, h);

    glGenFramebuffers(1, &fbo);
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, fboTex, 0);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, depthRb);
    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
        fprintf(stderr, "[render] post FBO incomplete\n");
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void beginScene() {
    ensurePost(vpW, vpH);
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);
}

void endScene(float time) {
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);
    glDisable(GL_BLEND);
    glUseProgram(progPost);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, fboTex);
    glUniform1i(uPostTex, 0);
    glUniform1f(uPostTime, time);
    glUniform2f(uPostRes, (float)postW, (float)postH);
    glBindVertexArray(vaoPost);
    glDrawArrays(GL_TRIANGLES, 0, 3);
    glBindVertexArray(0);
    glBindTexture(GL_TEXTURE_2D, 0);
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_CULL_FACE);
}

// ------------------------------------------------------------------ 3D pass
void begin3D(const Mat4& proj, const Mat4& view, Vec3 camPos, Vec3 fogColor, Vec2 fogRange) {
    glUseProgram(prog3D);
    glUniformMatrix4fv(u3.proj, 1, GL_FALSE, proj.m);
    glUniformMatrix4fv(u3.view, 1, GL_FALSE, view.m);
    glUniform3f(u3.camPos, camPos.x, camPos.y, camPos.z);
    Vec3 sun = norm(Vec3(0.45f, 0.85f, 0.30f));
    glUniform3f(u3.sunDir, sun.x, sun.y, sun.z);
    glUniform3f(u3.tint, 1, 1, 1);
    glUniform3f(u3.emissive, 0, 0, 0);
    glUniform1f(u3.alpha, 1);
    glUniform3f(u3.fogColor, fogColor.x, fogColor.y, fogColor.z);
    glUniform2f(u3.fogRange, fogRange.x, fogRange.y);
}

void drawMesh(const Mesh& m, const Mat4& model, Vec3 tint, Vec3 emissive, float alpha) {
    if (!m.ok) return;
    glBindVertexArray(m.vao);
    glUniformMatrix4fv(u3.model, 1, GL_FALSE, model.m);
    glUniform3f(u3.tint, tint.x, tint.y, tint.z);
    glUniform3f(u3.emissive, emissive.x, emissive.y, emissive.z);
    glUniform1f(u3.alpha, alpha);
    glDrawElements(GL_TRIANGLES, m.count, GL_UNSIGNED_INT, nullptr);
}

// ------------------------------------------------------------------ dynamic 3D batch
void dynClear() { dynVerts.clear(); dynIdx.clear(); }

void dynQuad(Vec3 a, Vec3 b, Vec3 c, Vec3 d, Vec4 color) {
    uint32_t base = (uint32_t)dynVerts.size();
    Vec3 n = norm(cross(b - a, d - a));
    Vec3 pts[4] = {a, b, c, d};
    for (int i = 0; i < 4; i++) dynVerts.push_back({pts[i], n, color});
    dynIdx.push_back(base); dynIdx.push_back(base + 1); dynIdx.push_back(base + 2);
    dynIdx.push_back(base); dynIdx.push_back(base + 2); dynIdx.push_back(base + 3);
}

void dynFlush() {
    if (dynIdx.empty()) return;
    glDisable(GL_CULL_FACE);
    glDepthMask(GL_FALSE);
    glEnable(GL_BLEND);
    glBindVertexArray(dynMesh.vao);
    glBindBuffer(GL_ARRAY_BUFFER, dynMesh.vbo);
    glBufferData(GL_ARRAY_BUFFER, (long)dynVerts.size() * sizeof(Vertex), dynVerts.data(), GL_STREAM_DRAW);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, dynMesh.ibo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, (long)dynIdx.size() * sizeof(uint32_t), dynIdx.data(), GL_STREAM_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, p));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, n));
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 4, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, c));
    glUniformMatrix4fv(u3.model, 1, GL_FALSE, Mat4().m);
    glUniform3f(u3.tint, 1, 1, 1);
    glUniform3f(u3.emissive, 0, 0, 0);
    glUniform1f(u3.alpha, 1);
    glDrawElements(GL_TRIANGLES, (int)dynIdx.size(), GL_UNSIGNED_INT, nullptr);
    glBindVertexArray(0);
    glDepthMask(GL_TRUE);
    glDisable(GL_BLEND);
    glEnable(GL_CULL_FACE);
    dynVerts.clear();
    dynIdx.clear();
}

// ------------------------------------------------------------------ bitmap font
#include "font5x7.inc" // glyph table: FONT_5X7[96][7] for ASCII 32..127

void buildFont() {
    const int cw = 8, ch = 8, cols = 16, rows = 6; // 16x6 cells of 8x8 px = 128x48
    int tw = cols * cw, th = rows * ch;
    std::vector<uint8_t> pixels((size_t)tw * th * 4, 0);
    for (int g = 0; g < 96; g++) {
        int gx = (g % cols) * cw, gy = (g / cols) * ch;
        for (int r = 0; r < 7; r++) {
            uint8_t bits = FONT_5X7[g][r];
            for (int c = 0; c < 5; c++) {
                if (bits & (0x10 >> c)) {
                    int px = gx + 1 + c, py = gy + 1 + r;
                    uint8_t* p = &pixels[(py * tw + px) * 4];
                    p[0] = p[1] = p[2] = 255;
                    p[3] = 255;
                }
            }
        }
    }
    glGenTextures(1, &fontTex);
    glBindTexture(GL_TEXTURE_2D, fontTex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, tw, th, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
}

static int glyphIndex(char ch) {
    int c = (int)(unsigned char)ch;
    if (c >= 'a' && c <= 'z') c = c - 'a' + 'A';
    if (c < 32) return 0;      // control -> space
    if (c > 127) c = '?';
    return c - 32;
}

float textW(float size, const std::string& s) {
    return (float)s.size() * 6.0f * (size / 7.0f);
}

void begin2D(int w, int h) {
    glUseProgram(prog2D);
    uiMode = -1;
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);
    glEnable(GL_BLEND);
    Mat4 o = matOrtho(0, (float)w, (float)h, 0, -1, 1);
    glUniformMatrix4fv(u2.ortho, 1, GL_FALSE, o.m);
    glUniform1i(u2.tex, 0);
    glUniform1f(u2.useTex, 0);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, 0);
}

static void uiFlush() {
    if (uiVerts.empty()) return;
    glBindVertexArray(vao2D);
    glBindBuffer(GL_ARRAY_BUFFER, vbo2D);
    glBufferData(GL_ARRAY_BUFFER, (long)uiVerts.size() * sizeof(V2D), uiVerts.data(), GL_STREAM_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(V2D), (void*)offsetof(V2D, p));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, sizeof(V2D), (void*)offsetof(V2D, c));
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(V2D), (void*)offsetof(V2D, uv));
    glDrawArrays(GL_TRIANGLES, 0, (int)uiVerts.size());
    uiVerts.clear();
}

void quadUV(float x, float y, float w, float h, Vec4 color, Vec2 uv0, Vec2 uv1) {
    if (uiMode != 1) { uiFlush(); uiMode = 1; glUniform1f(u2.useTex, 1); glBindTexture(GL_TEXTURE_2D, fontTex); }
    float x1 = x + w, y1 = y + h;
    V2D v[6] = {
        {{x, y}, color, uv0},
        {{x1, y}, color, {uv1.x, uv0.y}},
        {{x1, y1}, color, uv1},
        {{x, y}, color, uv0},
        {{x1, y1}, color, uv1},
        {{x, y1}, color, {uv0.x, uv1.y}},
    };
    uiVerts.insert(uiVerts.end(), v, v + 6);
    uiFlush();
}

void quad2D(float x, float y, float w, float h, Vec4 color) {
    if (uiMode != 0) { uiFlush(); uiMode = 0; glUniform1f(u2.useTex, 0); }
    float x1 = x + w, y1 = y + h;
    V2D v[6] = {
        {{x, y}, color, {0, 0}},
        {{x1, y}, color, {0, 0}},
        {{x1, y1}, color, {0, 0}},
        {{x, y}, color, {0, 0}},
        {{x1, y1}, color, {0, 0}},
        {{x, y1}, color, {0, 0}},
    };
    uiVerts.insert(uiVerts.end(), v, v + 6);
    uiFlush();
}

void text(float x, float y, float size, Vec4 color, const std::string& s, int align) {
    float scale = size / 7.0f;
    float tw = textW(size, s);
    if (align == 1) x -= tw * 0.5f;
    if (align == 2) x -= tw;
    const int cw = 8, ch = 8, cols = 16;
    float texW = (float)(cols * cw), texH = (float)(6 * ch);
    for (size_t i = 0; i < s.size(); i++) {
        int g = glyphIndex(s[i]);
        float gx = (float)(g % cols) * cw + 1.0f;
        float gy = (float)(g / cols) * ch + 1.0f;
        float px = x + (float)i * 6.0f * scale;
        quadUV(px, y, 5.0f * scale, 7.0f * scale, color,
               {gx / texW, gy / texH}, {(gx + 5) / texW, (gy + 7) / texH});
    }
    uiFlush();
    uiMode = -1;
}

void end2D() {
    uiFlush();
    uiMode = -1;
    glBindTexture(GL_TEXTURE_2D, 0);
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_CULL_FACE);
    glDisable(GL_BLEND);
}

} // namespace R

// ------------------------------------------------------------------ MeshBuilder
void MeshBuilder::quad(Vec3 a, Vec3 b, Vec3 c, Vec3 d, Vec3 col, float alpha) {
    quadNormal(a, b, c, d, norm(cross(b - a, d - a)), col, alpha);
}

void MeshBuilder::quadNormal(Vec3 a, Vec3 b, Vec3 c, Vec3 d, Vec3 n, Vec3 col, float alpha) {
    uint32_t base = (uint32_t)verts.size();
    Vec3 pts[4] = {a, b, c, d};
    for (int i = 0; i < 4; i++) verts.push_back({pts[i], n, {col.x, col.y, col.z, alpha}});
    idx.push_back(base); idx.push_back(base + 1); idx.push_back(base + 2);
    idx.push_back(base); idx.push_back(base + 2); idx.push_back(base + 3);
}

void MeshBuilder::box(Vec3 center, Vec3 half, Vec3 col, float alpha) {
    boxFaces(center, half, col, col, col);
}

void MeshBuilder::boxFaces(Vec3 center, Vec3 h, Vec3 cTop, Vec3 cSide, Vec3 cBottom) {
    float x0 = center.x - h.x, x1 = center.x + h.x;
    float y0 = center.y - h.y, y1 = center.y + h.y;
    float z0 = center.z - h.z, z1 = center.z + h.z;
    // +Y top
    quadNormal({x0, y1, z0}, {x0, y1, z1}, {x1, y1, z1}, {x1, y1, z0}, {0, 1, 0}, cTop);
    // -Y bottom
    quadNormal({x0, y0, z0}, {x1, y0, z0}, {x1, y0, z1}, {x0, y0, z1}, {0, -1, 0}, cBottom);
    // +Z front
    quad({x0, y0, z1}, {x1, y0, z1}, {x1, y1, z1}, {x0, y1, z1}, cSide);
    // -Z back
    quad({x1, y0, z0}, {x0, y0, z0}, {x0, y1, z0}, {x1, y1, z0}, cSide);
    // +X right
    quad({x1, y0, z1}, {x1, y0, z0}, {x1, y1, z0}, {x1, y1, z1}, cSide);
    // -X left
    quad({x0, y0, z0}, {x0, y0, z1}, {x0, y1, z1}, {x0, y1, z0}, cSide);
}

void MeshBuilder::cylinder(Vec3 center, float radius, float height, int sides, Vec3 col, Vec3 capCol) {
    float y0 = center.y - height * 0.5f, y1 = center.y + height * 0.5f;
    for (int i = 0; i < sides; i++) {
        float a0 = TAU * (float)i / sides, a1 = TAU * (float)(i + 1) / sides;
        Vec3 p0{center.x + cosf(a0) * radius, y0, center.z + sinf(a0) * radius};
        Vec3 p1{center.x + cosf(a1) * radius, y0, center.z + sinf(a1) * radius};
        Vec3 p2{center.x + cosf(a1) * radius, y1, center.z + sinf(a1) * radius};
        Vec3 p3{center.x + cosf(a0) * radius, y1, center.z + sinf(a0) * radius};
        // (bottom a0 -> top a0 -> top a1 -> bottom a1) winds outward
        quad(p0, p3, p2, p1, col);
        Vec3 cT = center + Vec3(0, height * 0.5f, 0);
        quadNormal(cT, p2, p3, p3 * 0.0f + p2, {0, 1, 0}, capCol); // tri (cT, p2, p3) + degenerate
        Vec3 cB = center + Vec3(0, -height * 0.5f, 0);
        quadNormal(cB, p0, p1, p1 * 0.0f + p0, {0, -1, 0}, capCol); // tri (cB, p0, p1)
    }
}

void MeshBuilder::taperedCylinder(Vec3 center, float r0, float r1, float height, int sides, Vec3 col) {
    float y0 = center.y - height * 0.5f, y1 = center.y + height * 0.5f;
    for (int i = 0; i < sides; i++) {
        float a0 = TAU * (float)i / sides, a1 = TAU * (float)(i + 1) / sides;
        Vec3 b0{center.x + cosf(a0) * r0, y0, center.z + sinf(a0) * r0};
        Vec3 b1{center.x + cosf(a1) * r0, y0, center.z + sinf(a1) * r0};
        Vec3 t1{center.x + cosf(a1) * r1, y1, center.z + sinf(a1) * r1};
        Vec3 t0{center.x + cosf(a0) * r1, y1, center.z + sinf(a0) * r1};
        // outward-wound side quad (bottom a0 -> top a0 -> top a1 -> bottom a1)
        float slope = (r0 - r1) / height;
        Vec3 n{cosf((a0 + a1) * 0.5f), slope, sinf((a0 + a1) * 0.5f)};
        quadNormal(b0, t0, t1, b1, norm(n), col);
    }
    // caps as triangle fans (single angle step beyond the loop)
    float aE = TAU;
    Vec3 cT{center.x, y1, center.z}, cB{center.x, y0, center.z};
    for (int i = 0; i < sides; i++) {
        float a0 = TAU * (float)i / sides, a1 = (i + 1 == sides) ? aE : TAU * (float)(i + 1) / sides;
        Vec3 t0{center.x + cosf(a0) * r1, y1, center.z + sinf(a0) * r1};
        Vec3 t1{center.x + cosf(a1) * r1, y1, center.z + sinf(a1) * r1};
        quadNormal(cT, t1, t0, t0 * 1.0f, {0, 1, 0}, col); // tri (cT, t1, t0)
        Vec3 b0{center.x + cosf(a0) * r0, y0, center.z + sinf(a0) * r0};
        Vec3 b1{center.x + cosf(a1) * r0, y0, center.z + sinf(a1) * r0};
        quadNormal(cB, b0, b1, b1 * 1.0f, {0, -1, 0}, col); // tri (cB, b0, b1)
    }
}

void MeshBuilder::sphere(Vec3 center, float radius, int stacks, int slices, Vec3 col) {
    for (int i = 0; i < stacks; i++) {
        float t0 = PI * (float)i / stacks, t1 = PI * (float)(i + 1) / stacks;
        for (int j = 0; j < slices; j++) {
            float p0 = TAU * (float)j / slices, p1 = TAU * (float)(j + 1) / slices;
            auto pt = [&](float t, float p) {
                return center + Vec3(sinf(t) * cosf(p) * radius, cosf(t) * radius, sinf(t) * sinf(p) * radius);
            };
            Vec3 a = pt(t0, p0), b = pt(t0, p1), c = pt(t1, p1), d = pt(t1, p0);
            if (i == 0) quad(a, b, c, d, col);           // triangle via degenerate top ok visually
            else if (i == stacks - 1) quad(a, b, c, d, col);
            else quad(a, b, c, d, col);
        }
    }
}

void MeshBuilder::grid(Vec3 origin, int cells, float cell, Vec3 colA, Vec3 colB, Vec3 colEdge) {
    for (int i = 0; i < cells; i++)
        for (int j = 0; j < cells; j++) {
            float x0 = origin.x + i * cell, z0 = origin.z + j * cell;
            float x1 = x0 + cell, z1 = z0 + cell;
            bool alt = ((i + j) & 1) != 0;
            Vec3 c = alt ? colB : colA;
            // subtle border via slightly darker center? Keep flat: draw quad, then thin edge lines as tiny quads.
            quadNormal({x0, origin.y, z0}, {x1, origin.y, z0}, {x1, origin.y, z1}, {x0, origin.y, z1}, {0, 1, 0}, c);
            // edge strips (skip outer edges get wall covered anyway)
            float e = 0.045f;
            quadNormal({x0, origin.y + 0.002f, z0}, {x0, origin.y + 0.002f, z1}, {x0 + e, origin.y + 0.002f, z1}, {x0 + e, origin.y + 0.002f, z0}, {0, 1, 0}, colEdge);
            quadNormal({x0, origin.y + 0.002f, z0}, {x0 + e, origin.y + 0.002f, z0}, {x1, origin.y + 0.002f, z0}, {x1, origin.y + 0.002f, z0 + e}, {0, 1, 0}, colEdge);
        }
}
