// Darkfield Lightwar - 批量渲染与 HUD
#pragma once

#include "Common.h"
#include "Stats.h"
#include "World.h"

class Renderer;

struct OverlayState {
    bool showUI = false;
    bool showLevel = false;
    bool showDebug = false;
    bool paused = false;
    bool showPortalInfo = true;
    bool showTitles = true;
    int speedIndex = 1;
    float speed = 1.0f;
    int visionMode = 0;
    float fps = 0.0f;
    float simMs = 0.0f;
    // 世界渲染区域（屏幕像素）。默认全窗口；拖右下角手柄 / V 键可等比缩小，
    // 缩小后仍能看到整张地图，给创造面板腾出空间。
    sf::FloatRect mapRect{0.0f, 0.0f, static_cast<float>(WINDOW_WIDTH),
                          static_cast<float>(WINDOW_HEIGHT)};
    // 1600×900 虚拟画面在真实窗口里的像素矩形（全屏/无边框时等比放大 + 黑边）。
    sf::FloatRect outputRect{0.0f, 0.0f, static_cast<float>(WINDOW_WIDTH),
                             static_cast<float>(WINDOW_HEIGHT)};
    int windowMode = 0; // 0 窗口 / 1 全屏 / 2 无边框留任务栏（面板显示用）
    int windowModeRequest = -1; // 沙盒面板请求切换显示模式，Game 处理后清零
};

struct MonitorRect {
    float x = 0.0f, y = 0.0f, w = 0.0f, h = 0.0f;
    bool contains(float px, float py) const {
        return px >= x && px <= x + w && py >= y && py <= y + h;
    }
};

// 一个排行条：key 用来跨帧匹配同一对象，从而做“换位/伸缩”动画。
struct MonitorBarAnim {
    uint64_t key = 0;
    int faction = 0;
    uint64_t factionId = 0;
    uint32_t serial = 0;
    int level = 1;
    std::string title;
    double value = 0.0;
    double shown = 0.0;
    float y = 0.0f;
    float targetY = 0.0f;
    float width = 0.0f;
    float targetWidth = 0.0f;
    float pop = 0.0f;
    bool valid = false;
};

// 统计面板打开的独立排行窗口（可无限开）
struct MonitorChild {
    bool individual = false;
    FactionMetric factionMetric = FactionMetric::ShipCount;
    IndividualMetric individualMetric = IndividualMetric::Level;
    StatRange range = StatRange::Total;
    IndividualFilter filter;
    float x = 0.0f, y = 0.0f;
    bool dragging = false;
    float dragDX = 0.0f, dragDY = 0.0f;
    int barCount = 4;
    int format = 1;          // 1 = 字/条同一行；2 = 字与细条分开
    float rowWidth = 430.0f; // 方条区域宽度（不含右侧数值列）
    bool showEliminated = false;
    std::array<MonitorBarAnim, 16> bars{};

    // 弹出到桌面：独立 OS 窗口 + 独立字体/渲染器（GL 纹理不能跨窗口共享）。
    bool detached = false;
    float dockX = 0.0f, dockY = 0.0f;
    std::shared_ptr<sf::RenderWindow> detachedWindow;
    std::shared_ptr<Renderer> detachedRenderer;
};

// 统计面板本体状态
struct MonitorUi {
    bool open = false;
    float x = 44.0f, y = 64.0f;
    bool dragging = false;
    float dragDX = 0.0f, dragDY = 0.0f;
    bool individual = false;
    int factionMetric = 0;
    int individualMetric = 0;
    StatRange range = StatRange::Total;
    IndividualFilter filter;
    bool showEliminated = false;
    float metricScroll = 0.0f;
    std::vector<MonitorChild> children;
    int activeChild = -1;
    sf::Clock uiClock;
};

// 创造面板状态：所有字段都是“面板值”，点随机/手动时才转成 CreationSpec。
struct CreationUi {
    bool open = false;
    float x = 944.0f, y = 64.0f;
    bool dragging = false;
    float dragDX = 0.0f, dragDY = 0.0f;

    int kind = 0;      // 0 飞船 / 1 小行星 / 2 机遇
    int mode = 0;      // 0 投放 / 1 增援 / 2 传送门
    int faction = 0;
    int formation = 0; // 0 方阵 / 1 圆阵 / 2 密集 / 3 随机

    int shipCount = 12;
    int shipSpeed = 40;
    ShipAttributePoints attrs;

    int asteroidType = 0;
    int asteroidRadius = 34;
    int asteroidCount = 12;
    int asteroidSpeed = 30;
    int irregularPercent = 55;

    int zoneDuration = 16;
    int zoneRadius = 100;
    int zoneType = 0;

    int portalHealth = 1000;
    int portalIntervalIndex = 0;

    // 手动放置：0=等待第一点，1=等待方向；机遇/传送门一次点击完成
    bool placing = false;
    int placementStage = 0;
    Vec2 anchor;

    std::string hint;
    float hintTimer = 0.0f;

    int editField = -1;
    std::string editBuffer;
    bool editFresh = false; // 刚点开输入框：第一次输入数字时替换旧值
    float mouseX = 0.0f, mouseY = 0.0f;

    // 弹出为独立 OS 窗口（和排行窗口一样），主窗口只留地图。
    bool detached = false;
    float dockX = 0.0f, dockY = 0.0f;
    std::shared_ptr<sf::RenderWindow> detachedWindow;
    std::shared_ptr<Renderer> detachedRenderer;
};

// 观察面板：统合视野 / 等级 / 传送门信息 / 称号显示四个开关。
struct ObservationUi {
    bool open = false;
    float x = 44.0f, y = 410.0f;
    bool dragging = false;
    float dragDX = 0.0f, dragDY = 0.0f;
    int lastVisionMode = 0; // 关闭视野前用的模式，重新打开时恢复
};

// 沙盒面板：战场面积倍率 + 显示模式（窗口 / 全屏 / 无边框留任务栏）。
struct SandboxUi {
    bool open = false;
    float x = 44.0f, y = 64.0f;
    bool dragging = false;
    float dragDX = 0.0f, dragDY = 0.0f;
};

class Renderer {
public:
    Renderer();
    ~Renderer();
    bool init(bool textOnly = false);

    void draw(sf::RenderWindow& window, const World& world, OverlayState& overlay);
    void pollDetachedWindows(World& world);
    void drawDetachedWindows(sf::RenderWindow& mainWindow, const World& world);

    // 统计面板 / 排行窗口的事件处理：返回 true 表示事件被 UI 吃掉。
    void monitorToggle();
    bool monitorIsOpen() const { return monitor_.open; }
    bool monitorHandleMousePress(float x, float y);
    bool monitorHandleMouseRelease(float x, float y);
    bool monitorHandleMouseMove(float x, float y);
    bool monitorHandleWheel(float x, float y, float delta);

    // 创造面板：N 开关；数字/回车/退格在编辑框内输入。
    void creationToggle();
    bool creationIsOpen() const { return creation_.open; }
    bool creationIsEditing() const { return creation_.editField >= 0; }
    bool creationIsPlacing() const { return creation_.placing; }
    bool creationHandleMousePress(float x, float y, World& world, bool fromDetached = false);
    bool creationHandleMouseRelease(float x, float y);
    bool creationHandleMouseMove(float x, float y);
    bool creationHandleKey(sf::Keyboard::Key code);
    bool creationHandleText(sf::Uint32 unicode);
    void creationCancelPlacing();
    void creationPopOut();
    bool creationDetached() const { return creation_.detached; }

    // 观察面板：K 开关。
    void observationToggle();
    bool observationIsOpen() const { return observation_.open; }
    bool observationHandleMousePress(float x, float y, OverlayState& overlay);
    bool observationHandleMouseRelease(float x, float y);
    bool observationHandleMouseMove(float x, float y);

    // 沙盒面板：B 开关；战场大小 + 显示模式。
    void sandboxToggle();
    bool sandboxIsOpen() const { return sandbox_.open; }
    bool sandboxHandleMousePress(float x, float y, World& world, OverlayState& overlay);
    bool sandboxHandleMouseRelease(float x, float y);
    bool sandboxHandleMouseMove(float x, float y);

private:
    void drawVision(sf::RenderWindow& window, const World& world, int mode);
    void drawZones(sf::RenderWindow& window, const World& world);
    void drawCometTrails(sf::RenderWindow& window, const World& world);
    void drawAsteroids(sf::RenderWindow& window, const World& world);
    void drawShips(sf::RenderWindow& window, const World& world, bool showLevel);
    void drawBeams(sf::RenderWindow& window, const World& world);
    void drawLights(sf::RenderWindow& window, const World& world);
    void drawTrails(sf::RenderWindow& window, const World& world);
    void drawSelection(sf::RenderWindow& window, const World& world);
    void drawLevelNumbers(sf::RenderWindow& window, const World& world);
    void drawTitles(sf::RenderWindow& window, const World& world);

    void drawUI(sf::RenderWindow& window, const World& world, OverlayState& overlay);
    void drawFactionPanel(sf::RenderWindow& window, const World& world, int faction, float x, float y, float w, float h);
    void drawSelectedInfo(sf::RenderWindow& window, const World& world);
    void drawStatus(sf::RenderWindow& window, const World& world, const OverlayState& overlay);
    void drawDebug(sf::RenderWindow& window, const World& world, const OverlayState& overlay);

    // 统计面板
    void drawMonitor(sf::RenderWindow& window, const World& world);
    void drawMonitorPanel(sf::RenderWindow& window, const World& world);
    void drawMonitorChild(sf::RenderWindow& window, const World& world, MonitorChild& child);
    void updateMonitorChild(MonitorChild& child, const World& world);
    void popOutChild(MonitorChild& child);
    void syncDetachedWindow(MonitorChild& child);
    void drawDetachedChild(sf::RenderWindow& window, const World& world, MonitorChild& child);
    MonitorRect monitorPanelRect() const;
    MonitorRect monitorChildRect(const MonitorChild& child) const;
    float monitorChildWidth(const MonitorChild& child) const;
    float monitorChildHeight(const MonitorChild& child) const;
    float monitorRowHeight(const MonitorChild& child) const;
    void clampMonitorChild(MonitorChild& child) const;
    int metricCount(bool individual) const;
    const char* metricName(bool individual, int index) const;
    bool metricCumulative(bool individual, int index) const;
    int selectedMetric() const;
    void clampRangeForMetric();

    // 创造面板 / 传送门绘制与输入
    void drawCreation(sf::RenderWindow& window, const World& world);
    void drawCreationPanel(sf::RenderWindow& window, const World& world);
    void drawCreationPreview(sf::RenderWindow& window, const World& world);
    void drawPlacementStrip(sf::RenderWindow& window);
    void drawPortals(sf::RenderWindow& window, const World& world, bool showInfo);

    // 观察面板
    void drawObservation(sf::RenderWindow& window, const World& world, OverlayState& overlay);
    void clampObservationPanel();

    // 沙盒面板
    void drawSandbox(sf::RenderWindow& window, const World& world, OverlayState& overlay);
    void clampSandboxPanel();
    void drawVortex(sf::RenderWindow& window, Vec2 pos, float radius, sf::Color color,
                    float rotation, float alpha) const;
    CreationSpec creationSpec() const;
    void creationBeginEdit(int field);
    void creationCommitEdit();
    int creationFieldValue(int field) const;
    void creationSetField(int field, int value);
    void creationRandomAttributes(int level);
    void creationRandomCreate(World& world);
    void creationPlaceCommit(World& world, Vec2 second);
    float creationPreviewRadius() const;
    sf::Color creationPreviewColor() const;
    void clampCreationPanel();
    void closeCreationPopout();
    void pollCreationDetachedWindow(World& world);
    Vec2 screenToWorld(float x, float y) const;

    void text(sf::RenderWindow& window, const std::string& utf8, float x, float y,
              unsigned size, sf::Color color);
    void textCentered(sf::RenderWindow& window, const std::string& utf8, float cx, float y,
                      unsigned size, sf::Color color);
    sf::FloatRect textBounds(const std::string& utf8, unsigned size) const;

    bool buildDigitAtlas();
    void addDigitQuads(sf::VertexArray& out, int number, float cx, float cy, float scale,
                       sf::Color color) const;

    // fontData_ 必须先声明、后析构：FreeType 的 face 引用这块内存，
    // 如果 font_ 先析构后再释放 fontData_，字体析构阶段可能访问已释放内存。
    std::vector<char> fontData_;
    sf::Font font_;
    bool fontLoaded_ = false;
    sf::Texture digitTex_;
    bool digitAtlasReady_ = false;
    float digitCellW_ = 20.0f;
    float digitCellH_ = 26.0f;

    // 小行星图集：所有小行星共用一张纹理，一次 draw call 画完
    sf::Texture asteroidAtlas_;
    bool asteroidAtlasReady_ = false;
    std::array<uint32_t, MAX_ASTEROIDS> asteroidAtlasGen_{};
    sf::VertexArray asteroidQuads_{sf::Quads};

    // 称号图集：每个称号懒加载到一个 96x22 单元，所有称号一次 draw call。
    struct TitleCell {
        int col = 0;
        int row = 0;
        float width = 0.0f;
    };
    bool ensureTitleAtlas();
    const TitleCell* ensureTitleCell(const std::string& title);
    sf::Texture titleTex_;
    bool titleAtlasReady_ = false;
    float titleCellW_ = 160.0f;
    float titleCellH_ = 28.0f;
    int titleNextCell_ = 0;
    std::unordered_map<std::string, TitleCell> titleCells_;
    sf::VertexArray titleBgQuads_{sf::Quads};
    sf::VertexArray titleQuads_{sf::Quads};

    sf::VertexArray shipBody_{sf::Triangles};
    sf::VertexArray shipGlow_{sf::Triangles};
    sf::VertexArray beamQuads_{sf::Quads};
    sf::VertexArray lightQuads_{sf::Quads};
    sf::VertexArray trailQuads_{sf::Quads};
    sf::VertexArray visionLines_{sf::Lines};
    sf::VertexArray visionFans_{sf::Triangles};
    sf::VertexArray levelQuads_{sf::Quads};
    mutable sf::Text measureText_;

    MonitorUi monitor_;
    CreationUi creation_;
    ObservationUi observation_;
    SandboxUi sandbox_;
    sf::FloatRect mapRect_{0.0f, 0.0f, static_cast<float>(WINDOW_WIDTH),
                           static_cast<float>(WINDOW_HEIGHT)};
    sf::FloatRect outputRect_{0.0f, 0.0f, static_cast<float>(WINDOW_WIDTH),
                             static_cast<float>(WINDOW_HEIGHT)};
    float worldWidth_ = static_cast<float>(WINDOW_WIDTH);
    float worldHeight_ = static_cast<float>(WINDOW_HEIGHT);
    std::mt19937 creationRng_{std::random_device{}()};
    float monitorDt_ = 1.0f / 60.0f;
};
