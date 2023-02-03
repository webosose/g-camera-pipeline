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
#include <pbnjson.hpp>
#include "log/log.h"
#include "SmoothSlidingController.hpp"
#include "FaceDataManager.hpp"
// clang-format on

#define NR_MAX_FACE_DETECT 6
#define NR_FACE_INFO_PARAM 5

#define NR_SSCTRL_SAMPLE 8

#define POS_MAX 4096

#define DEFAULT_CONFIDENCE_THRESHOLD 70

#define DEFAULT_DEBUG_FACE 0

namespace cmp
{

static uint16_t
convertToFaceInfo(uint8_t *meta, int32_t metaLen, uint16_t *facexy,
                  int32_t confidenceThreshold = DEFAULT_CONFIDENCE_THRESHOLD)
{
    pbnjson::JDomParser parser;

    uint16_t nrFace = 0;
    if (parser.parse((char *)meta)) {
        pbnjson::JValue faces = parser.getDom();
        if (faces.hasKey("faces")) {
            pbnjson::JValue face = faces["faces"];
            uint16_t nrF = std::min<uint16_t>(face.arraySize(), NR_MAX_FACE_DETECT);

            for (uint16_t f = 0; f < nrF; f++) {
                if (!face[f].hasKey("confidence") || !face[f].hasKey("x") ||
                    !face[f].hasKey("y") || !face[f].hasKey("w") ||
                    !face[f].hasKey("h"))
                    continue;

                uint16_t confidence = static_cast<uint16_t>(
                    face[f]["confidence"].asNumber<int>());

                if (confidence < confidenceThreshold) {
                    CMP_LOG_INFO("Low confidence , threshold : %d, cur = %d",
                               confidenceThreshold, confidence);
                    continue;
                }

                int x = std::max(face[f]["x"].asNumber<int>(), 0);
                int w = std::max(face[f]["w"].asNumber<int>(), 0);
                int y = std::max(face[f]["y"].asNumber<int>(), 0);
                int h = std::max(face[f]["h"].asNumber<int>(), 0);

                uint16_t l = static_cast<uint16_t>(x);
                uint16_t r = l + static_cast<uint16_t>(w);
                uint16_t t = static_cast<uint16_t>(y);
                uint16_t b = t + static_cast<uint16_t>(h);

                facexy[f * NR_FACE_INFO_PARAM]     = confidence;
                facexy[f * NR_FACE_INFO_PARAM + 1] = l;
                facexy[f * NR_FACE_INFO_PARAM + 2] = t;
                facexy[f * NR_FACE_INFO_PARAM + 3] = r - l;
                facexy[f * NR_FACE_INFO_PARAM + 4] = b - t;
                nrFace++;
            }
        }
    }
    return nrFace;
}

static uint16_t mergeFaceRect(uint16_t *data_face, uint16_t faceCount)
{
    if (faceCount < 2) {
        return faceCount;
    }
    uint16_t confidence = 0;
    uint16_t left       = POS_MAX;
    uint16_t right      = 0;
    uint16_t top        = POS_MAX;
    uint16_t bottom     = 0;

    faceCount = std::min<uint16_t>(faceCount, NR_MAX_FACE_DETECT);

    for (int32_t f = 0; f < faceCount; f++) {
        confidence += data_face[f * NR_FACE_INFO_PARAM + 0];
        uint16_t l = data_face[f * NR_FACE_INFO_PARAM + 1];
        uint16_t r = l + data_face[f * NR_FACE_INFO_PARAM + 3];
        uint16_t t = data_face[f * NR_FACE_INFO_PARAM + 2];
        uint16_t b = t + data_face[f * NR_FACE_INFO_PARAM + 4];

        left   = std::min(left, l);
        top    = std::min(top, t);
        right  = std::max(right, r);
        bottom = std::max(bottom, b);
    }
    data_face[0] = confidence / faceCount;
    data_face[1] = left;
    data_face[2] = top;
    data_face[3] = right - left;
    data_face[4] = bottom - top;

    return !!faceCount;
}

int8_t FaceDataManager::findWiderFaceIndex(std::vector<FaceData>& faceList) {
    int8_t   i = 0, widerIndex = -1;
    uint32_t widerFace = 0;
    uint32_t faceArea = 0;
    for(auto &face : faceList) {
        faceArea = face.w * face.h;
        if(faceArea > widerFace) {
            widerIndex = i;
            widerFace = faceArea;
        }
        i++;
    }
    return widerIndex;
}

FaceDataManager::FaceDataManager()
{
    ssCtrl_ = std::make_unique<SmoothSlidingController>(NR_SSCTRL_SAMPLE);
    if (access(DRAW_BOX_CHECK_FILE, F_OK) == 0)
        bDrawBox_ = true;
}

FaceDataManager::~FaceDataManager() {}

void FaceDataManager::setWidth(uint32_t aWidth)
{
    width_ = aWidth;
    if (width_ > 0 && height_ > 0)
        mVideoCrop.init(width_, height_);
}

void FaceDataManager::setHeight(uint32_t aHeight)
{
    height_ = aHeight;
    if (width_ > 0 && height_ > 0)
        mVideoCrop.init(width_, height_);
}

void FaceDataManager::updateFaceInfo(uint8_t *aMeta, int32_t aMetaLen)
{
    //CMP_LOG_INFO("(%s)", aMeta);
    uint16_t nrFace                                          = 0;
    uint16_t facexy[NR_FACE_INFO_PARAM * NR_MAX_FACE_DETECT] = {
        0,
    };

    if (aMeta) {
        nrFace = convertToFaceInfo(aMeta, aMetaLen, facexy);
#if (DEFAULT_DEBUG_FACE)
        facexy[0] = 100;
        facexy[1] = 100;
        facexy[3] = 100;
        facexy[2] = 100;
        facexy[4] = 100;
        nrFace    = 1;
#endif
        setFaceInfo(facexy, nrFace);

        //Find wider face and apply PTZ on wider face alone
        if ((access(FOCUS_WIDER_FACE_CHECK_FILE, F_OK) == 0) && (nrFace > 1))
				{
            uint16_t widerface[NR_FACE_INFO_PARAM * 1] = {
                0,
            };
            int8_t i = 0;
            int index = findWiderFaceIndex(faceList_);

            for (auto &faceInfo : faceList_)
            {
                CMP_LOG_DEBUG("Face %d [x : %d, y : %d, w : %d, h : %d]", i, faceInfo.x, faceInfo.y,
                              faceInfo.w, faceInfo.h);

                if(index == i)
                {
                    widerface[0] = 100;
                    widerface[1] = faceInfo.x;
                    widerface[2] = faceInfo.y;
                    widerface[3] = faceInfo.w;
                    widerface[4] = faceInfo.h;
                }
                i++;
            }
            memset(facexy, 0, NR_FACE_INFO_PARAM * NR_MAX_FACE_DETECT);
            facexy[0] = widerface[0];
            facexy[1] = widerface[1];
            facexy[2] = widerface[2];
            facexy[3] = widerface[3];
            facexy[4] = widerface[4];

            nrFace = 1;
            setFaceInfo(facexy, nrFace);
        }

        for (auto &faceInfo : faceList_)
        {
            CMP_LOG_DEBUG("Wider Face [x : %d, y : %d, w : %d, h : %d]", faceInfo.x, faceInfo.y,
                           faceInfo.w, faceInfo.h);

        }
        nrFace = mergeFaceRect(facexy, nrFace);
    }
    if (ssCtrl_->needFaceUpdate(faceList_.size()) == false)
        return;

    if (nrFace == 0) {
        facexy[0] = 100;
        facexy[1] = 0;
        facexy[3] = static_cast<uint16_t>(width_);

        facexy[2] = 0;
        facexy[4] = static_cast<uint16_t>(height_);
        nrFace    = 1;
    }
    setMergedFaceInfo(facexy, nrFace);
}

void FaceDataManager::cropAndRemapFace(GstElement *crop)
{
    CMP_LOG_INFO("cropAndRemapFace");
    static CropRect crop_rect_prev{0, 0, 0, 0};
    CropRect crop_rect{0, 0, 0, 0};
    FaceXY mergedFace{0, 0, 0, 0};

    mergedFace.x = meregedFace_.x;
    mergedFace.y = meregedFace_.y;
    mergedFace.w = meregedFace_.w;
    mergedFace.h = meregedFace_.h;

    bool crop_update = cropAroundFace(mergedFace, crop_rect);

    if (crop_update) {
        CMP_LOG_INFO("[CROP] crop property set");
        g_object_set(crop, "top", crop_rect.top, "right", crop_rect.right,
                     "left", crop_rect.left, "bottom", crop_rect.bottom, NULL);
        crop_rect_prev = crop_rect;
    }
    remapFaceList_.clear();

    for (auto &f : faceList_) {
        FaceXY remapped_facexy;
        FaceXY fxy{0, 0, 0, 0};
        fxy.x = f.x;
        fxy.y = f.y;
        fxy.w = f.w;
        fxy.h = f.h;

        remapped_facexy = remapFaceCoordinates(crop_rect_prev, fxy);
        if (remapped_facexy.w > 0 && remapped_facexy.h > 0)
            remapFaceList_.push_back({remapped_facexy.x, remapped_facexy.y,
                                      remapped_facexy.w, remapped_facexy.h,
                                      100});
    }
}

CropRect FaceDataManager::getCropAndRemapFace(bool *isRectUpdated)
{
    static CropRect crop_rect_prev{0, 0, 0, 0};
    CropRect crop_rect{0, 0, 0, 0};
    FaceXY mergedFace{0, 0, 0, 0};

    mergedFace.x = meregedFace_.x;
    mergedFace.y = meregedFace_.y;
    mergedFace.w = meregedFace_.w;
    mergedFace.h = meregedFace_.h;

    *isRectUpdated = cropAroundFace(mergedFace, crop_rect);

    if (*isRectUpdated) {
        CMP_LOG_DEBUG("[CROP] crop property set");
        crop_rect_prev = crop_rect;
    }
    remapFaceList_.clear();

    for (auto &f : faceList_) {
        FaceXY remapped_facexy;
        FaceXY fxy{0, 0, 0, 0};
        fxy.x = f.x;
        fxy.y = f.y;
        fxy.w = f.w;
        fxy.h = f.h;

        remapped_facexy = remapFaceCoordinates(crop_rect_prev, fxy);
        if (remapped_facexy.w > 0 && remapped_facexy.h > 0)
            remapFaceList_.push_back({remapped_facexy.x, remapped_facexy.y,
                                      remapped_facexy.w, remapped_facexy.h,
                                      100});
    }
    return crop_rect;
}

void FaceDataManager::drawBox(uint8_t *data, std::string cs, uint32_t aStride,
                              uint32_t offsetUV)
{
    if (bDrawBox_ == false)
        return;

    std::vector<FaceData> &facelist =
        (cs == "NV16") ? faceList_ : remapFaceList_;

    for (auto &faceInfo : facelist) {
        FaceXY face;
        face.x = faceInfo.x;
        face.y = faceInfo.y;
        face.w = faceInfo.w;
        face.h = faceInfo.h;

        if (cs == "NV12") {
            drawBoxInNV12Format(data, aStride, height_, face, offsetUV,
                                DRAW_BOX_GREEN);
        } else if (cs == "NV16") {
            drawBoxInNV16Format(data, aStride, height_, face, offsetUV,
                                DRAW_BOX_RED);
        }
    }
}

bool FaceDataManager::cropAroundFace(const FaceXY face, CropRect &crop_rect)
{
    CMP_LOG_DEBUG("[CROP] crop process");
    CMP_LOG_DEBUG("[FaceInfo] face x : %d, y : %d, w : %d, h : %d", face.x,
                face.y, face.w, face.h);

    // find center coordinate to initite crop processing
    uint32_t cx = face.x + face.w / 2;
    uint32_t cy = face.y + face.h / 2;

    bool crop_update = mVideoCrop.process(crop_rect, cx, cy, face.w, face.h);

    CMP_LOG_DEBUG("[FaceInfo] crop update : %d, crop value is crop_rect.top %d, "
                "crop_rect.right %d,"
                " crop_rect.left %d, crop_rect.bottom %d",
                crop_update, crop_rect.top, crop_rect.right, crop_rect.left,
                crop_rect.bottom);
    return crop_update;
}

FaceXY FaceDataManager::remapFaceCoordinates(const CropRect crop_rect,
                                             FaceXY face)
{
    FaceXY remapped_face{0, 0, 0, 0};
    if (face.w == 0 || face.h == 0)
        return remapped_face;

    float widthRatio =
        width_ / (float)(width_ - (crop_rect.left + crop_rect.right));
    float heightRatio =
        height_ / (float)(height_ - (crop_rect.top + crop_rect.bottom));

    remapped_face.x = (face.x - crop_rect.left) * widthRatio;
    remapped_face.w = face.w * widthRatio;

    remapped_face.y = (face.y - crop_rect.top) * heightRatio;
    remapped_face.h = face.h * heightRatio;

    return remapped_face;
}

void FaceDataManager::setFaceInfo(uint16_t *aFaceXY, const uint8_t aFaceCount)
{
    // remove all previous face info before getting new list
    faceList_.clear();

    if (aFaceXY == nullptr)
        return;

    for (uint8_t i = 0; i < aFaceCount; i++) {
        FaceData faceData;
        faceData.reserved   = 0;
        faceData.confidence = aFaceXY[i * 5 + 0];
        faceData.x          = aFaceXY[i * 5 + 1];
        faceData.y          = aFaceXY[i * 5 + 2];
        faceData.w          = aFaceXY[i * 5 + 3];
        faceData.h          = aFaceXY[i * 5 + 4];
        faceList_.push_back(faceData);
    }
}

void FaceDataManager::setMergedFaceInfo(uint16_t *aFaceXY,
                                        const uint8_t aFaceCount)
{
    if (aFaceXY == nullptr)
        return;

    ssCtrl_->pushDataFace(aFaceXY);
    ssCtrl_->getAvgDataFace(aFaceXY);

    meregedFace_.reserved   = 0;
    meregedFace_.confidence = aFaceXY[0];
    meregedFace_.x          = aFaceXY[1];
    meregedFace_.y          = aFaceXY[2];
    meregedFace_.w          = aFaceXY[3];
    meregedFace_.h          = aFaceXY[4];
}
}
