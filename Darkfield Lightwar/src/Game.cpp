#include "Game.h"

#include <chrono>

Game::Game(int shipsPerFaction, uint32_t seed, bool unlimitedFps, bool friendlyCollision, bool friendlyFire)
    : world_(seed), initialShipsPerFaction_(std::max(1, shipsPerFaction)), seed_(seed),
      unlimitedFps_(unlimitedFps) {
    sf::ContextSettings settings;
    // 小舰队保留 4x MSAA；大舰队时 GPU 的 4x 填充成本明显，直接关掉
    settings.antialiasingLevel = (shipsPerFaction > 500) ? 0 : 4;
    settings.depthBits = 0;
    settings.stencilBits = 0;

    // 只创建一次窗口：旧版在初始化列表和函数体各创建一次，
    // 会让启动阶段枚举到已被销毁的 HWND，也让截图工具抓错窗口。
    window_.create(sf::VideoMode(WINDOW_WIDTH, WINDOW_HEIGHT), L"暗域光战", sf::Style::Close, settings);
    window_.setVerticalSyncEnabled(false);
    window_.setFramerateLimit(unlimitedFps_ ? 0 : 60);
    window_.setKeyRepeatEnabled(false);

    {
        const HWND hwnd = static_cast<HWND>(window_.getSystemHandle());
        savedWindowStyle_ = GetWindowLongPtrW(hwnd, GWL_STYLE);
        GetWindowRect(hwnd, &savedWindowRect_);
        windowStyleSaved_ = true;
    }
    updateOutputRect();

    renderer_.init();
    world_.reset(seed_, initialShipsPerFaction_);
    world_.friendlyCollisionEnabled = friendlyCollision;
    world_.friendlyFireEnabled = friendlyFire;
    overlay_.speedIndex = 1;
    overlay_.speed = 1.0f;
    // 大规模舰队下，几百个视野圆会糊成一片，默认关掉；小规模保留旧版观感
    overlay_.visionMode = (initialShipsPerFaction_ > 20) ? 4 : 0;
}

int Game::run() {
    frameClock_.restart();
    while (window_.isOpen()) {
        renderer_.pollDetachedWindows(world_);
        const float dt = std::min(0.25f, std::max(0.0f, frameClock_.restart().asSeconds()));
        handleEvents();
        update(dt);
        render();
    }
    return 0;
}

void Game::handleEvents() {
    sf::Event event{};
    while (window_.pollEvent(event)) {
        if (event.type == sf::Event::Closed) {
            window_.close();
            return;
        }

        if (event.type == sf::Event::TextEntered) {
            if (renderer_.creationHandleText(event.text.unicode)) continue;
        }

        if (event.type == sf::Event::KeyPressed) {
            // 创造面板正在输入数字时，吞掉所有按键，避免误触 R / O / C / F。
            if (renderer_.creationHandleKey(event.key.code)) continue;
            switch (event.key.code) {
            case sf::Keyboard::Escape:
                if (renderer_.monitorIsOpen()) renderer_.monitorToggle();
                else if (renderer_.observationIsOpen()) renderer_.observationToggle();
                else if (renderer_.sandboxIsOpen()) renderer_.sandboxToggle();
                else overlay_.showUI = !overlay_.showUI;
                break;
            case sf::Keyboard::N:
                renderer_.creationToggle();
                break;
            case sf::Keyboard::K:
                renderer_.observationToggle();
                break;
            case sf::Keyboard::B:
                renderer_.sandboxToggle();
                break;
            case sf::Keyboard::F11:
                applyWindowMode((windowMode_ + 1) % 3);
                break;
            case sf::Keyboard::V:
                // 全屏 ↔ 900x506（16:9）等比缩小，仍然看到整张地图。
                if (overlay_.mapRect.width < static_cast<float>(WINDOW_WIDTH) - 1.0f) {
                    overlay_.mapRect = sf::FloatRect(0.0f, 0.0f,
                                                     static_cast<float>(WINDOW_WIDTH),
                                                     static_cast<float>(WINDOW_HEIGHT));
                } else {
                    overlay_.mapRect = sf::FloatRect(0.0f, 0.0f, 900.0f, 506.25f);
                }
                break;
            case sf::Keyboard::Tab:
                overlay_.visionMode = (overlay_.visionMode + 1) % 6;
                break;
            case sf::Keyboard::L:
                overlay_.showLevel = !overlay_.showLevel;
                break;
            case sf::Keyboard::O:
                world_.rebellionEnabled = !world_.rebellionEnabled;
                break;
            case sf::Keyboard::C:
                world_.friendlyCollisionEnabled = !world_.friendlyCollisionEnabled;
                break;
            case sf::Keyboard::F:
                world_.friendlyFireEnabled = !world_.friendlyFireEnabled;
                break;
            case sf::Keyboard::M:
            case sf::Keyboard::H:
                renderer_.monitorToggle();
                break;
            case sf::Keyboard::Space:
                overlay_.paused = !overlay_.paused;
                break;
            case sf::Keyboard::F3:
                overlay_.showDebug = !overlay_.showDebug;
                break;
            case sf::Keyboard::F2: {
                for (size_t i = 0; i < world_.ships().size(); ++i) {
                    if (world_.ships()[i].active) { world_.selectShip(static_cast<int>(i)); break; }
                }
                break;
            }
            case sf::Keyboard::F5:
                world_.scrambleFleets();
                break;
            case sf::Keyboard::F1:
                overlay_.showUI = true;
                break;
            case sf::Keyboard::R:
                reset();
                break;
            case sf::Keyboard::Num1: overlay_.speedIndex = 0; overlay_.speed = 0.5f; break;
            case sf::Keyboard::Num2: overlay_.speedIndex = 1; overlay_.speed = 1.0f; break;
            case sf::Keyboard::Num3: overlay_.speedIndex = 2; overlay_.speed = 2.0f; break;
            case sf::Keyboard::Num4: overlay_.speedIndex = 3; overlay_.speed = 4.0f; break;
            default: break;
            }
        }

        if (event.type == sf::Event::MouseButtonPressed && event.mouseButton.button == sf::Mouse::Left) {
            const Vec2 p = windowToVirtual(static_cast<float>(event.mouseButton.x),
                                           static_cast<float>(event.mouseButton.y));
            if (renderer_.monitorHandleMousePress(p.x, p.y)) continue;
            if (renderer_.creationHandleMousePress(p.x, p.y, world_)) continue;
            if (renderer_.observationHandleMousePress(p.x, p.y, overlay_)) continue;
            if (renderer_.sandboxHandleMousePress(p.x, p.y, world_, overlay_)) continue;

            // 右下角手柄：拖动可把地图等比缩小/放大。
            const sf::FloatRect& mr = overlay_.mapRect;
            if (p.x >= mr.left + mr.width - 24.0f && p.y >= mr.top + mr.height - 24.0f) {
                resizingMap_ = true;
                continue;
            }

            if (!mr.contains(p.x, p.y)) {
                world_.clearSelection();
                continue;
            }
            const int idx = world_.pickShip(screenToWorld(p.x, p.y));
            if (idx >= 0) world_.selectShip(idx);
            else world_.clearSelection();
        }

        if (event.type == sf::Event::MouseButtonPressed && event.mouseButton.button == sf::Mouse::Right) {
            // 右键只用于取消创造面板的放置预览。
            if (renderer_.creationIsPlacing()) renderer_.creationCancelPlacing();
        }

        if (event.type == sf::Event::MouseButtonReleased && event.mouseButton.button == sf::Mouse::Left) {
            const Vec2 p = windowToVirtual(static_cast<float>(event.mouseButton.x),
                                           static_cast<float>(event.mouseButton.y));
            resizingMap_ = false;
            renderer_.monitorHandleMouseRelease(p.x, p.y);
            renderer_.creationHandleMouseRelease(p.x, p.y);
            renderer_.observationHandleMouseRelease(p.x, p.y);
            renderer_.sandboxHandleMouseRelease(p.x, p.y);
        }

        if (event.type == sf::Event::MouseMoved) {
            const Vec2 p = windowToVirtual(static_cast<float>(event.mouseMove.x),
                                           static_cast<float>(event.mouseMove.y));
            renderer_.monitorHandleMouseMove(p.x, p.y);
            renderer_.creationHandleMouseMove(p.x, p.y);
            renderer_.observationHandleMouseMove(p.x, p.y);
            renderer_.sandboxHandleMouseMove(p.x, p.y);
            if (resizingMap_) updateMapResize(p.x, p.y);
        }

        if (event.type == sf::Event::MouseWheelScrolled) {
            const Vec2 p = windowToVirtual(static_cast<float>(event.mouseWheelScroll.x),
                                           static_cast<float>(event.mouseWheelScroll.y));
            renderer_.monitorHandleWheel(p.x, p.y, event.mouseWheelScroll.delta);
        }
    }
}

void Game::update(float frameDt) {
    if (frameDt <= 0.0f) return;

    if (overlay_.windowModeRequest >= 0) {
        applyWindowMode(overlay_.windowModeRequest);
        overlay_.windowModeRequest = -1;
    }

    if (!overlay_.paused) {
        accumulator_ += frameDt * overlay_.speed;
        constexpr int kMaxSteps = 10;
        int steps = 0;
        double totalMs = 0.0;

        while (accumulator_ >= FIXED_DT && steps < kMaxSteps) {
            const auto t0 = std::chrono::steady_clock::now();
            world_.step(FIXED_DT);
            const auto t1 = std::chrono::steady_clock::now();
            totalMs += std::chrono::duration<double, std::milli>(t1 - t0).count();
            accumulator_ -= FIXED_DT;
            ++steps;
        }
        if (steps == kMaxSteps) accumulator_ = 0.0f;
        simMs_ = steps > 0 ? static_cast<float>(totalMs / steps) : simMs_ * 0.9f;
    }

    const float instFps = 1.0f / std::max(0.0001f, frameDt);
    fps_ = fps_ * 0.92f + instFps * 0.08f;
    overlay_.fps = fps_;
    overlay_.simMs = simMs_;
}

void Game::render() {
    updateOutputRect();
    overlay_.outputRect = outputRect_;
    overlay_.windowMode = windowMode_;
    window_.clear(sf::Color::Black);
    renderer_.draw(window_, world_, overlay_);
    window_.display();
    renderer_.drawDetachedWindows(window_, world_);
}

void Game::reset() {
    world_.reset(seed_, initialShipsPerFaction_);
    accumulator_ = 0.0f;
    overlay_.paused = false;
    overlay_.visionMode = (initialShipsPerFaction_ > 20) ? 4 : 0;
}

Vec2 Game::screenToWorld(float x, float y) const {
    const sf::FloatRect& r = overlay_.mapRect;
    const float w = std::max(1.0f, r.width);
    const float h = std::max(1.0f, r.height);
    return { (x - r.left) * world_.worldWidth() / w,
             (y - r.top) * world_.worldHeight() / h };
}

Vec2 Game::windowToVirtual(float x, float y) const {
    const float w = std::max(1.0f, outputRect_.width);
    const float h = std::max(1.0f, outputRect_.height);
    return { (x - outputRect_.left) * static_cast<float>(WINDOW_WIDTH) / w,
             (y - outputRect_.top) * static_cast<float>(WINDOW_HEIGHT) / h };
}

void Game::updateOutputRect() {
    const sf::Vector2u size = window_.getSize();
    if (size.x == 0 || size.y == 0) return;
    const float scale = std::min(static_cast<float>(size.x) / static_cast<float>(WINDOW_WIDTH),
                                 static_cast<float>(size.y) / static_cast<float>(WINDOW_HEIGHT));
    const float w = static_cast<float>(WINDOW_WIDTH) * scale;
    const float h = static_cast<float>(WINDOW_HEIGHT) * scale;
    outputRect_ = sf::FloatRect((static_cast<float>(size.x) - w) * 0.5f,
                                (static_cast<float>(size.y) - h) * 0.5f, w, h);
}

void Game::applyWindowMode(int mode) {
    mode = std::clamp(mode, 0, 2);
    if (mode == windowMode_) return;
    if (!windowStyleSaved_) return;

    const HWND hwnd = static_cast<HWND>(window_.getSystemHandle());
    if (mode == 0) {
        SetWindowLongPtrW(hwnd, GWL_STYLE, savedWindowStyle_);
        SetWindowPos(hwnd, HWND_NOTOPMOST,
                     savedWindowRect_.left, savedWindowRect_.top,
                     savedWindowRect_.right - savedWindowRect_.left,
                     savedWindowRect_.bottom - savedWindowRect_.top,
                     SWP_FRAMECHANGED | SWP_SHOWWINDOW | SWP_NOOWNERZORDER);
        SetForegroundWindow(hwnd);
    } else {
        MONITORINFO mi{};
        mi.cbSize = sizeof(mi);
        const HMONITOR monitor = MonitorFromWindow(hwnd, MONITOR_DEFAULTTONEAREST);
        if (!GetMonitorInfoW(monitor, &mi)) return;
        const RECT target = (mode == 1) ? mi.rcMonitor : mi.rcWork;
        SetWindowLongPtrW(hwnd, GWL_STYLE, WS_POPUP | WS_VISIBLE | WS_CLIPCHILDREN);
        SetWindowPos(hwnd, (mode == 1) ? HWND_TOPMOST : HWND_NOTOPMOST,
                     target.left, target.top, target.right - target.left,
                     target.bottom - target.top,
                     SWP_FRAMECHANGED | SWP_SHOWWINDOW | SWP_NOOWNERZORDER);
        SetForegroundWindow(hwnd);
    }
    windowMode_ = mode;
    overlay_.windowMode = mode;
    window_.requestFocus();
    updateOutputRect();
}

void Game::updateMapResize(float x, float) {
    // 保持 16:9：拖右下角只改宽度，高度跟着算；始终锚在左上角。
    float w = clampf(x - overlay_.mapRect.left, 480.0f, static_cast<float>(WINDOW_WIDTH));
    float h = w * static_cast<float>(WINDOW_HEIGHT) / static_cast<float>(WINDOW_WIDTH);
    if (h > static_cast<float>(WINDOW_HEIGHT)) {
        h = static_cast<float>(WINDOW_HEIGHT);
        w = static_cast<float>(WINDOW_WIDTH);
    }
    overlay_.mapRect = sf::FloatRect(0.0f, 0.0f, w, h);
}
