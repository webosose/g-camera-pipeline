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

// clang-format off
#include <queue>
#include <cstdint>
#include "video_crop.hpp"
#include "FpsCalc.hpp"
// clang-format on

namespace cmp
{

class SmoothSlidingController
{
    uint32_t nrQLimit_{0};
    CropRect rectSum{0, 0, 0, 0};
    std::queue<CropRect> rectList;

    FpsCalc fpsCalc_;

    struct faceUpdateCalcParam {
        const int8_t defaultLeap_{6};
        const int8_t defaultLeapHalf_{3};
        int8_t count_{0};
        int32_t leap_{defaultLeap_};

        const int8_t faceDecisionThreshold_{10};

        int8_t selectedFaceCount_{0};
        int8_t prevFaceCount_{0};
        int8_t faceIdenticalCount_{0};
    };
    faceUpdateCalcParam fParam_;

public:
    SmoothSlidingController(int32_t nrQLimit) : nrQLimit_(nrQLimit) {}
    void pushRect(CropRect &newRect);
    CropRect getAvgRect();
    void pushDataFace(unsigned short *data_face);
    void getAvgDataFace(unsigned short *data_face);

    bool needFaceUpdate(int8_t aCurFaceCount);

private:
    bool isFaceInfoValidate(int8_t aCurFaceCount);
};
}
