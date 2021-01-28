// Copyright (c) 2020-2021 LG Electronics, Inc.
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

#include "buffer_encoder.h"
#include "parser/serializer.h"
#include "parser/parser.h"

#include <string.h>
#include <stdlib.h>
#include <glib.h>
#include <cmath>
#include <cstring>
#include <sstream>
#include <map>
#include <memory>
#include <gio/gio.h>

#include <gst/gst.h>
#include <gst/video/video.h>
#include <gst/app/gstappsrc.h>
#include <gst/base/gstbasesrc.h>
#include "resourcefacilitator/requestor.h"

#include <log/log.h>

const std::string kFormatYUV = "YUY2";
#define CURR_TIME_INTERVAL_MS    100
#define LOAD_DONE_TIMEOUT_MS     10

#define MEDIA_VIDEO_MAX      (15 * 1024 * 1024)  // 15MB
#define QUEUE_MAX_SIZE       (12 * 1024 * 1024)  // 12MB
#define QUEUE_MAX_TIME       (10 * GST_SECOND)   // 10Secs

#define BUFFER_MIN_PERCENT 50
#define MEDIA_CHANNEL_MAX  2


namespace cmp {
namespace player {

BufferEncoder::BufferEncoder():
  bus_(nullptr),
  load_complete_(false),
  pipeline_(nullptr),
  source_(nullptr),
  filter_YUY2_(nullptr),
  parse_(nullptr),
  converter_(nullptr),
  filter_NV12_(nullptr),
  encoder_(nullptr),
  sink_(nullptr),
  caps_YUY2_(nullptr),
  caps_NV12_(nullptr),
  encdata_{FALSE,0,0,0,0,0,nullptr,CMP_VIDEO_CODEC_NONE},
  userData(nullptr){
  CMP_INFO_PRINT("%d %s", __LINE__, __FUNCTION__);
}

BufferEncoder::~BufferEncoder() {
  CMP_INFO_PRINT("%d %s", __LINE__, __FUNCTION__);
  gst_element_set_state(pipeline_, GST_STATE_NULL);
}

bool BufferEncoder::deinit() {
  CMP_INFO_PRINT("%d %s", __LINE__, __FUNCTION__);
  resourceRequestor_->releaseResource();
  return true;
}

bool BufferEncoder::GetSourceInfo(const ENCODER_INIT_DATA_T* loadData)
{
    base::video_info_t video_stream_info = {};

    video_stream_info.width = loadData->width;
    video_stream_info.height = loadData->height;
    video_stream_info.encode = loadData->codecFormat;
    video_stream_info.decode = loadData->codecFormat;
    video_stream_info.frame_rate.num = loadData->frameRate;
    video_stream_info.frame_rate.den = 1;
    CMP_DEBUG_PRINT("[video info] width: %d, height: %d, frameRate: %d/%d",
            video_stream_info.width, video_stream_info.height,
            video_stream_info.frame_rate.num, video_stream_info.frame_rate.den);

    base::program_info_t program;
    program.video_stream = 1;
    source_info_.programs.push_back(program);

    source_info_.video_streams.push_back(video_stream_info);

    return true;
}

void BufferEncoder::Notify(const gint notification, const gint64 numValue,
        const gchar *strValue, void *payload)
{
    cmp::parser::Composer composer;
    cmp::base::media_info_t mediaInfo = { media_id_ };
    switch (notification)
    {
        case CMP_NOTIFY_SOURCE_INFO:
        {
            base::source_info_t info  = *static_cast<base::source_info_t *>(payload);
            composer.put("sourceInfo", info);
            break;
        }

        case CMP_NOTIFY_VIDEO_INFO:
        {
            base::video_info_t info = *static_cast<base::video_info_t*>(payload);
            composer.put("videoInfo", info);
            CMP_INFO_PRINT("videoInfo: width %d, height %d", info.width, info.height);
            break;
        }
        case CMP_NOTIFY_ERROR:
        {
            base::error_t error = *static_cast<base::error_t *>(payload);
            error.mediaId = media_id_;
            composer.put("error", error);

            if (numValue == CMP_ERROR_RES_ALLOC) {
                CMP_DEBUG_PRINT("policy action occured!");
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
        case CMP_NOTIFY_ACTIVITY: {
            CMP_DEBUG_PRINT("notifyActivity to resource requestor");
            if (resourceRequestor_)
                resourceRequestor_->notifyActivity();
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

void BufferEncoder::LoadCommon()
{
    if (!resourceRequestor_)
        CMP_DEBUG_PRINT("NotifyForeground fails");
    else
        resourceRequestor_->notifyForeground();


    if (resourceRequestor_) {
        resourceRequestor_->registerUMSPolicyActionCallback([this]() {
            base::error_t error;
            error.errorCode = MEDIA_MSG_ERR_POLICY;
            error.errorText = "Policy Action";
            Notify(CMP_NOTIFY_ERROR, CMP_ERROR_RES_ALLOC,
                                    nullptr, static_cast<void*>(&error));
            if (!resourceRequestor_)
                CMP_DEBUG_PRINT("notifyBackground fails");
            else
                resourceRequestor_->notifyBackground();
            });
    }
}

bool BufferEncoder::init(const ENCODER_INIT_DATA_T* loadData) {
  CMP_INFO_PRINT("%d %s", __LINE__, __FUNCTION__);
  cmp::resource::PortResource_t resourceMMap;
  ACQUIRE_RESOURCE_INFO_T resource_info;
  std::string display_mode_ = std::string("Textured");
  uint32_t display_path_ = CMP_DEFAULT_DISPLAY;

  if (loadData->codecFormat == CMP_VIDEO_CODEC_H264)
  {
    umc_ = std::make_unique<UMSConnector>("bufferEncoder", nullptr, nullptr,
            UMS_CONNECTOR_PRIVATE_BUS);

    resourceRequestor_ = std::make_unique<cmp::resource::ResourceRequestor>
                                              (app_id_, media_id_);
    if (!GetSourceInfo(loadData))
    {
      CMP_DEBUG_PRINT("get source information failed!");
      return false;
    }
    resource_info.sourceInfo = &source_info_;
    resource_info.displayMode = const_cast<char*>(display_mode_.c_str());
    resource_info.result = true;

    LoadCommon();

    if (resourceRequestor_)
    {
      if (!resourceRequestor_->acquireResources(
                  resourceMMap, source_info_, display_mode_, display_path_))
      {
        CMP_INFO_PRINT("resource acquisition failed");
        return false;
      }

      for (auto it : resourceMMap)
      {
        CMP_DEBUG_PRINT("Resource::[%s]=>index:%d", it.first.c_str(), it.second);
      }
    }
  }
  else
  {
    return false;
  }

  if (!CreatePipeline(loadData)) {
    CMP_INFO_PRINT("CreatePipeline Failed");
    return false;
  }
  encdata_.frameHeight = loadData->height;
  encdata_.frameWidth = loadData->width;
  encdata_.frameRate = loadData->frameRate;
  return true;
}

/* called when the appsink notifies us that there is a new buffer ready for
 * processing */
GstFlowReturn
BufferEncoder::on_new_sample_from_sink (GstElement * elt, ProgramData * data)
{
  CMP_INFO_PRINT("%d %s", __LINE__, __FUNCTION__);
  BufferEncoder *encoder = reinterpret_cast<BufferEncoder*>(data);
  GstSample *sample;
  GstBuffer *app_buffer, *buffer;
  GstElement *source;
  GstFlowReturn ret;

  /* get the sample from appsink */
  sample = gst_app_sink_pull_sample (GST_APP_SINK (elt));
  if (NULL != sample) {
    GstMapInfo map = {};
    GstBuffer *buffer;
    buffer = gst_sample_get_buffer(sample);
    gst_buffer_map(buffer, &map, GST_MAP_READ);
    CMP_INFO_PRINT("%d %s data size:%lu", __LINE__, __FUNCTION__, map.size);

    if(!GST_BUFFER_FLAG_IS_SET(buffer, GST_BUFFER_FLAG_DELTA_UNIT)){
      encoder->encdata_.isKeyFrame = true;
    } else {
      encoder->encdata_.isKeyFrame = false;
    }
    encoder->encdata_.videoCodec = CMP_VIDEO_CODEC_H264;
    if ((NULL != map.data) && (map.size != 0)) {
      encoder->encdata_.bufferSize = map.size;
      encoder->encdata_.timeStamp = GST_BUFFER_TIMESTAMP (buffer);
      CMP_INFO_PRINT("%d %s data size:%lu", __LINE__, __FUNCTION__, map.size);
      encoder->SendBackEncodedData(map.data, &encoder->encdata_);
    }
    gst_sample_unref(sample);
    gst_buffer_unmap(buffer, &map);
  }
  return GST_FLOW_OK;
}

void BufferEncoder::RegisterCallBack(FunctorEncoder callback) {
  CMP_INFO_PRINT("%d %s", __LINE__, __FUNCTION__);
  callback_ = callback;
}

int BufferEncoder::feed(const uint8_t* bufferPtr, size_t bufferSize) {
  CMP_INFO_PRINT("%d %s", __LINE__, __FUNCTION__);

  if (!pipeline_) {
    CMP_INFO_PRINT("Pipeline is null");
    return CMP_MEDIA_ERROR;
  }

  guint8 *feedBuffer = (guint8 *)g_malloc(bufferSize);
  if (feedBuffer == NULL) {
    CMP_DEBUG_PRINT("memory allocation error!!!!!");
    return CMP_MEDIA_ERROR;
  }

  memcpy(feedBuffer, bufferPtr, bufferSize);

  CMP_INFO_PRINT("bufferPtr(%p) length:%lu\n", feedBuffer, bufferSize);
  GstBuffer *gstBuffer = gst_buffer_new_wrapped(feedBuffer, bufferSize);
  if (!gstBuffer) {
    CMP_INFO_PRINT("Buffer wrapping error");
    return CMP_MEDIA_ERROR;
  }

  GstFlowReturn gstReturn = gst_app_src_push_buffer((GstAppSrc*)source_,
                                                    gstBuffer);
  if (gstReturn < GST_FLOW_OK) {
    CMP_INFO_PRINT("gst_app_src_push_buffer errCode[ %d ]", gstReturn);
    return CMP_MEDIA_ERROR;
  }
  return CMP_MEDIA_OK;
}

int BufferEncoder::feed(const uint8_t* yBuffer, const uint8_t* uBuffer,
                        const uint8_t* vBuffer, guint32 bufferSize){
  if (!pipeline_) {
    CMP_INFO_PRINT("Pipeline is null");
    return CMP_MEDIA_ERROR;
  }
  uint32_t y_size = bufferSize;
  uint32_t uv_size = y_size / 4;

  GstBuffer *buf = gst_buffer_new_allocate(NULL, bufferSize * sizeof(uint8_t) * 1.5, NULL);
  GstMapInfo writeBufferMap;
  gboolean bcheck = gst_buffer_map(buf, &writeBufferMap, GST_MAP_WRITE);
  memcpy(writeBufferMap.data, yBuffer, bufferSize);
  memcpy(writeBufferMap.data+ y_size, uBuffer, uv_size);
  memcpy(writeBufferMap.data+ y_size+ uv_size, vBuffer, uv_size);
  gst_buffer_unmap(buf, &writeBufferMap);
  gst_app_src_push_buffer((GstAppSrc*)source_, buf);
  return true;
}

bool BufferEncoder::CreateEncoder(CMP_VIDEO_CODEC codecFormat) {
  CMP_INFO_PRINT("%d %s", __LINE__, __FUNCTION__);

  if (codecFormat == CMP_VIDEO_CODEC_H264) {
    encoder_ = gst_element_factory_make ("omxh264enc", "encoder");
  } else {
    CMP_INFO_PRINT("%d %s ==> Unsupported Codedc", __LINE__, __FUNCTION__);
    return false;
  }

  if (!encoder_) {
    CMP_INFO_PRINT("encoder_ element creation failed.");
    return false;
  }
  return true;
}

bool BufferEncoder::SendBackEncodedData(uint8_t* buffer,
                                        ENCODED_BUFFER_T* encData) {
  bool res;
  if (nullptr != callback_) {
    res = callback_(buffer, encData);
    return true;
  }
  return false;
}

bool BufferEncoder::CreateSink() {
  CMP_INFO_PRINT("%d %s", __LINE__, __FUNCTION__);
  sink_ = gst_element_factory_make ("appsink", "sink");
  if (!sink_) {
    CMP_INFO_PRINT("sink_ element creation failed.");
    return false;
  }
  g_object_set (G_OBJECT (sink_), "emit-signals", TRUE, "sync", FALSE, NULL);
  g_signal_connect(sink_, "new-sample", G_CALLBACK(on_new_sample_from_sink), this);

  return true;
}

bool BufferEncoder::LinkElements(const ENCODER_INIT_DATA_T* loadData) {
  CMP_INFO_PRINT("%d %s, width: %d, height: %d", __LINE__, __FUNCTION__, loadData->width, loadData->height);

  filter_YUY2_ = gst_element_factory_make("capsfilter", "filter-YUY2");
  if (!filter_YUY2_) {
    CMP_INFO_PRINT("filter_YUY2_(%p) Failed", filter_YUY2_);
    return false;
  }

  caps_YUY2_ = gst_caps_new_simple("video/x-raw",
                                   "width", G_TYPE_INT, loadData->width,
                                   "height", G_TYPE_INT, loadData->height,
                                   "framerate", GST_TYPE_FRACTION, loadData->frameRate, 1,
                                   "format", G_TYPE_STRING, "I420",
                                   NULL);
  g_object_set(G_OBJECT(filter_YUY2_), "caps", caps_YUY2_, NULL);

  filter_NV12_ = gst_element_factory_make("capsfilter", "filter-NV");
  if (!filter_NV12_) {
    CMP_INFO_PRINT("filter_ element creation failed.");
    return false;
  }

  caps_NV12_ = gst_caps_new_simple("video/x-raw",
                                   "format", G_TYPE_STRING, "NV12",
                                   NULL);
  g_object_set(G_OBJECT(filter_NV12_), "caps", caps_NV12_, NULL);

  converter_ = gst_element_factory_make("videoconvert", "converted");
  if (!converter_) {
    CMP_INFO_PRINT("converter_(%p) Failed", converter_);
    return false;
  }

  parse_ = gst_element_factory_make("rawvideoparse", "parser");
  if (!parse_) {
    CMP_INFO_PRINT("parse_(%p) Failed", parse_);
    return false;
  }

  g_object_set(G_OBJECT(parse_), "width", loadData->width, NULL);
  g_object_set(G_OBJECT(parse_), "height", loadData->height, NULL);
  if (CMP_PIXEL_I420 == loadData->pixelFormat) {
    g_object_set(G_OBJECT(parse_), "format", 2, NULL);
  }

  gst_bin_add_many(GST_BIN(pipeline_), source_, filter_YUY2_, parse_, converter_, filter_NV12_, encoder_, sink_, NULL);
  CMP_INFO_PRINT(" BufferEncoder elements added to bin  \n ");

  if (TRUE != gst_element_link(source_, filter_YUY2_)) {
    CMP_INFO_PRINT ("elements could not be linked - source_ & filter_YUY2 \n");
    return false;
  }

  if (TRUE != gst_element_link(filter_YUY2_, parse_)) {
    CMP_INFO_PRINT ("elements could not be linked - filter_YUY2 & converter_ \n");
      return false;
  }

  if (TRUE != gst_element_link(parse_, converter_)) {
    CMP_INFO_PRINT ("elements could not be linked - parse_ & converter_ \n");
    return false;
  }

  if (TRUE != gst_element_link(converter_, filter_NV12_)) {
    CMP_INFO_PRINT ("elements could not be linked - converter_ & filter_NV12_ \n");
    return false;
  }

  if (TRUE != gst_element_link(filter_NV12_, encoder_)) {
    CMP_INFO_PRINT ("elements could not be linked - filter_NV12_ & encoder_ \n");
    return false;
  }

  if (TRUE != gst_element_link(encoder_, sink_)) {
    CMP_INFO_PRINT ("elements could not be linked - encoder_ & sink_ \n");
    return false;
  }

  return true;
}

void BufferEncoder::RegisterCbFunction(CALLBACK_T callBackFunction)
{
  CMP_INFO_PRINT("%d %s", __LINE__, __FUNCTION__);
  cbFunction_ = callBackFunction;
}

int32_t BufferEncoder::ConvertErrorCode(GQuark domain, gint code)
{
  CMP_INFO_PRINT("%d %s", __LINE__, __FUNCTION__);
  int32_t converted = MEDIA_MSG_ERR_PLAYING;

  if (GST_CORE_ERROR == domain) {
    switch (code) {
      case GST_CORE_ERROR_EVENT:
        converted = CMP_MSG__GST_CORE_ERROR_EVENT;
        break;

      default:
        break;
    }
  } else if (GST_LIBRARY_ERROR == domain) {
      // do nothing
  } else if (GST_RESOURCE_ERROR == domain) {
    switch (code) {
      case GST_RESOURCE_ERROR_SETTINGS:
        converted = CMP_MSG__GST_RESOURCE_ERROR_SETTINGS;
        break;

      case GST_RESOURCE_ERROR_NOT_FOUND:
        converted = CMP_MSG__GST_RESOURCE_ERROR_NOT_FOUND;
        break;

      case GST_RESOURCE_ERROR_OPEN_READ:
        converted = CMP_MSG__GST_RESOURCE_ERROR_OPEN_READ;
        break;

      case GST_RESOURCE_ERROR_READ:
        converted = CMP_MSG__GST_RESOURCE_ERROR_READ;
        break;

      default:
        break;
    }
  } else if (GST_STREAM_ERROR == domain) {
    switch (code) {
      case GST_STREAM_ERROR_TYPE_NOT_FOUND:
        converted = CMP_MSG__GST_STREAM_ERROR_TYPE_NOT_FOUND;
        break;

      case GST_STREAM_ERROR_DEMUX:
        converted = CMP_MSG__GST_STREAM_ERROR_DEMUX;
        break;

      default:
        break;
    }
  }
  return converted;
}

base::error_t BufferEncoder::HandleErrorMessage(GstMessage *message)
{
  CMP_INFO_PRINT("%d %s", __LINE__, __FUNCTION__);
  GError *err = NULL;
  gchar *debug_info;
  gst_message_parse_error(message, &err, &debug_info);
  GQuark domain = err->domain;

  base::error_t error;
  error.errorCode = ConvertErrorCode(domain, (gint)err->code);
  error.errorText = g_strdup(err->message);

  CMP_INFO_PRINT("[GST_MESSAGE_ERROR][domain:%s][from:%s][code:%d]"
                 "[converted:%d][msg:%s]",g_quark_to_string(domain),
                 (GST_OBJECT_NAME(GST_MESSAGE_SRC(message))), err->code, error.errorCode,
                 err->message);
  CMP_INFO_PRINT("Debug information: %s", debug_info ? debug_info : "none");

  g_clear_error(&err);
  g_free(debug_info);

  return error;
}

gboolean BufferEncoder::HandleBusMessage(
    GstBus *bus_, GstMessage *message, gpointer user_data)
{
  CMP_INFO_PRINT("%d %s", __LINE__, __FUNCTION__);
  GstMessageType messageType = GST_MESSAGE_TYPE(message);
  if (messageType != GST_MESSAGE_QOS && messageType != GST_MESSAGE_TAG) {
    CMP_INFO_PRINT("Element[ %s ][ %d ][ %s ]",
                    GST_MESSAGE_SRC_NAME(message),
                    messageType, gst_message_type_get_name(messageType));
  }

  BufferEncoder *encoder = reinterpret_cast<BufferEncoder *>(user_data);
  switch (GST_MESSAGE_TYPE(message)) {
    case GST_MESSAGE_ERROR: {
      if (encoder->cbFunction_) {
        ENCODING_ERRORS_T error;
        error.errorCode = 5;
        error.errorStr =(gchar *)"Dummy Str";
        encoder->cbFunction_(ENCODER_CB_NOTIFY_ERROR, 0, nullptr, &error);
      }
      break;
    }

    case GST_MESSAGE_EOS: {
      CMP_INFO_PRINT("Got endOfStream");
      if (encoder->cbFunction_)
        encoder->cbFunction_(ENCODER_CB_NOTIFY_EOS, 0, nullptr, nullptr);
      break;
    }

    case GST_MESSAGE_ASYNC_DONE: {
      CMP_INFO_PRINT("ASYNC DONE");
      if (!encoder->load_complete_) {
        encoder->cbFunction_(ENCODER_CB_LOAD_COMPLETE, 0, nullptr, nullptr);
        encoder->load_complete_ = true;
      }
      break;
    }

    case GST_STATE_PAUSED: {
      CMP_INFO_PRINT("PAUSED");
      if (encoder->cbFunction_)
        encoder->cbFunction_(ENCODER_CB_NOTIFY_PAUSED, 0, nullptr, nullptr);
      break;
    }

    case GST_STATE_PLAYING: {
      CMP_INFO_PRINT("PLAYING");
      if (encoder->cbFunction_)
        encoder->cbFunction_(ENCODER_CB_NOTIFY_PLAYING, 0, nullptr, nullptr);
      break;
    }

    case GST_MESSAGE_STATE_CHANGED: {
      GstState oldState = GST_STATE_NULL;
      GstState newState = GST_STATE_NULL;
      gst_message_parse_state_changed(message,
                                      &oldState, &newState, NULL);
      CMP_INFO_PRINT("Element[%s] State changed ...%s -> %s",
                      GST_MESSAGE_SRC_NAME(message),
                      gst_element_state_get_name(oldState),
                      gst_element_state_get_name(newState));
      break;
    }

    default:
      break;
  }

  return true;
}

bool BufferEncoder::CreatePipeline(const ENCODER_INIT_DATA_T* loadData) {
  CMP_INFO_PRINT("%d %s", __LINE__, __FUNCTION__);

  gst_init(NULL, NULL);
  gst_pb_utils_init();
  pipeline_ = gst_pipeline_new("buffer-encoder");
  CMP_INFO_PRINT("pipeline_ = %p", pipeline_);
  if (!pipeline_) {
    CMP_INFO_PRINT("Cannot create encoder pipeline!");
    return false;
  }

  source_ = gst_element_factory_make ("appsrc", "app-source");
  if (!source_) {
    CMP_INFO_PRINT("source_ element creation failed.");
    return false;
  }
  g_object_set(source_, "format", GST_FORMAT_TIME, NULL);
  g_object_set(source_, "do-timestamp", true, NULL);

  if (!CreateEncoder(loadData->codecFormat))
  {
    CMP_INFO_PRINT("Encoder creation failed !!!");
    return false;
  }

  if (!CreateSink())
  {
    CMP_INFO_PRINT("Sink creation failed !!!");
    return false;
  }

  if (!LinkElements(loadData))
  {
    CMP_INFO_PRINT("element linking failed !!!");
    return false;
  }

  bus_ = gst_pipeline_get_bus(GST_PIPELINE (pipeline_));
  gst_bus_add_watch(bus_, BufferEncoder::HandleBusMessage, this);
  gst_object_unref(bus_);

  return gst_element_set_state(pipeline_, GST_STATE_PLAYING);
}

}  // namespace player
}  // namespace cmp
