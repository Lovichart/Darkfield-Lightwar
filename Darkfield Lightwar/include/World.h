// Darkfield Lightwar - 世界 / 实体 / 模拟
#pragma once

#include "Common.h"
#include "Stats.h"

#include <deque>

enum class ShipState : uint8_t { Attacking, Collecting, Mining, Idle, Fleeing, AttackingPortal };
enum class AsteroidType : uint8_t { Normal, Comet };
enum class ZoneType : uint8_t { Reinforce, Survival, Attack, Mobility, Production, Domain };

// ---- 创造面板 ----
enum class CreationKind : uint8_t { Ship, Asteroid, Opportunity };
enum class SpawnMode : uint8_t { Drop, Reinforce, Portal };
enum class FormationType : uint8_t { Square, Circle, Dense, Random };

// 属性加点：与 World::applyUpgrade 的 1..5 一一对应。
struct ShipAttributePoints {
    int survival = 0;   // 生命 / 装甲 / 回复
    int attack = 0;     // 伤害 / 射速
    int mobility = 0;   // 速度 / 机动
    int production = 0; // 建造 / 采矿
    int vision = 0;     // 视野 / 射程
    int total() const { return survival + attack + mobility + production + vision; }
};

// 一次“创造”的完整参数。World 只认这个结构，UI 可以随便改字段。
struct CreationSpec {
    CreationKind kind = CreationKind::Ship;
    int faction = 0;
    int count = 12;
    FormationType formation = FormationType::Square;
    float speed = 40.0f;

    ShipAttributePoints attributes;

    AsteroidType asteroidType = AsteroidType::Normal;
    float asteroidRadius = 34.0f;
    float irregularity = 0.55f;

    float zoneDuration = 16.0f;
    float zoneRadius = 100.0f;
    ZoneType zoneType = ZoneType::Reinforce;

    float portalHealth = 1000.0f;
    float portalInterval = 60.0f;
};

struct Obstacle {
    Vec2 pos;
    Vec2 vel;
    float radius = 0.0f;
    float distance = 0.0f;
    float threat = 0.0f;
};

struct Ship {
    uint32_t gen = 0;
    bool active = false;
    bool deathHandled = false;
    bool selected = false;

    Vec2 pos;
    Vec2 vel;
    Vec2 acc;
    float rot = 0.0f;
    float mass = 3.0f;

    float health = 100.0f;
    float maxHealth = 100.0f;
    int faction = 0;
    sf::Color color{255, 255, 255};

    // 武器
    float fireRate = 2.0f;
    float fireTimer = 0.0f;
    float fireRange = 120.0f;
    float damage = 10.0f;
    float fireAngle = PI / 6.0f;

    // 机动
    float maneuverability = 0.8f;
    float maxSpeed = 120.0f;
    float accelerationRate = 50.0f;
    float brakeRate = 20.0f;

    // 生存 / 采矿
    float recoveryRate = 1.0f;
    float miningExpertise = 0.1f;
    float armor = 50.0f;
    float maxArmor = 100.0f;
    float armorRegen = 0.1f;
    float hitCooldown = 0.0f;

    // AI
    ShipState state = ShipState::Idle;
    uint32_t targetShip = UINT32_MAX;
    uint32_t targetShipGen = 0;
    uint32_t targetAst = UINT32_MAX;
    uint32_t targetAstGen = 0;
    uint32_t targetLight = UINT32_MAX;
    float viewRange = 180.0f;
    float decisionTimer = 0.0f;
    float obstacleTimer = 0.0f;
    float idleTimer = 0.0f;
    float stateTimer = 0.0f;

    // 受击逃逸：被非当前目标偷袭时，先加速拉开距离，再慢慢掉头
    float alertTimer = 0.0f;
    uint32_t threatShip = UINT32_MAX;
    uint32_t threatShipGen = 0;
    Vec2 threatPos;

    // 增援：从界面外飞进来。进入窗口前跳过边界夹取和 AI，直着飞。
    bool entering = false;

    // 传送门目标（“传送门被视为该阵营的飞船”时，敌舰会攻击它）
    uint32_t targetPortal = UINT32_MAX;
    uint32_t targetPortalGen = 0;

    // 连续低速计时：F3 的“停滞”只统计连续 >3s 速度 <1px/s 的船，
    // 避免把采集到位/碰撞瞬间的短暂低速误报成“原地不动”。
    float stillTimer = 0.0f;

    Obstacle obstacles[10];
    int obstacleCount = 0;

    // 速度控制
    float preferredSpeed = 20.0f;
    float drivingSkill = 0.33f;
    float standardSpeed = 80.0f;
    float targetSpeed = 80.0f;

    // 建造
    bool building = false;
    float buildProgress = 0.0f;
    float buildSpeed = 0.1f;

    // 成长
    int level = 1;
    float upgradeCooldown = 0.0f;
    int kills = 0;
    std::string title;

    // 统计：个体编号 + 累计数据（统计面板/排行榜用）
    uint32_t serial = 0;       // 个体编号（槽位复用后仍唯一）
    float age = 0.0f;          // 存活时间（秒）
    float damageDealt = 0.0f;  // 对敌对阵营造成的实际伤害
    float damageTaken = 0.0f;  // 实际承受伤害
    float friendlyDamage = 0.0f; // 对友军造成的实际伤害
    uint32_t friendlyHits = 0; // 造成友伤的次数
    uint32_t hitsTaken = 0;    // 受伤次数
    uint32_t collected = 0;    // 收集到的光点（资源）
    uint32_t mined = 0;        // 从小行星上采下的像素（矿料）

    float trailTimer = 0.0f;
};

struct Pixel {
    int16_t gx = 0;
    int16_t gy = 0;
    sf::Uint8 r = 150, g = 150, b = 150;
    bool active = true;
};

struct Asteroid {
    uint32_t gen = 0;
    bool active = false;
    bool deathHandled = false;

    Vec2 pos;
    Vec2 vel;
    Vec2 acc;
    float rot = 0.0f;
    float angVel = 0.0f;

    float mass = 50.0f;
    float health = 200.0f;
    float maxHealth = 200.0f;
    float radius = 30.0f;
    float baseMass = 50.0f;
    float baseHealth = 200.0f;
    float cushion = 0.3f;
    float hitCooldown = 0.0f;

    AsteroidType type = AsteroidType::Normal;
    std::vector<Pixel> pixels;
    int totalPixels = 0;
    int activePixels = 0;
    int gridN = 0;
    float pixelSize = 4.0f;

    sf::Image image;
    mutable bool imageDirty = true;

    std::array<Vec2, 24> trail{};
    int trailCount = 0;
    float trailTimer = 0.0f;
};

struct LightPoint {
    bool active = false;
    bool freed = false;
    Vec2 pos;
    Vec2 vel;
    sf::Color color{255, 255, 255};
    float radius = 2.0f;

    // 光点很脆：初始激光 10 伤害一下打坏；对石头碰撞有较高抗性。
    float health = 10.0f;
    float maxHealth = 10.0f;
    float hitCooldown = 0.0f;
};

struct Beam {
    Vec2 a;
    Vec2 b;
    sf::Color color{255, 255, 255};
    float life = 0.1f;
    float maxLife = 0.1f;
};

struct Zone {
    bool active = true;
    ZoneType type = ZoneType::Reinforce;
    Vec2 pos;
    float life = ZONE_DURATION;
    float maxLife = ZONE_DURATION;
    float radius = 5.0f;
    float maxRadius = 100.0f;
    float rotation = 0.0f;
    float reinforceTimer = 0.0f;
    sf::Color color{255, 255, 255};
};

// 创造面板的传送门：固定、无碰撞箱、可被打掉的血量圆圈，
// 每隔一段时间在原地刷一波创建参数指定的对象。
struct Portal {
    uint32_t gen = 0;
    bool active = false;
    Vec2 pos;
    int faction = -1;                 // >=0：视为该阵营飞船；-1：视为小行星/机遇
    CreationKind kind = CreationKind::Ship;
    float radius = 46.0f;
    float rotation = 0.0f;
    float health = 1000.0f;
    float maxHealth = 1000.0f;
    float spawnInterval = 60.0f;
    float spawnTimer = 0.0f;
    int waveIndex = 0;
    float flashTimer = 0.0f;
    sf::Color color{140, 170, 200};
    CreationSpec spec;
};

struct TrailParticle {
    Vec2 pos;
    Vec2 vel;
    float life = 0.0f;
    float maxLife = 1.0f;
    float size = 2.0f;
    sf::Color color{255, 255, 255};
};

// 均匀网格：只存数组下标，查询时用 stamp 去重
class Grid {
public:
    void init(float width, float height, float cellSize) {
        cellSize_ = cellSize;
        cols_ = std::max(1, static_cast<int>(std::ceil(width / cellSize)));
        rows_ = std::max(1, static_cast<int>(std::ceil(height / cellSize)));
        cells_.clear();
        cells_.resize(static_cast<size_t>(cols_) * rows_);
        stamp_.clear();
        queryId_ = 1;
    }

    void clear() {
        for (auto& cell : cells_) cell.clear();
    }

    void insert(uint32_t index, Vec2 p, float radius) {
        if (cells_.empty()) return;
        if (index >= stamp_.size()) stamp_.resize(static_cast<size_t>(index) + 1, 0u);

        int minX = static_cast<int>(std::floor((p.x - radius) / cellSize_));
        int maxX = static_cast<int>(std::floor((p.x + radius) / cellSize_));
        int minY = static_cast<int>(std::floor((p.y - radius) / cellSize_));
        int maxY = static_cast<int>(std::floor((p.y + radius) / cellSize_));
        minX = std::max(0, std::min(cols_ - 1, minX));
        maxX = std::max(0, std::min(cols_ - 1, maxX));
        minY = std::max(0, std::min(rows_ - 1, minY));
        maxY = std::max(0, std::min(rows_ - 1, maxY));

        for (int y = minY; y <= maxY; ++y) {
            for (int x = minX; x <= maxX; ++x) {
                cells_[static_cast<size_t>(y) * cols_ + x].push_back(index);
            }
        }
    }

    template <typename F>
    void query(Vec2 p, float radius, F&& fn) {
        if (cells_.empty()) return;
        if (++queryId_ == 0u) {
            std::fill(stamp_.begin(), stamp_.end(), 0u);
            queryId_ = 1u;
        }

        int minX = static_cast<int>(std::floor((p.x - radius) / cellSize_));
        int maxX = static_cast<int>(std::floor((p.x + radius) / cellSize_));
        int minY = static_cast<int>(std::floor((p.y - radius) / cellSize_));
        int maxY = static_cast<int>(std::floor((p.y + radius) / cellSize_));
        minX = std::max(0, std::min(cols_ - 1, minX));
        maxX = std::max(0, std::min(cols_ - 1, maxX));
        minY = std::max(0, std::min(rows_ - 1, minY));
        maxY = std::max(0, std::min(rows_ - 1, maxY));

        for (int y = minY; y <= maxY; ++y) {
            for (int x = minX; x <= maxX; ++x) {
                for (uint32_t idx : cells_[static_cast<size_t>(y) * cols_ + x]) {
                    if (idx >= stamp_.size()) continue;
                    if (stamp_[idx] == queryId_) continue;
                    stamp_[idx] = queryId_;
                    fn(idx);
                }
            }
        }
    }

    template <typename F>
    void queryRay(Vec2 origin, Vec2 dir, float maxDist, F&& fn) {
        if (cells_.empty() || maxDist <= 0.0f) return;
        const float len = dir.length();
        if (len < 1e-6f) return;
        dir = dir / len;

        if (++queryId_ == 0u) {
            std::fill(stamp_.begin(), stamp_.end(), 0u);
            queryId_ = 1u;
        }

        int cellX = static_cast<int>(std::floor(origin.x / cellSize_));
        int cellY = static_cast<int>(std::floor(origin.y / cellSize_));
        const int stepX = (dir.x > 1e-6f) ? 1 : ((dir.x < -1e-6f) ? -1 : 0);
        const int stepY = (dir.y > 1e-6f) ? 1 : ((dir.y < -1e-6f) ? -1 : 0);

        const float inf = std::numeric_limits<float>::infinity();
        float tMaxX = inf, tMaxY = inf;
        float tDeltaX = inf, tDeltaY = inf;
        if (stepX != 0) {
            const float nextX = static_cast<float>(cellX + (stepX > 0 ? 1 : 0)) * cellSize_;
            tMaxX = (nextX - origin.x) / dir.x;
            tDeltaX = cellSize_ / std::abs(dir.x);
        }
        if (stepY != 0) {
            const float nextY = static_cast<float>(cellY + (stepY > 0 ? 1 : 0)) * cellSize_;
            tMaxY = (nextY - origin.y) / dir.y;
            tDeltaY = cellSize_ / std::abs(dir.y);
        }

        float t = 0.0f;
        while (t <= maxDist) {
            if (cellX < 0 || cellX >= cols_ || cellY < 0 || cellY >= rows_) break;
            for (uint32_t idx : cells_[static_cast<size_t>(cellY) * cols_ + cellX]) {
                if (idx >= stamp_.size()) continue;
                if (stamp_[idx] == queryId_) continue;
                stamp_[idx] = queryId_;
                fn(idx);
            }
            if (tMaxX < tMaxY) {
                t = tMaxX;
                cellX += stepX;
                tMaxX += tDeltaX;
            } else {
                t = tMaxY;
                cellY += stepY;
                tMaxY += tDeltaY;
            }
        }
    }

private:
    int cols_ = 0;
    int rows_ = 0;
    float cellSize_ = 100.0f;
    std::vector<std::vector<uint32_t>> cells_;
    std::vector<uint32_t> stamp_;
    uint32_t queryId_ = 1;
};

class World {
public:
    explicit World(uint32_t seed = 12345u);

    void reset(uint32_t seed, int shipsPerFaction);
    void restart(); // 用当前 seed / 初始船数在当前战场大小上重开
    void step(float dt);

    // 战场大小：0.25x=800x450, 1x=1600x900, 4x=3200x1800, 9x=4800x2700。
    // 改变大小会按比例重排现有对象（位置/速度不变、相对布局保留）并重建网格。
    float worldWidth() const { return worldWidth_; }
    float worldHeight() const { return worldHeight_; }
    void setWorldSize(float width, float height);

    // 生成（游戏/基准测试共用）
    uint32_t spawnShip(Vec2 pos, int faction, const Ship* copyFrom = nullptr);
    uint32_t spawnAsteroid(Vec2 pos, Vec2 vel, float radius, AsteroidType type = AsteroidType::Normal,
                           float irregularity = 0.55f);
    uint32_t spawnLight(Vec2 pos, Vec2 vel, sf::Color color);
    uint32_t spawnRandomAsteroid();
    void spawnAsteroidWave();
    void populateAsteroids(int count);
    void scrambleFleets();

    // ---- 创造面板 ----
    // 预览半径：飞船是阵型外接圆，小行星是整组散布范围，机遇是最大范围。
    static float creationRadius(const CreationSpec& spec);
    // 增援的“界外中心”：从 center 沿 dir 推到战场外，再留出整组半径。
    static Vec2 reinforcementOrigin(Vec2 center, Vec2 dir, float radius,
                                    float worldWidth, float worldHeight);
    // 按中心 + 方向创建一批对象。返回实际创建数量（传送门返回 1）。
    uint32_t createBatch(const CreationSpec& spec, Vec2 center, Vec2 dir, SpawnMode mode);
    // 随机位置/随机方向创建。
    uint32_t createRandom(const CreationSpec& spec, SpawnMode mode);
    uint32_t spawnPortal(Vec2 pos, const CreationSpec& spec);
    uint32_t spawnConfiguredZone(Vec2 pos, ZoneType type, float duration, float radius);
    uint32_t spawnShipEx(Vec2 pos, Vec2 vel, float rot, int faction,
                         const ShipAttributePoints& attrs, bool entering);
    void damagePortal(int index, float dmg, int attacker);

    // 查询
    const std::vector<Ship>& ships() const { return ships_; }
    const std::vector<Asteroid>& asteroids() const { return asteroids_; }
    const std::vector<LightPoint>& lights() const { return lights_; }
    const std::vector<Beam>& beams() const { return beams_; }
    const std::vector<Zone>& zones() const { return zones_; }
    const std::vector<Portal>& portals() const { return portals_; }
    const std::vector<TrailParticle>& trails() const { return trails_; }

    int shipCount(int faction) const;
    float resource(int faction) const;

    // 阵营身份：同一颜色槽位被清空后重生，会得到新名字和新编号。
    std::string factionDisplayName(int faction) const;
    uint64_t factionId(int faction) const;
    int factionColorSlot(int faction) const { return faction; }

    // 统计面板 / 排行榜：统计快照按 sim 时间约 10 Hz 刷新；
    // 多个子窗口共享同一份快照，避免每帧重复扫船。
    const StatsSnapshot& statsSnapshot() const { return stats_; }
    double individualMetricValue(int shipIndex, IndividualMetric metric, StatRange range) const;
    const std::vector<IndividualRank>& topIndividuals(IndividualMetric metric, StatRange range,
                                                      const IndividualFilter& filter, int count,
                                                      bool includeEliminated = false) const;

    int pickShip(Vec2 p) const;
    const Ship* shipAt(int index) const;
    Ship* shipAt(int index);
    const Asteroid* asteroidAt(int index) const;
    uint32_t shipGen(int index) const;

    void selectShip(int index);
    void clearSelection();
    const Ship* selected() const;
    int selectedIndex() const { return selectedShip_; }

    std::string shipInfo(int index) const;
    const std::string& factionName(int faction) const;

    // 运行参数 / 统计
    bool rebellionEnabled = true;
    bool friendlyCollisionEnabled = true; // 友军是否互相碰撞（可扣血，见 friendlyFireEnabled）
    bool friendlyFireEnabled = true;      // 友军激光/碰撞是否造成伤害
    int selectedShip_ = -1;
    uint32_t selectedGen_ = 0;

    int activeShipCount = 0;
    int activeAsteroidCount = 0;
    int activeLightCount = 0;
    int activeBeamCount = 0;
    int activeTrailCount = 0;
    int activeZoneCount = 0;
    int activePortalCount = 0;
    int activeStoppedCount = 0; // 速度 < 1 px/s 的飞船数（F3 用来查“原地不动”）
    uint64_t stepCount = 0;
    uint32_t seed = 12345u;
    int shipCap = 20000;
    float worldWidth_ = static_cast<float>(WINDOW_WIDTH);
    float worldHeight_ = static_cast<float>(WINDOW_HEIGHT);

    // 基准测试用分阶段计时（默认关闭，不影响正式运行）
    bool profile = false;
    std::array<double, 12> phaseMs{};
    double collShipShipMs = 0.0;
    double collShipAstMs = 0.0;
    double collAstAstMs = 0.0;
    uint64_t collShipShipPairs = 0;
    uint64_t collShipAstPairs = 0;
    uint64_t collAstAstPairs = 0;

private:
    // 阶段
    void rebuildGrids();
    void updateShips(float dt);
    void updateAsteroids(float dt);
    void updateLights(float dt);
    void updateBeams(float dt);
    void updateZones(float dt);
    void updatePortals(float dt);
    void updateTrails(float dt);
    void updateCollisions();
    void updateBuilding(float dt);
    void updateRebellion(float dt);
    void cleanup();

    // 战斗 / 伤害
    void fireAtShip(int self, int target);
    void fireAtAsteroid(int self, int target);
    void fireAtPortal(int self, int target);
    void resolveBeam(int self, Vec2 start, Vec2 dir, float maxDist);
    void damageShip(int index, float dmg, int attackerIndex, bool collision);
    void damageAsteroid(int index, float dmg, Vec2 hitWorld, int attackerIndex, float mining);
    void damageLight(int index, float dmg);
    void killShipNow(int index);
    void killAsteroidNow(int index);
    void spawnShipFragments(const Ship& s);
    void erodeAsteroid(Asteroid& a, Vec2 hitLocal, int count, int attackerFaction, float mining);
    void buildAsteroidImage(Asteroid& a);
    void refreshAsteroidImage(Asteroid& a);

    // AI
    void evaluateState(Ship& s);
    void scanObstacles(Ship& s);
    Vec2 obstacleAvoidance(const Ship& s) const;
    Vec2 boundaryAvoidance(const Ship& s) const;
    void updateSpeedControl(Ship& s, float dt);
    bool needsEmergencyBrake(const Ship& s) const;
    void collectLights(Ship& s);
    void applyUpgrade(Ship& s, int type);
    void copyAttributes(Ship& dst, const Ship& src);
    void requestShip(Vec2 pos, int faction, int copyFrom);
    void flushShipRequests();

    // 区域
    void spawnZone();
    void applyZone(Zone& z, float dt);
    void reinforceZone(Zone& z);

    // 传送门 / 创造
    void spawnPortalWave(Portal& p);
    int portalSlotByHandle(uint32_t index, uint32_t gen) const;
    static float shipFormationRadius(FormationType formation, int count);
    void buildShipFormation(FormationType formation, int count, std::vector<Vec2>& out);
    static void buildAsteroidScatter(int count, float bodyRadius, std::vector<Vec2>& out);
    void placeShipBatch(const CreationSpec& spec, Vec2 origin, Vec2 dir, bool entering, uint32_t& created);
    uint32_t placeAsteroidBatch(const CreationSpec& spec, Vec2 origin, Vec2 dir, bool entering);

    // 工具
    void addBeam(Vec2 a, Vec2 b, sf::Color color);
    void addTrailParticle(Vec2 pos, Vec2 vel, sf::Color color, float size);
    void addLightBurst(Vec2 pos, sf::Color color, int count, float speed);
    bool isInsideWorld(Vec2 p, float margin = 0.0f) const;
    int shipSlotByHandle(uint32_t index, uint32_t gen) const;
    int asteroidSlotByHandle(uint32_t index, uint32_t gen) const;
    void loadNames();
    std::string randomTitle();

    // ---- 统计 ----
    // 阵营累计字段（与 FactionMetric 中的累计项一一对应）
    enum FactionField {
        FF_Kills = 0, FF_Output, FF_Collected, FF_Mined,
        FF_FriendlyHits, FF_FriendlyDamage, FF_DamageTaken, FF_HitsTaken,
        FF_Losses, FF_Built, FF_Count
    };
    // 个体累计字段（与 IndividualMetric 中的累计项一一对应）
    enum IndividualField {
        IF_Kills = 0, IF_Output, IF_Collected, IF_Mined,
        IF_FriendlyHits, IF_FriendlyDamage, IF_DamageTaken, IF_HitsTaken, IF_Count
    };

    // 每 10 秒一个桶，60 个桶 = 最近 10 分钟。
    // 用“已完成桶求和 + 当前桶实时增量”计算最近 1/5/10 分钟，
    // 内存固定（每船约 2 KB），不会随时间增长。
    struct ShipStatHistory {
        uint32_t serial = 0;
        std::array<float, IF_Count> last{};
        std::array<std::array<float, IF_Count>, 60> buckets{};
        std::array<std::array<float, IF_Count>, 3> recent{};
        uint8_t head = 0;
        uint8_t filled = 0;
    };

    struct FactionStatHistory {
        std::array<double, FF_Count> last{};
        std::array<std::array<float, FF_Count>, 60> buckets{};
        std::array<std::array<float, FF_Count>, 3> recent{};
        uint8_t head = 0;
        uint8_t filled = 0;
    };

    struct RankCache {
        uint64_t key = 0;
        uint64_t version = 0;
        int count = 0;
        std::vector<IndividualRank> rows;
    };

    void refreshStats();
    void sampleStatsBuckets();
    void ensureShipStatSlot(uint32_t index, uint32_t serial);
    double individualTotalValue(const Ship& s, IndividualMetric metric) const;
    static int individualFieldOf(IndividualMetric metric);

    // 阵营重生 / 淘汰者归档
    void assignFactionIdentity(int faction, bool resetStats);
    std::string chooseFactionName(int faction, const std::string& avoid);
    void archiveDeadShip(int index);
    double deadMetricValue(const DeadShipRecord& record, IndividualMetric metric,
                           StatRange range) const;

    struct ShipRequest {
        Vec2 pos;
        int faction = 0;
        int copyFrom = -1;
    };

    std::vector<Ship> ships_;
    std::vector<uint32_t> freeShips_;
    std::vector<Asteroid> asteroids_;
    std::vector<uint32_t> freeAsteroids_;
    std::vector<LightPoint> lights_;
    std::vector<uint32_t> freeLights_;
    std::vector<Beam> beams_;
    std::vector<Zone> zones_;
    std::vector<Portal> portals_;
    std::vector<TrailParticle> trails_;
    std::vector<uint32_t> erodeScratch_;
    std::vector<ShipRequest> shipRequests_;

    Grid shipGrid_;
    std::array<Grid, MAX_FACTIONS> collisionGrids_;
    Grid astGrid_;
    Grid lightGrid_;

    std::array<float, MAX_FACTIONS> resources_{};
    std::array<std::string, MAX_FACTIONS> factionNames_;
    std::array<std::vector<std::string>, MAX_FACTIONS> factionNamePools_;
    std::array<uint64_t, MAX_FACTIONS> factionIds_{};
    std::array<bool, MAX_FACTIONS> factionAlive_{};
    uint64_t nextFactionId_ = 1;
    std::vector<FactionStatRow> eliminatedFactions_;
    std::vector<DeadShipRecord> deadShips_;
    std::vector<std::string> titles_;

    // 统计：事件时 O(1) 累加，快照约 10 Hz 扫描一次当前聚合。
    std::array<std::array<double, FF_Count>, MAX_FACTIONS> factionTotals_{};
    std::array<FactionStatHistory, MAX_FACTIONS> factionStats_{};
    std::deque<ShipStatHistory> shipStats_;
    StatsSnapshot stats_;
    mutable std::vector<RankCache> rankCaches_;
    uint32_t nextShipSerial_ = 1;
    float statsRefreshTimer_ = 0.0f;
    float statsBucketTimer_ = 0.0f;

    Rng rng_;
    float asteroidTimer_ = 0.0f;
    float waveTimer_ = 0.0f;
    int waveCount_ = 0;
    float zoneTimer_ = 0.0f;
    float nextZoneTime_ = 6.0f;
    float rebellionTimer_ = 0.0f;
    int initialShipsPerFaction_ = 5;
};
