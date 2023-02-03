// Copyright (c) 2021-2023 LG Electronics, Inc.
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

#include "FpsCalc.hpp"

#define LIMIT_QUEUE_SIZE 120

namespace cmp
{

void FpsCalc::pushData()
{
    if (timeQueue_.size() > LIMIT_QUEUE_SIZE)
        timeQueue_.pop();
    timeQueue_.push(std::chrono::steady_clock::now());
}

double FpsCalc::getCurrentFps()
{
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
                        timeQueue_.back() - timeQueue_.front())
                        .count();
    double fps = timeQueue_.size() * 1000 / (double)duration;
    return fps;
}
}