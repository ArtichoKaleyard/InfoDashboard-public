#pragma once

#include "dashboard_layout.h"
#include "dashboard_style.h"

#include <cstdint>
#include <lvgl.h>

namespace dashboard_widgets {

// 基于 canvas 的 1bpp 视觉条。它属于绘制细节，放在组件层；
// user_app.cpp 只决定显示哪个百分比。
struct SegmentBar {
    lv_obj_t *canvas = nullptr;
    uint16_t pixels[dashboard_style::kSegmentBarWidth * dashboard_style::kSegmentBarHeight] = {};
};

struct LedRow {
    lv_obj_t *canvas = nullptr;
    uint16_t pixels[dashboard_style::kGpuLedRowWidth * dashboard_style::kGpuLedRowHeight] = {};
};

// 标签/文字基础件。创建 UI 时优先使用 layout::Rect 重载，让几何位置
// 始终从 dashboard_layout.h 调整。
lv_obj_t *CreateLabel(lv_obj_t *parent, const char *text, int x, int y, int width,
                      const lv_font_t *font, lv_text_align_t align,
                      int letter_space = dashboard_style::kTextLetterSpaceDefault);
lv_obj_t *CreateLabel(lv_obj_t *parent, const char *text, dashboard_layout::Rect rect,
                      const lv_font_t *font, lv_text_align_t align,
                      int letter_space = dashboard_style::kTextLetterSpaceDefault);
lv_obj_t *CreateText(lv_obj_t *parent, const char *text, int x, int y, int width,
                     const lv_font_t *font, lv_color_t color, lv_text_align_t align,
                     int letter_space = dashboard_style::kTextLetterSpaceDefault);
lv_obj_t *CreateText(lv_obj_t *parent, const char *text, dashboard_layout::Rect rect,
                     const lv_font_t *font, lv_color_t color, lv_text_align_t align,
                     int letter_space = dashboard_style::kTextLetterSpaceDefault);

// 填充背景和带框区域。顶栏、黑色分隔线、状态块都通过这些辅助函数创建，
// 不直接在业务代码里拼 LVGL 对象。
lv_obj_t *CreatePanel(lv_obj_t *parent, int x, int y, int width, int height);
lv_obj_t *CreateThinPanel(lv_obj_t *parent, int x, int y, int width, int height);
lv_obj_t *CreateThinPanel(lv_obj_t *parent, dashboard_layout::Rect rect);
lv_obj_t *CreateFill(lv_obj_t *parent, int x, int y, int width, int height, lv_color_t color);
lv_obj_t *CreateFill(lv_obj_t *parent, dashboard_layout::Rect rect, lv_color_t color);
lv_obj_t *CreateBadgeSmall(lv_obj_t *parent, const char *text, int x, int y, int width, int height);
lv_obj_t *CreateBadgeSmall(lv_obj_t *parent, const char *text, dashboard_layout::Rect rect);

// 分隔线和图表基础件。虚线、格子、条形图的绘制行为先改
// dashboard_style.h；位置坐标改 dashboard_layout.h。
void CreateDashedRule(lv_obj_t *parent, dashboard_layout::Rect rect);
lv_obj_t *CreateMiniChartBudgetLine(lv_obj_t *parent, dashboard_layout::Rect rect);
void SetMiniChartBudgetLine(lv_obj_t *line, dashboard_layout::Rect rect, uint8_t percent);
void CreateDashedRuleY(lv_obj_t *parent, int x, int y, int height);
void CreateDashedRuleY(lv_obj_t *parent, dashboard_layout::Rect rect);
void DrawLedRow(LedRow *row, uint8_t percent);
void CreateLedRow(lv_obj_t *parent, LedRow *row, int x, int y);
void CreateLedRow(lv_obj_t *parent, LedRow *row, dashboard_layout::Rect rect);
void DrawSegmentBar(SegmentBar *bar, uint8_t percent);
void CreateSegmentBar(lv_obj_t *parent, SegmentBar *bar, int x, int y);
void CreateSegmentBar(lv_obj_t *parent, SegmentBar *bar, dashboard_layout::Rect rect);
void CreateMiniChart(lv_obj_t *parent, lv_obj_t **bars, int count, int x, int y, int width, int height);
void CreateMiniChart(lv_obj_t *parent, lv_obj_t **bars, int count, dashboard_layout::Rect rect);
void SetMiniChart(lv_obj_t **bars, int count, const uint8_t *values, int baseline_y);

}  // 命名空间 dashboard_widgets
