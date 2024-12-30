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

#include "camera_pipeline_service.h"
#include "base.h"
#include "camera_player.h"
#include "log/log.h"
#include "parser/parser.h"
#include "parser/serializer.h"
#include "pipeline_factory.h"

namespace cmp
{
namespace service
{

CameraPipelineService::CameraPipelineService(const char *service_name)
    : LS::Handle(LS::registerService(service_name))
{
    CMP_LOG_INFO("Start : %s", service_name);

    LS_CATEGORY_BEGIN(CameraPipelineService, "/")
    LS_CATEGORY_METHOD(start)
    LS_CATEGORY_METHOD(stop)
    LS_CATEGORY_END;

    // attach to mainloop and run it
    attachToLoop(main_loop_ptr_.get());

    // run the gmainloop
    g_main_loop_run(main_loop_ptr_.get());

    CMP_LOG_INFO("end");
}

void CameraPipelineService::Notify(const gint notification, const gint32 numValue,
                                   const gchar *strValue, void *payload)
{
    cmp::parser::Composer composer;
    cmp::base::media_info_t mediaInfo = {media_id_};
    switch (notification)
    {
    case CMP_NOTIFY_SOURCE_INFO:
    {
        base::source_info_t info = *static_cast<base::source_info_t *>(payload);
        composer.put("sourceInfo", info);
        break;
    }

    case CMP_NOTIFY_VIDEO_INFO:
    {
        base::video_info_t info = *static_cast<base::video_info_t *>(payload);
        composer.put("videoInfo", info);
        CMP_LOG_INFO("videoInfo: width %d, height %d", info.width, info.height);
        break;
    }
    case CMP_NOTIFY_ERROR:
    {
        base::error_t error = *static_cast<base::error_t *>(payload);
        error.mediaId       = media_id_;
        composer.put("error", error);

        if (numValue == CMP_ERROR_RES_ALLOC)
        {
            CMP_LOG_ERROR("policy action occured!");
        }
        break;
    }
    case CMP_NOTIFY_LOAD_COMPLETED:
    {
        composer.put("loadCompleted", mediaInfo);
        break;
    }

    case CMP_NOTIFY_UNLOAD_COMPLETED:
    {
        composer.put("unloadCompleted", mediaInfo);
        break;
    }

    case CMP_NOTIFY_END_OF_STREAM:
    {
        composer.put("endOfStream", mediaInfo);
        break;
    }

    case CMP_NOTIFY_PLAYING:
    {
        composer.put("playing", mediaInfo);
        break;
    }

    case CMP_NOTIFY_PAUSED:
    {
        composer.put("paused", mediaInfo);
        break;
    }
    case CMP_NOTIFY_ACTIVITY:
    {
        CMP_LOG_INFO("notifyActivity to resource requestor");
        if (resourceRequestor_)
            resourceRequestor_->notifyActivity();
        break;
    }
    case CMP_NOTIFY_ACQUIRE_RESOURCE:
    {
        CMP_LOG_INFO("Notify, CMP_NOTIFY_ACQUIRE_RESOURCE");
        ACQUIRE_RESOURCE_INFO_T *info = static_cast<ACQUIRE_RESOURCE_INFO_T *>(payload);
        info->result = AcquireResources(*(info->sourceInfo), info->displayMode, numValue);
        break;
    }
    default:
    {
        CMP_LOG_ERROR("This notification(%d) can't be handled here!", notification);
        break;
    }
    }

    if (!composer.result().empty())
    {
        //[TODO] LSSubscriptionReply
        CMP_LOG_INFO("%s", composer.result().c_str());
    }
}

bool CameraPipelineService::start(LSMessage &message)
{
    jvalue_ref json_outobj = jobject_create();
    auto *payload          = LSMessageGetPayload(&message);
    CMP_LOG_INFO("payload %s", payload);

    pbnjson::JValue parsed = pbnjson::JDomParser::fromString(payload);

    app_id_            = "com.webos.app.mediaevents-test";
    media_id_          = "";
    resourceRequestor_ = std::make_unique<resource::ResourceRequestor>(app_id_, media_id_);

    player_ = PipelineFactory::CreatePlayer(parsed);

    if (!player_)
    {
        CMP_LOG_ERROR("Error: Player not created");
    }
    else
    {
        LoadCommon();

        if (player_->Load(parsed.stringify()))
        {
            CMP_LOG_INFO("Loaded Player");
            isLoaded_ = true;
        }
        else
        {
            CMP_LOG_ERROR("Failed to load player");
        }
    }

    player_->Play();

    jobject_put(json_outobj, J_CSTR_TO_JVAL("returnValue"), jboolean_create(true));

    LS::Message request(&message);
    request.respond(jvalue_stringify(json_outobj));
    CMP_LOG_INFO("response message : %s", jvalue_stringify(json_outobj));

    j_release(&json_outobj);

    return true;
}

bool CameraPipelineService::stop(LSMessage &message)
{
    bool ret               = false;
    jvalue_ref json_outobj = jobject_create();
    auto *payload          = LSMessageGetPayload(&message);
    CMP_LOG_INFO("payload %s", payload);

    if (!isLoaded_)
    {
        CMP_LOG_INFO("already unloaded");
        ret = true;
    }
    else
    {
        if (!player_ || !player_->Unload())
            CMP_LOG_ERROR("fails to unload the player");
        else
        {
            isLoaded_ = false;
            ret       = true;
            if (resourceRequestor_)
            {
                resourceRequestor_->notifyBackground();
                resourceRequestor_->releaseResource();
            }
            else
                CMP_LOG_ERROR("NotifyBackground & ReleaseResources fails");
        }
    }

    jobject_put(json_outobj, J_CSTR_TO_JVAL("returnValue"), jboolean_create(ret));

    LS::Message request(&message);
    request.respond(jvalue_stringify(json_outobj));
    CMP_LOG_INFO("response message : %s", jvalue_stringify(json_outobj));

    j_release(&json_outobj);

    CMP_LOG_INFO("quit main loop");
    g_main_loop_quit(main_loop_ptr_.get());

    return true;
}

void CameraPipelineService::LoadCommon()
{
    if (!resourceRequestor_)
        CMP_LOG_ERROR("NotifyForeground fails");
    else
        resourceRequestor_->notifyForeground();

    player_->RegisterCbFunction(std::bind(&CameraPipelineService::Notify, this,
                                          std::placeholders::_1, std::placeholders::_2,
                                          std::placeholders::_3, std::placeholders::_4));

    if (resourceRequestor_)
    {
        resourceRequestor_->registerUMSPolicyActionCallback(
            [this]()
            {
                base::error_t error;
                error.errorCode = MEDIA_MSG_ERR_POLICY;
                error.errorText = "Policy Action";
                Notify(CMP_NOTIFY_ERROR, CMP_ERROR_RES_ALLOC, nullptr, static_cast<void *>(&error));
                if (!resourceRequestor_)
                    CMP_LOG_ERROR("notifyBackground fails");
                else
                    resourceRequestor_->notifyBackground();
            });
    }
}

bool CameraPipelineService::AcquireResources(const base::source_info_t &sourceInfo,
                                             const std::string &display_mode,
                                             const int32_t display_path)
{
    CMP_LOG_INFO("Service::AcquireResources");
    cmp::resource::PortResource_t resourceMMap;

    if (resourceRequestor_)
    {
        if (!resourceRequestor_->acquireResources(resourceMMap, sourceInfo, display_mode,
                                                  display_path))
        {
            CMP_LOG_ERROR("resource acquisition failed");
            return false;
        }

        for (const auto &it : resourceMMap)
        {
            CMP_LOG_INFO("Resource::[%s]=>index:%d", it.first.c_str(), it.second);
        }
    }

    return true;
}

} // namespace service
} // namespace cmp
