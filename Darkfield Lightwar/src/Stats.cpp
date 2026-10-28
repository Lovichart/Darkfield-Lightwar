#include "Stats.h"

#include <cstdio>
#include <iterator>
#include <sstream>

namespace {
const MetricInfo kFactionMetrics[] = {
    { "阵营飞船数",           false, true,  0 },
    { "阵营累计击杀数",       true,  true,  0 },
    { "阵营累计输出",         true,  true,  0 },
    { "阵营累计开采数",       true,  true,  0 },
    { "阵营资源",             false, true,  0 },
    { "阵营累计收集资源",     true,  true,  0 },
    { "阵营平均等级",         false, false, 2 },
    { "阵营战力（等级和）",   false, true,  0 },
    { "阵营累计友伤",         true,  true,  0 },
    { "阵营累计友伤次数",     true,  true,  0 },
    { "阵营累计损失",         true,  true,  0 },
    { "阵营累计建造",         true,  true,  0 },
    { "阵营累计承伤",         true,  true,  0 },
    { "阵营累计受伤次数",     true,  true,  0 },
    { "阵营总生命",           false, true,  0 },
    { "阵营平均生命",         false, false, 1 },
    { "阵营总装甲",           false, true,  0 },
    { "阵营平均装甲",         false, false, 1 },
    { "阵营平均击杀",         false, false, 2 },
    { "阵营最高等级",         false, true,  0 },
    { "阵营输出/承伤比",      false, false, 2 },
    { "阵营击杀/损失比",      false, false, 2 },
    { "阵营人均输出",         false, false, 1 },
    { "阵营人均开采",         false, false, 1 },
    { "阵营人均收集",         false, false, 1 },
};
static_assert(std::size(kFactionMetrics) == static_cast<size_t>(FactionMetric::Count),
              "faction metric metadata out of sync");

const MetricInfo kIndividualMetrics[] = {
    { "个体等级",                 false, true,  0 },
    { "个体累计击杀数",           true,  true,  0 },
    { "个体累计输出",             true,  true,  0 },
    { "个体累计收集",             true,  true,  0 },
    { "个体累计开采数",           true,  true,  0 },
    { "个体累计造成友伤次数",     true,  true,  0 },
    { "个体累计友伤",             true,  true,  0 },
    { "个体累计承伤",             true,  true,  0 },
    { "个体累计受伤次数",         true,  true,  0 },
    { "个体生命",                 false, true,  0 },
    { "个体装甲",                 false, true,  0 },
    { "个体存活时间（秒）",       false, false, 1 },
    { "个体击杀/等级",            false, false, 2 },
    { "个体每分钟输出",           false, false, 1 },
};
static_assert(std::size(kIndividualMetrics) == static_cast<size_t>(IndividualMetric::Count),
              "individual metric metadata out of sync");

constexpr size_t fmi(FactionMetric m) { return static_cast<size_t>(m); }
} // namespace

const MetricInfo& factionMetricInfo(FactionMetric metric) {
    const size_t i = fmi(metric);
    return kFactionMetrics[i < std::size(kFactionMetrics) ? i : 0];
}

const MetricInfo& individualMetricInfo(IndividualMetric metric) {
    const size_t i = static_cast<size_t>(metric);
    return kIndividualMetrics[i < std::size(kIndividualMetrics) ? i : 0];
}

const char* statRangeName(StatRange range) {
    switch (range) {
    case StatRange::Recent1m: return "近1分钟";
    case StatRange::Recent5m: return "近5分钟";
    case StatRange::Recent10m: return "近10分钟";
    case StatRange::Peak: return "仅峰值";
    case StatRange::Total:
    default: return "累计";
    }
}

const char* statRangeHint(StatRange range) {
    switch (range) {
    case StatRange::Recent1m: return "最近 60 秒的增量";
    case StatRange::Recent5m: return "最近 5 分钟的增量";
    case StatRange::Recent10m: return "最近 10 分钟的增量";
    case StatRange::Peak: return "仅峰值：累计项取历史最佳 1 分钟，当前项取最高瞬时值";
    case StatRange::Total:
    default: return "本局开始至今的累计值（当前项为实时值）";
    }
}

bool statRangeSupported(StatRange range, bool individual, bool cumulative) {
    (void)individual;
    switch (range) {
    case StatRange::Total:
    case StatRange::Peak: return true;
    case StatRange::Recent1m:
    case StatRange::Recent5m:
    case StatRange::Recent10m: return cumulative;
    default: return false;
    }
}

double factionMetricValue(const FactionStatRow& row, FactionMetric metric, StatRange range) {
    const size_t i = fmi(metric);
    double v = 0.0;
    if (range == StatRange::Peak) {
        v = row.peak[i];
    } else if (range == StatRange::Total) {
        v = row.total[i];
    } else {
        const int w = static_cast<int>(range) - 1;
        v = (w >= 0 && w < 3) ? row.recent[static_cast<size_t>(w)][i] : 0.0;
    }
    return v > 0.0 ? v : 0.0;
}

std::string factionRowDisplayName(const FactionStatRow& row) {
    if (row.id == 0) return row.name;
    return row.name + " #" + std::to_string(row.id);
}

std::string formatCompactNumber(double value, int decimals) {
    const bool neg = value < -0.0000001;
    double v = neg ? -value : value;
    if (v < 0.0000001) v = 0.0;

    char buf[64];
    if (v >= 1.0e8) {
        std::snprintf(buf, sizeof(buf), "%.2f亿", v / 1.0e8);
    } else if (v >= 1.0e4) {
        std::snprintf(buf, sizeof(buf), "%.2f万", v / 1.0e4);
    } else if (decimals > 0) {
        std::snprintf(buf, sizeof(buf), "%.*f", decimals, v);
    } else {
        std::snprintf(buf, sizeof(buf), "%.0f", v);
    }
    std::string out = neg ? "-" : "";
    out += buf;
    return out;
}

std::string formatMetricValue(double value, const MetricInfo& info) {
    return formatCompactNumber(value, info.decimals);
}
