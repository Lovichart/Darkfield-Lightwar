// Darkfield Lightwar - 统计指标定义（统计面板 / 排行榜共用）
#pragma once

#include "Common.h"

// 时间范围：累计 / 最近 1、5、10 分钟 / 仅峰值
enum class StatRange : uint8_t {
    Total = 0,
    Recent1m,
    Recent5m,
    Recent10m,
    Peak,
    Count
};

// 阵营指标。cumulative 的项才有“最近 N 分钟”变体；
// “仅峰值”：累计项取历史最佳 1 分钟，当前项取历史最高瞬时值。
// “显示淘汰者”开关：把已灭亡阵营/个体一起加入排行（不是单独只看淘汰者）。
enum class FactionMetric : uint8_t {
    ShipCount = 0,
    Kills,
    Output,
    Mined,
    Resources,
    Collected,
    AvgLevel,
    Power,
    FriendlyDamage,
    FriendlyHits,
    Losses,
    Built,
    DamageTaken,
    HitsTaken,
    TotalHealth,
    AvgHealth,
    TotalArmor,
    AvgArmor,
    AvgKills,
    MaxLevel,
    DamageRatio,
    KillsPerLoss,
    OutputPerShip,
    MinedPerShip,
    CollectedPerShip,
    Count
};

// 个体指标。cumulative 的项支持最近 1/5/10 分钟。
enum class IndividualMetric : uint8_t {
    Level = 0,
    Kills,
    Output,
    Collected,
    Mined,
    FriendlyHits,
    FriendlyDamage,
    DamageTaken,
    HitsTaken,
    Health,
    Armor,
    Age,
    KillsPerLevel,
    OutputPerMinute,
    Count
};

struct MetricInfo {
    const char* name = "";
    bool cumulative = false; // 是否有“累计”语义（支持最近 N 分钟）
    bool integer = true;     // 显示为整数
    int decimals = 0;
};

const MetricInfo& factionMetricInfo(FactionMetric metric);
const MetricInfo& individualMetricInfo(IndividualMetric metric);
const char* statRangeName(StatRange range);
bool statRangeSupported(StatRange range, bool individual, bool cumulative);
const char* statRangeHint(StatRange range);

// 个体排行榜筛选条件
struct IndividualFilter {
    int faction = -1;      // -1 = 全部
    int minLevel = 1;      // 最低等级
    bool titlesOnly = false; // 只看有称号的
};

// 个体排行榜的一行
struct IndividualRank {
    uint64_t key = 0;       // 个体唯一键（serial）
    uint32_t serial = 0;    // 编号
    int faction = 0;        // 颜色槽位（0..3）
    uint64_t factionId = 0; // 阵营编号（同一颜色多次重生会不同）
    int level = 1;
    std::string title;
    double value = 0.0;
};

// 淘汰的个体：死亡瞬间的快照。用于“淘汰者”排行。
struct DeadShipRecord {
    uint32_t serial = 0;
    int faction = 0;
    uint64_t factionId = 0;
    int level = 1;
    float age = 0.0f;
    float maxHealth = 100.0f;
    float maxArmor = 100.0f;
    std::string title;
    double kills = 0.0;
    double output = 0.0;
    double collected = 0.0;
    double mined = 0.0;
    double friendlyHits = 0.0;
    double friendlyDamage = 0.0;
    double damageTaken = 0.0;
    double hitsTaken = 0.0;
    // 死亡瞬间的最近 1/5/10 分钟窗口（按 IndividualField 索引），
    // 让“显示淘汰者”在近 N 分钟范围里也能看到他们最后的贡献。
    std::array<std::array<float, 8>, 3> recent{};
};

struct FactionStatRow {
    std::array<double, static_cast<size_t>(FactionMetric::Count)> total{};
    std::array<double, static_cast<size_t>(FactionMetric::Count)> recent[3]{}; // 1/5/10 分钟
    std::array<double, static_cast<size_t>(FactionMetric::Count)> peak{};

    // 阵营身份：颜色槽位相同也可能对应多个历史阵营（被清空后重生）。
    std::string name;
    uint64_t id = 0;
    int colorSlot = 0;
    bool alive = true;
    bool eliminated = false;
};

struct StatsSnapshot {
    std::array<FactionStatRow, MAX_FACTIONS> factions{};
    std::vector<FactionStatRow> eliminatedFactions;
    std::array<double, static_cast<size_t>(FactionMetric::Count)> overallPeak{};
    size_t deadIndividualCount = 0;
    uint64_t version = 0;
    uint64_t step = 0;
};

// 取阵营某项指标在某个时间范围的值。Peak 返回历史峰值。
double factionMetricValue(const FactionStatRow& row, FactionMetric metric, StatRange range);
std::string factionRowDisplayName(const FactionStatRow& row);

// 数字格式化：大数用“万 / 亿”，并按指标决定小数位。
std::string formatMetricValue(double value, const MetricInfo& info);
std::string formatCompactNumber(double value, int decimals);
