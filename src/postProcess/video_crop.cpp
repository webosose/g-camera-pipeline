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
#include <unistd.h>
#include <string>
#include <cstdio>
#include <cmath>
#include <algorithm>
#include "log/log.h"
#include "video_crop.hpp"
// clang-format on

#define ZOON_STEPS 40
#define PTZ_LIMIT 60
#define ZOOM_DEPTH (0.25f)
#define PAN_TILT_SENSITIVITY_COEF 15

#define PTZ_HIST_PATH "/tmp/ptz_history"
#define PTZ_HIST_REF_PATH "/tmp/ptz_history_ref"

namespace cmp
{

VideoCrop::VideoCrop()
    : width_(0), height_(0), center_{0}, zoomLevel_(0), zoomState_(NORMAL),
      cx_pre_(0), cy_pre_(0), cx_old_(0), cy_old_(0), c_cnt_(0),
      isUpdate_(false)
{
    if (access(PTZ_HIST_REF_PATH, F_OK) == 0)
        dbgSaveHist_ = true;

    unlink(PTZ_HIST_PATH);
}

VideoCrop::~VideoCrop() {}

void VideoCrop::init(int w, int h)
{
    width_  = w;
    height_ = h;

    zoomStepSizeX_ = static_cast<float>(width_) * ZOOM_DEPTH / ZOON_STEPS;
    zoomStepSizeY_ = static_cast<float>(height_) * ZOOM_DEPTH / ZOON_STEPS;

    ptSensitivity_.x = width_ / PAN_TILT_SENSITIVITY_COEF;
    ptSensitivity_.y = height_ / PAN_TILT_SENSITIVITY_COEF;

    CMP_LOG_INFO("width = %d, height = %d, zoomStepSizeX_ = %f, zoomStepSizeY_ = "
               "%f, ptSensitivity_.x = %d, ptSensitivity_.y = %d",
               width_, height_, zoomStepSizeX_, zoomStepSizeY_,
               ptSensitivity_.x, ptSensitivity_.y);
}

void VideoCrop::readFaceInfo(int facex, int facey, int width, int height)
{
    CMP_LOG_DEBUG("%s\n", __func__);
    int cx, cy;

    cx = facex;
    cy = facey;

    if (cx < 0 || cy < 0 || cx >= width_ || cy >= height_) {
        cx = 0;
        cy = 0;
    }

    if (cx > 0) {
        if (cx < width_ / 4)
            cx = width_ / 4;
        else if (cx > width_ - width_ / 4)
            cx = width_ - width_ / 4;
    }

    if (cy > 0) {
        if (cy < height_ / 4)
            cy = height_ / 4;
        else if (cy > height_ - height_ / 4)
            cy = height_ - height_ / 4;
    }

    if (cx == cx_pre_ && cy == cy_pre_) {
        c_cnt_++;
    } else
        c_cnt_ = 0;

    cx_pre_ = cx;
    cy_pre_ = cy;

    if (c_cnt_ == 3) {
        c_cnt_ = 0;
        if (width > width_ / 2 || height > height_ / 2) {
            cx_old_ = 0;
            cy_old_ = 0;
        } else {
            int difX = std::abs(cx - cx_old_);
            int difY = std::abs(cy - cy_old_);

            if (difX > ptSensitivity_.x || difY > ptSensitivity_.y) {
                cx_old_ = cx;
                cy_old_ = cy;
            }
        }
    }

    CMP_LOG_DEBUG("%dx%d, state %d\n", cx_old_, cy_old_, zoomState_);
}

bool VideoCrop::process(CropRect &crop, int facex, int facey, int width,
                        int height)
{
    CMP_LOG_DEBUG("%s\n", __func__);

    readFaceInfo(facex, facey, width, height);

    ptzMovingParam_.pushData();

    isUpdate_ = false;

    static int sl;
    static int sx, sy;
    static int tx, ty;

    int32_t zoomMovingParam = 0;
    float *zoomCurve        = nullptr;

    zoomCurve = ptzMovingParam_.getZoomMovingParam(zoomMovingParam);

    switch (zoomState_) {
    case NORMAL:
        if (cx_old_ == 0 && cy_old_ == 0) {
            break;
        }

        center_.x = cx_old_;
        center_.y = cy_old_;

        if (zoomLevel_ < zoomMovingParam) {
            zoomLevel_++;
            cropVideo(zoomLevel_, crop, zoomCurve);
        } else {
            zoomState_ = ZOOM_IN;
            CMP_LOG_INFO("GoTo Zoom In!!!(%d)", zoomLevel_);
            if (dbgSaveHist_) {
                std::string cmd =
                    std::string("echo \"ZoomIn\" >> ") + PTZ_HIST_PATH;
                system(cmd.c_str());
            }
        }
        break;
    case ZOOM_IN:
        if (cx_old_ == 0 && cy_old_ == 0) {
            CMP_LOG_INFO("GoTo Zoom Out!!!(%d)", zoomLevel_);
            if (dbgSaveHist_) {
                std::string cmd =
                    std::string("echo \"ZoomOut\" >> ") + PTZ_HIST_PATH;
                system(cmd.c_str());
            }
            zoomState_ = ZOOM_OUT;
        } else {
            if (cx_old_ == center_.x && cy_old_ == center_.y)
                break;

            ptzMovingParam_.updatePenTiltMovingParam();
            sx = cx_old_ - center_.x;
            sy = cy_old_ - center_.y;
            tx = center_.x;
            ty = center_.y;
            sl = 0;

            if (sx > PTZ_LIMIT)
                sx = PTZ_LIMIT;
            if (sx < -PTZ_LIMIT)
                sx = -PTZ_LIMIT;

            if (sy > PTZ_LIMIT)
                sy = PTZ_LIMIT;
            if (sy < -PTZ_LIMIT)
                sy = -PTZ_LIMIT;
            CMP_LOG_INFO("PAN and Tilt!!!!");
            if (dbgSaveHist_) {
                std::string cmd =
                    std::string("echo \"PanTilt\" >> ") + PTZ_HIST_PATH;
                system(cmd.c_str());
            }
            zoomState_ = PAN_AND_TILT;
        }

        break;
    case PAN_AND_TILT:
        sl++;
        center_.x = tx + sx * sl / ptzMovingParam_.getPemTiltMovingParam();
        center_.y = ty + sy * sl / ptzMovingParam_.getPemTiltMovingParam();
        CMP_LOG_DEBUG("sl %d %d %d %d %d\n", sl, tx, ty, cx_old_, cy_old_);
        cropVideo(zoomLevel_, crop, zoomCurve);
        if (sl == ptzMovingParam_.getPemTiltMovingParam())
            zoomState_ = ZOOM_IN;
        break;
    case ZOOM_OUT:
        if (zoomLevel_ > 0) {
            zoomLevel_--;
            cropVideo(zoomLevel_, crop, zoomCurve);
        } else {
            CMP_LOG_INFO("GoTo Normal!!!");
            zoomState_ = NORMAL;
            ptzMovingParam_.updateZoomMovingParam();
        }
        break;
    }

    return isUpdate_;
}

void VideoCrop::cropVideo(int level, CropRect &crop, float *zoomCurve)
{
    int cx = center_.x;
    int cy = center_.y;

    int CropWidth, CropHeight;

    CropWidth  = width_ - zoomStepSizeX_ * zoomCurve[level];
    CropHeight = height_ - zoomStepSizeY_ * zoomCurve[level];

    int originWidth  = width_;
    int originHeight = height_;

    // 이미지를 crop 할 좌상단 좌표
    int x1 = 0;
    int y1 = 0;
    int x2, y2;

    x1 = cx - CropWidth / 2;
    y1 = cy - CropHeight / 2;

    x1 = std::min(std::max(x1, 0), originWidth - CropWidth);
    y1 = std::min(std::max(y1, 0), originHeight - CropHeight);

    CMP_LOG_DEBUG("CropWidth = %d, level = %d, x1 = %d, y1 = %d", CropWidth,
                level, x1, y1);

    x2 = x1 + CropWidth;
    y2 = y1 + CropHeight;

    int cTop    = y1;
    int cBottom = originHeight - y2;
    int cLeft   = x1;
    int cRight  = originWidth - x2;

    CMP_LOG_DEBUG("cropVideo : top=%d, bottom=%d left=%d right=%d, %dx%d\n", cTop,
                cBottom, cLeft, cRight, width_ - (cLeft + cRight),
                height_ - (cTop + cBottom));
    crop.top    = cTop;
    crop.bottom = cBottom;
    crop.left   = cLeft;
    crop.right  = cRight;

    isUpdate_ = true;
}
}
