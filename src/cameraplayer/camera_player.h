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


#ifndef SRC_CAMERA_PLAYER_H_
#define SRC_CAMERA_PLAYER_H_

#include <glib.h>
#include <glib-unix.h>
#include <gst/gst.h>
#include <string>
#include <memory>
#include <thread>
#include <gst/player/player.h>
#include <gst/pbutils/pbutils.h>
#include <gst/app/gstappsrc.h>

#include "base.h"


typedef void *SHMEM_HANDLE;
typedef struct _GstAppSrcContext
{
    SHMEM_HANDLE shmemHandle;
    gint streamingAllowState;
    int shmemKey;
    gint isStreaming;
    gint isFirstCallback;
    GstAppSrc *appsrc;
}GstAppSrcContext;
static int count = 0;
typedef struct
{
      GMainLoop *loop;
      GstElement *source;
      GstElement *sink;
      char *shmpointer;
      int shmemid;
} ProgramData;

namespace cmp { namespace service { class IService; }}
namespace cmp { namespace resource { class ResourceRequestor; }}

namespace cmp { namespace player {

static GstElement *pipeline, *capture_queue, *preview_queue, *capture_sink,*capture_encoder,
                  *tee_ref, *record_queue, *record_sink,*record_encoder,
                  *record_bin, *capture_bin, *preview_bin,
                  *preview_sink, *tee, *source, *parser,
                  *decoder, *filter, *filter2, *vconv;
static GstPad *capture_ghost_sinkpad, *preview_ghost_sinkpad,*record_ghost_sinkpad,
              *preview_queue_pad, *capture_tee_pad_ref, *record_tee_pad_ref,
              *record_queue_pad,*capture_queue_pad, *tee_capture_pad,
              *tee_preview_pad, *tee_record_pad;

static int planeNo;
static int crctNo;
static int connNo;

class Player {
 public:
  Player()
    : pipeline_(NULL)
    , service_(NULL)
    , load_complete_(false) {}

  virtual ~Player() {}

  virtual bool Load(const std::string &str,const std::string &payload) = 0;
  virtual bool Unload() = 0;
  virtual bool Play() = 0;
  virtual bool SetPlane(int planeId) = 0;
  virtual bool SetDisplayResource(cmp::base::disp_res_t &res) = 0;
  virtual void Initialize(cmp::service::IService *service) = 0;
  virtual bool takeSnapshot(const std::string  &location) = 0;
  virtual bool startRecord(const std::string  &location) = 0;
  virtual bool stopRecord() = 0;

 protected:
  GstElement *pipeline_;
  cmp::service::IService *service_;
  bool load_complete_;
};

class CameraPlayer : public Player {
 public:
  CameraPlayer();
  ~CameraPlayer();
  bool Load(const std::string &str,const std::string &payload) override;
  bool Unload() override;
  bool Play() override;
  bool SetPlane(int planeId) override;
  bool SetDisplayResource(cmp::base::disp_res_t &res) override;
  void Initialize(cmp::service::IService *service) override;
  static gboolean HandleBusMessage(GstBus *bus,
                                   GstMessage *message, gpointer user_data);
  bool takeSnapshot(const std::string  &location) override;
  bool startRecord(const std::string  &location) override;
  bool stopRecord() override;

 private:
  int32_t planeId_, shmkey_, width_, height_,framerate_;

  std::string memtype_;
  std::string memsrc_;
  std::string format_;

  base::source_info_t source_info_;
  std::shared_ptr<cmp::resource::ResourceRequestor> res_requestor_;
  base::playback_state_t current_state_;
  GstBus *bus;
  GstCaps *caps, *caps2, *scalecaps;

  void NotifySourceInfo();
  bool GetSourceInfo();
  bool LoadPipeline();
  base::error_t HandleErrorMessage(GstMessage *message);
  int32_t ConvertErrorCode(GQuark domain, gint code);
  void SetGstreamerDebug();
  int GetShmkey(const std::string &uri);
  base::playback_state_t GetPlayerState() const { return current_state_; }
  bool SetPlayerState(base::playback_state_t state) {
    current_state_ = state;
    return true;
  }
  void ParseOptionString(const std::string &str);
  int32_t crtcId_;
  int32_t connId_;
  int32_t display_path_;
  std::string uri_;
};

}  // namespace player
}  // namespace cmp
#endif  // SRC_CAMERA_PLAYER_H_

