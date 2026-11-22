// audio.h — OpenAL engine with procedurally synthesized SFX (no asset files)
#pragma once
#include "core.h"

enum Sfx {
    SFX_SHOOT = 0, SFX_DRY, SFX_RELOAD, SFX_HIT, SFX_HEADSHOT, SFX_EHURT, SFX_EDEATH,
    SFX_HURT, SFX_PICKUP, SFX_JUMP, SFX_LAND, SFX_STEP, SFX_WAVE, SFX_ZAP, SFX_SLAM,
    SFX_SPAWN, SFX_GAMEOVER, SFX_START, SFX_COUNT
};

namespace audio {
bool init();
void shutdown();
void updateListener(Vec3 pos, Vec3 forward);
// pos: nullptr = 2D (UI/global). gain & pitch in [0..]
void play(Sfx id, const Vec3* pos = nullptr, float gain = 1.0f, float pitch = 1.0f);
}
