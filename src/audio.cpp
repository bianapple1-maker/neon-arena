// audio.cpp — OpenAL + procedural DSP sound synthesis
#include "audio.h"

#ifdef HAVE_OPENAL
#include <AL/al.h>
#include <AL/alc.h>
#include <vector>
#include <functional>
#include <cmath>

namespace audio {

static ALCdevice* dev = nullptr;
static ALCcontext* ctx = nullptr;
static ALuint buffers[SFX_COUNT];
static ALuint sources[24];
static int nextSource = 0;
static bool ok = false;

static const int RATE = 22050;

// ------------------------------------------------------------- DSP helpers
struct Sample {
    std::vector<int16_t> data;
};

static float noiseGen() {
    return randf(-1.0f, 1.0f);
}

// One-pole lowpass state
struct LP {
    float y = 0;
    float operator()(float x, float cutoff01) {
        float a = clampf(cutoff01, 0.001f, 0.999f);
        y += a * (x - y);
        return y;
    }
};

static Sample build(int frames, const std::function<float(float t, float dur)>& gen) {
    Sample s;
    s.data.resize(frames);
    for (int i = 0; i < frames; i++) {
        float t = (float)i / RATE;
        float v = gen(t, (float)frames / RATE);
        s.data[i] = (int16_t)clampf(v * 32767.0f, -32767.0f, 32767.0f);
    }
    return s;
}

static ALuint upload(const Sample& s) {
    ALuint buf;
    alGenBuffers(1, &buf);
    alBufferData(buf, AL_FORMAT_MONO16, s.data.data(), (ALsizei)(s.data.size() * 2), RATE);
    return buf;
}

// ------------------------------------------------------------- sound design
static Sample makeShoot() { // punchy rifle: noise burst + low thump
    return build((int)(RATE * 0.16), [lp = LP()](float t, float dur) mutable {
        
        float env = expf(-t * 34.0f);
        float n = lp(noiseGen() * env, 0.35f - 0.25f * (t / dur)) * 1.5f;
        float thump = sinf(TAU * (110.0f - 60.0f * t) * t) * expf(-t * 60.0f) * 0.9f;
        float crack = (t < 0.006f) ? noiseGen() * 0.8f : 0;
        return clampf(n * 0.7f + thump + crack, -1.5f, 1.5f) * 0.8f;
    });
}

static Sample makeDry() {
    return build((int)(RATE * 0.05f), [lp = LP()](float t, float) mutable {
        return (sinf(TAU * 900 * t) * expf(-t * 120.0f)) * 0.5f;
    });
}

static Sample makeReload() { // two mechanical clicks
    return build((int)(RATE * 0.3f), [lp = LP()](float t, float) mutable {
        float c1 = (t < 0.03f) ? sinf(TAU * 1400 * t) * expf(-t * 160) : 0;
        float c2 = (t > 0.14f && t < 0.17f) ? sinf(TAU * 900 * t) * expf(-(t - 0.14f) * 140) : 0;
        float clack = (t > 0.05f && t < 0.06f) ? noiseGen() * expf(-(t - 0.05f) * 300) * 0.6f : 0;
        return (c1 + c2 + clack) * 0.7f;
    });
}

static Sample makeHit() {
    return build((int)(RATE * 0.07f), [lp = LP()](float t, float) mutable {
        return sinf(TAU * 1150 * t) * expf(-t * 70.0f) * 0.7f;
    });
}

static Sample makeHeadshot() {
    return build((int)(RATE * 0.12f), [lp = LP()](float t, float) mutable {
        float f = 1600.0f + 900.0f * t * 8;
        return sinf(TAU * f * t) * expf(-t * 40.0f) * 0.7f;
    });
}

static Sample makeEHurt() {
    return build((int)(RATE * 0.09f), [lp = LP()](float t, float) mutable {
        return lp(noiseGen(), 0.25f) * expf(-t * 45.0f) * 0.9f + sinf(TAU * 220 * t) * expf(-t * 40) * 0.3f;
    });
}

static Sample makeEDeath() {
    return build((int)(RATE * 0.4f), [lp = LP()](float t, float dur) mutable {
        float f = 320.0f - 260.0f * (t / dur);
        float saw = 2.0f * fmodf(f * t, 1.0f) - 1.0f;
        return (saw * 0.5f + lp(noiseGen(), 0.2f) * 0.4f) * expf(-t * 6.0f) * 0.8f;
    });
}

static Sample makeHurt() {
    return build((int)(RATE * 0.28f), [lp = LP()](float t, float) mutable {
        float thud = sinf(TAU * 90 * t) * expf(-t * 18.0f) * 1.1f;
        float n = lp(noiseGen(), 0.12f) * expf(-t * 25.0f) * 0.6f;
        return thud * 0.8f + n;
    });
}

static Sample makePickup() {
    return build((int)(RATE * 0.28f), [lp = LP()](float t, float) mutable {
        float f = 500.0f + (int)(t * 16) * 220.0f;
        return sinf(TAU * f * t) * expf(-t * 9.0f) * 0.5f;
    });
}

static Sample makeJump() {
    return build((int)(RATE * 0.1f), [lp = LP()](float t, float) mutable {
        return lp(noiseGen(), 0.3f) * expf(-t * 40.0f) * 0.5f;
    });
}

static Sample makeLand() {
    return build((int)(RATE * 0.12f), [lp = LP()](float t, float) mutable {
        return sinf(TAU * 70 * t) * expf(-t * 30.0f) * 0.8f;
    });
}

static Sample makeStep() {
    return build((int)(RATE * 0.07f), [lp = LP()](float t, float) mutable {
        return lp(noiseGen(), 0.18f) * expf(-t * 55.0f) * 0.45f;
    });
}

static Sample makeWave() { // two-tone horn
    return build((int)(RATE * 0.7f), [lp = LP()](float t, float) mutable {
        float f = t < 0.3f ? 220.0f : 330.0f;
        float saw = 2.0f * fmodf(f * t, 1.0f) - 1.0f;
        return lp(saw, 0.25f) * (t < 0.3f ? 0.5f : expf(-(t - 0.3f) * 6.0f)) * 0.6f;
    });
}

static Sample makeZap() {
    return build((int)(RATE * 0.22f), [lp = LP()](float t, float dur) mutable {
        float f = 1500.0f - 1200.0f * (t / dur);
        float sq = sinf(TAU * f * t) > 0 ? 1.0f : -1.0f;
        return lp(sq, 0.35f) * expf(-t * 12.0f) * 0.5f;
    });
}

static Sample makeSlam() {
    return build((int)(RATE * 0.45f), [lp = LP()](float t, float) mutable {
        float thump = sinf(TAU * 55 * t) * expf(-t * 12.0f) * 1.2f;
        return thump + lp(noiseGen(), 0.1f) * expf(-t * 10.0f) * 0.8f;
    });
}

static Sample makeSpawn() { // teleporter shimmer
    return build((int)(RATE * 0.5f), [lp = LP()](float t, float dur) mutable {
        float f = 300.0f + 1200.0f * (t / dur);
        return sinf(TAU * f * t) * (0.25f + 0.25f * sinf(TAU * 22 * t)) * expf(-t * 3.0f) * 0.6f;
    });
}

static Sample makeGameOver() {
    return build((int)(RATE * 1.2f), [lp = LP()](float t, float dur) mutable {
        float f = 300.0f - 220.0f * (t / dur);
        float saw = 2.0f * fmodf(f * t, 1.0f) - 1.0f;
        return lp(saw, 0.2f) * expf(-t * 2.0f) * 0.6f;
    });
}

static Sample makeStart() { // rising chord
    return build((int)(RATE * 0.8f), [lp = LP()](float t, float) mutable {
        float e = expf(-t * 3.0f);
        float s = sinf(TAU * 261 * t) * 0.35f + sinf(TAU * 392 * t) * 0.3f +
                  ((t > 0.15f) ? sinf(TAU * 523 * t) * 0.3f : 0);
        return s * e;
    });
}

// ------------------------------------------------------------- init
bool init() {
    dev = alcOpenDevice(nullptr);
    if (!dev) { fprintf(stderr, "[audio] no OpenAL device, running silent\n"); return false; }
    ctx = alcCreateContext(dev, nullptr);
    if (!ctx || !alcMakeContextCurrent(ctx)) { fprintf(stderr, "[audio] no OpenAL context\n"); return false; }
    alDistanceModel(AL_LINEAR_DISTANCE_CLAMPED);

    Sample (*makers[SFX_COUNT])() = {
        makeShoot, makeDry, makeReload, makeHit, makeHeadshot, makeEHurt, makeEDeath,
        makeHurt, makePickup, makeJump, makeLand, makeStep, makeWave, makeZap, makeSlam,
        makeSpawn, makeGameOver, makeStart
    };
    for (int i = 0; i < SFX_COUNT; i++) buffers[i] = upload(makers[i]());

    alGenSources(24, sources);
    for (ALuint s : sources) {
        alSourcef(s, AL_REFERENCE_DISTANCE, 4.0f);
        alSourcef(s, AL_MAX_DISTANCE, 70.0f);
    }
    ok = true;
    return true;
}

void shutdown() {
    if (!ok) return;
    alDeleteSources(24, sources);
    alDeleteBuffers(SFX_COUNT, buffers);
    alcMakeContextCurrent(nullptr);
    if (ctx) alcDestroyContext(ctx);
    if (dev) alcCloseDevice(dev);
    ok = false;
}

void updateListener(Vec3 pos, Vec3 forward) {
    if (!ok) return;
    Vec3 v = norm(forward);
    ALfloat ori[6] = {v.x, v.y, v.z, 0, 1, 0};
    alListenerfv(AL_POSITION, &pos.x);
    alListenerfv(AL_ORIENTATION, ori);
}

void play(Sfx id, const Vec3* pos, float gain, float pitch) {
    if (!ok) return;
    ALuint src = sources[nextSource];
    nextSource = (nextSource + 1) % 24;
    alSourceStop(src);
    alSourcei(src, AL_BUFFER, (ALint)buffers[id]);
    alSourcef(src, AL_GAIN, clampf(gain, 0, 2));
    alSourcef(src, AL_PITCH, clampf(pitch, 0.25f, 3.0f));
    if (pos) {
        alSourcei(src, AL_SOURCE_RELATIVE, AL_FALSE);
        alSource3f(src, AL_POSITION, pos->x, pos->y, pos->z);
    } else {
        alSourcei(src, AL_SOURCE_RELATIVE, AL_TRUE);
        alSource3f(src, AL_POSITION, 0, 0, 0);
    }
    alSourcePlay(src);
}

} // namespace audio

#else // !HAVE_OPENAL — silent stubs

namespace audio {
bool init() { return false; }
void shutdown() {}
void updateListener(Vec3, Vec3) {}
void play(Sfx, const Vec3*, float, float) {}
}
#endif
