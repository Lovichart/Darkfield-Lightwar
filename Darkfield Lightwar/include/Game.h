// Darkfield Lightwar - 游戏循环 / 输入
#pragma once

#include "Common.h"
#include "Renderer.h"
#include "World.h"

class Game {
public:
    Game(int shipsPerFaction, uint32_t seed, bool unlimitedFps,
         bool friendlyCollision = true, bool friendlyFire = true);
    int run();

private:
    void handleEvents();
    void update(float frameDt);
    void render();
    void reset();

    Vec2 screenToWorld(float x, float y) const;
    Vec2 windowToVirtual(float x, float y) const;
    void updateOutputRect();
    void applyWindowMode(int mode);
    void updateMapResize(float x, float y);

    sf::RenderWindow window_;
    World world_;
    Renderer renderer_;
    OverlayState overlay_;

    sf::Clock frameClock_;
    float accumulator_ = 0.0f;
    float fps_ = 60.0f;
    float simMs_ = 0.0f;

    int initialShipsPerFaction_ = 5;
    uint32_t seed_ = 12345u;
    bool unlimitedFps_ = false;
    bool resizingMap_ = false;

    // 0 窗口 / 1 全屏 / 2 无边框全屏（保留任务栏）
    int windowMode_ = 0;
    bool windowStyleSaved_ = false;
    LONG_PTR savedWindowStyle_ = 0;
    RECT savedWindowRect_{};
    sf::FloatRect outputRect_{0.0f, 0.0f, static_cast<float>(WINDOW_WIDTH),
                              static_cast<float>(WINDOW_HEIGHT)};
};
