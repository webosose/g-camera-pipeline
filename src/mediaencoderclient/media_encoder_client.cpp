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


#include <functional>
#include <vector>
#include <map>

#include <log/log.h>

#include "media_encoder_client.h"
#include "base/message.h"
#include "parser/serializer.h"
#include "parser/parser.h"
#include "bufferencoder/buffer_encoder.h"

namespace cmp { namespace player {

bool MediaEncoderClient::IsCodecSupported(CMP_VIDEO_CODEC videoCodec) {

  return (videoCodec == CMP_VIDEO_CODEC_H264);
}

MediaEncoderClient::MediaEncoderClient() {
  CMP_INFO_PRINT("%d %s", __LINE__, __FUNCTION__);
}

MediaEncoderClient::~MediaEncoderClient() {
  CMP_INFO_PRINT("%d %s", __LINE__, __FUNCTION__);
}

void MediaEncoderClient::RegisterCallback(
    ENCODER_CALLBACK_T callback, void *uData) {
  CMP_INFO_PRINT("%d %s", __LINE__, __FUNCTION__);
  callback_ = callback;
  userData_ = uData;
}

bool MediaEncoderClient::Init(const ENCODER_INIT_DATA_T* loadData) {
  CMP_INFO_PRINT("%d %s", __LINE__, __FUNCTION__);
  CMP_INFO_PRINT("Load loadData = %p", loadData);
  CMP_INFO_PRINT("Load width = %d", loadData->width);
  CMP_INFO_PRINT("Load height = %d", loadData->height);
  CMP_INFO_PRINT("Load FrameRate = %d", loadData->frameRate);
  CMP_INFO_PRINT("Load pixelFormat = %d", loadData->pixelFormat);

  bufferEncoder = std::make_shared<cmp::player::BufferEncoder>();
  if (!bufferEncoder) {
    CMP_INFO_PRINT("Encoder Buffer not created");
    return false;
  }
  bool ret = bufferEncoder -> init(loadData);
  bufferEncoder->RegisterCbFunction (
          std::bind(&MediaEncoderClient::Notify, this ,
              std::placeholders::_1, std::placeholders::_2,
              std::placeholders::_3, std::placeholders::_4));
  bufferEncoder -> RegisterCallBack(
          std::bind(&MediaEncoderClient::OnEncodedDataAvailable, this,
              std::placeholders::_1, std::placeholders::_2));

  return ret;
}

bool MediaEncoderClient::Deinit() {
  CMP_INFO_PRINT("%d %s", __LINE__, __FUNCTION__);
  return bufferEncoder->deinit();
}

int MediaEncoderClient::Encode(const uint8_t* bufferPtr, size_t bufferSize) {
  if (!bufferEncoder) {
    CMP_INFO_PRINT("Invalid state, player(%p) should be loaded", bufferEncoder.get());
    return false;
  }
  return bufferEncoder->feed(bufferPtr, bufferSize);
}

int MediaEncoderClient::Encode(const uint8_t* yBuffer, const uint8_t* uBuffer,
                               const uint8_t* vBuffer, guint32 bufferSize) {
  if (!bufferEncoder) {
    CMP_INFO_PRINT("Invalid state, player(%p) should be loaded", bufferEncoder.get());
    return false;
  }
  return bufferEncoder->feed(yBuffer, uBuffer, vBuffer, bufferSize);
}

bool MediaEncoderClient::OnEncodedDataAvailable(uint8_t* buffer, ENCODED_BUFFER_T* encData) {
  bool res;

  if (nullptr != callback_) {
    encData->encodedBuffer = buffer;
    callback_(ENCODER_CB_BUFFER_ENCODED, encData, userData_);
    return true;
  }
  return false;
}

void MediaEncoderClient::Notify(const gint notification, const gint64 numValue,
        const gchar *strValue, void *payload)
{
  CMP_INFO_PRINT("%d %s", __LINE__, __FUNCTION__);
  if (nullptr != callback_) {
    callback_(notification, payload, userData_);
    return;
  }
}

bool MediaEncoderClient::UpdateEncodingParams(const ENCODING_PARAMS_T* properties)
{
  //Dynamic updationof resolution, bitrate and framerate needs to be handled
  return true;
}

} //End of player
}  // End of namespace cmp
