// Copyright (c) 2023 LG Electronics, Inc.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
// http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//
// SPDX-License-Identifier: Apache-2.0

#include "drawbox.hpp"

#define NV12_VCOLOR(c) (uint16_t)(c << 8)
#define NV12_UCOLOR(c) (uint16_t)(c)

#define NV12_COLOR_WHITE (NV12_UCOLOR(0x80) | NV12_VCOLOR(0x80))
#define NV12_COLOR_BLACK (NV12_UCOLOR(0x80) | NV12_VCOLOR(0x80))
#define NV12_COLOR_MAGENTA (NV12_UCOLOR(0xCA) | NV12_VCOLOR(0xDE))
#define NV12_COLOR_CYAN (NV12_UCOLOR(0xA6) | NV12_VCOLOR(0x10))
#define NV12_COLOR_YELLOW (NV12_UCOLOR(0x00) | NV12_VCOLOR(0x94))
#define NV12_COLOR_RED (NV12_UCOLOR(0x54) | NV12_VCOLOR(0xFF))
#define NV12_COLOR_GREEN (NV12_UCOLOR(0x2B) | NV12_VCOLOR(0x15))
#define NV12_COLOR_BLUE (NV12_UCOLOR(0xFF) | NV12_VCOLOR(0x6B))
#define NV12_COLOR_ORANGE (NV12_UCOLOR(0x1E) | NV12_VCOLOR(0xBA))

#define NV12_LUM_WHITE 0xFF
#define NV12_LUM_BLACK 0x00
#define NV12_LUM_MAGENTA 0x6A
#define NV12_LUM_CYAN 0xAA
#define NV12_LUM_BLUE 0x1D
#define NV12_LUM_YELLOW 0xE1
#define NV12_LUM_ORANGE 0xAD
#define NV12_LUM_GREEN 0x95
#define NV12_LUM_RED 0x4C

typedef struct {
    uint8_t reserved;
    uint8_t lumin_val;
    uint16_t color_val;
} NV12ColorScale;

static NV12ColorScale getColor(const DRAW_BOX_COLOR color)
{
    NV12ColorScale c{0, 0, 0};
    switch (color) {
    case DRAW_BOX_WHITE:
        c.lumin_val = NV12_LUM_WHITE;
        c.color_val = NV12_COLOR_WHITE;
        break;
    case DRAW_BOX_BLACK:
        c.lumin_val = NV12_LUM_BLACK;
        c.color_val = NV12_COLOR_BLACK;
        break;
    case DRAW_BOX_MAGENTA:
        c.lumin_val = NV12_LUM_MAGENTA;
        c.color_val = NV12_COLOR_MAGENTA;
        break;
    case DRAW_BOX_CYAN:
        c.lumin_val = NV12_LUM_CYAN;
        c.color_val = NV12_COLOR_CYAN;
        break;
    case DRAW_BOX_BLUE:
        c.lumin_val = NV12_LUM_BLUE;
        c.color_val = NV12_COLOR_BLUE;
        break;
    case DRAW_BOX_YELLOW:
        c.lumin_val = NV12_LUM_YELLOW;
        c.color_val = NV12_COLOR_YELLOW;
        break;
    case DRAW_BOX_ORANGE:
        c.lumin_val = NV12_LUM_ORANGE;
        c.color_val = NV12_COLOR_ORANGE;
        break;
    case DRAW_BOX_RED:
        c.lumin_val = NV12_LUM_RED;
        c.color_val = NV12_COLOR_RED;
        break;
    case DRAW_BOX_GREEN:
    default:
        c.lumin_val = NV12_LUM_GREEN;
        c.color_val = NV12_COLOR_GREEN;
        break;
    }
    return c;
}

void drawBoxInNV12Format(uint8_t *data, const int &maxWidth,
                         const int &maxHeight, const FaceXY &face,
                         uint32_t offsetUV, const DRAW_BOX_COLOR color)
{
    uint32_t clr_pos = 0, lum_pos = 0;
    uint8_t *lumin  = data;
    uint16_t *tuple = reinterpret_cast<uint16_t *>(data + offsetUV);

    uint16_t x1 = face.x, x2 = face.x + face.w;
    uint16_t y1 = face.y, y2 = face.y + face.h;

    x1 = (x1 > maxWidth) ? maxWidth : x1;
    x2 = (x2 > maxWidth) ? maxWidth : x2;
    y1 = (y1 > maxHeight) ? maxHeight : y1;
    y2 = (y2 > maxHeight) ? maxHeight : y2;

    NV12ColorScale clrscale = getColor(color);

    // each 2 row of pixels will be in 1 row of UV data
    for (uint16_t y = y1; y < y2; y += 1) {
        lum_pos = y * maxWidth;
        clr_pos = y / 2 * maxWidth / 2;
        for (uint16_t x = x1; x < x2; x += 1) {
            // skip pixels which are inside the box as we need outliner box
            if ((x1 + 2 <= x && x < x2 - 2) && (y1 + 2 <= y && y < y2 - 2))
                continue;

            lumin[lum_pos + x] = clrscale.lumin_val;

            // color tuples are for 2 rows of pixels so alternative rows are
            // enough to consider
            if (y % 2 != 0 || x % 2 != 0)
                continue;

            tuple[clr_pos + (x / 2)] = clrscale.color_val;
        }
    }
}

void drawBoxInNV16Format(uint8_t *data, const int &maxWidth,
                         const int &maxHeight, const FaceXY &face,
                         uint32_t offsetUV, const DRAW_BOX_COLOR color)
{
    uint32_t clr_pos = 0, lum_pos = 0;
    uint8_t *lumin  = data;
    uint16_t *tuple = reinterpret_cast<uint16_t *>(data + offsetUV);

    uint16_t x1 = face.x, x2 = face.x + face.w;
    uint16_t y1 = face.y, y2 = face.y + face.h;

    x1 = (x1 > maxWidth) ? maxWidth : x1;
    x2 = (x2 > maxWidth) ? maxWidth : x2;
    y1 = (y1 > maxHeight) ? maxHeight : y1;
    y2 = (y2 > maxHeight) ? maxHeight : y2;

    NV12ColorScale clrscale = getColor(color);

    // Rows of UV in NV16 is same as Y
    for (uint16_t y = y1; y < y2; y += 1) {
        lum_pos = y * maxWidth;
        clr_pos = y * maxWidth / 2;
        for (uint16_t x = x1; x < x2; x += 1) {
            // skip pixels which are inside the box as we need outliner box
            if ((x1 + 2 <= x && x < x2 - 2) && (y1 + 2 <= y && y < y2 - 2))
                continue;
            lumin[lum_pos + x] = clrscale.lumin_val;
            // color tuples are for 1 rows of pixels so alternative rows are
            // enough to consider
            if (x % 2 != 0)
                continue;
            tuple[clr_pos + (x / 2)] = clrscale.color_val;
        }
    }
}
