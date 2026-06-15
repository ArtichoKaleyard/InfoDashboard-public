/*******************************************************************************
 * Size: 16 px
 * Bpp: 1
 ******************************************************************************/

#ifdef LV_LVGL_H_INCLUDE_SIMPLE
#include "lvgl.h"
#else
#include "lvgl.h"
#endif

#ifndef DASHBOARD_FONT_UI_SEGOE_SEMIBOLD_16_1BPP
#define DASHBOARD_FONT_UI_SEGOE_SEMIBOLD_16_1BPP 1
#endif

#if DASHBOARD_FONT_UI_SEGOE_SEMIBOLD_16_1BPP

/*-----------------
 *    BITMAPS
 *----------------*/

/*Store the image of the glyphs*/
static LV_ATTRIBUTE_LARGE_CONST const uint8_t glyph_bitmap[] = {
    /* U+0020 " " */
    0x0,

    /* U+0021 "!" */
    0xff, 0xfc, 0x3c,

    /* U+0022 "\"" */
    0xde, 0xf6,

    /* U+0023 "#" */
    0x19, 0xd, 0x84, 0x8f, 0xf1, 0x21, 0x93, 0xfc,
    0x48, 0x24, 0x12, 0x0,

    /* U+0024 "$" */
    0x10, 0x7b, 0x46, 0x8d, 0x1e, 0xe, 0xe, 0x16,
    0x2e, 0x5f, 0xc1, 0x2, 0x0,

    /* U+0025 "%" */
    0x78, 0x8c, 0xc8, 0xcd, 0xc, 0xd0, 0x7a, 0x0,
    0x5e, 0x7, 0x30, 0xb3, 0xb, 0x31, 0x33, 0x11,
    0xe0,

    /* U+0026 "&" */
    0x3e, 0xc, 0x61, 0x8c, 0x3b, 0x83, 0xc0, 0x78,
    0xbb, 0x96, 0x3e, 0xc3, 0x9c, 0x70, 0xfb, 0x0,

    /* U+0027 "'" */
    0xfc,

    /* U+0028 "(" */
    0x32, 0x66, 0xcc, 0xcc, 0xcc, 0x66, 0x23,

    /* U+0029 ")" */
    0xc4, 0x66, 0x33, 0x33, 0x33, 0x66, 0x4c,

    /* U+002A "*" */
    0x21, 0x3e, 0xed, 0x0,

    /* U+002B "+" */
    0x18, 0x30, 0x67, 0xf1, 0x83, 0x6, 0x0,

    /* U+002C "," */
    0x69, 0x60,

    /* U+002D "-" */
    0xf0,

    /* U+002E "." */
    0xf0,

    /* U+002F "/" */
    0x6, 0x18, 0x30, 0x41, 0x83, 0xc, 0x18, 0x20,
    0xc1, 0x6, 0xc, 0x0,

    /* U+0030 "0" */
    0x3c, 0x66, 0xc3, 0xc3, 0xc3, 0xc3, 0xc3, 0xc3,
    0xc3, 0x66, 0x3c,

    /* U+0031 "1" */
    0x1b, 0xf6, 0x31, 0x8c, 0x63, 0x18, 0xc6,

    /* U+0032 "2" */
    0x7d, 0x9c, 0x18, 0x30, 0xe3, 0x8e, 0x38, 0xe1,
    0x83, 0xf8,

    /* U+0033 "3" */
    0x7c, 0xc, 0x18, 0x30, 0xc7, 0x3, 0x83, 0x7,
    0x1f, 0xe0,

    /* U+0034 "4" */
    0x6, 0x7, 0x7, 0x82, 0xc3, 0x63, 0x33, 0x19,
    0xff, 0x6, 0x3, 0x1, 0x80,

    /* U+0035 "5" */
    0x7c, 0xc1, 0x83, 0x7, 0xc1, 0xc1, 0x83, 0x7,
    0x1b, 0xe0,

    /* U+0036 "6" */
    0x1e, 0x20, 0x60, 0xc0, 0xfe, 0xe7, 0xc3, 0xc3,
    0xc3, 0x66, 0x3c,

    /* U+0037 "7" */
    0xfe, 0xc, 0x30, 0x61, 0x83, 0x4, 0x18, 0x30,
    0x60, 0x80,

    /* U+0038 "8" */
    0x7c, 0xc6, 0xc6, 0xc6, 0x3c, 0x66, 0xc3, 0xc3,
    0xc3, 0x66, 0x3c,

    /* U+0039 "9" */
    0x3c, 0x66, 0xc3, 0xc3, 0xc3, 0x67, 0x3f, 0x3,
    0x2, 0x6, 0x78,

    /* U+003A ":" */
    0xf0, 0xf,

    /* U+003B ";" */
    0x6c, 0x0, 0x1a, 0x48,

    /* U+003C "<" */
    0x6, 0x39, 0xc6, 0x7, 0x3, 0x81, 0x80,

    /* U+003D "=" */
    0xfe, 0x0, 0x7, 0xf0,

    /* U+003E ">" */
    0xc0, 0xe0, 0x70, 0x31, 0xce, 0x30, 0x0,

    /* U+003F "?" */
    0xfa, 0x30, 0xc3, 0x18, 0xe3, 0xc, 0x0, 0xc3,
    0x0,

    /* U+0040 "@" */
    0xf, 0x81, 0xc3, 0x18, 0xc, 0xcf, 0x3c, 0xc9,
    0xec, 0x4f, 0x62, 0x7b, 0x13, 0xd9, 0xb3, 0x77,
    0x18, 0x0, 0x70, 0x80, 0xfc, 0x0,

    /* U+0041 "A" */
    0xe, 0x1, 0xc0, 0x68, 0xd, 0x81, 0x30, 0x63,
    0xc, 0x63, 0xfc, 0x60, 0xcc, 0x1b, 0x3, 0x0,

    /* U+0042 "B" */
    0xfc, 0xc6, 0xc6, 0xc6, 0xcc, 0xf8, 0xc6, 0xc3,
    0xc3, 0xc6, 0xfc,

    /* U+0043 "C" */
    0x1f, 0x98, 0x58, 0x18, 0xc, 0x6, 0x3, 0x1,
    0x80, 0x60, 0x38, 0x47, 0xe0,

    /* U+0044 "D" */
    0xfc, 0x63, 0xb0, 0xd8, 0x3c, 0x1e, 0xf, 0x7,
    0x83, 0xc3, 0x63, 0x3f, 0x0,

    /* U+0045 "E" */
    0xfd, 0x83, 0x6, 0xc, 0x1f, 0xb0, 0x60, 0xc1,
    0x83, 0xf8,

    /* U+0046 "F" */
    0xff, 0xc, 0x30, 0xc3, 0xfc, 0x30, 0xc3, 0xc,
    0x0,

    /* U+0047 "G" */
    0xf, 0xcc, 0x16, 0x3, 0x0, 0xc0, 0x31, 0xfc,
    0xf, 0x3, 0x60, 0xcc, 0x31, 0xfc,

    /* U+0048 "H" */
    0xc1, 0xe0, 0xf0, 0x78, 0x3c, 0x1f, 0xff, 0x7,
    0x83, 0xc1, 0xe0, 0xf0, 0x60,

    /* U+0049 "I" */
    0xff, 0xff, 0xfc,

    /* U+004A "J" */
    0x18, 0xc6, 0x31, 0x8c, 0x63, 0x19, 0xb8,

    /* U+004B "K" */
    0xc3, 0x63, 0x33, 0x1b, 0xd, 0x87, 0x83, 0x61,
    0xb8, 0xce, 0x63, 0x30, 0xc0,

    /* U+004C "L" */
    0xc3, 0xc, 0x30, 0xc3, 0xc, 0x30, 0xc3, 0xf,
    0xc0,

    /* U+004D "M" */
    0xe0, 0x7e, 0x7, 0xf0, 0xfd, 0xf, 0xd8, 0xbd,
    0x9b, 0xc9, 0x3c, 0xf3, 0xcf, 0x3c, 0x63, 0xc6,
    0x30,

    /* U+004E "N" */
    0xe0, 0xf8, 0x3f, 0xf, 0x63, 0xd8, 0xf3, 0x3c,
    0x6f, 0x1b, 0xc3, 0xf0, 0x7c, 0x1c,

    /* U+004F "O" */
    0x1f, 0x6, 0x39, 0x83, 0x60, 0x3c, 0x7, 0x80,
    0xf0, 0x1e, 0x3, 0x60, 0xce, 0x30, 0x7c, 0x0,

    /* U+0050 "P" */
    0xf9, 0x9f, 0x1e, 0x3c, 0x79, 0xbe, 0x60, 0xc1,
    0x83, 0x0,

    /* U+0051 "Q" */
    0x1f, 0x3, 0x18, 0x60, 0xcc, 0x6, 0xc0, 0x6c,
    0x6, 0xc0, 0x6c, 0x6, 0x60, 0xc7, 0x1c, 0x1f,
    0x80, 0xe, 0x0, 0x70,

    /* U+0052 "R" */
    0xfc, 0xc6, 0xc6, 0xc6, 0xcc, 0xf8, 0xcc, 0xcc,
    0xc6, 0xc6, 0xc3,

    /* U+0053 "S" */
    0x3f, 0xc7, 0x6, 0xf, 0xf, 0x87, 0x83, 0x7,
    0xf, 0xe0,

    /* U+0054 "T" */
    0xff, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18,
    0x18, 0x18, 0x18,

    /* U+0055 "U" */
    0xc1, 0xe0, 0xf0, 0x78, 0x3c, 0x1e, 0xf, 0x7,
    0x83, 0xc1, 0xb1, 0x8f, 0x80,

    /* U+0056 "V" */
    0xc0, 0xd8, 0x36, 0x19, 0x86, 0x31, 0x8c, 0xc3,
    0x30, 0x68, 0x1e, 0x3, 0x80, 0xc0,

    /* U+0057 "W" */
    0xc1, 0x86, 0xc3, 0xd, 0x8f, 0x1b, 0x1e, 0x66,
    0x2c, 0xc6, 0xc9, 0x8d, 0x9a, 0x1a, 0x34, 0x14,
    0x78, 0x38, 0x70, 0x70, 0xc0,

    /* U+0058 "X" */
    0x61, 0x9c, 0x63, 0x30, 0x68, 0x1e, 0x3, 0x1,
    0xe0, 0xcc, 0x33, 0x18, 0x6e, 0x18,

    /* U+0059 "Y" */
    0x61, 0x98, 0x63, 0x30, 0xcc, 0x1e, 0x7, 0x80,
    0xc0, 0x30, 0xc, 0x3, 0x0, 0xc0,

    /* U+005A "Z" */
    0x7f, 0x81, 0x80, 0xc0, 0xc0, 0xe0, 0xe0, 0x60,
    0x70, 0x70, 0x30, 0x3f, 0xe0,

    /* U+005B "[" */
    0xfc, 0xcc, 0xcc, 0xcc, 0xcc, 0xcc, 0xf0,

    /* U+005C "\\" */
    0xc1, 0x81, 0x83, 0x2, 0x6, 0x4, 0xc, 0x18,
    0x10, 0x30, 0x20, 0x60,

    /* U+005D "]" */
    0xf3, 0x33, 0x33, 0x33, 0x33, 0x33, 0xf0,

    /* U+005E "^" */
    0x10, 0x60, 0xa3, 0x64, 0x58, 0xe0, 0x80,

    /* U+005F "_" */
    0xfe,

    /* U+0060 "`" */
    0x89, 0x80,

    /* U+0061 "a" */
    0x7c, 0x8c, 0x1b, 0xfe, 0x78, 0xf3, 0xbb,

    /* U+0062 "b" */
    0xc0, 0xc0, 0xc0, 0xc0, 0xde, 0xe6, 0xc3, 0xc3,
    0xc3, 0xc3, 0xe6, 0xfc,

    /* U+0063 "c" */
    0x3e, 0xc7, 0x6, 0xc, 0x18, 0x18, 0x9f,

    /* U+0064 "d" */
    0x3, 0x3, 0x3, 0x3, 0x3f, 0x67, 0xc3, 0xc3,
    0xc3, 0xc3, 0x67, 0x7b,

    /* U+0065 "e" */
    0x3c, 0xcf, 0x1f, 0xfc, 0x18, 0x19, 0x1e,

    /* U+0066 "f" */
    0x3d, 0x86, 0x18, 0xf9, 0x86, 0x18, 0x61, 0x86,
    0x18,

    /* U+0067 "g" */
    0x3f, 0x67, 0xc3, 0xc3, 0xc3, 0xc3, 0x67, 0x7b,
    0x3, 0x3, 0x46, 0x7c,

    /* U+0068 "h" */
    0xc1, 0x83, 0x6, 0xd, 0xdc, 0xf1, 0xe3, 0xc7,
    0x8f, 0x1e, 0x30,

    /* U+0069 "i" */
    0xf0, 0xff, 0xff,

    /* U+006A "j" */
    0x18, 0xc0, 0x1, 0x8c, 0x63, 0x18, 0xc6, 0x31,
    0x8c, 0xdc,

    /* U+006B "k" */
    0xc1, 0x83, 0x6, 0xc, 0x79, 0xb6, 0x78, 0xf1,
    0xb3, 0x36, 0x30,

    /* U+006C "l" */
    0xff, 0xff, 0xff,

    /* U+006D "m" */
    0xdd, 0xee, 0x73, 0xc6, 0x3c, 0x63, 0xc6, 0x3c,
    0x63, 0xc6, 0x3c, 0x63,

    /* U+006E "n" */
    0xdd, 0xcf, 0x1e, 0x3c, 0x78, 0xf1, 0xe3,

    /* U+006F "o" */
    0x3c, 0x66, 0xc3, 0xc3, 0xc3, 0xc3, 0x66, 0x3c,

    /* U+0070 "p" */
    0xde, 0xe6, 0xc3, 0xc3, 0xc3, 0xc3, 0xe6, 0xfc,
    0xc0, 0xc0, 0xc0, 0xc0,

    /* U+0071 "q" */
    0x3f, 0x67, 0xc3, 0xc3, 0xc3, 0xc3, 0x67, 0x7b,
    0x3, 0x3, 0x3, 0x3,

    /* U+0072 "r" */
    0xdf, 0x31, 0x8c, 0x63, 0x18,

    /* U+0073 "s" */
    0x7f, 0x1c, 0x3c, 0x3c, 0x30, 0xfe,

    /* U+0074 "t" */
    0x63, 0x3e, 0xc6, 0x31, 0x8c, 0x61, 0xc0,

    /* U+0075 "u" */
    0xc7, 0x8f, 0x1e, 0x3c, 0x78, 0xf3, 0xbb,

    /* U+0076 "v" */
    0xc3, 0x63, 0x66, 0x66, 0x24, 0x3c, 0x3c, 0x18,

    /* U+0077 "w" */
    0xc6, 0x34, 0x63, 0x67, 0x26, 0xf6, 0x69, 0x62,
    0x94, 0x39, 0xc3, 0x8c,

    /* U+0078 "x" */
    0x63, 0x66, 0x3c, 0x3c, 0x1c, 0x3c, 0x66, 0xe7,

    /* U+0079 "y" */
    0xc3, 0xc6, 0x46, 0x64, 0x6c, 0x2c, 0x38, 0x38,
    0x10, 0x30, 0x30, 0xe0,

    /* U+007A "z" */
    0x7e, 0x18, 0x30, 0xc3, 0x8e, 0x18, 0x7f,

    /* U+007B "{" */
    0x36, 0x66, 0x66, 0x86, 0x66, 0x66, 0x30,

    /* U+007C "|" */
    0xff, 0xff, 0xff, 0xff,

    /* U+007D "}" */
    0xc6, 0x66, 0x66, 0x16, 0x66, 0x66, 0xc0,

    /* U+007E "~" */
    0x71, 0xce
};


/*---------------------
 *  GLYPH DESCRIPTION
 *--------------------*/

static const lv_font_fmt_txt_glyph_dsc_t glyph_dsc[] = {
    {.bitmap_index = 0, .adv_w = 0, .box_w = 0, .box_h = 0, .ofs_x = 0, .ofs_y = 0} /* id = 0 reserved */,
    {.bitmap_index = 0, .adv_w = 70, .box_w = 1, .box_h = 1, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 1, .adv_w = 78, .box_w = 2, .box_h = 11, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 4, .adv_w = 112, .box_w = 5, .box_h = 3, .ofs_x = 1, .ofs_y = 8},
    {.bitmap_index = 6, .adv_w = 151, .box_w = 9, .box_h = 10, .ofs_x = 0, .ofs_y = 1},
    {.bitmap_index = 18, .adv_w = 142, .box_w = 7, .box_h = 14, .ofs_x = 1, .ofs_y = -2},
    {.bitmap_index = 31, .adv_w = 215, .box_w = 12, .box_h = 11, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 48, .adv_w = 183, .box_w = 11, .box_h = 11, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 64, .adv_w = 66, .box_w = 2, .box_h = 3, .ofs_x = 1, .ofs_y = 8},
    {.bitmap_index = 65, .adv_w = 85, .box_w = 4, .box_h = 14, .ofs_x = 1, .ofs_y = -3},
    {.bitmap_index = 72, .adv_w = 85, .box_w = 4, .box_h = 14, .ofs_x = 1, .ofs_y = -3},
    {.bitmap_index = 79, .adv_w = 111, .box_w = 5, .box_h = 6, .ofs_x = 1, .ofs_y = 5},
    {.bitmap_index = 83, .adv_w = 178, .box_w = 7, .box_h = 7, .ofs_x = 2, .ofs_y = 1},
    {.bitmap_index = 90, .adv_w = 62, .box_w = 3, .box_h = 4, .ofs_x = 0, .ofs_y = -2},
    {.bitmap_index = 92, .adv_w = 103, .box_w = 4, .box_h = 1, .ofs_x = 1, .ofs_y = 4},
    {.bitmap_index = 93, .adv_w = 62, .box_w = 2, .box_h = 2, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 94, .adv_w = 106, .box_w = 7, .box_h = 13, .ofs_x = 0, .ofs_y = -2},
    {.bitmap_index = 106, .adv_w = 142, .box_w = 8, .box_h = 11, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 117, .adv_w = 103, .box_w = 5, .box_h = 11, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 124, .adv_w = 142, .box_w = 7, .box_h = 11, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 134, .adv_w = 142, .box_w = 7, .box_h = 11, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 144, .adv_w = 148, .box_w = 9, .box_h = 11, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 157, .adv_w = 142, .box_w = 7, .box_h = 11, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 167, .adv_w = 143, .box_w = 8, .box_h = 11, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 178, .adv_w = 137, .box_w = 7, .box_h = 11, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 188, .adv_w = 142, .box_w = 8, .box_h = 11, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 199, .adv_w = 143, .box_w = 8, .box_h = 11, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 210, .adv_w = 62, .box_w = 2, .box_h = 8, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 212, .adv_w = 62, .box_w = 3, .box_h = 10, .ofs_x = 0, .ofs_y = -2},
    {.bitmap_index = 216, .adv_w = 178, .box_w = 7, .box_h = 7, .ofs_x = 2, .ofs_y = 1},
    {.bitmap_index = 223, .adv_w = 178, .box_w = 7, .box_h = 4, .ofs_x = 2, .ofs_y = 2},
    {.bitmap_index = 227, .adv_w = 178, .box_w = 7, .box_h = 7, .ofs_x = 2, .ofs_y = 1},
    {.bitmap_index = 234, .adv_w = 114, .box_w = 6, .box_h = 11, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 243, .adv_w = 244, .box_w = 13, .box_h = 13, .ofs_x = 1, .ofs_y = -2},
    {.bitmap_index = 265, .adv_w = 172, .box_w = 11, .box_h = 11, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 281, .adv_w = 155, .box_w = 8, .box_h = 11, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 292, .adv_w = 159, .box_w = 9, .box_h = 11, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 305, .adv_w = 184, .box_w = 9, .box_h = 11, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 318, .adv_w = 133, .box_w = 7, .box_h = 11, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 328, .adv_w = 129, .box_w = 6, .box_h = 11, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 337, .adv_w = 179, .box_w = 10, .box_h = 11, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 351, .adv_w = 188, .box_w = 9, .box_h = 11, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 364, .adv_w = 75, .box_w = 2, .box_h = 11, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 367, .adv_w = 102, .box_w = 5, .box_h = 11, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 374, .adv_w = 156, .box_w = 9, .box_h = 11, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 387, .adv_w = 125, .box_w = 6, .box_h = 11, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 396, .adv_w = 237, .box_w = 12, .box_h = 11, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 413, .adv_w = 196, .box_w = 10, .box_h = 11, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 427, .adv_w = 194, .box_w = 11, .box_h = 11, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 443, .adv_w = 150, .box_w = 7, .box_h = 11, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 453, .adv_w = 194, .box_w = 12, .box_h = 13, .ofs_x = 1, .ofs_y = -2},
    {.bitmap_index = 473, .adv_w = 159, .box_w = 8, .box_h = 11, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 484, .adv_w = 139, .box_w = 7, .box_h = 11, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 494, .adv_w = 141, .box_w = 8, .box_h = 11, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 505, .adv_w = 180, .box_w = 9, .box_h = 11, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 518, .adv_w = 164, .box_w = 10, .box_h = 11, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 532, .adv_w = 247, .box_w = 15, .box_h = 11, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 553, .adv_w = 159, .box_w = 10, .box_h = 11, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 567, .adv_w = 148, .box_w = 10, .box_h = 11, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 581, .adv_w = 150, .box_w = 9, .box_h = 11, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 594, .adv_w = 85, .box_w = 4, .box_h = 13, .ofs_x = 2, .ofs_y = -2},
    {.bitmap_index = 601, .adv_w = 104, .box_w = 7, .box_h = 13, .ofs_x = 0, .ofs_y = -2},
    {.bitmap_index = 613, .adv_w = 85, .box_w = 4, .box_h = 13, .ofs_x = 0, .ofs_y = -2},
    {.bitmap_index = 620, .adv_w = 178, .box_w = 7, .box_h = 7, .ofs_x = 2, .ofs_y = 5},
    {.bitmap_index = 627, .adv_w = 106, .box_w = 7, .box_h = 1, .ofs_x = 0, .ofs_y = -2},
    {.bitmap_index = 628, .adv_w = 74, .box_w = 3, .box_h = 3, .ofs_x = 1, .ofs_y = 9},
    {.bitmap_index = 630, .adv_w = 134, .box_w = 7, .box_h = 8, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 637, .adv_w = 154, .box_w = 8, .box_h = 12, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 649, .adv_w = 120, .box_w = 7, .box_h = 8, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 656, .adv_w = 154, .box_w = 8, .box_h = 12, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 668, .adv_w = 136, .box_w = 7, .box_h = 8, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 675, .adv_w = 88, .box_w = 6, .box_h = 12, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 684, .adv_w = 154, .box_w = 8, .box_h = 12, .ofs_x = 1, .ofs_y = -4},
    {.bitmap_index = 696, .adv_w = 149, .box_w = 7, .box_h = 12, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 707, .adv_w = 67, .box_w = 2, .box_h = 12, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 710, .adv_w = 67, .box_w = 5, .box_h = 16, .ofs_x = -2, .ofs_y = -4},
    {.bitmap_index = 720, .adv_w = 134, .box_w = 7, .box_h = 12, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 731, .adv_w = 67, .box_w = 2, .box_h = 12, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 734, .adv_w = 227, .box_w = 12, .box_h = 8, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 746, .adv_w = 149, .box_w = 7, .box_h = 8, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 753, .adv_w = 153, .box_w = 8, .box_h = 8, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 761, .adv_w = 154, .box_w = 8, .box_h = 12, .ofs_x = 1, .ofs_y = -4},
    {.bitmap_index = 773, .adv_w = 154, .box_w = 8, .box_h = 12, .ofs_x = 1, .ofs_y = -4},
    {.bitmap_index = 785, .adv_w = 95, .box_w = 5, .box_h = 8, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 790, .adv_w = 110, .box_w = 6, .box_h = 8, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 796, .adv_w = 93, .box_w = 5, .box_h = 10, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 803, .adv_w = 149, .box_w = 7, .box_h = 8, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 810, .adv_w = 130, .box_w = 8, .box_h = 8, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 818, .adv_w = 194, .box_w = 12, .box_h = 8, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 830, .adv_w = 128, .box_w = 8, .box_h = 8, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 838, .adv_w = 130, .box_w = 8, .box_h = 12, .ofs_x = 0, .ofs_y = -4},
    {.bitmap_index = 850, .adv_w = 119, .box_w = 7, .box_h = 8, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 857, .adv_w = 85, .box_w = 4, .box_h = 13, .ofs_x = 1, .ofs_y = -2},
    {.bitmap_index = 864, .adv_w = 71, .box_w = 2, .box_h = 16, .ofs_x = 1, .ofs_y = -4},
    {.bitmap_index = 868, .adv_w = 85, .box_w = 4, .box_h = 13, .ofs_x = 1, .ofs_y = -2},
    {.bitmap_index = 875, .adv_w = 178, .box_w = 8, .box_h = 2, .ofs_x = 2, .ofs_y = 3}
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
const lv_font_t dashboard_font_ui_segoe_semibold_16_1bpp = {
#else
lv_font_t dashboard_font_ui_segoe_semibold_16_1bpp = {
#endif
    .get_glyph_dsc = lv_font_get_glyph_dsc_fmt_txt,    /*Function pointer to get glyph's data*/
    .get_glyph_bitmap = lv_font_get_bitmap_fmt_txt,    /*Function pointer to get glyph's bitmap*/
    .line_height = 16,          /*The maximum line height required by the font*/
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



#endif /*#if DASHBOARD_FONT_UI_SEGOE_SEMIBOLD_16_1BPP*/

