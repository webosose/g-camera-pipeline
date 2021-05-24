// Copyright (c) 2019-2021 LG Electronics, Inc.
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
#include <gst/player/player.h>
#include <gst/pbutils/pbutils.h>
#include <gst/app/gstappsrc.h>
#include <gst/app/gstappsink.h>
#include <gst/pbutils/pbutils.h>
#include <camera_window_manager.h>
#include <cameraservice/camera_service.h>
#include <luna-service2/lunaservice.hpp>
#include "base.h"
#include "message.h"
#include "camshm.h"
#include "cam_posixshm.h"
#include "camera_types.h"

using namespace std;

static bool getFdCb(LSHandle *, LSMessage *, void *);
static constexpr char const *waylandDisplayHandleContextType =
    "GstWaylandDisplayHandleContextType";
using CALLBACK_T = std::function<void(const gint type, const gint64 numValue,
        const gchar *strValue, void *udata)>;
typedef struct _GstAppSrcContext
{
    SHMEM_HANDLE shmemHandle;
    gint streamingAllowState;
    int key;
    gint isStreaming;
    gint isFirstCallback;
    GstAppSrc *appsrc;
}GstAppSrcContext;

typedef struct ACQUIRE_RESOURCE_INFO {
  cmp::base::source_info_t* sourceInfo;
  char *displayMode;
  gboolean result;
} ACQUIRE_RESOURCE_INFO_T;

namespace cmp { namespace service { class Service; }}

namespace cmp { namespace player {

class CameraPlayer {
 public:

  CameraPlayer();
  ~CameraPlayer();
  bool Load(const std::string& mediaId,
            const std::string& options, const std::string& payload);
  bool Load(const std::string& str);
  bool LoadPlayer();
  bool Unload();
  void RegisterCbFunction(CALLBACK_T);
  bool Play();
  bool subscribeToCameraService();
  bool TakeSnapshot(const std::string& location);
  bool StartRecord(const std::string& location, const std::string& format,
                     bool audio, const std::string& audioSrc);
  bool StopRecord();

  static gboolean HandleBusMessage(GstBus *bus,
                                   GstMessage *message, gpointer user_data);
  static GstBusSyncReply HandleSyncBusMessage(GstBus *bus,
                                     GstMessage *msg, gpointer data);
  static guint mCameraServiceCbTimerID;
  static gboolean CameraServiceCbTimerCallback(void* data);
  void CameraServiceCbTimerReset();
 private:
  void PauseInternalSync();
  void ParseOptionString(const std::string& options);
  void NotifySourceInfo();
  void SetGstreamerDebug();
  bool attachSurface(bool allow_no_window = false);
  bool detachSurface();
  void WriteImageToFile(const void *p, int size);
  bool GetSourceInfo();
  bool LoadPipeline();
  bool SetPlayerState(base::playback_state_t state) {
    current_state_ = state;
    return true;
  }

  bool CreatePreviewBin(GstPad * pad);
  bool CreateCaptureElements(GstPad * pad);
  bool CreateRecordElements(GstPad * pad, GstPad *, const std::string& fileFormat);
  bool CreateAudioRecordElements(const std::string&, GstPad * pad);
  bool LoadYUY2Pipeline();
  bool LoadJPEGPipeline();
  int32_t ConvertErrorCode(GQuark domain, gint code);
  base::error_t HandleErrorMessage(GstMessage *message);

  void FreeLoadPipelineElements();
  void FreeCaptureElements();
  void FreeRecordElements();
  void FreePreviewBinElements();

  static void FeedData(GstElement * appsrc, guint size, gpointer gdata);
  static void FeedPosixData(GstElement * appsrc, guint size, gpointer gdata);
  static GstFlowReturn GetSample(GstAppSink *elt, gpointer data);
  static GstPadProbeReturn CaptureRemoveProbe(GstPad * pad,
                                                GstPadProbeInfo * info,
                                                gpointer user_data);
  static GstPadProbeReturn RecordRemoveProbe(GstPad * pad,
                                               GstPadProbeInfo * info,
                                               gpointer user_data);

  std::string media_id_;
  uint32_t display_path_;
  CALLBACK_T cbFunction_;

  int32_t planeId_, width_, height_, framerate_, crtcId_, connId_,
            display_path_idx_,handle_, iomode_;
  int  num_of_images_to_capture_, num_of_captured_images_;
  std::string uri_, memtype_, memsrc_, format_, capture_path_, record_path_;
  GstElement *pipeline_, *source_, *parser_, *decoder_, *filter_YUY2_, *filter_NV12_,
             *filter_I420_, *filter_JPEG_, *filter_RGB_, *vconv_, *record_convert_,
             *preview_decoder_, *preview_parser_, *preview_encoder_, *preview_convert_,
             *tee_, *capture_queue_, *capture_encoder_, *capture_sink_, *record_queue_,
             *record_encoder_, *record_parse_, *record_decoder_, *record_mux_, *record_sink_,
             *preview_queue_, *preview_sink_, *record_audio_src_, *record_audio_queue_,
             *record_audio_convert_, *record_video_queue_, *record_audio_encoder_;
  GstPad *tee_preview_pad_, *preview_ghost_sinkpad_, *preview_queue_pad_,
         *capture_queue_pad_, *tee_capture_pad_, *record_queue_pad_,
         *tee_record_pad_, *record_audio_encoder_pad_, *record_video_queue_pad_,
         *record_audio_mux_pad_, *record_video_mux_pad_;
  GstAppSrcContext context_ ;
  base::source_info_t source_info_;
  base::playback_state_t current_state_;
  GstBus *bus_;
  GstCaps *caps_YUY2_, *caps_NV12_, *caps_I420_, *caps_JPEG_, *caps_RGB_;
  cmp::service::Service *service_;
  bool load_complete_;

  /* GAV Features */
  LSM::CameraWindowManager lsm_camera_window_manager_;
  std::string display_mode_;
  std::string window_id_;
};

}  // namespace player
}  // namespace cmp
#endif  // SRC_CAMERA_PLAYER_H_
