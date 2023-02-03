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
#include <cstdint>
#include <memory>
#include <vector>
#include <gst/gst.h>

#include "video_crop.hpp"
#include "drawbox.hpp"
// clang-format on

#define DRAW_BOX_CHECK_FILE "/var/luna/preferences/enable_draw_face_rect"
#define FOCUS_WIDER_FACE_CHECK_FILE "/var/luna/preferences/enable_wider_face_focus"

namespace cmp
{
class SmoothSlidingController;

struct FaceData {
    uint16_t x{0};
    uint16_t y{0};
    uint16_t w{0};
    uint16_t h{0};
    uint16_t confidence{0};
    uint16_t reserved{0};
};

class FaceDataManager
{
public:
    FaceDataManager();
    ~FaceDataManager();
    void setWidth(uint32_t aWidth);
    void setHeight(uint32_t aHeight);
    void updateFaceInfo(uint8_t *aMeta, int32_t aMetaLen);
    void cropAndRemapFace(GstElement *crop);
    void drawBox(uint8_t *data, std::string cs, uint32_t aStrride,
                 uint32_t offsetUV);
    CropRect getCropAndRemapFace(bool *isRectUpdated);
    static int8_t findWiderFaceIndex(std::vector<FaceData>& faceList);

private:
    void setFaceInfo(uint16_t *faceData, const uint8_t faceCount);
    void setMergedFaceInfo(uint16_t *faceData, const uint8_t faceCount);

    FaceXY remapFaceCoordinates(const CropRect crop_rect, FaceXY face);
    bool cropAroundFace(const FaceXY face, CropRect &crop_rect);

    uint32_t width_{0};
    uint32_t height_{0};

    std::vector<FaceData> faceList_;
    std::vector<FaceData> remapFaceList_;

    FaceData meregedFace_;
    FaceData remapMeregedFace_;
    std::unique_ptr<SmoothSlidingController> ssCtrl_;

    VideoCrop mVideoCrop;

    bool bDrawBox_{false};
};
}
