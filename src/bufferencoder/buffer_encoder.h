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


#ifndef SRC_ENCODE_BUFFER_H_
#define SRC_ENCODE_BUFFER_H_

#include "media_encoder_client.h"
#include "camera_types.h"
#include "message.h"
#include <functional>
#include <map>

using namespace std;
using CALLBACK_T = std::function<void(const gint type, const gint64 numValue,
        const gchar *strValue, void *udata)>;
namespace cmp {
namespace player {

class BufferEncoder {
  public:
    BufferEncoder();
    ~BufferEncoder();

    bool init(const ENCODER_INIT_DATA_T* loadData);
    bool deinit();
    int feed(const uint8_t* bufferPtr, size_t bufferSize);
    int feed(const uint8_t* yBuffer, const uint8_t* uBuffer,
        const uint8_t* vBuffer, guint32 bufferSize);
    static gboolean HandleBusMessage(
            GstBus *bus_, GstMessage *message, gpointer user_data);
    void RegisterCbFunction(CALLBACK_T);
    CALLBACK_T cbFunction_ = nullptr;
    void RegisterCallBack(FunctorEncoder callback);
    bool SendBackEncodedData(uint8_t* buffer, ENCODED_BUFFER_T* encData);


  private:
    bool CreatePipeline(const ENCODER_INIT_DATA_T* loadData);
    bool CreateEncoder(CMP_VIDEO_CODEC codecFormat);
    bool CreateSink();
    bool LinkElements(const ENCODER_INIT_DATA_T* loadData);
    base::error_t HandleErrorMessage(GstMessage *message);
    int32_t ConvertErrorCode(GQuark domain, gint code);

    static GstFlowReturn
        on_new_sample_from_sink (GstElement * elt, ProgramData * data);

    GstBus *bus_;
    bool load_complete_;
    GstElement *pipeline_, *source_, *filter_YUY2_, *parse_, *converter_, *filter_NV12_,*encoder_, *sink_;
    GstCaps *caps_YUY2_, *caps_NV12_;
    ENCODED_BUFFER_T encdata_;
    FunctorEncoder callback_;
    void *userData = nullptr;

};

}  // namespace player
}  // namespace cmp

#endif  // SRC_PLAYER_BUFFER_PLAYER_H_
