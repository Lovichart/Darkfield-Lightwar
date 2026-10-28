#include "Common.h"
#include "Game.h"
#include "World.h"

#include <chrono>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <string>
#include <vector>

#ifdef _WIN32
// 双显卡笔记本：请求 NVIDIA / AMD 驱动使用独显
extern "C" {
__declspec(dllexport) unsigned long NvOptimusEnablement = 0x00000001;
__declspec(dllexport) int AmdPowerXpressRequestHighPerformance = 1;
}
#endif

namespace {

struct BenchOptions {
    int totalShips = 400;
    float seconds = 8.0f;
    int asteroids = 200;
    uint32_t seed = 12345u;
    bool friendlyCollision = true;
    bool friendlyFire = true;
};

int runBenchmark(const BenchOptions& opt) {
    const int perFaction = std::max(1, (opt.totalShips + 3) / 4);
    const int actualShips = perFaction * 4;

    World world(opt.seed);
    world.reset(opt.seed, perFaction);
    world.friendlyCollisionEnabled = opt.friendlyCollision;
    world.friendlyFireEnabled = opt.friendlyFire;

    Rng rng(opt.seed ^ 0x9e3779b9u);
    const int outside = std::max(0, opt.asteroids / 2);
    const int inside = std::max(0, opt.asteroids - outside);
    world.populateAsteroids(outside);
    for (int i = 0; i < inside; ++i) {
        const Vec2 p(rng.f(40.0f, WINDOW_WIDTH - 40.0f), rng.f(40.0f, WINDOW_HEIGHT - 40.0f));
        const float a = rng.f(0.0f, TAU);
        const Vec2 v(std::cos(a) * rng.f(3.0f, 18.0f), std::sin(a) * rng.f(3.0f, 18.0f));
        world.spawnAsteroid(p, v, rng.f(12.0f, 55.0f), AsteroidType::Normal);
    }
    // 基准测试强制混战：否则四个阵营各占一角，前十几秒根本没有交火
    world.scrambleFleets();

    // 预热，避免第一帧的缓存/分配影响平均值
    for (int i = 0; i < 60; ++i) world.step(FIXED_DT);

    const int steps = std::max(1, static_cast<int>(opt.seconds * 60.0f));
    std::vector<double> samples;
    samples.reserve(static_cast<size_t>(steps));
    world.profile = true;
    world.phaseMs.fill(0.0);
    world.collShipShipMs = 0.0;
    world.collShipAstMs = 0.0;
    world.collAstAstMs = 0.0;
    world.collShipShipPairs = 0;
    world.collShipAstPairs = 0;
    world.collAstAstPairs = 0;
    const auto t0 = std::chrono::steady_clock::now();
    for (int i = 0; i < steps; ++i) {
        const auto a = std::chrono::steady_clock::now();
        world.step(FIXED_DT);
        const auto b = std::chrono::steady_clock::now();
        samples.push_back(std::chrono::duration<double, std::milli>(b - a).count());
    }
    const auto t1 = std::chrono::steady_clock::now();
    const double totalMs = std::chrono::duration<double, std::milli>(t1 - t0).count();
    const double avgMs = totalMs / static_cast<double>(steps);

    std::sort(samples.begin(), samples.end());
    const size_t p95i = std::min<size_t>(samples.size() - 1,
                                        static_cast<size_t>(static_cast<double>(samples.size()) * 0.95));
    const size_t p99i = std::min<size_t>(samples.size() - 1,
                                        static_cast<size_t>(static_cast<double>(samples.size()) * 0.99));
    const double p95 = samples[p95i];
    const double p99 = samples[p99i];

    std::printf("ships=%d (per_faction=%d) asteroids=%d steps=%d\n",
                actualShips, perFaction, opt.asteroids, steps);
    std::printf("avg_ms=%.3f p95_ms=%.3f p99_ms=%.3f max_ms=%.3f\n",
                avgMs, p95, p99, samples.back());
    std::printf("sim_steps_per_sec=%.0f  final_ships=%d final_asteroids=%d final_lights=%d final_beams=%d final_stopped=%d\n",
                1000.0 / std::max(0.001, avgMs), world.activeShipCount, world.activeAsteroidCount,
                world.activeLightCount, world.activeBeamCount, world.activeStoppedCount);

    static const char* phaseNames[12] = {
        "ships", "asteroids", "lights", "beams", "zones", "grid",
        "collisions", "collect", "building", "rebellion", "trails", "cleanup"
    };
    std::printf("phases_ms:");
    for (int i = 0; i < 12; ++i) {
        std::printf(" %s=%.3f", phaseNames[i], world.phaseMs[static_cast<size_t>(i)] / steps);
    }
    std::printf("\n");
    std::printf("collision_ms: ship_ship=%.3f ship_ast=%.3f ast_ast=%.3f  pairs/step: %llu %llu %llu\n",
                world.collShipShipMs / steps, world.collShipAstMs / steps, world.collAstAstMs / steps,
                static_cast<unsigned long long>(world.collShipShipPairs / steps),
                static_cast<unsigned long long>(world.collShipAstPairs / steps),
                static_cast<unsigned long long>(world.collAstAstPairs / steps));

    std::ofstream out("bench_results.txt", std::ios::app);
    if (out.is_open()) {
        out << "ships=" << actualShips << " asteroids=" << opt.asteroids
            << " steps=" << steps << " avg_ms=" << avgMs << " p95_ms=" << p95
            << " p99_ms=" << p99 << " max_ms=" << samples.back()
            << " final_ships=" << world.activeShipCount
            << " final_asteroids=" << world.activeAsteroidCount
            << " final_lights=" << world.activeLightCount
            << " final_stopped=" << world.activeStoppedCount << "\n";
    }
    return 0;
}

} // namespace

int main(int argc, char** argv) {
#ifdef _WIN32
    SetProcessDPIAware();
#endif

    int shipsPerFaction = 5;
    uint32_t seed = 12345u;
    bool unlimitedFps = false;
    bool friendlyCollision = true;
    bool friendlyFire = true;
    bool bench = false;
    BenchOptions benchOpt;

    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        auto nextInt = [&](int def) {
            if (i + 1 < argc) return std::atoi(argv[++i]);
            return def;
        };
        auto nextFloat = [&](float def) {
            if (i + 1 < argc) return static_cast<float>(std::atof(argv[++i]));
            return def;
        };

        if (arg == "--ships") shipsPerFaction = std::max(1, nextInt(shipsPerFaction));
        else if (arg == "--seed") seed = static_cast<uint32_t>(std::max(1, nextInt(static_cast<int>(seed))));
        else if (arg == "--unlimited") unlimitedFps = true;
        else if (arg == "--friendly-collision") friendlyCollision = true;
        else if (arg == "--no-friendly-collision") friendlyCollision = false;
        else if (arg == "--friendly-fire") friendlyFire = true;
        else if (arg == "--no-friendly-fire") friendlyFire = false;
        else if (arg == "--bench") { bench = true; benchOpt.totalShips = std::max(4, nextInt(benchOpt.totalShips)); }
        else if (arg == "--bench-seconds") benchOpt.seconds = std::max(0.5f, nextFloat(benchOpt.seconds));
        else if (arg == "--bench-asteroids") benchOpt.asteroids = std::max(0, nextInt(benchOpt.asteroids));
        else if (arg == "--bench-seed") benchOpt.seed = static_cast<uint32_t>(std::max(1, nextInt(static_cast<int>(benchOpt.seed))));
        else if (arg == "--help" || arg == "-h") {
            std::printf("Darkfield Lightwar\n");
            std::printf("  --ships N            每阵营初始飞船数（默认 5，上限 5000）\n");
            std::printf("  --seed N             随机种子\n");
            std::printf("  --unlimited          不限制帧率\n");
            std::printf("  --friendly-collision 开局开启友军碰撞（默认开；游戏内 C 切换）\n");
            std::printf("  --no-friendly-collision 关闭友军碰撞\n");
            std::printf("  --no-friendly-fire   关闭友军伤害（默认开；游戏内 F 切换）\n");
            std::printf("  --bench N            无窗口基准测试：总飞船数\n");
            std::printf("  --bench-seconds S    基准测试时长（默认 8 秒）\n");
            std::printf("  --bench-asteroids N  基准测试小行星数（默认 200）\n");
            return 0;
        }
    }

    if (bench) {
        benchOpt.friendlyCollision = friendlyCollision;
        benchOpt.friendlyFire = friendlyFire;
        return runBenchmark(benchOpt);
    }

#ifdef _WIN32
    if (HWND console = GetConsoleWindow()) ShowWindow(console, SW_HIDE);
#endif

    Game game(shipsPerFaction, seed, unlimitedFps, friendlyCollision, friendlyFire);
    return game.run();
}
