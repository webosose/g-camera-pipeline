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

#include "IPostProcessSolution.hpp"
#include "PTZMovingParam.hpp"

namespace cmp
{

struct Point {
    int x, y;
};

typedef enum {
    NORMAL,
    ZOOM_IN,
    PAN_AND_TILT,
    ZOOM_OUT,
} ZOOM_STATUS_T;

class VideoCrop
{
    int width_{0}, height_{0};
    Point center_{0, 0};
    int zoomLevel_{0};
    int zoomState_{0};
    int cx_pre_{0}, cy_pre_{0}, cx_old_{0}, cy_old_{0}, c_cnt_{0};
    bool isUpdate_{false};

    float zoomStepSizeX_{.0f};
    float zoomStepSizeY_{.0f};

    Point ptSensitivity_{0, 0};

    void cropVideo(int level, CropRect &crop, float *zoomCurve);

public:
    VideoCrop();
    ~VideoCrop();

    void init(int w, int h);
    void readFaceInfo(int x, int y, int width, int height);
    bool process(CropRect &crop, int facex, int facey, int width, int height);

private:
    PTZMovingParam ptzMovingParam_;
    bool dbgSaveHist_{false};
};
}
