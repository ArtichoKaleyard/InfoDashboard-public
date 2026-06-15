#include "dashboard_style.h"

namespace {

// 这些符号由 lv_font_conv 生成。集中放在这里后，user_app.cpp 不需要知道
// 生成 C 文件名或字体符号名。
extern "C" {
extern const lv_font_t dashboard_font_ui_10_1bpp;
extern const lv_font_t dashboard_font_ui_12_1bpp;
extern const lv_font_t dashboard_font_ui_consolas_regular_14_1bpp;
extern const lv_font_t dashboard_font_ui_consolas_bold_14_1bpp;
extern const lv_font_t dashboard_font_ui_16_1bpp;
extern const lv_font_t dashboard_font_ui_16m_1bpp;
extern const lv_font_t dashboard_font_ui_segoe_semibold_16_1bpp;
extern const lv_font_t dashboard_font_ui_28_1bpp;
}

}  // 匿名命名空间

namespace dashboard_style {

const lv_font_t *FontFusion10() {
    return &dashboard_font_ui_10_1bpp;
}

const lv_font_t *FontFusion12() {
    return &dashboard_font_ui_12_1bpp;
}

const lv_font_t *FontConsolasRegular14() {
    return &dashboard_font_ui_consolas_regular_14_1bpp;
}

const lv_font_t *FontConsolasBold14() {
    return &dashboard_font_ui_consolas_bold_14_1bpp;
}

const lv_font_t *FontZhengge16() {
    return &dashboard_font_ui_16_1bpp;
}

const lv_font_t *FontSegoe16() {
    return &dashboard_font_ui_16m_1bpp;
}

const lv_font_t *FontSegoeSemibold16() {
    return &dashboard_font_ui_segoe_semibold_16_1bpp;
}

const lv_font_t *FontSegoe32() {
    return &dashboard_font_ui_28_1bpp;
}

}  // 命名空间 dashboard_style
