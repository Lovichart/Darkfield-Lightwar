// Darkfield Lightwar - 公共类型 / 常量 / 工具
#pragma once

#ifdef _WIN32
#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

#include <SFML/Graphics.hpp>
#include <SFML/Window.hpp>
#include <SFML/System.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <limits>
#include <memory>
#include <random>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

constexpr float PI = 3.14159265358979323846f;
constexpr float TAU = 2.0f * PI;
constexpr float FIXED_DT = 1.0f / 60.0f;

constexpr int WINDOW_WIDTH = 1600;
constexpr int WINDOW_HEIGHT = 900;
// 战场最大尺寸（9 倍战场 = 长宽各 3 倍）。网格一次性按最大尺寸初始化，
// 切换 0.25/1/4/9 倍时只改世界边界和对象坐标，不重建网格。
constexpr int BATTLE_MAX_WIDTH = 4800;
constexpr int BATTLE_MAX_HEIGHT = 2700;
constexpr int WORLD_WIDTH = 2400;
constexpr int WORLD_HEIGHT = 1700;

constexpr int MAX_FACTIONS = 4;

constexpr float RESOURCE_TO_BUILD = 10.0f;
constexpr float BUILD_TIME = 10.0f;
constexpr float ZONE_INTERVAL_MIN = 6.0f;
constexpr float ZONE_INTERVAL_MAX = 30.0f;
constexpr float ZONE_DURATION = 16.0f;
// 小行星从窗口外缘生成；飞出这个余量就回收，避免屏幕外的天体永久占满槽位。
constexpr float ASTEROID_EDGE_OFFSET_MIN = 30.0f;
constexpr float ASTEROID_EDGE_OFFSET_MAX = 130.0f;
constexpr float ASTEROID_DESPAWN_MARGIN = 180.0f;
constexpr float COLLECTION_DISTANCE = 12.0f;

// 硬上限：防止长时间运行/极端参数把内存和帧时间拖垮
constexpr int MAX_ASTEROIDS = 360;
constexpr int MAX_LIGHT_POINTS = 12000;
constexpr int MAX_BEAMS = 4000;
constexpr int MAX_TRAIL_PARTICLES = 16000;
constexpr int MAX_ZONES = 8;
constexpr int MAX_PORTALS = 8;

struct Vec2 {
    float x{0}, y{0};
    Vec2() = default;
    constexpr Vec2(float x_, float y_) : x(x_), y(y_) {}

    Vec2 operator+(const Vec2& o) const { return {x + o.x, y + o.y}; }
    Vec2 operator-(const Vec2& o) const { return {x - o.x, y - o.y}; }
    Vec2 operator*(float s) const { return {x * s, y * s}; }
    Vec2 operator/(float s) const { return {x / s, y / s}; }
    Vec2 operator-() const { return {-x, -y}; }
    Vec2& operator+=(const Vec2& o) { x += o.x; y += o.y; return *this; }
    Vec2& operator-=(const Vec2& o) { x -= o.x; y -= o.y; return *this; }
    Vec2& operator*=(float s) { x *= s; y *= s; return *this; }

    float dot(const Vec2& o) const { return x * o.x + y * o.y; }
    float cross(const Vec2& o) const { return x * o.y - y * o.x; }
    float lengthSq() const { return x * x + y * y; }
    float length() const { return std::sqrt(lengthSq()); }

    Vec2 normalized() const {
        const float len = length();
        return len > 1e-6f ? Vec2{x / len, y / len} : Vec2{};
    }
    Vec2 rotated(float a) const {
        const float c = std::cos(a), s = std::sin(a);
        return {x * c - y * s, x * s + y * c};
    }
    Vec2 perpendicular() const { return {-y, x}; }
    float angle() const { return std::atan2(y, x); }
    sf::Vector2f sf() const { return {x, y}; }

    static float angleDiff(float a, float b) {
        float d = b - a;
        while (d > PI) d -= TAU;
        while (d < -PI) d += TAU;
        return d;
    }
};

inline Vec2 fromSf(sf::Vector2f v) { return {v.x, v.y}; }
inline float clampf(float v, float lo, float hi) { return v < lo ? lo : (v > hi ? hi : v); }
inline float lerpf(float a, float b, float t) { return a + (b - a) * t; }
inline float randf(std::mt19937& rng, float a, float b) {
    return std::uniform_real_distribution<float>(a, b)(rng);
}
inline int randi(std::mt19937& rng, int a, int b) {
    return std::uniform_int_distribution<int>(a, b)(rng);
}

inline sf::Color factionColor(int faction) {
    switch (faction) {
    case 0: return sf::Color(255, 50, 50);
    case 1: return sf::Color(50, 150, 255);
    case 2: return sf::Color(50, 255, 100);
    case 3: return sf::Color(255, 255, 50);
    default: return sf::Color(200, 200, 200);
    }
}

inline sf::Color colorScale(sf::Color c, float f, sf::Uint8 alpha = 255) {
    auto ch = [f](sf::Uint8 v) {
        return static_cast<sf::Uint8>(clampf(static_cast<float>(v) * f, 0.0f, 255.0f));
    };
    return {ch(c.r), ch(c.g), ch(c.b), alpha};
}

// 资源路径：优先 exe 目录，其次当前工作目录
std::filesystem::path exeDirectory();
std::filesystem::path assetPath(const std::string& relative);
std::vector<std::string> readLines(const std::filesystem::path& path);

// 固定种子 + 可复现的随机数
struct Rng {
    std::mt19937 engine;
    explicit Rng(uint32_t seed = 12345u) : engine(seed) {}
    float f(float a = 0.0f, float b = 1.0f) { return randf(engine, a, b); }
    int i(int a, int b) { return randi(engine, a, b); }
    bool chance(float p) { return f() < p; }
    void reseed(uint32_t seed) { engine.seed(seed); }
};
