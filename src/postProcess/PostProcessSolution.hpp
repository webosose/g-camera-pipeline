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

#define PARAM_ID_VDEC_PORT "vdec_port"
#define PARAM_ID_WIDTH "width"
#define PARAM_ID_HEIGHT "height"
#define PARAM_ID_CROP_OBJ "crop_obj"

#define PROPERTY_ID_PLATFORM "platform"
#define PROPERTY_ID_FOURCC_FOR_DECBUF "fourcc_decbuf"

namespace cmp
{
class PostProcessSolution : public IPostProcessSolution
{
public:
    virtual ~PostProcessSolution() {}
    virtual bool pushMetaData(uint8_t *aMeta, int32_t aMetaLen) override;
    virtual bool doPostProcess() override;
    virtual bool doPostProcess(uint8_t *aData, uint32_t aStrride,
                               uint32_t offsetUV) override;
    virtual bool doPostProcess(CropRect &cropRect) override;
    virtual bool setParam(std::string aParamId, void *aParam) override;
    virtual void *getParam(std::string aParam) override;
    virtual bool needImageOverwrite() override;

protected:
    int32_t vdecPort_{-1};
    uint32_t width_{0};
    uint32_t height_{0};
    std::string platform_;
};
}
