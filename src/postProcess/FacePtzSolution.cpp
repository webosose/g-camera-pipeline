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
#include "log/log.h"
//#include "StarfishCameraConfig.hpp"
#include "FacePtzSolution.hpp"
// clang-format on

namespace cmp
{

FacePtzSolution::FacePtzSolution() {}

bool FacePtzSolution::pushMetaData(uint8_t *aMeta, int32_t aMetaLen)
{
    //CMP_LOG_INFO("%s", aMeta);
    faceDataMgr_.updateFaceInfo(aMeta, aMetaLen);
    return true;
}
bool FacePtzSolution::doPostProcess()
{
    CMP_LOG_INFO("doPostProcess");
    if (crop_ == nullptr) {
        CMP_LOG_INFO("crop_ is empty");
        return false;
    }
    faceDataMgr_.cropAndRemapFace(crop_);
    return true;
}

bool FacePtzSolution::doPostProcess(uint8_t *aData, uint32_t aStrride,
                                    uint32_t offsetUV)
{
    faceDataMgr_.drawBox(aData, fourccDecBuf_, aStrride, offsetUV);
    return true;
}

bool FacePtzSolution::doPostProcess(CropRect &cropRect)
{
    bool isRectUpdated = false;
    cropRect           = faceDataMgr_.getCropAndRemapFace(&isRectUpdated);
    return isRectUpdated;
}

bool FacePtzSolution::setParam(std::string aParamId, void *aParam)
{
    PostProcessSolution::setParam(aParamId, aParam);
    if (aParamId == PARAM_ID_CROP_OBJ) {
        GstElement *pipeline = static_cast<GstElement *>(aParam);
        crop_ = gst_bin_get_by_name(GST_BIN(pipeline), "preview-video-crop");

    } else if (aParamId == PARAM_ID_WIDTH) {
        faceDataMgr_.setWidth(*(static_cast<uint32_t *>(aParam)));
    } else if (aParamId == PARAM_ID_HEIGHT) {
        faceDataMgr_.setHeight(*(static_cast<uint32_t *>(aParam)));
    }
    return true;
}
void *FacePtzSolution::getParam(std::string aParam)
{
    return PostProcessSolution::getParam(aParam);
}

bool FacePtzSolution::needImageOverwrite()
{
    if (access(DRAW_BOX_CHECK_FILE, F_OK) == 0)
        return true;
    return false;
}
}
