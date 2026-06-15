#pragma once

#include <cstdint>
#include <lvgl.h>

namespace dashboard_style {

// 固件 UI 使用的字体槽。修改这些映射前，先用
// tools/generate_dashboard_fonts.ps1 重新生成对应字体 C 文件。
const lv_font_t *FontFusion10();
const lv_font_t *FontFusion12();
const lv_font_t *FontConsolasRegular14();
const lv_font_t *FontConsolasBold14();
const lv_font_t *FontZhengge16();
const lv_font_t *FontSegoe16();
const lv_font_t *FontSegoeSemibold16();
const lv_font_t *FontSegoe32();

// 黑白屏颜色入口。黑底条、状态块、卡片、格子和文字颜色都从这里调，
// 避免在业务逻辑里分散写颜色。
inline lv_color_t Ink() {
    return lv_color_black();
}

inline lv_color_t Paper() {
    return lv_color_white();
}

// 通用 LVGL 形状样式。这里对应屏幕上可见的卡片边框、圆角和填充，
// 不承载数据含义。
inline constexpr int kPanelRadius = 4;
inline constexpr int kPanelBorderWidth = 2;
inline constexpr int kThinPanelRadius = 4;
inline constexpr int kThinPanelBorderWidth = 1;
inline constexpr int kFillRadius = 0;
inline constexpr int kBlockRadius = 2;
inline constexpr int kFooterBarRadius = 3;
inline constexpr int kBadgeTextInset = 2;
inline constexpr int kTextLetterSpaceDefault = 0;
inline constexpr int kTextLetterSpaceRelaxed = 1;
inline constexpr int kTextLetterSpaceLoose = 2;

// 虚线分隔线参数。调服务器/Codex 面板里的虚线密度和节奏时改这里。
inline constexpr int kDashWidth = 4;
inline constexpr int kDashGap = 3;
inline constexpr int kDashHeight = 4;
inline constexpr int kDashYGap = 3;

// GPU 利用率格子和小型趋势柱参数。
inline constexpr int kGpuLedCellCount = 9;
inline constexpr int kGpuLedCellGap = 2;
inline constexpr int kGpuLedRowWidth = 82;
inline constexpr int kGpuLedRowHeight = 7;
inline constexpr int kMiniChartMaxBarCount = 8;
inline constexpr int kMiniChartWeekBarCount = 7;
inline constexpr int kMiniChartFiveHourBarCount = 5;
inline constexpr int kMiniChartBarWidth = 5;
inline constexpr int kMiniChartBarRadius = 2;
inline constexpr int kMiniChartGap = 6;
inline constexpr int kMiniChartMaxHeight = 42;
inline constexpr int kMiniChartMinBarHeight = 3;
inline constexpr int kMiniChartZeroBarHeight = 1;
inline constexpr int kMiniChartInitialBarHeight = 4;
inline constexpr int kMiniChartBudgetLineNumerator = 2;
inline constexpr int kMiniChartBudgetLineDenominator = 3;

// 基于 LVGL canvas 的斑马条/分段条参数。宽高必须和 SegmentBar 的固定
// 缓冲区大小一致。
inline constexpr int kSegmentBarWidth = 174;
inline constexpr int kSegmentBarHeight = 12;
inline constexpr int kSegmentBarParts = 36;
inline constexpr int kSegmentBarGap = 1;
inline constexpr int kSegmentBarRadius = 2;
inline constexpr int kSegmentBarBorder = 1;
inline constexpr int kSegmentBarFillInsetY = 2;
inline constexpr uint16_t kSegmentColorBlack = 0x0000;
inline constexpr uint16_t kSegmentColorWhite = 0xffff;

}  // 命名空间 dashboard_style
