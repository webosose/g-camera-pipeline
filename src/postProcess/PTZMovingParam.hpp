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

#include "FpsCalc.hpp"

#define ZOOM_RATE_STEP1 40
#define ZOOM_RATE_STEP2 20
#define ZOOM_RATE_STEP3 10

namespace cmp
{

class PTZMovingParam
{
    FpsCalc fpsCalc_;

public:
    PTZMovingParam();
    void pushData();
    float *getZoomMovingParam(int32_t &zoomMovingParam);
    int32_t getPemTiltMovingParam();

    bool updateZoomMovingParam();
    bool updatePenTiltMovingParam();

private:
    int32_t zoomMovingParam_;
    int32_t penTiltMovingParam_;
    double getCurrentFps();

    float zoomCurve1_[ZOOM_RATE_STEP1 + 1] = {
        0.00,  0.26,  0.56,  0.90,  1.31,  1.78,  2.32,  2.93,  3.64,
        4.44,  5.34,  6.36,  7.48,  8.72,  10.07, 11.53, 13.09, 14.74,
        16.45, 18.21, 20.00, 21.79, 23.55, 25.26, 26.91, 28.47, 29.93,
        31.28, 32.52, 33.64, 34.66, 35.56, 36.36, 37.07, 37.68, 38.22,
        38.69, 39.10, 39.44, 39.74, 40.00};

    float zoomCurve2_[ZOOM_RATE_STEP2 + 1] = {
        0.00,  0.56,  1.31,  2.32,  3.64,  5.34,  7.48,
        10.07, 13.09, 16.45, 20.00, 23.55, 26.91, 29.93,
        32.52, 34.66, 36.36, 37.68, 38.69, 39.44, 40.00};

    float zoomCurve3_[ZOOM_RATE_STEP3 + 1] = {0.00,  1.31,  3.64,  7.48,
                                              13.09, 20.00, 26.91, 32.52,
                                              36.36, 38.69, 40.00};
};
}