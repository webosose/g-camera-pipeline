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

#include "PostProcessSolution.hpp"

namespace cmp
{

bool PostProcessSolution::pushMetaData(uint8_t *aMeta, int32_t aMetaLen)
{
    return true;
}
bool PostProcessSolution::doPostProcess() { return true; }
bool PostProcessSolution::doPostProcess(uint8_t *aData, uint32_t aStrride,
                                        uint32_t offsetUV)
{
    return true;
}
bool PostProcessSolution::doPostProcess(CropRect &cropRect) { return true; }
bool PostProcessSolution::setParam(std::string aParamId, void *aParam)
{
    if (aParam == nullptr)
        return false;

    if (aParamId == PARAM_ID_VDEC_PORT) {
        vdecPort_ = *(static_cast<int32_t *>(aParam));
    } else if (aParamId == PARAM_ID_WIDTH) {
        width_ = *(static_cast<uint32_t *>(aParam));
    } else if (aParamId == PARAM_ID_HEIGHT) {
        height_ = *(static_cast<uint32_t *>(aParam));
    }
    return true;
}
void *PostProcessSolution::getParam(std::string aParam) { return nullptr; }

bool PostProcessSolution::needImageOverwrite() { return false; }
}
