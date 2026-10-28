#include "World.h"

#include <chrono>
#include <iomanip>
#include <sstream>
#include <cstring>

namespace {
constexpr float SHIP_RADIUS = 5.0f;
constexpr float SHIP_MASS = 3.0f;

float standardSpeedOf(const Ship& s) {
    const float maneuverFactor = 0.5f + 0.5f * (s.maneuverability / 2.0f);
    const float visionFactor = 0.5f + 0.5f * (s.viewRange / 300.0f);
    const float speedFactor = 0.3f + 0.7f * (s.maxSpeed / 200.0f);
    return s.maxSpeed * 0.6f * maneuverFactor * visionFactor * speedFactor;
}

float obstacleRangeOf(const Ship& s) {
    return s.viewRange * 0.7f + 50.0f * s.maneuverability;
}

// 射线与圆求交：返回沿 dir（单位向量）的最近交点距离，-1 表示不相交。
// 用于激光被路径上的飞船/小行星/光点拦截。
float rayCircleHit(Vec2 origin, Vec2 dir, Vec2 center, float radius) {
    const Vec2 oc = center - origin;
    const float b = oc.dot(dir);
    const float c = oc.lengthSq() - radius * radius;
    if (c <= 0.0f) return 0.0f; // 起点已经在圆内
    if (b < 0.0f) return -1.0f;
    const float disc = b * b - c;
    if (disc < 0.0f) return -1.0f;
    return b - std::sqrt(disc);
}

const char* stateName(ShipState s) {
    switch (s) {
    case ShipState::Attacking: return "攻击";
    case ShipState::Collecting: return "收集";
    case ShipState::Mining: return "采矿";
    case ShipState::Fleeing: return "逃跑";
    case ShipState::AttackingPortal: return "攻击传送门";
    default: return "空闲";
    }
}

const char* zoneName(ZoneType t) {
    switch (t) {
    case ZoneType::Reinforce: return "增援";
    case ZoneType::Survival: return "生存";
    case ZoneType::Attack: return "进攻";
    case ZoneType::Mobility: return "机动";
    case ZoneType::Production: return "生产";
    default: return "领域";
    }
}

sf::Color zoneColor(ZoneType t) {
    switch (t) {
    case ZoneType::Reinforce: return sf::Color(235, 245, 255);
    case ZoneType::Survival: return sf::Color(255, 100, 100);
    case ZoneType::Attack: return sf::Color(255, 255, 100);
    case ZoneType::Mobility: return sf::Color(100, 255, 100);
    case ZoneType::Production: return sf::Color(100, 150, 255);
    default: return sf::Color(200, 100, 255);
    }
}
} // namespace

World::World(uint32_t seedValue) : seed(seedValue), rng_(seedValue) {
    ships_.reserve(24576);
    asteroids_.reserve(MAX_ASTEROIDS + 64);
    lights_.reserve(MAX_LIGHT_POINTS + 64);
    beams_.reserve(MAX_BEAMS);
    trails_.reserve(MAX_TRAIL_PARTICLES);
    portals_.reserve(MAX_PORTALS);
    erodeScratch_.reserve(4096);
    shipRequests_.reserve(128);

    shipGrid_.init(static_cast<float>(BATTLE_MAX_WIDTH), static_cast<float>(BATTLE_MAX_HEIGHT), 100.0f);
    for (auto& g : collisionGrids_) {
        g.init(static_cast<float>(BATTLE_MAX_WIDTH), static_cast<float>(BATTLE_MAX_HEIGHT), 24.0f);
    }
    astGrid_.init(static_cast<float>(BATTLE_MAX_WIDTH), static_cast<float>(BATTLE_MAX_HEIGHT), 100.0f);
    lightGrid_.init(static_cast<float>(BATTLE_MAX_WIDTH), static_cast<float>(BATTLE_MAX_HEIGHT), 100.0f);

    loadNames();
    reset(seedValue, 5);
}

void World::restart() {
    reset(seed, initialShipsPerFaction_);
}

void World::setWorldSize(float width, float height) {
    width = clampf(width, 400.0f, static_cast<float>(BATTLE_MAX_WIDTH));
    height = clampf(height, 225.0f, static_cast<float>(BATTLE_MAX_HEIGHT));
    if (std::abs(width - worldWidth_) < 0.5f && std::abs(height - worldHeight_) < 0.5f) {
        return;
    }
    const float sx = width / std::max(1.0f, worldWidth_);
    const float sy = height / std::max(1.0f, worldHeight_);
    worldWidth_ = width;
    worldHeight_ = height;

    const auto scalePos = [&](Vec2& p) {
        p.x *= sx;
        p.y *= sy;
    };

    for (Ship& s : ships_) {
        if (!s.active) continue;
        scalePos(s.pos);
        scalePos(s.threatPos);
        for (int i = 0; i < s.obstacleCount; ++i) scalePos(s.obstacles[i].pos);
        if (!s.entering) {
            s.pos.x = clampf(s.pos.x, SHIP_RADIUS, worldWidth_ - SHIP_RADIUS);
            s.pos.y = clampf(s.pos.y, SHIP_RADIUS, worldHeight_ - SHIP_RADIUS);
        }
    }
    for (Asteroid& a : asteroids_) {
        if (!a.active) continue;
        scalePos(a.pos);
        for (int i = 0; i < a.trailCount; ++i) scalePos(a.trail[i]);
        a.pos.x = clampf(a.pos.x, 0.0f, worldWidth_);
        a.pos.y = clampf(a.pos.y, 0.0f, worldHeight_);
    }
    for (LightPoint& l : lights_) {
        if (!l.active) continue;
        scalePos(l.pos);
    }
    for (Beam& b : beams_) {
        scalePos(b.a);
        scalePos(b.b);
    }
    for (Zone& z : zones_) {
        if (!z.active) continue;
        scalePos(z.pos);
    }
    for (Portal& p : portals_) {
        if (!p.active) continue;
        scalePos(p.pos);
    }
    for (TrailParticle& t : trails_) scalePos(t.pos);
    for (ShipRequest& r : shipRequests_) scalePos(r.pos);

    rebuildGrids();
}

void World::loadNames() {
    // 名称池去重：扩充 names/*.txt 后可能有重复项，随机抽到重复名字会很显眼。
    auto dedupe = [](std::vector<std::string>& v) {
        std::sort(v.begin(), v.end());
        v.erase(std::unique(v.begin(), v.end()), v.end());
    };

    const char* files[4] = {
        "names/camp_red.txt",
        "names/camp_blue.txt",
        "names/camp_green.txt",
        "names/camp_yellow.txt"
    };
    const char* fallback[4] = { "赤焰", "苍蓝", "翠影", "流光" };
    for (int f = 0; f < 4; ++f) {
        auto lines = readLines(assetPath(files[f]));
        dedupe(lines);
        factionNamePools_[static_cast<size_t>(f)] = lines;
        if (!lines.empty()) factionNames_[f] = lines[rng_.i(0, static_cast<int>(lines.size()) - 1)];
        else factionNames_[f] = fallback[f];
    }

    titles_ = readLines(assetPath("names/ship.txt"));
    dedupe(titles_);
    if (titles_.empty()) {
        titles_ = { "新星", "游侠", "猎手", "守望者", "破晓", "长夜" };
    }
}

std::string World::randomTitle() {
    if (titles_.empty()) return {};
    return titles_[rng_.i(0, static_cast<int>(titles_.size()) - 1)];
}

std::string World::chooseFactionName(int faction, const std::string& avoid) {
    static const char* fallback[4] = { "赤焰", "苍蓝", "翠影", "流光" };
    const std::vector<std::string>& pool = factionNamePools_[static_cast<size_t>(faction)];
    if (pool.empty()) {
        std::string name = fallback[std::clamp(faction, 0, 3)];
        if (name == avoid) name += "·新";
        return name;
    }
    for (int attempt = 0; attempt < 64; ++attempt) {
        const std::string& candidate = pool[static_cast<size_t>(
            rng_.i(0, static_cast<int>(pool.size()) - 1))];
        if (!avoid.empty() && candidate == avoid) continue;
        bool used = false;
        for (int f = 0; f < MAX_FACTIONS; ++f) {
            if (f != faction && factionAlive_[static_cast<size_t>(f)] &&
                factionNames_[static_cast<size_t>(f)] == candidate) {
                used = true;
                break;
            }
        }
        if (!used) return candidate;
    }
    return pool[static_cast<size_t>(rng_.i(0, static_cast<int>(pool.size()) - 1))];
}

void World::assignFactionIdentity(int faction, bool resetStats) {
    if (faction < 0 || faction >= MAX_FACTIONS) return;
    const std::string oldName = factionNames_[static_cast<size_t>(faction)];
    factionNames_[static_cast<size_t>(faction)] = chooseFactionName(faction, oldName);
    factionIds_[static_cast<size_t>(faction)] = nextFactionId_++;
    factionAlive_[static_cast<size_t>(faction)] = true;

    if (resetStats) {
        factionTotals_[static_cast<size_t>(faction)].fill(0.0);
        factionStats_[static_cast<size_t>(faction)] = FactionStatHistory{};
        stats_.factions[static_cast<size_t>(faction)] = FactionStatRow{};
    }
    FactionStatRow& row = stats_.factions[static_cast<size_t>(faction)];
    row.name = factionNames_[static_cast<size_t>(faction)];
    row.id = factionIds_[static_cast<size_t>(faction)];
    row.colorSlot = faction;
    row.alive = true;
    row.eliminated = false;
}

std::string World::factionDisplayName(int faction) const {
    if (faction < 0 || faction >= MAX_FACTIONS) return {};
    return factionNames_[static_cast<size_t>(faction)] + " #" +
           std::to_string(factionIds_[static_cast<size_t>(faction)]);
}

uint64_t World::factionId(int faction) const {
    if (faction < 0 || faction >= MAX_FACTIONS) return 0;
    return factionIds_[static_cast<size_t>(faction)];
}

void World::reset(uint32_t seedValue, int shipsPerFaction) {
    seed = seedValue;
    rng_.reseed(seedValue);

    ships_.clear();
    freeShips_.clear();
    asteroids_.clear();
    freeAsteroids_.clear();
    lights_.clear();
    freeLights_.clear();
    beams_.clear();
    zones_.clear();
    portals_.clear();
    trails_.clear();
    shipRequests_.clear();
    erodeScratch_.clear();

    resources_.fill(0.0f);
    for (auto& row : factionTotals_) row.fill(0.0);
    for (auto& h : factionStats_) h = FactionStatHistory{};
    shipStats_.clear();
    stats_ = StatsSnapshot{};
    rankCaches_.clear();
    eliminatedFactions_.clear();
    deadShips_.clear();
    nextFactionId_ = 1;
    for (auto& name : factionNames_) name.clear();
    for (int f = 0; f < MAX_FACTIONS; ++f) assignFactionIdentity(f, true);
    nextShipSerial_ = 1;
    statsRefreshTimer_ = 0.0f;
    statsBucketTimer_ = 0.0f;

    selectedShip_ = -1;
    selectedGen_ = 0;
    stepCount = 0;
    activeShipCount = 0;
    activeAsteroidCount = 0;
    activeLightCount = 0;
    activeBeamCount = 0;
    activeTrailCount = 0;
    activeZoneCount = 0;
    activePortalCount = 0;
    activeStoppedCount = 0;

    asteroidTimer_ = 0.0f;
    waveTimer_ = 0.0f;
    waveCount_ = 0;
    zoneTimer_ = 0.0f;
    nextZoneTime_ = 6.0f;
    rebellionTimer_ = 0.0f;

    initialShipsPerFaction_ = std::clamp(shipsPerFaction, 1, 5000);
    shipCap = std::max(20000, initialShipsPerFaction_ * 4 + 512);

    const int per = initialShipsPerFaction_;
    const int cols = std::clamp(static_cast<int>(std::ceil(std::sqrt(static_cast<float>(per) * 3.0f))), 5, 100);
    const float spacing = std::min(16.0f, std::max(6.0f, 420.0f / static_cast<float>(cols)));
    const Vec2 corners[4] = {
        {70.0f, 70.0f},
        {worldWidth_ - 70.0f, 70.0f},
        {70.0f, worldHeight_ - 70.0f},
        {worldWidth_ - 70.0f, worldHeight_ - 70.0f}
    };
    const Vec2 inward[4] = {
        {1, 1}, {-1, 1}, {1, -1}, {-1, -1}
    };

    for (int f = 0; f < 4; ++f) {
        const Vec2 dir = inward[f].normalized();
        const Vec2 side = dir.perpendicular();
        for (int i = 0; i < per; ++i) {
            const int row = i / cols;
            const int col = i % cols;
            Vec2 p = corners[f] + dir * (static_cast<float>(row) * spacing) +
                     side * ((static_cast<float>(col) - (cols - 1) * 0.5f) * spacing);
            p.x = clampf(p.x, 20.0f, worldWidth_ - 20.0f);
            p.y = clampf(p.y, 20.0f, worldHeight_ - 20.0f);
            const uint32_t idx = spawnShip(p, f, nullptr);
            if (idx == UINT32_MAX) break;
            Ship& s = ships_[idx];
            const Vec2 toCenter = Vec2(worldWidth_ * 0.5f, worldHeight_ * 0.5f) - p;
            s.rot = toCenter.angle();
            s.vel = toCenter.normalized() * (12.0f + rng_.f(0.0f, 6.0f));
        }
    }

    // 开局给几颗场内小行星，避免前 16 秒空场（也便于观察采矿 / 光点链路）
    for (int i = 0; i < 6; ++i) {
        const Vec2 p(rng_.f(worldWidth_ * 0.26f, worldWidth_ * 0.74f),
                     rng_.f(worldHeight_ * 0.29f, worldHeight_ * 0.71f));
        const float a = rng_.f(0.0f, TAU);
        const Vec2 v(std::cos(a) * rng_.f(3.0f, 12.0f), std::sin(a) * rng_.f(3.0f, 12.0f));
        spawnAsteroid(p, v, rng_.f(14.0f, 42.0f), AsteroidType::Normal);
    }
    asteroidTimer_ = 12.0f;

    rebuildGrids();
    refreshStats();
}

uint32_t World::spawnShip(Vec2 pos, int faction, const Ship* copyFrom) {
    if (ships_.size() >= static_cast<size_t>(shipCap) && freeShips_.empty()) return UINT32_MAX;

    uint32_t idx = UINT32_MAX;
    if (!freeShips_.empty()) {
        idx = freeShips_.back();
        freeShips_.pop_back();
        const uint32_t nextGen = ships_[idx].gen + 1u;
        ships_[idx] = Ship{};
        ships_[idx].gen = nextGen;
    } else {
        if (ships_.size() >= ships_.capacity()) {
            // 超过预留容量时不再扩容，保证已有引用稳定
            return UINT32_MAX;
        }
        idx = static_cast<uint32_t>(ships_.size());
        ships_.emplace_back();
        ships_[idx].gen = 1u;
    }

    Ship& s = ships_[idx];
    s.active = true;
    s.deathHandled = false;
    s.pos = pos;
    s.faction = faction;
    s.color = factionColor(faction);
    s.mass = SHIP_MASS;
    s.preferredSpeed = rng_.f(15.0f, 25.0f);
    s.drivingSkill = rng_.f(0.2f, 0.6f);
    s.standardSpeed = standardSpeedOf(s);
    s.rot = rng_.f(0.0f, TAU);
    s.decisionTimer = rng_.f(0.0f, 0.5f);
    s.obstacleTimer = rng_.f(0.0f, 0.12f);

    if (copyFrom) {
        copyAttributes(s, *copyFrom);
        s.pos = pos;
        s.faction = faction;
        s.color = copyFrom->color;
        s.active = true;
        s.deathHandled = false;
    }
    s.serial = nextShipSerial_++;
    ensureShipStatSlot(idx, s.serial);
    return idx;
}

void World::copyAttributes(Ship& dst, const Ship& src) {
    dst.maxHealth = src.maxHealth;
    dst.health = src.maxHealth;
    dst.fireRate = src.fireRate;
    dst.fireRange = src.fireRange;
    dst.damage = src.damage;
    dst.fireAngle = src.fireAngle;
    dst.maneuverability = src.maneuverability;
    dst.maxSpeed = src.maxSpeed;
    dst.accelerationRate = src.accelerationRate;
    dst.brakeRate = src.brakeRate;
    dst.recoveryRate = src.recoveryRate;
    dst.miningExpertise = src.miningExpertise;
    dst.maxArmor = src.maxArmor;
    dst.armor = src.maxArmor;
    dst.armorRegen = src.armorRegen;
    dst.viewRange = src.viewRange;
    dst.buildSpeed = src.buildSpeed;
    dst.level = src.level;
    dst.upgradeCooldown = 0.0f;
    // 不能无夹取地复制：建造链上随机游走会把 preferredSpeed 推到 0 或负数。
    // 建造船的 drivingSkill=0，targetSpeed 基本就等于 preferredSpeed；一旦 <=0，
    // updateSpeedControl 的推进条件永远不成立，船会永久停在原地（后期常见）。
    dst.preferredSpeed = clampf(src.preferredSpeed + rng_.f(-3.0f, 3.0f), 15.0f, 30.0f);
    dst.drivingSkill = 0.0f;
    dst.standardSpeed = standardSpeedOf(dst);
    dst.kills = 0;
    dst.title.clear();
    dst.building = false;
    dst.buildProgress = 0.0f;
    dst.state = ShipState::Idle;
    dst.hitCooldown = 0.0f;
    dst.fireTimer = 0.0f;
}

uint32_t World::spawnAsteroid(Vec2 pos, Vec2 vel, float radius, AsteroidType type,
                              float irregularity) {
    if (asteroids_.size() >= static_cast<size_t>(MAX_ASTEROIDS) && freeAsteroids_.empty()) return UINT32_MAX;

    uint32_t idx = UINT32_MAX;
    if (!freeAsteroids_.empty()) {
        idx = freeAsteroids_.back();
        freeAsteroids_.pop_back();
        const uint32_t nextGen = asteroids_[idx].gen + 1u;
        asteroids_[idx] = Asteroid{};
        asteroids_[idx].gen = nextGen;
    } else {
        if (asteroids_.size() >= asteroids_.capacity()) return UINT32_MAX;
        idx = static_cast<uint32_t>(asteroids_.size());
        asteroids_.emplace_back();
        asteroids_[idx].gen = 1u;
    }

    Asteroid& a = asteroids_[idx];
    a.active = true;
    a.deathHandled = false;
    a.pos = pos;
    a.vel = vel;
    a.type = type;
    // 小行星图集单元是 72px（纹理最大 67px），所以 gridN 不能超过 33。
    // 要做“非常大”的石头时把 pixelSize 放大，保持 33x33 的像素轮廓。
    constexpr int kMaxGridN = 33;
    a.gridN = std::clamp(static_cast<int>(std::lround(radius / 4.0f)), 1, kMaxGridN);
    a.pixelSize = std::max(0.5f, radius / static_cast<float>(a.gridN));
    a.radius = static_cast<float>(a.gridN) * a.pixelSize;
    a.rot = rng_.f(0.0f, TAU);
    a.angVel = rng_.f(-0.5f, 0.5f);
    a.pixels.clear();
    a.pixels.reserve(static_cast<size_t>((a.gridN * 2 + 1) * (a.gridN * 2 + 1)) / 2);
    a.activePixels = 0;
    a.totalPixels = 0;

    const float irr = clampf(irregularity, 0.0f, 1.0f);
    const float phase = rng_.f(0.0f, TAU);
    const float phase2 = rng_.f(0.0f, TAU);
    const int n = a.gridN;
    for (int gy = -n; gy <= n; ++gy) {
        for (int gx = -n; gx <= n; ++gx) {
            const float d = std::sqrt(static_cast<float>(gx * gx + gy * gy)) / static_cast<float>(n);
            if (d > 1.05f) continue;
            const float ang = std::atan2(static_cast<float>(gy), static_cast<float>(gx));
            // 不规则程度 0 时接近圆形；1 时轮廓起伏大，边缘更碎。
            const float edge = (0.98f - 0.22f * irr) +
                               (0.06f + 0.14f * irr) * std::sin(ang * 3.0f + phase) +
                               (0.02f + 0.10f * irr) * std::sin(ang * 7.0f + phase2);
            if (d > edge) continue;

            Pixel p;
            p.gx = static_cast<int16_t>(gx);
            p.gy = static_cast<int16_t>(gy);
            if (type == AsteroidType::Comet) {
                const int v = rng_.i(-12, 12);
                p.r = static_cast<sf::Uint8>(std::clamp(185 + v, 0, 255));
                p.g = static_cast<sf::Uint8>(std::clamp(205 + v, 0, 255));
                p.b = 255;
            } else {
                const int v = rng_.i(-18, 18);
                const int base = rng_.i(105, 165);
                p.r = static_cast<sf::Uint8>(std::clamp(base + v, 60, 220));
                p.g = static_cast<sf::Uint8>(std::clamp(base + v, 60, 220));
                p.b = static_cast<sf::Uint8>(std::clamp(base + v, 60, 220));
            }
            p.active = true;
            a.pixels.push_back(p);
            ++a.activePixels;
        }
    }

    if (a.activePixels < 3) {
        a.pixels.clear();
        a.activePixels = 0;
        const int fallback[5][2] = { {0,0}, {1,0}, {-1,0}, {0,1}, {0,-1} };
        for (const auto& xy : fallback) {
            Pixel p;
            p.gx = static_cast<int16_t>(xy[0]);
            p.gy = static_cast<int16_t>(xy[1]);
            p.r = p.g = p.b = 150;
            p.active = true;
            a.pixels.push_back(p);
            ++a.activePixels;
        }
    }
    a.totalPixels = a.activePixels;

    a.baseMass = a.radius * a.radius * a.radius * 0.01f;
    a.baseHealth = 100.0f * std::pow(a.baseMass, 2.0f / 3.0f);
    a.cushion = 0.3f;
    if (type == AsteroidType::Comet) {
        a.baseMass *= 3.0f;
        a.baseHealth *= 5.0f;
        a.cushion = 0.6f;
    }
    a.mass = a.baseMass;
    a.health = a.baseHealth;
    a.maxHealth = a.baseHealth;
    a.trailCount = 0;
    a.trailTimer = 0.0f;
    a.hitCooldown = 0.0f;
    a.imageDirty = true;
    buildAsteroidImage(a);
    return idx;
}

void World::buildAsteroidImage(Asteroid& a) {
    const int size = a.gridN * 2 + 1;
    a.image.create(static_cast<unsigned>(size), static_cast<unsigned>(size), sf::Color::Transparent);
    for (const Pixel& p : a.pixels) {
        if (!p.active) continue;
        const int x = p.gx + a.gridN;
        const int y = p.gy + a.gridN;
        a.image.setPixel(static_cast<unsigned>(x), static_cast<unsigned>(y), sf::Color(p.r, p.g, p.b, 255));
    }
    a.imageDirty = true;
}

void World::refreshAsteroidImage(Asteroid& a) {
    buildAsteroidImage(a);
}

uint32_t World::spawnLight(Vec2 pos, Vec2 vel, sf::Color color) {
    if (lights_.size() >= static_cast<size_t>(MAX_LIGHT_POINTS) && freeLights_.empty()) return UINT32_MAX;
    uint32_t idx;
    if (!freeLights_.empty()) {
        idx = freeLights_.back();
        freeLights_.pop_back();
        lights_[idx] = LightPoint{};
    } else {
        if (lights_.size() >= lights_.capacity()) return UINT32_MAX;
        idx = static_cast<uint32_t>(lights_.size());
        lights_.emplace_back();
    }
    LightPoint& l = lights_[idx];
    l.active = true;
    l.freed = false;
    l.pos = pos;
    l.vel = vel;
    l.color = color;
    l.radius = 2.0f;
    l.health = 10.0f;
    l.maxHealth = 10.0f;
    l.hitCooldown = 0.0f;
    return idx;
}

void World::addLightBurst(Vec2 pos, sf::Color color, int count, float speed) {
    for (int i = 0; i < count; ++i) {
        const float ang = rng_.f(0.0f, TAU);
        const Vec2 dir(std::cos(ang), std::sin(ang));
        spawnLight(pos + dir * rng_.f(0.0f, 8.0f), dir * rng_.f(4.0f, speed), color);
    }
}

void World::addTrailParticle(Vec2 pos, Vec2 vel, sf::Color color, float size) {
    if (trails_.size() >= static_cast<size_t>(MAX_TRAIL_PARTICLES)) {
        TrailParticle& p = trails_[static_cast<size_t>(rng_.i(0, MAX_TRAIL_PARTICLES - 1))];
        p.pos = pos;
        p.vel = vel;
        p.life = 0.0f;
        p.maxLife = rng_.f(0.7f, 1.1f);
        p.size = size;
        p.color = color;
        return;
    }
    TrailParticle p;
    p.pos = pos;
    p.vel = vel;
    p.life = 0.0f;
    p.maxLife = rng_.f(0.7f, 1.1f);
    p.size = size;
    p.color = color;
    trails_.push_back(p);
}

bool World::isInsideWorld(Vec2 p, float margin) const {
    return p.x >= -margin && p.x <= worldWidth_ + margin &&
           p.y >= -margin && p.y <= worldHeight_ + margin;
}

uint32_t World::spawnRandomAsteroid() {
    // 在可视窗口四边外侧的窄带里生成，朝窗口内部飞：
    // 1) 几秒内就能在屏幕上看到，而不是在 1000px 外慢慢漂；
    // 2) 飞出窗口后由 updateAsteroids 回收，不会把 MAX_ASTEROIDS 个槽位永久占满。
    constexpr float kEdgeInset = 40.0f;
    const float offset = rng_.f(ASTEROID_EDGE_OFFSET_MIN, ASTEROID_EDGE_OFFSET_MAX);
    Vec2 pos;
    switch (rng_.i(0, 3)) {
    case 0: // 上
        pos = { rng_.f(kEdgeInset, worldWidth_ - kEdgeInset),
                -offset };
        break;
    case 1: // 右
        pos = { static_cast<float>(worldWidth_) + offset,
                rng_.f(kEdgeInset, worldHeight_ - kEdgeInset) };
        break;
    case 2: // 下
        pos = { rng_.f(kEdgeInset, worldWidth_ - kEdgeInset),
                static_cast<float>(worldHeight_) + offset };
        break;
    default: // 左
        pos = { -offset,
                rng_.f(kEdgeInset, worldHeight_ - kEdgeInset) };
        break;
    }

    const Vec2 aim(rng_.f(worldWidth_ * 0.15f, worldWidth_ * 0.85f),
                   rng_.f(worldHeight_ * 0.15f, worldHeight_ * 0.85f));
    Vec2 dir = (aim - pos).normalized();
    if (dir.lengthSq() < 1e-4f) dir = { 0.0f, -1.0f };
    dir = dir.rotated(rng_.f(-12.0f, 12.0f) * PI / 180.0f);

    float size = 30.0f;
    float speed = 30.0f;
    AsteroidType type = AsteroidType::Normal;
    const int roll = rng_.i(0, 999);
    if (roll < 945) {
        const int sub = rng_.i(0, 99);
        if (sub < 70) { size = rng_.f(25.0f, 42.0f); speed = rng_.f(30.0f, 55.0f); }
        else if (sub < 85) { size = rng_.f(45.0f, 80.0f); speed = rng_.f(18.0f, 35.0f); }
        else { size = rng_.f(10.0f, 20.0f); speed = rng_.f(45.0f, 80.0f); }
    } else {
        type = AsteroidType::Comet;
        size = rng_.f(80.0f, 120.0f);
        speed = rng_.f(280.0f, 430.0f);
    }

    return spawnAsteroid(pos, dir * speed, size, type);
}

void World::spawnAsteroidWave() {
    const float factor = std::min(5.0f, waveTimer_ / 60.0f);
    int count = static_cast<int>(2.0f + factor);
    if (rng_.chance(0.02f)) count += 2 + rng_.i(0, 2);
    count = std::min(count, 6);
    for (int i = 0; i < count; ++i) spawnRandomAsteroid();
    waveTimer_ = 0.0f;
    ++waveCount_;
}

void World::populateAsteroids(int count) {
    for (int i = 0; i < count; ++i) {
        if (spawnRandomAsteroid() == UINT32_MAX) break;
    }
}

void World::scrambleFleets() {
    for (auto& s : ships_) {
        if (!s.active) continue;
        s.pos = { rng_.f(worldWidth_ * 0.2f, worldWidth_ * 0.8f),
                  rng_.f(worldHeight_ * 0.2f, worldHeight_ * 0.8f) };
        const float a = rng_.f(0.0f, TAU);
        s.rot = a;
        s.vel = Vec2(std::cos(a), std::sin(a)) * rng_.f(10.0f, 60.0f);
        s.decisionTimer = rng_.f(0.0f, 0.4f);
    }
    rebuildGrids();
}

void World::step(float dt) {
    if (dt <= 0.0f) return;
    ++stepCount;

    using Clock = std::chrono::steady_clock;
    auto mark = [&]() { return profile ? Clock::now() : Clock::time_point{}; };
    auto add = [&](int slot, Clock::time_point a) {
        if (profile) phaseMs[slot] += std::chrono::duration<double, std::milli>(Clock::now() - a).count();
    };

    auto t = mark();
    updateShips(dt);
    add(0, t);

    t = mark(); updateAsteroids(dt); add(1, t);
    t = mark(); updateLights(dt); add(2, t);
    t = mark(); updateBeams(dt); add(3, t);
    t = mark(); updateZones(dt); add(4, t);
    t = mark(); updatePortals(dt); add(4, t);

    t = mark();
    rebuildGrids();
    add(5, t);

    t = mark();
    if ((stepCount % 3ull) == 0ull) updateCollisions();
    add(6, t);

    t = mark();
    if ((stepCount & 1ull) == 0ull) {
        for (auto& s : ships_) {
            if (s.active) collectLights(s);
        }
    }
    add(7, t);

    t = mark(); updateBuilding(dt); add(8, t);
    t = mark(); updateRebellion(dt); add(9, t);
    t = mark(); updateTrails(dt); add(10, t);

    t = mark();
    cleanup();
    flushShipRequests();
    add(11, t);

    asteroidTimer_ += dt;
    waveTimer_ += dt;
    const float interval = std::max(6.0f, 16.0f - static_cast<float>(waveCount_) * 0.1f);
    if (asteroidTimer_ >= interval) {
        asteroidTimer_ = 0.0f;
        spawnAsteroidWave();
    }

    activeShipCount = 0;
    activeStoppedCount = 0;
    for (const auto& s : ships_) {
        if (!s.active) continue;
        ++activeShipCount;
        if (s.stillTimer > 3.0f) ++activeStoppedCount;
    }
    activeAsteroidCount = 0;
    for (const auto& a : asteroids_) if (a.active) ++activeAsteroidCount;
    activeLightCount = 0;
    for (const auto& l : lights_) if (l.active) ++activeLightCount;
    activeBeamCount = static_cast<int>(beams_.size());
    activeTrailCount = static_cast<int>(trails_.size());
    activeZoneCount = static_cast<int>(zones_.size());
    activePortalCount = 0;
    for (const auto& p : portals_) if (p.active) ++activePortalCount;

    // 统计：10 秒一个历史桶；快照约 10 Hz（每 6 步）刷新。
    // 快照扫描是 O(活跃船数)，10k 时约 10 万次/秒读取，远低于 AI/碰撞成本。
    statsBucketTimer_ += dt;
    if (statsBucketTimer_ >= 10.0f) {
        statsBucketTimer_ -= 10.0f;
        sampleStatsBuckets();
    }
    statsRefreshTimer_ += dt;
    if (statsRefreshTimer_ >= 0.1f) {
        statsRefreshTimer_ = 0.0f;
        refreshStats();
    }
}

void World::rebuildGrids() {
    shipGrid_.clear();
    for (auto& g : collisionGrids_) g.clear();
    astGrid_.clear();
    lightGrid_.clear();

    for (size_t i = 0; i < ships_.size(); ++i) {
        const Ship& s = ships_[i];
        if (!s.active) continue;
        shipGrid_.insert(static_cast<uint32_t>(i), s.pos, SHIP_RADIUS + 1.0f);
        if (s.faction >= 0 && s.faction < MAX_FACTIONS) {
            collisionGrids_[static_cast<size_t>(s.faction)].insert(static_cast<uint32_t>(i), s.pos, SHIP_RADIUS + 1.0f);
        }
    }
    for (size_t i = 0; i < asteroids_.size(); ++i) {
        const Asteroid& a = asteroids_[i];
        if (a.active) astGrid_.insert(static_cast<uint32_t>(i), a.pos, a.radius);
    }
    for (size_t i = 0; i < lights_.size(); ++i) {
        const LightPoint& l = lights_[i];
        if (l.active) lightGrid_.insert(static_cast<uint32_t>(i), l.pos, l.radius + 1.0f);
    }
}

void World::updateShips(float dt) {
    const size_t n = ships_.size();
    for (size_t i = 0; i < n; ++i) {
        Ship& s = ships_[i];
        if (!s.active) continue;

        s.age += dt;
        s.vel += s.acc * dt;
        s.acc = {};
        const float speed = s.vel.length();
        if (speed > s.maxSpeed && speed > 1e-4f) s.vel = s.vel * (s.maxSpeed / speed);

        s.pos += s.vel * dt;

        // 增援从窗口外飞入：进入窗口前不做边界夹取、不跑 AI，保持直线进场。
        if (s.entering) {
            if (s.pos.x > SHIP_RADIUS && s.pos.x < worldWidth_ - SHIP_RADIUS &&
                s.pos.y > SHIP_RADIUS && s.pos.y < worldHeight_ - SHIP_RADIUS) {
                s.entering = false;
            } else {
                s.rot = s.vel.angle();
                continue;
            }
        }

        const float margin = SHIP_RADIUS;
        if (s.pos.x < margin) { s.pos.x = margin; s.vel.x = std::abs(s.vel.x) * 0.5f; }
        if (s.pos.x > worldWidth_ - margin) { s.pos.x = worldWidth_ - margin; s.vel.x = -std::abs(s.vel.x) * 0.5f; }
        if (s.pos.y < margin) { s.pos.y = margin; s.vel.y = std::abs(s.vel.y) * 0.5f; }
        if (s.pos.y > worldHeight_ - margin) { s.pos.y = worldHeight_ - margin; s.vel.y = -std::abs(s.vel.y) * 0.5f; }

        s.health = std::min(s.maxHealth, s.health + s.recoveryRate * dt);
        s.armor = std::min(s.maxArmor, s.armor + s.maxArmor * s.armorRegen * dt);
        s.hitCooldown = std::max(0.0f, s.hitCooldown - dt);
        s.upgradeCooldown = std::max(0.0f, s.upgradeCooldown - dt);
        s.fireTimer += dt;
        s.decisionTimer -= dt;
        s.obstacleTimer -= dt;
        s.stateTimer += dt;
        s.idleTimer += dt;
        s.alertTimer = std::max(0.0f, s.alertTimer - dt);
        if (s.vel.lengthSq() < 1.0f) s.stillTimer += dt;
        else s.stillTimer = 0.0f;

        if (s.obstacleTimer <= 0.0f) {
            scanObstacles(s);
            s.obstacleTimer = 0.2f + rng_.f(0.0f, 0.1f);
        }

        const Vec2 avoid = obstacleAvoidance(s);
        if (avoid.lengthSq() > 0.01f) s.acc += avoid / s.mass;
        const Vec2 bound = boundaryAvoidance(s);
        if (bound.lengthSq() > 0.01f) s.acc += bound / s.mass;
        if (needsEmergencyBrake(s)) {
            const Vec2 dir = s.vel.normalized();
            s.acc -= dir * (s.brakeRate * 0.8f / s.mass);
        }

        if (s.state == ShipState::Fleeing) {
            // 逃跑是即时反应：不等 0.45–0.9s 的决策计时，威胁消失/计时结束才重新决策。
            const int ti = shipSlotByHandle(s.threatShip, s.threatShipGen);
            if (ti >= 0) s.threatPos = ships_[ti].pos;
            if (ti < 0 || s.alertTimer <= 0.0f) {
                s.alertTimer = 0.0f;
                evaluateState(s);
                s.decisionTimer = 0.0f;
            }
        } else if (s.decisionTimer <= 0.0f) {
            bool keep = false;
            if (s.state == ShipState::Attacking) {
                const int ti = shipSlotByHandle(s.targetShip, s.targetShipGen);
                if (ti >= 0 && ships_[ti].faction != s.faction &&
                    (ships_[ti].pos - s.pos).lengthSq() <= s.viewRange * s.viewRange) {
                    keep = true;
                }
            } else if (s.state == ShipState::AttackingPortal) {
                const int pi = portalSlotByHandle(s.targetPortal, s.targetPortalGen);
                if (pi >= 0 && portals_[pi].faction != s.faction &&
                    (portals_[pi].pos - s.pos).lengthSq() <= s.viewRange * s.viewRange) {
                    keep = true;
                }
            } else if (s.state == ShipState::Collecting) {
                if (s.targetLight < lights_.size() && lights_[s.targetLight].active &&
                    (lights_[s.targetLight].pos - s.pos).lengthSq() <= s.viewRange * s.viewRange) {
                    keep = true;
                }
            } else if (s.state == ShipState::Mining) {
                const int ai = asteroidSlotByHandle(s.targetAst, s.targetAstGen);
                if (ai >= 0 && (asteroids_[ai].pos - s.pos).lengthSq() <= s.viewRange * s.viewRange) {
                    keep = true;
                }
            }
            if (!keep) evaluateState(s);
            s.decisionTimer = 0.45f + rng_.f(0.0f, 0.45f);
        }

        updateSpeedControl(s, dt);

        switch (s.state) {
        case ShipState::Attacking: {
            const int target = shipSlotByHandle(s.targetShip, s.targetShipGen);
            if (target < 0 || !ships_[target].active || ships_[target].faction == s.faction) {
                s.state = ShipState::Idle;
                s.targetShip = UINT32_MAX;
                s.decisionTimer = 0.0f;
                break;
            }
            const Ship& t = ships_[target];
            const Vec2 toTarget = t.pos - s.pos;
            const float dist = toTarget.length();
            if (dist > s.viewRange) { s.state = ShipState::Idle; s.decisionTimer = 0.0f; break; }
            const float targetAngle = toTarget.angle();
            float diff = Vec2::angleDiff(s.rot, targetAngle);
            const float maxRot = s.maneuverability * dt;
            diff = clampf(diff, -maxRot, maxRot);
            s.rot += diff;

            if (std::abs(diff) < s.fireAngle && dist <= s.fireRange) fireAtShip(static_cast<int>(i), target);
            const Vec2 dir = Vec2(1.0f, 0.0f).rotated(s.rot);
            s.acc += dir * (s.accelerationRate * 0.5f / s.mass);
            if (dist < s.fireRange * 0.45f) {
                s.acc -= s.vel.normalized() * (s.brakeRate * 0.5f / s.mass);
            }
            break;
        }
        case ShipState::AttackingPortal: {
            const int target = portalSlotByHandle(s.targetPortal, s.targetPortalGen);
            if (target < 0 || portals_[target].faction == s.faction) {
                s.state = ShipState::Idle;
                s.targetPortal = UINT32_MAX;
                s.decisionTimer = 0.0f;
                break;
            }
            const Portal& p = portals_[target];
            const Vec2 toTarget = p.pos - s.pos;
            const float dist = std::max(0.0f, toTarget.length() - p.radius * 0.5f);
            if (dist > s.viewRange) { s.state = ShipState::Idle; s.decisionTimer = 0.0f; break; }
            const float targetAngle = toTarget.angle();
            float diff = Vec2::angleDiff(s.rot, targetAngle);
            const float maxRot = s.maneuverability * dt;
            diff = clampf(diff, -maxRot, maxRot);
            s.rot += diff;

            if (std::abs(diff) < s.fireAngle && dist <= s.fireRange) {
                fireAtPortal(static_cast<int>(i), target);
            }
            const Vec2 dir = Vec2(1.0f, 0.0f).rotated(s.rot);
            s.acc += dir * (s.accelerationRate * 0.4f / s.mass);
            if (dist < s.fireRange * 0.5f) {
                s.acc -= s.vel.normalized() * (s.brakeRate * 0.4f / s.mass);
            }
            break;
        }
        case ShipState::Mining: {
            const int target = asteroidSlotByHandle(s.targetAst, s.targetAstGen);
            if (target < 0 || !asteroids_[target].active) {
                s.state = ShipState::Idle;
                s.targetAst = UINT32_MAX;
                s.decisionTimer = 0.0f;
                break;
            }
            const Asteroid& t = asteroids_[target];
            const Vec2 toTarget = t.pos - s.pos;
            const float dist = toTarget.length();
            if (dist > s.viewRange) { s.state = ShipState::Idle; s.decisionTimer = 0.0f; break; }
            const float targetAngle = toTarget.angle();
            float diff = Vec2::angleDiff(s.rot, targetAngle);
            const float maxRot = s.maneuverability * dt;
            diff = clampf(diff, -maxRot, maxRot);
            s.rot += diff;

            if (std::abs(diff) < s.fireAngle && dist <= s.fireRange) fireAtAsteroid(static_cast<int>(i), target);
            const Vec2 dir = Vec2(1.0f, 0.0f).rotated(s.rot);
            s.acc += dir * (s.accelerationRate * 0.35f / s.mass);
            break;
        }
        case ShipState::Collecting: {
            if (s.targetLight >= lights_.size() || !lights_[s.targetLight].active) {
                s.state = ShipState::Idle;
                s.targetLight = UINT32_MAX;
                s.decisionTimer = 0.0f;
                break;
            }
            const LightPoint& l = lights_[s.targetLight];
            const Vec2 toTarget = l.pos - s.pos;
            const float dist = toTarget.length();
            if (dist > s.viewRange) { s.state = ShipState::Idle; s.decisionTimer = 0.0f; break; }
            const float targetAngle = toTarget.angle();
            float diff = Vec2::angleDiff(s.rot, targetAngle);
            const float maxRot = s.maneuverability * dt * 1.6f;
            diff = clampf(diff, -maxRot, maxRot);
            s.rot += diff;

            // 纵向推进和横向惯性抵消都在 updateSpeedControl 里统一做速度匹配，
            // 这里只负责把船头转向光点（纯视觉/朝向），不再额外刹车。
            break;
        }
        case ShipState::Fleeing: {
            // 朝远离威胁的方向转身；同时用侧/后向推进器立刻加速拉开距离，
            // 而不是先原地慢慢掉头再走。
            Vec2 away = s.pos - s.threatPos;
            if (away.lengthSq() < 1e-4f) away = Vec2(1.0f, 0.0f).rotated(s.rot + PI);
            const Vec2 escapeDir = away.normalized();
            const float maxRot = s.maneuverability * dt * 1.35f;
            const float diff = Vec2::angleDiff(s.rot, escapeDir.angle());
            s.rot += clampf(diff, -maxRot, maxRot);

            s.acc += escapeDir * (s.accelerationRate * 1.8f / s.mass);
            const Vec2 forward = Vec2(1.0f, 0.0f).rotated(s.rot);
            if (forward.dot(escapeDir) < -0.1f && s.vel.lengthSq() > 1.0f) {
                s.acc -= s.vel.normalized() * (s.brakeRate * 0.6f / s.mass);
            }
            break;
        }
        case ShipState::Idle:
        default: {
            if (s.idleTimer > 0.3f + rng_.f(0.0f, 1.2f)) {
                s.rot += (rng_.f(0.0f, 1.0f) - 0.5f) * PI * 0.1f * s.maneuverability * 0.8f;
                s.idleTimer = 0.0f;
            }
            break;
        }
        }

        if (s.building) {
            s.buildProgress += dt * s.buildSpeed;
            if (s.buildProgress >= 1.0f) {
                const Vec2 offset = Vec2(1.0f, 0.0f).rotated(s.rot) * -18.0f +
                                    Vec2(std::cos(rng_.f(0.0f, TAU)), std::sin(rng_.f(0.0f, TAU))) * 8.0f;
                requestShip(s.pos + offset, s.faction, static_cast<int>(i));
                s.building = false;
                s.buildProgress = 0.0f;
            }
        }
    }
}

void World::updateAsteroids(float dt) {
    const size_t n = asteroids_.size();
    for (size_t i = 0; i < n; ++i) {
        Asteroid& a = asteroids_[i];
        if (!a.active) continue;

        a.vel += a.acc * dt;
        a.acc = {};
        a.pos += a.vel * dt;
        a.rot += a.angVel * dt;
        while (a.rot > TAU) a.rot -= TAU;
        while (a.rot < 0.0f) a.rot += TAU;
        a.hitCooldown = std::max(0.0f, a.hitCooldown - dt);

        // 飞出窗口外一定距离就回收（health 仍 > 0，cleanup 不会把它当击杀处理）。
        // 旧版没有回收：所有槽位被屏幕外的小行星占满，数量停在 MAX_ASTEROIDS，
        // 而窗口里一颗都没有。
        if (!isInsideWorld(a.pos, ASTEROID_DESPAWN_MARGIN)) {
            a.active = false;
            a.deathHandled = false;
            continue;
        }

        if (a.type == AsteroidType::Comet) {
            a.trailTimer += dt;
            if (a.trailTimer >= 0.035f) {
                a.trailTimer = 0.0f;
                for (int k = std::min(a.trailCount, 23); k > 0; --k) a.trail[k] = a.trail[k - 1];
                a.trail[0] = a.pos;
                a.trailCount = std::min(a.trailCount + 1, 24);
            }
        }
    }
}

void World::updateLights(float dt) {
    constexpr float kEdge = 10.0f;
    const size_t n = lights_.size();
    for (size_t i = 0; i < n; ++i) {
        LightPoint& l = lights_[i];
        if (!l.active) continue;
        l.vel = l.vel * 0.985f;
        l.pos += l.vel * dt;
        l.hitCooldown = std::max(0.0f, l.hitCooldown - dt);

        // 光点必须留在飞船够得到的范围内：碰到窗口边缘反弹。
        // 旧版直接飞出窗口就销毁，边缘光点会先贴在窗外，飞船中心又被
        // 限制在窗口内 5px，导致「看得见、捡不到、一直耗着」。
        if (l.pos.x < kEdge) {
            l.pos.x = kEdge;
            l.vel.x = std::abs(l.vel.x) * 0.8f;
        } else if (l.pos.x > worldWidth_ - kEdge) {
            l.pos.x = worldWidth_ - kEdge;
            l.vel.x = -std::abs(l.vel.x) * 0.8f;
        }
        if (l.pos.y < kEdge) {
            l.pos.y = kEdge;
            l.vel.y = std::abs(l.vel.y) * 0.8f;
        } else if (l.pos.y > worldHeight_ - kEdge) {
            l.pos.y = worldHeight_ - kEdge;
            l.vel.y = -std::abs(l.vel.y) * 0.8f;
        }
    }
}

void World::updateBeams(float dt) {
    for (auto& b : beams_) b.life -= dt;
    beams_.erase(std::remove_if(beams_.begin(), beams_.end(),
                                [](const Beam& b) { return b.life <= 0.0f; }),
                 beams_.end());
}

void World::updateZones(float dt) {
    zoneTimer_ += dt;
    if (zoneTimer_ >= nextZoneTime_) {
        spawnZone();
        zoneTimer_ = 0.0f;
        nextZoneTime_ = 6.0f + static_cast<float>(rng_.i(0, 24));
    }

    for (auto& z : zones_) {
        if (!z.active) continue;
        z.life -= dt;
        if (z.life <= 0.0f) {
            z.active = false;
            continue;
        }
        const float t = 1.0f - z.life / z.maxLife;
        if (t < 0.25f) z.radius = lerpf(5.0f, z.maxRadius, t / 0.25f);
        else if (t < 0.75f) z.radius = z.maxRadius;
        else z.radius = lerpf(z.maxRadius, 0.0f, (t - 0.75f) / 0.25f);
        z.rotation += dt * 0.5f;
        if (z.rotation > TAU) z.rotation -= TAU;
        applyZone(z, dt);
    }

    zones_.erase(std::remove_if(zones_.begin(), zones_.end(),
                                [](const Zone& z) { return !z.active; }),
                 zones_.end());
}

void World::spawnZone() {
    if (zones_.size() >= static_cast<size_t>(MAX_ZONES)) return;
    Zone z;
    z.type = static_cast<ZoneType>(rng_.i(0, 5));
    z.pos = { rng_.f(100.0f, worldWidth_ - 100.0f), rng_.f(100.0f, worldHeight_ - 100.0f) };
    z.life = ZONE_DURATION;
    z.maxLife = ZONE_DURATION;
    z.radius = 5.0f;
    z.maxRadius = 100.0f;
    z.color = zoneColor(z.type);
    zones_.push_back(z);
}

// ============================================================
// 创造面板：飞船 / 小行星 / 机遇 / 传送门
// ============================================================

float World::shipFormationRadius(FormationType formation, int count) {
    count = std::clamp(count, 1, 2000);
    if (formation == FormationType::Square) {
        const int cols = std::max(1, static_cast<int>(std::ceil(std::sqrt(static_cast<float>(count)))));
        const int rows = std::max(1, (count + cols - 1) / cols);
        const float s = 13.0f;
        const float hw = static_cast<float>(cols - 1) * s * 0.5f;
        const float hh = static_cast<float>(rows - 1) * s * 0.5f;
        return std::sqrt(hw * hw + hh * hh) + 8.0f;
    }
    if (formation == FormationType::Circle) {
        if (count <= 1) return 8.0f;
        int placed = 1;
        float r = 0.0f;
        for (int k = 1; placed < count; ++k) {
            const int cap = std::max(1, static_cast<int>(TAU * static_cast<float>(k)));
            placed += cap;
            r = static_cast<float>(k) * 13.0f;
        }
        return r + 8.0f;
    }
    const float spacing = (formation == FormationType::Dense) ? 10.5f : 13.5f;
    return spacing * std::sqrt(static_cast<float>(count) / PI) + 8.0f;
}

void World::buildShipFormation(FormationType formation, int count, std::vector<Vec2>& out) {
    count = std::clamp(count, 1, 2000);
    out.clear();
    out.reserve(static_cast<size_t>(count));
    if (formation == FormationType::Square) {
        const int cols = std::max(1, static_cast<int>(std::ceil(std::sqrt(static_cast<float>(count)))));
        const int rows = std::max(1, (count + cols - 1) / cols);
        const float s = 13.0f;
        for (int i = 0; i < count; ++i) {
            const int row = i / cols;
            const int col = i % cols;
            out.push_back({ (static_cast<float>(col) - static_cast<float>(cols - 1) * 0.5f) * s,
                            (static_cast<float>(row) - static_cast<float>(rows - 1) * 0.5f) * s });
        }
    } else if (formation == FormationType::Circle) {
        out.push_back({0.0f, 0.0f});
        int placed = 1;
        for (int k = 1; placed < count; ++k) {
            const int cap = std::max(1, static_cast<int>(TAU * static_cast<float>(k)));
            const float r = static_cast<float>(k) * 13.0f;
            for (int j = 0; j < cap && placed < count; ++j, ++placed) {
                const float a = TAU * static_cast<float>(j) / static_cast<float>(cap) +
                                static_cast<float>(k) * 0.37f;
                out.push_back({ std::cos(a) * r, std::sin(a) * r });
            }
        }
    } else if (formation == FormationType::Dense) {
        constexpr float kGolden = 2.39996323f;
        constexpr float s = 10.5f;
        for (int i = 0; i < count; ++i) {
            const float r = s * std::sqrt(static_cast<float>(i) + 0.5f);
            const float a = static_cast<float>(i) * kGolden;
            out.push_back({ std::cos(a) * r, std::sin(a) * r });
        }
    } else {
        const float radius = shipFormationRadius(FormationType::Random, count) - 8.0f;
        for (int i = 0; i < count; ++i) {
            const float r = radius * std::sqrt(rng_.f(0.0f, 1.0f));
            const float a = rng_.f(0.0f, TAU);
            out.push_back({ std::cos(a) * r, std::sin(a) * r });
        }
    }
}

void World::buildAsteroidScatter(int count, float bodyRadius, std::vector<Vec2>& out) {
    count = std::clamp(count, 1, 360);
    out.clear();
    out.reserve(static_cast<size_t>(count));
    constexpr float kGolden = 2.39996323f;
    const float d = bodyRadius * 2.0f + 8.0f;
    for (int i = 0; i < count; ++i) {
        const float r = d * 0.8f * std::sqrt(static_cast<float>(i) + 0.5f);
        const float a = static_cast<float>(i) * kGolden;
        out.push_back({ std::cos(a) * r, std::sin(a) * r });
    }
}

float World::creationRadius(const CreationSpec& spec) {
    if (spec.kind == CreationKind::Ship) {
        return shipFormationRadius(spec.formation, spec.count);
    }
    if (spec.kind == CreationKind::Asteroid) {
        const int count = std::clamp(spec.count, 1, 360);
        const float body = std::max(1.0f, spec.asteroidRadius);
        const float d = body * 2.0f + 8.0f;
        const float maxR = d * 0.8f * std::sqrt(static_cast<float>(count - 1) + 0.5f);
        return body + maxR;
    }
    if (spec.kind == CreationKind::Opportunity) {
        return std::max(1.0f, spec.zoneRadius);
    }
    return 70.0f;
}

Vec2 World::reinforcementOrigin(Vec2 center, Vec2 dir, float radius,
                                float worldWidth, float worldHeight) {
    const float len = dir.length();
    if (len < 1e-4f) dir = {1.0f, 0.0f};
    else dir = dir / len;

    float tEdge = std::numeric_limits<float>::max();
    if (dir.x > 1e-4f) tEdge = std::min(tEdge, (worldWidth - center.x) / dir.x);
    else if (dir.x < -1e-4f) tEdge = std::min(tEdge, (0.0f - center.x) / dir.x);
    if (dir.y > 1e-4f) tEdge = std::min(tEdge, (worldHeight - center.y) / dir.y);
    else if (dir.y < -1e-4f) tEdge = std::min(tEdge, (0.0f - center.y) / dir.y);
    if (tEdge == std::numeric_limits<float>::max() || tEdge < 0.0f) {
        tEdge = std::max(worldWidth, worldHeight) * 0.5f;
    }
    return center + dir * (tEdge + radius + 90.0f);
}

uint32_t World::spawnShipEx(Vec2 pos, Vec2 vel, float rot, int faction,
                            const ShipAttributePoints& attrs, bool entering) {
    const uint32_t idx = spawnShip(pos, std::clamp(faction, 0, MAX_FACTIONS - 1), nullptr);
    if (idx == UINT32_MAX) return UINT32_MAX;
    Ship& s = ships_[idx];

    const auto apply = [&](int n, int type) {
        for (int k = 0; k < n; ++k) applyUpgrade(s, type);
    };
    apply(attrs.survival, 1);
    apply(attrs.attack, 2);
    apply(attrs.mobility, 3);
    apply(attrs.production, 4);
    apply(attrs.vision, 5);

    s.level = std::max(1, 1 + attrs.total());
    s.upgradeCooldown = 0.0f;
    s.health = s.maxHealth;
    s.armor = s.maxArmor;
    s.vel = vel;
    s.rot = rot;
    s.entering = entering;
    s.decisionTimer = rng_.f(0.0f, 0.3f);
    s.fireTimer = 0.0f;
    if (s.vel.length() > s.maxSpeed) s.vel = s.vel.normalized() * s.maxSpeed;
    return idx;
}

uint32_t World::spawnConfiguredZone(Vec2 pos, ZoneType type, float duration, float radius) {
    Zone z;
    z.type = type;
    z.pos = pos;
    z.life = std::max(0.5f, duration);
    z.maxLife = z.life;
    z.radius = 5.0f;
    z.maxRadius = std::max(5.0f, radius);
    z.rotation = rng_.f(0.0f, TAU);
    z.reinforceTimer = 0.0f;
    z.color = zoneColor(type);
    zones_.push_back(z);
    return static_cast<uint32_t>(zones_.size() - 1);
}

void World::placeShipBatch(const CreationSpec& spec, Vec2 origin, Vec2 dir, bool entering,
                           uint32_t& created) {
    std::vector<Vec2> offsets;
    buildShipFormation(spec.formation, spec.count, offsets);
    const Vec2 vel = dir * std::max(0.0f, spec.speed);
    const float rot = dir.angle();
    for (const Vec2& off : offsets) {
        Vec2 p = origin + off;
        if (!entering) {
            p.x = clampf(p.x, 8.0f, static_cast<float>(worldWidth_) - 8.0f);
            p.y = clampf(p.y, 8.0f, static_cast<float>(worldHeight_) - 8.0f);
        }
        if (spawnShipEx(p, vel, rot, spec.faction, spec.attributes, entering) != UINT32_MAX) {
            ++created;
        }
    }
}

uint32_t World::placeAsteroidBatch(const CreationSpec& spec, Vec2 origin, Vec2 dir, bool entering) {
    std::vector<Vec2> offsets;
    buildAsteroidScatter(spec.count, spec.asteroidRadius, offsets);
    const Vec2 vel = dir * std::max(0.0f, spec.speed);
    uint32_t created = 0;
    for (const Vec2& off : offsets) {
        Vec2 p = origin + off;
        if (entering) {
            // 小行星没有 entering 标记：必须让它出生在回收余量内，否则会被当成飞出场外回收。
            p.x = clampf(p.x, -150.0f, static_cast<float>(worldWidth_) + 150.0f);
            p.y = clampf(p.y, -150.0f, static_cast<float>(worldHeight_) + 150.0f);
        } else {
            p.x = clampf(p.x, 4.0f, static_cast<float>(worldWidth_) - 4.0f);
            p.y = clampf(p.y, 4.0f, static_cast<float>(worldHeight_) - 4.0f);
        }
        const uint32_t idx = spawnAsteroid(p, vel, spec.asteroidRadius, spec.asteroidType,
                                           spec.irregularity);
        if (idx != UINT32_MAX) ++created;
    }
    return created;
}

uint32_t World::createBatch(const CreationSpec& spec, Vec2 center, Vec2 dir, SpawnMode mode) {
    if (mode == SpawnMode::Portal) {
        return spawnPortal(center, spec) == UINT32_MAX ? 0u : 1u;
    }

    Vec2 d = dir;
    if (d.lengthSq() < 1e-6f) d = {1.0f, 0.0f};
    else d = d.normalized();

    if (spec.kind == CreationKind::Opportunity) {
        return spawnConfiguredZone(center, spec.zoneType, spec.zoneDuration, spec.zoneRadius) ==
                       UINT32_MAX
                   ? 0u
                   : 1u;
    }

    const float radius = creationRadius(spec);
    const bool entering = mode == SpawnMode::Reinforce;
    Vec2 origin = center;
    if (entering) {
        // 增援从“速度方向”的反侧界外出发，沿箭头方向飞进地图。
        // 小行星不能离窗口太远（>180px 会被回收），按边缘带处理；飞船可以整组在界外。
        const float push = (spec.kind == CreationKind::Asteroid) ? std::min(radius, 120.0f) : radius;
        origin = reinforcementOrigin(center, -d, push, worldWidth_, worldHeight_);
    }

    uint32_t created = 0;
    if (spec.kind == CreationKind::Ship) {
        placeShipBatch(spec, origin, d, entering, created);
    } else {
        created = placeAsteroidBatch(spec, origin, d, entering);
    }
    return created;
}

uint32_t World::createRandom(const CreationSpec& spec, SpawnMode mode) {
    const float radius = creationRadius(spec);
    Vec2 center;
    const float mx = std::min(radius, static_cast<float>(worldWidth_) * 0.5f - 30.0f);
    const float my = std::min(radius, static_cast<float>(worldHeight_) * 0.5f - 30.0f);
    if (mx * 2.0f < static_cast<float>(worldWidth_) - 20.0f) {
        center.x = rng_.f(mx + 10.0f, static_cast<float>(worldWidth_) - mx - 10.0f);
    } else {
        center.x = static_cast<float>(worldWidth_) * 0.5f;
    }
    if (my * 2.0f < static_cast<float>(worldHeight_) - 20.0f) {
        center.y = rng_.f(my + 10.0f, static_cast<float>(worldHeight_) - my - 10.0f);
    } else {
        center.y = static_cast<float>(worldHeight_) * 0.5f;
    }
    const float a = rng_.f(0.0f, TAU);
    return createBatch(spec, center, {std::cos(a), std::sin(a)}, mode);
}

uint32_t World::spawnPortal(Vec2 pos, const CreationSpec& spec) {
    int slot = -1;
    for (size_t i = 0; i < portals_.size(); ++i) {
        if (!portals_[i].active) { slot = static_cast<int>(i); break; }
    }
    if (slot < 0) {
        if (portals_.size() >= static_cast<size_t>(MAX_PORTALS)) return UINT32_MAX;
        portals_.push_back(Portal{});
        slot = static_cast<int>(portals_.size() - 1);
    }

    Portal& p = portals_[static_cast<size_t>(slot)];
    const uint32_t nextGen = p.gen + 1u;
    p = Portal{};
    p.gen = nextGen;
    p.active = true;
    p.pos = pos;
    p.spec = spec;
    p.kind = spec.kind;
    p.faction = (spec.kind == CreationKind::Ship) ? std::clamp(spec.faction, 0, MAX_FACTIONS - 1) : -1;
    p.radius = 46.0f;
    p.rotation = rng_.f(0.0f, TAU);
    p.maxHealth = std::max(1.0f, spec.portalHealth);
    p.health = p.maxHealth;
    p.spawnInterval = std::max(1.0f, spec.portalInterval);
    p.spawnTimer = 0.0f;
    p.waveIndex = 0;
    p.flashTimer = 0.0f;
    if (p.faction >= 0) p.color = factionColor(p.faction);
    else if (p.kind == CreationKind::Asteroid) p.color = sf::Color(150, 180, 210);
    else p.color = zoneColor(spec.zoneType);

    spawnPortalWave(p); // 传送门落成时立刻刷第一波，避免用户等 1 分钟才看到效果
    return static_cast<uint32_t>(slot);
}

void World::spawnPortalWave(Portal& p) {
    if (!p.active) return;
    const CreationSpec& spec = p.spec;
    const int count = std::clamp(spec.count, 1, 2000);
    if (spec.kind == CreationKind::Ship) {
        std::vector<Vec2> offsets;
        buildShipFormation(spec.formation, count, offsets);
        const float a = rng_.f(0.0f, TAU);
        const Vec2 dir(std::cos(a), std::sin(a));
        const Vec2 vel = dir * std::max(0.0f, spec.speed);
        for (const Vec2& off : offsets) {
            Vec2 q = p.pos + off;
            q.x = clampf(q.x, 8.0f, static_cast<float>(worldWidth_) - 8.0f);
            q.y = clampf(q.y, 8.0f, static_cast<float>(worldHeight_) - 8.0f);
            spawnShipEx(q, vel, dir.angle(), p.faction, spec.attributes, false);
        }
    } else if (spec.kind == CreationKind::Asteroid) {
        std::vector<Vec2> offsets;
        buildAsteroidScatter(count, spec.asteroidRadius, offsets);
        const float a = rng_.f(0.0f, TAU);
        const Vec2 dir(std::cos(a), std::sin(a));
        const Vec2 vel = dir * std::max(0.0f, spec.speed);
        for (const Vec2& off : offsets) {
            Vec2 q = p.pos + off;
            q.x = clampf(q.x, 4.0f, static_cast<float>(worldWidth_) - 4.0f);
            q.y = clampf(q.y, 4.0f, static_cast<float>(worldHeight_) - 4.0f);
            spawnAsteroid(q, vel, spec.asteroidRadius, spec.asteroidType, spec.irregularity);
        }
    } else {
        spawnConfiguredZone(p.pos, spec.zoneType, spec.zoneDuration, spec.zoneRadius);
    }
    p.flashTimer = 0.6f;
    ++p.waveIndex;
}

void World::updatePortals(float dt) {
    for (Portal& p : portals_) {
        if (!p.active) continue;
        p.rotation += dt * 1.1f;
        while (p.rotation > TAU) p.rotation -= TAU;
        p.flashTimer = std::max(0.0f, p.flashTimer - dt);
        p.spawnTimer += dt;
        int guard = 0;
        while (p.spawnTimer >= p.spawnInterval && guard < 4) {
            p.spawnTimer -= p.spawnInterval;
            spawnPortalWave(p);
            ++guard;
        }
    }
}

int World::portalSlotByHandle(uint32_t index, uint32_t gen) const {
    if (index >= portals_.size()) return -1;
    const Portal& p = portals_[index];
    if (!p.active || p.gen != gen) return -1;
    return static_cast<int>(index);
}

void World::applyZone(Zone& z, float dt) {
    if (z.radius < 1.0f) return;

    if (z.type == ZoneType::Reinforce) {
        z.reinforceTimer += dt;
        if (z.reinforceTimer < 3.0f) return;
        z.reinforceTimer = 0.0f;

        std::vector<uint32_t> candidates;
        candidates.reserve(64);
        shipGrid_.query(z.pos, z.radius, [&](uint32_t idx) {
            if (idx >= ships_.size() || !ships_[idx].active) return;
            if ((ships_[idx].pos - z.pos).lengthSq() <= z.radius * z.radius) candidates.push_back(idx);
        });
        if (candidates.empty()) return;
        const uint32_t pick = candidates[static_cast<size_t>(rng_.i(0, static_cast<int>(candidates.size()) - 1))];
        const Vec2 pos = z.pos + Vec2(std::cos(rng_.f(0.0f, TAU)), std::sin(rng_.f(0.0f, TAU))) *
                                       (z.radius * rng_.f(0.0f, 0.7f));
        requestShip(pos, ships_[pick].faction, static_cast<int>(pick));
        return;
    }

    const int type = static_cast<int>(z.type);
    shipGrid_.query(z.pos, z.radius, [&](uint32_t idx) {
        if (idx >= ships_.size()) return;
        Ship& s = ships_[idx];
        if (!s.active || s.building) return;
        if ((s.pos - z.pos).lengthSq() > z.radius * z.radius) return;
        if (s.upgradeCooldown > 0.0f) return;
        applyUpgrade(s, type);
        s.upgradeCooldown = 5.0f;
    });
}

void World::reinforceZone(Zone& z) {
    applyZone(z, 3.0f);
}

void World::applyUpgrade(Ship& s, int type) {
    ++s.level;
    switch (type) {
    case 1:
        s.recoveryRate += 1.0f;
        s.maxHealth += 50.0f;
        s.health = std::min(s.maxHealth, s.health + 20.0f);
        s.maxArmor += 50.0f;
        s.armor = std::min(s.armor + 20.0f, s.maxArmor);
        s.armorRegen += 0.05f;
        break;
    case 2:
        s.damage += 5.0f;
        s.fireRate += 1.0f;
        break;
    case 3:
        s.maxSpeed += 30.0f;
        s.maneuverability += 0.2f;
        s.accelerationRate = 50.0f * (s.maneuverability / 0.8f);
        s.brakeRate = 20.0f * (s.maneuverability / 0.8f);
        s.standardSpeed = standardSpeedOf(s);
        break;
    case 4:
        s.buildSpeed += 0.05f;
        s.miningExpertise += 0.05f;
        break;
    case 5:
        s.viewRange += 30.0f;
        s.fireRange += 20.0f;
        s.standardSpeed = standardSpeedOf(s);
        break;
    default:
        break;
    }
}

void World::requestShip(Vec2 pos, int faction, int copyFrom) {
    if (shipRequests_.size() >= 64) return;
    shipRequests_.push_back({ pos, faction, copyFrom });
}

void World::flushShipRequests() {
    if (shipRequests_.empty()) return;
    for (const ShipRequest& req : shipRequests_) {
        const Ship* proto = (req.copyFrom >= 0 && req.copyFrom < static_cast<int>(ships_.size()) &&
                             ships_[req.copyFrom].active)
                                ? &ships_[req.copyFrom]
                                : nullptr;
        const uint32_t idx = spawnShip(req.pos, req.faction, proto);
        if (idx == UINT32_MAX) continue;
        Ship& s = ships_[idx];
        if (proto) {
            s.level = proto->level;
            s.rot = rng_.f(0.0f, TAU);
            s.vel = proto->vel + Vec2(std::cos(rng_.f(0.0f, TAU)), std::sin(rng_.f(0.0f, TAU))) * rng_.f(5.0f, 25.0f);
            s.health = s.maxHealth;
            s.armor = s.maxArmor;
        }
        if (req.faction >= 0 && req.faction < 4) {
            factionTotals_[static_cast<size_t>(req.faction)][FF_Built] += 1.0;
        }
    }
    shipRequests_.clear();
}

void World::collectLights(Ship& s) {
    lightGrid_.query(s.pos, COLLECTION_DISTANCE, [&](uint32_t idx) {
        if (idx >= lights_.size()) return;
        LightPoint& l = lights_[idx];
        if (!l.active) return;
        if ((l.pos - s.pos).lengthSq() > COLLECTION_DISTANCE * COLLECTION_DISTANCE) return;
        l.active = false;
        if (s.faction >= 0 && s.faction < MAX_FACTIONS) {
            resources_[s.faction] += 1.0f;
            factionTotals_[static_cast<size_t>(s.faction)][FF_Collected] += 1.0;
        }
        ++s.collected;
    });
}

void World::updateBuilding(float dt) {
    for (int f = 0; f < MAX_FACTIONS; ++f) {
        if (resources_[f] < RESOURCE_TO_BUILD) continue;
        int best = -1;
        float bestScore = -1.0f;
        for (size_t i = 0; i < ships_.size(); ++i) {
            const Ship& s = ships_[i];
            if (!s.active || s.faction != f || s.building) continue;
            const float ratio = s.maxHealth > 0.0f ? s.health / s.maxHealth : 0.0f;
            const float score = static_cast<float>(s.level) * 10.0f + ratio;
            if (score > bestScore) { bestScore = score; best = static_cast<int>(i); }
        }
        if (best >= 0) {
            ships_[best].building = true;
            ships_[best].buildProgress = 0.0f;
            resources_[f] -= RESOURCE_TO_BUILD;
        }
    }
}

void World::updateRebellion(float dt) {
    if (!rebellionEnabled) return;
    rebellionTimer_ += dt;
    if (rebellionTimer_ < 1.0f) return;
    rebellionTimer_ = 0.0f;

    bool empty[4] = { true, true, true, true };
    for (const auto& s : ships_) {
        if (s.active && s.faction >= 0 && s.faction < 4) empty[s.faction] = false;
    }
    std::vector<int> emptyFactions;
    for (int f = 0; f < 4; ++f) if (empty[f]) emptyFactions.push_back(f);
    if (emptyFactions.empty()) return;
    if (rng_.f() >= 0.01f) return;

    const int targetFaction = emptyFactions[static_cast<size_t>(rng_.i(0, static_cast<int>(emptyFactions.size()) - 1))];
    std::vector<int> populated;
    for (int f = 0; f < 4; ++f) if (!empty[f]) populated.push_back(f);
    if (populated.empty()) return;
    const int sourceFaction = populated[static_cast<size_t>(rng_.i(0, static_cast<int>(populated.size()) - 1))];

    int maxLevel = 0;
    for (const auto& s : ships_) {
        if (s.active && s.faction == sourceFaction) maxLevel = std::max(maxLevel, s.level);
    }

    for (auto& s : ships_) {
        if (!s.active || s.faction != sourceFaction) continue;
        float chance = 0.1f * static_cast<float>(maxLevel - s.level);
        chance = std::max(0.05f, chance);
        if (rng_.f() < chance) {
            s.faction = targetFaction;
            s.color = factionColor(targetFaction);
            // 新阵营的名字/编号由 refreshStats() 的生命周期处理：
            // 空槽位重新出现飞船时，assignFactionIdentity() 会换名并分配新编号。
        }
    }
}

void World::updateTrails(float dt) {
    for (auto& s : ships_) {
        if (!s.active || s.kills <= 3) continue;
        s.trailTimer += dt;
        if (s.trailTimer >= 0.12f) {
            s.trailTimer = 0.0f;
            const Vec2 rear = s.pos + Vec2(-5.0f, 0.0f).rotated(s.rot);
            const Vec2 base = s.vel * -0.15f;
            const Vec2 jitter(std::cos(rng_.f(0.0f, TAU)), std::sin(rng_.f(0.0f, TAU)));
            addTrailParticle(rear, base + jitter * rng_.f(1.0f, 4.0f),
                             colorScale(s.color, 1.2f, 190), rng_.f(1.5f, 3.5f));
        }
    }

    for (auto& p : trails_) {
        if (p.life >= p.maxLife) continue;
        p.pos += p.vel * dt;
        p.life += dt;
        p.size *= 0.96f;
    }
    trails_.erase(std::remove_if(trails_.begin(), trails_.end(),
                                 [](const TrailParticle& p) {
                                     return p.life >= p.maxLife || p.size < 0.2f;
                                 }),
                  trails_.end());
}

void World::scanObstacles(Ship& s) {
    s.obstacleCount = 0;
    const float range = obstacleRangeOf(s);
    const Vec2 shipDir = Vec2(1.0f, 0.0f).rotated(s.rot);

    astGrid_.query(s.pos, range, [&](uint32_t idx) {
        if (idx >= asteroids_.size()) return;
        const Asteroid& a = asteroids_[idx];
        if (!a.active) return;
        const Vec2 toObj = a.pos - s.pos;
        const float dist = toObj.length();
        if (dist > range || dist < 1e-3f) return;

        float threat = 0.0f;
        const Vec2 relVel = a.vel - s.vel;
        const float closing = -relVel.dot(toObj.normalized());
        if (closing > 0.0f) threat += std::min(closing / 50.0f, 0.5f);
        const float forward = shipDir.dot(toObj.normalized());
        if (forward > 0.0f) threat += forward * 0.3f;
        threat += (1.0f - dist / range) * 0.5f;
        if (threat < 0.12f) return;

        Obstacle o;
        o.pos = a.pos;
        o.vel = a.vel;
        o.radius = a.radius;
        o.distance = dist;
        o.threat = threat;

        if (s.obstacleCount < 10) {
            s.obstacles[s.obstacleCount++] = o;
        } else {
            int worst = 0;
            for (int k = 1; k < 10; ++k) if (s.obstacles[k].threat < s.obstacles[worst].threat) worst = k;
            if (threat > s.obstacles[worst].threat) s.obstacles[worst] = o;
        }
    });
}

Vec2 World::obstacleAvoidance(const Ship& s) const {
    if (s.obstacleCount <= 0) return {};
    Vec2 total;
    const Vec2 shipDir = Vec2(1.0f, 0.0f).rotated(s.rot);
    for (int i = 0; i < s.obstacleCount; ++i) {
        const Obstacle& o = s.obstacles[i];
        const Vec2 toObstacle = o.pos - s.pos;
        const float dist = toObstacle.length();
        if (dist < 1.0f) continue;
        const Vec2 relVel = s.vel - o.vel;
        const float closing = -relVel.dot(toObstacle.normalized());
        if (closing <= 0.0f) continue;
        const float ttc = dist / closing;
        if (ttc >= 1.0f) continue;

        Vec2 perp = toObstacle.perpendicular().normalized();
        if (perp.dot(shipDir) < 0.0f) perp = -perp;
        const float magnitude = s.maneuverability * 10.0f * o.threat * (1.0f / (ttc + 0.1f));
        total += perp * magnitude;
        if (ttc < 0.5f) total -= shipDir * (magnitude * 0.5f);
    }
    return total;
}

Vec2 World::boundaryAvoidance(const Ship& s) const {
    Vec2 force;
    const float margin = 24.0f;
    if (s.pos.x < margin) force.x += (margin - s.pos.x) * 20.0f;
    else if (s.pos.x > worldWidth_ - margin) force.x -= (s.pos.x - (worldWidth_ - margin)) * 20.0f;
    if (s.pos.y < margin) force.y += (margin - s.pos.y) * 20.0f;
    else if (s.pos.y > worldHeight_ - margin) force.y -= (s.pos.y - (worldHeight_ - margin)) * 20.0f;
    return force;
}

bool World::needsEmergencyBrake(const Ship& s) const {
    for (int i = 0; i < s.obstacleCount; ++i) {
        const Obstacle& o = s.obstacles[i];
        if (o.distance < 20.0f) {
            const Vec2 toObstacle = o.pos - s.pos;
            const Vec2 relVel = s.vel - o.vel;
            const float closing = -relVel.dot(toObstacle.normalized());
            if (closing > 10.0f && o.distance < 14.0f) return true;
        }
    }
    return false;
}

void World::updateSpeedControl(Ship& s, float dt) {
    (void)dt;
    if (s.state == ShipState::Fleeing) {
        // Fleeing 行为自己施加逃逸推力（含侧向/后向），这里不要再叠加前向推力。
        s.targetSpeed = s.maxSpeed;
        return;
    }

    // 采集：直接做“速度匹配”，而不是只沿船头方向推。
    // 旧逻辑只改前向速度，横向惯性一直保留，船会绕着光点画圈。
    // 这里把期望速度指向光点，同时先抵消横向速度；横向越大，前进目标速度越低，
    // 避免带着大横向惯性冲过光点后再折返（看起来就是绕圈）。
    if (s.state == ShipState::Collecting) {
        if (s.targetLight < lights_.size()) {
            const LightPoint& l = lights_[s.targetLight];
            if (l.active) {
                const Vec2 toTarget = l.pos - s.pos;
                const float dist = toTarget.length();
                if (dist > 1e-4f) {
                    const Vec2 dir = toTarget / dist;
                    const Vec2 perp = dir.perpendicular();
                    const float lateral = s.vel.dot(perp);

                    // 横向速度大时先把前进速度降到 0，优先把侧滑刹掉。
                    float desiredSpeed = clampf(dist * 1.5f, 8.0f, 60.0f);
                    desiredSpeed *= clampf(1.0f - std::max(0.0f, std::abs(lateral) - 4.0f) / 24.0f,
                                           0.0f, 1.0f);
                    if (std::abs(lateral) > 28.0f) desiredSpeed = 0.0f;

                    const Vec2 desiredVel = dir * desiredSpeed;
                    const Vec2 dv = desiredVel - s.vel;
                    const float maxAcc = s.accelerationRate * 1.5f / std::max(0.5f, s.mass);
                    Vec2 steer = dv * 3.0f;
                    if (steer.length() > maxAcc) steer = steer.normalized() * maxAcc;
                    s.acc += steer;
                    s.targetSpeed = desiredSpeed;
                    return;
                }
            }
        }
        s.targetSpeed = 0.0f;
        return;
    }

    float stateFactor = 1.0f;
    switch (s.state) {
    case ShipState::Attacking: stateFactor = 1.2f; break;
    case ShipState::AttackingPortal: stateFactor = 1.0f; break;
    case ShipState::Mining: stateFactor = 0.3f; break;
    default: stateFactor = 1.0f; break;
    }
    const float mixed = s.drivingSkill * s.standardSpeed + (1.0f - s.drivingSkill) * s.preferredSpeed;
    // 兜底：建造出来的船会继承 preferredSpeed；旧版随机游走没有下限，
    // 可能变成 0 或负数，targetSpeed<=0 时推进条件永远不成立，船就永远停住。
    float targetSpeed = std::max(mixed * stateFactor, 12.0f);
    const Vec2 forward = Vec2(1.0f, 0.0f).rotated(s.rot);

    s.targetSpeed = targetSpeed;
    const float current = s.vel.length();
    const float diff = s.targetSpeed - current;
    if (diff > 0.0f) {
        const float factor = std::min(1.0f, diff / s.targetSpeed);
        s.acc += forward * (s.accelerationRate * factor / s.mass);
    } else if (diff < -5.0f && current > 1e-3f) {
        const float factor = std::min(1.0f, -diff / current);
        s.acc -= s.vel.normalized() * (s.brakeRate * factor * 0.5f / s.mass);
    }
}

void World::evaluateState(Ship& s) {
    if (!s.active) return;

    // 环形搜索：先在近距离找，找到就不扩大范围。
    // 大规模混战里最近敌人通常就在几十像素内，避免每次扫描整个视野。
    int bestEnemy = -1;
    float bestEnemyDist = std::numeric_limits<float>::max();
    const float enemyRings[3] = { 32.0f, 80.0f, std::max(80.0f, s.viewRange) };
    float prevR = 0.0f;
    for (float r : enemyRings) {
        const float radius = std::min(r, s.viewRange);
        if (radius <= prevR) continue;
        prevR = radius;
        for (int g = 0; g < MAX_FACTIONS; ++g) {
            if (g == s.faction) continue;
            collisionGrids_[static_cast<size_t>(g)].query(s.pos, radius, [&](uint32_t idx) {
                if (idx >= ships_.size()) return;
                const Ship& other = ships_[idx];
                if (!other.active || other.faction == s.faction) return;
                const float d = (other.pos - s.pos).lengthSq();
                if (d < bestEnemyDist) { bestEnemyDist = d; bestEnemy = static_cast<int>(idx); }
            });
        }
        if (bestEnemy >= 0) break;
        if (radius >= s.viewRange) break;
    }
    if (bestEnemy >= 0) {
        s.state = ShipState::Attacking;
        s.targetShip = static_cast<uint32_t>(bestEnemy);
        s.targetShipGen = ships_[bestEnemy].gen;
        s.targetAst = UINT32_MAX;
        s.targetLight = UINT32_MAX;
        s.targetPortal = UINT32_MAX;
        return;
    }

    // 传送门“被视为该阵营的飞船”：没有更近的敌舰时，敌舰会转而攻击它。
    // 小行星/机遇身份的传送门只当场景装饰，不参与 AI 目标选择。
    int bestPortal = -1;
    float bestPortalDist = std::numeric_limits<float>::max();
    for (size_t pi = 0; pi < portals_.size(); ++pi) {
        const Portal& p = portals_[pi];
        if (!p.active || p.kind != CreationKind::Ship) continue;
        if (p.faction < 0 || p.faction == s.faction) continue;
        const float d = (p.pos - s.pos).lengthSq();
        if (d > s.viewRange * s.viewRange) continue;
        if (d < bestPortalDist) { bestPortalDist = d; bestPortal = static_cast<int>(pi); }
    }
    if (bestPortal >= 0) {
        s.state = ShipState::AttackingPortal;
        s.targetPortal = static_cast<uint32_t>(bestPortal);
        s.targetPortalGen = portals_[static_cast<size_t>(bestPortal)].gen;
        s.targetShip = UINT32_MAX;
        s.targetAst = UINT32_MAX;
        s.targetLight = UINT32_MAX;
        return;
    }

    int bestLight = -1;
    float bestLightDist = std::numeric_limits<float>::max();
    const float lightRings[3] = { 48.0f, 100.0f, std::max(100.0f, s.viewRange) };
    prevR = 0.0f;
    for (float r : lightRings) {
        const float radius = std::min(r, s.viewRange);
        if (radius <= prevR) continue;
        prevR = radius;
        lightGrid_.query(s.pos, radius, [&](uint32_t idx) {
            if (idx >= lights_.size()) return;
            const LightPoint& l = lights_[idx];
            if (!l.active) return;
            const float d = (l.pos - s.pos).lengthSq();
            if (d < bestLightDist) { bestLightDist = d; bestLight = static_cast<int>(idx); }
        });
        if (bestLight >= 0) break;
        if (radius >= s.viewRange) break;
    }
    if (bestLight >= 0) {
        s.state = ShipState::Collecting;
        s.targetLight = static_cast<uint32_t>(bestLight);
        s.targetShip = UINT32_MAX;
        s.targetAst = UINT32_MAX;
        s.targetPortal = UINT32_MAX;
        return;
    }

    int bestAst = -1;
    float bestAstDist = std::numeric_limits<float>::max();
    const float astRings[3] = { 48.0f, 100.0f, std::max(100.0f, s.viewRange) };
    prevR = 0.0f;
    for (float r : astRings) {
        const float radius = std::min(r, s.viewRange);
        if (radius <= prevR) continue;
        prevR = radius;
        astGrid_.query(s.pos, radius, [&](uint32_t idx) {
            if (idx >= asteroids_.size()) return;
            const Asteroid& a = asteroids_[idx];
            if (!a.active) return;
            const float d = (a.pos - s.pos).lengthSq();
            if (d < bestAstDist) { bestAstDist = d; bestAst = static_cast<int>(idx); }
        });
        if (bestAst >= 0) break;
        if (radius >= s.viewRange) break;
    }
    if (bestAst >= 0) {
        s.state = ShipState::Mining;
        s.targetAst = static_cast<uint32_t>(bestAst);
        s.targetAstGen = asteroids_[bestAst].gen;
        s.targetShip = UINT32_MAX;
        s.targetLight = UINT32_MAX;
        s.targetPortal = UINT32_MAX;
        return;
    }

    s.state = ShipState::Idle;
    s.targetShip = UINT32_MAX;
    s.targetAst = UINT32_MAX;
    s.targetLight = UINT32_MAX;
    s.targetPortal = UINT32_MAX;
}

void World::fireAtShip(int self, int target) {
    if (self < 0 || target < 0) return;
    Ship& s = ships_[self];
    Ship& t = ships_[target];
    if (!s.active || !t.active) return;
    if (s.fireTimer < 1.0f / std::max(0.1f, s.fireRate)) return;
    const Vec2 toTarget = t.pos - s.pos;
    const float dist = toTarget.length();
    if (dist > s.fireRange) return;
    const Vec2 dir = Vec2(1.0f, 0.0f).rotated(s.rot);
    if (std::acos(clampf(dir.dot(toTarget.normalized()), -1.0f, 1.0f)) > s.fireAngle) return;

    s.fireTimer = 0.0f;
    resolveBeam(self, s.pos + dir * SHIP_RADIUS, dir, s.fireRange);
}

void World::fireAtAsteroid(int self, int target) {
    if (self < 0 || target < 0) return;
    Ship& s = ships_[self];
    Asteroid& a = asteroids_[target];
    if (!s.active || !a.active) return;
    if (s.fireTimer < 1.0f / std::max(0.1f, s.fireRate)) return;
    const Vec2 toTarget = a.pos - s.pos;
    const float dist = toTarget.length();
    if (dist > s.fireRange) return;
    const Vec2 dir = Vec2(1.0f, 0.0f).rotated(s.rot);
    if (std::acos(clampf(dir.dot(toTarget.normalized()), -1.0f, 1.0f)) > s.fireAngle) return;

    s.fireTimer = 0.0f;
    resolveBeam(self, s.pos + dir * SHIP_RADIUS, dir, s.fireRange);
}

void World::fireAtPortal(int self, int target) {
    if (self < 0 || target < 0) return;
    if (target >= static_cast<int>(portals_.size())) return;
    Ship& s = ships_[self];
    const Portal& p = portals_[target];
    if (!s.active || !p.active) return;
    if (s.fireTimer < 1.0f / std::max(0.1f, s.fireRate)) return;
    const Vec2 toTarget = p.pos - s.pos;
    const float dist = toTarget.length();
    if (dist > s.fireRange + p.radius) return;
    const Vec2 dir = Vec2(1.0f, 0.0f).rotated(s.rot);
    if (std::acos(clampf(dir.dot(toTarget.normalized()), -1.0f, 1.0f)) > s.fireAngle) return;

    s.fireTimer = 0.0f;
    resolveBeam(self, s.pos + dir * SHIP_RADIUS, dir, s.fireRange + p.radius);
}

void World::resolveBeam(int self, Vec2 start, Vec2 dir, float maxDist) {
    if (self < 0 || self >= static_cast<int>(ships_.size())) return;
    Ship& shooter = ships_[self];
    if (!shooter.active) return;

    int hitType = 0; // 1=ships, 2=asteroid, 3=light, 4=portal
    int hitIndex = -1;
    float bestT = maxDist + 1.0f;
    Vec2 bestPoint = start + dir * maxDist;

    auto consider = [&](int type, int index, Vec2 pos, float radius) {
        const float t = rayCircleHit(start, dir, pos, radius);
        if (t < 0.0f || t > maxDist || t >= bestT) return;
        bestT = t;
        hitType = type;
        hitIndex = index;
        bestPoint = start + dir * t;
    };

    shipGrid_.queryRay(start, dir, maxDist, [&](uint32_t idx) {
        if (idx >= ships_.size() || static_cast<int>(idx) == self) return;
        const Ship& t = ships_[idx];
        if (!t.active) return;
        // 友军默认也会挡激光并吃伤害（真实）；F 可关闭友军伤害。
        if (t.faction == shooter.faction && !friendlyFireEnabled) return;
        consider(1, static_cast<int>(idx), t.pos, SHIP_RADIUS);
    });

    astGrid_.queryRay(start, dir, maxDist, [&](uint32_t idx) {
        if (idx >= asteroids_.size()) return;
        const Asteroid& a = asteroids_[idx];
        if (!a.active) return;
        consider(2, static_cast<int>(idx), a.pos, a.radius);
    });

    lightGrid_.queryRay(start, dir, maxDist, [&](uint32_t idx) {
        if (idx >= lights_.size()) return;
        const LightPoint& l = lights_[idx];
        if (!l.active || l.health <= 0.0f) return;
        consider(3, static_cast<int>(idx), l.pos, l.radius + 1.0f);
    });

    // 传送门没有碰撞箱，但会被激光击中；数量很少（<=8），直接遍历即可。
    for (size_t i = 0; i < portals_.size(); ++i) {
        const Portal& p = portals_[i];
        if (!p.active) continue;
        if (p.faction == shooter.faction && !friendlyFireEnabled) continue;
        consider(4, static_cast<int>(i), p.pos, p.radius);
    }

    if (hitType == 0) {
        addBeam(start, start + dir * maxDist, shooter.color);
        return;
    }

    addBeam(start, bestPoint, shooter.color);
    if (hitType == 1) {
        damageShip(hitIndex, shooter.damage, self, false);
    } else if (hitType == 2) {
        damageAsteroid(hitIndex, shooter.damage, bestPoint, self, shooter.miningExpertise);
    } else if (hitType == 4) {
        damagePortal(hitIndex, shooter.damage, self);
    } else {
        damageLight(hitIndex, shooter.damage);
    }
}

void World::damageShip(int index, float dmg, int attacker, bool collision) {
    if (index < 0 || index >= static_cast<int>(ships_.size())) return;
    Ship& s = ships_[index];
    if (!s.active) return;

    if (collision) {
        if (s.hitCooldown > 0.0f) return;
        const float absorb = std::min(s.armor, dmg * 0.7f);
        s.armor -= absorb;
        dmg -= absorb;
        s.hitCooldown = 0.25f;
    }
    if (dmg < 0.0f) dmg = 0.0f;

    const float applied = dmg;
    if (applied > 0.0f) {
        s.damageTaken += applied;
        ++s.hitsTaken;
        if (s.faction >= 0 && s.faction < MAX_FACTIONS) {
            auto& ft = factionTotals_[static_cast<size_t>(s.faction)];
            ft[FF_DamageTaken] += applied;
            ft[FF_HitsTaken] += 1.0;
        }
        if (attacker >= 0 && attacker < static_cast<int>(ships_.size()) && attacker != index) {
            Ship& a = ships_[attacker];
            if (a.active) {
                if (a.faction == s.faction) {
                    a.friendlyDamage += applied;
                    ++a.friendlyHits;
                    if (a.faction >= 0 && a.faction < MAX_FACTIONS) {
                        auto& ft = factionTotals_[static_cast<size_t>(a.faction)];
                        ft[FF_FriendlyDamage] += applied;
                        ft[FF_FriendlyHits] += 1.0;
                    }
                } else {
                    a.damageDealt += applied;
                    if (a.faction >= 0 && a.faction < MAX_FACTIONS) {
                        factionTotals_[static_cast<size_t>(a.faction)][FF_Output] += applied;
                    }
                }
            }
        }
    }
    s.health -= dmg;

    if (s.health > 0.0f) {
        // 被「不是自己当前目标」的敌舰偷袭：立刻进入逃跑。
        // 碰撞伤害是双向的，双方都知道对方，不触发单方面逃逸。
        if (!collision && attacker >= 0 && attacker != index &&
            attacker < static_cast<int>(ships_.size())) {
            Ship& a = ships_[attacker];
            if (a.active && a.faction != s.faction) {
                const bool alreadyEngaged = s.state == ShipState::Attacking &&
                                            s.targetShip == static_cast<uint32_t>(attacker) &&
                                            s.targetShipGen == a.gen;
                if (!alreadyEngaged) {
                    // 只在「刚被打断」时重置逃逸计时；已经在逃跑时只更新威胁方向，
                    // 否则追击者每打一枪都能把逃跑时间无限续上，船永远不敢回头。
                    if (s.state != ShipState::Fleeing) {
                        s.state = ShipState::Fleeing;
                        s.alertTimer = std::max(s.alertTimer,
                                                2.2f + std::min(1.2f, dmg * 0.08f));
                        s.targetShip = UINT32_MAX;
                        s.targetAst = UINT32_MAX;
                        s.targetLight = UINT32_MAX;
                    }
                    s.threatShip = static_cast<uint32_t>(attacker);
                    s.threatShipGen = a.gen;
                    s.threatPos = a.pos;
                }
            }
        }
        return;
    }

    s.health = 0.0f;
    s.active = false;
    s.deathHandled = false;
    archiveDeadShip(index);
    if (s.faction >= 0 && s.faction < MAX_FACTIONS) {
        factionTotals_[static_cast<size_t>(s.faction)][FF_Losses] += 1.0;
    }

    if (attacker >= 0 && attacker < static_cast<int>(ships_.size())) {
        Ship& a = ships_[attacker];
        if (a.active) {
            ++a.kills;
            if (a.faction >= 0 && a.faction < MAX_FACTIONS) {
                factionTotals_[static_cast<size_t>(a.faction)][FF_Kills] += 1.0;
            }
            if (a.kills >= 5 && a.title.empty()) a.title = randomTitle();
        }
    }
    if (selectedShip_ == index) clearSelection();
}

void World::damageAsteroid(int index, float dmg, Vec2 hitWorld, int attacker, float mining) {
    if (index < 0 || index >= static_cast<int>(asteroids_.size())) return;
    Asteroid& a = asteroids_[index];
    if (!a.active) return;

    const float reduced = dmg * (1.0f - a.cushion * 0.5f);
    a.health -= reduced;
    const Vec2 hitLocal = (hitWorld - a.pos).rotated(-a.rot);
    const float radiusFactor = 30.0f / (a.radius + 30.0f);
    int count = static_cast<int>((dmg / 10.0f) * (0.5f + radiusFactor));
    count = std::max(1, count);
    count = std::min(count, std::max(1, a.activePixels / 4));
    (void)attacker;
    erodeAsteroid(a, hitLocal, count, attacker, mining);

    if (a.health <= 0.0f) {
        a.health = 0.0f;
        a.active = false;
        a.deathHandled = false;
    }
}

void World::damageLight(int index, float dmg) {
    if (index < 0 || index >= static_cast<int>(lights_.size())) return;
    LightPoint& l = lights_[index];
    if (!l.active) return;
    l.health -= dmg;
    if (l.health <= 0.0f) {
        l.health = 0.0f;
        l.active = false;
    }
}

void World::damagePortal(int index, float dmg, int attacker) {
    if (index < 0 || index >= static_cast<int>(portals_.size())) return;
    Portal& p = portals_[index];
    if (!p.active || dmg <= 0.0f) return;

    p.health -= dmg;
    p.flashTimer = std::max(p.flashTimer, 0.18f);

    if (attacker >= 0 && attacker < static_cast<int>(ships_.size())) {
        Ship& a = ships_[attacker];
        if (a.active) {
            if (p.faction >= 0 && a.faction == p.faction) {
                a.friendlyDamage += dmg;
                ++a.friendlyHits;
                if (a.faction >= 0 && a.faction < MAX_FACTIONS) {
                    auto& ft = factionTotals_[static_cast<size_t>(a.faction)];
                    ft[FF_FriendlyDamage] += dmg;
                    ft[FF_FriendlyHits] += 1.0;
                }
            } else {
                a.damageDealt += dmg;
                if (a.faction >= 0 && a.faction < MAX_FACTIONS) {
                    factionTotals_[static_cast<size_t>(a.faction)][FF_Output] += dmg;
                }
            }
        }
    }

    if (p.health > 0.0f) return;
    p.health = 0.0f;
    p.active = false;
    addLightBurst(p.pos, p.color, 26, 150.0f);

    if (attacker >= 0 && attacker < static_cast<int>(ships_.size())) {
        Ship& a = ships_[attacker];
        if (a.active) {
            ++a.kills;
            if (a.faction >= 0 && a.faction < MAX_FACTIONS) {
                factionTotals_[static_cast<size_t>(a.faction)][FF_Kills] += 1.0;
            }
            if (a.kills >= 5 && a.title.empty()) a.title = randomTitle();
        }
    }
}

void World::erodeAsteroid(Asteroid& a, Vec2 hitLocal, int count, int attacker, float mining) {
    if (count <= 0 || a.activePixels <= 0) return;

    erodeScratch_.clear();
    for (size_t i = 0; i < a.pixels.size(); ++i) {
        if (a.pixels[i].active) erodeScratch_.push_back(static_cast<uint32_t>(i));
    }
    if (erodeScratch_.empty()) return;

    const int n = std::min(count, static_cast<int>(erodeScratch_.size()));
    auto dist2 = [&](uint32_t idx) {
        const float dx = static_cast<float>(a.pixels[idx].gx) * a.pixelSize - hitLocal.x;
        const float dy = static_cast<float>(a.pixels[idx].gy) * a.pixelSize - hitLocal.y;
        return dx * dx + dy * dy;
    };
    std::nth_element(erodeScratch_.begin(), erodeScratch_.begin() + n, erodeScratch_.end(),
                     [&](uint32_t x, uint32_t y) { return dist2(x) < dist2(y); });

    for (int i = 0; i < n; ++i) {
        Pixel& p = a.pixels[erodeScratch_[static_cast<size_t>(i)]];
        if (!p.active) continue;
        p.active = false;
        --a.activePixels;
        if (attacker >= 0 && attacker < static_cast<int>(ships_.size())) {
            Ship& miner = ships_[static_cast<size_t>(attacker)];
            if (miner.active) {
                ++miner.mined;
                if (miner.faction >= 0 && miner.faction < MAX_FACTIONS) {
                    factionTotals_[static_cast<size_t>(miner.faction)][FF_Mined] += 1.0;
                }
            }
        }
        a.image.setPixel(static_cast<unsigned>(p.gx + a.gridN),
                         static_cast<unsigned>(p.gy + a.gridN), sf::Color::Transparent);
        a.imageDirty = true;

        const float chance = 0.05f + mining;
        if (rng_.f() < chance) {
            const Vec2 local(static_cast<float>(p.gx) * a.pixelSize, static_cast<float>(p.gy) * a.pixelSize);
            const Vec2 world = a.pos + local.rotated(a.rot);
            const Vec2 dir = (world - a.pos).normalized();
            spawnLight(world, a.vel + dir * rng_.f(1.0f, 4.0f), sf::Color(p.r, p.g, p.b));
        }
    }

    a.mass = a.baseMass * (a.totalPixels > 0 ? static_cast<float>(a.activePixels) / a.totalPixels : 0.0f);
    if (a.activePixels <= 0) {
        a.health = 0.0f;
        a.active = false;
        a.deathHandled = false;
    }
}

void World::killShipNow(int index) {
    if (index < 0 || index >= static_cast<int>(ships_.size())) return;
    Ship& s = ships_[index];
    if (!s.active) return;
    s.health = 0.0f;
    s.active = false;
    s.deathHandled = false;
    archiveDeadShip(index);
    if (s.faction >= 0 && s.faction < MAX_FACTIONS) {
        factionTotals_[static_cast<size_t>(s.faction)][FF_Losses] += 1.0;
    }
}

void World::killAsteroidNow(int index) {
    if (index < 0 || index >= static_cast<int>(asteroids_.size())) return;
    Asteroid& a = asteroids_[index];
    if (!a.active) return;
    a.health = 0.0f;
    a.active = false;
    a.deathHandled = false;
}

void World::spawnShipFragments(const Ship& s) {
    addLightBurst(s.pos, s.color, 4 + rng_.i(0, 4), 22.0f);
    // spawnAsteroid 自己会处理上限和空闲槽；不要再拿 asteroids_.size() 当上限，
    // 否则池子一旦涨到 360，之后的飞船残骸就永远生成不出来。
    if (rng_.chance(0.35f)) {
        const Vec2 dir(std::cos(rng_.f(0.0f, TAU)), std::sin(rng_.f(0.0f, TAU)));
        spawnAsteroid(s.pos + dir * 6.0f, s.vel + dir * 18.0f, rng_.f(5.0f, 9.0f), AsteroidType::Normal);
    }
}

void World::cleanup() {
    for (size_t i = 0; i < ships_.size(); ++i) {
        Ship& s = ships_[i];
        if (s.active || s.deathHandled) continue;
        s.deathHandled = true;
        if (s.faction >= 0 && s.faction < MAX_FACTIONS) {
            // 损失已在死亡瞬间记录；这里只处理碎片与回收
        }
        spawnShipFragments(s);
        if (selectedShip_ == static_cast<int>(i)) clearSelection();
        freeShips_.push_back(static_cast<uint32_t>(i));
    }

    for (size_t i = 0; i < asteroids_.size(); ++i) {
        Asteroid& a = asteroids_[i];
        if (a.active || a.deathHandled) continue;
        a.deathHandled = true;

        // health > 0 表示它是「飞出窗口被回收」，不是被打碎：不生成碎片/光点。
        const bool destroyed = a.health <= 0.0f;
        if (destroyed && a.type == AsteroidType::Normal && a.radius > 12.0f) {
            const int fragments = std::min(4, 2 + static_cast<int>(a.radius / 40.0f));
            for (int k = 0; k < fragments; ++k) {
                const float ang = rng_.f(0.0f, TAU);
                const Vec2 dir(std::cos(ang), std::sin(ang));
                const Vec2 p = a.pos + dir * (a.radius * 0.35f);
                const Vec2 v = a.vel + dir * rng_.f(10.0f, 30.0f);
                if (spawnAsteroid(p, v, std::max(5.0f, a.radius * 0.35f), AsteroidType::Normal) == UINT32_MAX) break;
            }
        }
        if (destroyed) addLightBurst(a.pos, sf::Color(180, 200, 255), 1 + rng_.i(0, 2), 12.0f);
        freeAsteroids_.push_back(static_cast<uint32_t>(i));
    }

    for (size_t i = 0; i < lights_.size(); ++i) {
        LightPoint& l = lights_[i];
        if (l.active || l.freed) continue;
        l.freed = true;
        freeLights_.push_back(static_cast<uint32_t>(i));
    }
}

void World::updateCollisions() {
    using Clock = std::chrono::steady_clock;
    auto mark = [&]() { return profile ? Clock::now() : Clock::time_point{}; };
    auto add = [&](double& slot, Clock::time_point a) {
        if (profile) slot += std::chrono::duration<double, std::milli>(Clock::now() - a).count();
    };

    const size_t shipN = ships_.size();

    // 飞船 vs 飞船
    auto c0 = mark();
    for (int f = 0; f < MAX_FACTIONS; ++f) {
        for (size_t i = 0; i < shipN; ++i) {
            Ship& a = ships_[i];
            if (!a.active || a.faction != f) continue;
            // 只查询阵营编号更大的网格：每对异阵营飞船只处理一次。
            for (int g = f + 1; g < MAX_FACTIONS; ++g) {
                collisionGrids_[static_cast<size_t>(g)].query(a.pos, SHIP_RADIUS * 2.0f + 2.0f, [&](uint32_t j) {
                    if (j >= ships_.size()) return;
                    Ship& b = ships_[j];
                    if (!b.active) return;
                    ++collShipShipPairs;
                    Vec2 delta = b.pos - a.pos;
                    const float minDist = SHIP_RADIUS * 2.0f;
                    const float distSq = delta.lengthSq();
                    if (distSq >= minDist * minDist) return;
                    float dist = std::sqrt(distSq);
                    if (dist < 1e-3f) { delta = {1, 0}; dist = 1e-3f; }
                    const Vec2 normal = delta / dist;
                    const float overlap = minDist - dist;
                    a.pos -= normal * (overlap * 0.5f);
                    b.pos += normal * (overlap * 0.5f);

                    const Vec2 relVel = b.vel - a.vel;
                    const float vn = relVel.dot(normal);
                    if (vn < 0.0f) {
                        const float invA = 1.0f / a.mass;
                        const float invB = 1.0f / b.mass;
                        const float impulse = -(1.0f + 0.3f) * vn / (invA + invB);
                        a.vel -= normal * (impulse * invA);
                        b.vel += normal * (impulse * invB);
                    }
                    if (a.hitCooldown <= 0.0f && b.hitCooldown <= 0.0f) {
                        const float impact = std::max(0.0f, -vn);
                        if (impact > 4.0f) {
                            const float dmg = 1.0f + impact * 0.35f;
                            damageShip(static_cast<int>(i), dmg, static_cast<int>(j), true);
                            damageShip(static_cast<int>(j), dmg, static_cast<int>(i), true);
                        }
                    }
                });
            }
        }
    }
    add(collShipShipMs, c0);

    // 友军 vs 友军：可开关（默认关，万艘混战性能取舍）。
    // 只做分离/推挤，不扣血；j <= i 保证每对只处理一次。
    if (friendlyCollisionEnabled) {
        for (int f = 0; f < MAX_FACTIONS; ++f) {
            for (size_t i = 0; i < shipN; ++i) {
                Ship& a = ships_[i];
                if (!a.active || a.faction != f) continue;
                collisionGrids_[static_cast<size_t>(f)].query(a.pos, SHIP_RADIUS * 2.0f + 2.0f, [&](uint32_t j) {
                    if (j <= i || j >= ships_.size()) return;
                    Ship& b = ships_[j];
                    if (!b.active || b.faction != f) return;
                    Vec2 delta = b.pos - a.pos;
                    const float minDist = SHIP_RADIUS * 2.0f;
                    const float distSq = delta.lengthSq();
                    if (distSq >= minDist * minDist) return;
                    float dist = std::sqrt(distSq);
                    if (dist < 1e-3f) { delta = {1, 0}; dist = 1e-3f; }
                    const Vec2 normal = delta / dist;
                    const float overlap = minDist - dist;
                    a.pos -= normal * (overlap * 0.5f);
                    b.pos += normal * (overlap * 0.5f);

                    const Vec2 relVel = b.vel - a.vel;
                    const float vn = relVel.dot(normal);
                    if (vn < 0.0f) {
                        const float invA = 1.0f / a.mass;
                        const float invB = 1.0f / b.mass;
                        const float impulse = -(1.0f + 0.2f) * vn / (invA + invB);
                        a.vel -= normal * (impulse * invA);
                        b.vel += normal * (impulse * invB);
                    }
                    if (friendlyFireEnabled && a.hitCooldown <= 0.0f && b.hitCooldown <= 0.0f) {
                        const float impact = std::max(0.0f, -vn);
                        if (impact > 4.0f) {
                            const float dmg = 1.0f + impact * 0.35f;
                            damageShip(static_cast<int>(i), dmg, static_cast<int>(j), true);
                            damageShip(static_cast<int>(j), dmg, static_cast<int>(i), true);
                        }
                    }
                });
            }
        }
    }

    // 飞船 vs 小行星
    auto c1 = mark();
    for (size_t i = 0; i < shipN; ++i) {
        Ship& s = ships_[i];
        if (!s.active) continue;
        astGrid_.query(s.pos, SHIP_RADIUS + 2.0f, [&](uint32_t j) {
            if (j >= asteroids_.size()) return;
            Asteroid& a = asteroids_[j];
            if (!a.active) return;
            ++collShipAstPairs;
            Vec2 delta = s.pos - a.pos;
            const float minDist = a.radius + SHIP_RADIUS;
            const float distSq = delta.lengthSq();
            if (distSq >= minDist * minDist) return;
            float dist = std::sqrt(distSq);
            if (dist < 1e-3f) { delta = {1, 0}; dist = 1e-3f; }
            const Vec2 normal = delta / dist;
            const float overlap = minDist - dist;
            s.pos += normal * overlap;
            s.pos.x = clampf(s.pos.x, SHIP_RADIUS, worldWidth_ - SHIP_RADIUS);
            s.pos.y = clampf(s.pos.y, SHIP_RADIUS, worldHeight_ - SHIP_RADIUS);

            // 关键：用相对速度，而不是飞船自己的速度。
            // 旧版 s.vel.dot(normal) 在“彗星撞静止船”时恒为 0，所以只推不伤。
            const Vec2 relVel = s.vel - a.vel;
            const float vn = relVel.dot(normal);
            if (vn < 0.0f) {
                const float e = 0.35f;
                s.vel -= normal * ((1.0f + e) * vn);
            }
            if (s.hitCooldown <= 0.0f) {
                const float impact = std::max(0.0f, -vn);
                if (impact > 3.0f) {
                    // 相对速度决定伤害：彗星 280–430，一记重创但通常不秒杀；
                    // 普通小行星是擦伤。旧版用飞船自身速度，静止船被彗星撞也恒为 0。
                    const float scale = (a.type == AsteroidType::Comet) ? 0.35f : 0.4f;
                    damageShip(static_cast<int>(i), 2.0f + impact * scale, -1, true);
                }
                if (a.hitCooldown <= 0.0f) {
                    const Vec2 hit = a.pos + normal * (a.radius * 0.8f);
                    damageAsteroid(static_cast<int>(j), 1.0f + impact * 0.05f, hit, -1, 0.0f);
                    a.hitCooldown = 0.2f;
                }
            }
        });
    }
    add(collShipAstMs, c1);

    // 小行星 / 彗星 vs 光点：光点有血量，对碰撞抗性比激光高。
    // 只遍历小行星，再用 lightGrid_ 查附近光点（光点最多 12000，反着查太贵）。
    for (size_t i = 0; i < asteroids_.size(); ++i) {
        Asteroid& a = asteroids_[i];
        if (!a.active) continue;
        lightGrid_.query(a.pos, a.radius + 4.0f, [&](uint32_t j) {
            if (j >= lights_.size()) return;
            LightPoint& l = lights_[j];
            if (!l.active || l.hitCooldown > 0.0f) return;
            const float minDist = a.radius + l.radius;
            const Vec2 delta = l.pos - a.pos;
            if (delta.lengthSq() >= minDist * minDist) return;

            const Vec2 relVel = a.vel - l.vel;
            const float impact = relVel.length();
            const float dmg = impact * 0.02f; // 碰撞抗性：普通石头擦过不会秒碎
            if (dmg < 0.05f) return;
            l.health -= dmg;
            l.hitCooldown = 0.25f;
            if (l.health <= 0.0f) {
                l.health = 0.0f;
                l.active = false;
            }
        });
    }

    // 小行星 vs 小行星
    auto c2 = mark();
    const size_t astN = asteroids_.size();
    for (size_t i = 0; i < astN; ++i) {
        Asteroid& a = asteroids_[i];
        if (!a.active) continue;
        astGrid_.query(a.pos, a.radius, [&](uint32_t j) {
            if (j <= i || j >= asteroids_.size()) return;
            Asteroid& b = asteroids_[j];
            if (!b.active) return;
            ++collAstAstPairs;
            Vec2 delta = b.pos - a.pos;
            const float minDist = a.radius + b.radius;
            const float distSq = delta.lengthSq();
            if (distSq >= minDist * minDist) return;
            float dist = std::sqrt(distSq);
            if (dist < 1e-3f) { delta = {1, 0}; dist = 1e-3f; }
            const Vec2 normal = delta / dist;
            const float overlap = minDist - dist;
            const float invA = 1.0f / std::max(0.1f, a.mass);
            const float invB = 1.0f / std::max(0.1f, b.mass);
            const float totalInv = invA + invB;
            a.pos -= normal * (overlap * (invA / totalInv));
            b.pos += normal * (overlap * (invB / totalInv));

            const Vec2 relVel = b.vel - a.vel;
            const float vn = relVel.dot(normal);
            if (vn < 0.0f) {
                const float impulse = -(1.0f + 0.2f) * vn / totalInv;
                a.vel -= normal * (impulse * invA);
                b.vel += normal * (impulse * invB);
            }
            if (a.hitCooldown <= 0.0f && b.hitCooldown <= 0.0f) {
                const float impact = std::max(0.0f, -vn);
                if (impact > 2.0f) {
                    const Vec2 hit = a.pos + normal * (a.radius * 0.8f);
                    damageAsteroid(static_cast<int>(i), impact * 0.6f, hit, -1, 0.0f);
                    damageAsteroid(static_cast<int>(j), impact * 0.6f, hit, -1, 0.0f);
                    a.hitCooldown = b.hitCooldown = 0.25f;
                }
            }
        });
    }
    add(collAstAstMs, c2);
}

void World::addBeam(Vec2 a, Vec2 b, sf::Color color) {
    if (beams_.size() >= static_cast<size_t>(MAX_BEAMS)) return;
    Beam beam;
    beam.a = a;
    beam.b = b;
    beam.color = color;
    beam.life = 0.08f;
    beam.maxLife = 0.08f;
    beams_.push_back(beam);
}

int World::shipSlotByHandle(uint32_t index, uint32_t gen) const {
    if (index >= ships_.size()) return -1;
    if (!ships_[index].active || ships_[index].gen != gen) return -1;
    return static_cast<int>(index);
}

int World::asteroidSlotByHandle(uint32_t index, uint32_t gen) const {
    if (index >= asteroids_.size()) return -1;
    if (!asteroids_[index].active || asteroids_[index].gen != gen) return -1;
    return static_cast<int>(index);
}

int World::pickShip(Vec2 p) const {
    int best = -1;
    float bestDist = 12.0f * 12.0f;
    const_cast<Grid&>(shipGrid_).query(p, 12.0f, [&](uint32_t idx) {
        if (idx >= ships_.size()) return;
        const Ship& s = ships_[idx];
        if (!s.active) return;
        const float d = (s.pos - p).lengthSq();
        if (d < bestDist) { bestDist = d; best = static_cast<int>(idx); }
    });
    return best;
}

const Ship* World::shipAt(int index) const {
    if (index < 0 || index >= static_cast<int>(ships_.size())) return nullptr;
    const Ship& s = ships_[index];
    return s.active ? &s : nullptr;
}

Ship* World::shipAt(int index) {
    if (index < 0 || index >= static_cast<int>(ships_.size())) return nullptr;
    Ship& s = ships_[index];
    return s.active ? &s : nullptr;
}

const Asteroid* World::asteroidAt(int index) const {
    if (index < 0 || index >= static_cast<int>(asteroids_.size())) return nullptr;
    const Asteroid& a = asteroids_[index];
    return a.active ? &a : nullptr;
}

uint32_t World::shipGen(int index) const {
    if (index < 0 || index >= static_cast<int>(ships_.size())) return 0;
    return ships_[index].gen;
}

void World::selectShip(int index) {
    clearSelection();
    if (index < 0 || index >= static_cast<int>(ships_.size())) return;
    Ship& s = ships_[index];
    if (!s.active) return;
    s.selected = true;
    selectedShip_ = index;
    selectedGen_ = s.gen;
}

void World::clearSelection() {
    if (selectedShip_ >= 0 && selectedShip_ < static_cast<int>(ships_.size())) {
        ships_[selectedShip_].selected = false;
    }
    selectedShip_ = -1;
    selectedGen_ = 0;
}

const Ship* World::selected() const {
    if (selectedShip_ < 0 || selectedShip_ >= static_cast<int>(ships_.size())) return nullptr;
    const Ship& s = ships_[selectedShip_];
    if (!s.active || s.gen != selectedGen_) return nullptr;
    return &s;
}

int World::shipCount(int faction) const {
    if (faction < 0 || faction >= MAX_FACTIONS) return 0;
    int n = 0;
    for (const auto& s : ships_) if (s.active && s.faction == faction) ++n;
    return n;
}

float World::resource(int faction) const {
    if (faction < 0 || faction >= MAX_FACTIONS) return 0.0f;
    return resources_[faction];
}

int World::individualFieldOf(IndividualMetric metric) {
    switch (metric) {
    case IndividualMetric::Kills: return IF_Kills;
    case IndividualMetric::Output: return IF_Output;
    case IndividualMetric::Collected: return IF_Collected;
    case IndividualMetric::Mined: return IF_Mined;
    case IndividualMetric::FriendlyHits: return IF_FriendlyHits;
    case IndividualMetric::FriendlyDamage: return IF_FriendlyDamage;
    case IndividualMetric::DamageTaken: return IF_DamageTaken;
    case IndividualMetric::HitsTaken: return IF_HitsTaken;
    default: return -1;
    }
}

double World::individualTotalValue(const Ship& s, IndividualMetric metric) const {
    switch (metric) {
    case IndividualMetric::Level: return s.level;
    case IndividualMetric::Kills: return s.kills;
    case IndividualMetric::Output: return s.damageDealt;
    case IndividualMetric::Collected: return s.collected;
    case IndividualMetric::Mined: return s.mined;
    case IndividualMetric::FriendlyHits: return s.friendlyHits;
    case IndividualMetric::FriendlyDamage: return s.friendlyDamage;
    case IndividualMetric::DamageTaken: return s.damageTaken;
    case IndividualMetric::HitsTaken: return s.hitsTaken;
    case IndividualMetric::Health: return s.health;
    case IndividualMetric::Armor: return s.armor;
    case IndividualMetric::Age: return s.age;
    case IndividualMetric::KillsPerLevel:
        return static_cast<double>(s.kills) / std::max(1, s.level);
    case IndividualMetric::OutputPerMinute:
        return s.age > 0.1f ? s.damageDealt / (s.age / 60.0f) : 0.0;
    default: return 0.0;
    }
}

double World::individualMetricValue(int shipIndex, IndividualMetric metric, StatRange range) const {
    if (shipIndex < 0 || shipIndex >= static_cast<int>(ships_.size())) return 0.0;
    const Ship& s = ships_[static_cast<size_t>(shipIndex)];
    if (!s.active) return 0.0;

    const double total = individualTotalValue(s, metric);
    if (range == StatRange::Total || range == StatRange::Peak) return total;

    const int field = individualFieldOf(metric);
    const int w = static_cast<int>(range) - 1;
    if (field < 0 || w < 0 || w >= 3) return total;
    if (shipIndex >= static_cast<int>(shipStats_.size())) return 0.0;
    const ShipStatHistory& h = shipStats_[static_cast<size_t>(shipIndex)];
    if (h.serial != s.serial) return 0.0;

    const double partial = total - static_cast<double>(h.last[static_cast<size_t>(field)]);
    const double v = static_cast<double>(h.recent[static_cast<size_t>(w)][static_cast<size_t>(field)]) + partial;
    return v > 0.0 ? v : 0.0;
}

void World::archiveDeadShip(int index) {
    if (index < 0 || index >= static_cast<int>(ships_.size())) return;
    const Ship& s = ships_[static_cast<size_t>(index)];
    DeadShipRecord r;
    r.serial = s.serial;
    r.faction = s.faction;
    r.factionId = (s.faction >= 0 && s.faction < MAX_FACTIONS)
                      ? factionIds_[static_cast<size_t>(s.faction)]
                      : 0;
    r.level = s.level;
    r.age = s.age;
    r.maxHealth = s.maxHealth;
    r.maxArmor = s.maxArmor;
    r.title = s.title;
    r.kills = s.kills;
    r.output = s.damageDealt;
    r.collected = s.collected;
    r.mined = s.mined;
    r.friendlyHits = s.friendlyHits;
    r.friendlyDamage = s.friendlyDamage;
    r.damageTaken = s.damageTaken;
    r.hitsTaken = s.hitsTaken;

    // 保存死亡瞬间的三个最近窗口，让“显示淘汰者”在近 N 分钟里也能看到他们。
    if (index < static_cast<int>(shipStats_.size())) {
        const ShipStatHistory& h = shipStats_[static_cast<size_t>(index)];
        if (h.serial == s.serial) {
            std::array<float, IF_Count> cur{};
            cur[IF_Kills] = static_cast<float>(s.kills);
            cur[IF_Output] = s.damageDealt;
            cur[IF_Collected] = static_cast<float>(s.collected);
            cur[IF_Mined] = static_cast<float>(s.mined);
            cur[IF_FriendlyHits] = static_cast<float>(s.friendlyHits);
            cur[IF_FriendlyDamage] = s.friendlyDamage;
            cur[IF_DamageTaken] = s.damageTaken;
            cur[IF_HitsTaken] = static_cast<float>(s.hitsTaken);
            for (int w = 0; w < 3; ++w) {
                for (int k = 0; k < IF_Count; ++k) {
                    const float partial = cur[static_cast<size_t>(k)] -
                                          h.last[static_cast<size_t>(k)];
                    r.recent[static_cast<size_t>(w)][static_cast<size_t>(k)] =
                        std::max(0.0f, h.recent[static_cast<size_t>(w)][static_cast<size_t>(k)] + partial);
                }
            }
        }
    }
    deadShips_.push_back(std::move(r));
}

double World::deadMetricValue(const DeadShipRecord& record, IndividualMetric metric,
                              StatRange range) const {
    const int field = individualFieldOf(metric);
    const int w = static_cast<int>(range) - 1;
    if (field >= 0 && w >= 0 && w < 3) {
        const double v = record.recent[static_cast<size_t>(w)][static_cast<size_t>(field)];
        return v > 0.0 ? v : 0.0;
    }
    switch (metric) {
    case IndividualMetric::Level: return record.level;
    case IndividualMetric::Kills: return record.kills;
    case IndividualMetric::Output: return record.output;
    case IndividualMetric::Collected: return record.collected;
    case IndividualMetric::Mined: return record.mined;
    case IndividualMetric::FriendlyHits: return record.friendlyHits;
    case IndividualMetric::FriendlyDamage: return record.friendlyDamage;
    case IndividualMetric::DamageTaken: return record.damageTaken;
    case IndividualMetric::HitsTaken: return record.hitsTaken;
    case IndividualMetric::Health: return record.maxHealth;
    case IndividualMetric::Armor: return record.maxArmor;
    case IndividualMetric::Age: return record.age;
    case IndividualMetric::KillsPerLevel:
        return static_cast<double>(record.kills) / std::max(1, record.level);
    case IndividualMetric::OutputPerMinute:
        return record.age > 0.1f ? record.output / (record.age / 60.0f) : 0.0;
    default: return 0.0;
    }
}

const std::vector<IndividualRank>& World::topIndividuals(IndividualMetric metric, StatRange range,
                                                         const IndividualFilter& filter, int count,
                                                         bool includeEliminated) const {
    count = std::clamp(count, 1, 32);
    const uint64_t key = static_cast<uint64_t>(metric) |
                         (static_cast<uint64_t>(range) << 8) |
                         (static_cast<uint64_t>(filter.faction + 1) << 12) |
                         (static_cast<uint64_t>(filter.minLevel) << 16) |
                         (filter.titlesOnly ? (1ull << 24) : 0ull) |
                         (static_cast<uint64_t>(count) << 25) |
                         (includeEliminated ? (1ull << 31) : 0ull);

    for (const RankCache& c : rankCaches_) {
        if (c.key == key && c.version == stats_.version && c.count == count) return c.rows;
    }

    std::array<IndividualRank, 32> best{};
    int n = 0;
    auto better = [](const IndividualRank& a, const IndividualRank& b) {
        if (a.value != b.value) return a.value > b.value;
        return a.serial < b.serial;
    };
    auto consider = [&](uint32_t serial, int faction, uint64_t factionId, int level,
                        const std::string& title, double value) {
        if (n < count) {
            IndividualRank& r = best[static_cast<size_t>(n++)];
            r.key = serial;
            r.serial = serial;
            r.faction = faction;
            r.factionId = factionId;
            r.level = level;
            r.title = title;
            r.value = value;
        } else {
            int worst = 0;
            for (int k = 1; k < n; ++k) {
                if (better(best[static_cast<size_t>(worst)], best[static_cast<size_t>(k)])) worst = k;
            }
            const IndividualRank& w = best[static_cast<size_t>(worst)];
            // 只有真正能挤进前 K 的个体才复制 title，避免 10k 船每 0.1 秒
            // 每窗都做上万次 std::string 拷贝。
            if (value > w.value || (value == w.value && serial < w.serial)) {
                IndividualRank r;
                r.key = serial;
                r.serial = serial;
                r.faction = faction;
                r.factionId = factionId;
                r.level = level;
                r.title = title;
                r.value = value;
                best[static_cast<size_t>(worst)] = std::move(r);
            }
        }
    };

    for (size_t i = 0; i < ships_.size(); ++i) {
        const Ship& s = ships_[i];
        if (!s.active) continue;
        if (filter.faction >= 0 && s.faction != filter.faction) continue;
        if (s.level < filter.minLevel) continue;
        if (filter.titlesOnly && s.title.empty()) continue;
        consider(s.serial, s.faction,
                 (s.faction >= 0 && s.faction < MAX_FACTIONS)
                     ? factionIds_[static_cast<size_t>(s.faction)]
                     : 0,
                 s.level, s.title,
                 individualMetricValue(static_cast<int>(i), metric, range));
    }

    if (includeEliminated) {
        for (const DeadShipRecord& d : deadShips_) {
            if (filter.faction >= 0 && d.faction != filter.faction) continue;
            if (d.level < filter.minLevel) continue;
            if (filter.titlesOnly && d.title.empty()) continue;
            consider(d.serial, d.faction, d.factionId, d.level, d.title,
                     deadMetricValue(d, metric, range));
        }
    }

    std::sort(best.begin(), best.begin() + n, better);

    RankCache cache;
    cache.key = key;
    cache.version = stats_.version;
    cache.count = count;
    cache.rows.assign(best.begin(), best.begin() + n);
    if (rankCaches_.size() >= 64) rankCaches_.erase(rankCaches_.begin());
    rankCaches_.push_back(std::move(cache));
    return rankCaches_.back().rows;
}

void World::ensureShipStatSlot(uint32_t index, uint32_t serial) {
    if (shipStats_.size() <= index) shipStats_.resize(static_cast<size_t>(index) + 1);
    ShipStatHistory& h = shipStats_[static_cast<size_t>(index)];
    h = ShipStatHistory{};
    h.serial = serial;
}

void World::sampleStatsBuckets() {
    // 阵营：累计字段做 10 秒差分，环形桶 + 三个窗口的滚动和。
    for (int f = 0; f < MAX_FACTIONS; ++f) {
        FactionStatHistory& h = factionStats_[static_cast<size_t>(f)];
        std::array<float, FF_Count> d{};
        for (int k = 0; k < FF_Count; ++k) {
            const double cur = factionTotals_[static_cast<size_t>(f)][static_cast<size_t>(k)];
            d[static_cast<size_t>(k)] = static_cast<float>(std::max(0.0, cur - h.last[static_cast<size_t>(k)]));
            h.last[static_cast<size_t>(k)] = cur;
        }
        const int idx = h.head;
        h.buckets[static_cast<size_t>(idx)] = d;
        h.head = static_cast<uint8_t>((idx + 1) % 60);
        if (h.filled < 60) ++h.filled;

        static const int kWindows[3] = { 6, 30, 59 };
        for (int w = 0; w < 3; ++w) {
            const int win = kWindows[w];
            for (int k = 0; k < FF_Count; ++k) {
                h.recent[static_cast<size_t>(w)][static_cast<size_t>(k)] += d[static_cast<size_t>(k)];
                if (h.filled > win) {
                    const int old = (h.head - win - 1 + 60) % 60;
                    h.recent[static_cast<size_t>(w)][static_cast<size_t>(k)] -=
                        h.buckets[static_cast<size_t>(old)][static_cast<size_t>(k)];
                }
            }
        }
    }

    // 个体：只对活跃船做差分；槽位复用时 ensureShipStatSlot 会清空。
    for (size_t i = 0; i < ships_.size(); ++i) {
        const Ship& s = ships_[i];
        if (!s.active) continue;
        if (i >= shipStats_.size()) continue;
        ShipStatHistory& h = shipStats_[i];
        if (h.serial != s.serial) {
            h = ShipStatHistory{};
            h.serial = s.serial;
            continue;
        }

        std::array<float, IF_Count> d{};
        d[IF_Kills] = static_cast<float>(s.kills);
        d[IF_Output] = s.damageDealt;
        d[IF_Collected] = static_cast<float>(s.collected);
        d[IF_Mined] = static_cast<float>(s.mined);
        d[IF_FriendlyHits] = static_cast<float>(s.friendlyHits);
        d[IF_FriendlyDamage] = s.friendlyDamage;
        d[IF_DamageTaken] = s.damageTaken;
        d[IF_HitsTaken] = static_cast<float>(s.hitsTaken);

        const int idx = h.head;
        for (int k = 0; k < IF_Count; ++k) {
            const float delta = std::max(0.0f, d[static_cast<size_t>(k)] - h.last[static_cast<size_t>(k)]);
            h.last[static_cast<size_t>(k)] = d[static_cast<size_t>(k)];
            h.buckets[static_cast<size_t>(idx)][static_cast<size_t>(k)] = delta;
        }
        h.head = static_cast<uint8_t>((idx + 1) % 60);
        if (h.filled < 60) ++h.filled;

        static const int kWindows[3] = { 6, 30, 59 };
        for (int w = 0; w < 3; ++w) {
            const int win = kWindows[w];
            for (int k = 0; k < IF_Count; ++k) {
                const float delta = h.buckets[static_cast<size_t>(idx)][static_cast<size_t>(k)];
                h.recent[static_cast<size_t>(w)][static_cast<size_t>(k)] += delta;
                if (h.filled > win) {
                    const int old = (h.head - win - 1 + 60) % 60;
                    h.recent[static_cast<size_t>(w)][static_cast<size_t>(k)] -=
                        h.buckets[static_cast<size_t>(old)][static_cast<size_t>(k)];
                }
            }
        }
    }
}

void World::refreshStats() {
    struct Agg {
        int ships = 0;
        int levelSum = 0;
        int maxLevel = 0;
        double health = 0.0;
        double armor = 0.0;
        double kills = 0.0;
    };
    std::array<Agg, MAX_FACTIONS> agg{};

    for (const Ship& s : ships_) {
        if (!s.active || s.faction < 0 || s.faction >= MAX_FACTIONS) continue;
        Agg& a = agg[static_cast<size_t>(s.faction)];
        ++a.ships;
        a.levelSum += s.level;
        a.maxLevel = std::max(a.maxLevel, s.level);
        a.health += s.health;
        a.armor += s.armor;
        a.kills += s.kills;
    }

    ++stats_.version;
    stats_.step = stepCount;

    // 重生：槽位从 0 艘重新出现时，视为一个全新的阵营身份。
    // 先重置累计值/峰值，再在下面重新填当前行；旧身份已进入 eliminatedFactions_。
    for (int f = 0; f < MAX_FACTIONS; ++f) {
        if (!factionAlive_[static_cast<size_t>(f)] && agg[static_cast<size_t>(f)].ships > 0) {
            assignFactionIdentity(f, true);
            resources_[static_cast<size_t>(f)] = 0.0f;
        }
    }

    auto factionFieldOf = [](FactionMetric m) -> int {
        switch (m) {
        case FactionMetric::Kills: return FF_Kills;
        case FactionMetric::Output: return FF_Output;
        case FactionMetric::Collected: return FF_Collected;
        case FactionMetric::Mined: return FF_Mined;
        case FactionMetric::FriendlyHits: return FF_FriendlyHits;
        case FactionMetric::FriendlyDamage: return FF_FriendlyDamage;
        case FactionMetric::DamageTaken: return FF_DamageTaken;
        case FactionMetric::HitsTaken: return FF_HitsTaken;
        case FactionMetric::Losses: return FF_Losses;
        case FactionMetric::Built: return FF_Built;
        default: return -1;
        }
    };

    for (int f = 0; f < MAX_FACTIONS; ++f) {
        FactionStatRow& row = stats_.factions[static_cast<size_t>(f)];
        const Agg& a = agg[static_cast<size_t>(f)];
        const auto& ft = factionTotals_[static_cast<size_t>(f)];
        row.name = factionNames_[static_cast<size_t>(f)];
        row.id = factionIds_[static_cast<size_t>(f)];
        row.colorSlot = f;
        row.alive = factionAlive_[static_cast<size_t>(f)];
        row.eliminated = false;
        row.total.fill(0.0);
        for (auto& r : row.recent) r.fill(0.0);

        row.total[static_cast<size_t>(FactionMetric::ShipCount)] = a.ships;
        row.total[static_cast<size_t>(FactionMetric::Kills)] = ft[FF_Kills];
        row.total[static_cast<size_t>(FactionMetric::Output)] = ft[FF_Output];
        row.total[static_cast<size_t>(FactionMetric::Mined)] = ft[FF_Mined];
        row.total[static_cast<size_t>(FactionMetric::Resources)] = resources_[static_cast<size_t>(f)];
        row.total[static_cast<size_t>(FactionMetric::Collected)] = ft[FF_Collected];
        row.total[static_cast<size_t>(FactionMetric::AvgLevel)] =
            a.ships > 0 ? static_cast<double>(a.levelSum) / a.ships : 0.0;
        row.total[static_cast<size_t>(FactionMetric::Power)] = a.levelSum;
        row.total[static_cast<size_t>(FactionMetric::FriendlyDamage)] = ft[FF_FriendlyDamage];
        row.total[static_cast<size_t>(FactionMetric::FriendlyHits)] = ft[FF_FriendlyHits];
        row.total[static_cast<size_t>(FactionMetric::Losses)] = ft[FF_Losses];
        row.total[static_cast<size_t>(FactionMetric::Built)] = ft[FF_Built];
        row.total[static_cast<size_t>(FactionMetric::DamageTaken)] = ft[FF_DamageTaken];
        row.total[static_cast<size_t>(FactionMetric::HitsTaken)] = ft[FF_HitsTaken];
        row.total[static_cast<size_t>(FactionMetric::TotalHealth)] = a.health;
        row.total[static_cast<size_t>(FactionMetric::AvgHealth)] = a.ships > 0 ? a.health / a.ships : 0.0;
        row.total[static_cast<size_t>(FactionMetric::TotalArmor)] = a.armor;
        row.total[static_cast<size_t>(FactionMetric::AvgArmor)] = a.ships > 0 ? a.armor / a.ships : 0.0;
        row.total[static_cast<size_t>(FactionMetric::AvgKills)] = a.ships > 0 ? a.kills / a.ships : 0.0;
        row.total[static_cast<size_t>(FactionMetric::MaxLevel)] = a.maxLevel;
        row.total[static_cast<size_t>(FactionMetric::DamageRatio)] =
            ft[FF_DamageTaken] > 0.0 ? ft[FF_Output] / ft[FF_DamageTaken] : 0.0;
        row.total[static_cast<size_t>(FactionMetric::KillsPerLoss)] =
            ft[FF_Losses] > 0.0 ? ft[FF_Kills] / ft[FF_Losses] : ft[FF_Kills];
        row.total[static_cast<size_t>(FactionMetric::OutputPerShip)] =
            a.ships > 0 ? ft[FF_Output] / a.ships : 0.0;
        row.total[static_cast<size_t>(FactionMetric::MinedPerShip)] =
            a.ships > 0 ? ft[FF_Mined] / a.ships : 0.0;
        row.total[static_cast<size_t>(FactionMetric::CollectedPerShip)] =
            a.ships > 0 ? ft[FF_Collected] / a.ships : 0.0;

        const FactionStatHistory& h = factionStats_[static_cast<size_t>(f)];
        const size_t metricCount = static_cast<size_t>(FactionMetric::Count);
        for (size_t m = 0; m < metricCount; ++m) {
            const FactionMetric fm = static_cast<FactionMetric>(m);
            const int field = factionFieldOf(fm);
            if (field < 0) continue;
            for (int w = 0; w < 3; ++w) {
                const double partial = ft[static_cast<size_t>(field)] - h.last[static_cast<size_t>(field)];
                const double v = static_cast<double>(h.recent[static_cast<size_t>(w)][static_cast<size_t>(field)]) + partial;
                row.recent[static_cast<size_t>(w)][m] = v > 0.0 ? v : 0.0;
            }
        }

        for (size_t m = 0; m < metricCount; ++m) {
            const MetricInfo& info = factionMetricInfo(static_cast<FactionMetric>(m));
            const double candidate = info.cumulative ? row.recent[0][m] : row.total[m];
            if (candidate > row.peak[m]) row.peak[m] = candidate;
            if (row.peak[m] > stats_.overallPeak[m]) stats_.overallPeak[m] = row.peak[m];
        }

        // 最后一个单位死亡：把这一代阵营（名字+#编号/颜色/峰值）归档。
        // 当前槽位保留旧统计用于面板显示，但 alive=false，不再参与“仅峰值”排行。
        if (factionAlive_[static_cast<size_t>(f)] && a.ships == 0) {
            FactionStatRow archived = row;
            archived.eliminated = true;
            archived.alive = false;
            eliminatedFactions_.push_back(std::move(archived));
            factionAlive_[static_cast<size_t>(f)] = false;
            row.alive = false;
        }
    }

    stats_.eliminatedFactions = eliminatedFactions_;
    stats_.deadIndividualCount = deadShips_.size();
}

const std::string& World::factionName(int faction) const {
    static const std::string empty;
    if (faction < 0 || faction >= MAX_FACTIONS) return empty;
    return factionNames_[faction];
}

std::string World::shipInfo(int index) const {
    const Ship* s = shipAt(index);
    if (!s) return {};
    std::ostringstream ss;
    if (!s->title.empty()) ss << "称号: " << s->title << "\n";
    ss << "编号: #" << s->serial << "\n";
    ss << "阵营: " << factionDisplayName(s->faction) << "\n";
    ss << "击杀: " << s->kills << "\n";
    ss << "等级: " << s->level << "\n";
    ss << "生命: " << static_cast<int>(s->health) << "/" << static_cast<int>(s->maxHealth) << "\n";
    ss << "装甲: " << static_cast<int>(s->armor) << "/" << static_cast<int>(s->maxArmor) << "\n";
    ss << "机动: " << std::fixed << std::setprecision(1) << s->maneuverability << "\n";
    ss << "最大速度: " << static_cast<int>(s->maxSpeed) << "\n";
    ss << "射速: " << std::fixed << std::setprecision(1) << s->fireRate << "\n";
    ss << "伤害: " << static_cast<int>(s->damage) << "\n";
    ss << "射程: " << static_cast<int>(s->fireRange) << "\n";
    ss << "视野: " << static_cast<int>(s->viewRange) << "\n";
    ss << "采矿: " << std::fixed << std::setprecision(2) << s->miningExpertise << "\n";
    ss << "建造: " << std::fixed << std::setprecision(2) << s->buildSpeed << "\n";
    ss << "累计输出: " << static_cast<int>(s->damageDealt) << "\n";
    ss << "累计承伤: " << static_cast<int>(s->damageTaken) << " / " << s->hitsTaken << " 次\n";
    ss << "累计收集: " << s->collected << "   开采: " << s->mined << "\n";
    ss << "友伤: " << static_cast<int>(s->friendlyDamage) << " / " << s->friendlyHits << " 次\n";
    ss << "存活: " << std::fixed << std::setprecision(1) << s->age << " 秒\n";
    ss << "当前速度: " << std::fixed << std::setprecision(1) << s->vel.length() << "\n";
    ss << "状态: " << stateName(s->state);
    if (s->building) ss << " (建造 " << static_cast<int>(s->buildProgress * 100.0f) << "%)";
    return ss.str();
}

