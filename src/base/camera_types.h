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

#ifndef SRC_CAMERA_TYPES_H_
#define SRC_CAMERA_TYPES_H_

#include <gst/app/gstappsink.h>
#include <gst/app/gstappsrc.h>
#include <gst/gst.h>
#include <gst/pbutils/pbutils.h>
#include <gst/player/player.h>

typedef struct
{
    GMainLoop *loop;
    GstElement *source;
    GstElement *sink;
    char *shmpointer;
    int shmemid;
} ProgramData;

typedef enum
{
    CMP_ERROR_NONE,
    CMP_ERROR_STREAM,
    CMP_ERROR_ASYNC,
    CMP_ERROR_RES_ALLOC,
    CMP_ERROR_MAX
} CMP_ERROR_CODE;

typedef enum
{
    CMP_DEFAULT_DISPLAY = 0,
    CMP_PRIMARY_DISPLAY = 0,
    CMP_SECONDARY_DISPLAY,
} CMP_DISPLAY_PATH;

typedef enum
{
    CMP_NOTIFY_LOAD_COMPLETED = 0,
    CMP_NOTIFY_UNLOAD_COMPLETED,
    CMP_NOTIFY_SOURCE_INFO,
    CMP_NOTIFY_END_OF_STREAM,
    CMP_NOTIFY_PLAYING,
    CMP_NOTIFY_PAUSED,
    CMP_NOTIFY_ERROR,
    CMP_NOTIFY_VIDEO_INFO,
    CMP_NOTIFY_ACTIVITY,
    CMP_NOTIFY_ACQUIRE_RESOURCE,
    CMP_NOTIFY_MAX
} CMP_NOTIFY_TYPE_T;

/* player status enum type */
typedef enum
{
    CMP_LOADING_STATE,
    CMP_STOPPED_STATE,
    CMP_PAUSING_STATE,
    CMP_PAUSED_STATE,
    CMP_PLAYING_STATE,
    CMP_PLAYED_STATE,
} CMP_PIPELINE_STATE;

#endif
