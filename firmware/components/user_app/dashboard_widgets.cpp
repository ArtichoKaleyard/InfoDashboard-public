#include "dashboard_widgets.h"

namespace layout = dashboard_layout;
namespace style = dashboard_style;

namespace dashboard_widgets {

lv_obj_t *CreateLabel(lv_obj_t *parent, const char *text, int x, int y, int width,
                      const lv_font_t *font, lv_text_align_t align, int letter_space) {
    lv_obj_t *label = lv_label_create(parent);
    lv_obj_set_pos(label, x, y);
    lv_obj_set_width(label, width);
    lv_label_set_long_mode(label, LV_LABEL_LONG_CLIP);
    lv_label_set_text(label, text);
    lv_obj_set_style_text_font(label, font, LV_PART_MAIN);
    lv_obj_set_style_text_color(label, style::Ink(), LV_PART_MAIN);
    lv_obj_set_style_text_align(label, align, LV_PART_MAIN);
    lv_obj_set_style_text_letter_space(label, letter_space, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(label, LV_OPA_TRANSP, LV_PART_MAIN);
    return label;
}

lv_obj_t *CreateText(lv_obj_t *parent, const char *text, int x, int y, int width,
                     const lv_font_t *font, lv_color_t color, lv_text_align_t align, int letter_space) {
    lv_obj_t *label = CreateLabel(parent, text, x, y, width, font, align, letter_space);
    lv_obj_set_style_text_color(label, color, LV_PART_MAIN);
    return label;
}

lv_obj_t *CreateLabel(lv_obj_t *parent, const char *text, layout::Rect rect,
                      const lv_font_t *font, lv_text_align_t align, int letter_space) {
    return CreateLabel(parent, text, rect.x, rect.y, rect.w, font, align, letter_space);
}

lv_obj_t *CreateText(lv_obj_t *parent, const char *text, layout::Rect rect,
                     const lv_font_t *font, lv_color_t color, lv_text_align_t align, int letter_space) {
    return CreateText(parent, text, rect.x, rect.y, rect.w, font, color, align, letter_space);
}

lv_obj_t *CreatePanel(lv_obj_t *parent, int x, int y, int width, int height) {
    lv_obj_t *panel = lv_obj_create(parent);
    lv_obj_set_pos(panel, x, y);
    lv_obj_set_size(panel, width, height);
    lv_obj_clear_flag(panel, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_radius(panel, style::kPanelRadius, LV_PART_MAIN);
    lv_obj_set_style_border_width(panel, style::kPanelBorderWidth, LV_PART_MAIN);
    lv_obj_set_style_border_color(panel, style::Ink(), LV_PART_MAIN);
    lv_obj_set_style_pad_all(panel, 0, LV_PART_MAIN);
    lv_obj_set_style_bg_color(panel, style::Paper(), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(panel, LV_OPA_COVER, LV_PART_MAIN);
    return panel;
}

lv_obj_t *CreateThinPanel(lv_obj_t *parent, int x, int y, int width, int height) {
    lv_obj_t *panel = CreatePanel(parent, x, y, width, height);
    lv_obj_set_style_radius(panel, style::kThinPanelRadius, LV_PART_MAIN);
    lv_obj_set_style_border_width(panel, style::kThinPanelBorderWidth, LV_PART_MAIN);
    return panel;
}

lv_obj_t *CreateThinPanel(lv_obj_t *parent, layout::Rect rect) {
    return CreateThinPanel(parent, rect.x, rect.y, rect.w, rect.h);
}

lv_obj_t *CreateFill(lv_obj_t *parent, int x, int y, int width, int height, lv_color_t color) {
    lv_obj_t *fill = lv_obj_create(parent);
    lv_obj_set_pos(fill, x, y);
    lv_obj_set_size(fill, width, height);
    lv_obj_clear_flag(fill, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_radius(fill, style::kFillRadius, LV_PART_MAIN);
    lv_obj_set_style_border_width(fill, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(fill, 0, LV_PART_MAIN);
    lv_obj_set_style_bg_color(fill, color, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(fill, LV_OPA_COVER, LV_PART_MAIN);
    return fill;
}

lv_obj_t *CreateFill(lv_obj_t *parent, layout::Rect rect, lv_color_t color) {
    return CreateFill(parent, rect.x, rect.y, rect.w, rect.h, color);
}

lv_obj_t *CreateBadgeSmall(lv_obj_t *parent, const char *text, int x, int y, int width, int height) {
    lv_obj_t *badge = CreateFill(parent, x, y, width, height, style::Ink());
    lv_obj_set_style_radius(badge, style::kBlockRadius, LV_PART_MAIN);
    return CreateText(badge, text, style::kBadgeTextInset, style::kBadgeTextInset, width - style::kBadgeTextInset * 2,
                      style::FontFusion10(), style::Paper(), LV_TEXT_ALIGN_CENTER);
}

lv_obj_t *CreateBadgeSmall(lv_obj_t *parent, const char *text, layout::Rect rect) {
    return CreateBadgeSmall(parent, text, rect.x, rect.y, rect.w, rect.h);
}

void CreateDashedRule(lv_obj_t *parent, layout::Rect rect) {
    for (int x = rect.x; x < rect.x + rect.w; x += style::kDashWidth + style::kDashGap) {
        const int width = ((x + style::kDashWidth) > (rect.x + rect.w)) ? (rect.x + rect.w - x) : style::kDashWidth;
        CreateFill(parent, x, rect.y, width, rect.h, style::Ink());
    }
}

lv_obj_t *CreateMiniChartBudgetLine(lv_obj_t *parent, layout::Rect rect) {
    constexpr int kBudgetLineDashWidth = 2;
    constexpr int kBudgetLineGap = 16;
    lv_obj_t *line = lv_obj_create(parent);
    lv_obj_remove_style_all(line);
    lv_obj_set_pos(line, rect.x, rect.y);
    lv_obj_set_size(line, rect.w, 1);
    lv_obj_set_style_bg_opa(line, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_border_width(line, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(line, 0, LV_PART_MAIN);
    lv_obj_clear_flag(line, LV_OBJ_FLAG_SCROLLABLE);
    for (int x = 0; x < rect.w; x += kBudgetLineDashWidth + kBudgetLineGap) {
        const int width = ((x + kBudgetLineDashWidth) > rect.w) ? (rect.w - x) : kBudgetLineDashWidth;
        CreateFill(line, x, 0, width, 1, style::Ink());
    }
    SetMiniChartBudgetLine(line, rect, 100 * style::kMiniChartBudgetLineNumerator /
                                      style::kMiniChartBudgetLineDenominator);
    return line;
}

void SetMiniChartBudgetLine(lv_obj_t *line, layout::Rect rect, uint8_t percent) {
    if (!line) {
        return;
    }
    const uint8_t clipped_percent = percent > 100 ? 100 : percent;
    const int line_height = (style::kMiniChartMaxHeight * clipped_percent + 50) / 100;
    lv_obj_set_y(line, rect.y + rect.h - line_height);
}

void CreateDashedRuleY(lv_obj_t *parent, int x, int y, int height) {
    for (int pos = y; pos < y + height; pos += style::kDashHeight + style::kDashYGap) {
        const int h = ((pos + style::kDashHeight) > (y + height)) ? (y + height - pos) : style::kDashHeight;
        CreateFill(parent, x, pos, 1, h, style::Ink());
    }
}

void CreateDashedRuleY(lv_obj_t *parent, layout::Rect rect) {
    CreateDashedRuleY(parent, rect.x, rect.y, rect.h);
}

void DrawLedPixel(LedRow *row, int x, int y, uint16_t color) {
    if (x < 0 || x >= style::kGpuLedRowWidth || y < 0 || y >= style::kGpuLedRowHeight) {
        return;
    }
    row->pixels[y * style::kGpuLedRowWidth + x] = color;
}

void FillLedRect(LedRow *row, int x, int y, int width, int height, uint16_t color) {
    for (int py = y; py < y + height; ++py) {
        for (int px = x; px < x + width; ++px) {
            DrawLedPixel(row, px, py, color);
        }
    }
}

void DrawLedCell(LedRow *row, int x, int width, bool filled, bool first, bool last) {
    constexpr int h = style::kGpuLedRowHeight;
    for (int py = 0; py < h; ++py) {
        for (int px = 0; px < width; ++px) {
            const bool outside_left_corner = first && px == 0 && (py == 0 || py == h - 1);
            const bool outside_right_corner = last && px == width - 1 && (py == 0 || py == h - 1);
            if (outside_left_corner || outside_right_corner) {
                continue;
            }
            const bool border = px == 0 || px == width - 1 || py == 0 || py == h - 1;
            const uint16_t color = filled || border ? style::kSegmentColorBlack : style::kSegmentColorWhite;
            DrawLedPixel(row, x + px, py, color);
        }
    }
}

void DrawLedRow(LedRow *row, uint8_t percent) {
    FillLedRect(row, 0, 0, style::kGpuLedRowWidth, style::kGpuLedRowHeight, style::kSegmentColorWhite);
    constexpr int count = style::kGpuLedCellCount;
    const int filled = (count * (percent > 100 ? 100 : percent) + 99) / 100;
    const int cell_width = (style::kGpuLedRowWidth - style::kGpuLedCellGap * (count - 1)) / count;
    for (int i = 0; i < count; ++i) {
        const int x = i * (cell_width + style::kGpuLedCellGap);
        DrawLedCell(row, x, cell_width, i < filled, i == 0, i == count - 1);
    }
    if (row->canvas) {
        lv_obj_invalidate(row->canvas);
    }
}

void CreateLedRow(lv_obj_t *parent, LedRow *row, int x, int y) {
    row->canvas = lv_canvas_create(parent);
    lv_canvas_set_buffer(row->canvas, row->pixels, style::kGpuLedRowWidth, style::kGpuLedRowHeight,
                         LV_COLOR_FORMAT_RGB565);
    lv_obj_set_pos(row->canvas, x, y);
    lv_obj_set_size(row->canvas, style::kGpuLedRowWidth, style::kGpuLedRowHeight);
    DrawLedRow(row, 0);
}

void CreateLedRow(lv_obj_t *parent, LedRow *row, layout::Rect rect) {
    CreateLedRow(parent, row, rect.x, rect.y);
}

void DrawPixel(SegmentBar *bar, int x, int y, uint16_t color) {
    if (x < 0 || x >= style::kSegmentBarWidth || y < 0 || y >= style::kSegmentBarHeight) {
        return;
    }
    bar->pixels[y * style::kSegmentBarWidth + x] = color;
}

void FillRect(SegmentBar *bar, int x, int y, int width, int height, uint16_t color) {
    for (int py = y; py < y + height; ++py) {
        for (int px = x; px < x + width; ++px) {
            DrawPixel(bar, px, py, color);
        }
    }
}

void DrawRoundedSegmentBorder(SegmentBar *bar) {
    constexpr int w = style::kSegmentBarWidth;
    constexpr int h = style::kSegmentBarHeight;
    constexpr int r = style::kSegmentBarRadius;

    FillRect(bar, r, 0, w - r * 2, style::kSegmentBarBorder, style::kSegmentColorBlack);
    FillRect(bar, r, h - style::kSegmentBarBorder, w - r * 2, style::kSegmentBarBorder,
             style::kSegmentColorBlack);
    FillRect(bar, 0, r, style::kSegmentBarBorder, h - r * 2, style::kSegmentColorBlack);
    FillRect(bar, w - style::kSegmentBarBorder, r, style::kSegmentBarBorder, h - r * 2,
             style::kSegmentColorBlack);

    DrawPixel(bar, 1, 0, style::kSegmentColorBlack);
    DrawPixel(bar, 0, 1, style::kSegmentColorBlack);
    DrawPixel(bar, 1, 1, style::kSegmentColorBlack);
    DrawPixel(bar, w - 2, 0, style::kSegmentColorBlack);
    DrawPixel(bar, w - 1, 1, style::kSegmentColorBlack);
    DrawPixel(bar, w - 2, 1, style::kSegmentColorBlack);
    DrawPixel(bar, 1, h - 1, style::kSegmentColorBlack);
    DrawPixel(bar, 0, h - 2, style::kSegmentColorBlack);
    DrawPixel(bar, 1, h - 2, style::kSegmentColorBlack);
    DrawPixel(bar, w - 2, h - 1, style::kSegmentColorBlack);
    DrawPixel(bar, w - 1, h - 2, style::kSegmentColorBlack);
    DrawPixel(bar, w - 2, h - 2, style::kSegmentColorBlack);
}

void DrawSegmentBar(SegmentBar *bar, uint8_t percent) {
    FillRect(bar, 0, 0, style::kSegmentBarWidth, style::kSegmentBarHeight, style::kSegmentColorWhite);
    DrawRoundedSegmentBorder(bar);

    const int inner_width = style::kSegmentBarWidth - style::kSegmentBarBorder * 2;
    const int filled_width = (inner_width * (percent > 100 ? 100 : percent)) / 100;
    const int total_part_width = inner_width - style::kSegmentBarGap * (style::kSegmentBarParts - 1);
    const int base_part_width = total_part_width / style::kSegmentBarParts;
    const int extra_pixels = total_part_width % style::kSegmentBarParts;
    int x = style::kSegmentBarBorder;
    for (int i = 0; i < style::kSegmentBarParts; ++i) {
        const int part_width = base_part_width + (i < extra_pixels ? 1 : 0);
        if (x - style::kSegmentBarBorder >= filled_width) {
            break;
        }
        const int width = ((x - style::kSegmentBarBorder + part_width) > filled_width)
                              ? (filled_width - (x - style::kSegmentBarBorder))
                              : part_width;
        if (width > 0) {
            FillRect(bar, x, style::kSegmentBarFillInsetY, width,
                     style::kSegmentBarHeight - style::kSegmentBarFillInsetY * 2, style::kSegmentColorBlack);
        }
        x += part_width + style::kSegmentBarGap;
    }
    if (bar->canvas) {
        lv_obj_invalidate(bar->canvas);
    }
}

void CreateSegmentBar(lv_obj_t *parent, SegmentBar *bar, int x, int y) {
    bar->canvas = lv_canvas_create(parent);
    lv_canvas_set_buffer(bar->canvas, bar->pixels, style::kSegmentBarWidth, style::kSegmentBarHeight,
                         LV_COLOR_FORMAT_RGB565);
    lv_obj_set_pos(bar->canvas, x, y);
    lv_obj_set_size(bar->canvas, style::kSegmentBarWidth, style::kSegmentBarHeight);
    lv_obj_set_style_radius(bar->canvas, style::kSegmentBarRadius, LV_PART_MAIN);
    DrawSegmentBar(bar, 0);
}

void CreateSegmentBar(lv_obj_t *parent, SegmentBar *bar, layout::Rect rect) {
    CreateSegmentBar(parent, bar, rect.x, rect.y);
}

void CreateMiniChart(lv_obj_t *parent, lv_obj_t **bars, int count, int x, int y, int width, int height) {
    const int chart_count = count <= 0 ? 1 : count;
    const int total_width = style::kMiniChartBarWidth * chart_count +
                            style::kMiniChartGap * (chart_count - 1);
    const int start_x = x + (width - total_width) / 2;
    for (int i = 0; i < chart_count; ++i) {
        bars[i] = CreateFill(parent, start_x + i * (style::kMiniChartBarWidth + style::kMiniChartGap),
                             y + height - style::kMiniChartInitialBarHeight,
                             style::kMiniChartBarWidth, style::kMiniChartInitialBarHeight, style::Ink());
        lv_obj_set_style_radius(bars[i], style::kMiniChartBarRadius, LV_PART_MAIN);
    }
}

void CreateMiniChart(lv_obj_t *parent, lv_obj_t **bars, int count, layout::Rect rect) {
    CreateMiniChart(parent, bars, count, rect.x, rect.y, rect.w, rect.h);
}

void SetMiniChart(lv_obj_t **bars, int count, const uint8_t *values, int baseline_y) {
    for (int i = 0; i < count; ++i) {
        const uint8_t value = values[i];
        const uint8_t clipped_value = value > 100 ? 100 : value;
        int height = (style::kMiniChartMaxHeight * clipped_value) / 100;
        if (clipped_value == 0) {
            height = style::kMiniChartZeroBarHeight;
        } else if (height < style::kMiniChartMinBarHeight) {
            height = style::kMiniChartMinBarHeight;
        }
        lv_obj_set_y(bars[i], baseline_y - height);
        lv_obj_set_height(bars[i], height);
        lv_obj_set_style_bg_color(bars[i], style::Ink(), LV_PART_MAIN);
    }
}

}  // 命名空间 dashboard_widgets
