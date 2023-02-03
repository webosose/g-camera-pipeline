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
#include <iostream>
#include "log.h"
#include "SmoothSlidingController.hpp"
// clang-format on

namespace cmp
{

void SmoothSlidingController::pushDataFace(unsigned short *data_face)
{
    auto &l = data_face[1];
    auto &t = data_face[2];
    auto &w = data_face[3];
    auto &h = data_face[4];

    CropRect rect{t, t + h, l, l + w};
    pushRect(rect);
}

void SmoothSlidingController::getAvgDataFace(unsigned short *data_face)
{
    CropRect rect = getAvgRect();
    data_face[1]  = rect.left;
    data_face[2]  = rect.top;
    data_face[3]  = rect.right - rect.left;
    data_face[4]  = rect.bottom - rect.top;
}
void SmoothSlidingController::pushRect(CropRect &newRect)
{
    if (rectList.size() > nrQLimit_) {
        auto tmp = rectList.front();
        rectList.pop();
        rectList.push(newRect);

        rectSum.top += newRect.top - tmp.top;
        rectSum.bottom += newRect.bottom - tmp.bottom;
        rectSum.left += newRect.left - tmp.left;
        rectSum.right += newRect.right - tmp.right;
    } else {
        rectList.push(newRect);
        rectSum.top += newRect.top;
        rectSum.bottom += newRect.bottom;
        rectSum.left += newRect.left;
        rectSum.right += newRect.right;
    }
}
CropRect SmoothSlidingController::getAvgRect()
{
    CropRect result;
    auto szRectList = rectList.size();
    result.top      = rectSum.top / szRectList;
    result.bottom   = rectSum.bottom / szRectList;
    result.left     = rectSum.left / szRectList;
    result.right    = rectSum.right / szRectList;
    return result;
}

bool SmoothSlidingController::needFaceUpdate(int8_t aCurFaceCount)
{
    fpsCalc_.pushData();
    if (++fParam_.count_ % fParam_.leap_ == 0) {
        fParam_.count_ = 0;
        auto curFps    = fpsCalc_.getCurrentFps();
        if (curFps > 15.0f)
            fParam_.leap_ = fParam_.defaultLeap_;
        else
            fParam_.leap_ = fParam_.defaultLeapHalf_;

        return isFaceInfoValidate(aCurFaceCount);
    }
    return false;
}

bool SmoothSlidingController::isFaceInfoValidate(int8_t aCurFaceCount)
{
    //CMP_LOG_INFO("(%d)", aCurFaceCount);
    if (fParam_.selectedFaceCount_ == aCurFaceCount) {
        fParam_.faceIdenticalCount_ = 0;
        return true;
    }

    bool result = false;

    if (fParam_.prevFaceCount_ != aCurFaceCount) {
        fParam_.prevFaceCount_      = aCurFaceCount;
        fParam_.faceIdenticalCount_ = 0;
    } else {
        CMP_LOG_INFO("sushant fParam_.faceIdenticalCount_ = %d(%d) faceDecisionThreshold_ = %d\n",
                   fParam_.faceIdenticalCount_, aCurFaceCount, fParam_.faceDecisionThreshold_);
        if (++fParam_.faceIdenticalCount_ == fParam_.faceDecisionThreshold_) {
            fParam_.faceIdenticalCount_ = 0;
            fParam_.selectedFaceCount_  = aCurFaceCount;
            result                      = true;
        }
    }
    return result;
}
}
