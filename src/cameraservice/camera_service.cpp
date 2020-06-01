// Copyright (c) 2019-2020 LG Electronics, Inc.
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


#include "cameraservice/camera_service.h"
#include "parser/serializer.h"
#include "parser/parser.h"
#include "log/log.h"

#ifdef CMP_DEBUG_PRINT
#undef CMP_DEBUG_PRINT
#endif
#define CMP_DEBUG_PRINT CMP_INFO_PRINT

namespace cmp { namespace service {
Service *Service::instance_ = nullptr;

Service::Service(const char *service_name): media_id_(""), app_id_(""),
      umc_(nullptr), player_(nullptr), resourceRequestor_(nullptr),
       isLoaded_(false)
{
    CMP_DEBUG_PRINT(" this[%p]", this);

    umc_ = std::make_unique<UMSConnector>(service_name, nullptr, nullptr,
            UMS_CONNECTOR_PRIVATE_BUS);

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
        {"getPipelineState", Service::GetPipelineStateEvent},
        {"logPipelineState", Service::LogPipelineStateEvent},
        {"getActivePipelines", Service::GetActivePipelinesEvent},
        {"setPipelineDebugState", Service::SetPipelineDebugStateEvent},

        // exit
        {"exit", Service::ExitEvent},
        {NULL, NULL}};

    umc_->addEventHandlers(
            reinterpret_cast<UMSConnectorEventHandler *>(event_handlers));

}

Service *Service::GetInstance(const char *service_name)
{
    if (!instance_)
        instance_ = new Service(service_name);
    return instance_;
}

Service::~Service()
{
    if (isLoaded_) {
        CMP_DEBUG_PRINT("Unload() should be called if it is still loaded");
        player_->Unload();
    }

}

void Service::Notify(const gint notification, const gint64 numValue,
        const gchar *strValue, void *payload)
{
    cmp::parser::Composer composer;
    cmp::base::media_info_t mediaInfo = { media_id_ };
    switch (notification)
    {
        case NOTIFY_SOURCE_INFO:
        {
            base::source_info_t info  = *static_cast<base::source_info_t *>(payload);
            composer.put("sourceInfo", info);
            break;
        }

        case NOTIFY_VIDEO_INFO:
        {
            base::video_info_t info = *static_cast<base::video_info_t*>(payload);
            composer.put("videoInfo", info);
            CMP_INFO_PRINT("videoInfo: width %d, height %d", info.width, info.height);
            break;
        }
        case NOTIFY_ERROR:
        {
            base::error_t error = *static_cast<base::error_t *>(payload);
            error.mediaId = media_id_;
            composer.put("error", error);

            if (numValue == CMP_ERROR_RES_ALLOC) {
                CMP_DEBUG_PRINT("policy action occured!");
            }
            break;
        }
        case NOTIFY_LOAD_COMPLETED:
        {
            composer.put("loadCompleted", mediaInfo);
            break;
        }

        case NOTIFY_UNLOAD_COMPLETED:
        {
            composer.put("unloadCompleted", mediaInfo);
            break;
        }

        case NOTIFY_END_OF_STREAM:
        {
            composer.put("endOfStream", mediaInfo);
            break;
        }

        case NOTIFY_PLAYING:
        {
            composer.put("playing", mediaInfo);
            break;
        }

        case NOTIFY_PAUSED:
        {
            composer.put("paused", mediaInfo);
            break;
        }
        case NOTIFY_ACTIVITY: {
            CMP_DEBUG_PRINT("notifyActivity to resource requestor");
            if (resourceRequestor_)
                resourceRequestor_->notifyActivity();
            break;
        }
        case NOTIFY_ACQUIRE_RESOURCE: {
            CMP_DEBUG_PRINT("Notify, NOTIFY_ACQUIRE_RESOURCE");
            ACQUIRE_RESOURCE_INFO_T* info = static_cast<ACQUIRE_RESOURCE_INFO_T*>(payload);
            info->result = AcquireResources(*(info->sourceInfo), info->displayMode, numValue);
            break;
        }
        default:
        {
            CMP_DEBUG_PRINT("This notification(%d) can't be handled here!", notification);
            break;
        }
    }

    if (!composer.result().empty())
        umc_->sendChangeNotificationJsonString(composer.result());
}

bool Service::Wait()
{
    return umc_->wait();
}

bool Service::Stop()
{
    return umc_->stop();
}

// uMediaserver public API
bool Service::LoadEvent(UMSConnectorHandle *handle,
                        UMSConnectorMessage *message, void *ctxt)
{
    std::string msg = instance_->umc_->getMessageText(message);
    CMP_DEBUG_PRINT("message : %s", msg.c_str());

    pbnjson::JDomParser jsonparser;
    if (!jsonparser.parse(msg, pbnjson::JSchema::AllSchema())) {
        CMP_DEBUG_PRINT("ERROR : JDomParser.parse Failed!!!");
        return false;
    }

    pbnjson::JValue parsed = jsonparser.getDom();
    if (!parsed.hasKey("id") && parsed["id"].isString()){
        CMP_DEBUG_PRINT("id is invalid");
        return false;
    }

    instance_->media_id_ = parsed["id"].asString();
    instance_->app_id_ = parsed["options"]["option"]["appId"].asString();

    CMP_DEBUG_PRINT("media_id_ : %s", instance_->media_id_.c_str());
    CMP_DEBUG_PRINT("app_id_ : %s", instance_->app_id_.c_str());

    if (instance_->app_id_.empty()){
        CMP_DEBUG_PRINT("appId is empty! resourceRequestor is not created");
        instance_->app_id_ = "EmptyAppId_" + instance_->media_id_;
    } else
        instance_->resourceRequestor_ = std::make_unique<cmp::resource::ResourceRequestor>
                                            (instance_->app_id_, instance_->media_id_);

    instance_->player_= std::make_shared<cmp::player::CameraPlayer>();

    if (!instance_->player_) {
        CMP_INFO_PRINT("Error: Player not created");
    } else {
        instance_->LoadCommon();

        if (instance_->player_->Load(msg)) {
            CMP_DEBUG_PRINT("Loaded Player");
            instance_->isLoaded_ = true;
            return true;
        } else {
            CMP_DEBUG_PRINT("Failed to load player");
        }
    }

    base::error_t error;
    error.errorCode = MEDIA_MSG_ERR_LOAD;
    error.errorText = "Load Failed";
    instance_->Notify(NOTIFY_ERROR, 0, nullptr, static_cast<void*>(&error));

    return false;
}

bool Service::TakeCameraSnapshotEvent(UMSConnectorHandle *handle,
                                UMSConnectorMessage *message, void *ctxt)
{
    std::string msg = instance_->umc_->getMessageText(message);
    CMP_DEBUG_PRINT("message : %s", msg.c_str());

    pbnjson::JDomParser jsonparser;
    if (!jsonparser.parse(msg, pbnjson::JSchema::AllSchema())) {
        CMP_DEBUG_PRINT("ERROR : JDomParser.parse Failed!!!");
      return false;
    }

    pbnjson::JValue parsed = jsonparser.getDom();
    if (!parsed.hasKey("location") && parsed["location"].isString())
    {
        CMP_DEBUG_PRINT("id is invalid");
        return false;
    }
    std::string strLocation = parsed["location"].asString();

    if (strLocation.empty())
    {
        CMP_DEBUG_PRINT("InvalidCameraPlayerClient::TakeCameraSnapshot() Error");
        return false;
    }

    return instance_->player_->TakeSnapshot(strLocation);
}

bool Service::StartCameraRecordEvent(UMSConnectorHandle *handle,
                               UMSConnectorMessage *message, void *ctxt)
{
    pbnjson::JDomParser jsonparser;
    std::string cmd = instance_->umc_->getMessageText(message);
    CMP_DEBUG_PRINT("Service : StartCameraRecordEvent  cmd : ",cmd.c_str());

    if (!jsonparser.parse(cmd, pbnjson::JSchema::AllSchema()))
    {
        CMP_DEBUG_PRINT("ERROR : JDomParser.parse cmd : ",cmd.c_str());
        return false;
    }
    pbnjson::JValue parsed = jsonparser.getDom();

    return instance_->player_->StartRecord(parsed["location"].asString());
}

bool Service::StopCameraRecordEvent(UMSConnectorHandle *handle,
                              UMSConnectorMessage *message, void *ctxt)
{
    return instance_->player_->StopRecord();
}

bool Service::AttachEvent(UMSConnectorHandle *handle,
                          UMSConnectorMessage *message, void *ctxt)
{
    return true;
}

bool Service::UnloadEvent(UMSConnectorHandle *handle,
                          UMSConnectorMessage *message, void *ctxt)
{
    bool ret = false;
    base::error_t error;
    std::string msg = instance_->umc_->getMessageText(message);
    CMP_DEBUG_PRINT("%s", msg.c_str());

    if (!instance_->isLoaded_) {
        CMP_DEBUG_PRINT("already unloaded");
        ret = true;
    } else {
        if (!instance_->player_ || !instance_->player_->Unload())
            CMP_DEBUG_PRINT("fails to unload the player");
        else {
            instance_->isLoaded_ = false;
            ret = true;
            if (instance_->resourceRequestor_){
                instance_->resourceRequestor_->notifyBackground();
            instance_->resourceRequestor_->releaseResource();
            } else
                CMP_DEBUG_PRINT("NotifyBackground & ReleaseResources fails");
         }
    }

    if (!ret) {
        base::error_t error;
        error.errorCode = MEDIA_MSG_ERR_LOAD;
        error.errorText = "Unload Failed";
        error.mediaId = instance_->media_id_;
        instance_->Notify(NOTIFY_ERROR, 0, nullptr, static_cast<void*>(&error));
    }

    instance_->player_.reset();
    instance_->Notify(NOTIFY_UNLOAD_COMPLETED, 0, nullptr, nullptr);

    CMP_DEBUG_PRINT("UnloadEvent Done");
    return ret;
}

// media operations
bool Service::PlayEvent(UMSConnectorHandle *handle,
        UMSConnectorMessage *message, void *ctxt)
{
    std::string msg = instance_->umc_->getMessageText(message);
    CMP_DEBUG_PRINT("message : %s", msg.c_str());

    if (!instance_->player_ || !instance_->isLoaded_) {
        CMP_DEBUG_PRINT("Invalid CameraPlayerClient state, player should be loaded");
        return false;
    }

    return instance_->player_->Play();
}

bool Service::PauseEvent(UMSConnectorHandle *handle,
                         UMSConnectorMessage *message, void *ctxt)
{
    CMP_DEBUG_PRINT("PauseEvent");
    return true;
}

bool Service::StateChangeEvent(UMSConnectorHandle *handle,
                               UMSConnectorMessage *message, void *ctxt)
{
    return instance_->umc_->addSubscriber(handle, message);
}

bool Service::UnsubscribeEvent(UMSConnectorHandle *handle,
                               UMSConnectorMessage *message, void *ctxt)
{
    return true;
}

bool Service::SetUriEvent(UMSConnectorHandle *handle,
                          UMSConnectorMessage *message, void *ctxt)
{
    return true;
}


bool Service::SetPlaneEvent(UMSConnectorHandle *handle,
                            UMSConnectorMessage *message, void *ctxt)
{
    return true;
}

// Resource Manager API
bool Service::GetPipelineStateEvent(UMSConnectorHandle *handle,
                                    UMSConnectorMessage *message, void *ctxt)
{
    return true;
}

bool Service::LogPipelineStateEvent(UMSConnectorHandle *handle,
                                    UMSConnectorMessage *message, void *ctxt)
{
  return true;
}

bool Service::GetActivePipelinesEvent(UMSConnectorHandle *handle,
                                      UMSConnectorMessage *message, void *ctxt)
{
    return true;
}

bool Service::SetPipelineDebugStateEvent(UMSConnectorHandle *handle,
                                         UMSConnectorMessage *message,
                                         void *ctxt)
{
    return true;
}
// exit
bool Service::ExitEvent(UMSConnectorHandle *handle,
                        UMSConnectorMessage *message, void *ctxt)
{
    return instance_->umc_->stop();
}

void Service::LoadCommon()
{
    if (!resourceRequestor_)
        CMP_DEBUG_PRINT("NotifyForeground fails");
    else
        resourceRequestor_->notifyForeground();

    player_->RegisterCbFunction (
                std::bind(&Service::Notify, instance_,
                    std::placeholders::_1, std::placeholders::_2,
                    std::placeholders::_3, std::placeholders::_4));

    if (resourceRequestor_) {
        resourceRequestor_->registerUMSPolicyActionCallback([this]() {
            base::error_t error;
            error.errorCode = MEDIA_MSG_ERR_POLICY;
            error.errorText = "Policy Action";
            Notify(NOTIFY_ERROR, CMP_ERROR_RES_ALLOC,
                                    nullptr, static_cast<void*>(&error));
            if (!resourceRequestor_)
                CMP_DEBUG_PRINT("NotifyForeground fails");
            else
                resourceRequestor_->notifyForeground();
            });
    }
}

bool Service::AcquireResources(const base::source_info_t &sourceInfo,
                                         const std::string &display_mode,
                                         uint32_t display_path)
{
    CMP_DEBUG_PRINT("Service::AcquireResources");
    cmp::resource::PortResource_t resourceMMap;

    if (resourceRequestor_) {
        if (!resourceRequestor_->acquireResources(
                  resourceMMap, sourceInfo, display_mode, display_path)) {
            CMP_INFO_PRINT("resource acquisition failed");
            return false;
        }

        for (auto it : resourceMMap) {
            CMP_DEBUG_PRINT("Resource::[%s]=>index:%d", it.first.c_str(), it.second);
        }
    }

    return true;
}

}  // namespace service
}  // namespace cmp

