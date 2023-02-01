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

#ifndef SRC_BASE_BASE_H_
#define SRC_BASE_BASE_H_

#include <cstdint>
#include <string>
#include <vector>

namespace cmp { namespace base {

enum class playback_state_t : int {
  STOPPED,
  LOADED,
  PAUSED,
  PLAYING,
};

struct rational_t {
  int32_t num;
  int32_t den;
};

struct stream_info_t {
  int32_t codec;
  int32_t decode;
  int32_t encode;
  uint64_t bit_rate;
};

struct video_info_t : stream_info_t {
  uint32_t width;
  uint32_t height;
  rational_t frame_rate;
};

struct program_info_t {
  uint32_t video_stream;
};

struct source_info_t {
  std::string container;
  std::vector<program_info_t> programs;
  std::vector<video_info_t> video_streams;
};

struct vdec_port_info_t {
  int32_t vdec_port;
  std::string core_type;
};

struct result_t {
  bool state;
  std::string mediaId;
};

struct error_t {
  int32_t errorCode;
  std::string errorText;
  std::string mediaId;
};

struct disp_res_t {
  int32_t plane_id;
  int32_t crtc_id;
  int32_t conn_id;
};

struct media_info_t {
  std::string mediaId;
};

struct load_param_t {
  int32_t displayPath;
  std::string videoDisplayMode;
  std::string windowId;
  std::string uri;
};

}  // namespace base
}  // namespace cmp

#endif  // SRC_BASE_BASE_H_

