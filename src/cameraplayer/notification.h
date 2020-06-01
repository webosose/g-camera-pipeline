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

#ifndef SRC_CAMERA_PLAYER_NOTIFICATION_H_
#define SRC_CAMERA_PLAYER_NOTIFICATION_H_


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


#endif  // SRC_CAMERA_PLAYER_NOTIFICATION_H_

