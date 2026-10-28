#include "Renderer.h"

#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <iomanip>
#include <iterator>
#include <sstream>

namespace {
constexpr float SHIP_SIZE = 5.0f;

sf::Color withAlpha(sf::Color c, int a) {
    c.a = static_cast<sf::Uint8>(std::clamp(a, 0, 255));
    return c;
}

void addQuad(sf::VertexArray& out, Vec2 p, float half, sf::Color color) {
    const sf::Vector2f c = p.sf();
    out.append(sf::Vertex({c.x - half, c.y - half}, color));
    out.append(sf::Vertex({c.x + half, c.y - half}, color));
    out.append(sf::Vertex({c.x + half, c.y + half}, color));
    out.append(sf::Vertex({c.x - half, c.y + half}, color));
}

std::string fmt1(float v) {
    std::ostringstream ss;
    ss << std::fixed << std::setprecision(1) << v;
    return ss.str();
}

constexpr float kPanelW = 600.0f;
constexpr float kPanelH = 780.0f;
constexpr float kChildTitleH = 32.0f;
constexpr float kChildControlH = 28.0f;
constexpr float kChildNoteH = 24.0f;
constexpr float kChildLeft = 44.0f;
constexpr float kValueColW = 86.0f;
constexpr float kChildRight = 12.0f;
constexpr float kBarRowH1 = 44.0f;
constexpr float kBarRowH2 = 38.0f;
constexpr float kBarH1 = 34.0f;
constexpr float kBarH2 = 10.0f;
constexpr int kMinBars = 3;
constexpr int kMaxBars = 14;

float childPixelWidth(const MonitorChild& c) {
    return kChildLeft + c.rowWidth + 8.0f + kValueColW + kChildRight;
}
float childPixelHeight(const MonitorChild& c) {
    const int rows = std::clamp(c.barCount, kMinBars, kMaxBars);
    return kChildTitleH + kChildControlH + static_cast<float>(rows) * (c.format == 1 ? kBarRowH1 : kBarRowH2) + kChildNoteH;
}
float childRowHeight(const MonitorChild& c) {
    return c.format == 1 ? kBarRowH1 : kBarRowH2;
}

struct PanelLayout {
    MonitorRect rect, title, close;
    MonitorRect scope[2];
    MonitorRect range[5];
    MonitorRect eliminatedToggle;
    MonitorRect filterFaction[5];
    MonitorRect titlesOnly;
    MonitorRect levelLabel, levelMinus, levelValue, levelPlus;
    MonitorRect metricList;
    MonitorRect open, closeAll;
};

struct ChildLayout {
    MonitorRect rect, title, close;
    MonitorRect barMinus, barPlus, widthMinus, widthPlus, formatBtn, popBtn;
};

PanelLayout panelLayout(const MonitorUi& ui) {
    PanelLayout l;
    l.rect = { ui.x, ui.y, kPanelW, kPanelH };
    l.title = { ui.x, ui.y, kPanelW, 36.0f };
    l.close = { ui.x + kPanelW - 36.0f, ui.y + 6.0f, 28.0f, 24.0f };
    l.scope[0] = { ui.x + 20.0f, ui.y + 70.0f, 110.0f, 30.0f };
    l.scope[1] = { ui.x + 140.0f, ui.y + 70.0f, 110.0f, 30.0f };
    for (int i = 0; i < 5; ++i) {
        l.range[i] = { ui.x + 20.0f + static_cast<float>(i) * 88.0f, ui.y + 132.0f, 84.0f, 30.0f };
    }
    l.eliminatedToggle = { ui.x + 464.0f, ui.y + 132.0f, 110.0f, 30.0f };
    for (int i = 0; i < 5; ++i) {
        l.filterFaction[i] = { ui.x + 20.0f + static_cast<float>(i) * 58.0f, ui.y + 194.0f, 54.0f, 30.0f };
    }
    l.titlesOnly = { ui.x + 316.0f, ui.y + 194.0f, 100.0f, 30.0f };
    l.levelLabel = { ui.x + 424.0f, ui.y + 194.0f, 52.0f, 30.0f };
    l.levelMinus = { ui.x + 478.0f, ui.y + 194.0f, 28.0f, 30.0f };
    l.levelValue = { ui.x + 508.0f, ui.y + 194.0f, 34.0f, 30.0f };
    l.levelPlus = { ui.x + 544.0f, ui.y + 194.0f, 30.0f, 30.0f };
    l.metricList = { ui.x + 16.0f, ui.y + 258.0f, 568.0f, 402.0f };
    l.open = { ui.x + 20.0f, ui.y + kPanelH - 104.0f, 280.0f, 44.0f };
    l.closeAll = { ui.x + 310.0f, ui.y + kPanelH - 104.0f, 270.0f, 44.0f };
    return l;
}

ChildLayout childLayout(const MonitorChild& c) {
    ChildLayout l;
    const float w = childPixelWidth(c);
    const float h = childPixelHeight(c);
    l.rect = { c.x, c.y, w, h };
    l.title = { c.x, c.y, w, kChildTitleH };
    l.close = { c.x + w - 32.0f, c.y + 4.0f, 26.0f, 24.0f };
    const float cy = c.y + kChildTitleH + 4.0f;
    l.barMinus = { c.x + 44.0f, cy, 22.0f, 20.0f };
    l.barPlus = { c.x + 92.0f, cy, 22.0f, 20.0f };
    l.widthMinus = { c.x + 148.0f, cy, 22.0f, 20.0f };
    l.widthPlus = { c.x + 196.0f, cy, 22.0f, 20.0f };
    l.formatBtn = { c.x + 230.0f, cy, 74.0f, 20.0f };
    l.popBtn = { c.x + 310.0f, cy, 64.0f, 20.0f };
    return l;
}

void drawFactionDot(sf::RenderWindow& window, int faction, float x, float y, float r) {
    sf::CircleShape dot(r);
    dot.setOrigin(r, r);
    dot.setPosition(x, y);
    dot.setFillColor(factionColor(faction));
    window.draw(dot);
}

// 主窗口全屏时是 topmost；弹出面板也要 topmost，否则会被游戏窗口盖住。
void makeTopmost(sf::RenderWindow& window) {
#ifdef _WIN32
    SetWindowPos(static_cast<HWND>(window.getSystemHandle()), HWND_TOPMOST,
                 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
#endif
}

// 个体排行里的“飞船图案”：和游戏里的船一样是个小三角。
void drawShipGlyph(sf::RenderWindow& window, int faction, float x, float y, float size) {
    sf::ConvexShape tri;
    tri.setPointCount(3);
    tri.setPoint(0, { size, 0.0f });
    tri.setPoint(1, { -size * 0.8f, -size * 0.7f });
    tri.setPoint(2, { -size * 0.8f, size * 0.7f });
    tri.setPosition(x, y);
    tri.setFillColor(factionColor(faction));
    window.draw(tri);
}

// ============================================================
// 创造面板布局
// ============================================================

constexpr float kCreateW = 640.0f;
constexpr float kCreateH = 700.0f;

enum CreationFieldId {
    CF_NONE = -1,
    CF_SHIP_COUNT,
    CF_SHIP_SPEED,
    CF_SHIP_LEVEL,
    CF_ATTR0,
    CF_ATTR1,
    CF_ATTR2,
    CF_ATTR3,
    CF_ATTR4,
    CF_AST_RADIUS,
    CF_AST_COUNT,
    CF_AST_SPEED,
    CF_IRR,
    CF_ZONE_DUR,
    CF_ZONE_RAD,
    CF_PORTAL_HP
};

struct CreationLayout {
    MonitorRect rect, title, close, popBtn;
    MonitorRect kind[3], mode[3];
    MonitorRect portalHpMinus, portalHpField, portalHpPlus;
    MonitorRect interval[4];
    MonitorRect faction[4];
    MonitorRect shipCountMinus, shipCountField, shipCountPlus;
    MonitorRect formation[4];
    MonitorRect shipSpeedMinus, shipSpeedField, shipSpeedPlus;
    MonitorRect attrMinus[5], attrField[5], attrPlus[5];
    MonitorRect levelField, levelRandom;
    MonitorRect astType[2];
    MonitorRect astRadiusMinus, astRadiusField, astRadiusPlus;
    MonitorRect astCountMinus, astCountField, astCountPlus;
    MonitorRect astSpeedMinus, astSpeedField, astSpeedPlus;
    MonitorRect irrMinus, irrField, irrPlus;
    MonitorRect zoneDurMinus, zoneDurField, zoneDurPlus;
    MonitorRect zoneRadMinus, zoneRadField, zoneRadPlus;
    MonitorRect zoneType[6];
    MonitorRect randomBtn, manualBtn;
};

CreationLayout creationLayout(const CreationUi& ui) {
    const float x = ui.x;
    const float y = ui.y;
    CreationLayout l;
    l.rect = { x, y, kCreateW, kCreateH };
    l.title = { x, y, kCreateW, 36.0f };
    l.close = { x + kCreateW - 36.0f, y + 6.0f, 28.0f, 24.0f };
    l.popBtn = { x + kCreateW - 104.0f, y + 6.0f, 60.0f, 24.0f };

    for (int i = 0; i < 3; ++i) {
        l.kind[i] = { x + 16.0f + static_cast<float>(i) * 160.0f, y + 64.0f, 150.0f, 28.0f };
        l.mode[i] = { x + 16.0f + static_cast<float>(i) * 160.0f, y + 116.0f, 150.0f, 28.0f };
    }

    l.portalHpMinus = { x + 52.0f, y + 152.0f, 24.0f, 28.0f };
    l.portalHpField = { x + 78.0f, y + 152.0f, 88.0f, 28.0f };
    l.portalHpPlus = { x + 168.0f, y + 152.0f, 24.0f, 28.0f };
    for (int i = 0; i < 4; ++i) {
        l.interval[i] = { x + 244.0f + static_cast<float>(i) * 70.0f, y + 152.0f, 64.0f, 28.0f };
    }

    // 飞船
    for (int i = 0; i < 4; ++i) {
        l.faction[i] = { x + 70.0f + static_cast<float>(i) * 52.0f, y + 192.0f, 46.0f, 28.0f };
    }
    l.shipCountMinus = { x + 70.0f, y + 228.0f, 24.0f, 28.0f };
    l.shipCountField = { x + 96.0f, y + 228.0f, 62.0f, 28.0f };
    l.shipCountPlus = { x + 160.0f, y + 228.0f, 24.0f, 28.0f };
    for (int i = 0; i < 4; ++i) {
        l.formation[i] = { x + 290.0f + static_cast<float>(i) * 64.0f, y + 228.0f, 58.0f, 28.0f };
    }
    l.shipSpeedMinus = { x + 70.0f, y + 264.0f, 24.0f, 28.0f };
    l.shipSpeedField = { x + 96.0f, y + 264.0f, 62.0f, 28.0f };
    l.shipSpeedPlus = { x + 160.0f, y + 264.0f, 24.0f, 28.0f };
    for (int i = 0; i < 5; ++i) {
        const float ry = y + 326.0f + static_cast<float>(i) * 32.0f;
        l.attrMinus[i] = { x + 118.0f, ry, 24.0f, 28.0f };
        l.attrField[i] = { x + 144.0f, ry, 62.0f, 28.0f };
        l.attrPlus[i] = { x + 208.0f, ry, 24.0f, 28.0f };
    }
    l.levelField = { x + 86.0f, y + 492.0f, 62.0f, 28.0f };
    l.levelRandom = { x + 154.0f, y + 492.0f, 92.0f, 28.0f };

    // 小行星
    for (int i = 0; i < 2; ++i) {
        l.astType[i] = { x + 70.0f + static_cast<float>(i) * 86.0f, y + 192.0f, 80.0f, 28.0f };
    }
    l.astRadiusMinus = { x + 70.0f, y + 228.0f, 24.0f, 28.0f };
    l.astRadiusField = { x + 96.0f, y + 228.0f, 62.0f, 28.0f };
    l.astRadiusPlus = { x + 160.0f, y + 228.0f, 24.0f, 28.0f };
    l.astCountMinus = { x + 70.0f, y + 264.0f, 24.0f, 28.0f };
    l.astCountField = { x + 96.0f, y + 264.0f, 62.0f, 28.0f };
    l.astCountPlus = { x + 160.0f, y + 264.0f, 24.0f, 28.0f };
    l.astSpeedMinus = { x + 70.0f, y + 300.0f, 24.0f, 28.0f };
    l.astSpeedField = { x + 96.0f, y + 300.0f, 62.0f, 28.0f };
    l.astSpeedPlus = { x + 160.0f, y + 300.0f, 24.0f, 28.0f };
    l.irrMinus = { x + 70.0f, y + 336.0f, 24.0f, 28.0f };
    l.irrField = { x + 96.0f, y + 336.0f, 62.0f, 28.0f };
    l.irrPlus = { x + 160.0f, y + 336.0f, 24.0f, 28.0f };

    // 机遇
    l.zoneDurMinus = { x + 70.0f, y + 192.0f, 24.0f, 28.0f };
    l.zoneDurField = { x + 96.0f, y + 192.0f, 62.0f, 28.0f };
    l.zoneDurPlus = { x + 160.0f, y + 192.0f, 24.0f, 28.0f };
    l.zoneRadMinus = { x + 70.0f, y + 228.0f, 24.0f, 28.0f };
    l.zoneRadField = { x + 96.0f, y + 228.0f, 62.0f, 28.0f };
    l.zoneRadPlus = { x + 160.0f, y + 228.0f, 24.0f, 28.0f };
    for (int i = 0; i < 6; ++i) {
        const int row = i / 2;
        const int col = i % 2;
        l.zoneType[i] = { x + 16.0f + static_cast<float>(col) * 304.0f,
                          y + 288.0f + static_cast<float>(row) * 40.0f, 288.0f, 34.0f };
    }

    l.randomBtn = { x + 16.0f, y + kCreateH - 120.0f, 292.0f, 46.0f };
    l.manualBtn = { x + 320.0f, y + kCreateH - 120.0f, 292.0f, 46.0f };
    return l;
}

const char* creationKindName(int kind) {
    static const char* names[3] = { "飞船", "小行星", "机遇" };
    return names[std::clamp(kind, 0, 2)];
}

const char* spawnModeName(int mode) {
    static const char* names[3] = { "投放", "增援", "传送门" };
    return names[std::clamp(mode, 0, 2)];
}

const char* formationName(int formation) {
    static const char* names[4] = { "方阵", "圆阵", "密集", "随机" };
    return names[std::clamp(formation, 0, 3)];
}

const char* asteroidTypeName(int type) {
    return type == 1 ? "彗星" : "小行星";
}

const char* zoneTypeName(int type) {
    static const char* names[6] = { "增援", "生存", "进攻", "机动", "生产", "领域" };
    return names[std::clamp(type, 0, 5)];
}

const char* factionShortName(int faction) {
    static const char* names[4] = { "红", "蓝", "绿", "黄" };
    return names[std::clamp(faction, 0, 3)];
}

const int kPortalIntervals[4] = { 60, 180, 300, 600 };

const char* visionModeName(int mode) {
    static const char* names[6] = { "圆形", "半透明", "细线", "叠加", "关闭", "实心" };
    return names[std::clamp(mode, 0, 5)];
}

// ============================================================
// 观察面板布局
// ============================================================

constexpr float kObsW = 380.0f;
constexpr float kObsH = 292.0f;

struct ObservationLayout {
    MonitorRect rect, title, close;
    MonitorRect row[4];
    MonitorRect allOn, allOff;
};

ObservationLayout observationLayout(const ObservationUi& ui) {
    ObservationLayout l;
    l.rect = { ui.x, ui.y, kObsW, kObsH };
    l.title = { ui.x, ui.y, kObsW, 34.0f };
    l.close = { ui.x + kObsW - 34.0f, ui.y + 5.0f, 26.0f, 24.0f };
    for (int i = 0; i < 4; ++i) {
        l.row[i] = { ui.x + 12.0f, ui.y + 46.0f + static_cast<float>(i) * 42.0f,
                     356.0f, 34.0f };
    }
    l.allOn = { ui.x + 16.0f, ui.y + kObsH - 54.0f, 170.0f, 38.0f };
    l.allOff = { ui.x + 194.0f, ui.y + kObsH - 54.0f, 170.0f, 38.0f };
    return l;
}

// ============================================================
// 沙盒面板布局
// ============================================================

constexpr float kSandboxW = 470.0f;
constexpr float kSandboxH = 340.0f;

struct SandboxLayout {
    MonitorRect rect, title, close;
    MonitorRect sizeBtn[4];
    MonitorRect modeBtn[3];
    MonitorRect restartBtn;
};

SandboxLayout sandboxLayout(const SandboxUi& ui) {
    SandboxLayout l;
    l.rect = { ui.x, ui.y, kSandboxW, kSandboxH };
    l.title = { ui.x, ui.y, kSandboxW, 34.0f };
    l.close = { ui.x + kSandboxW - 34.0f, ui.y + 5.0f, 26.0f, 24.0f };
    for (int i = 0; i < 4; ++i) {
        l.sizeBtn[i] = { ui.x + 16.0f + static_cast<float>(i) * 112.0f,
                         ui.y + 70.0f, 104.0f, 52.0f };
    }
    for (int i = 0; i < 3; ++i) {
        l.modeBtn[i] = { ui.x + 16.0f + static_cast<float>(i) * 146.0f,
                         ui.y + 168.0f, 140.0f, 44.0f };
    }
    l.restartBtn = { ui.x + 16.0f, ui.y + kSandboxH - 48.0f, 210.0f, 34.0f };
    return l;
}

} // namespace

Renderer::Renderer()
    : shipBody_(sf::Triangles), shipGlow_(sf::Triangles), beamQuads_(sf::Quads),
      lightQuads_(sf::Quads), trailQuads_(sf::Quads), visionLines_(sf::Lines),
      visionFans_(sf::Triangles), levelQuads_(sf::Quads) {}

Renderer::~Renderer() = default;

bool Renderer::init(bool textOnly) {
    // SFML 的 loadFromFile 走窄字符 fopen，中文路径会失败。
    // 这里自己按二进制读入再 loadFromMemory，路径由 std::filesystem 处理（宽字符）。
    auto loadFont = [&](const std::filesystem::path& path) -> bool {
        std::ifstream in(path, std::ios::binary);
        if (!in.is_open()) return false;
        std::vector<char> data((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
        if (data.empty()) return false;
        if (!font_.loadFromMemory(data.data(), data.size())) return false;
        fontData_ = std::move(data); // FreeType face 引用这块内存
        return true;
    };

    const char* fonts[] = {
        "fonts/LXGWWenKaiTC-Light.ttf",
        "fonts/msyh.ttc",
        "fonts/SourceHanSans-Light.ttf",
        "fonts/SmileySans-Oblique.otf",
        "fonts/arial.ttf"
    };
    for (const char* f : fonts) {
        if (loadFont(assetPath(f))) {
            fontLoaded_ = true;
            break;
        }
    }
    if (!fontLoaded_) {
        fontLoaded_ = loadFont(std::filesystem::path("C:/Windows/Fonts/msyh.ttc"));
    }
    if (textOnly) return fontLoaded_;

    buildDigitAtlas();

    // 2048x2048，28x28 个 72px 单元，足够 360 颗小行星
    sf::Image atlas;
    atlas.create(2048, 2048, sf::Color::Transparent);
    asteroidAtlasReady_ = asteroidAtlas_.loadFromImage(atlas);
    if (asteroidAtlasReady_) asteroidAtlas_.setSmooth(false);
    asteroidAtlasGen_.fill(0u);

    ensureTitleAtlas();
    return fontLoaded_;
}

bool Renderer::buildDigitAtlas() {
    if (!fontLoaded_) return false;

    // 数字按 1:1 像素画进图集，绘制时不再二次缩放。
    // 旧版用 24x32 的 cell 缩到 0.58*0.5，每个数字只剩约 7x9 像素，
    // 笔画被最近邻采样吃掉，飞船一动就闪。
    constexpr unsigned kCellW = 20;
    constexpr unsigned kCellH = 26;
    constexpr unsigned kPad = 1;
    constexpr unsigned kCharSize = 22;

    sf::RenderTexture rt;
    if (!rt.create(kCellW * 10, kCellH)) return false;
    rt.clear(sf::Color::Transparent);
    for (int i = 0; i < 10; ++i) {
        sf::Text t;
        t.setFont(font_);
        t.setString(sf::String(static_cast<sf::Uint32>('0' + i)));
        t.setCharacterSize(kCharSize);
        t.setFillColor(sf::Color::White);
        t.setStyle(sf::Text::Bold);
        const sf::FloatRect b = t.getLocalBounds();
        const float cellX = static_cast<float>(i * kCellW);
        const float innerW = static_cast<float>(kCellW - kPad * 2);
        const float innerH = static_cast<float>(kCellH - kPad * 2);
        t.setPosition(cellX + static_cast<float>(kPad) + (innerW - b.width) * 0.5f - b.left,
                      static_cast<float>(kPad) + (innerH - b.height) * 0.5f - b.top);
        rt.draw(t);
    }
    rt.display();
    if (!digitTex_.loadFromImage(rt.getTexture().copyToImage())) return false;
    digitTex_.setSmooth(false);
    digitCellW_ = static_cast<float>(kCellW);
    digitCellH_ = static_cast<float>(kCellH);
    digitAtlasReady_ = true;
    return true;
}

bool Renderer::ensureTitleAtlas() {
    if (titleAtlasReady_) return true;
    sf::Image atlas;
    atlas.create(2048, 2048, sf::Color::Transparent);
    titleAtlasReady_ = titleTex_.loadFromImage(atlas);
    if (titleAtlasReady_) titleTex_.setSmooth(false);
    return titleAtlasReady_;
}

const Renderer::TitleCell* Renderer::ensureTitleCell(const std::string& title) {
    if (!titleAtlasReady_ || title.empty()) return nullptr;
    auto it = titleCells_.find(title);
    if (it != titleCells_.end()) return &it->second;

    const int cols = static_cast<int>(2048.0f / titleCellW_);
    const int rows = static_cast<int>(2048.0f / titleCellH_);
    if (titleNextCell_ >= cols * rows) return nullptr;
    const int col = titleNextCell_ % cols;
    const int row = titleNextCell_ / cols;
    ++titleNextCell_;

    sf::RenderTexture rt;
    if (!rt.create(static_cast<unsigned>(titleCellW_), static_cast<unsigned>(titleCellH_))) {
        return nullptr;
    }
    rt.clear(sf::Color::Transparent);
    sf::Text t;
    t.setFont(font_);
    t.setString(sf::String::fromUtf8(title.begin(), title.end()));
    t.setCharacterSize(18);
    t.setStyle(sf::Text::Bold);
    t.setFillColor(sf::Color::White);
    const sf::FloatRect b = t.getLocalBounds();
    t.setPosition(2.0f - b.left, 1.0f - b.top);
    rt.draw(t);
    rt.display();

    sf::Image img = rt.getTexture().copyToImage();
    titleTex_.update(img, static_cast<unsigned>(col) * static_cast<unsigned>(titleCellW_),
                     static_cast<unsigned>(row) * static_cast<unsigned>(titleCellH_));

    TitleCell cell;
    cell.col = col;
    cell.row = row;
    cell.width = std::min(titleCellW_ - 2.0f, std::ceil(b.width) + 4.0f);
    auto ins = titleCells_.emplace(title, cell);
    return &ins.first->second;
}

void Renderer::drawTitles(sf::RenderWindow& window, const World& world) {
    if (!titleAtlasReady_) return;
    titleBgQuads_.clear();
    titleQuads_.clear();
    constexpr float kDrawH = 24.0f;
    for (const auto& s : world.ships()) {
        if (!s.active || s.title.empty()) continue;
        if (s.pos.x < -80.0f || s.pos.x > worldWidth_ + 80.0f ||
            s.pos.y < -80.0f || s.pos.y > worldHeight_ + 80.0f) {
            continue;
        }
        const TitleCell* cell = ensureTitleCell(s.title);
        if (!cell) continue;
        const float w = cell->width;
        const float x = std::floor(s.pos.x - w * 0.5f);
        // 两档纵向错开：密集编队里称号不会全部叠在同一行。
        const float stagger = static_cast<float>(s.serial % 2u) * 24.0f;
        float titleY = s.pos.y + 12.0f + stagger;
        if (titleY + kDrawH > worldHeight_ - 4.0f) titleY = s.pos.y - 34.0f;
        const float y = std::floor(titleY);

        // 半透明黑底，压在金色文字下面，亮色小行星/激光上也能读。
        titleBgQuads_.append(sf::Vertex({x - 4.0f, y - 2.0f}, sf::Color(0, 0, 0, 145)));
        titleBgQuads_.append(sf::Vertex({x + w + 4.0f, y - 2.0f}, sf::Color(0, 0, 0, 145)));
        titleBgQuads_.append(sf::Vertex({x + w + 4.0f, y + kDrawH + 2.0f}, sf::Color(0, 0, 0, 145)));
        titleBgQuads_.append(sf::Vertex({x - 4.0f, y + kDrawH + 2.0f}, sf::Color(0, 0, 0, 145)));

        const float u0 = static_cast<float>(cell->col) * titleCellW_ + 1.0f;
        const float v0 = static_cast<float>(cell->row) * titleCellH_ + 1.0f;
        const float u1 = u0 + w;
        const float v1 = v0 + kDrawH;
        const auto add = [&](float ox, float oy, sf::Color color) {
            titleQuads_.append(sf::Vertex({x + ox, y + oy}, color, {u0, v0}));
            titleQuads_.append(sf::Vertex({x + ox + w, y + oy}, color, {u1, v0}));
            titleQuads_.append(sf::Vertex({x + ox + w, y + oy + kDrawH}, color, {u1, v1}));
            titleQuads_.append(sf::Vertex({x + ox, y + oy + kDrawH}, color, {u0, v1}));
        };
        add(1.0f, 1.0f, sf::Color(0, 0, 0, 190));
        add(0.0f, 0.0f, sf::Color(255, 228, 150, 235));
    }
    if (titleBgQuads_.getVertexCount() > 0) window.draw(titleBgQuads_);
    if (titleQuads_.getVertexCount() == 0) return;
    sf::RenderStates states;
    states.texture = &titleTex_;
    window.draw(titleQuads_, states);
}

void Renderer::text(sf::RenderWindow& window, const std::string& utf8, float x, float y,
                    unsigned size, sf::Color color) {
    if (!fontLoaded_) return;
    sf::Text t;
    t.setFont(font_);
    t.setString(sf::String::fromUtf8(utf8.begin(), utf8.end()));
    t.setCharacterSize(size);
    t.setFillColor(color);
    t.setPosition(std::floor(x), std::floor(y));
    window.draw(t);
}

sf::FloatRect Renderer::textBounds(const std::string& utf8, unsigned size) const {
    measureText_.setFont(font_);
    measureText_.setString(sf::String::fromUtf8(utf8.begin(), utf8.end()));
    measureText_.setCharacterSize(size);
    return measureText_.getLocalBounds();
}

void Renderer::textCentered(sf::RenderWindow& window, const std::string& utf8, float cx, float y,
                            unsigned size, sf::Color color) {
    if (!fontLoaded_) return;
    const sf::FloatRect b = textBounds(utf8, size);
    text(window, utf8, cx - b.width * 0.5f - b.left, y, size, color);
}

void Renderer::draw(sf::RenderWindow& window, const World& world, OverlayState& overlay) {
    mapRect_ = overlay.mapRect;
    if (mapRect_.width < 320.0f || mapRect_.height < 180.0f) {
        mapRect_ = sf::FloatRect(0.0f, 0.0f, static_cast<float>(WINDOW_WIDTH),
                                 static_cast<float>(WINDOW_HEIGHT));
    }
    worldWidth_ = std::max(1.0f, world.worldWidth());
    worldHeight_ = std::max(1.0f, world.worldHeight());

    const sf::Vector2u winSize = window.getSize();
    const float winW = static_cast<float>(std::max(1u, winSize.x));
    const float winH = static_cast<float>(std::max(1u, winSize.y));
    outputRect_ = overlay.outputRect;
    if (outputRect_.width < 1.0f || outputRect_.height < 1.0f) {
        outputRect_ = sf::FloatRect(0.0f, 0.0f, winW, winH);
    }
    const float uiScaleX = outputRect_.width / static_cast<float>(WINDOW_WIDTH);
    const float uiScaleY = outputRect_.height / static_cast<float>(WINDOW_HEIGHT);

    // 1600×900 虚拟画面 → 真实窗口（全屏/无边框时等比放大，周围留黑边）。
    sf::View uiView(sf::FloatRect(0.0f, 0.0f, static_cast<float>(WINDOW_WIDTH),
                                  static_cast<float>(WINDOW_HEIGHT)));
    uiView.setViewport(sf::FloatRect(outputRect_.left / winW, outputRect_.top / winH,
                                     outputRect_.width / winW, outputRect_.height / winH));
    const bool mapScaled = mapRect_.left > 0.5f || mapRect_.top > 0.5f ||
                           mapRect_.width < static_cast<float>(WINDOW_WIDTH) - 0.5f ||
                           mapRect_.height < static_cast<float>(WINDOW_HEIGHT) - 0.5f;

    // 世界 → 虚拟 mapRect → 真实窗口。世界越大，物体越小。
    sf::View worldView(sf::FloatRect(0.0f, 0.0f, worldWidth_, worldHeight_));
    const float px = outputRect_.left + mapRect_.left * uiScaleX;
    const float py = outputRect_.top + mapRect_.top * uiScaleY;
    const float pw = mapRect_.width * uiScaleX;
    const float ph = mapRect_.height * uiScaleY;
    worldView.setViewport(sf::FloatRect(px / winW, py / winH, pw / winW, ph / winH));
    window.setView(worldView);

    drawVision(window, world, overlay.visionMode);
    drawZones(window, world);
    drawPortals(window, world, overlay.showPortalInfo);
    drawCometTrails(window, world);
    drawAsteroids(window, world);
    drawShips(window, world, overlay.showLevel);
    drawBeams(window, world);
    drawLights(window, world);
    drawTrails(window, world);
    drawSelection(window, world);
    if (overlay.showLevel) drawLevelNumbers(window, world);
    if (overlay.showTitles) drawTitles(window, world);
    if (creation_.placing) drawCreationPreview(window, world);

    window.setView(uiView);

    // 缩放后的地图边框 + 右下角拖拽手柄（用鼠标拖动可继续缩小/放大）。
    {
        sf::RectangleShape border({mapRect_.width, mapRect_.height});
        border.setPosition(mapRect_.left, mapRect_.top);
        border.setFillColor(sf::Color::Transparent);
        border.setOutlineColor(sf::Color(100, 190, 175, mapScaled ? 120 : 40));
        border.setOutlineThickness(1.0f);
        window.draw(border);

        const float gx = mapRect_.left + mapRect_.width - 22.0f;
        const float gy = mapRect_.top + mapRect_.height - 22.0f;
        sf::VertexArray grip(sf::Lines, 6);
        for (int i = 0; i < 3; ++i) {
            grip[static_cast<size_t>(i * 2)] =
                sf::Vertex({gx + 4.0f + static_cast<float>(i) * 5.0f, gy + 18.0f},
                           sf::Color(150, 235, 220, 150));
            grip[static_cast<size_t>(i * 2 + 1)] =
                sf::Vertex({gx + 18.0f, gy + 4.0f + static_cast<float>(i) * 5.0f},
                           sf::Color(150, 235, 220, 150));
        }
        window.draw(grip);
    }

    drawUI(window, world, overlay);
}

void Renderer::drawVision(sf::RenderWindow& window, const World& world, int mode) {
    if (mode == 4) return;
    visionLines_.clear();
    visionFans_.clear();

    const int segments = (mode == 2) ? 32 : 40;
    const bool onlySelected = world.activeShipCount > 120;
    for (const auto& s : world.ships()) {
        if (!s.active) continue;
        if (onlySelected && !s.selected) continue;
        if (s.pos.x + s.viewRange < 0.0f || s.pos.x - s.viewRange > worldWidth_) continue;
        if (s.pos.y + s.viewRange < 0.0f || s.pos.y - s.viewRange > worldHeight_) continue;

        const sf::Color c = factionColor(s.faction);
        if (mode == 0 || mode == 2) {
            const int alpha = (mode == 2) ? 45 : 75;
            for (int k = 0; k < segments; ++k) {
                const float a0 = static_cast<float>(k) / segments * TAU;
                const float a1 = static_cast<float>(k + 1) / segments * TAU;
                const Vec2 p0 = s.pos + Vec2(std::cos(a0), std::sin(a0)) * s.viewRange;
                const Vec2 p1 = s.pos + Vec2(std::cos(a1), std::sin(a1)) * s.viewRange;
                visionLines_.append(sf::Vertex(p0.sf(), withAlpha(c, alpha)));
                visionLines_.append(sf::Vertex(p1.sf(), withAlpha(c, alpha)));
            }
        } else {
            const int alpha = (mode == 1) ? 16 : (mode == 3 ? 8 : 30);
            for (int k = 0; k < segments; ++k) {
                const float a0 = static_cast<float>(k) / segments * TAU;
                const float a1 = static_cast<float>(k + 1) / segments * TAU;
                const Vec2 p0 = s.pos + Vec2(std::cos(a0), std::sin(a0)) * s.viewRange;
                const Vec2 p1 = s.pos + Vec2(std::cos(a1), std::sin(a1)) * s.viewRange;
                visionFans_.append(sf::Vertex(s.pos.sf(), withAlpha(c, alpha)));
                visionFans_.append(sf::Vertex(p0.sf(), withAlpha(c, alpha)));
                visionFans_.append(sf::Vertex(p1.sf(), withAlpha(c, alpha)));
            }
        }
    }

    if (visionFans_.getVertexCount() > 0) {
        sf::RenderStates states;
        if (mode == 3) states.blendMode = sf::BlendAdd;
        window.draw(visionFans_, states);
    }
    if (visionLines_.getVertexCount() > 0) {
        window.draw(visionLines_);
    }
}

void Renderer::drawZones(sf::RenderWindow& window, const World& world) {
    for (const auto& z : world.zones()) {
        if (!z.active) continue;
        const sf::Color c = z.color;

        sf::VertexArray ring(sf::Lines);
        const int segments = 48;
        for (int i = 0; i < segments; ++i) {
            if (i % 2 != 0) continue;
            const float a0 = static_cast<float>(i) / segments * TAU + z.rotation;
            const float a1 = static_cast<float>(i + 1) / segments * TAU + z.rotation;
            const Vec2 p0 = z.pos + Vec2(std::cos(a0), std::sin(a0)) * z.radius;
            const Vec2 p1 = z.pos + Vec2(std::cos(a1), std::sin(a1)) * z.radius;
            ring.append(sf::Vertex(p0.sf(), withAlpha(c, 150)));
            ring.append(sf::Vertex(p1.sf(), withAlpha(c, 150)));
        }
        window.draw(ring);

        sf::CircleShape dot(3.0f);
        dot.setOrigin(3.0f, 3.0f);
        dot.setPosition(z.pos.sf());
        dot.setFillColor(withAlpha(c, 110));
        window.draw(dot);

        const float scale = std::min(1.0f, z.radius / 30.0f);
        if (scale <= 0.05f) continue;
        switch (z.type) {
        case ZoneType::Reinforce: {
            sf::ConvexShape tri;
            tri.setPointCount(3);
            tri.setPoint(0, {0.0f, -8.0f});
            tri.setPoint(1, {-7.0f, 6.0f});
            tri.setPoint(2, {7.0f, 6.0f});
            tri.setOrigin(0.0f, 0.0f);
            tri.setPosition(z.pos.sf());
            tri.setScale(scale, scale);
            tri.setFillColor(sf::Color::Transparent);
            tri.setOutlineColor(withAlpha(c, 200));
            tri.setOutlineThickness(1.2f);
            window.draw(tri);
            break;
        }
        case ZoneType::Survival: {
            sf::RectangleShape h({16.0f, 4.0f});
            h.setOrigin(8.0f, 2.0f);
            h.setPosition(z.pos.sf());
            h.setScale(scale, scale);
            h.setFillColor(withAlpha(c, 190));
            window.draw(h);
            sf::RectangleShape v({4.0f, 16.0f});
            v.setOrigin(2.0f, 8.0f);
            v.setPosition(z.pos.sf());
            v.setScale(scale, scale);
            v.setFillColor(withAlpha(c, 190));
            window.draw(v);
            break;
        }
        case ZoneType::Attack: {
            for (int k = 0; k < 3; ++k) {
                const float a = z.rotation + static_cast<float>(k) * TAU / 3.0f;
                const Vec2 p1 = z.pos + Vec2(std::cos(a), std::sin(a)) * (10.0f * scale);
                sf::Vertex line[2] = { sf::Vertex(z.pos.sf(), withAlpha(c, 210)),
                                       sf::Vertex(p1.sf(), withAlpha(c, 210)) };
                window.draw(line, 2, sf::Lines);
            }
            break;
        }
        case ZoneType::Mobility: {
            sf::VertexArray arrow(sf::Lines, 4);
            arrow[0].position = z.pos.sf();
            arrow[0].color = withAlpha(c, 200);
            arrow[1].position = (z.pos + Vec2(10.0f * scale, 0.0f)).sf();
            arrow[1].color = withAlpha(c, 200);
            arrow[2].position = arrow[1].position;
            arrow[2].color = withAlpha(c, 200);
            arrow[3].position = (z.pos + Vec2(5.0f * scale, -5.0f * scale)).sf();
            arrow[3].color = withAlpha(c, 200);
            window.draw(arrow);
            break;
        }
        case ZoneType::Production: {
            sf::RectangleShape box({14.0f, 14.0f});
            box.setOrigin(7.0f, 7.0f);
            box.setPosition(z.pos.sf());
            box.setScale(scale, scale);
            box.setFillColor(sf::Color::Transparent);
            box.setOutlineColor(withAlpha(c, 200));
            box.setOutlineThickness(1.2f);
            window.draw(box);
            break;
        }
        case ZoneType::Domain:
        default: {
            sf::CircleShape ring2(10.0f * scale);
            ring2.setOrigin(10.0f * scale, 10.0f * scale);
            ring2.setPosition(z.pos.sf());
            ring2.setFillColor(sf::Color::Transparent);
            ring2.setOutlineColor(withAlpha(c, 190));
            ring2.setOutlineThickness(1.0f);
            window.draw(ring2);
            break;
        }
        }
    }
}

void Renderer::drawVortex(sf::RenderWindow& window, Vec2 pos, float radius, sf::Color color,
                          float rotation, float alpha) const {
    if (radius < 2.0f) return;
    const float a = clampf(alpha, 0.0f, 1.0f);
    sf::RenderStates add;
    add.blendMode = sf::BlendAdd;

    sf::CircleShape glow(radius * 1.08f, 40);
    glow.setOrigin(radius * 1.08f, radius * 1.08f);
    glow.setPosition(pos.sf());
    glow.setFillColor(sf::Color(color.r, color.g, color.b,
                                static_cast<sf::Uint8>(22.0f * a)));
    window.draw(glow, add);

    // 三条对数螺旋臂：旋转时就是旋涡。
    sf::VertexArray arms(sf::Lines);
    constexpr int kArms = 3;
    constexpr int kSeg = 46;
    arms.resize(static_cast<size_t>(kArms * kSeg * 2));
    size_t vi = 0;
    for (int arm = 0; arm < kArms; ++arm) {
        for (int k = 0; k < kSeg; ++k) {
            const float t0 = static_cast<float>(k) / kSeg;
            const float t1 = static_cast<float>(k + 1) / kSeg;
            const float r0 = radius * (0.14f + 0.86f * t0);
            const float r1 = radius * (0.14f + 0.86f * t1);
            const float a0 = rotation + static_cast<float>(arm) * TAU / 3.0f + t0 * 1.25f * PI;
            const float a1 = rotation + static_cast<float>(arm) * TAU / 3.0f + t1 * 1.25f * PI;
            const Vec2 p0 = pos + Vec2(std::cos(a0), std::sin(a0)) * r0;
            const Vec2 p1 = pos + Vec2(std::cos(a1), std::sin(a1)) * r1;
            const int alpha0 = static_cast<int>((26.0f + 190.0f * t0) * a);
            const int alpha1 = static_cast<int>((26.0f + 190.0f * t1) * a);
            arms[vi++] = sf::Vertex(p0.sf(), withAlpha(color, alpha0));
            arms[vi++] = sf::Vertex(p1.sf(), withAlpha(color, alpha1));
        }
    }
    window.draw(arms, add);

    sf::CircleShape core(radius * 0.12f, 24);
    core.setOrigin(radius * 0.12f, radius * 0.12f);
    core.setPosition(pos.sf());
    core.setFillColor(withAlpha(color, static_cast<int>(210.0f * a)));
    window.draw(core, add);

    sf::CircleShape ring(radius, 48);
    ring.setOrigin(radius, radius);
    ring.setPosition(pos.sf());
    ring.setFillColor(sf::Color::Transparent);
    ring.setOutlineColor(withAlpha(color, static_cast<int>(95.0f * a)));
    ring.setOutlineThickness(1.3f);
    window.draw(ring, add);
}

void Renderer::drawPortals(sf::RenderWindow& window, const World& world, bool showInfo) {
    for (const Portal& p : world.portals()) {
        if (!p.active) continue;

        const float flash = clampf(p.flashTimer / 0.6f, 0.0f, 1.0f);
        drawVortex(window, p.pos, p.radius * (1.0f + 0.06f * flash),
                   colorScale(p.color, 1.1f + 0.6f * flash), p.rotation, 1.0f);

        // 血环：淡淡一圈底环 + 按血量比例点亮的弧形。
        const float frac = p.maxHealth > 0.0f ? clampf(p.health / p.maxHealth, 0.0f, 1.0f) : 0.0f;
        constexpr int kSeg = 64;
        sf::VertexArray baseRing(sf::Lines);
        sf::VertexArray healthArc(sf::Lines);
        for (int i = 0; i < kSeg; ++i) {
            const float a0 = static_cast<float>(i) / kSeg * TAU - PI * 0.5f;
            const float a1 = static_cast<float>(i + 1) / kSeg * TAU - PI * 0.5f;
            const float rr = p.radius + 5.0f;
            const Vec2 q0 = p.pos + Vec2(std::cos(a0), std::sin(a0)) * rr;
            const Vec2 q1 = p.pos + Vec2(std::cos(a1), std::sin(a1)) * rr;
            baseRing.append(sf::Vertex(q0.sf(), sf::Color(255, 255, 255, 28)));
            baseRing.append(sf::Vertex(q1.sf(), sf::Color(255, 255, 255, 28)));
            if (static_cast<float>(i) / kSeg < frac) {
                healthArc.append(sf::Vertex(q0.sf(), withAlpha(p.color, 220)));
                healthArc.append(sf::Vertex(q1.sf(), withAlpha(p.color, 220)));
            }
        }
        window.draw(baseRing);
        window.draw(healthArc);

        if (!showInfo) continue;

        std::string label = "传送门 · ";
        if (p.kind == CreationKind::Ship) {
            label += std::string(factionShortName(p.faction)) + "方飞船";
        } else if (p.kind == CreationKind::Asteroid) {
            label += "小行星";
        } else {
            label += std::string("机遇·") + zoneTypeName(static_cast<int>(p.spec.zoneType));
        }
        textCentered(window, label, p.pos.x, p.pos.y - p.radius - 40.0f, 13,
                     withAlpha(p.color, 230));
        const int hp = static_cast<int>(std::ceil(p.health));
        const int next = std::max(0, static_cast<int>(std::ceil(p.spawnInterval - p.spawnTimer)));
        textCentered(window, std::to_string(hp) + "/" + std::to_string(static_cast<int>(p.maxHealth)) +
                                 " · 下一波 " + std::to_string(next) + "s",
                     p.pos.x, p.pos.y - p.radius - 24.0f, 11, sf::Color(220, 228, 240, 190));
    }
}

void Renderer::drawCometTrails(sf::RenderWindow& window, const World& world) {
    sf::RenderStates states;
    states.blendMode = sf::BlendAdd;
    for (const auto& a : world.asteroids()) {
        if (!a.active || a.type != AsteroidType::Comet || a.trailCount < 2) continue;
        const int count = a.trailCount;
        sf::VertexArray strip(sf::TriangleStrip, static_cast<size_t>(count) * 2);
        for (int k = 0; k < count; ++k) {
            const Vec2 p = a.trail[k];
            const Vec2 next = a.trail[std::min(k + 1, count - 1)];
            Vec2 dir = p - next;
            if (dir.lengthSq() < 1e-6f) dir = {1.0f, 0.0f};
            const Vec2 perp = dir.normalized().perpendicular();
            const float t = static_cast<float>(k) / static_cast<float>(std::max(1, count - 1));
            const float w = a.radius * 0.75f * (1.0f - t);
            const sf::Color c = withAlpha(sf::Color(120, 150, 255), static_cast<int>(85.0f * (1.0f - t)));
            strip[static_cast<size_t>(k) * 2] = sf::Vertex((p + perp * w).sf(), c);
            strip[static_cast<size_t>(k) * 2 + 1] = sf::Vertex((p - perp * w).sf(), c);
        }
        window.draw(strip, states);
    }
}

void Renderer::drawAsteroids(sf::RenderWindow& window, const World& world) {
    if (world.asteroids().empty()) return;
    if (!asteroidAtlasReady_) return;

    constexpr int kCell = 72;
    constexpr int kCols = 28;
    constexpr int kPad = 2;

    asteroidQuads_.clear();
    for (size_t i = 0; i < world.asteroids().size() && i < MAX_ASTEROIDS; ++i) {
        const Asteroid& a = world.asteroids()[i];
        if (!a.active) continue;

        const int cellCol = static_cast<int>(i) % kCols;
        const int cellRow = static_cast<int>(i) / kCols;
        const int cellX = cellCol * kCell;
        const int cellY = cellRow * kCell;

        if (asteroidAtlasGen_[i] != a.gen || a.imageDirty) {
            sf::Image cell;
            cell.create(kCell, kCell, sf::Color::Transparent);
            if (a.image.getSize().x > 0) cell.copy(a.image, kPad, kPad);
            asteroidAtlas_.update(cell, static_cast<unsigned>(cellX), static_cast<unsigned>(cellY));
            asteroidAtlasGen_[i] = a.gen;
            a.imageDirty = false;
        }

        const float texSize = static_cast<float>(a.gridN * 2 + 1);
        if (texSize <= 0.0f) continue;
        const float half = texSize * a.pixelSize * 0.5f;
        const float c = std::cos(a.rot), s = std::sin(a.rot);
        const Vec2 corners[4] = { {-half, -half}, {half, -half}, {half, half}, {-half, half} };
        const float u0 = static_cast<float>(cellX + kPad);
        const float v0 = static_cast<float>(cellY + kPad);
        const float u1 = u0 + texSize;
        const float v1 = v0 + texSize;
        const sf::Vector2f uv[4] = { {u0, v0}, {u1, v0}, {u1, v1}, {u0, v1} };
        const sf::Color white = sf::Color::White;
        for (int k = 0; k < 4; ++k) {
            const Vec2 p = a.pos + Vec2(corners[k].x * c - corners[k].y * s,
                                        corners[k].x * s + corners[k].y * c);
            asteroidQuads_.append(sf::Vertex(p.sf(), white, uv[k]));
        }

        if (a.type == AsteroidType::Comet) {
            sf::CircleShape glow(a.radius * 0.9f);
            glow.setOrigin(a.radius * 0.9f, a.radius * 0.9f);
            glow.setPosition(a.pos.sf());
            glow.setFillColor(sf::Color(80, 110, 255, 22));
            window.draw(glow);
        }
    }

    if (asteroidQuads_.getVertexCount() > 0) {
        sf::RenderStates states;
        states.texture = &asteroidAtlas_;
        window.draw(asteroidQuads_, states);
    }
}

void Renderer::drawShips(sf::RenderWindow& window, const World& world, bool showLevel) {
    (void)showLevel;
    shipBody_.clear();
    shipGlow_.clear();

    const Vec2 local[3] = { {SHIP_SIZE, 0.0f}, {-SHIP_SIZE * 0.8f, -SHIP_SIZE * 0.5f},
                            {-SHIP_SIZE * 0.8f, SHIP_SIZE * 0.5f} };

    for (const auto& s : world.ships()) {
        if (!s.active) continue;
        const float ratio = s.maxHealth > 0.0f ? std::clamp(s.health / s.maxHealth, 0.0f, 1.0f) : 0.0f;
        const sf::Color body = colorScale(s.color, 0.35f + 0.65f * ratio, 255);
        const sf::Color glow = colorScale(s.color, 1.0f, 45);

        // 每艘船只算一次 sin/cos：10k 艘时这是实打实的每帧省 4 次三角函数
        const float c = std::cos(s.rot);
        const float sn = std::sin(s.rot);
        for (int k = 0; k < 3; ++k) {
            const float lx = local[k].x;
            const float ly = local[k].y;
            const Vec2 p(s.pos.x + lx * c - ly * sn, s.pos.y + lx * sn + ly * c);
            shipBody_.append(sf::Vertex(p.sf(), body));
        }
        for (int k = 0; k < 3; ++k) {
            const float lx = local[k].x * 1.55f;
            const float ly = local[k].y * 1.55f;
            const Vec2 q(s.pos.x + lx * c - ly * sn, s.pos.y + lx * sn + ly * c);
            shipGlow_.append(sf::Vertex(q.sf(), glow));
        }
    }

    if (shipGlow_.getVertexCount() > 0 && world.activeShipCount <= 1500) {
        sf::RenderStates states;
        states.blendMode = sf::BlendAdd;
        window.draw(shipGlow_, states);
    }
    if (shipBody_.getVertexCount() > 0) window.draw(shipBody_);
}

void Renderer::drawBeams(sf::RenderWindow& window, const World& world) {
    beamQuads_.clear();
    for (const auto& b : world.beams()) {
        if (b.life <= 0.0f) continue;
        const Vec2 dir = (b.b - b.a).normalized();
        const Vec2 perp = dir.perpendicular() * 1.4f;
        const int alpha = static_cast<int>(220.0f * std::clamp(b.life / std::max(0.001f, b.maxLife), 0.0f, 1.0f));
        const sf::Color c = withAlpha(b.color, alpha);
        beamQuads_.append(sf::Vertex((b.a + perp).sf(), c));
        beamQuads_.append(sf::Vertex((b.a - perp).sf(), c));
        beamQuads_.append(sf::Vertex((b.b - perp).sf(), c));
        beamQuads_.append(sf::Vertex((b.b + perp).sf(), c));
    }
    if (beamQuads_.getVertexCount() > 0) {
        sf::RenderStates states;
        states.blendMode = sf::BlendAdd;
        window.draw(beamQuads_, states);
    }
}

void Renderer::drawLights(sf::RenderWindow& window, const World& world) {
    lightQuads_.clear();
    for (const auto& l : world.lights()) {
        if (!l.active) continue;
        const float flicker = 0.8f + 0.2f * std::sin(static_cast<float>(world.stepCount) * 0.3f +
                                                     (l.pos.x + l.pos.y) * 0.05f);
        const float healthRatio = l.maxHealth > 0.0f
                                      ? std::clamp(l.health / l.maxHealth, 0.25f, 1.0f)
                                      : 1.0f;
        addQuad(lightQuads_, l.pos, l.radius * 1.7f * flicker,
                withAlpha(l.color, static_cast<int>(210.0f * healthRatio)));
    }
    if (lightQuads_.getVertexCount() > 0) {
        sf::RenderStates states;
        states.blendMode = sf::BlendAdd;
        window.draw(lightQuads_, states);
    }
}

void Renderer::drawTrails(sf::RenderWindow& window, const World& world) {
    trailQuads_.clear();
    for (const auto& p : world.trails()) {
        const float t = p.maxLife > 0.0f ? std::clamp(p.life / p.maxLife, 0.0f, 1.0f) : 1.0f;
        addQuad(trailQuads_, p.pos, p.size, withAlpha(p.color, static_cast<int>(170.0f * (1.0f - t))));
    }
    if (trailQuads_.getVertexCount() > 0) {
        sf::RenderStates states;
        states.blendMode = sf::BlendAdd;
        window.draw(trailQuads_, states);
    }
}

void Renderer::drawSelection(sf::RenderWindow& window, const World& world) {
    const Ship* s = world.selected();
    if (!s) return;

    sf::CircleShape circle(11.0f);
    circle.setOrigin(11.0f, 11.0f);
    circle.setPosition(s->pos.sf());
    circle.setFillColor(sf::Color::Transparent);
    circle.setOutlineColor(sf::Color(255, 255, 255, 190));
    circle.setOutlineThickness(1.2f);
    window.draw(circle);

    Vec2 target;
    bool hasTarget = false;
    if (s->state == ShipState::Attacking) {
        if (s->targetShip < world.ships().size()) {
            const Ship& t = world.ships()[s->targetShip];
            if (t.active) { target = t.pos; hasTarget = true; }
        }
    } else if (s->state == ShipState::Mining) {
        if (s->targetAst < world.asteroids().size()) {
            const Asteroid& a = world.asteroids()[s->targetAst];
            if (a.active) { target = a.pos; hasTarget = true; }
        }
    }
    if (hasTarget) {
        sf::Vertex line[2] = { sf::Vertex(s->pos.sf(), sf::Color(255, 255, 255, 70)),
                               sf::Vertex(target.sf(), sf::Color(255, 255, 255, 70)) };
        window.draw(line, 2, sf::Lines);
        sf::CircleShape marker(5.0f);
        marker.setOrigin(5.0f, 5.0f);
        marker.setPosition(target.sf());
        marker.setFillColor(sf::Color::Transparent);
        marker.setOutlineColor(sf::Color(255, 255, 255, 130));
        marker.setOutlineThickness(1.0f);
        window.draw(marker);
    }
}

void Renderer::drawLevelNumbers(sf::RenderWindow& window, const World& world) {
    if (!digitAtlasReady_) return;
    levelQuads_.clear();

    // 先画一层 1px 黑色阴影，再画白色数字：亮色小行星/激光上也能看清。
    for (const auto& s : world.ships()) {
        if (!s.active) continue;
        if (s.pos.x < -40.0f || s.pos.x > worldWidth_ + 40.0f ||
            s.pos.y < -40.0f || s.pos.y > worldHeight_ + 40.0f) {
            continue;
        }
        addDigitQuads(levelQuads_, s.level, s.pos.x + 1.0f, s.pos.y - 21.0f, 1.0f,
                      sf::Color(0, 0, 0, 200));
    }
    for (const auto& s : world.ships()) {
        if (!s.active) continue;
        if (s.pos.x < -40.0f || s.pos.x > worldWidth_ + 40.0f ||
            s.pos.y < -40.0f || s.pos.y > worldHeight_ + 40.0f) {
            continue;
        }
        addDigitQuads(levelQuads_, s.level, s.pos.x, s.pos.y - 22.0f, 1.0f,
                      sf::Color(255, 255, 255, 235));
    }
    if (levelQuads_.getVertexCount() == 0) return;
    sf::RenderStates states;
    states.texture = &digitTex_;
    window.draw(levelQuads_, states);
}

void Renderer::addDigitQuads(sf::VertexArray& out, int number, float cx, float cy, float scale,
                             sf::Color color) const {
    if (!digitAtlasReady_) return;
    char buf[16];
    const int len = std::snprintf(buf, sizeof(buf), "%d", std::max(0, number));
    const float w = digitCellW_ * scale;
    const float h = digitCellH_ * scale;
    const float total = static_cast<float>(len) * w;
    // 整像素对齐：数字纹理是最近邻采样，落在半像素上会丢笔画、随飞船移动闪烁。
    float x = std::floor(cx - total * 0.5f);
    const float y = std::floor(cy - h * 0.5f);
    for (int i = 0; i < len; ++i) {
        const int d = buf[i] - '0';
        const float u0 = static_cast<float>(d) * digitCellW_;
        const float u1 = u0 + digitCellW_;
        const float v0 = 0.0f;
        const float v1 = digitCellH_;
        out.append(sf::Vertex({x, y}, color, {u0, v0}));
        out.append(sf::Vertex({x + w, y}, color, {u1, v0}));
        out.append(sf::Vertex({x + w, y + h}, color, {u1, v1}));
        out.append(sf::Vertex({x, y + h}, color, {u0, v1}));
        x += w;
    }
}

void Renderer::drawUI(sf::RenderWindow& window, const World& world, OverlayState& overlay) {
    if (overlay.showUI) {
        drawFactionPanel(window, world, 0, 16.0f, 16.0f, 200.0f, 92.0f);
        drawFactionPanel(window, world, 1, WINDOW_WIDTH - 216.0f, 16.0f, 200.0f, 92.0f);
        drawFactionPanel(window, world, 2, 16.0f, WINDOW_HEIGHT - 108.0f, 200.0f, 92.0f);
        drawFactionPanel(window, world, 3, WINDOW_WIDTH - 216.0f, WINDOW_HEIGHT - 108.0f, 200.0f, 92.0f);
    }

    if (world.selected()) drawSelectedInfo(window, world);
    if (overlay.showUI) drawStatus(window, world, overlay);
    else {
        text(window, "Esc 菜单   M 统计面板   N 创造面板   K 观察面板   B 沙盒面板", 16.0f,
             WINDOW_HEIGHT - 24.0f, 12, sf::Color(255, 255, 255, 90));
    }
    if (overlay.showDebug) drawDebug(window, world, overlay);

    if (overlay.paused) {
        textCentered(window, "暂停", WINDOW_WIDTH * 0.5f, WINDOW_HEIGHT * 0.42f, 46,
                     sf::Color(255, 255, 255, 220));
        textCentered(window, "空格继续", WINDOW_WIDTH * 0.5f, WINDOW_HEIGHT * 0.42f + 56.0f, 16,
                     sf::Color(255, 255, 255, 160));
    }

    // 面板：沙盒 < 观察 < 创造 < 统计（后画的在上层）。
    drawSandbox(window, world, overlay);
    drawObservation(window, world, overlay);
    drawCreation(window, world);
    drawMonitor(window, world);
}

void Renderer::drawFactionPanel(sf::RenderWindow& window, const World& world, int faction,
                                float x, float y, float w, float h) {
    const sf::Color c = factionColor(faction);
    sf::RectangleShape bg({w, h});
    bg.setPosition(x, y);
    bg.setFillColor(sf::Color(0, 0, 0, 150));
    bg.setOutlineColor(withAlpha(c, 180));
    bg.setOutlineThickness(1.2f);
    window.draw(bg);

    sf::CircleShape dot(7.0f);
    dot.setOrigin(7.0f, 7.0f);
    dot.setPosition(x + 16.0f, y + 18.0f);
    dot.setFillColor(c);
    window.draw(dot);

    const auto& row = world.statsSnapshot().factions[static_cast<size_t>(faction)];
    std::string name = world.factionDisplayName(faction);
    if (!row.alive) name += " · 已淘汰";
    const unsigned nameSize = textBounds(name, 16).width > 170.0f ? 12u : 16u;
    text(window, name, x + 30.0f, y + 7.0f, nameSize, sf::Color::White);

    const auto val = [&](FactionMetric m) {
        return factionMetricValue(row, m, StatRange::Total);
    };
    const std::string ships = "舰船 " + std::to_string(static_cast<int>(val(FactionMetric::ShipCount))) +
                              "   战力 " + formatCompactNumber(val(FactionMetric::Power), 0);
    const std::string res = "资源 " + std::to_string(static_cast<int>(val(FactionMetric::Resources))) +
                            "   收集 " + formatCompactNumber(val(FactionMetric::Collected), 0);
    const std::string kd = "击杀 " + formatCompactNumber(val(FactionMetric::Kills), 0) +
                           "   损失 " + formatCompactNumber(val(FactionMetric::Losses), 0) +
                           "   输出 " + formatCompactNumber(val(FactionMetric::Output), 0);
    text(window, ships, x + 30.0f, y + 30.0f, 13, sf::Color(225, 225, 225));
    text(window, res, x + 30.0f, y + 48.0f, 13, sf::Color(225, 225, 225));
    text(window, kd, x + 30.0f, y + 68.0f, 12, withAlpha(c, 220));
}

void Renderer::drawSelectedInfo(sf::RenderWindow& window, const World& world) {
    const Ship* s = world.selected();
    if (!s) return;
    const int index = world.selectedIndex();
    const std::string info = world.shipInfo(index);
    if (info.empty()) return;

    const sf::FloatRect b = textBounds(info, 14);
    const float w = 300.0f;
    const float h = b.height + 28.0f;
    const float x = 16.0f;
    const float y = 124.0f;

    sf::RectangleShape bg({w, h});
    bg.setPosition(x, y);
    bg.setFillColor(sf::Color(0, 0, 0, 190));
    bg.setOutlineColor(withAlpha(factionColor(s->faction), 200));
    bg.setOutlineThickness(1.2f);
    window.draw(bg);

    sf::CircleShape dot(7.0f);
    dot.setOrigin(7.0f, 7.0f);
    dot.setPosition(x + 16.0f, y + 16.0f);
    dot.setFillColor(factionColor(s->faction));
    window.draw(dot);

    text(window, info, x + 14.0f, y + 12.0f, 14, sf::Color(240, 240, 240));
}

void Renderer::drawStatus(sf::RenderWindow& window, const World& world, const OverlayState& overlay) {
    const int vm = std::clamp(overlay.visionMode, 0, 5);
    std::ostringstream ss;
    ss << "视野 " << visionModeName(vm)
       << "   |   等级 " << (overlay.showLevel ? "开" : "关")
       << "   |   叛乱 " << (world.rebellionEnabled ? "开" : "关")
       << "   |   友军碰撞 " << (world.friendlyCollisionEnabled ? "开" : "关")
       << "   |   友军伤害 " << (world.friendlyFireEnabled ? "开" : "关")
       << "   |   统计 " << (monitor_.open ? "开" : "关")
       << "(" << monitor_.children.size() << "窗)"
       << "   |   创造 " << (creation_.open ? "开" : "关")
       << "   |   观察 " << (observation_.open ? "开" : "关")
       << "   |   速度 " << overlay.speed << "x"
       << "   |   " << (overlay.paused ? "暂停" : "运行");

    const std::string line = ss.str();
    const float w = 1240.0f;
    const float h = 26.0f;
    const float x = (WINDOW_WIDTH - w) * 0.5f;
    const float y = WINDOW_HEIGHT - 32.0f;
    sf::RectangleShape bg({w, h});
    bg.setPosition(x, y);
    bg.setFillColor(sf::Color(0, 0, 0, 170));
    bg.setOutlineColor(sf::Color(255, 255, 255, 60));
    bg.setOutlineThickness(1.0f);
    window.draw(bg);
    textCentered(window, line, WINDOW_WIDTH * 0.5f, y + 4.0f, 13, sf::Color(230, 230, 230));

    textCentered(window,
                 "Esc 菜单   Tab 视野   L 等级   O 叛乱   C 友军碰撞   F 友军伤害   M 统计   N 创造   K 观察   B 沙盒   F11 全屏   V 地图缩放   空格 暂停   1-4 速度   F2 选中   F3 调试   F5 混战   左键选船   R 重开",
                 WINDOW_WIDTH * 0.5f, WINDOW_HEIGHT - 54.0f, 11, sf::Color(255, 255, 255, 120));
}

void Renderer::drawDebug(sf::RenderWindow& window, const World& world, const OverlayState& overlay) {
    std::ostringstream ss;
    ss << "FPS " << fmt1(overlay.fps) << "   模拟 " << fmt1(overlay.simMs) << " ms/步"
       << "   速度 " << overlay.speed << "x" << (overlay.paused ? "   暂停" : "") << "\n";
    ss << "飞船 " << world.activeShipCount << "   小行星 " << world.activeAsteroidCount
       << "   光点 " << world.activeLightCount << "   光束 " << world.activeBeamCount
       << "   粒子 " << world.activeTrailCount << "   区域 " << world.activeZoneCount
       << "   传送门 " << world.activePortalCount
       << "   停滞 " << world.activeStoppedCount
       << "   友军碰撞 " << (world.friendlyCollisionEnabled ? "开" : "关")
       << "   友军伤害 " << (world.friendlyFireEnabled ? "开" : "关") << "\n";
    ss << "红 " << world.shipCount(0) << "  蓝 " << world.shipCount(1)
       << "  绿 " << world.shipCount(2) << "  黄 " << world.shipCount(3)
       << "   种子 " << world.seed << "   步 " << world.stepCount
       << "   战场 " << static_cast<int>(world.worldWidth()) << "x"
       << static_cast<int>(world.worldHeight())
       << "   地图 " << static_cast<int>(overlay.mapRect.width * 100.0f / WINDOW_WIDTH) << "%"
       << "   显示 " << (overlay.windowMode == 0 ? "窗口"
                                                : (overlay.windowMode == 1 ? "全屏" : "无边框"));

    const std::string s = ss.str();
    const sf::FloatRect b = textBounds(s, 12);
    const float w = std::max(420.0f, b.width + 24.0f);
    const float h = b.height + 20.0f;
    const float x = (WINDOW_WIDTH - w) * 0.5f;
    const float y = 8.0f;

    sf::RectangleShape bg({w, h});
    bg.setPosition(x, y);
    bg.setFillColor(sf::Color(0, 0, 0, 185));
    bg.setOutlineColor(sf::Color(0, 255, 180, 100));
    bg.setOutlineThickness(1.0f);
    window.draw(bg);
    text(window, s, x + 12.0f, y + 8.0f, 12, sf::Color(180, 255, 225));
}

// ============================================================
// 统计面板 / 排行窗口
// ============================================================

void Renderer::monitorToggle() {
    monitor_.open = !monitor_.open;
    monitor_.dragging = false;
    if (monitor_.open) monitor_.uiClock.restart();
}

int Renderer::metricCount(bool individual) const {
    return individual ? static_cast<int>(IndividualMetric::Count)
                      : static_cast<int>(FactionMetric::Count);
}

const char* Renderer::metricName(bool individual, int index) const {
    if (individual) {
        return individualMetricInfo(static_cast<IndividualMetric>(index)).name;
    }
    return factionMetricInfo(static_cast<FactionMetric>(index)).name;
}

bool Renderer::metricCumulative(bool individual, int index) const {
    if (individual) {
        return individualMetricInfo(static_cast<IndividualMetric>(index)).cumulative;
    }
    return factionMetricInfo(static_cast<FactionMetric>(index)).cumulative;
}

int Renderer::selectedMetric() const {
    return monitor_.individual ? monitor_.individualMetric : monitor_.factionMetric;
}

void Renderer::clampRangeForMetric() {
    const bool cumulative = metricCumulative(monitor_.individual, selectedMetric());
    if (!statRangeSupported(monitor_.range, monitor_.individual, cumulative)) {
        monitor_.range = StatRange::Total;
    }
}

MonitorRect Renderer::monitorPanelRect() const {
    return panelLayout(monitor_).rect;
}

MonitorRect Renderer::monitorChildRect(const MonitorChild& child) const {
    return childLayout(child).rect;
}

float Renderer::monitorChildWidth(const MonitorChild& child) const {
    return childPixelWidth(child);
}

float Renderer::monitorChildHeight(const MonitorChild& child) const {
    return childPixelHeight(child);
}

float Renderer::monitorRowHeight(const MonitorChild& child) const {
    return childRowHeight(child);
}

void Renderer::clampMonitorChild(MonitorChild& child) const {
    child.x = clampf(child.x, 0.0f, std::max(0.0f, WINDOW_WIDTH - childPixelWidth(child)));
    child.y = clampf(child.y, 0.0f, std::max(0.0f, WINDOW_HEIGHT - childPixelHeight(child)));
}

bool Renderer::monitorHandleMousePress(float x, float y) {
    if (monitor_.open) {
        const PanelLayout l = panelLayout(monitor_);
        if (l.rect.contains(x, y)) {
            if (l.close.contains(x, y)) {
                monitor_.open = false;
                return true;
            }
            if (l.title.contains(x, y)) {
                monitor_.dragging = true;
                monitor_.dragDX = x - monitor_.x;
                monitor_.dragDY = y - monitor_.y;
                return true;
            }
            if (l.scope[0].contains(x, y)) {
                monitor_.individual = false;
                monitor_.range = StatRange::Total;
                monitor_.metricScroll = 0.0f;
                return true;
            }
            if (l.scope[1].contains(x, y)) {
                monitor_.individual = true;
                monitor_.range = StatRange::Total;
                monitor_.metricScroll = 0.0f;
                return true;
            }
            for (int i = 0; i < 5; ++i) {
                if (l.range[i].contains(x, y)) {
                    const StatRange r = static_cast<StatRange>(i);
                    if (statRangeSupported(r, monitor_.individual,
                                           metricCumulative(monitor_.individual, selectedMetric()))) {
                        monitor_.range = r;
                    }
                    return true;
                }
            }
            if (l.eliminatedToggle.contains(x, y)) {
                monitor_.showEliminated = !monitor_.showEliminated;
                return true;
            }
            if (monitor_.individual) {
                for (int i = 0; i < 5; ++i) {
                    if (l.filterFaction[i].contains(x, y)) {
                        monitor_.filter.faction = i - 1;
                        return true;
                    }
                }
                if (l.titlesOnly.contains(x, y)) {
                    monitor_.filter.titlesOnly = !monitor_.filter.titlesOnly;
                    return true;
                }
                const int step = sf::Keyboard::isKeyPressed(sf::Keyboard::LShift) ? 5 : 1;
                if (l.levelMinus.contains(x, y)) {
                    monitor_.filter.minLevel = std::max(1, monitor_.filter.minLevel - step);
                    return true;
                }
                if (l.levelPlus.contains(x, y)) {
                    monitor_.filter.minLevel = std::min(99, monitor_.filter.minLevel + step);
                    return true;
                }
            }
            if (l.metricList.contains(x, y)) {
                const int count = metricCount(monitor_.individual);
                const int rows = (count + 1) / 2;
                const float rowH = 28.0f;
                const float colW = 276.0f;
                const float gap = 16.0f;
                const float maxScroll = std::max(0.0f, static_cast<float>(rows) * rowH - l.metricList.h);
                monitor_.metricScroll = clampf(monitor_.metricScroll, 0.0f, maxScroll);
                for (int i = 0; i < count; ++i) {
                    const int row = i / 2;
                    const int col = i % 2;
                    MonitorRect b{ l.metricList.x + static_cast<float>(col) * (colW + gap),
                                   l.metricList.y + static_cast<float>(row) * rowH - monitor_.metricScroll,
                                   colW, 25.0f };
                    if (b.y + b.h < l.metricList.y || b.y > l.metricList.y + l.metricList.h) continue;
                    if (b.contains(x, y)) {
                        if (monitor_.individual) monitor_.individualMetric = i;
                        else monitor_.factionMetric = i;
                        clampRangeForMetric();
                        return true;
                    }
                }
                return true;
            }
            if (l.open.contains(x, y)) {
                MonitorChild child;
                child.individual = monitor_.individual;
                child.factionMetric = static_cast<FactionMetric>(monitor_.factionMetric);
                child.individualMetric = static_cast<IndividualMetric>(monitor_.individualMetric);
                child.range = monitor_.range;
                child.filter = monitor_.filter;
                child.showEliminated = monitor_.showEliminated;
                const int n = static_cast<int>(monitor_.children.size());
                child.x = 668.0f + static_cast<float>(n / 3) * 26.0f;
                child.y = 60.0f + static_cast<float>(n % 3) * 270.0f;
                clampMonitorChild(child);
                monitor_.children.push_back(std::move(child));
                monitor_.activeChild = static_cast<int>(monitor_.children.size()) - 1;
                return true;
            }
            if (l.closeAll.contains(x, y)) {
                monitor_.children.clear();
                monitor_.activeChild = -1;
                return true;
            }
            return true;
        }
    }

    // 排行窗口（后画的在上层，所以从后往前命中）
    for (int i = static_cast<int>(monitor_.children.size()) - 1; i >= 0; --i) {
        if (monitor_.children[static_cast<size_t>(i)].detached) continue;
        const ChildLayout l = childLayout(monitor_.children[static_cast<size_t>(i)]);
        if (!l.rect.contains(x, y)) continue;

        if (l.close.contains(x, y)) {
            monitor_.children.erase(monitor_.children.begin() + i);
            monitor_.activeChild = -1;
            return true;
        }

        const bool hitTitle = l.title.contains(x, y);
        const bool hitBarMinus = l.barMinus.contains(x, y);
        const bool hitBarPlus = l.barPlus.contains(x, y);
        const bool hitWidthMinus = l.widthMinus.contains(x, y);
        const bool hitWidthPlus = l.widthPlus.contains(x, y);
        const bool hitFormat = l.formatBtn.contains(x, y);
        const bool hitPop = l.popBtn.contains(x, y);

        MonitorChild c = std::move(monitor_.children[static_cast<size_t>(i)]);
        monitor_.children.erase(monitor_.children.begin() + i);

        if (hitBarMinus) c.barCount = std::max(kMinBars, c.barCount - 1);
        if (hitBarPlus) c.barCount = std::min(kMaxBars, c.barCount + 1);
        if (hitWidthMinus) c.rowWidth = clampf(c.rowWidth - 40.0f, 260.0f, 700.0f);
        if (hitWidthPlus) c.rowWidth = clampf(c.rowWidth + 40.0f, 260.0f, 700.0f);
        if (hitFormat) c.format = (c.format == 1) ? 2 : 1;
        if (hitPop) popOutChild(c);

        if (hitBarMinus || hitBarPlus || hitWidthMinus || hitWidthPlus || hitFormat) {
            clampMonitorChild(c);
        } else if (hitTitle) {
            c.dragging = true;
            c.dragDX = x - c.x;
            c.dragDY = y - c.y;
        }
        monitor_.children.push_back(std::move(c));
        monitor_.activeChild = static_cast<int>(monitor_.children.size()) - 1;
        return true;
    }
    return false;
}

bool Renderer::monitorHandleMouseRelease(float, float) {
    bool wasDragging = monitor_.dragging;
    monitor_.dragging = false;
    for (auto& c : monitor_.children) {
        if (c.dragging) wasDragging = true;
        c.dragging = false;
    }
    return wasDragging;
}

bool Renderer::monitorHandleMouseMove(float x, float y) {
    bool dragging = false;
    if (monitor_.dragging) {
        monitor_.x = clampf(x - monitor_.dragDX, 0.0f, WINDOW_WIDTH - kPanelW);
        monitor_.y = clampf(y - monitor_.dragDY, 0.0f, WINDOW_HEIGHT - kPanelH);
        dragging = true;
    }
    for (auto& c : monitor_.children) {
        if (!c.dragging) continue;
        c.x = x - c.dragDX;
        c.y = y - c.dragDY;
        clampMonitorChild(c);
        dragging = true;
    }
    return dragging;
}

bool Renderer::monitorHandleWheel(float x, float y, float delta) {
    if (!monitor_.open) return false;
    const PanelLayout l = panelLayout(monitor_);
    if (!l.metricList.contains(x, y)) return false;
    const int count = metricCount(monitor_.individual);
    const int rows = (count + 1) / 2;
    const float maxScroll = std::max(0.0f, static_cast<float>(rows) * 28.0f - l.metricList.h);
    monitor_.metricScroll = clampf(monitor_.metricScroll - delta * 36.0f, 0.0f, maxScroll);
    return true;
}

void Renderer::updateMonitorChild(MonitorChild& child, const World& world) {
    struct Cand {
        uint64_t key = 0;
        int faction = 0;
        uint64_t factionId = 0;
        uint32_t serial = 0;
        int level = 1;
        std::string title;
        double value = 0.0;
    };
    const int maxBars = std::clamp(child.barCount, kMinBars, kMaxBars);
    std::array<Cand, 16> cand{};
    int cn = 0;

    auto consider = [&](uint64_t key, int faction, uint64_t factionId, uint32_t serial,
                        int level, const std::string& title, double value) {
        if (cn < maxBars) {
            Cand& c = cand[static_cast<size_t>(cn++)];
            c.key = key;
            c.faction = faction;
            c.factionId = factionId;
            c.serial = serial;
            c.level = level;
            c.title = title;
            c.value = value;
        } else {
            int worst = 0;
            for (int k = 1; k < cn; ++k) {
                if (cand[static_cast<size_t>(k)].value < cand[static_cast<size_t>(worst)].value) {
                    worst = k;
                }
            }
            if (value > cand[static_cast<size_t>(worst)].value) {
                Cand& c = cand[static_cast<size_t>(worst)];
                c.key = key;
                c.faction = faction;
                c.factionId = factionId;
                c.serial = serial;
                c.level = level;
                c.title = title;
                c.value = value;
            }
        }
    };

    if (child.individual) {
        const auto& rows = world.topIndividuals(child.individualMetric, child.range,
                                                child.filter, maxBars, child.showEliminated);
        for (const IndividualRank& r : rows) {
            consider(r.key, r.faction, r.factionId, r.serial, r.level, r.title, r.value);
        }
    } else {
        const StatsSnapshot& snap = world.statsSnapshot();
        for (int f = 0; f < MAX_FACTIONS; ++f) {
            const FactionStatRow& row = snap.factions[static_cast<size_t>(f)];
            if (!row.alive) continue;
            consider(0x8000000000000000ull | row.id, row.colorSlot, row.id, 0, 0,
                     row.name, factionMetricValue(row, child.factionMetric, child.range));
        }
        if (child.showEliminated) {
            for (const FactionStatRow& row : snap.eliminatedFactions) {
                consider(0x4000000000000000ull | row.id, row.colorSlot, row.id, 0, 0,
                         row.name, factionMetricValue(row, child.factionMetric, child.range));
            }
        }
        std::sort(cand.begin(), cand.begin() + cn, [](const Cand& a, const Cand& b) {
            if (a.value != b.value) return a.value > b.value;
            if (a.faction != b.faction) return a.faction < b.faction;
            return a.factionId < b.factionId;
        });
    }

    std::array<MonitorBarAnim, 16> next{};
    double maxValue = 0.0;
    const float rowH = monitorRowHeight(child);
    for (int i = 0; i < cn; ++i) {
        const Cand& c = cand[static_cast<size_t>(i)];
        int oldIndex = -1;
        for (int j = 0; j < 16; ++j) {
            if (child.bars[static_cast<size_t>(j)].valid &&
                child.bars[static_cast<size_t>(j)].key == c.key) {
                oldIndex = j;
                break;
            }
        }
        MonitorBarAnim b = oldIndex >= 0 ? child.bars[static_cast<size_t>(oldIndex)] : MonitorBarAnim{};
        const double oldValue = b.value;
        b.key = c.key;
        b.faction = c.faction;
        b.factionId = c.factionId;
        b.serial = c.serial;
        b.level = c.level;
        b.title = c.title;
        b.value = c.value;
        b.targetY = static_cast<float>(i) * rowH;
        b.valid = true;
        if (oldIndex < 0) {
            b.y = b.targetY;
            b.shown = 0.0;
            b.width = 0.0f;
            b.pop = 1.0f;
        } else if (oldIndex != i || (c.value - oldValue) > std::max(1.0, oldValue * 0.05)) {
            b.pop = 1.0f;
        }
        maxValue = std::max(maxValue, b.value);
        next[static_cast<size_t>(i)] = std::move(b);
    }

    for (int i = 0; i < cn; ++i) {
        MonitorBarAnim& b = next[static_cast<size_t>(i)];
        if (maxValue > 0.0) {
            const double ratio = std::clamp(b.value / maxValue, 0.0, 1.0);
            b.targetWidth = ratio > 0.0 ? static_cast<float>(ratio) * (child.rowWidth - 6.0f) : 0.0f;
            if (b.value > 0.0) b.targetWidth = std::max(4.0f, b.targetWidth);
        } else {
            b.targetWidth = 0.0f;
        }
    }

    const float k = 1.0f - std::exp(-monitorDt_ * 10.0f);
    for (int i = 0; i < cn; ++i) {
        MonitorBarAnim& b = next[static_cast<size_t>(i)];
        b.shown += (b.value - b.shown) * static_cast<double>(k);
        b.y += (b.targetY - b.y) * k;
        b.width += (b.targetWidth - b.width) * k;
        b.pop = std::max(0.0f, b.pop - monitorDt_ * 3.0f);
    }

    child.bars = next;
}

void Renderer::drawMonitor(sf::RenderWindow& window, const World& world) {
    monitorDt_ = clampf(monitor_.uiClock.restart().asSeconds(), 0.0f, 0.1f);
    if (monitor_.children.empty() && !monitor_.open) return;

    for (auto& c : monitor_.children) {
        if (!c.detached) updateMonitorChild(c, world);
    }
    for (auto& c : monitor_.children) {
        if (!c.detached) drawMonitorChild(window, world, c);
    }
    if (monitor_.open) drawMonitorPanel(window, world);
}

void Renderer::popOutChild(MonitorChild& child) {
    if (child.detachedWindow) {
        // 收回游戏窗口
        child.detachedWindow->close();
        child.detachedWindow.reset();
        child.detachedRenderer.reset();
        child.detached = false;
        child.x = child.dockX;
        child.y = child.dockY;
        clampMonitorChild(child);
        return;
    }

    const unsigned w = static_cast<unsigned>(std::max(200.0f, childPixelWidth(child)));
    const unsigned h = static_cast<unsigned>(std::max(160.0f, childPixelHeight(child)));
    auto win = std::make_shared<sf::RenderWindow>();
    win->create(sf::VideoMode(w, h), L"暗域光战 · 排行窗口",
                sf::Style::Titlebar | sf::Style::Close);
    win->setVerticalSyncEnabled(false);
    win->setFramerateLimit(60);

    auto renderer = std::make_shared<Renderer>();
    if (!renderer->init(true)) {
        win->close();
        return;
    }

    child.dockX = child.x;
    child.dockY = child.y;
    child.x = 0.0f;
    child.y = 0.0f;
    child.detachedWindow = win;
    child.detachedRenderer = renderer;
    child.detached = true;
    int offset = 0;
    for (const auto& other : monitor_.children) {
        if (other.detachedWindow) ++offset;
    }
    win->setPosition(sf::Vector2i(900 + (offset % 6) * 28, 120 + (offset % 6) * 28));
    win->requestFocus();
    makeTopmost(*win);
}

void Renderer::syncDetachedWindow(MonitorChild& child) {
    if (!child.detachedWindow) return;
    const unsigned w = static_cast<unsigned>(std::max(200.0f, childPixelWidth(child)));
    const unsigned h = static_cast<unsigned>(std::max(160.0f, childPixelHeight(child)));
    child.detachedWindow->setSize({ w, h });
    child.detachedWindow->setView(sf::View(sf::FloatRect(0.0f, 0.0f,
                                                         static_cast<float>(w),
                                                         static_cast<float>(h))));
}

void Renderer::pollDetachedWindows(World& world) {
    std::vector<size_t> remove;
    for (size_t i = 0; i < monitor_.children.size(); ++i) {
        MonitorChild& c = monitor_.children[i];
        if (!c.detachedWindow) continue;
        bool closeWindow = false;
        sf::Event event{};
        while (c.detachedWindow->pollEvent(event)) {
            if (event.type == sf::Event::Closed) {
                closeWindow = true;
                break;
            }
            if (event.type == sf::Event::KeyPressed && event.key.code == sf::Keyboard::Escape) {
                closeWindow = true;
                break;
            }
            if (event.type == sf::Event::MouseButtonPressed &&
                event.mouseButton.button == sf::Mouse::Left) {
                const float x = static_cast<float>(event.mouseButton.x);
                const float y = static_cast<float>(event.mouseButton.y);
                const ChildLayout l = childLayout(c);
                if (l.close.contains(x, y)) {
                    closeWindow = true;
                    break;
                }
                if (l.popBtn.contains(x, y)) {
                    popOutChild(c);
                    break;
                }
                bool changed = false;
                if (l.barMinus.contains(x, y)) {
                    c.barCount = std::max(kMinBars, c.barCount - 1);
                    changed = true;
                } else if (l.barPlus.contains(x, y)) {
                    c.barCount = std::min(kMaxBars, c.barCount + 1);
                    changed = true;
                } else if (l.widthMinus.contains(x, y)) {
                    c.rowWidth = clampf(c.rowWidth - 40.0f, 260.0f, 700.0f);
                    changed = true;
                } else if (l.widthPlus.contains(x, y)) {
                    c.rowWidth = clampf(c.rowWidth + 40.0f, 260.0f, 700.0f);
                    changed = true;
                } else if (l.formatBtn.contains(x, y)) {
                    c.format = (c.format == 1) ? 2 : 1;
                    changed = true;
                }
                if (changed) {
                    syncDetachedWindow(c);
                }
            }
        }
        if (closeWindow) {
            c.detachedWindow->close();
            remove.push_back(i);
        }
    }
    for (auto it = remove.rbegin(); it != remove.rend(); ++it) {
        monitor_.children.erase(monitor_.children.begin() + static_cast<std::ptrdiff_t>(*it));
    }
    if (monitor_.activeChild >= static_cast<int>(monitor_.children.size())) {
        monitor_.activeChild = -1;
    }
    pollCreationDetachedWindow(world);
}

void Renderer::pollCreationDetachedWindow(World& world) {
    if (!creation_.detachedWindow) return;
    if (!creation_.open) {
        closeCreationPopout();
        return;
    }
    sf::RenderWindow& w = *creation_.detachedWindow;
    bool closePanel = false;
    sf::Event event{};
    while (w.pollEvent(event)) {
        if (event.type == sf::Event::Closed) {
            closePanel = true;
            break;
        }
        if (event.type == sf::Event::KeyPressed) {
            if (event.key.code == sf::Keyboard::Escape) {
                if (creation_.editField >= 0) {
                    creationHandleKey(event.key.code);
                } else if (creation_.placing) {
                    creationCancelPlacing();
                } else {
                    closePanel = true;
                }
            } else {
                creationHandleKey(event.key.code);
            }
        } else if (event.type == sf::Event::TextEntered) {
            creationHandleText(event.text.unicode);
        } else if (event.type == sf::Event::MouseButtonPressed &&
                   event.mouseButton.button == sf::Mouse::Left) {
            creationHandleMousePress(static_cast<float>(event.mouseButton.x),
                                     static_cast<float>(event.mouseButton.y), world, true);
        } else if (event.type == sf::Event::MouseButtonReleased &&
                   event.mouseButton.button == sf::Mouse::Left) {
            creationHandleMouseRelease(static_cast<float>(event.mouseButton.x),
                                       static_cast<float>(event.mouseButton.y));
        }
        if (!creation_.detached) break; // 弹出窗被“收回”时立刻停止访问
    }
    if (closePanel) {
        creation_.open = false;
        creationCancelPlacing();
        closeCreationPopout();
    }
}

void Renderer::drawDetachedChild(sf::RenderWindow& window, const World& world, MonitorChild& child) {
    monitorDt_ = clampf(monitor_.uiClock.restart().asSeconds(), 0.0f, 0.1f);
    updateMonitorChild(child, world);
    drawMonitorChild(window, world, child);
}

void Renderer::drawDetachedWindows(sf::RenderWindow&, const World& world) {
    for (auto& c : monitor_.children) {
        if (!c.detachedWindow || !c.detachedRenderer) continue;
        if (!c.detachedWindow->isOpen()) continue;
        sf::RenderWindow& w = *c.detachedWindow;
        w.clear(sf::Color(8, 12, 18));
        c.detachedRenderer->drawDetachedChild(w, world, c);
        w.display();
    }

    if (creation_.open && creation_.detachedWindow && creation_.detachedRenderer &&
        creation_.detachedWindow->isOpen()) {
        // 独立窗口有自己的 GL 上下文：只能用自己的 Renderer/字体。
        // 这里复制一份 UI 状态用于绘制，但清掉 shared_ptr，避免自引用循环。
        creation_.detachedRenderer->creation_ = creation_;
        creation_.detachedRenderer->creation_.detachedWindow.reset();
        creation_.detachedRenderer->creation_.detachedRenderer.reset();
        creation_.detachedRenderer->monitorDt_ = monitorDt_;
        sf::RenderWindow& w = *creation_.detachedWindow;
        w.clear(sf::Color(8, 14, 18));
        creation_.detachedRenderer->drawCreationPanel(w, world);
        w.display();
    }
}

void Renderer::drawMonitorPanel(sf::RenderWindow& window, const World& world) {
    const PanelLayout l = panelLayout(monitor_);
    const sf::Color accent(90, 185, 255);
    const sf::Color dim(150, 160, 175);

    sf::RectangleShape bg({l.rect.w, l.rect.h});
    bg.setPosition(l.rect.x, l.rect.y);
    bg.setFillColor(sf::Color(8, 12, 18, 248));
    bg.setOutlineColor(sf::Color(120, 190, 255, 170));
    bg.setOutlineThickness(1.4f);
    window.draw(bg);

    sf::RectangleShape titleBar({l.rect.w, 36.0f});
    titleBar.setPosition(l.rect.x, l.rect.y);
    titleBar.setFillColor(sf::Color(18, 34, 52, 245));
    window.draw(titleBar);

    text(window, "统计面板", l.rect.x + 16.0f, l.rect.y + 8.0f, 19, sf::Color::White);
    text(window, "统计排行 · 实时", l.rect.x + 116.0f, l.rect.y + 12.0f, 12, withAlpha(accent, 190));

    // 关闭按钮
    {
        sf::VertexArray x(sf::Lines, 4);
        x[0] = sf::Vertex({l.close.x + 7.0f, l.close.y + 6.0f}, sf::Color(230, 230, 230));
        x[1] = sf::Vertex({l.close.x + 21.0f, l.close.y + 20.0f}, sf::Color(230, 230, 230));
        x[2] = sf::Vertex({l.close.x + 21.0f, l.close.y + 6.0f}, sf::Color(230, 230, 230));
        x[3] = sf::Vertex({l.close.x + 7.0f, l.close.y + 20.0f}, sf::Color(230, 230, 230));
        window.draw(x);
    }

    auto button = [&](const MonitorRect& r, const std::string& label, bool selected,
                      bool enabled, sf::Color tint) {
        sf::RectangleShape b({r.w, r.h});
        b.setPosition(r.x, r.y);
        b.setFillColor(enabled ? (selected ? withAlpha(tint, 210) : sf::Color(26, 34, 44, 240))
                               : sf::Color(18, 22, 28, 220));
        b.setOutlineColor(enabled ? withAlpha(selected ? tint : sf::Color(90, 105, 125), selected ? 230 : 110)
                                  : sf::Color(55, 60, 70, 120));
        b.setOutlineThickness(selected ? 1.6f : 1.0f);
        window.draw(b);
        textCentered(window, label, r.x + r.w * 0.5f, r.y + (r.h - 13.0f) * 0.5f - 1.0f, 13,
                     enabled ? (selected ? sf::Color::White : sf::Color(215, 220, 230))
                             : sf::Color(95, 100, 110));
    };

    text(window, "范围", l.rect.x + 20.0f, l.rect.y + 48.0f, 13, dim);
    button(l.scope[0], "阵营排行", !monitor_.individual, true, accent);
    button(l.scope[1], "个体排行", monitor_.individual, true, accent);

    text(window, "时间", l.rect.x + 260.0f, l.rect.y + 48.0f, 13, dim);
    text(window, statRangeHint(monitor_.range), l.rect.x + 306.0f, l.rect.y + 49.0f, 11,
         withAlpha(dim, 190));

    const bool cumulative = metricCumulative(monitor_.individual, selectedMetric());
    for (int i = 0; i < 5; ++i) {
        const StatRange r = static_cast<StatRange>(i);
        const bool enabled = statRangeSupported(r, monitor_.individual, cumulative);
        button(l.range[i], statRangeName(r), monitor_.range == r, enabled, accent);
    }
    button(l.eliminatedToggle, "显示淘汰者", monitor_.showEliminated, true,
           sf::Color(190, 120, 60));

    text(window, "筛选", l.rect.x + 20.0f, l.rect.y + 172.0f, 13, dim);
    if (monitor_.individual) {
        static const char* factionLabels[5] = { "全部", "红", "蓝", "绿", "黄" };
        for (int i = 0; i < 5; ++i) {
            const bool selected = monitor_.filter.faction == i - 1;
            button(l.filterFaction[i], factionLabels[i], selected, true,
                   i == 0 ? accent : factionColor(i - 1));
        }
        button(l.titlesOnly, "仅称号", monitor_.filter.titlesOnly, true, accent);
        text(window, "等级≥", l.levelLabel.x, l.levelLabel.y + 8.0f, 12, dim);
        button(l.levelMinus, "-", false, true, accent);
        textCentered(window, std::to_string(monitor_.filter.minLevel),
                     l.levelValue.x + l.levelValue.w * 0.5f, l.levelValue.y + 8.0f, 13,
                     sf::Color::White);
        button(l.levelPlus, "+", false, true, accent);
    } else {
        text(window, "阵营排行固定显示 4 个阵营；个体排行可筛选阵营/等级/称号。",
             l.rect.x + 20.0f, l.rect.y + 200.0f, 12, withAlpha(dim, 210));
    }

    text(window, monitor_.individual ? "个体指标" : "阵营指标", l.rect.x + 20.0f, l.rect.y + 238.0f, 13, dim);
    text(window, "点击选中后打开独立排行窗口", l.rect.x + 100.0f, l.rect.y + 238.0f, 11,
         withAlpha(dim, 190));

    // 指标列表
    const int count = metricCount(monitor_.individual);
    const int rows = (count + 1) / 2;
    constexpr float rowH = 28.0f;
    constexpr float colW = 276.0f;
    constexpr float gap = 16.0f;
    const float maxScroll = std::max(0.0f, static_cast<float>(rows) * rowH - l.metricList.h);
    monitor_.metricScroll = clampf(monitor_.metricScroll, 0.0f, maxScroll);

    for (int i = 0; i < count; ++i) {
        const int row = i / 2;
        const int col = i % 2;
        MonitorRect b{ l.metricList.x + static_cast<float>(col) * (colW + gap),
                       l.metricList.y + static_cast<float>(row) * rowH - monitor_.metricScroll,
                       colW, 25.0f };
        if (b.y + b.h < l.metricList.y || b.y > l.metricList.y + l.metricList.h) continue;

        const bool selected = (i == selectedMetric());
        sf::RectangleShape mb({b.w, b.h});
        mb.setPosition(b.x, b.y);
        mb.setFillColor(selected ? sf::Color(36, 78, 112, 235) : sf::Color(20, 26, 34, 225));
        mb.setOutlineColor(selected ? withAlpha(accent, 230) : sf::Color(70, 82, 98, 120));
        mb.setOutlineThickness(selected ? 1.4f : 1.0f);
        window.draw(mb);
        text(window, metricName(monitor_.individual, i), b.x + 8.0f, b.y + 5.0f, 12,
             selected ? sf::Color::White : sf::Color(205, 212, 225));
    }

    // 滚动条（内容超过列表时）
    if (maxScroll > 0.0f) {
        const float trackH = l.metricList.h;
        const float thumbH = std::max(24.0f, trackH * trackH / (trackH + maxScroll));
        const float thumbY = l.metricList.y + (trackH - thumbH) * (monitor_.metricScroll / maxScroll);
        sf::RectangleShape track({4.0f, trackH});
        track.setPosition(l.metricList.x + l.metricList.w - 2.0f, l.metricList.y);
        track.setFillColor(sf::Color(255, 255, 255, 25));
        window.draw(track);
        sf::RectangleShape thumb({4.0f, thumbH});
        thumb.setPosition(l.metricList.x + l.metricList.w - 2.0f, thumbY);
        thumb.setFillColor(withAlpha(accent, 170));
        window.draw(thumb);
    }

    button(l.open, "+ 打开排行窗口", true, true, sf::Color(36, 150, 110));
    button(l.closeAll, "关闭全部子窗口 (" + std::to_string(monitor_.children.size()) + ")",
           false, true, sf::Color(190, 80, 80));

    text(window, "仅峰值：累计项=历史最佳1分钟；显示淘汰者：把已灭亡对象按淘汰时数据一起排。",
         l.rect.x + 20.0f, l.rect.y + kPanelH - 50.0f, 11, withAlpha(dim, 200));
    text(window, "Esc 关闭面板 · M/H 切换 · Shift 点 +/- 等级步进 5。",
         l.rect.x + 20.0f, l.rect.y + kPanelH - 32.0f, 11, withAlpha(dim, 160));
}

void Renderer::drawMonitorChild(sf::RenderWindow& window, const World& world, MonitorChild& child) {
    const ChildLayout l = childLayout(child);
    const sf::Color accent(90, 185, 255);

    sf::RectangleShape bg({l.rect.w, l.rect.h});
    bg.setPosition(l.rect.x, l.rect.y);
    bg.setFillColor(sf::Color(8, 12, 18, 250));
    bg.setOutlineColor(sf::Color(120, 190, 255, 130));
    bg.setOutlineThickness(1.2f);
    window.draw(bg);

    // 标题栏
    sf::RectangleShape titleBar({l.rect.w, kChildTitleH});
    titleBar.setPosition(l.rect.x, l.rect.y);
    titleBar.setFillColor(sf::Color(18, 34, 52, 245));
    window.draw(titleBar);

    const char* metric = child.individual
                             ? individualMetricInfo(child.individualMetric).name
                             : factionMetricInfo(child.factionMetric).name;
    std::string title = std::string(metric) + "  ·  " + statRangeName(child.range);
    if (child.showEliminated) title += "  ·  含淘汰者";
    if (child.individual && child.filter.faction >= 0) {
        title += "  ·  " + world.factionDisplayName(child.filter.faction);
    }
    if (child.individual && child.filter.titlesOnly) title += "  ·  仅称号";
    if (child.individual && child.filter.minLevel > 1) {
        title += "  ·  等级≥" + std::to_string(child.filter.minLevel);
    }
    const unsigned titleSize = textBounds(title, 14).width > (l.rect.w - 80.0f) ? 12u : 14u;
    text(window, title, l.rect.x + 12.0f, l.rect.y + 7.0f, titleSize, sf::Color::White);

    {
        sf::VertexArray x(sf::Lines, 4);
        x[0] = sf::Vertex({l.close.x + 7.0f, l.close.y + 6.0f}, sf::Color(230, 230, 230));
        x[1] = sf::Vertex({l.close.x + 19.0f, l.close.y + 18.0f}, sf::Color(230, 230, 230));
        x[2] = sf::Vertex({l.close.x + 19.0f, l.close.y + 6.0f}, sf::Color(230, 230, 230));
        x[3] = sf::Vertex({l.close.x + 7.0f, l.close.y + 18.0f}, sf::Color(230, 230, 230));
        window.draw(x);
    }

    // 控制条：行数 -/+、单行宽度 -/+、格式 1/2
    sf::RectangleShape controlBar({l.rect.w, kChildControlH});
    controlBar.setPosition(l.rect.x, l.rect.y + kChildTitleH);
    controlBar.setFillColor(sf::Color(14, 22, 32, 245));
    window.draw(controlBar);

    auto miniButton = [&](const MonitorRect& r, const std::string& label) {
        sf::RectangleShape b({r.w, r.h});
        b.setPosition(r.x, r.y);
        b.setFillColor(sf::Color(32, 44, 58, 245));
        b.setOutlineColor(sf::Color(90, 110, 130, 180));
        b.setOutlineThickness(1.0f);
        window.draw(b);
        textCentered(window, label, r.x + r.w * 0.5f, r.y + 3.0f, 12, sf::Color(225, 232, 242));
    };

    const float cy = l.rect.y + kChildTitleH + 4.0f;
    text(window, "行数", l.rect.x + 10.0f, cy + 4.0f, 10, sf::Color(160, 172, 188));
    miniButton(l.barMinus, "-");
    miniButton(l.barPlus, "+");
    textCentered(window, std::to_string(child.barCount), l.rect.x + 79.0f, cy + 4.0f, 12,
                 sf::Color::White);

    text(window, "行宽", l.rect.x + 120.0f, cy + 4.0f, 10, sf::Color(160, 172, 188));
    miniButton(l.widthMinus, "-");
    miniButton(l.widthPlus, "+");
    textCentered(window, std::to_string(static_cast<int>(child.rowWidth)), l.rect.x + 183.0f,
                 cy + 4.0f, 11, sf::Color::White);

    {
        sf::RectangleShape fb({l.formatBtn.w, l.formatBtn.h});
        fb.setPosition(l.formatBtn.x, l.formatBtn.y);
        fb.setFillColor(sf::Color(32, 44, 58, 245));
        fb.setOutlineColor(sf::Color(90, 110, 130, 180));
        fb.setOutlineThickness(1.0f);
        window.draw(fb);
        textCentered(window, "格式" + std::to_string(child.format),
                     l.formatBtn.x + l.formatBtn.w * 0.5f, l.formatBtn.y + 3.0f, 12,
                     sf::Color(225, 232, 242));
    }
    {
        sf::RectangleShape pb({l.popBtn.w, l.popBtn.h});
        pb.setPosition(l.popBtn.x, l.popBtn.y);
        pb.setFillColor(child.detached ? sf::Color(90, 60, 32, 245) : sf::Color(32, 58, 48, 245));
        pb.setOutlineColor(child.detached ? sf::Color(230, 160, 80, 200) : sf::Color(90, 190, 140, 190));
        pb.setOutlineThickness(1.0f);
        window.draw(pb);
        textCentered(window, child.detached ? "收回" : "弹出",
                     l.popBtn.x + l.popBtn.w * 0.5f, l.popBtn.y + 3.0f, 12,
                     sf::Color(235, 241, 248));
    }

    const MetricInfo* info = nullptr;
    if (child.individual) info = &individualMetricInfo(child.individualMetric);
    else info = &factionMetricInfo(child.factionMetric);

    const float barsTop = l.rect.y + kChildTitleH + kChildControlH;
    const float barX = l.rect.x + kChildLeft;
    const float barW = child.rowWidth;
    const float valueX = barX + barW + 8.0f;
    const float valueRight = valueX + kValueColW;
    const float rowH = monitorRowHeight(child);
    const float barH = child.format == 1 ? kBarH1 : kBarH2;

    // 数值列分隔线
    {
        sf::Vertex line[2] = { sf::Vertex({valueX - 4.0f, barsTop + 4.0f}, sf::Color(255, 255, 255, 35)),
                               sf::Vertex({valueX - 4.0f, l.rect.y + l.rect.h - 26.0f},
                                          sf::Color(255, 255, 255, 35)) };
        window.draw(line, 2, sf::Lines);
    }

    const int visibleBars = std::clamp(child.barCount, kMinBars, kMaxBars);
    int validBars = 0;
    for (int i = 0; i < visibleBars; ++i) {
        MonitorBarAnim& b = child.bars[static_cast<size_t>(i)];
        if (!b.valid) continue;
        ++validBars;

        const float rowTop = barsTop + b.y + 2.0f;
        const bool format1 = child.format == 1;
        const float barTop = format1 ? rowTop + 5.0f : rowTop + 24.0f;
        const float labelX = barX + 28.0f;

        sf::RectangleShape barBg({barW, barH});
        barBg.setPosition(barX, barTop);
        barBg.setFillColor(sf::Color(255, 255, 255, 14));
        barBg.setOutlineColor(sf::Color(255, 255, 255, 28));
        barBg.setOutlineThickness(1.0f);
        window.draw(barBg);

        const float drawW = b.value > 0.0
                                ? clampf(b.width + b.pop * 6.0f, 0.0f, barW)
                                : 0.0f;
        if (drawW > 0.5f) {
            sf::RectangleShape fill({drawW, barH});
            fill.setPosition(barX, barTop);
            fill.setFillColor(withAlpha(factionColor(b.faction), 190));
            window.draw(fill);
        }

        static const sf::Color rankColors[4] = {
            sf::Color(255, 215, 90), sf::Color(205, 215, 230),
            sf::Color(220, 150, 90), sf::Color(160, 165, 175)
        };
        text(window, std::to_string(i + 1), l.rect.x + 14.0f, rowTop + (format1 ? 11.0f : 2.0f),
             15, rankColors[std::min(i, 3)]);

        const float iconY = format1 ? rowTop + 22.0f : rowTop + 10.0f;
        const float labelY = format1 ? rowTop + 10.0f : rowTop + 1.0f;
        if (child.individual) {
            drawShipGlyph(window, b.faction, barX + 12.0f, iconY, 7.0f);
            const std::string serial = "#" + std::to_string(b.serial);
            text(window, serial, labelX, labelY, 14, sf::Color::White);
            float cursor = labelX + textBounds(serial, 14).width + 8.0f;
            const std::string lv = "Lv." + std::to_string(b.level);
            text(window, lv, cursor, labelY + 2.0f, 11, withAlpha(factionColor(b.faction), 230));
            cursor += textBounds(lv, 11).width + 10.0f;
            if (!b.title.empty()) {
                const float tw = textBounds(b.title, 11).width;
                if (cursor + tw < valueX - 8.0f) {
                    text(window, b.title, cursor, labelY + 2.0f, 11,
                         sf::Color(255, 226, 150));
                }
            }
        } else {
            drawFactionDot(window, b.faction, barX + 12.0f, iconY, 5.0f);
            std::string name = b.title;
            if (b.factionId != 0) name += " #" + std::to_string(b.factionId);
            text(window, name, labelX, labelY, 14, sf::Color::White);
        }

        const std::string value = formatMetricValue(b.shown, *info);
        const float vw = textBounds(value, 15).width;
        text(window, value, valueRight - vw, rowTop + (format1 ? 10.0f : 0.0f), 15,
             sf::Color::White);
    }

    if (validBars == 0) {
        textCentered(window, "暂无匹配数据", l.rect.x + l.rect.w * 0.5f,
                     barsTop + 70.0f, 15, sf::Color(190, 195, 205));
    }

    const float noteY = l.rect.y + l.rect.h - 18.0f;
    if (child.showEliminated) {
        const size_t count = child.individual
                                 ? world.statsSnapshot().deadIndividualCount
                                 : world.statsSnapshot().eliminatedFactions.size();
        const std::string note = std::string("含淘汰者 · 已淘汰 ") + std::to_string(count) +
                                 (child.individual ? " 个个体" : " 个阵营") +
                                 " · 前 " + std::to_string(visibleBars) + " 名";
        text(window, note, l.rect.x + 12.0f, noteY, 11, sf::Color(255, 210, 140, 210));
    } else if (!child.individual && child.range == StatRange::Peak) {
        const double overall = world.statsSnapshot().overallPeak[
            static_cast<size_t>(child.factionMetric)];
        const std::string note = "仅峰值 · 全服纪录 " + formatMetricValue(overall, *info);
        text(window, note, l.rect.x + 12.0f, noteY, 11, sf::Color(255, 226, 150, 210));
    } else {
        const std::string note = "实时刷新 · 前 " + std::to_string(visibleBars) +
                                 " 名 · 行数/行宽/格式可调";
        text(window, note, l.rect.x + 12.0f, noteY, 11, sf::Color(180, 190, 205, 180));
    }
}

// ============================================================
// 创造面板
// ============================================================

void Renderer::creationToggle() {
    if (creation_.open) {
        creation_.open = false;
        closeCreationPopout();
        creationCancelPlacing();
        return;
    }
    creation_.open = !creation_.open;
    creation_.dragging = false;
    if (creation_.open) {
        creation_.hint = "选择创造物与出现方式，然后点“随机创造”或“手动放置”";
        creation_.hintTimer = 4.0f;
    }
}

void Renderer::creationPopOut() {
    if (!creation_.open) return;
    if (creation_.detachedWindow) {
        closeCreationPopout();
        return;
    }

    auto win = std::make_shared<sf::RenderWindow>();
    win->create(sf::VideoMode(static_cast<unsigned>(kCreateW), static_cast<unsigned>(kCreateH)),
                L"暗域光战 · 创造面板", sf::Style::Titlebar | sf::Style::Close);
    win->setVerticalSyncEnabled(false);
    win->setFramerateLimit(60);

    auto renderer = std::make_shared<Renderer>();
    if (!renderer->init(true)) {
        win->close();
        return;
    }

    creation_.dockX = creation_.x;
    creation_.dockY = creation_.y;
    creation_.x = 0.0f;
    creation_.y = 0.0f;
    creation_.dragging = false;
    creation_.detachedWindow = win;
    creation_.detachedRenderer = renderer;
    creation_.detached = true;
    const sf::VideoMode desktop = sf::VideoMode::getDesktopMode();
    win->setPosition(sf::Vector2i(std::max(0, static_cast<int>(desktop.width) - 664), 80));
    win->requestFocus();
    makeTopmost(*win);
}

Vec2 Renderer::screenToWorld(float x, float y) const {
    const float w = std::max(1.0f, mapRect_.width);
    const float h = std::max(1.0f, mapRect_.height);
    return { (x - mapRect_.left) * worldWidth_ / w,
             (y - mapRect_.top) * worldHeight_ / h };
}

void Renderer::closeCreationPopout() {
    if (!creation_.detached) return;
    if (creation_.detachedWindow) creation_.detachedWindow->close();
    creation_.detachedWindow.reset();
    creation_.detachedRenderer.reset();
    creation_.detached = false;
    creation_.x = creation_.dockX;
    creation_.y = creation_.dockY;
    clampCreationPanel();
}

void Renderer::creationCancelPlacing() {
    if (!creation_.placing && creation_.placementStage == 0) return;
    creation_.placing = false;
    creation_.placementStage = 0;
    creation_.hint = "已取消放置";
    creation_.hintTimer = 2.0f;
}

void Renderer::clampCreationPanel() {
    creation_.x = clampf(creation_.x, 0.0f, std::max(0.0f, WINDOW_WIDTH - kCreateW));
    creation_.y = clampf(creation_.y, 0.0f, std::max(0.0f, WINDOW_HEIGHT - kCreateH));
}

CreationSpec Renderer::creationSpec() const {
    CreationSpec s;
    s.kind = static_cast<CreationKind>(std::clamp(creation_.kind, 0, 2));
    s.faction = std::clamp(creation_.faction, 0, MAX_FACTIONS - 1);
    if (s.kind == CreationKind::Ship) {
        s.count = std::clamp(creation_.shipCount, 1, 2000);
        s.formation = static_cast<FormationType>(std::clamp(creation_.formation, 0, 3));
        s.speed = static_cast<float>(std::clamp(creation_.shipSpeed, 0, 200));
        s.attributes = creation_.attrs;
    } else if (s.kind == CreationKind::Asteroid) {
        s.count = std::clamp(creation_.asteroidCount, 1, 360);
        s.asteroidType = creation_.asteroidType == 1 ? AsteroidType::Comet : AsteroidType::Normal;
        s.asteroidRadius = static_cast<float>(std::clamp(creation_.asteroidRadius, 4, 600));
        s.speed = static_cast<float>(std::clamp(creation_.asteroidSpeed, 0, 800));
        s.irregularity = static_cast<float>(std::clamp(creation_.irregularPercent, 0, 100)) / 100.0f;
    } else {
        s.count = 1;
        s.zoneDuration = static_cast<float>(std::clamp(creation_.zoneDuration, 1, 3600));
        s.zoneRadius = static_cast<float>(std::clamp(creation_.zoneRadius, 10, 800));
        s.zoneType = static_cast<ZoneType>(std::clamp(creation_.zoneType, 0, 5));
    }
    s.portalHealth = static_cast<float>(std::clamp(creation_.portalHealth, 1, 1000000));
    s.portalInterval = static_cast<float>(kPortalIntervals[std::clamp(creation_.portalIntervalIndex, 0, 3)]);
    return s;
}

float Renderer::creationPreviewRadius() const {
    if (creation_.mode == 2) return 62.0f;
    return std::max(18.0f, World::creationRadius(creationSpec()));
}

sf::Color Renderer::creationPreviewColor() const {
    if (creation_.kind == 0) return factionColor(std::clamp(creation_.faction, 0, MAX_FACTIONS - 1));
    if (creation_.kind == 1) return sf::Color(190, 190, 190);
    static const sf::Color zoneColors[6] = {
        sf::Color(235, 245, 255), sf::Color(255, 100, 100), sf::Color(255, 255, 100),
        sf::Color(100, 255, 100), sf::Color(100, 150, 255), sf::Color(200, 100, 255)
    };
    return zoneColors[std::clamp(creation_.zoneType, 0, 5)];
}

int Renderer::creationFieldValue(int field) const {
    switch (field) {
    case CF_SHIP_COUNT: return creation_.shipCount;
    case CF_SHIP_SPEED: return creation_.shipSpeed;
    case CF_SHIP_LEVEL: return 1 + creation_.attrs.total();
    case CF_ATTR0: return creation_.attrs.survival;
    case CF_ATTR1: return creation_.attrs.attack;
    case CF_ATTR2: return creation_.attrs.mobility;
    case CF_ATTR3: return creation_.attrs.production;
    case CF_ATTR4: return creation_.attrs.vision;
    case CF_AST_RADIUS: return creation_.asteroidRadius;
    case CF_AST_COUNT: return creation_.asteroidCount;
    case CF_AST_SPEED: return creation_.asteroidSpeed;
    case CF_IRR: return creation_.irregularPercent;
    case CF_ZONE_DUR: return creation_.zoneDuration;
    case CF_ZONE_RAD: return creation_.zoneRadius;
    case CF_PORTAL_HP: return creation_.portalHealth;
    default: return 0;
    }
}

void Renderer::creationSetField(int field, int value) {
    switch (field) {
    case CF_SHIP_COUNT:
        creation_.shipCount = std::clamp(value, 1, 2000);
        break;
    case CF_SHIP_SPEED:
        creation_.shipSpeed = std::clamp(value, 0, 200);
        break;
    case CF_SHIP_LEVEL:
        creationRandomAttributes(std::clamp(value, 1, 201));
        break;
    case CF_ATTR0: case CF_ATTR1: case CF_ATTR2: case CF_ATTR3: case CF_ATTR4: {
        const int index = field - CF_ATTR0;
        int* slots[5] = { &creation_.attrs.survival, &creation_.attrs.attack,
                          &creation_.attrs.mobility, &creation_.attrs.production,
                          &creation_.attrs.vision };
        const int others = creation_.attrs.total() - *slots[index];
        *slots[index] = std::clamp(value, 0, std::max(0, 200 - others));
        break;
    }
    case CF_AST_RADIUS:
        creation_.asteroidRadius = std::clamp(value, 4, 600);
        break;
    case CF_AST_COUNT:
        creation_.asteroidCount = std::clamp(value, 1, 360);
        break;
    case CF_AST_SPEED:
        creation_.asteroidSpeed = std::clamp(value, 0, 800);
        break;
    case CF_IRR:
        creation_.irregularPercent = std::clamp(value, 0, 100);
        break;
    case CF_ZONE_DUR:
        creation_.zoneDuration = std::clamp(value, 1, 3600);
        break;
    case CF_ZONE_RAD:
        creation_.zoneRadius = std::clamp(value, 10, 800);
        break;
    case CF_PORTAL_HP:
        creation_.portalHealth = std::clamp(value, 1, 1000000);
        break;
    default:
        break;
    }
}

void Renderer::creationRandomAttributes(int level) {
    creation_.attrs = ShipAttributePoints{};
    const int total = std::clamp(level - 1, 0, 200);
    std::uniform_int_distribution<int> dist(0, 4);
    int* slots[5] = { &creation_.attrs.survival, &creation_.attrs.attack,
                      &creation_.attrs.mobility, &creation_.attrs.production,
                      &creation_.attrs.vision };
    for (int i = 0; i < total; ++i) {
        ++(*slots[dist(creationRng_)]);
    }
    creation_.hint = "已随机分配 " + std::to_string(total) + " 点属性（等级 " +
                     std::to_string(level) + "）";
    creation_.hintTimer = 3.0f;
}

void Renderer::creationBeginEdit(int field) {
    creation_.editField = field;
    creation_.editBuffer = std::to_string(creationFieldValue(field));
    creation_.editFresh = true;
}

void Renderer::creationCommitEdit() {
    if (creation_.editField < 0) return;
    const int field = creation_.editField;
    creation_.editField = -1;
    creation_.editFresh = false;
    if (creation_.editBuffer.empty()) return;
    creationSetField(field, std::atoi(creation_.editBuffer.c_str()));
    creation_.editBuffer.clear();
}

void Renderer::creationRandomCreate(World& world) {
    creation_.placing = false;
    creation_.placementStage = 0;
    CreationSpec spec = creationSpec();
    SpawnMode mode = static_cast<SpawnMode>(std::clamp(creation_.mode, 0, 2));
    if (mode == SpawnMode::Reinforce && spec.kind == CreationKind::Opportunity) {
        mode = SpawnMode::Drop;
    }
    const uint32_t n = world.createRandom(spec, mode);
    if (n == 0) {
        creation_.hint = "创造失败：对象数量已达上限";
    } else if (mode == SpawnMode::Portal) {
        creation_.hint = "已随机放置传送门，第一波已刷新";
    } else if (spec.kind == CreationKind::Ship) {
        creation_.hint = "已随机创造 " + std::to_string(n) + " 艘飞船";
    } else if (spec.kind == CreationKind::Asteroid) {
        creation_.hint = "已随机创造 " + std::to_string(n) + " 颗天体";
    } else {
        creation_.hint = "已随机投放 1 个机遇";
    }
    creation_.hintTimer = 4.0f;
}

void Renderer::creationPlaceCommit(World& world, Vec2 second) {
    CreationSpec spec = creationSpec();
    SpawnMode mode = static_cast<SpawnMode>(std::clamp(creation_.mode, 0, 2));
    if (mode == SpawnMode::Reinforce && spec.kind == CreationKind::Opportunity) {
        mode = SpawnMode::Drop;
    }
    const bool needsDirection = (mode != SpawnMode::Portal) &&
                                (spec.kind != CreationKind::Opportunity);
    if (!needsDirection) {
        const uint32_t n = world.createBatch(spec, second, {1.0f, 0.0f}, mode);
        creation_.placing = false;
        creation_.placementStage = 0;
        if (n == 0) {
            creation_.hint = "放置失败：对象数量已达上限";
        } else if (mode == SpawnMode::Portal) {
            creation_.hint = "传送门已放置，第一波已刷新";
        } else {
            creation_.hint = "机遇已投放";
        }
        creation_.hintTimer = 4.0f;
        return;
    }

    if (creation_.placementStage == 0) {
        creation_.anchor = second;
        creation_.placementStage = 1;
        creation_.hint = "再点一次确定初始速度方向（右键取消）";
        creation_.hintTimer = 5.0f;
        return;
    }

    Vec2 dir = second - creation_.anchor;
    if (dir.lengthSq() < 4.0f) dir = {1.0f, 0.0f};
    const uint32_t n = world.createBatch(spec, creation_.anchor, dir, mode);
    creation_.placing = false;
    creation_.placementStage = 0;
    if (n == 0) {
        creation_.hint = "放置失败：对象数量已达上限";
    } else if (spec.kind == CreationKind::Ship) {
        creation_.hint = "已创造 " + std::to_string(n) + " 艘飞船" +
                         (mode == SpawnMode::Reinforce ? "（从界外飞入）" : "");
    } else {
        creation_.hint = "已创造 " + std::to_string(n) + " 颗天体" +
                         (mode == SpawnMode::Reinforce ? "（从界外飞入）" : "");
    }
    creation_.hintTimer = 4.0f;
}

bool Renderer::creationHandleMousePress(float x, float y, World& world, bool fromDetached) {
    if (!creation_.open) return false;

    // 弹出后的面板在独立窗口里：主窗口只负责地图上的放置点击。
    if (!fromDetached && creation_.detached) {
        if (creation_.placing) {
            creationPlaceCommit(world, screenToWorld(x, y));
            return true;
        }
        return false;
    }

    // 手动放置时主窗口面板自动收起，任何地图点击都直接用于放置。
    if (!fromDetached && creation_.placing) {
        if (!mapRect_.contains(x, y)) return true; // 黑边/界面外不放置
        creationPlaceCommit(world, screenToWorld(x, y));
        return true;
    }

    const CreationLayout l = creationLayout(creation_);
    const bool inside = l.rect.contains(x, y);

    if (creation_.editField >= 0) {
        creationCommitEdit();
        if (!inside) return true;
    }

    if (!inside) {
        if (fromDetached) return true;
        return false;
    }

    auto hit = [&](const MonitorRect& r) { return r.contains(x, y); };

    if (hit(l.close)) {
        creation_.open = false;
        closeCreationPopout();
        creationCancelPlacing();
        return true;
    }
    if (hit(l.popBtn)) {
        creationPopOut();
        return true;
    }
    if (hit(l.title)) {
        if (!creation_.detached) {
            creation_.dragging = true;
            creation_.dragDX = x - creation_.x;
            creation_.dragDY = y - creation_.y;
        }
        return true;
    }

    for (int i = 0; i < 3; ++i) {
        if (!hit(l.kind[i])) continue;
        creation_.kind = i;
        if (i == 2 && creation_.mode == 1) creation_.mode = 0;
        creationCancelPlacing();
        return true;
    }
    for (int i = 0; i < 3; ++i) {
        if (!hit(l.mode[i])) continue;
        if (i == 1 && creation_.kind == 2) {
            creation_.hint = "机遇不支持增援，请选择投放或传送门";
            creation_.hintTimer = 2.5f;
            return true;
        }
        creation_.mode = i;
        creationCancelPlacing();
        return true;
    }

    const bool shift = sf::Keyboard::isKeyPressed(sf::Keyboard::LShift);

    if (creation_.mode == 2) {
        if (hit(l.portalHpMinus)) {
            creation_.portalHealth = std::clamp(creation_.portalHealth - (shift ? 10000 : 100),
                                                1, 1000000);
            return true;
        }
        if (hit(l.portalHpPlus)) {
            creation_.portalHealth = std::clamp(creation_.portalHealth + (shift ? 10000 : 100),
                                                1, 1000000);
            return true;
        }
        if (hit(l.portalHpField)) { creationBeginEdit(CF_PORTAL_HP); return true; }
        for (int i = 0; i < 4; ++i) {
            if (hit(l.interval[i])) { creation_.portalIntervalIndex = i; return true; }
        }
    }

    if (creation_.kind == 0) {
        for (int i = 0; i < 4; ++i) {
            if (hit(l.faction[i])) { creation_.faction = i; return true; }
        }
        if (hit(l.shipCountMinus)) {
            creationSetField(CF_SHIP_COUNT, creation_.shipCount - (shift ? 10 : 1));
            return true;
        }
        if (hit(l.shipCountPlus)) {
            creationSetField(CF_SHIP_COUNT, creation_.shipCount + (shift ? 10 : 1));
            return true;
        }
        if (hit(l.shipCountField)) { creationBeginEdit(CF_SHIP_COUNT); return true; }
        for (int i = 0; i < 4; ++i) {
            if (hit(l.formation[i])) { creation_.formation = i; return true; }
        }
        if (hit(l.shipSpeedMinus)) {
            creationSetField(CF_SHIP_SPEED, creation_.shipSpeed - (shift ? 20 : 5));
            return true;
        }
        if (hit(l.shipSpeedPlus)) {
            creationSetField(CF_SHIP_SPEED, creation_.shipSpeed + (shift ? 20 : 5));
            return true;
        }
        if (hit(l.shipSpeedField)) { creationBeginEdit(CF_SHIP_SPEED); return true; }
        for (int i = 0; i < 5; ++i) {
            const int field = CF_ATTR0 + i;
            if (hit(l.attrMinus[i])) {
                creationSetField(field, creationFieldValue(field) - (shift ? 10 : 1));
                return true;
            }
            if (hit(l.attrPlus[i])) {
                creationSetField(field, creationFieldValue(field) + (shift ? 10 : 1));
                return true;
            }
            if (hit(l.attrField[i])) { creationBeginEdit(field); return true; }
        }
        if (hit(l.levelField)) { creationBeginEdit(CF_SHIP_LEVEL); return true; }
        if (hit(l.levelRandom)) {
            creationRandomAttributes(1 + creation_.attrs.total());
            return true;
        }
    } else if (creation_.kind == 1) {
        for (int i = 0; i < 2; ++i) {
            if (!hit(l.astType[i])) continue;
            creation_.asteroidType = i;
            creation_.asteroidSpeed = (i == 1) ? 320 : 30;
            return true;
        }
        if (hit(l.astRadiusMinus)) {
            creationSetField(CF_AST_RADIUS, creation_.asteroidRadius - (shift ? 25 : 5));
            return true;
        }
        if (hit(l.astRadiusPlus)) {
            creationSetField(CF_AST_RADIUS, creation_.asteroidRadius + (shift ? 25 : 5));
            return true;
        }
        if (hit(l.astRadiusField)) { creationBeginEdit(CF_AST_RADIUS); return true; }
        if (hit(l.astCountMinus)) {
            creationSetField(CF_AST_COUNT, creation_.asteroidCount - (shift ? 10 : 1));
            return true;
        }
        if (hit(l.astCountPlus)) {
            creationSetField(CF_AST_COUNT, creation_.asteroidCount + (shift ? 10 : 1));
            return true;
        }
        if (hit(l.astCountField)) { creationBeginEdit(CF_AST_COUNT); return true; }
        if (hit(l.astSpeedMinus)) {
            creationSetField(CF_AST_SPEED, creation_.asteroidSpeed - (shift ? 50 : 10));
            return true;
        }
        if (hit(l.astSpeedPlus)) {
            creationSetField(CF_AST_SPEED, creation_.asteroidSpeed + (shift ? 50 : 10));
            return true;
        }
        if (hit(l.astSpeedField)) { creationBeginEdit(CF_AST_SPEED); return true; }
        if (hit(l.irrMinus)) {
            creationSetField(CF_IRR, creation_.irregularPercent - (shift ? 25 : 5));
            return true;
        }
        if (hit(l.irrPlus)) {
            creationSetField(CF_IRR, creation_.irregularPercent + (shift ? 25 : 5));
            return true;
        }
        if (hit(l.irrField)) { creationBeginEdit(CF_IRR); return true; }
    } else {
        if (hit(l.zoneDurMinus)) {
            creationSetField(CF_ZONE_DUR, creation_.zoneDuration - (shift ? 30 : 5));
            return true;
        }
        if (hit(l.zoneDurPlus)) {
            creationSetField(CF_ZONE_DUR, creation_.zoneDuration + (shift ? 30 : 5));
            return true;
        }
        if (hit(l.zoneDurField)) { creationBeginEdit(CF_ZONE_DUR); return true; }
        if (hit(l.zoneRadMinus)) {
            creationSetField(CF_ZONE_RAD, creation_.zoneRadius - (shift ? 50 : 10));
            return true;
        }
        if (hit(l.zoneRadPlus)) {
            creationSetField(CF_ZONE_RAD, creation_.zoneRadius + (shift ? 50 : 10));
            return true;
        }
        if (hit(l.zoneRadField)) { creationBeginEdit(CF_ZONE_RAD); return true; }
        for (int i = 0; i < 6; ++i) {
            if (hit(l.zoneType[i])) { creation_.zoneType = i; return true; }
        }
    }

    if (hit(l.randomBtn)) {
        creationRandomCreate(world);
        return true;
    }
    if (hit(l.manualBtn)) {
        if (creation_.placing) {
            creationCancelPlacing();
        } else {
            creation_.placing = true;
            creation_.placementStage = 0;
            creation_.hint = "鼠标移到地图上点击确定范围；右键/Esc 取消";
            creation_.hintTimer = 5.0f;
        }
        return true;
    }
    return true;
}

bool Renderer::creationHandleMouseRelease(float, float) {
    const bool wasDragging = creation_.dragging;
    creation_.dragging = false;
    return wasDragging;
}

bool Renderer::creationHandleMouseMove(float x, float y) {
    creation_.mouseX = x;
    creation_.mouseY = y;
    if (!creation_.dragging) return false;
    creation_.x = x - creation_.dragDX;
    creation_.y = y - creation_.dragDY;
    clampCreationPanel();
    return true;
}

bool Renderer::creationHandleKey(sf::Keyboard::Key code) {
    if (creation_.editField >= 0) {
        if (code == sf::Keyboard::BackSpace) {
            creation_.editFresh = false;
            if (!creation_.editBuffer.empty()) creation_.editBuffer.pop_back();
            return true;
        }
        if (code == sf::Keyboard::Enter || code == sf::Keyboard::Return) {
            creationCommitEdit();
            return true;
        }
        if (code == sf::Keyboard::Escape) {
            creation_.editField = -1;
            creation_.editFresh = false;
            creation_.editBuffer.clear();
            return true;
        }
        int digit = -1;
        if (code >= sf::Keyboard::Num0 && code <= sf::Keyboard::Num9) {
            digit = static_cast<int>(code) - static_cast<int>(sf::Keyboard::Num0);
        } else if (code >= sf::Keyboard::Numpad0 && code <= sf::Keyboard::Numpad9) {
            digit = static_cast<int>(code) - static_cast<int>(sf::Keyboard::Numpad0);
        }
        if (digit >= 0) {
            if (creation_.editFresh) {
                creation_.editBuffer.clear();
                creation_.editFresh = false;
            }
            if (creation_.editBuffer.size() < 9) {
                if (creation_.editBuffer == "0") creation_.editBuffer.clear();
                creation_.editBuffer.push_back(static_cast<char>('0' + digit));
            }
        }
        return true; // 编辑时吞掉其它按键，避免误触 R/O/C/F 等
    }

    if (code == sf::Keyboard::Escape) {
        if (creation_.placing) {
            creationCancelPlacing();
            return true;
        }
        if (creation_.open) {
            creation_.open = false;
            closeCreationPopout();
            return true;
        }
    }
    return false;
}

bool Renderer::creationHandleText(sf::Uint32) {
    // 数字输入统一走 KeyPressed（这样自动化 PostMessage 也能输入），
    // 这里只负责把文本事件吞掉，避免落到其它界面。
    return creation_.editField >= 0;
}

void Renderer::drawCreation(sf::RenderWindow& window, const World& world) {
    if (creation_.hintTimer > 0.0f) {
        creation_.hintTimer = std::max(0.0f, creation_.hintTimer - 1.0f / 60.0f);
    }
    if (!creation_.open && !creation_.placing) return;

    if (creation_.placing) drawPlacementStrip(window);
    if (creation_.detached) return;
    if (creation_.open && !creation_.placing) drawCreationPanel(window, world);
}

void Renderer::drawPlacementStrip(sf::RenderWindow& window) {
    const float w = 620.0f;
    const float h = 58.0f;
    const float x = (WINDOW_WIDTH - w) * 0.5f;
    const float y = WINDOW_HEIGHT - 132.0f;
    const sf::Color accent(110, 230, 205);

    sf::RectangleShape bg({w, h});
    bg.setPosition(x, y);
    bg.setFillColor(sf::Color(8, 14, 18, 235));
    bg.setOutlineColor(withAlpha(accent, 170));
    bg.setOutlineThickness(1.2f);
    window.draw(bg);

    const bool direction = creation_.placementStage == 1;
    text(window, "创造放置中", x + 16.0f, y + 8.0f, 15, sf::Color::White);
    text(window, direction ? "再点一次确定初始速度方向" : "在地图上点击确定范围/位置",
         x + 126.0f, y + 9.0f, 13, withAlpha(accent, 230));
    text(window, "面板已暂时收起 · 右键或 Esc 取消 · 完成后面板自动恢复",
         x + 16.0f, y + 32.0f, 11, sf::Color(190, 200, 212, 190));
}

void Renderer::drawCreationPreview(sf::RenderWindow& window, const World&) {
    (void)window;
    if (!creation_.placing) return;
    const float sx = creation_.mouseX;
    const float sy = creation_.mouseY;
    if (!mapRect_.contains(sx, sy)) return;
    const Vec2 mouse = screenToWorld(sx, sy);
    const bool waitingDirection = creation_.placementStage == 1;
    const Vec2 center = waitingDirection ? creation_.anchor : mouse;
    const float radius = creationPreviewRadius();
    const sf::Color accent = creationPreviewColor();

    // 范围预览：虚线圆 + 淡填充。
    if (creation_.mode == 2) {
        drawVortex(window, center, 46.0f, accent, 0.0f, 0.65f);
    } else {
        sf::CircleShape fill(radius, 64);
        fill.setOrigin(radius, radius);
        fill.setPosition(center.sf());
        fill.setFillColor(sf::Color(accent.r, accent.g, accent.b, creation_.kind == 2 ? 20 : 14));
        window.draw(fill);
    }

    sf::VertexArray ring(sf::Lines);
    constexpr int kSeg = 96;
    for (int i = 0; i < kSeg; i += 2) {
        const float a0 = static_cast<float>(i) / kSeg * TAU;
        const float a1 = static_cast<float>(i + 1) / kSeg * TAU;
        ring.append(sf::Vertex((center + Vec2(std::cos(a0), std::sin(a0)) * radius).sf(),
                               sf::Color(accent.r, accent.g, accent.b, 150)));
        ring.append(sf::Vertex((center + Vec2(std::cos(a1), std::sin(a1)) * radius).sf(),
                               sf::Color(accent.r, accent.g, accent.b, 150)));
    }
    window.draw(ring);

    // 中心十字。
    sf::VertexArray cross(sf::Lines, 4);
    cross[0] = sf::Vertex({center.x - 8.0f, center.y}, accent);
    cross[1] = sf::Vertex({center.x + 8.0f, center.y}, accent);
    cross[2] = sf::Vertex({center.x, center.y - 8.0f}, accent);
    cross[3] = sf::Vertex({center.x, center.y + 8.0f}, accent);
    window.draw(cross);

    if (waitingDirection) {
        Vec2 dir = mouse - creation_.anchor;
        if (dir.lengthSq() < 4.0f) dir = {1.0f, 0.0f};
        const Vec2 unit = dir.normalized();
        sf::VertexArray arrow(sf::Lines, 6);
        const sf::Color arrowColor = withAlpha(accent, 235);
        arrow[0] = sf::Vertex(creation_.anchor.sf(), arrowColor);
        arrow[1] = sf::Vertex(mouse.sf(), arrowColor);
        const Vec2 tip = mouse;
        const Vec2 perp = unit.perpendicular();
        arrow[2] = sf::Vertex(tip.sf(), arrowColor);
        arrow[3] = sf::Vertex((tip - unit * 16.0f + perp * 7.0f).sf(), arrowColor);
        arrow[4] = sf::Vertex(tip.sf(), arrowColor);
        arrow[5] = sf::Vertex((tip - unit * 16.0f - perp * 7.0f).sf(), arrowColor);
        window.draw(arrow);

        if (creation_.mode == 1) {
            const CreationSpec spec = creationSpec();
            const float push = (spec.kind == CreationKind::Asteroid)
                                   ? std::min(radius, 120.0f)
                                   : radius;
            const Vec2 ghost = World::reinforcementOrigin(creation_.anchor, -unit, push,
                                                          worldWidth_, worldHeight_);
            const sf::Color ghostColor = withAlpha(accent, 95);
            sf::VertexArray ghostRing(sf::Lines);
            for (int i = 0; i < kSeg; i += 4) {
                const float a0 = static_cast<float>(i) / kSeg * TAU;
                const float a1 = static_cast<float>(i + 1) / kSeg * TAU;
                ghostRing.append(sf::Vertex((ghost + Vec2(std::cos(a0), std::sin(a0)) * radius).sf(),
                                            ghostColor));
                ghostRing.append(sf::Vertex((ghost + Vec2(std::cos(a1), std::sin(a1)) * radius).sf(),
                                            ghostColor));
            }
            window.draw(ghostRing);
            textCentered(window, "界外增援点", ghost.x, ghost.y + radius + 8.0f, 11,
                         withAlpha(accent, 210));
        }
    }
}

void Renderer::drawCreationPanel(sf::RenderWindow& window, const World&) {
    const CreationLayout l = creationLayout(creation_);
    const sf::Color accent(110, 230, 205);
    const sf::Color dim(150, 162, 178);
    const sf::Color warm(255, 196, 100);

    sf::RectangleShape bg({l.rect.w, l.rect.h});
    bg.setPosition(l.rect.x, l.rect.y);
    bg.setFillColor(sf::Color(8, 14, 18, 248));
    bg.setOutlineColor(sf::Color(110, 225, 205, 170));
    bg.setOutlineThickness(1.4f);
    window.draw(bg);

    sf::RectangleShape titleBar({l.rect.w, 36.0f});
    titleBar.setPosition(l.rect.x, l.rect.y);
    titleBar.setFillColor(sf::Color(16, 40, 40, 245));
    window.draw(titleBar);

    text(window, "创造面板", l.rect.x + 16.0f, l.rect.y + 8.0f, 19, sf::Color::White);
    text(window, "创造物 · 出现方式 · 随机/手动", l.rect.x + 128.0f, l.rect.y + 12.0f, 12,
         withAlpha(accent, 200));

    {
        sf::VertexArray x(sf::Lines, 4);
        x[0] = sf::Vertex({l.close.x + 7.0f, l.close.y + 6.0f}, sf::Color(230, 230, 230));
        x[1] = sf::Vertex({l.close.x + 21.0f, l.close.y + 20.0f}, sf::Color(230, 230, 230));
        x[2] = sf::Vertex({l.close.x + 21.0f, l.close.y + 6.0f}, sf::Color(230, 230, 230));
        x[3] = sf::Vertex({l.close.x + 7.0f, l.close.y + 20.0f}, sf::Color(230, 230, 230));
        window.draw(x);
    }

    {
        sf::RectangleShape pb({l.popBtn.w, l.popBtn.h});
        pb.setPosition(l.popBtn.x, l.popBtn.y);
        pb.setFillColor(creation_.detached ? sf::Color(90, 60, 32, 245)
                                           : sf::Color(32, 58, 48, 245));
        pb.setOutlineColor(creation_.detached ? sf::Color(230, 160, 80, 200)
                                              : sf::Color(90, 190, 140, 190));
        pb.setOutlineThickness(1.0f);
        window.draw(pb);
        textCentered(window, creation_.detached ? "收回" : "弹出",
                     l.popBtn.x + l.popBtn.w * 0.5f, l.popBtn.y + 4.0f, 12,
                     sf::Color(235, 241, 248));
    }

    auto button = [&](const MonitorRect& r, const std::string& label, bool selected,
                      bool enabled, sf::Color tint) {
        sf::RectangleShape b({r.w, r.h});
        b.setPosition(r.x, r.y);
        b.setFillColor(enabled ? (selected ? withAlpha(tint, 205) : sf::Color(24, 34, 40, 240))
                               : sf::Color(16, 20, 24, 220));
        b.setOutlineColor(enabled ? withAlpha(selected ? tint : sf::Color(80, 105, 110),
                                              selected ? 235 : 110)
                                  : sf::Color(48, 55, 60, 110));
        b.setOutlineThickness(selected ? 1.6f : 1.0f);
        window.draw(b);
        textCentered(window, label, r.x + r.w * 0.5f, r.y + (r.h - 13.0f) * 0.5f - 1.0f, 12,
                     enabled ? (selected ? sf::Color::White : sf::Color(210, 222, 228))
                             : sf::Color(90, 98, 105));
    };

    auto field = [&](const MonitorRect& r, int id, const std::string& value) {
        const bool editing = creation_.editField == id;
        sf::RectangleShape fb({r.w, r.h});
        fb.setPosition(r.x, r.y);
        fb.setFillColor(editing ? sf::Color(32, 74, 70, 250) : sf::Color(18, 26, 32, 245));
        fb.setOutlineColor(editing ? withAlpha(accent, 240) : sf::Color(80, 100, 110, 180));
        fb.setOutlineThickness(editing ? 1.8f : 1.0f);
        window.draw(fb);
        std::string shown = editing ? (creation_.editBuffer.empty() ? "_" : creation_.editBuffer + "_")
                                    : value;
        textCentered(window, shown, r.x + r.w * 0.5f, r.y + (r.h - 14.0f) * 0.5f - 1.0f, 13,
                     editing ? sf::Color(255, 245, 200) : sf::Color::White);
    };

    auto label = [&](const std::string& s, float px, float py, sf::Color c = sf::Color(150, 162, 178)) {
        text(window, s, px, py, 12, c);
    };

    label("创造物", l.rect.x + 16.0f, l.rect.y + 46.0f);
    for (int i = 0; i < 3; ++i) {
        button(l.kind[i], creationKindName(i), creation_.kind == i, true, accent);
    }

    label("出现方式", l.rect.x + 16.0f, l.rect.y + 98.0f);
    for (int i = 0; i < 3; ++i) {
        const bool enabled = !(i == 1 && creation_.kind == 2);
        button(l.mode[i], spawnModeName(i), creation_.mode == i, enabled,
               i == 1 ? warm : accent);
    }

    if (creation_.mode == 2) {
        label("血量", l.rect.x + 16.0f, l.rect.y + 160.0f);
        button(l.portalHpMinus, "-", false, true, accent);
        field(l.portalHpField, CF_PORTAL_HP, std::to_string(creation_.portalHealth));
        button(l.portalHpPlus, "+", false, true, accent);
        label("刷新", l.rect.x + 206.0f, l.rect.y + 160.0f);
        static const char* intervalNames[4] = { "1分", "3分", "5分", "10分" };
        for (int i = 0; i < 4; ++i) {
            button(l.interval[i], intervalNames[i], creation_.portalIntervalIndex == i, true,
                   accent);
        }
    } else {
        text(window,
             creation_.mode == 0 ? "投放：在指定范围内直接出现。"
                                 : "增援：最终位置沿方向延伸到界面外，再飞进来。",
             l.rect.x + 16.0f, l.rect.y + 158.0f, 12, withAlpha(dim, 210));
    }

    static const char* attrNames[5] = { "生存", "攻击", "机动", "生产", "视野" };

    if (creation_.kind == 0) {
        label("阵营", l.rect.x + 16.0f, l.rect.y + 200.0f);
        static const char* factionLabels[4] = { "红", "蓝", "绿", "黄" };
        for (int i = 0; i < 4; ++i) {
            button(l.faction[i], factionLabels[i], creation_.faction == i, true, factionColor(i));
        }

        label("数量", l.rect.x + 16.0f, l.rect.y + 236.0f);
        button(l.shipCountMinus, "-", false, true, accent);
        field(l.shipCountField, CF_SHIP_COUNT, std::to_string(creation_.shipCount));
        button(l.shipCountPlus, "+", false, true, accent);
        text(window, "1-2000", l.rect.x + 190.0f, l.rect.y + 236.0f, 10, withAlpha(dim, 180));
        label("阵型", l.rect.x + 250.0f, l.rect.y + 236.0f);
        for (int i = 0; i < 4; ++i) {
            button(l.formation[i], formationName(i), creation_.formation == i, true, accent);
        }

        label("初速", l.rect.x + 16.0f, l.rect.y + 272.0f);
        button(l.shipSpeedMinus, "-", false, true, accent);
        field(l.shipSpeedField, CF_SHIP_SPEED, std::to_string(creation_.shipSpeed));
        button(l.shipSpeedPlus, "+", false, true, accent);
        text(window, "px/s（上限 200）", l.rect.x + 192.0f, l.rect.y + 272.0f, 10,
             withAlpha(dim, 180));

        label("属性加点", l.rect.x + 16.0f, l.rect.y + 310.0f);
        text(window, "等级 Lv." + std::to_string(1 + creation_.attrs.total()),
             l.rect.x + 420.0f, l.rect.y + 308.0f, 14, sf::Color(255, 236, 170));
        for (int i = 0; i < 5; ++i) {
            const float rowY = l.rect.y + 332.0f + static_cast<float>(i) * 32.0f;
            label(attrNames[i], l.rect.x + 24.0f, rowY);
            button(l.attrMinus[i], "-", false, true, accent);
            field(l.attrField[i], CF_ATTR0 + i, std::to_string(creationFieldValue(CF_ATTR0 + i)));
            button(l.attrPlus[i], "+", false, true, accent);
            if (i == 0) {
                text(window, "累计 " + std::to_string(creation_.attrs.total()) + " 点（上限 200）",
                     l.rect.x + 260.0f, rowY, 10, withAlpha(dim, 160));
            }
        }

        label("直接等级", l.rect.x + 16.0f, l.rect.y + 498.0f);
        field(l.levelField, CF_SHIP_LEVEL, std::to_string(1 + creation_.attrs.total()));
        button(l.levelRandom, "随机分配", false, true, warm);
        text(window, "输入等级后回车：自动随机分配等级-1 点；等级 = 1 + 总加点。",
             l.rect.x + 16.0f, l.rect.y + 530.0f, 10, withAlpha(dim, 170));
    } else if (creation_.kind == 1) {
        label("种类", l.rect.x + 16.0f, l.rect.y + 200.0f);
        for (int i = 0; i < 2; ++i) {
            button(l.astType[i], asteroidTypeName(i), creation_.asteroidType == i, true,
                   i == 1 ? sf::Color(140, 165, 255) : sf::Color(190, 180, 160));
        }

        label("大小", l.rect.x + 16.0f, l.rect.y + 236.0f);
        button(l.astRadiusMinus, "-", false, true, accent);
        field(l.astRadiusField, CF_AST_RADIUS, std::to_string(creation_.asteroidRadius));
        button(l.astRadiusPlus, "+", false, true, accent);
        text(window, "半径 px（最大 600，可非常大）", l.rect.x + 192.0f, l.rect.y + 236.0f, 10,
             withAlpha(dim, 180));

        label("数量", l.rect.x + 16.0f, l.rect.y + 272.0f);
        button(l.astCountMinus, "-", false, true, accent);
        field(l.astCountField, CF_AST_COUNT, std::to_string(creation_.asteroidCount));
        button(l.astCountPlus, "+", false, true, accent);
        text(window, "1-360（受场景小行星上限限制）", l.rect.x + 192.0f, l.rect.y + 272.0f, 10,
             withAlpha(dim, 180));

        label("初速", l.rect.x + 16.0f, l.rect.y + 308.0f);
        button(l.astSpeedMinus, "-", false, true, accent);
        field(l.astSpeedField, CF_AST_SPEED, std::to_string(creation_.asteroidSpeed));
        button(l.astSpeedPlus, "+", false, true, accent);
        text(window, creation_.asteroidType == 1 ? "默认 320 px/s" : "默认 30 px/s",
             l.rect.x + 192.0f, l.rect.y + 308.0f, 10, withAlpha(dim, 180));

        label("不规则", l.rect.x + 16.0f, l.rect.y + 344.0f);
        button(l.irrMinus, "-", false, true, accent);
        field(l.irrField, CF_IRR, std::to_string(creation_.irregularPercent));
        button(l.irrPlus, "+", false, true, accent);
        text(window, "%（0 圆润 - 100 破碎）", l.rect.x + 192.0f, l.rect.y + 344.0f, 10,
             withAlpha(dim, 180));

        const CreationSpec spec = creationSpec();
        const float groupR = World::creationRadius(spec);
        text(window, "本组散布半径 ≈ " + std::to_string(static_cast<int>(groupR)) + " px" +
                         (groupR > 450.0f ? "（很大，部分会落在界面外）" : ""),
             l.rect.x + 16.0f, l.rect.y + 386.0f, 12, withAlpha(accent, 220));
    } else {
        label("持续", l.rect.x + 16.0f, l.rect.y + 200.0f);
        button(l.zoneDurMinus, "-", false, true, accent);
        field(l.zoneDurField, CF_ZONE_DUR, std::to_string(creation_.zoneDuration));
        button(l.zoneDurPlus, "+", false, true, accent);
        text(window, "秒（1-3600）", l.rect.x + 192.0f, l.rect.y + 200.0f, 10,
             withAlpha(dim, 180));

        label("最大范围", l.rect.x + 16.0f, l.rect.y + 236.0f);
        button(l.zoneRadMinus, "-", false, true, accent);
        field(l.zoneRadField, CF_ZONE_RAD, std::to_string(creation_.zoneRadius));
        button(l.zoneRadPlus, "+", false, true, accent);
        text(window, "px（10-800）", l.rect.x + 192.0f, l.rect.y + 236.0f, 10,
             withAlpha(dim, 180));

        label("机遇种类", l.rect.x + 16.0f, l.rect.y + 270.0f);
        for (int i = 0; i < 6; ++i) {
            button(l.zoneType[i], zoneTypeName(i), creation_.zoneType == i, true,
                   i == 1 ? sf::Color(255, 120, 120) : accent);
        }
        text(window, "机遇只能投放 / 传送门；点击一次即完成，不需要第二次方向点击。",
             l.rect.x + 16.0f, l.rect.y + 414.0f, 11, withAlpha(dim, 190));
    }

    // 底部说明 + 状态 + 当前参数摘要
    std::string modeNote;
    const std::string identity =
        creation_.kind == 0
            ? std::string(factionShortName(creation_.faction)) + "方飞船"
            : (creation_.kind == 1 ? "小行星"
                                   : std::string("机遇·") + zoneTypeName(creation_.zoneType));
    if (creation_.mode == 0) {
        modeNote = "投放：在指定范围内直接出现。";
    } else if (creation_.mode == 1) {
        modeNote = "增援：最终位置沿方向推到界面外，再飞进来。";
    } else {
        modeNote = "传送门：固定、无碰撞箱、可被激光打掉；视为" + identity + "，每 " +
                   std::to_string(kPortalIntervals[std::clamp(creation_.portalIntervalIndex, 0, 3)] / 60) +
                   " 分钟原地刷一波。";
    }
    text(window, modeNote, l.rect.x + 16.0f, l.rect.y + kCreateH - 66.0f, 11,
         withAlpha(dim, 220));

    const std::string hint = (creation_.hintTimer > 0.0f && !creation_.hint.empty())
                                 ? creation_.hint
                                 : (creation_.placing
                                        ? "在地图上点击确定范围；右键/Esc 取消"
                                        : "随机=地图随机位置；手动=点击确定范围，再点确定方向。");
    text(window, hint, l.rect.x + 16.0f, l.rect.y + kCreateH - 46.0f, 11,
         creation_.hintTimer > 0.0f ? sf::Color(255, 230, 160) : withAlpha(accent, 210));

    const CreationSpec spec = creationSpec();
    std::string summary = "当前：";
    if (spec.kind == CreationKind::Ship) {
        summary += std::string(factionShortName(spec.faction)) + "方 " +
                   std::to_string(spec.count) + " 艘 · " + formationName(creation_.formation) +
                   " · 初速 " + std::to_string(static_cast<int>(spec.speed)) + " · Lv." +
                   std::to_string(1 + spec.attributes.total());
    } else if (spec.kind == CreationKind::Asteroid) {
        summary += asteroidTypeName(creation_.asteroidType) + std::string(" ×") +
                   std::to_string(spec.count) + " · 半径 " +
                   std::to_string(static_cast<int>(spec.asteroidRadius)) + " · 初速 " +
                   std::to_string(static_cast<int>(spec.speed)) + " · 不规则 " +
                   std::to_string(creation_.irregularPercent) + "%";
    } else {
        summary += std::string("机遇·") + zoneTypeName(creation_.zoneType) + " · " +
                   std::to_string(static_cast<int>(spec.zoneDuration)) + "s · 最大范围 " +
                   std::to_string(static_cast<int>(spec.zoneRadius));
    }
    if (spec.kind != CreationKind::Opportunity) {
        summary += " · 范围≈" + std::to_string(static_cast<int>(World::creationRadius(spec))) + "px";
    }
    text(window, summary, l.rect.x + 16.0f, l.rect.y + kCreateH - 26.0f, 10,
         withAlpha(dim, 170));

    button(l.randomBtn, "随机创造", true, true, sf::Color(40, 160, 110));
    button(l.manualBtn, creation_.placing ? "取消放置" : "手动放置", false, true,
           creation_.placing ? sf::Color(190, 80, 80) : sf::Color(50, 140, 160));
}

// ============================================================
// 观察面板：视野 / 等级 / 传送门信息 / 称号
// ============================================================

void Renderer::observationToggle() {
    observation_.open = !observation_.open;
    observation_.dragging = false;
}

void Renderer::clampObservationPanel() {
    observation_.x = clampf(observation_.x, 0.0f, std::max(0.0f, WINDOW_WIDTH - kObsW));
    observation_.y = clampf(observation_.y, 0.0f, std::max(0.0f, WINDOW_HEIGHT - kObsH));
}

bool Renderer::observationHandleMousePress(float x, float y, OverlayState& overlay) {
    if (!observation_.open) return false;
    const ObservationLayout l = observationLayout(observation_);
    if (!l.rect.contains(x, y)) return false;

    if (l.close.contains(x, y)) {
        observation_.open = false;
        observation_.dragging = false;
        return true;
    }
    if (l.title.contains(x, y)) {
        observation_.dragging = true;
        observation_.dragDX = x - observation_.x;
        observation_.dragDY = y - observation_.y;
        return true;
    }

    auto toggleVision = [&]() {
        if (overlay.visionMode != 4) {
            observation_.lastVisionMode = overlay.visionMode;
            overlay.visionMode = 4;
        } else {
            overlay.visionMode = (observation_.lastVisionMode == 4) ? 0 : observation_.lastVisionMode;
        }
    };

    auto hit = [&](const MonitorRect& r) { return r.contains(x, y); };

    for (int i = 0; i < 4; ++i) {
        if (!hit(l.row[i])) continue;
        switch (i) {
        case 0: toggleVision(); break;
        case 1: overlay.showLevel = !overlay.showLevel; break;
        case 2: overlay.showPortalInfo = !overlay.showPortalInfo; break;
        case 3: overlay.showTitles = !overlay.showTitles; break;
        default: break;
        }
        return true;
    }

    if (hit(l.allOn)) {
        if (overlay.visionMode == 4) {
            overlay.visionMode = (observation_.lastVisionMode == 4) ? 0 : observation_.lastVisionMode;
        }
        overlay.showLevel = true;
        overlay.showPortalInfo = true;
        overlay.showTitles = true;
        return true;
    }
    if (hit(l.allOff)) {
        if (overlay.visionMode != 4) observation_.lastVisionMode = overlay.visionMode;
        overlay.visionMode = 4;
        overlay.showLevel = false;
        overlay.showPortalInfo = false;
        overlay.showTitles = false;
        return true;
    }
    return true;
}

bool Renderer::observationHandleMouseRelease(float, float) {
    const bool wasDragging = observation_.dragging;
    observation_.dragging = false;
    return wasDragging;
}

bool Renderer::observationHandleMouseMove(float x, float y) {
    if (!observation_.dragging) return false;
    observation_.x = x - observation_.dragDX;
    observation_.y = y - observation_.dragDY;
    clampObservationPanel();
    return true;
}

void Renderer::drawObservation(sf::RenderWindow& window, const World&, OverlayState& overlay) {
    if (!observation_.open) return;
    if (overlay.visionMode != 4) observation_.lastVisionMode = overlay.visionMode;

    const ObservationLayout l = observationLayout(observation_);
    const sf::Color accent(110, 230, 205);
    const sf::Color dim(150, 162, 178);
    const bool visionOn = overlay.visionMode != 4;
    const bool states[4] = { visionOn, overlay.showLevel, overlay.showPortalInfo,
                             overlay.showTitles };
    static const char* names[4] = { "视野显示", "等级显示", "传送门信息", "称号显示" };

    sf::RectangleShape bg({l.rect.w, l.rect.h});
    bg.setPosition(l.rect.x, l.rect.y);
    bg.setFillColor(sf::Color(8, 14, 16, 245));
    bg.setOutlineColor(sf::Color(110, 225, 205, 150));
    bg.setOutlineThickness(1.3f);
    window.draw(bg);

    sf::RectangleShape titleBar({l.rect.w, 34.0f});
    titleBar.setPosition(l.rect.x, l.rect.y);
    titleBar.setFillColor(sf::Color(16, 38, 38, 245));
    window.draw(titleBar);

    text(window, "观察面板", l.rect.x + 14.0f, l.rect.y + 7.0f, 18, sf::Color::White);
    text(window, "视野 · 等级 · 传送门 · 称号", l.rect.x + 110.0f, l.rect.y + 11.0f, 11,
         withAlpha(accent, 190));

    {
        sf::VertexArray x(sf::Lines, 4);
        x[0] = sf::Vertex({l.close.x + 6.0f, l.close.y + 6.0f}, sf::Color(230, 230, 230));
        x[1] = sf::Vertex({l.close.x + 19.0f, l.close.y + 19.0f}, sf::Color(230, 230, 230));
        x[2] = sf::Vertex({l.close.x + 19.0f, l.close.y + 6.0f}, sf::Color(230, 230, 230));
        x[3] = sf::Vertex({l.close.x + 6.0f, l.close.y + 19.0f}, sf::Color(230, 230, 230));
        window.draw(x);
    }

    for (int i = 0; i < 4; ++i) {
        const MonitorRect& r = l.row[i];
        sf::RectangleShape rowBg({r.w, r.h});
        rowBg.setPosition(r.x, r.y);
        rowBg.setFillColor(states[i] ? sf::Color(24, 58, 54, 235) : sf::Color(20, 26, 32, 230));
        rowBg.setOutlineColor(states[i] ? withAlpha(accent, 150) : sf::Color(60, 72, 82, 130));
        rowBg.setOutlineThickness(1.0f);
        window.draw(rowBg);

        text(window, names[i], r.x + 14.0f, r.y + 9.0f, 14,
             states[i] ? sf::Color::White : sf::Color(200, 208, 218));
        if (i == 0) {
            text(window, visionModeName(overlay.visionMode), r.x + 92.0f, r.y + 11.0f, 11,
                 withAlpha(dim, 200));
        }

        sf::RectangleShape sw({56.0f, 22.0f});
        sw.setPosition(r.x + r.w - 70.0f, r.y + 6.0f);
        sw.setFillColor(states[i] ? withAlpha(accent, 220) : sf::Color(52, 60, 68, 240));
        sw.setOutlineColor(states[i] ? withAlpha(accent, 240) : sf::Color(90, 102, 112, 160));
        sw.setOutlineThickness(1.0f);
        window.draw(sw);
        textCentered(window, states[i] ? "开" : "关", sw.getPosition().x + 28.0f,
                     sw.getPosition().y + 3.0f, 12,
                     states[i] ? sf::Color(8, 20, 18) : sf::Color(200, 208, 216));
    }

    text(window, "点整行切换 · Tab 切视野模式 · 全开/全关只影响这四项", l.rect.x + 16.0f,
         l.rect.y + 212.0f, 10, withAlpha(dim, 170));

    auto button = [&](const MonitorRect& r, const std::string& label, sf::Color tint) {
        sf::RectangleShape b({r.w, r.h});
        b.setPosition(r.x, r.y);
        b.setFillColor(withAlpha(tint, 210));
        b.setOutlineColor(withAlpha(tint, 240));
        b.setOutlineThickness(1.2f);
        window.draw(b);
        textCentered(window, label, r.x + r.w * 0.5f, r.y + 9.0f, 14, sf::Color::White);
    };
    button(l.allOn, "全开", sf::Color(44, 150, 110));
    button(l.allOff, "全关", sf::Color(175, 80, 80));
}

// ============================================================
// 沙盒面板：战场大小 / 显示模式
// ============================================================

void Renderer::sandboxToggle() {
    sandbox_.open = !sandbox_.open;
    sandbox_.dragging = false;
}

void Renderer::clampSandboxPanel() {
    sandbox_.x = clampf(sandbox_.x, 0.0f, std::max(0.0f, WINDOW_WIDTH - kSandboxW));
    sandbox_.y = clampf(sandbox_.y, 0.0f, std::max(0.0f, WINDOW_HEIGHT - kSandboxH));
}

bool Renderer::sandboxHandleMousePress(float x, float y, World& world, OverlayState& overlay) {
    if (!sandbox_.open) return false;
    const SandboxLayout l = sandboxLayout(sandbox_);
    if (!l.rect.contains(x, y)) return false;

    if (l.close.contains(x, y)) {
        sandbox_.open = false;
        sandbox_.dragging = false;
        return true;
    }
    if (l.title.contains(x, y)) {
        sandbox_.dragging = true;
        sandbox_.dragDX = x - sandbox_.x;
        sandbox_.dragDY = y - sandbox_.y;
        return true;
    }

    static const float sizes[4][2] = {
        { 800.0f, 450.0f }, { 1600.0f, 900.0f },
        { 3200.0f, 1800.0f }, { 4800.0f, 2700.0f }
    };
    for (int i = 0; i < 4; ++i) {
        if (l.sizeBtn[i].contains(x, y)) {
            world.setWorldSize(sizes[i][0], sizes[i][1]);
            return true;
        }
    }
    for (int i = 0; i < 3; ++i) {
        if (l.modeBtn[i].contains(x, y)) {
            overlay.windowModeRequest = i;
            return true;
        }
    }
    if (l.restartBtn.contains(x, y)) {
        world.restart();
        return true;
    }
    return true;
}

bool Renderer::sandboxHandleMouseRelease(float, float) {
    const bool wasDragging = sandbox_.dragging;
    sandbox_.dragging = false;
    return wasDragging;
}

bool Renderer::sandboxHandleMouseMove(float x, float y) {
    if (!sandbox_.dragging) return false;
    sandbox_.x = x - sandbox_.dragDX;
    sandbox_.y = y - sandbox_.dragDY;
    clampSandboxPanel();
    return true;
}

void Renderer::drawSandbox(sf::RenderWindow& window, const World& world, OverlayState& overlay) {
    if (!sandbox_.open) return;
    const SandboxLayout l = sandboxLayout(sandbox_);
    const sf::Color accent(120, 210, 255);
    const sf::Color dim(150, 162, 178);
    const sf::Color warm(255, 196, 100);

    static const float sizes[4][2] = {
        { 800.0f, 450.0f }, { 1600.0f, 900.0f },
        { 3200.0f, 1800.0f }, { 4800.0f, 2700.0f }
    };
    static const char* sizeLabels[4] = { "0.25×", "1×", "4×", "9×" };
    static const char* sizeDims[4] = { "800×450", "1600×900", "3200×1800", "4800×2700" };
    static const char* modeLabels[3] = { "窗口", "全屏", "无边框（留任务栏）" };

    sf::RectangleShape bg({l.rect.w, l.rect.h});
    bg.setPosition(l.rect.x, l.rect.y);
    bg.setFillColor(sf::Color(8, 14, 18, 245));
    bg.setOutlineColor(sf::Color(120, 200, 255, 160));
    bg.setOutlineThickness(1.3f);
    window.draw(bg);

    sf::RectangleShape titleBar({l.rect.w, 34.0f});
    titleBar.setPosition(l.rect.x, l.rect.y);
    titleBar.setFillColor(sf::Color(16, 34, 50, 245));
    window.draw(titleBar);

    text(window, "沙盒面板", l.rect.x + 16.0f, l.rect.y + 7.0f, 18, sf::Color::White);
    text(window, "战场大小 · 显示模式", l.rect.x + 110.0f, l.rect.y + 11.0f, 11,
         withAlpha(accent, 190));

    {
        sf::VertexArray x(sf::Lines, 4);
        x[0] = sf::Vertex({l.close.x + 6.0f, l.close.y + 6.0f}, sf::Color(230, 230, 230));
        x[1] = sf::Vertex({l.close.x + 19.0f, l.close.y + 19.0f}, sf::Color(230, 230, 230));
        x[2] = sf::Vertex({l.close.x + 19.0f, l.close.y + 6.0f}, sf::Color(230, 230, 230));
        x[3] = sf::Vertex({l.close.x + 6.0f, l.close.y + 19.0f}, sf::Color(230, 230, 230));
        window.draw(x);
    }

    text(window, "战场大小（面积倍率）", l.rect.x + 16.0f, l.rect.y + 46.0f, 12, dim);
    for (int i = 0; i < 4; ++i) {
        const bool selected = std::abs(world.worldWidth() - sizes[i][0]) < 1.0f;
        const MonitorRect& r = l.sizeBtn[i];
        sf::RectangleShape b({r.w, r.h});
        b.setPosition(r.x, r.y);
        b.setFillColor(selected ? withAlpha(accent, 205) : sf::Color(24, 34, 42, 240));
        b.setOutlineColor(selected ? withAlpha(accent, 235) : sf::Color(80, 100, 110, 120));
        b.setOutlineThickness(selected ? 1.6f : 1.0f);
        window.draw(b);
        textCentered(window, sizeLabels[i], r.x + r.w * 0.5f, r.y + 5.0f, 16,
                     selected ? sf::Color(8, 20, 28) : sf::Color::White);
        textCentered(window, sizeDims[i], r.x + r.w * 0.5f, r.y + 30.0f, 9,
                     selected ? sf::Color(20, 40, 50) : withAlpha(dim, 190));
    }

    std::ostringstream info;
    info << "当前战场 " << static_cast<int>(world.worldWidth()) << "×"
         << static_cast<int>(world.worldHeight()) << "   飞船 " << world.activeShipCount
         << "   小行星 " << world.activeAsteroidCount << "   传送门 " << world.activePortalCount;
    text(window, info.str(), l.rect.x + 16.0f, l.rect.y + 130.0f, 11, withAlpha(dim, 210));

    text(window, "显示模式（F11 循环）", l.rect.x + 16.0f, l.rect.y + 148.0f, 12, dim);
    for (int i = 0; i < 3; ++i) {
        const bool selected = overlay.windowMode == i;
        const MonitorRect& r = l.modeBtn[i];
        sf::RectangleShape b({r.w, r.h});
        b.setPosition(r.x, r.y);
        b.setFillColor(selected ? withAlpha(warm, 205) : sf::Color(24, 34, 42, 240));
        b.setOutlineColor(selected ? withAlpha(warm, 235) : sf::Color(80, 100, 110, 120));
        b.setOutlineThickness(selected ? 1.6f : 1.0f);
        window.draw(b);
        textCentered(window, modeLabels[i], r.x + r.w * 0.5f, r.y + 13.0f, 12,
                     selected ? sf::Color(30, 20, 8) : sf::Color(220, 228, 235));
    }

    text(window, "改战场会把现有对象按比例重排并自动缩放镜头；镜头始终显示整张地图。",
         l.rect.x + 16.0f, l.rect.y + 224.0f, 10, withAlpha(dim, 190));
    text(window, "全屏覆盖整个显示器；无边框全屏按工作区（任务栏可见）铺满。",
         l.rect.x + 16.0f, l.rect.y + 240.0f, 10, withAlpha(dim, 170));

    {
        const MonitorRect& r = l.restartBtn;
        sf::RectangleShape b({r.w, r.h});
        b.setPosition(r.x, r.y);
        b.setFillColor(sf::Color(36, 90, 70, 235));
        b.setOutlineColor(sf::Color(90, 190, 140, 170));
        b.setOutlineThickness(1.0f);
        window.draw(b);
        textCentered(window, "按当前战场重开", r.x + r.w * 0.5f, r.y + 9.0f, 12,
                     sf::Color(225, 240, 230));
    }
    text(window, "B 开关沙盒面板 · Esc 关闭", l.rect.x + 244.0f, l.rect.y + kSandboxH - 38.0f,
         10, withAlpha(dim, 150));
}



