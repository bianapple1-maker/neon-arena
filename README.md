# NEON ARENA — a bodycam-style wave-survival FPS in C++

Pure C++20 + OpenGL 3.3 core + GLFW. No assets, no engine: every mesh, texture,
font and sound is generated procedurally at startup. The footage is rendered
through a bodycam post-process (fisheye lens, film grain, chromatic aberration,
vignette, scanlines) with chest-cam sway, strafe roll and breathing. Audio uses
OpenAL when available (builds and runs silently otherwise).

## Build

```sh
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
```

## Run

```sh
./build/neon_arena
```

## Controls

| Key | Action |
|---|---|
| WASD | Move |
| Mouse | Look |
| Left click | Fire (full-auto) |
| Right click (hold) | Aim down sights |
| R | Reload |
| Shift | Sprint |
| Space | Jump |
| Esc | Pause / resume |
| Enter | Restart (from pause or game-over) |
| Mouse wheel (menus) | Mouse sensitivity |

## The game

- Endless waves in a gritty overcast depot: floodlight poles, dumpsters, pallet
  stacks, sandbag nests, generators, wall pipes, painted floor markings, oil
  stains and scattered debris.
- Enemies are armed operators with visible rifles: **Grunts** advance firing
  pot-shots, **Shooters** strafe and fire telegraphed bursts, **Heavies** (every
  4th wave) ground-slam. All of them have **limited magazines and perform
  tactical reloads** (a free window to push them), flinch when hit, and aim
  their rifle pitch at your eye.
- Headshots deal double damage (aim for the visor).
- **Animated reload**: mag drops out with your support hand, a fresh one slaps
  in, bolt releases - two-stage sounds included. Hold **RMB** for bodycam-style
  aim: camera zooms, sights align, sway tightens.
- Combo multiplier grows with fast consecutive kills; wave-clear and
  headshot bonuses; floating score popups, kill feed, radar, hit markers.
- Health & ammo pickups respawn at fixed pads; health slowly regenerates after
  4 seconds without damage.

## Code map

```
src/core.h       math (vec/mat), RNG, shared types
src/glrender.*   hand-rolled GL 3.3 loader, shaders, meshes, batching, bitmap font
src/world.*      arena geometry + AABB collision & raycasts
src/game.*       player, weapons, enemy AI, waves, scoring, particles
src/ui.*         HUD, menus, radar, popups (5x7 bitmap font)
src/audio.*      OpenAL + procedural DSP sound synthesis
src/font5x7.inc  public-domain-style 5x7 glyph table
```
