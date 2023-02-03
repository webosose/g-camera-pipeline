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

// clang-format off
#include "log/log.h"
#include "PTZMovingParam.hpp"
// clang-format on

#define PENTILT_RATE_STEP1 10
#define PENTILT_RATE_STEP2 5
#define PENTILT_RATE_STEP3 3

#define DEFAULT_ZOOM_RATE ZOOM_RATE_STEP1
#define DEFAULT_PENTILT_RATE PENTILT_RATE_STEP1

namespace cmp
{

PTZMovingParam::PTZMovingParam()
{
    zoomMovingParam_    = DEFAULT_ZOOM_RATE;
    penTiltMovingParam_ = DEFAULT_PENTILT_RATE;
}

void PTZMovingParam::pushData() { fpsCalc_.pushData(); }

float *PTZMovingParam::getZoomMovingParam(int32_t &zoomMovingParam)
{
    zoomMovingParam = zoomMovingParam_;
    if (zoomMovingParam_ == ZOOM_RATE_STEP1)
        return zoomCurve1_;
    else if (zoomMovingParam_ == ZOOM_RATE_STEP2)
        return zoomCurve2_;
    else
        return zoomCurve3_;
}
int32_t PTZMovingParam::getPemTiltMovingParam() { return penTiltMovingParam_; }

bool PTZMovingParam::updateZoomMovingParam()
{
    double fps = getCurrentFps();
    if (fps >= 20.0)
        zoomMovingParam_ = ZOOM_RATE_STEP1;
    else if (fps >= 10.0)
        zoomMovingParam_ = ZOOM_RATE_STEP2;
    else
        zoomMovingParam_ = ZOOM_RATE_STEP3;
    return true;
}

bool PTZMovingParam::updatePenTiltMovingParam()
{
    double fps = getCurrentFps();
    if (fps >= 20.0)
        penTiltMovingParam_ = PENTILT_RATE_STEP1;
    else if (fps >= 10.0)
        penTiltMovingParam_ = PENTILT_RATE_STEP2;
    else
        penTiltMovingParam_ = PENTILT_RATE_STEP3;
    return true;
}

double PTZMovingParam::getCurrentFps() { return fpsCalc_.getCurrentFps(); }
}
