// Copyright (c) 2020 LG Electronics, Inc.
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


#ifndef SRC_MediaEncoderClient_MediaEncoderClient_H_
#define SRC_MediaEncoderClient_MediaEncoderClient_H_

#include <glib.h>
#include <string>
#include <memory>
#include <functional>

#include "message.h"
#include "base.h"

namespace cmp {

namespace player {

typedef std::function<bool(uint8_t*, ENCODED_BUFFER_T*)> FunctorEncoder;

class BufferEncoder;

class MediaEncoderClient {
  public:
    MediaEncoderClient();
    ~MediaEncoderClient();

    static bool IsCodecSupported(CMP_VIDEO_CODEC videoCodec);
    bool Init(const ENCODER_INIT_DATA_T* loadData);
    bool Deinit();
    int Encode(const uint8_t* bufferPtr, size_t bufferSize);
    int Encode(const uint8_t* yPlane, const uint8_t* uPlane,
               const uint8_t* vPlane, guint32 bufferSize);
    void RegisterCallback(ENCODER_CALLBACK_T callback, void *uData);
    bool UpdateEncodingParams(const ENCODING_PARAMS_T* properties);

  private:
    bool OnEncodedDataAvailable(uint8_t* buffer, ENCODED_BUFFER_T* encData);
    void Notify(const gint notification, const gint64 numValue, const gchar *strValue, void *payload);

    std::string appId_;
    std::string media_id_;
    std::string instanceId_;
    std::string connectionId_;
    ENCODER_CALLBACK_T callback_;

    std::shared_ptr<cmp::player::BufferEncoder> bufferEncoder;
    GMainContext *playerContext_ = nullptr;
    void *userData_ = nullptr;

};

}  // namespace player
}  // namespace cmp

#endif  // _MediaEncoderClient_H_
