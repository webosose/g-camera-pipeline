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

#include <cstdint>
#include <string>

struct CropRect {
    int top{0}, bottom{0}, left{0}, right{0};
};

namespace cmp
{
class IPostProcessSolution
{
public:
    virtual ~IPostProcessSolution() {}
    virtual bool pushMetaData(uint8_t *aMeta, int32_t aMetaLen) = 0;
    virtual bool doPostProcess() = 0;
    virtual bool doPostProcess(uint8_t *aData, uint32_t aStrride,
                               uint32_t offsetUV)  = 0;
    virtual bool doPostProcess(CropRect &cropRect) = 0;
    virtual bool setParam(std::string aParamId, void *aParam) = 0;
    virtual void *getParam(std::string aParam) = 0;
    virtual bool needImageOverwrite()                        = 0;
};
}
