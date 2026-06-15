#pragma once

namespace dashboard_text {

// 固定文案和占位符。改屏幕文字时优先改这里；需要快照数值参与的动态
// 格式化仍在 user_app.cpp 调用这些格式串。
inline constexpr char kHeaderTitle[] = "> SYS.OP_BOARD";
inline constexpr char kHeaderDatePlaceholder[] = "--";
inline constexpr char kHeaderTimePlaceholder[] = "--:--:--";

inline constexpr char kServerTargetPlaceholder[] = "[NODE] --";
inline constexpr char kBadgePlaceholder[] = "--";
inline constexpr char kGpuLabelPlaceholder[] = "GPU.-";
inline constexpr char kGpuTempPlaceholder[] = "--C";
inline constexpr char kGpuUtilPlaceholder[] = "--";
inline constexpr char kGpuPercentSign[] = "%";
inline constexpr char kVramLabelPlaceholder[] = "VRAM.GPU-";
inline constexpr char kPercentPlaceholder[] = "--%";
inline constexpr char kGpuPower0Placeholder[] = "P.GPU0 --W";
inline constexpr char kGpuPower1Placeholder[] = "P.GPU1 --W";
inline constexpr char kCpuPlaceholder[] = "CPU --%";
inline constexpr char kMemPlaceholder[] = "MEM --%";
inline constexpr char kProcPlaceholder[] = "PROC --";

inline constexpr char kCodexTitle[] = "Codex Pro 5x";
inline constexpr char kTrend7Day[] = "7_DAY";
inline constexpr char kTrend5Hour[] = "5_HOUR";
inline constexpr char kWeekQuotaLabel[] = "QUOTA.WEEK";
inline constexpr char kFiveHourQuotaLabel[] = "QUOTA.5_HOUR";
inline constexpr char kWeekResetLabel[] = "WEEK RESET";
inline constexpr char kFiveHourResetLabel[] = "5H RESET";

inline constexpr char kWeatherPlaceholder[] = "> Weather: --";
inline constexpr char kIndoorPlaceholder[] = "Indoor: --";
inline constexpr char kFooterLinkPlaceholder[] = "> Link: [ -- ]";
inline constexpr char kFooterBatteryPlaceholder[] = "Battery: --";
inline constexpr char kFooterUpdatedPlaceholder[] = "Updated: --";

// 运行时格式串。和固定文案放在一起，避免改显示文字时去数据拉取逻辑里查找。
inline constexpr char kGpuLabelFormat[] = "GPU.%u";
inline constexpr char kGpuTempFormat[] = "%uC";
inline constexpr char kUnsignedValueFormat[] = "%u";
inline constexpr char kVramLabelFormat[] = "VRAM.GPU%u";
inline constexpr char kPercentValueFormat[] = "%u%%";
inline constexpr char kGpuPowerFormat[] = "P.GPU%u %uW";
inline constexpr char kGpuPowerMissingFormat[] = "P.GPU%u --W";
inline constexpr char kCpuFormat[] = "CPU %u%%";
inline constexpr char kMemFormat[] = "MEM %u%%";
inline constexpr char kProcFormat[] = "PROC %02u";
inline constexpr char kWeatherFormat[] = "> Weather: %s %.1fC / %s";
inline constexpr char kIndoorMissing[] = "Indoor: --";
inline constexpr char kIndoorFormat[] = "Indoor: %.1fC / %u%%";
inline constexpr char kFooterBatteryFormat[] = "Battery %s";
inline constexpr char kFooterLinkFormat[] = "> Link: [ %s ]";
inline constexpr char kFooterUpdatedFormat[] = "Updated %s";

}  // 命名空间 dashboard_text
