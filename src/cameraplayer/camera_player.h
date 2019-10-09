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
#include <gst/app/gstappsink.h>

#include "camshm.h"
#include "base.h"

typedef struct _GstAppSrcContext
{
    SHMEM_HANDLE shmemHandle;
    gint streamingAllowState;
    int shmemKey;
    gint isStreaming;
    gint isFirstCallback;
    GstAppSrc *appsrc;
}GstAppSrcContext;
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

class CameraPlayer {
 public:
  CameraPlayer();
  ~CameraPlayer();
  bool Load(const std::string& str, const std::string& payload);
  bool Unload();
  bool Play();
  bool SetPlane(int planeId);
  bool SetDisplayResource(cmp::base::disp_res_t &res);
  bool TakeSnapshot(const std::string& location);
  bool StartRecord(const std::string& location);
  bool StopRecord();
  void Initialize(cmp::service::IService *service);
  static gboolean HandleBusMessage(GstBus *bus,
                                   GstMessage *message, gpointer user_data);

 private:

  void NotifySourceInfo();
  void SetGstreamerDebug();
  void ParseOptionString(const std::string& str);
  void WriteImageToFile(const void *p, int size);
  bool GetSourceInfo();
  bool LoadPipeline();
  bool SetPlayerState(base::playback_state_t state) {
    current_state_ = state;
    return true;
  }

  bool CreatePreviewBin(GstPad * pad);
  bool CreateCaptureElements(GstPad *tee_capture_pad);
  bool CreateRecordElements(GstPad *tee_record_pad);
  int GetFileIndex(const std::string& record_path);
  bool LoadYUY2Pipeline();
  bool LoadJPEGPipeline();
  int32_t ConvertErrorCode(GQuark domain, gint code);
  base::error_t HandleErrorMessage(GstMessage *message);

  static void FeedData(GstElement * appsrc, guint size, gpointer gdata);
  static GstFlowReturn GetSample(GstAppSink *elt, gpointer data);
  static GstPadProbeReturn CaptureRemoveProbe(GstPad * pad,
                                                GstPadProbeInfo * info,
                                                gpointer user_data);
  static GstPadProbeReturn RecordRemoveProbe(GstPad * pad,
                                               GstPadProbeInfo * info,
                                               gpointer user_data);

  int32_t planeId_, shmkey_, width_, height_, framerate_, crtcId_, connId_,
            display_path_idx_;
  int  num_of_images_to_capture_, num_of_captured_images_;
  std::string uri_, memtype_, memsrc_, format_, capture_path_, record_path_;
  GstElement *pipeline_, *source_, *parser_, *decoder_, *filter_YUY2_,
             *filter_I420_, *filter_JPEG_, *vconv_, *tee_, *capture_queue_,
             *capture_encoder_, *capture_sink_, *record_queue_,
             *record_encoder_, *record_decoder_, *record_mux_, *record_sink_,
             *preview_queue_, *preview_sink_;
  GstPad *tee_preview_pad_, *preview_queue_pad_,
         *capture_queue_pad_, *tee_capture_pad_, *record_queue_pad_,
         *tee_record_pad_;
  GstAppSrcContext context_ ;
  base::source_info_t source_info_;
  std::shared_ptr<cmp::resource::ResourceRequestor> res_requestor_;
  base::playback_state_t current_state_;
  GstBus *bus_;
  GstCaps *caps_YUY2_, *caps_I420_, *caps_JPEG_;
  cmp::service::IService *service_;
  bool load_complete_;
};

}  // namespace player
}  // namespace cmp
#endif  // SRC_CAMERA_PLAYER_H_
