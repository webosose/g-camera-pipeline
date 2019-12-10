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


#ifndef SRC_CAMERA_SERVICE_H_
#define SRC_CAMERA_SERVICE_H_

#include <memory>
#include <string>

#include "notification.h"
#include "base.h"

class UMSConnector;
class UMSConnectorHandle;
class UMSConnectorMessage;

namespace cmp { namespace player { class CameraPlayer; }}
namespace cmp { namespace base { struct source_info_t; }}
namespace cmp { namespace resource { class ResourceRequestor; }}

#define PLANE_MAP_SIZE 4

namespace cmp { namespace service {
class IService {
 public:
  virtual void Notify(const NOTIFY_TYPE_T notification) = 0;
  virtual void Notify(const NOTIFY_TYPE_T notification, const void *payload) = 0;
  virtual bool Wait() = 0;
  virtual bool Stop()= 0;
  virtual bool acquire(cmp::base::source_info_t   &source_info, const int32_t display_path = 0) = 0;
};

class Service : public IService {
 public:
  ~Service();
  static Service *GetInstance(const char *service_name);

  void Notify(const NOTIFY_TYPE_T notification) override;
  void Notify(const NOTIFY_TYPE_T notification, const void *payload) override;

  bool Wait() override;
  bool Stop() override;
  void Initialize(cmp::player::CameraPlayer *player);
  bool acquire(cmp::base::source_info_t   &source_info, const int32_t display_path = 0) override;

  // uMediaserver public API
  static bool LoadEvent(UMSConnectorHandle *handle, UMSConnectorMessage *message, void *ctxt);
  static bool TakeCameraSnapshotEvent(UMSConnectorHandle *handle, UMSConnectorMessage *message, void *ctxt);
  static bool StartCameraRecordEvent(UMSConnectorHandle *handle, UMSConnectorMessage *message, void *ctxt);
  static bool StopCameraRecordEvent(UMSConnectorHandle *handle, UMSConnectorMessage *message, void *ctxt);
  static bool AttachEvent(UMSConnectorHandle *handle, UMSConnectorMessage *message, void *ctxt);
  static bool UnloadEvent(UMSConnectorHandle *handle, UMSConnectorMessage *message, void *ctxt);

  // media operations
  static bool PlayEvent(UMSConnectorHandle *handle, UMSConnectorMessage *message, void *ctxt);
  static bool PauseEvent(UMSConnectorHandle *handle, UMSConnectorMessage *message, void *ctxt);
  static bool StateChangeEvent(UMSConnectorHandle *handle, UMSConnectorMessage *message, void *ctxt);
  static bool UnsubscribeEvent(UMSConnectorHandle *handle, UMSConnectorMessage *message, void *ctxt);
  static bool SetUriEvent(UMSConnectorHandle *handle, UMSConnectorMessage *message, void *ctxt);
  static bool SetPlaneEvent(UMSConnectorHandle *handle, UMSConnectorMessage *message, void *ctxt);

  // Resource Manager API
  static bool RegisterPipelineEvent(UMSConnectorHandle *handle, UMSConnectorMessage *message, void *ctxt);
  static bool UnregisterPipelineEvent(UMSConnectorHandle *handle, UMSConnectorMessage *message, void *ctxt);
  static bool AcquireEvent(UMSConnectorHandle *handle, UMSConnectorMessage *message, void *ctxt);
  static bool TryAcquireEvent(UMSConnectorHandle *handle, UMSConnectorMessage *message, void *ctxt);
  static bool ReleaseEvent(UMSConnectorHandle *handle, UMSConnectorMessage *message, void *ctxt);
  static bool NotifyForegroundEvent(UMSConnectorHandle *handle, UMSConnectorMessage *message, void *ctxt);
  static bool NotifyBackgroundEvent(UMSConnectorHandle *handle, UMSConnectorMessage *message, void *ctxt);
  static bool NotifyActivityEvent(UMSConnectorHandle *handle, UMSConnectorMessage *message, void *ctxt);
  static bool TrackAppProcessesEvent(UMSConnectorHandle *handle, UMSConnectorMessage *message, void *ctxt);

  // pipeline state query API
  static bool GetPipelineStateEvent(UMSConnectorHandle *handle, UMSConnectorMessage *message, void *ctxt);
  static bool LogPipelineStateEvent(UMSConnectorHandle *handle, UMSConnectorMessage *message, void *ctxt);
  static bool GetActivePipelinesEvent(UMSConnectorHandle *handle, UMSConnectorMessage *message, void *ctxt);
  static bool SetPipelineDebugStateEvent(UMSConnectorHandle *handle, UMSConnectorMessage *message, void *ctxt);

  // exit
  static bool ExitEvent(UMSConnectorHandle *handle, UMSConnectorMessage *message, void *ctxt);

 private:
  Service();
  Service(const char *service_name);
  Service(Service const &) = delete;
  Service &operator=(Service const &) = delete;

  UMSConnector *umc_;
  std::string media_id_;  // connection_id
  cmp::player::CameraPlayer *player_;
  static Service *instance_;
  std::shared_ptr<cmp::resource::ResourceRequestor> res_requestor_;

  // plane 51 is used for LSM. So we have to avoid using plane 51.
  int kPlaneMap[PLANE_MAP_SIZE] = {47, 46, 45, 44};
};

}  // namespace service
}  // namespace cmp

#endif  // SRC_CAMERA_SERVICE_H_

