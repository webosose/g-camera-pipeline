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

#include <log/log.h>
#include <base/base.h>
#include <base/message.h>
#include <cameraservice/camera_service.h>
#include <gst/pbutils/pbutils.h>
#include <pbnjson.hpp>
#include <sys/types.h>
#include <unistd.h>
#include <gst/gst.h>
#include <gst/app/gstappsrc.h>
#include <gst/app/gstappsink.h>
#include <stdio.h>
#include <semaphore.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/sem.h>
#include <sys/stat.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <pthread.h>

#include "../util/camshm.h"
#include "parser/parser.h"
#include "camera_player.h"

#ifdef CMP_DEBUG_PRINT
#undef CMP_DEBUG_PRINT
#endif
#define CMP_DEBUG_PRINT CMP_INFO_PRINT

#define DISCOVER_EXPIRE_TIME (10 * GST_SECOND)
#define UPDATE_INTERVAL_MS 200

GstAppSrcContext context = {NULL,1,0,0,FALSE,NULL};
static int  numOfImagesToCapture = 0;
static int numOfCapturedImages = 0;
static int num = 0;
#define NUMBEROFIMAGES 1

void writeImageToFile(const void *p,int size)
{
    FILE *fp;
    char image_name[20];

    sprintf(image_name, "/tmp/capture.jpeg");

    if (NULL == (fp = fopen(image_name, "wb")))
    {
    }

    fwrite(p, size, 1, fp);
    fclose(fp);
}

static void
feed_data (GstElement * appsrc, guint size, ProgramData * gdata)
{
    GstFlowReturn ret;
    GstMapInfo writeBufferMap;
    GstBuffer *buf = NULL;
    unsigned char *data = 0;
    int len = 0;
    gboolean bcheck;
    ReadShmem(context.shmemHandle,&data,&len);
    buf = gst_buffer_new_allocate (NULL,len,NULL);
    bcheck = gst_buffer_map (buf, &writeBufferMap, GST_MAP_WRITE);
    memcpy(writeBufferMap.data, data, len);
    gst_buffer_unmap (buf,&writeBufferMap);
    ret = gst_app_src_push_buffer ((GstAppSrc*)appsrc, buf);
}

namespace cmp { namespace player {
static GstPadProbeReturn
capture_remove_probe(GstPad * pad, GstPadProbeInfo * info, gpointer user_data)
{
    GstStateChangeReturn ret;
    gst_pad_unlink (capture_tee_pad_ref, capture_ghost_sinkpad);
    gst_element_remove_pad(capture_bin, capture_queue_pad);
    g_object_unref(capture_queue_pad);
    gst_bin_remove(GST_BIN(pipeline), capture_bin);
    gst_object_unref(capture_bin);
    gst_element_release_request_pad(tee,tee_capture_pad);
    gst_object_unref(capture_tee_pad_ref);
    gst_object_unref(tee_capture_pad);
    tee_capture_pad = NULL;
    capture_tee_pad_ref = NULL;

    gst_pad_remove_probe (pad, GST_PAD_PROBE_INFO_ID (info));

    return GST_PAD_PROBE_OK;

}

    static GstFlowReturn
get_sample (GstAppSink *elt, ProgramData *data)
{
    GstSample *sample;
    GstBuffer *buffer;
    GstMapInfo map;

    if (numOfImagesToCapture == numOfCapturedImages)
    {
        numOfImagesToCapture = 0;
        numOfCapturedImages = 0;
        gst_pad_add_probe(tee_capture_pad, GST_PAD_PROBE_TYPE_IDLE ,
                capture_remove_probe, NULL, NULL);
    }
    else
    {
        sample = gst_app_sink_pull_sample (GST_APP_SINK (elt));
        if (NULL != sample)
        {
            buffer = gst_sample_get_buffer (sample);
            gst_buffer_map (buffer, &map, GST_MAP_READ);
            if ((NULL != map.data) && (map.size != 0))
            {
                writeImageToFile(map.data,map.size);
            }
            gst_sample_unref (sample);
            gst_buffer_unmap (buffer, &map);
            numOfCapturedImages++;
            count++;
        }
    }
    return GST_FLOW_OK;
}

bool create_capture_bin(GstPad * pad)
{
    capture_bin = gst_bin_new ("capture_bin");
    capture_queue = gst_element_factory_make ("queue", "capture_queue");
    capture_encoder = gst_element_factory_make ("jpegenc", "capture_encoder");
    capture_sink = gst_element_factory_make("appsink", NULL);
    if (TRUE != gst_bin_add (GST_BIN(capture_bin),capture_queue))
    {
        CMP_DEBUG_PRINT("Failed:%d\n\n",__LINE__);
        return false;
    }
    if (TRUE != gst_bin_add (GST_BIN(capture_bin),capture_encoder))
    {
        CMP_DEBUG_PRINT("Failed:%d\n\n",__LINE__);
        return false;
    }
    if (TRUE != gst_bin_add (GST_BIN(capture_bin),capture_sink))
    {
        CMP_DEBUG_PRINT("Failed:%d\n\n",__LINE__);
        return false;
    }
    capture_queue_pad = gst_element_get_static_pad (capture_queue, "sink");
    if (TRUE != gst_element_link_many (capture_queue, capture_encoder, capture_sink, NULL))
    {
        CMP_DEBUG_PRINT ("link capture elements could not be linked.\n");
        return false;
    }

    capture_ghost_sinkpad = gst_ghost_pad_new ("sink", capture_queue_pad);
    gst_pad_set_active (capture_ghost_sinkpad, TRUE);
    gst_element_add_pad (GST_ELEMENT(capture_bin), capture_ghost_sinkpad);
    gst_object_unref (capture_queue_pad);

    g_object_set (G_OBJECT (capture_sink), "emit-signals", TRUE, "sync", FALSE, NULL);
    numOfImagesToCapture = NUMBEROFIMAGES;
    g_signal_connect (capture_sink, "new-sample", G_CALLBACK (get_sample), NULL);

    if (TRUE != gst_bin_add (GST_BIN (pipeline), capture_bin))
    {
        CMP_DEBUG_PRINT ("gst bin add failed\n\n");
        return false;
    }
    if (pad == NULL || capture_ghost_sinkpad == NULL)
    {
        CMP_DEBUG_PRINT ("pad is NULL\n\n");
        return false;
    }
    if (GST_PAD_LINK_OK != gst_pad_link (pad, capture_ghost_sinkpad))
    {
        CMP_DEBUG_PRINT ("Capture Tee could not be linked.\n");
        return false;
    }
    if(TRUE != gst_element_sync_state_with_parent (capture_bin))
    {
        CMP_DEBUG_PRINT("Sync state failed:%d\n",__LINE__);
        return false;
    }
    return true;
}

bool create_record_bin(GstPad * pad)
{
    record_bin = gst_bin_new ("record_bin");
    record_queue = gst_element_factory_make ("queue", "record_queue");
    record_encoder = gst_element_factory_make ("omxh264enc", "record_encoder");
    record_sink = gst_element_factory_make("filesink", NULL);
    gst_bin_add_many (GST_BIN (record_bin), record_queue, record_encoder, record_sink, NULL);
    record_queue_pad = gst_element_get_static_pad (record_queue, "sink");
    g_object_set(G_OBJECT(record_sink), "sync", true, NULL);
    if (TRUE != gst_element_link_many (record_queue, record_encoder, record_sink, NULL))
    {
        CMP_DEBUG_PRINT ("link capture elements could not be linked.\n");
        return false;
    }

    record_ghost_sinkpad = gst_ghost_pad_new ("sink", record_queue_pad);
    gst_pad_set_active (record_ghost_sinkpad, TRUE);
    gst_element_add_pad (GST_ELEMENT(record_bin), record_ghost_sinkpad);
    gst_object_unref (record_queue_pad);

    if (!record_encoder || !record_queue_pad || !record_sink || !record_queue )
    {
        CMP_DEBUG_PRINT("Record elements not created.");
        return false;
    }

    numOfImagesToCapture = NUMBEROFIMAGES;
    g_object_set(G_OBJECT(record_sink), "location", "/media/internal/Record.h264", NULL);

    if (TRUE != gst_bin_add (GST_BIN (pipeline), record_bin))
    {
        CMP_DEBUG_PRINT ("gst bin add failed\n\n");
        return false;
    }
    if (NULL == record_ghost_sinkpad)
    {
        CMP_DEBUG_PRINT ("record_ghost_sinkpad is NULL\n\n");
        return false;
    }
    if (GST_PAD_LINK_OK != gst_pad_link (pad, record_ghost_sinkpad))
    {
        CMP_DEBUG_PRINT ("Record Tee could not be linked.\n");
        return false;
    }
    if(TRUE != gst_element_sync_state_with_parent (record_bin))
    {
        CMP_DEBUG_PRINT("Sync state failed:%d\n",__LINE__);
        return false;
    }
    return true;
}

bool create_preview_bin(GstPad * pad)
{
    preview_bin = gst_bin_new ("preview_bin");
    preview_queue = gst_element_factory_make ("queue", "preview_queue");
    preview_sink = gst_element_factory_make("kmssink", "preview_sink");

    g_object_set(G_OBJECT(preview_sink), "sync", false, NULL);
#ifdef PLATFORM_RASPBERRYPI3
    g_object_set(G_OBJECT(preview_sink), "driver-name", "vc4", NULL);
#endif

    if (0 < planeNo) {
       CMP_DEBUG_PRINT("Plane Id setting with %d",planeNo);
       g_object_set(G_OBJECT(preview_sink), "plane-id", planeNo, NULL);
    }

#ifdef PLATFORM_INTEL_APOLLO_LAKE
    if (0 < crctNo) {
       CMP_DEBUG_PRINT("Crtc Id setting with %d",crctNo);
       g_object_set(G_OBJECT(preview_sink), "crtc-id", crctNo, NULL);
    }
#endif

    if (0 < connNo) {
       CMP_DEBUG_PRINT("Conn Id setting with %d",connNo);
       g_object_set(G_OBJECT(preview_sink), "connector-id", connNo, NULL);
    }

    gst_bin_add_many (GST_BIN (preview_bin), preview_queue, preview_sink, NULL);
    if (TRUE != gst_element_link (preview_queue, preview_sink))
    {
        g_printerr ("Elements could not be linked.\n");
        return false;
    }

    preview_queue_pad = gst_element_get_static_pad (preview_queue, "sink");
    preview_ghost_sinkpad = gst_ghost_pad_new ("sink", preview_queue_pad);
    gst_pad_set_active (preview_ghost_sinkpad, TRUE);
    gst_element_add_pad (GST_ELEMENT(preview_bin), preview_ghost_sinkpad);
    if (TRUE != gst_bin_add (GST_BIN (pipeline), preview_bin))
    {
        CMP_DEBUG_PRINT ("gst bin add failed\n\n");
        return false;
    }
    if (GST_PAD_LINK_OK != gst_pad_link (pad, preview_ghost_sinkpad))
    {
        CMP_DEBUG_PRINT ("Record Tee could not be linked.\n");
        return false;
    }
    if(TRUE != gst_element_sync_state_with_parent (preview_bin))
    {
        CMP_DEBUG_PRINT("Sync state failed:%d\n",__LINE__);
        return false;
    }
    return true;
}

bool create_bin(GstPad * pad,char *mode)
{
    if (0 == strncmp(mode,"capture", strlen("capture")))
    {
      return create_capture_bin(pad);
    }
    else if (0 == strncmp(mode,"record", strlen("record")))
    {
      return create_record_bin(pad);
    }
    else if (0 == strncmp(mode,"preview", strlen("preview")))
    {
      return create_preview_bin(pad);
    }
    return true;
}

static GstPadProbeReturn
record_remove_probe(GstPad * pad, GstPadProbeInfo * info, gpointer user_data)
{
    gst_pad_unlink (record_tee_pad_ref, record_ghost_sinkpad);
    gst_element_remove_pad(record_bin, record_queue_pad);
    g_object_unref(record_queue_pad);
    gst_bin_remove(GST_BIN(pipeline), record_bin);
    gst_object_unref(record_bin);
    gst_element_release_request_pad(tee,tee_record_pad);
    gst_object_unref(record_tee_pad_ref);
    gst_object_unref(tee_record_pad);

    gst_pad_remove_probe (pad, GST_PAD_PROBE_INFO_ID (info));

    return GST_PAD_PROBE_OK;

}

CameraPlayer::CameraPlayer()
    : planeId_(-1),
      display_path_(0),
      current_state_(base::playback_state_t::STOPPED) {
    }

CameraPlayer::~CameraPlayer()
{
    Unload();
    gst_deinit();
}

bool CameraPlayer::Load(const std::string &str, const std::string &payload)
{
    CMP_DEBUG_PRINT("payload : %s", payload.c_str());

    CMPASSERT(!str.empty());

    cmp::parser::Parser parser(payload.c_str());
    format_ = parser.get<std::string>("format");
    CMP_DEBUG_PRINT("format_ : %s", format_.c_str());

    width_ = parser.get<int>("width");
    CMP_DEBUG_PRINT("width_ : %d", width_);

    height_ = parser.get<int>("height");
    CMP_DEBUG_PRINT("height_ : %d", height_);

    framerate_ = parser.get<int>("frameRate");
    CMP_DEBUG_PRINT("framerate_: %d", framerate_);

    memtype_ = parser.get<std::string>("memType");
    CMP_DEBUG_PRINT("memtype_ : %s", memtype_.c_str());

    memsrc_ = parser.get<std::string>("memSrc");
    CMP_DEBUG_PRINT("memsrc_ : %s", memsrc_.c_str());

    shmkey_ = atoi(memsrc_.c_str());
    CMP_DEBUG_PRINT("shmkey_ : %d",shmkey_);

    ParseOptionString(str);
    SetGstreamerDebug();
    gst_init(NULL, NULL);
    gst_pb_utils_init();

    if (!GetSourceInfo()) {
        CMP_DEBUG_PRINT("get source information failed!");
        return false;
    }

    if (!service_->acquire(source_info_, display_path_)) {
        CMP_DEBUG_PRINT("resouce acquire fail!");
        return false;
    }

    if (!LoadPipeline()) {
        CMP_DEBUG_PRINT("pipeline load failed!");
        return false;
    }
    SetPlayerState(base::playback_state_t::LOADED);

    CMP_DEBUG_PRINT("Load Done:");
    return true;
}

bool CameraPlayer::Unload()
{
    CMP_DEBUG_PRINT("unload");
    if (!pipeline_)
    {
        CMP_DEBUG_PRINT("pipeline is null");
        return false;
    }
    gst_element_set_state(pipeline_, GST_STATE_NULL);
    gst_object_unref(GST_OBJECT(pipeline_));
    pipeline_ = NULL;

    SetPlayerState(base::playback_state_t::STOPPED);

    service_->Notify(NOTIFY_UNLOAD_COMPLETED);
    return service_->Stop();
}

bool CameraPlayer::Play()
{
    CMP_DEBUG_PRINT("play");
    if (!pipeline_)
    {
        CMP_DEBUG_PRINT("pipeline is null");
        return false;
    }

    if (!gst_element_set_state(pipeline_, GST_STATE_PLAYING)) {
        CMP_DEBUG_PRINT("set GST_STATE_PLAYING failed!!!");
        return false;
    }

    SetPlayerState(base::playback_state_t::PLAYING);

    service_->Notify(NOTIFY_PLAYING);
    return true;
}

bool CameraPlayer::SetPlane(int planeId)
{
    CMP_DEBUG_PRINT("setPlane planeId:(%d)", planeId);
    planeId_ = planeId;

    return true;
}

bool CameraPlayer::SetDisplayResource(cmp::base::disp_res_t &res) {
  CMP_DEBUG_PRINT("setDisplayResource planeId:(%d), crtcId:(%d), connId(%d)", res.plane_id, res.crtc_id, res.conn_id);
  planeId_ = res.plane_id;
  crtcId_ = res.crtc_id;
  connId_ = res.conn_id;
  return true;
}

void CameraPlayer::Initialize(cmp::service::IService *service)
{
    if (!service)
        return;

    service_ = service;
}

bool CameraPlayer::GetSourceInfo()
{
    GError *err = NULL;
    GstDiscoverer* discoverer = NULL;
    GstDiscovererInfo* info = NULL;
    discoverer = gst_discoverer_new((GstClockTime)DISCOVER_EXPIRE_TIME, &err);


    base::video_info_t video_stream_info;

    video_stream_info.width = width_;
    video_stream_info.height = height_;
    video_stream_info.codec = 0;
    video_stream_info.frame_rate.num = framerate_;
    video_stream_info.frame_rate.den = 1;

    CMP_DEBUG_PRINT("[video info] width: %d, height: %d, bitRate: %lld, frameRate: %d/%d",
            video_stream_info.width,
            video_stream_info.height,
            video_stream_info.bit_rate,
            video_stream_info.frame_rate.num,
            video_stream_info.frame_rate.den);

    base::program_info_t program;
    program.video_stream = 1;
    source_info_.programs.push_back(program);

    source_info_.video_streams.push_back(video_stream_info);

    g_clear_error(&err);
    g_object_unref(discoverer);
    g_object_unref(info);
    return true;
}

void CameraPlayer::NotifySourceInfo()
{
    service_->Notify(NOTIFY_SOURCE_INFO, &source_info_);
}

gboolean CameraPlayer::HandleBusMessage(GstBus *bus,
        GstMessage *message, gpointer user_data)
{
    CameraPlayer *player = reinterpret_cast<CameraPlayer *>(user_data);

    switch (GST_MESSAGE_TYPE(message))
    {
        case GST_MESSAGE_ERROR:
            {
                base::error_t error = player->HandleErrorMessage(message);
                player->service_->Notify(NOTIFY_ERROR, &error);
                break;
            }

        case GST_MESSAGE_EOS:
            {
                CMP_DEBUG_PRINT("Got endOfStream");
                player->service_->Notify(NOTIFY_END_OF_STREAM);
                break;
            }

        case GST_MESSAGE_ASYNC_DONE:
            {
                CMP_DEBUG_PRINT("ASYNC DONE");

                if (!player->load_complete_)
                {
                    player->service_->Notify(NOTIFY_LOAD_COMPLETED);
                    player->load_complete_ = true;
                }

                break;
            }

        case GST_STATE_PAUSED:
            {
                CMP_DEBUG_PRINT("PAUSED");
                player->service_->Notify(NOTIFY_PAUSED);
                break;
            }

        case GST_STATE_PLAYING:
            {
                CMP_DEBUG_PRINT("PLAYING");
                pipeline = player->pipeline_;
                player->service_->Notify(NOTIFY_PLAYING);
                break;
            }

        case GST_MESSAGE_STATE_CHANGED:
            {
                pipeline = player->pipeline_;
                if (GST_MESSAGE_SRC(message) == GST_OBJECT_CAST(pipeline))
                {
                    GstState old_state, new_state;
                    gst_message_parse_state_changed(message, &old_state, &new_state, NULL);

                    // generate dot graph when play start only(READY -> PAUSED)
                    if (old_state == GST_STATE_READY && new_state == GST_STATE_PAUSED)
                    {
                        CMP_INFO_PRINT("Element [ %s ] State changed ...%s -> %s",
                                       GST_MESSAGE_SRC_NAME(message),
                                       gst_element_state_get_name(old_state),
                                       gst_element_state_get_name(new_state));
                    }
                }
                break;
            }

        case GST_MESSAGE_APPLICATION:
            {
                const GstStructure *gStruct = gst_message_get_structure(message);

                /* video-info message comes from sink element */
                if (gst_structure_has_name(gStruct, "video-info"))
                {
                    CMP_INFO_PRINT("got video-info message");
                    base::video_info_t video_info;
                    memset(&video_info, 0, sizeof(base::video_info_t));
                    gint width, height, fps_n, fps_d, par_n, par_d;
                    gst_structure_get_int(gStruct, "width", &width);
                    gst_structure_get_int(gStruct, "height", &height);
                    gst_structure_get_fraction(gStruct, "framerate", &fps_n, &fps_d);
                    gst_structure_get_int(gStruct, "par_n", &par_n);
                    gst_structure_get_int(gStruct, "par_d", &par_d);

                    CMP_INFO_PRINT("width[%d], height[%d], framerate[%d/%d], pixel_aspect_ratio[%d/%d]",
                            width, height, fps_n, fps_d, par_n, par_d);

                    video_info.width = width;
                    video_info.height = height;
                    video_info.frame_rate.num = fps_n;
                    video_info.frame_rate.den = fps_d;
                    video_info.bit_rate = 0;
                    video_info.codec = 0;

                    player->service_->Notify(NOTIFY_VIDEO_INFO, &video_info);
                }
                break;
            }
        default:  break;
    }

    return true;
}

bool CameraPlayer::LoadPipeline()
{
    CMP_DEBUG_PRINT("LoadPipeline planeId:%d ", planeId_);
    NotifySourceInfo();
    ProgramData *data = NULL;
    GMainLoop* loop = NULL;
    key_t key = shmkey_;
    loop = g_main_loop_new (NULL, FALSE);
    context.shmemKey = key;

    pipeline_ = gst_pipeline_new ("camera-player");
    if(strncmp(memtype_.c_str(),"device",6) == 0)
    {
        source   = gst_element_factory_make ("camsrc", "source");
        g_object_set (source, "device", memsrc_.c_str(), NULL);
    }
    else if(strncmp(memtype_.c_str(),"shmem",5) == 0)
    {
        source   = gst_element_factory_make ("appsrc", "source");
        g_object_set (source, "format", GST_FORMAT_TIME, NULL);
        g_signal_connect (source, "need-data", G_CALLBACK (feed_data), data);
        if (OpenShmem((SHMEM_HANDLE *)(&(context.shmemHandle)),context.shmemKey) != 0)
        {
            CMP_DEBUG_PRINT("openShmem failed");
        }
    }
    else
    {
        CMP_DEBUG_PRINT("Create Bin Failed.\n");
        return (gst_element_set_state (pipeline_, GST_STATE_NULL));
    }


    if(0 == strncmp(format_.c_str(),"YUY2", strlen("YUY2")))
    {
        CMP_DEBUG_PRINT("YUV FORMAT");

        tee = gst_element_factory_make ("tee", "tee");
        tee_ref = tee;
        filter   = gst_element_factory_make ("capsfilter", "filter");
        filter2  = gst_element_factory_make ("capsfilter", "filter2");
        caps = gst_caps_new_simple ("video/x-raw",
                "width", G_TYPE_INT, width_,
                "height", G_TYPE_INT, height_,
                "framerate", GST_TYPE_FRACTION, framerate_, 1,
                "format", G_TYPE_STRING,"YUY2",
                NULL);
        caps2 = gst_caps_new_simple ("video/x-raw",
                "width", G_TYPE_INT, width_,
                "height", G_TYPE_INT, height_,
                "format", G_TYPE_STRING,"I420",
                NULL);
        if (!pipeline_ || !source) {
            g_printerr ("elements could not be created. Exiting.");
            return -1;
        }
        g_object_set (G_OBJECT (filter), "caps", caps, NULL);
        g_object_set (G_OBJECT (filter2), "caps", caps2, NULL);

        vconv = gst_element_factory_make ("videoconvert", "vconv");
        gst_bin_add_many (GST_BIN (pipeline_), source, filter, vconv, filter2, tee, NULL);

        gst_element_link_many (source, filter, vconv, filter2, tee, NULL);
        tee_preview_pad = gst_element_get_request_pad (tee, "src_%u");

        pipeline = pipeline_;
        if (create_bin(tee_preview_pad,"preview"))
        {
            bus = gst_pipeline_get_bus (GST_PIPELINE (pipeline_));
            gst_bus_add_watch(bus, CameraPlayer::HandleBusMessage, this);
            gst_object_unref (bus);
            g_main_loop_unref(loop);

        }
        else
        {
            CMP_DEBUG_PRINT("Create Bin Failed.\n");
            return (gst_element_set_state (pipeline_, GST_STATE_NULL));
        }

    }//end of YUV

    if(0 == strncmp(format_.c_str(),"JPEG",strlen("JPEG")))
    {
        CMP_DEBUG_PRINT("JPEG FORMAT");
        tee = gst_element_factory_make ("tee", "tee");
        tee_ref = tee;
        decoder  = gst_element_factory_make ("jpegdec", "decoder");
        parser   = gst_element_factory_make ("jpegparse",   "parser");


        if (!pipeline_ || !source || !tee || !decoder ) {
            g_printerr ("One element could not be created. Exiting.\n");
        }

        filter  = gst_element_factory_make ("capsfilter", "filter");
        caps = gst_caps_new_simple ("image/jpeg",
                "width", G_TYPE_INT, width_,
                "height", G_TYPE_INT, height_,
                "framerate", GST_TYPE_FRACTION, framerate_, 1,
                NULL);
        g_object_set (G_OBJECT (filter), "caps", caps, NULL);
        gst_bin_add_many (GST_BIN (pipeline_), source, filter, parser, decoder, tee, NULL);

        if (TRUE != gst_element_link_many (source, filter, parser, decoder, tee, NULL))
        {
            CMP_DEBUG_PRINT("Elements could not be linked.\n");
        }

        tee_preview_pad = gst_element_get_request_pad (tee, "src_%u");

        pipeline = pipeline_;

        if (0 < planeId_) {
           planeNo = planeId_;
        }

        if (0 < crtcId_) {
           crctNo = crtcId_;
        }

        if (0 < connId_) {
           connNo = connId_;
        }

        if (create_bin(tee_preview_pad,"preview"))
        {
            bus = gst_pipeline_get_bus (GST_PIPELINE (pipeline_));
            gst_bus_add_watch(bus, CameraPlayer::HandleBusMessage, this);
            gst_object_unref (bus);
            g_main_loop_unref(loop);
        }
        else
        {
            CMP_DEBUG_PRINT("Create Bin Failed.\n");
            return (gst_element_set_state (pipeline_, GST_STATE_NULL));
        }
    }
    return (gst_element_set_state (pipeline_, GST_STATE_PAUSED));
}

bool CameraPlayer::takeSnapshot(const std::string  &location)
{
    if (NULL == tee_capture_pad)
    {
        tee_capture_pad = gst_element_get_request_pad (tee, "src_%u");
        capture_tee_pad_ref = tee_capture_pad;

        if (tee_capture_pad == NULL || capture_tee_pad_ref == NULL)
        {
            CMP_DEBUG_PRINT("capture tee pad is NULL\n");
        }
        if (create_bin(tee_capture_pad,"capture"))
        {
            return true;
        }
        else
        {
            CMP_DEBUG_PRINT("Create Bin Failed.\n");
            return false;
        }
    }
    else
    {
            CMP_DEBUG_PRINT("capture tee pad is not NULL\n");
        numOfImagesToCapture = 1;
    }
}

bool CameraPlayer::startRecord(const std::string  &location)
{
    tee_record_pad = gst_element_get_request_pad (tee, "src_%u");
    record_tee_pad_ref = tee_record_pad;
    if (create_bin(tee_record_pad,"record"))
    {
        return true;
    }
    else
    {
        CMP_DEBUG_PRINT("Create Bin Failed.\n");
        return false;
    }
}

bool CameraPlayer::stopRecord()
{
    gst_pad_add_probe (record_tee_pad_ref, GST_PAD_PROBE_TYPE_IDLE,
            (GstPadProbeCallback) record_remove_probe, NULL, NULL);
    return true;
}

base::error_t CameraPlayer::HandleErrorMessage(GstMessage *message)
{
    GError *err = NULL;
    gchar *debug_info;
    gst_message_parse_error(message, &err, &debug_info);
    GQuark domain = err->domain;

    base::error_t error;
    error.errorCode = ConvertErrorCode(domain, (gint)err->code);
    error.errorText = g_strdup(err->message);

    CMP_DEBUG_PRINT("[GST_MESSAGE_ERROR][domain:%s][from:%s][code:%d][converted:%d][msg:%s]",
            g_quark_to_string(domain), (GST_OBJECT_NAME(GST_MESSAGE_SRC(message))),
            err->code, error.errorCode, err->message);
    CMP_DEBUG_PRINT("Debugging information: %s", debug_info ? debug_info : "none");

    g_clear_error(&err);
    g_free(debug_info);

    return error;
}

int32_t CameraPlayer::ConvertErrorCode(GQuark domain, gint code)
{
    int32_t converted = MEDIA_MSG_ERR_PLAYING;

    if (GST_CORE_ERROR == domain)
    {
        switch (code)
        {
            case GST_CORE_ERROR_EVENT:
                converted = MEDIA_MSG__GST_CORE_ERROR_EVENT;
                break;
            default:
                break;
        }
    }
    else if (GST_LIBRARY_ERROR == domain)
    {
        // do nothing
    }
    else if (GST_RESOURCE_ERROR == domain)
    {
        switch (code)
        {
            case GST_RESOURCE_ERROR_SETTINGS:
                converted = MEDIA_MSG__GST_RESOURCE_ERROR_SETTINGS;
                break;
            case GST_RESOURCE_ERROR_NOT_FOUND:
                converted = MEDIA_MSG__GST_RESOURCE_ERROR_NOT_FOUND;
                break;
            case GST_RESOURCE_ERROR_OPEN_READ:
                converted = MEDIA_MSG__GST_RESOURCE_ERROR_OPEN_READ;
                break;
            case GST_RESOURCE_ERROR_READ:
                converted = MEDIA_MSG__GST_RESOURCE_ERROR_READ;
                break;
            default:
                break;
        }
    }
    else if (GST_STREAM_ERROR == domain)
    {
        switch (code)
        {
            case GST_STREAM_ERROR_TYPE_NOT_FOUND:
                converted = MEDIA_MSG__GST_STREAM_ERROR_TYPE_NOT_FOUND;
                break;
            case GST_STREAM_ERROR_DEMUX:
                converted = MEDIA_MSG__GST_STREAM_ERROR_DEMUX;
                break;
            default:
                break;
        }
    }

    return converted;
}

void CameraPlayer::SetGstreamerDebug()
{
    const char *kDebug = "GST_DEBUG";
    const char *kDebugFile = "GST_DEBUG_FILE";
    const char *kDebugDot = "GST_DEBUG_DUMP_DOT_DIR";
    int size = 0;

    std::string input_file("/etc/g-camera-pipeline/gst_debug.conf");
    pbnjson::JDomParser parser(NULL);

    if (!parser.parseFile(input_file, pbnjson::JSchema::AllSchema(), NULL))
    {
        CMP_DEBUG_PRINT("Debug file parsing error");
        return;
    }

    pbnjson::JValue parsed = parser.getDom();
    pbnjson::JValue debug = parsed["gst_debug"];

    size = debug.arraySize();
    for (int i = 0; i < size; i++)
    {
        if (debug[i].hasKey(kDebug) && !debug[i][kDebug].asString().empty())
            setenv(kDebug, debug[i][kDebug].asString().c_str(), 1);
        if (debug[i].hasKey(kDebugFile) && !debug[i][kDebugFile].asString().empty())
            setenv(kDebugFile, debug[i][kDebugFile].asString().c_str(), 1);
        if (debug[i].hasKey(kDebugDot) && !debug[i][kDebugDot].asString().empty())
            setenv(kDebugDot, debug[i][kDebugDot].asString().c_str(), 1);
    }
}

void CameraPlayer::ParseOptionString(const std::string &str) {
    CMP_DEBUG_PRINT("option string: %s", str.c_str());

    pbnjson::JDomParser jdparser;
    if (!jdparser.parse(str, pbnjson::JSchema::AllSchema())) {
       CMP_DEBUG_PRINT("ERROR JDomParser.parse. msg: %s ", str.c_str());
    return;
    }
    pbnjson::JValue parsed = jdparser.getDom();

    uri_ = parsed["uri"].asString();
    if (parsed["options"]["option"].hasKey("display-path")) {
       int32_t display_path = parsed["options"]["option"]["display-path"].asNumber<int32_t>();
       display_path_ = (display_path >= MAX_NUM_DISPLAY ? 0 : display_path);
    }

    CMP_DEBUG_PRINT("uri: %s, display-path: %d", uri_.c_str(), display_path_);
}

}  // namespace player
}  // namespace cmp
