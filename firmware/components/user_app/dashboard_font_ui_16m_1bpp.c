/*******************************************************************************
 * Size: 16 px
 * Bpp: 1
 ******************************************************************************/

#ifdef LV_LVGL_H_INCLUDE_SIMPLE
#include "lvgl.h"
#else
#include "lvgl.h"
#endif

#ifndef DASHBOARD_FONT_UI_16M_1BPP
#define DASHBOARD_FONT_UI_16M_1BPP 1
#endif

#if DASHBOARD_FONT_UI_16M_1BPP

/*-----------------
 *    BITMAPS
 *----------------*/

/*Store the image of the glyphs*/
static LV_ATTRIBUTE_LARGE_CONST const uint8_t glyph_bitmap[] = {
    /* U+0020 " " */
    0x0,

    /* U+0021 "!" */
    0xff, 0x20,

    /* U+0022 "\"" */
    0x99, 0x90,

    /* U+0023 "#" */
    0x9, 0x9, 0x4, 0x8f, 0xf1, 0x20, 0x93, 0xfc,
    0x48, 0x24, 0x12, 0x0,

    /* U+0024 "$" */
    0x10, 0x43, 0xb4, 0x92, 0x47, 0xe, 0x1c, 0x51,
    0x65, 0xf8, 0x41, 0x0,

    /* U+0025 "%" */
    0x70, 0x48, 0x88, 0x89, 0x88, 0x90, 0x8a, 0x7,
    0x4e, 0x5, 0x10, 0x91, 0x11, 0x11, 0x11, 0x20,
    0xe0,

    /* U+0026 "&" */
    0x3c, 0x4, 0x20, 0x42, 0x4, 0x64, 0x38, 0x46,
    0xc4, 0x86, 0x88, 0x38, 0x81, 0x8c, 0x38, 0x7c,
    0x70,

    /* U+0027 "'" */
    0xe0,

    /* U+0028 "(" */
    0x29, 0x69, 0x24, 0x93, 0x24, 0x40,

    /* U+0029 ")" */
    0x89, 0x32, 0x49, 0x25, 0xa5, 0x0,

    /* U+002A "*" */
    0x21, 0x3e, 0xc5, 0x0,

    /* U+002B "+" */
    0x10, 0x20, 0x47, 0xf1, 0x2, 0x4, 0x0,

    /* U+002C "," */
    0x56,

    /* U+002D "-" */
    0xf0,

    /* U+002E "." */
    0xc0,

    /* U+002F "/" */
    0x4, 0x10, 0x82, 0x18, 0x43, 0x8, 0x21, 0x4,
    0x30, 0x80,

    /* U+0030 "0" */
    0x38, 0x8a, 0xc, 0x18, 0x30, 0x60, 0xc1, 0x82,
    0x88, 0xe0,

    /* U+0031 "1" */
    0x17, 0xd1, 0x11, 0x11, 0x11, 0x10,

    /* U+0032 "2" */
    0x7a, 0x30, 0x41, 0x4, 0x21, 0x18, 0xc2, 0xf,
    0xc0,

    /* U+0033 "3" */
    0x79, 0x18, 0x10, 0x20, 0x8e, 0x3, 0x1, 0x3,
    0xb, 0xe0,

    /* U+0034 "4" */
    0x4, 0xc, 0x1c, 0x14, 0x24, 0x64, 0xc4, 0xff,
    0x4, 0x4, 0x4,

    /* U+0035 "5" */
    0x7e, 0x81, 0x2, 0x7, 0x80, 0x80, 0x81, 0x3,
    0xb, 0xe0,

    /* U+0036 "6" */
    0x1c, 0xc1, 0x4, 0xb, 0xd8, 0xe0, 0xc1, 0x82,
    0x88, 0xe0,

    /* U+0037 "7" */
    0xfe, 0xc, 0x10, 0x60, 0x83, 0x4, 0x8, 0x30,
    0x40, 0x80,

    /* U+0038 "8" */
    0x3c, 0xcd, 0xb, 0x33, 0xc8, 0xe0, 0xc1, 0x82,
    0x88, 0xe0,

    /* U+0039 "9" */
    0x38, 0x8a, 0xc, 0x18, 0x38, 0xce, 0x81, 0x4,
    0x1b, 0xe0,

    /* U+003A ":" */
    0xc0, 0x3,

    /* U+003B ";" */
    0x60, 0x0, 0x12, 0x50,

    /* U+003C "<" */
    0x6, 0x31, 0x86, 0x7, 0x3, 0x81, 0x80,

    /* U+003D "=" */
    0xfe, 0x0, 0x7, 0xf0,

    /* U+003E ">" */
    0xc0, 0xc0, 0x60, 0x31, 0x8c, 0x20, 0x0,

    /* U+003F "?" */
    0x7a, 0x30, 0x41, 0x8, 0x42, 0x8, 0x0, 0x6,
    0x0,

    /* U+0040 "@" */
    0xf, 0x81, 0x83, 0x18, 0x4, 0x8f, 0x38, 0x88,
    0xc8, 0x46, 0x42, 0x32, 0x11, 0x91, 0x92, 0x77,
    0x10, 0x0, 0x60, 0x0, 0xfc, 0x0,

    /* U+0041 "A" */
    0xc, 0x3, 0x1, 0xe0, 0x48, 0x13, 0xc, 0x42,
    0x10, 0xfe, 0x60, 0x90, 0x3c, 0xc,

    /* U+0042 "B" */
    0xf9, 0x1a, 0x14, 0x28, 0x9f, 0x21, 0xc1, 0x83,
    0xf, 0xe0,

    /* U+0043 "C" */
    0x1f, 0x90, 0x50, 0x10, 0x8, 0x4, 0x2, 0x1,
    0x0, 0x40, 0x30, 0x47, 0xc0,

    /* U+0044 "D" */
    0xfc, 0x41, 0xa0, 0x50, 0x18, 0xc, 0x6, 0x3,
    0x1, 0x81, 0x43, 0x3f, 0x0,

    /* U+0045 "E" */
    0xfa, 0x8, 0x20, 0x83, 0xe8, 0x20, 0x82, 0xf,
    0xc0,

    /* U+0046 "F" */
    0xfc, 0x21, 0x8, 0x7e, 0x10, 0x84, 0x20,

    /* U+0047 "G" */
    0x1f, 0x90, 0x50, 0x10, 0x8, 0x4, 0x3e, 0x3,
    0x1, 0x40, 0xb0, 0x47, 0xc0,

    /* U+0048 "H" */
    0x81, 0x81, 0x81, 0x81, 0x81, 0xff, 0x81, 0x81,
    0x81, 0x81, 0x81,

    /* U+0049 "I" */
    0xff, 0xe0,

    /* U+004A "J" */
    0x11, 0x11, 0x11, 0x11, 0x12, 0xe0,

    /* U+004B "K" */
    0x87, 0x1a, 0x65, 0x8a, 0x1c, 0x28, 0x48, 0x99,
    0x1a, 0x18,

    /* U+004C "L" */
    0x82, 0x8, 0x20, 0x82, 0x8, 0x20, 0x82, 0xf,
    0xc0,

    /* U+004D "M" */
    0xc0, 0x78, 0xf, 0x83, 0xd0, 0x5b, 0xb, 0x22,
    0x64, 0x4c, 0x51, 0x8a, 0x30, 0xc6, 0x10, 0x80,

    /* U+004E "N" */
    0xc0, 0xf0, 0x68, 0x36, 0x19, 0x8c, 0x46, 0x33,
    0xd, 0x82, 0xc0, 0xe0, 0x60,

    /* U+004F "O" */
    0x1e, 0x18, 0x64, 0xa, 0x1, 0x80, 0x60, 0x18,
    0x6, 0x1, 0x40, 0x98, 0x61, 0xe0,

    /* U+0050 "P" */
    0xf9, 0xe, 0xc, 0x18, 0x30, 0xbe, 0x40, 0x81,
    0x2, 0x0,

    /* U+0051 "Q" */
    0x1e, 0xc, 0x31, 0x2, 0x40, 0x28, 0x5, 0x0,
    0xa0, 0x14, 0x2, 0x40, 0x8c, 0x30, 0x7e, 0x0,
    0x60, 0x6,

    /* U+0052 "R" */
    0xfc, 0x86, 0x82, 0x82, 0x84, 0xf8, 0x8c, 0x84,
    0x86, 0x82, 0x83,

    /* U+0053 "S" */
    0x3f, 0x86, 0x4, 0x6, 0x7, 0x3, 0x1, 0x3,
    0xf, 0xf0,

    /* U+0054 "T" */
    0xff, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10,
    0x10, 0x10, 0x10,

    /* U+0055 "U" */
    0x81, 0x81, 0x81, 0x81, 0x81, 0x81, 0x81, 0x81,
    0x81, 0x42, 0x3c,

    /* U+0056 "V" */
    0xc0, 0xd0, 0x24, 0x19, 0x84, 0x21, 0xc, 0xc1,
    0x20, 0x48, 0x1e, 0x3, 0x0, 0xc0,

    /* U+0057 "W" */
    0xc1, 0x6, 0x83, 0x9, 0xa, 0x13, 0x14, 0x62,
    0x28, 0x84, 0xd9, 0x9, 0x12, 0x1a, 0x2c, 0x1c,
    0x70, 0x30, 0x60, 0x60, 0xc0,

    /* U+0058 "X" */
    0x41, 0xb0, 0x8c, 0xc2, 0xc1, 0xc0, 0x60, 0x70,
    0x2c, 0x23, 0x30, 0x90, 0x60,

    /* U+0059 "Y" */
    0x41, 0x30, 0x88, 0x86, 0x41, 0x40, 0xa0, 0x20,
    0x10, 0x8, 0x4, 0x2, 0x0,

    /* U+005A "Z" */
    0xff, 0x2, 0x6, 0x4, 0x8, 0x18, 0x10, 0x20,
    0x40, 0x40, 0xff,

    /* U+005B "[" */
    0xf2, 0x49, 0x24, 0x92, 0x49, 0xc0,

    /* U+005C "\\" */
    0x83, 0x4, 0x10, 0x20, 0x83, 0x4, 0x10, 0x20,
    0x83, 0x4,

    /* U+005D "]" */
    0xe4, 0x92, 0x49, 0x24, 0x93, 0xc0,

    /* U+005E "^" */
    0x10, 0x20, 0xa1, 0x44, 0x48, 0xa0, 0x80,

    /* U+005F "_" */
    0xfe,

    /* U+0060 "`" */
    0x99, 0x0,

    /* U+0061 "a" */
    0x78, 0x10, 0x4f, 0xc6, 0x18, 0xdd,

    /* U+0062 "b" */
    0x81, 0x2, 0x4, 0xb, 0xd8, 0xa0, 0xc1, 0x83,
    0x7, 0x15, 0xc0,

    /* U+0063 "c" */
    0x3d, 0x8, 0x20, 0x82, 0x4, 0xf,

    /* U+0064 "d" */
    0x2, 0x4, 0x8, 0x13, 0xa8, 0xe0, 0xc1, 0x83,
    0x5, 0x1b, 0xd0,

    /* U+0065 "e" */
    0x3c, 0x8e, 0xf, 0xf8, 0x10, 0x10, 0x9e,

    /* U+0066 "f" */
    0x19, 0x8, 0x4f, 0x90, 0x84, 0x21, 0x8, 0x40,

    /* U+0067 "g" */
    0x3a, 0x8e, 0xc, 0x18, 0x30, 0x51, 0xbd, 0x2,
    0x6, 0x17, 0xc0,

    /* U+0068 "h" */
    0x81, 0x2, 0x4, 0xb, 0xd8, 0xe0, 0xc1, 0x83,
    0x6, 0xc, 0x10,

    /* U+0069 "i" */
    0x8f, 0xf0,

    /* U+006A "j" */
    0x10, 0x0, 0x11, 0x11, 0x11, 0x11, 0x11, 0x2e,

    /* U+006B "k" */
    0x82, 0x8, 0x20, 0x8e, 0x6b, 0x38, 0xe2, 0xc9,
    0xa3,

    /* U+006C "l" */
    0xff, 0xf0,

    /* U+006D "m" */
    0xb9, 0xd9, 0xce, 0x10, 0xc2, 0x18, 0x43, 0x8,
    0x61, 0xc, 0x21,

    /* U+006E "n" */
    0xbd, 0x8e, 0xc, 0x18, 0x30, 0x60, 0xc1,

    /* U+006F "o" */
    0x3c, 0x42, 0x81, 0x81, 0x81, 0x81, 0x42, 0x3c,

    /* U+0070 "p" */
    0xbd, 0x8a, 0xc, 0x18, 0x30, 0x71, 0x5c, 0x81,
    0x2, 0x4, 0x0,

    /* U+0071 "q" */
    0x3a, 0x8e, 0xc, 0x18, 0x30, 0x51, 0xbd, 0x2,
    0x4, 0x8, 0x10,

    /* U+0072 "r" */
    0xbc, 0x88, 0x88, 0x88,

    /* U+0073 "s" */
    0x7c, 0x21, 0xc3, 0x84, 0x3e,

    /* U+0074 "t" */
    0x21, 0x3e, 0x42, 0x10, 0x84, 0x20, 0xc0,

    /* U+0075 "u" */
    0x83, 0x6, 0xc, 0x18, 0x30, 0x71, 0xbd,

    /* U+0076 "v" */
    0xc2, 0x42, 0x46, 0x64, 0x24, 0x28, 0x18, 0x18,

    /* U+0077 "w" */
    0xc6, 0x28, 0xc5, 0x19, 0xa5, 0x22, 0x94, 0x52,
    0x8c, 0x61, 0x84,

    /* U+0078 "x" */
    0x46, 0xc8, 0xa0, 0xc1, 0x85, 0x19, 0x23,

    /* U+0079 "y" */
    0x83, 0x8d, 0x12, 0x26, 0xc5, 0xa, 0x8, 0x10,
    0x60, 0x86, 0x0,

    /* U+007A "z" */
    0x7e, 0x8, 0x30, 0xc3, 0x4, 0x18, 0x7f,

    /* U+007B "{" */
    0x29, 0x24, 0xa2, 0x49, 0x22,

    /* U+007C "|" */
    0xff, 0xff,

    /* U+007D "}" */
    0x89, 0x24, 0x8a, 0x49, 0x28,

    /* U+007E "~" */
    0x72, 0x8e
};


/*---------------------
 *  GLYPH DESCRIPTION
 *--------------------*/

static const lv_font_fmt_txt_glyph_dsc_t glyph_dsc[] = {
    {.bitmap_index = 0, .adv_w = 0, .box_w = 0, .box_h = 0, .ofs_x = 0, .ofs_y = 0} /* id = 0 reserved */,
    {.bitmap_index = 0, .adv_w = 70, .box_w = 1, .box_h = 1, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 1, .adv_w = 73, .box_w = 1, .box_h = 11, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 3, .adv_w = 100, .box_w = 4, .box_h = 3, .ofs_x = 1, .ofs_y = 8},
    {.bitmap_index = 5, .adv_w = 151, .box_w = 9, .box_h = 10, .ofs_x = 0, .ofs_y = 1},
    {.bitmap_index = 17, .adv_w = 138, .box_w = 6, .box_h = 15, .ofs_x = 1, .ofs_y = -2},
    {.bitmap_index = 29, .adv_w = 210, .box_w = 12, .box_h = 11, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 46, .adv_w = 205, .box_w = 12, .box_h = 11, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 63, .adv_w = 59, .box_w = 1, .box_h = 3, .ofs_x = 1, .ofs_y = 8},
    {.bitmap_index = 64, .adv_w = 77, .box_w = 3, .box_h = 14, .ofs_x = 1, .ofs_y = -3},
    {.bitmap_index = 70, .adv_w = 77, .box_w = 3, .box_h = 14, .ofs_x = 1, .ofs_y = -3},
    {.bitmap_index = 76, .adv_w = 107, .box_w = 5, .box_h = 5, .ofs_x = 1, .ofs_y = 6},
    {.bitmap_index = 80, .adv_w = 175, .box_w = 7, .box_h = 7, .ofs_x = 2, .ofs_y = 1},
    {.bitmap_index = 87, .adv_w = 56, .box_w = 2, .box_h = 4, .ofs_x = 0, .ofs_y = -2},
    {.bitmap_index = 88, .adv_w = 102, .box_w = 4, .box_h = 1, .ofs_x = 1, .ofs_y = 4},
    {.bitmap_index = 89, .adv_w = 56, .box_w = 2, .box_h = 1, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 90, .adv_w = 100, .box_w = 6, .box_h = 13, .ofs_x = 0, .ofs_y = -2},
    {.bitmap_index = 100, .adv_w = 138, .box_w = 7, .box_h = 11, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 110, .adv_w = 138, .box_w = 4, .box_h = 11, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 116, .adv_w = 138, .box_w = 6, .box_h = 11, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 125, .adv_w = 138, .box_w = 7, .box_h = 11, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 135, .adv_w = 138, .box_w = 8, .box_h = 11, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 146, .adv_w = 138, .box_w = 7, .box_h = 11, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 156, .adv_w = 138, .box_w = 7, .box_h = 11, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 166, .adv_w = 138, .box_w = 7, .box_h = 11, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 176, .adv_w = 138, .box_w = 7, .box_h = 11, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 186, .adv_w = 138, .box_w = 7, .box_h = 11, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 196, .adv_w = 56, .box_w = 2, .box_h = 8, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 198, .adv_w = 56, .box_w = 3, .box_h = 10, .ofs_x = 0, .ofs_y = -2},
    {.bitmap_index = 202, .adv_w = 175, .box_w = 7, .box_h = 7, .ofs_x = 2, .ofs_y = 1},
    {.bitmap_index = 209, .adv_w = 175, .box_w = 7, .box_h = 4, .ofs_x = 2, .ofs_y = 2},
    {.bitmap_index = 213, .adv_w = 175, .box_w = 7, .box_h = 7, .ofs_x = 2, .ofs_y = 1},
    {.bitmap_index = 220, .adv_w = 115, .box_w = 6, .box_h = 11, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 229, .adv_w = 245, .box_w = 13, .box_h = 13, .ofs_x = 1, .ofs_y = -2},
    {.bitmap_index = 251, .adv_w = 165, .box_w = 10, .box_h = 11, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 265, .adv_w = 147, .box_w = 7, .box_h = 11, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 275, .adv_w = 159, .box_w = 9, .box_h = 11, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 288, .adv_w = 180, .box_w = 9, .box_h = 11, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 301, .adv_w = 130, .box_w = 6, .box_h = 11, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 310, .adv_w = 125, .box_w = 5, .box_h = 11, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 317, .adv_w = 176, .box_w = 9, .box_h = 11, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 330, .adv_w = 182, .box_w = 8, .box_h = 11, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 341, .adv_w = 68, .box_w = 1, .box_h = 11, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 343, .adv_w = 91, .box_w = 4, .box_h = 11, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 349, .adv_w = 149, .box_w = 7, .box_h = 11, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 359, .adv_w = 121, .box_w = 6, .box_h = 11, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 368, .adv_w = 230, .box_w = 11, .box_h = 11, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 384, .adv_w = 192, .box_w = 9, .box_h = 11, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 397, .adv_w = 193, .box_w = 10, .box_h = 11, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 411, .adv_w = 143, .box_w = 7, .box_h = 11, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 421, .adv_w = 193, .box_w = 11, .box_h = 13, .ofs_x = 1, .ofs_y = -2},
    {.bitmap_index = 439, .adv_w = 153, .box_w = 8, .box_h = 11, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 450, .adv_w = 136, .box_w = 7, .box_h = 11, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 460, .adv_w = 134, .box_w = 8, .box_h = 11, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 471, .adv_w = 176, .box_w = 8, .box_h = 11, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 482, .adv_w = 159, .box_w = 10, .box_h = 11, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 496, .adv_w = 239, .box_w = 15, .box_h = 11, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 517, .adv_w = 151, .box_w = 9, .box_h = 11, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 530, .adv_w = 142, .box_w = 9, .box_h = 11, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 543, .adv_w = 146, .box_w = 8, .box_h = 11, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 554, .adv_w = 77, .box_w = 3, .box_h = 14, .ofs_x = 2, .ofs_y = -3},
    {.bitmap_index = 560, .adv_w = 97, .box_w = 6, .box_h = 13, .ofs_x = 0, .ofs_y = -2},
    {.bitmap_index = 570, .adv_w = 77, .box_w = 3, .box_h = 14, .ofs_x = 0, .ofs_y = -3},
    {.bitmap_index = 576, .adv_w = 175, .box_w = 7, .box_h = 7, .ofs_x = 2, .ofs_y = 5},
    {.bitmap_index = 583, .adv_w = 106, .box_w = 7, .box_h = 1, .ofs_x = 0, .ofs_y = -2},
    {.bitmap_index = 584, .adv_w = 69, .box_w = 3, .box_h = 3, .ofs_x = 1, .ofs_y = 9},
    {.bitmap_index = 586, .adv_w = 130, .box_w = 6, .box_h = 8, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 592, .adv_w = 151, .box_w = 7, .box_h = 12, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 603, .adv_w = 118, .box_w = 6, .box_h = 8, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 609, .adv_w = 151, .box_w = 7, .box_h = 12, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 620, .adv_w = 134, .box_w = 7, .box_h = 8, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 627, .adv_w = 80, .box_w = 5, .box_h = 12, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 635, .adv_w = 151, .box_w = 7, .box_h = 12, .ofs_x = 1, .ofs_y = -4},
    {.bitmap_index = 646, .adv_w = 145, .box_w = 7, .box_h = 12, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 657, .adv_w = 62, .box_w = 1, .box_h = 12, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 659, .adv_w = 62, .box_w = 4, .box_h = 16, .ofs_x = -1, .ofs_y = -4},
    {.bitmap_index = 667, .adv_w = 127, .box_w = 6, .box_h = 12, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 676, .adv_w = 62, .box_w = 1, .box_h = 12, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 678, .adv_w = 221, .box_w = 11, .box_h = 8, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 689, .adv_w = 145, .box_w = 7, .box_h = 8, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 696, .adv_w = 150, .box_w = 8, .box_h = 8, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 704, .adv_w = 151, .box_w = 7, .box_h = 12, .ofs_x = 1, .ofs_y = -4},
    {.bitmap_index = 715, .adv_w = 151, .box_w = 7, .box_h = 12, .ofs_x = 1, .ofs_y = -4},
    {.bitmap_index = 726, .adv_w = 89, .box_w = 4, .box_h = 8, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 730, .adv_w = 109, .box_w = 5, .box_h = 8, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 735, .adv_w = 87, .box_w = 5, .box_h = 10, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 742, .adv_w = 145, .box_w = 7, .box_h = 8, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 749, .adv_w = 123, .box_w = 8, .box_h = 8, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 757, .adv_w = 185, .box_w = 11, .box_h = 8, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 768, .adv_w = 118, .box_w = 7, .box_h = 8, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 775, .adv_w = 124, .box_w = 7, .box_h = 12, .ofs_x = 0, .ofs_y = -4},
    {.bitmap_index = 786, .adv_w = 116, .box_w = 7, .box_h = 8, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 793, .adv_w = 77, .box_w = 3, .box_h = 13, .ofs_x = 1, .ofs_y = -2},
    {.bitmap_index = 798, .adv_w = 61, .box_w = 1, .box_h = 16, .ofs_x = 1, .ofs_y = -4},
    {.bitmap_index = 800, .adv_w = 77, .box_w = 3, .box_h = 13, .ofs_x = 1, .ofs_y = -2},
    {.bitmap_index = 805, .adv_w = 175, .box_w = 8, .box_h = 2, .ofs_x = 2, .ofs_y = 3}
};

/*---------------------
 *  CHARACTER MAPPING
 *--------------------*/



/*Collect the unicode lists and glyph_id offsets*/
static const lv_font_fmt_txt_cmap_t cmaps[] =
{
    {
        .range_start = 32, .range_length = 95, .glyph_id_start = 1,
        .unicode_list = NULL, .glyph_id_ofs_list = NULL, .list_length = 0, .type = LV_FONT_FMT_TXT_CMAP_FORMAT0_TINY
    }
};



/*--------------------
 *  ALL CUSTOM DATA
 *--------------------*/

#if LVGL_VERSION_MAJOR == 8
/*Store all the custom data of the font*/
static  lv_font_fmt_txt_glyph_cache_t cache;
#endif

#if LVGL_VERSION_MAJOR >= 8
static const lv_font_fmt_txt_dsc_t font_dsc = {
#else
static lv_font_fmt_txt_dsc_t font_dsc = {
#endif
    .glyph_bitmap = glyph_bitmap,
    .glyph_dsc = glyph_dsc,
    .cmaps = cmaps,
    .kern_dsc = NULL,
    .kern_scale = 0,
    .cmap_num = 1,
    .bpp = 1,
    .kern_classes = 0,
    .bitmap_format = 0,
#if LVGL_VERSION_MAJOR == 8
    .cache = &cache
#endif
};



/*-----------------
 *  PUBLIC FONT
 *----------------*/

/*Initialize a public general font descriptor*/
#if LVGL_VERSION_MAJOR >= 8
const lv_font_t dashboard_font_ui_16m_1bpp = {
#else
lv_font_t dashboard_font_ui_16m_1bpp = {
#endif
    .get_glyph_dsc = lv_font_get_glyph_dsc_fmt_txt,    /*Function pointer to get glyph's data*/
    .get_glyph_bitmap = lv_font_get_bitmap_fmt_txt,    /*Function pointer to get glyph's bitmap*/
    .line_height = 17,          /*The maximum line height required by the font*/
    .base_line = 4,             /*Baseline measured from the bottom of the line*/
#if !(LVGL_VERSION_MAJOR == 6 && LVGL_VERSION_MINOR == 0)
    .subpx = LV_FONT_SUBPX_NONE,
#endif
#if LV_VERSION_CHECK(7, 4, 0) || LVGL_VERSION_MAJOR >= 8
    .underline_position = -1,
    .underline_thickness = 1,
#endif
    .dsc = &font_dsc,          /*The custom font data. Will be accessed by `get_glyph_bitmap/dsc` */
#if LV_VERSION_CHECK(8, 2, 0) || LVGL_VERSION_MAJOR >= 9
    .fallback = NULL,
#endif
    .user_data = NULL,
};



#endif /*#if DASHBOARD_FONT_UI_16M_1BPP*/

