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


#ifndef SRC_BASE_MESSAGE_H_
#define SRC_BASE_MESSAGE_H_

typedef enum MEDIA_MESSAGE {
  MEDIA_MSG_MSG_NONE = 0x00,                         /**< no message */
  MEDIA_MSG_ERR_LOAD = 0x0068,
  MEDIA_MSG_ERR_POLICY = 0x0259,
  MEDIA_MSG_ERR_PLAYING = 0xf000,

  /* gstrreamer core error msg */
  MEDIA_MSG_START_GST_MSG = 0xf100,              // d16
  CMP_MSG__GST_CORE_ERROR_FAILED,              // a general error which doesn't fit in any other category. Make sure you add a custom message to the error call.
  CMP_MSG__GST_CORE_ERROR_TOO_LAZY,            // do not use this except as a placeholder for deciding where to go while developing code.
  CMP_MSG__GST_CORE_ERROR_NOT_IMPLEMENTED,     // use this when you do not want to implement this functionality yet.
  CMP_MSG__GST_CORE_ERROR_STATE_CHANGE,        // used for state change errors.
  CMP_MSG__GST_CORE_ERROR_PAD,                 // used for pad-related errors.
  CMP_MSG__GST_CORE_ERROR_THREAD,              // used for thread-related errors.
  CMP_MSG__GST_CORE_ERROR_NEGOTIATION,         // used for negotiation-related errors.
  CMP_MSG__GST_CORE_ERROR_EVENT,               // used for event-related errors.
  CMP_MSG__GST_CORE_ERROR_CAPS,                // used for caps-related errors.
  CMP_MSG__GST_CORE_ERROR_TAG,                 // used for negotiation-related errors.
  CMP_MSG__GST_CORE_ERROR_MISSING_PLUGIN,      // used if a plugin is missing.
  CMP_MSG__GST_CORE_ERROR_CLOCK,               // used for clock related errors.
  CMP_MSG__GST_CORE_ERROR_DISABLED,            // d30 // used if functionality has been disabled at compile time (Since: 0.10.13).
  /* gstreamer library error msg */
  CMP_MSG__GST_LIBRARY_ERROR_FAILED,           // a general error which doesn't fit in any other category. Make sure you add a custom message to the error call.
  CMP_MSG__GST_LIBRARY_ERROR_TOO_LAZY,         // do not use this except as a placeholder for deciding where to go while developing code.
  CMP_MSG__GST_LIBRARY_ERROR_INIT,             // used when the library could not be opened.
  CMP_MSG__GST_LIBRARY_ERROR_SHUTDOWN,         // used when the library could not be closed.
  CMP_MSG__GST_LIBRARY_ERROR_SETTINGS,         // used when the library doesn't accept settings.
  CMP_MSG__GST_LIBRARY_ERROR_ENCODE,           // used when the library generated an encoding error.
  /* gstreamer resource error msg */
  CMP_MSG__GST_RESOURCE_ERROR_FAILED,          // a general error which doesn't fit in any other category. Make sure you add a custom message to the error call.
  CMP_MSG__GST_RESOURCE_ERROR_TOO_LAZY,        // do not use this except as a placeholder for deciding where to go while developing code.
  CMP_MSG__GST_RESOURCE_ERROR_NOT_FOUND,       // used when the resource could not be found.
  CMP_MSG__GST_RESOURCE_ERROR_BUSY,            // d40 // used when resource is busy.
  CMP_MSG__GST_RESOURCE_ERROR_OPEN_READ,       // used when resource fails to open for reading.
  CMP_MSG__GST_RESOURCE_ERROR_OPEN_WRITE,      // used when resource fails to open for writing.
  CMP_MSG__GST_RESOURCE_ERROR_OPEN_READ_WRITE,  // used when resource cannot be opened for both reading and writing, or either (but unspecified which).
  CMP_MSG__GST_RESOURCE_ERROR_CLOSE,           // used when the resource can't be closed.
  CMP_MSG__GST_RESOURCE_ERROR_READ,            // used when the resource can't be read from.
  CMP_MSG__GST_RESOURCE_ERROR_WRITE,           // used when the resource can't be written to.
  CMP_MSG__GST_RESOURCE_ERROR_SYNC,            // used when a synchronize on the resource fails.
  CMP_MSG__GST_RESOURCE_ERROR_SETTINGS,        // used when settings can't be manipulated on.
  CMP_MSG__GST_RESOURCE_ERROR_NO_SPACE_LEFT,   // d50 // used when the resource has no space left.
  /* gstreamer stream error msg */
  CMP_MSG__GST_STREAM_ERROR_FAILED,            // a general error which doesn't fit in any other category. Make sure you add a custom message to the error call.
  CMP_MSG__GST_STREAM_ERROR_TOO_LAZY,          // do not use this except as a placeholder for deciding where to go while developing code.
  CMP_MSG__GST_STREAM_ERROR_NOT_IMPLEMENTED,   // use this when you do not want to implement this functionality yet.
  CMP_MSG__GST_STREAM_ERROR_TYPE_NOT_FOUND,    // used when the element doesn't know the stream's type.
  CMP_MSG__GST_STREAM_ERROR_WRONG_TYPE,        // used when the element doesn't handle this type of stream.
  CMP_MSG__GST_STREAM_ERROR_CODEC_NOT_FOUND,   // used when there's no codec to handle the stream's type.
  CMP_MSG__GST_STREAM_ERROR_DECODE,            // used when decoding fails.
  CMP_MSG__GST_STREAM_ERROR_ENCODE,            // used when encoding fails.
  CMP_MSG__GST_STREAM_ERROR_DEMUX,             // used when demuxing fails.
  CMP_MSG__GST_STREAM_ERROR_MUX,               // used when muxing fails.
  CMP_MSG__GST_STREAM_ERROR_FORMAT,            // used when the stream is of the wrong format (for example, wrong caps).
  CMP_MSG__GST_STREAM_ERROR_DECRYPT,           // used when the stream is encrypted and can't be decrypted because this is not supported by the element. (Since: 0.10.20)
  CMP_MSG__GST_STREAM_ERROR_DECRYPT_NOKEY,     // used when the stream is encrypted and can't be decrypted because no suitable key is available. (Since: 0.10.20)
  MEDIA_MSG_END_GST_MSG,

  MEDIA_CB_MSG_LAST
} MEDIA_MESSAGE_T;

typedef enum CMP_MEDIA_STATUS {
  CMP_MEDIA_OK = 0,
  CMP_MEDIA_ERROR = -1,
  CMP_MEDIA_NOT_IMPLEMENTED = -2,
  CMP_MEDIA_NOT_SUPPORTED = -6,
  CMP_MEDIA_BUFFER_FULL = -7,                         /**< function doesn't works cause buffer is full */
  CMP_MEDIA_INVALID_PARAMS = -3,                      /**< Invalid parameters */
  CMP_MEDIA_NOT_READY = -11,                          /**< API's resource is not ready */
} CMP_MEDIA_STATUS_T;

/* video codec */
typedef enum {
  CMP_PIXEL_NONE,
  CMP_PIXEL_I420,
  CMP_PIXEL_YUY2,
  CMP_PIXEL_YUYV,
} CMP_PIXEL_FMT;

/* video codec */
typedef enum {
  CMP_VIDEO_CODEC_NONE,
  CMP_VIDEO_CODEC_H264,
  CMP_VIDEO_CODEC_VC1,
  CMP_VIDEO_CODEC_MPEG2,
  CMP_VIDEO_CODEC_MPEG4,
  CMP_VIDEO_CODEC_THEORA,
  CMP_VIDEO_CODEC_VP8,
  CMP_VIDEO_CODEC_VP9,
  CMP_VIDEO_CODEC_H265,
  CMP_VIDEO_CODEC_MJPEG,
  CMP_VIDEO_CODEC_MAX = CMP_VIDEO_CODEC_MJPEG,
} CMP_VIDEO_CODEC;

/**
 * Data structure for encoding parameters
 */
typedef struct ENCODING_PARAMS {
  gint32 bitRate;
  guint32 frameRate;
} ENCODING_PARAMS_T;

/**
 * Load data structure for Buffer Player
 */
typedef struct ENCODER_INIT_DATA {
  /* config for video */
  guint32 frameRate;
  guint32 width;
  guint32 height;
  CMP_PIXEL_FMT pixelFormat;
  CMP_VIDEO_CODEC codecFormat;
} ENCODER_INIT_DATA_T;

/**
 * Data structure for encoding parameters
 */
typedef struct ENCODING_ERRORS {
  gint32 errorCode;
  gchar* errorStr;
} ENCODING_ERRORS_T;

typedef struct {
  guint srcIdx;
  guint bufferMinByte;
  guint bufferMaxByte;
  guint bufferMinPercent;
} CMP_SRC_T;

typedef struct {
  bool isKeyFrame;
  guint32 frameHeight;
  guint32 frameWidth;
  guint32 bufferSize;
  guint32 frameRate;
  guint64 timeStamp;
  uint8_t* encodedBuffer;
  CMP_VIDEO_CODEC videoCodec;
} ENCODED_BUFFER_T;

typedef enum {
  ENCODER_CB_LOAD_COMPLETE = 0,
  ENCODER_CB_NOTIFY_PLAYING,
  ENCODER_CB_NOTIFY_PAUSED,
  ENCODER_CB_BUFFER_ENCODED,
  ENCODER_CB_NOTIFY_EOS,
  ENCODER_CB_NOTIFY_ERROR,
  ENCODER_CB_SOURCE_INFO,
  ENCODER_CB_UNLOAD_COMPLETE,
  ENCODER_CB_TYPE_MAX = ENCODER_CB_UNLOAD_COMPLETE,
} ENCODER_CB_TYPE_T;

using ENCODER_CALLBACK_T = std::function<void(
    const gint type, const void* cbData, void *userData)>;

#endif  // SRC_BASE_MESSAGE_H_
