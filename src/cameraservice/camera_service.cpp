// Copyright (c) 2019 LG Electronics, Inc.
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


#include <stdlib.h>
#include <unistd.h>
#include <UMSConnector.h>
#include <string>
#include "log/log.h"
#include "base/base.h"
#include "parser/parser.h"
#include "parser/serializer.h"
#include "cameraplayer/camera_player.h"
#include "resourcefacilitator/requestor.h"
#include "cameraservice/camera_service.h"
#include <pbnjson.hpp>

#ifdef CMP_DEBUG_PRINT
#undef CMP_DEBUG_PRINT
#endif
#define CMP_DEBUG_PRINT CMP_INFO_PRINT

namespace cmp { namespace service {
Service *Service::instance_ = NULL;

Service::Service(const char *service_name)
  : umc_(NULL)
  , media_id_("")
  , player_(NULL) {
  umc_ = new UMSConnector(service_name, NULL, NULL, UMS_CONNECTOR_PRIVATE_BUS);
}

Service *Service::GetInstance(const char *service_name)
{
    if (!instance_)
        instance_ = new Service(service_name);
    return instance_;
}

Service::~Service()
{
    if (umc_)
        delete umc_;

    if (res_requestor_) {
        res_requestor_->releaseResource();
    }
}

void Service::Notify(const NOTIFY_TYPE_T notification)
{
    switch (notification)
    {
        case NOTIFY_LOAD_COMPLETED:
            {
                gchar *message = g_strdup_printf("{\"loadCompleted\":{\"mediaId\":\"%s\"}}", media_id_.c_str());
                umc_->sendChangeNotificationJsonString(message);
                g_free(message);
                break;
            }

        case NOTIFY_UNLOAD_COMPLETED:
            {
                gchar *message = g_strdup_printf("{\"unloadCompleted\":{\"mediaId\":\"%s\"}}", media_id_.c_str());
                umc_->sendChangeNotificationJsonString(message);
                g_free(message);
                break;
            }

        case NOTIFY_END_OF_STREAM:
            {
                gchar *message = g_strdup_printf("{\"endOfStream\":{\"mediaId\":\"%s\"}}", media_id_.c_str());
                umc_->sendChangeNotificationJsonString(message);
                g_free(message);
                break;
            }

        case NOTIFY_PLAYING:
            {
                gchar *message = g_strdup_printf("{\"playing\":{\"mediaId\":\"%s\"}}", media_id_.c_str());
                umc_->sendChangeNotificationJsonString(message);
                g_free(message);
                break;
            }

        case NOTIFY_PAUSED:
            {
                gchar *message = g_strdup_printf("{\"paused\":{\"mediaId\":\"%s\"}}", media_id_.c_str());
                umc_->sendChangeNotificationJsonString(message);
                g_free(message);
                break;
            }

        default:
            {
                CMP_DEBUG_PRINT("This notification can't be handled here!");
                break;
            }
    }
}

void Service::Notify(const NOTIFY_TYPE_T notification, const void *payload)
{
    if (!payload)
        return;

    switch (notification)
    {
        case NOTIFY_SOURCE_INFO:
            {
                cmp::parser::Composer composer;
                base::source_info_t *info  = (base::source_info_t *)payload;
                composer.put("sourceInfo", *info);
                umc_->sendChangeNotificationJsonString(composer.result());

                base::video_info_t videoInfo;
                memset(&videoInfo, 0, sizeof(base::video_info_t));
                videoInfo.width = info->video_streams.front().width;
                videoInfo.height = info->video_streams.front().height;
                videoInfo.frame_rate.num = info->video_streams.front().frame_rate.num;
                videoInfo.frame_rate.den = info->video_streams.front().frame_rate.den;

                res_requestor_->setVideoInfo(videoInfo);

                break;
            }

        case NOTIFY_VIDEO_INFO:
            {
                cmp::parser::Composer composer;
                base::video_info_t *info = (base::video_info_t*)payload;
                composer.put("videoInfo", *info);
                CMP_INFO_PRINT("%s : info->width[%d], info->height[%d]", __func__, info->width, info->height);
                umc_->sendChangeNotificationJsonString(composer.result());
                res_requestor_->setVideoInfo(*info);
                break;
            }

        case NOTIFY_ERROR:
            {
                cmp::parser::Composer composer;
                base::error_t *error = (base::error_t *)payload;
                error->mediaId = media_id_;
                composer.put("error", *error);
                umc_->sendChangeNotificationJsonString(composer.result());
                break;
            }

        default:
            {
                CMP_DEBUG_PRINT("This notification can't be handled here!");
                break;
            }
    }
}

void Service::Initialize(cmp::player::Player *player)
{
    if (!player || !umc_)
        return;

    player_ = player;
    player_->Initialize(this);

    static UMSConnectorEventHandler event_handlers[] = {
        // uMediaserver public API
        {"load", Service::LoadEvent},
        {"takeCameraSnapshot", Service::TakeCameraSnapshotEvent},
        {"startCameraRecord", Service::StartCameraRecordEvent},
        {"stopCameraRecord", Service::StopCameraRecordEvent},
        {"attach", Service::AttachEvent},
        {"unload", Service::UnloadEvent},

        // media operations
        {"play", Service::PlayEvent},
        {"pause", Service::PauseEvent},
        {"stateChange", Service::StateChangeEvent},
        {"unsubscribe", Service::UnsubscribeEvent},
        {"setUri", Service::SetUriEvent},
        {"setPlane", Service::SetPlaneEvent},

        // Resource Manager API
        {"registerPipeline", Service::RegisterPipelineEvent},
        {"unregisterPipeline", Service::UnregisterPipelineEvent},
        {"acquire", Service::AcquireEvent},
        {"tryAcquire", Service::TryAcquireEvent},
        {"release", Service::ReleaseEvent},
        {"notifyForeground", Service::NotifyForegroundEvent},
        {"notifyBackground", Service::NotifyBackgroundEvent},
        {"notifyActivity", Service::NotifyActivityEvent},
        {"trackAppProcesses", Service::TrackAppProcessesEvent},

        // pipeline state query API
        {"getPipelineState", Service::GetPipelineStateEvent},
        {"logPipelineState", Service::LogPipelineStateEvent},
        {"getActivePipelines", Service::GetActivePipelinesEvent},
        {"setPipelineDebugState", Service::SetPipelineDebugStateEvent},

        // exit
        {"exit", Service::ExitEvent},
        {NULL, NULL}};

    umc_->addEventHandlers(reinterpret_cast<UMSConnectorEventHandler *>(event_handlers));
}

bool Service::Wait()
{
    return umc_->wait();
}

bool Service::Stop()
{
    return umc_->stop();
}

bool Service::acquire(cmp::base::source_info_t &source_info, const int32_t display_path)
{
    cmp::resource::PortResource_t resourceMMap;
    cmp::base::disp_res_t dispRes = {-1,-1,-1};

    res_requestor_->setSourceInfo(source_info);

    if (!res_requestor_->acquireResources(NULL, resourceMMap, dispRes, display_path)) {
        CMP_DEBUG_PRINT("resource acquisition failed");
        return false;
    }

    for (auto it : resourceMMap)
    {
        CMP_DEBUG_PRINT("Got Resource - name:%s, index:%d", it.first.c_str(), it.second);
    }

    if (dispRes.plane_id > 0 && dispRes.crtc_id > 0 && dispRes.conn_id > 0) {
       player_->SetDisplayResource(dispRes);
    } else {
       CMP_DEBUG_PRINT("ERROR : Failed to get displayResource(%d,%d,%d)", dispRes.plane_id, dispRes.crtc_id, dispRes.conn_id);
       return false;
    }

    CMP_DEBUG_PRINT("resource acquired!!!, plane_id: %d, crtc_id: %d, conn_id: %d", dispRes.plane_id, dispRes.crtc_id, dispRes.conn_id);
    return true;
}

// uMediaserver public API
bool Service::LoadEvent(UMSConnectorHandle *handle, UMSConnectorMessage *message, void *ctxt)
{
    std::string cmd = instance_->umc_->getMessageText(message);
    CMP_DEBUG_PRINT("message : %s", cmd.c_str());

    cmp::parser::Parser parser(cmd.c_str());
    instance_->media_id_ = parser.get<std::string>("id");

    CMP_DEBUG_PRINT("media_id_ : %s", instance_->media_id_.c_str());
    instance_->res_requestor_ = std::make_shared<cmp::resource::ResourceRequestor>("media", instance_->media_id_);

    instance_->res_requestor_->registerUMSPolicyActionCallback([=] () {
            CMP_DEBUG_PRINT("registerUMSPolicyActionCallback");
            instance_->res_requestor_->notifyBackground();
            instance_->player_->Unload();
            });

    pbnjson::JDomParser jsonparser;
    if (!jsonparser.parse(cmd, pbnjson::JSchema::AllSchema())) {
        CMP_DEBUG_PRINT("ERROR : JDomParser.parse Failed!!!");
        return false;
    }

    pbnjson::JValue parsed = jsonparser.getDom();
    std::string payload = pbnjson::JGenerator::serialize(parsed["options"], pbnjson::JSchema::AllSchema());
    bool ret = instance_->player_->Load(cmd, payload);

    return ret;
}

bool Service::TakeCameraSnapshotEvent(UMSConnectorHandle *handle, UMSConnectorMessage *message, void *ctxt)
{
    pbnjson::JDomParser jsonparser;
    std::string cmd = instance_->umc_->getMessageText(message);

    if (!jsonparser.parse(cmd, pbnjson::JSchema::AllSchema()))
    {
        CMP_DEBUG_PRINT("ERROR : JDomParser.parse cmd : ",cmd.c_str());
        return false;
    }
    pbnjson::JValue parsed = jsonparser.getDom();
    bool ret = instance_->player_->takeSnapshot(parsed["location"].asString());
    return ret;
}

bool Service::StartCameraRecordEvent(UMSConnectorHandle *handle, UMSConnectorMessage *message, void *ctxt)
{
    pbnjson::JDomParser jsonparser;
    std::string cmd = instance_->umc_->getMessageText(message);

    if (!jsonparser.parse(cmd, pbnjson::JSchema::AllSchema()))
    {
        CMP_DEBUG_PRINT("ERROR : JDomParser.parse cmd : ",cmd.c_str());
        return false;
    }
    pbnjson::JValue parsed = jsonparser.getDom();
    bool ret = instance_->player_->startRecord(parsed["location"].asString());
    return ret;
}

bool Service::StopCameraRecordEvent(UMSConnectorHandle *handle, UMSConnectorMessage *message, void *ctxt)
{
    return instance_->player_->stopRecord();
}

bool Service::AttachEvent(UMSConnectorHandle *handle, UMSConnectorMessage *message, void *ctxt)
{
    return true;
}

bool Service::UnloadEvent(UMSConnectorHandle *handle, UMSConnectorMessage *message, void *ctxt)
{
    instance_->res_requestor_->notifyBackground();
    instance_->res_requestor_->releaseResource();
    return instance_->player_->Unload();
}

// media operations
bool Service::PlayEvent(UMSConnectorHandle *handle, UMSConnectorMessage *message, void *ctxt)
{
    CMP_DEBUG_PRINT("PlayEvent");
    return instance_->player_->Play();
}

bool Service::PauseEvent(UMSConnectorHandle *handle, UMSConnectorMessage *message, void *ctxt)
{
    CMP_DEBUG_PRINT("PauseEvent");
    return true;
}

bool Service::StateChangeEvent(UMSConnectorHandle *handle, UMSConnectorMessage *message, void *ctxt)
{
    return instance_->umc_->addSubscriber(handle, message);
}

bool Service::UnsubscribeEvent(UMSConnectorHandle *handle, UMSConnectorMessage *message, void *ctxt)
{
    return true;
}

bool Service::SetUriEvent(UMSConnectorHandle *handle, UMSConnectorMessage *message, void *ctxt)
{
    return true;
}


bool Service::SetPlaneEvent(UMSConnectorHandle *handle, UMSConnectorMessage *message, void *ctxt)
{
    return true;
}

// Resource Manager API
bool Service::RegisterPipelineEvent(UMSConnectorHandle *handle, UMSConnectorMessage *message, void *ctxt)
{
    return true;
}

bool Service::UnregisterPipelineEvent(UMSConnectorHandle *handle, UMSConnectorMessage *message, void *ctxt)
{
    return true;
}

bool Service::AcquireEvent(UMSConnectorHandle *handle, UMSConnectorMessage *message, void *ctxt)
{
    return true;
}

bool Service::TryAcquireEvent(UMSConnectorHandle *handle, UMSConnectorMessage *message, void *ctxt)
{
    return true;
}

bool Service::ReleaseEvent(UMSConnectorHandle *handle, UMSConnectorMessage *message, void *ctxt)
{
    return true;
}

bool Service::NotifyForegroundEvent(UMSConnectorHandle *handle, UMSConnectorMessage *message, void *ctxt)
{
    return true;
}

bool Service::NotifyBackgroundEvent(UMSConnectorHandle *handle, UMSConnectorMessage *message, void *ctxt)
{
    return true;
}

bool Service::NotifyActivityEvent(UMSConnectorHandle *handle, UMSConnectorMessage *message, void *ctxt)
{
    return true;
}

bool Service::TrackAppProcessesEvent(UMSConnectorHandle *handle, UMSConnectorMessage *message, void *ctxt)
{
    return true;
}

// pipeline state query API
bool Service::GetPipelineStateEvent(UMSConnectorHandle *handle, UMSConnectorMessage *message, void *ctxt)
{
    return true;
}

bool Service::LogPipelineStateEvent(UMSConnectorHandle *handle, UMSConnectorMessage *message, void *ctxt)
{
  return true;
}

bool Service::GetActivePipelinesEvent(UMSConnectorHandle *handle, UMSConnectorMessage *message, void *ctxt)
{
    return true;
}

bool Service::SetPipelineDebugStateEvent(UMSConnectorHandle *handle, UMSConnectorMessage *message, void *ctxt)
{
    return true;
}
// exit
bool Service::ExitEvent(UMSConnectorHandle *handle, UMSConnectorMessage *message, void *ctxt)
{
    return instance_->umc_->stop();
}

}  // namespace service
}  // namespace cmp

