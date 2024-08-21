// Copyright (c) 2019-2024 LG Electronics, Inc.
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

#ifndef SRC_CAMERA_PIPELINE_SERVICE_H_
#define SRC_CAMERA_PIPELINE_SERVICE_H_

#include "luna-service2/lunaservice.hpp"
#include <glib.h>

class CameraPipeline;

namespace cmp
{
namespace player
{
class CameraPlayer;
}
} // namespace cmp
namespace cmp
{
namespace base
{
struct source_info_t;
}
} // namespace cmp
namespace cmp
{
namespace resource
{
class ResourceRequestor;
}
} // namespace cmp

namespace cmp
{
namespace service
{

class CameraPipelineService : public LS::Handle
{
    using mainloop          = std::unique_ptr<GMainLoop, void (*)(GMainLoop *)>;
    mainloop main_loop_ptr_ = {g_main_loop_new(nullptr, false), g_main_loop_unref};

public:
    void Notify(const gint notification, const gint32 numValue, const gchar *strValue,
                void *payload = nullptr);

    CameraPipelineService(const char *service_name);

    CameraPipelineService(CameraPipelineService const &)            = delete;
    CameraPipelineService(CameraPipelineService &&)                 = delete;
    CameraPipelineService &operator=(CameraPipelineService const &) = delete;
    CameraPipelineService &operator=(CameraPipelineService &&)      = delete;

    bool start(LSMessage &message);
    bool stop(LSMessage &message);

private:
    void LoadCommon();
    bool AcquireResources(const base::source_info_t &sourceInfo,
                          const std::string &display_mode = "Default",
                          const int32_t display_path      = 0);

    std::string media_id_; // connection_id
    std::string app_id_;
    std::shared_ptr<CameraPipeline> player_;
    std::unique_ptr<resource::ResourceRequestor> resourceRequestor_;
    bool isLoaded_ = false;
};

} // namespace service
} // namespace cmp

#endif // SRC_CAMERA_PIPELINE_SERVICE_H_
