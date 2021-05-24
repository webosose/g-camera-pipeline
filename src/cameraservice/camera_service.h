// Copyright (c) 2019-20201 LG Electronics, Inc.
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


#ifndef SRC_CAMERA_SERVICE_H_
#define SRC_CAMERA_SERVICE_H_

#include <UMSConnector.h>
#include "resourcefacilitator/requestor.h"
#include "cameraplayer/camera_player.h"
#include "base/base.h"
#include <base/message.h>

class UMSConnector;
class UMSConnectorHandle;
class UMSConnectorMessage;

namespace cmp { namespace player { class CameraPlayer; }}
namespace cmp { namespace base { struct source_info_t; }}
namespace cmp { namespace resource { class ResourceRequestor; }}

namespace cmp { namespace service {

class Service {
 public:
  static Service *GetInstance(const char *service_name);

  ~Service();

  void Notify(const gint notification, const gint64 numValue,
          const gchar *strValue, void *payload = nullptr);

  bool Wait();
  bool Stop();

  // uMediaserver public API
  static bool LoadEvent(UMSConnectorHandle *handle,
                        UMSConnectorMessage *message, void *ctxt);
  static bool TakeCameraSnapshotEvent(UMSConnectorHandle *handle,
                                UMSConnectorMessage *message, void *ctxt);
  static bool StartCameraRecordEvent(UMSConnectorHandle *handle,
                               UMSConnectorMessage *message, void *ctxt);
  static bool StopCameraRecordEvent(UMSConnectorHandle *handle,
                              UMSConnectorMessage *message, void *ctxt);
  static bool AttachEvent(UMSConnectorHandle *handle,
                          UMSConnectorMessage *message, void *ctxt);
  static bool UnloadEvent(UMSConnectorHandle *handle,
                          UMSConnectorMessage *message, void *ctxt);

  // media operations
  static bool PlayEvent(UMSConnectorHandle *handle,
                        UMSConnectorMessage *message, void *ctxt);
  static bool PauseEvent(UMSConnectorHandle *handle,
                         UMSConnectorMessage *message, void *ctxt);
  static bool StateChangeEvent(UMSConnectorHandle *handle,
                               UMSConnectorMessage *message, void *ctxt);
  static bool UnsubscribeEvent(UMSConnectorHandle *handle,
                               UMSConnectorMessage *message, void *ctxt);
  static bool SetUriEvent(UMSConnectorHandle *handle,
                          UMSConnectorMessage *message, void *ctxt);
  static bool SetPlaneEvent(UMSConnectorHandle *handle,
                            UMSConnectorMessage *message, void *ctxt);

  // Resource Manager API
  static bool GetPipelineStateEvent(UMSConnectorHandle *handle,
                                    UMSConnectorMessage *message, void *ctxt);
  static bool LogPipelineStateEvent(UMSConnectorHandle *handle,
                                      UMSConnectorMessage *message, void *ctxt);
  static bool GetActivePipelinesEvent(UMSConnectorHandle *handle,
                           UMSConnectorMessage *message, void *ctxt);
  static bool SetPipelineDebugStateEvent(UMSConnectorHandle *handle,
                              UMSConnectorMessage *message, void *ctxt);
  static bool ExitEvent(UMSConnectorHandle *handle,
                           UMSConnectorMessage *message, void *ctxt);


 private:
  explicit Service(const char *service_name);
  void LoadCommon();
  bool AcquireResources(const base::source_info_t &sourceInfo,
              const std::string &display_mode = "Default", uint32_t display_path = 0);

  std::string media_id_;  // connection_id
  std::string app_id_;
  std::unique_ptr<UMSConnector> umc_;
  std::shared_ptr<cmp::player::CameraPlayer> player_;
  std::unique_ptr<cmp::resource::ResourceRequestor> resourceRequestor_;
  bool isLoaded_;

  static Service *instance_;

  Service(const Service& s) = delete;
  void operator=(const Service& s) = delete;
};

}  // namespace service
}  // namespace cmp

#endif  // SRC_CAMERA_SERVICE_H_

