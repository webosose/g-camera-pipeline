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

#include <glib.h>
#include <iostream>
#include <vector>
#include <fstream>
#include "string.h"
#include "camshm.h"
#include "media_encoder_client.h"
#include "bufferencoder/buffer_encoder.h"

class MediaEncoderClient;
int main(int argc, char const *argv[])
{
  unsigned char buf[1024];
  ENCODER_INIT_DATA_T loadData;
  FILE *fp = fopen("/var/webrtc_file_1_video.yuv","r");
  // check if the file to read from exists and if so read the file in chunks
  ifstream ifile("/var/webrtc_file_1_video.yuv", std::ifstream::binary);
  const int BUFFER_SIZE = 1024;
  std::vector<char> buffer (BUFFER_SIZE + 1, 0);
  printf("%d:%s:%s",__LINE__, __FUNCTION__, __FILE__);
  cmp::player::MediaEncoderClient *EncoderClient = new cmp::player::MediaEncoderClient();
  EncoderClient ->Init(&loadData);
  printf("%d:%s:%s",__LINE__, __FUNCTION__, __FILE__);
  while(1)
  {
    ifile.read(buffer.data(), BUFFER_SIZE);
    std::streamsize s = ((ifile) ? BUFFER_SIZE : ifile.gcount());
    buffer[s] = 0;
    //Since the feed api is changed to accept three arguments,need to modify this
    //which will be done later since testing can be done now using webRTC
    //EncoderClient ->Feed((unsigned char*)buffer.data(), BUFFER_SIZE);
    if(!ifile) break;
  }
  free(buf);
}
