// ui.h — HUD, menus, radar, kill feed
#pragma once
#include "core.h"
#include "game.h"

namespace ui {
void drawHUD(Game& g, const Mat4& vp, int w, int h);
void drawMenu(Game& g, int w, int h);
void drawPause(Game& g, int w, int h);
void drawGameOver(Game& g, int w, int h);
}
