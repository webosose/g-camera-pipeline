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
#include <gst/gst.h>
#include "PostProcessSolution.hpp"
#include "FaceDataManager.hpp"
// clang-format on

#define ENABLE_PTZ_FORCE "/var/luna/preferences/enable_ptz_force"

namespace cmp
{
class FacePtzSolution : public PostProcessSolution
{
public:
    FacePtzSolution();
    virtual ~FacePtzSolution() {}
    virtual bool pushMetaData(uint8_t *aMeta, int32_t aMetaLen) override;
    virtual bool doPostProcess() override;
    virtual bool doPostProcess(uint8_t *aData, uint32_t aStrride,
                               uint32_t offsetUV) override;
    virtual bool doPostProcess(CropRect &cropRect) override;
    virtual bool setParam(std::string aParamId, void *aParam) override;
    virtual void *getParam(std::string aParam) override;
    virtual bool needImageOverwrite() override;

private:
    GstElement *crop_{nullptr};
    FaceDataManager faceDataMgr_;
    std::string fourccDecBuf_;
};
}
