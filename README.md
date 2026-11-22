# NEON ARENA — a bodycam-style wave-survival FPS in C++

jadi ie game pertama kali NU aing,aing nyien na low budget da soalna laptop aing panas pas compile ulang kali c++ na,tapi game na simple sih tempat temabakan

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

game na mah wave na moal Eren Eren,ciga game zombie jeng loba retro spice na meh low poly katingali na😅

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
