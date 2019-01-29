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


#include <base/base.h>
#include "serializer.h"

namespace cmp { namespace parser {

template<>
pbnjson::JValue to_json(const base::result_t & result) {
  return pbnjson::JObject {{"state", result.state},
                           {"mediaId", result.mediaId}};
}

template<>
pbnjson::JValue to_json(const base::video_info_t & info) {
  return pbnjson::JObject {{"codec", info.codec},
               {"bitrate", (int64_t)info.bit_rate},
               {"width", (int32_t)info.width},
               {"height", (int32_t)info.height},
               {"frame_rate", pbnjson::JObject {{"num", info.frame_rate.num},
                                {"den", info.frame_rate.den}}}};
}

template<>
pbnjson::JValue to_json(const base::source_info_t & info) {
  pbnjson::JArray programs;
  for (const auto & program : info.programs)
    programs.put(programs.arraySize(), pbnjson::JObject {
                              {"video_stream", (int32_t)program.video_stream}
                              });
  pbnjson::JArray video_streams;
  for (const auto & video_stream : info.video_streams)
    video_streams.put(video_streams.arraySize(), to_json(video_stream));

  return pbnjson::JObject {{"container", info.container},
               {"programs", programs},
               {"video_streams", video_streams}};
}

template<>
pbnjson::JValue to_json(const base::error_t & error) {
  return pbnjson::JObject {{"errorCode", error.errorCode},
               {"errorText", error.errorText},
               {"mediaId", error.mediaId}};
}

Composer::Composer() : _dom(pbnjson::JObject()) {}

std::string Composer::result() {
  std::string result;
  pbnjson::JGenerator(nullptr).toString(_dom,
                pbnjson::JSchemaFragment("{}"), result);
  return result;
}

}  // namespace parser
}  // namespace cmp
