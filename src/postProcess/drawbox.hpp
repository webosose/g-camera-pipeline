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

#pragma once

#include <cstdint>

typedef enum {
    DRAW_BOX_WHITE,
    DRAW_BOX_BLACK,
    DRAW_BOX_MAGENTA,
    DRAW_BOX_BLUE,
    DRAW_BOX_CYAN,
    DRAW_BOX_GREEN,
    DRAW_BOX_YELLOW,
    DRAW_BOX_ORANGE,
    DRAW_BOX_RED
} DRAW_BOX_COLOR;

struct FaceXY {
    uint16_t x{0};
    uint16_t y{0};
    uint16_t w{0};
    uint16_t h{0};
};

void drawBoxInNV12Format(uint8_t *data, const int &maxWidth,
                         const int &maxHeight, const FaceXY &face,
                         uint32_t offsetUV,
                         const DRAW_BOX_COLOR color = DRAW_BOX_GREEN);
void drawBoxInNV16Format(uint8_t *data, const int &maxWidth,
                         const int &maxHeight, const FaceXY &face,
                         uint32_t offsetUV,
                         const DRAW_BOX_COLOR color = DRAW_BOX_GREEN);
