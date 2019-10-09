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
#include <dirent.h>

#include "parser/parser.h"
#include "camera_player.h"

#ifdef CMP_DEBUG_PRINT
#undef CMP_DEBUG_PRINT
#endif

#define CMP_DEBUG_PRINT CMP_INFO_PRINT

#define DISCOVER_EXPIRE_TIME (10 * GST_SECOND)

const int kNumOfImages = 1;
const std::string kFormatYUV = "YUY2";
const std::string kFormatJPEG = "JPEG";
const std::string kFormatI420 = "I420";
const std::string kModePreview = "preview";
const std::string kModeCapture = "capture";
const std::string kModeRecord = "record";
const std::string kMemtypeDevice = "device";
const std::string kMemtypeShmem = "shmem";
const std::string kCaptureImageFile = "/tmp/capture.jpeg";
const std::string kRecordPath = "/media/internal/";
static int kExistingFileIndex = -1;

namespace cmp { namespace player {

CameraPlayer::CameraPlayer()
    : planeId_(-1),
    shmkey_(0),
    width_(0),
    height_(0),
    framerate_(0),
    crtcId_(0),
    connId_(0),
    display_path_idx_(0),
    num_of_images_to_capture_(0),
    num_of_captured_images_(0),
    uri_(""),
    memtype_(""),
    memsrc_(""),
    format_(""),
    capture_path_(""),
    record_path_(""),
    pipeline_(NULL),
    source_(NULL),
    parser_(NULL),
    decoder_(NULL),
    filter_YUY2_(NULL),
    filter_I420_(NULL),
    filter_JPEG_(NULL),
    vconv_(NULL),
    tee_(NULL),
    capture_queue_(NULL),
    capture_encoder_(NULL),
    capture_sink_(NULL),
    record_queue_(NULL),
    record_encoder_(NULL),
    record_decoder_(NULL),
    record_mux_(NULL),
    record_sink_(NULL),
    preview_queue_(NULL),
    preview_sink_(NULL),
    tee_preview_pad_(NULL),
    preview_queue_pad_(NULL),
    capture_queue_pad_(NULL),
    tee_capture_pad_(NULL),
    record_queue_pad_(NULL),
    tee_record_pad_(NULL),
    context_{NULL,1,0,0,FALSE,NULL},
    source_info_(),
    current_state_(base::playback_state_t::STOPPED),
    bus_(NULL),
    caps_YUY2_(NULL),
    caps_I420_(NULL),
    caps_JPEG_(NULL),
    service_(NULL),
    load_complete_(false){
    }

CameraPlayer::~CameraPlayer()
{
    Unload();
    gst_deinit();
}

bool CameraPlayer::Load(const std::string& str, const std::string& payload)
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
    CMP_DEBUG_PRINT("shmkey_ : %d", shmkey_);

    ParseOptionString(str);
    SetGstreamerDebug();
    gst_init(NULL, NULL);
    gst_pb_utils_init();

    if (!GetSourceInfo()) {
        CMP_DEBUG_PRINT("get source information failed!");
        return false;
    }

    if (!service_->acquire(source_info_, display_path_idx_)) {
        CMP_DEBUG_PRINT("resouce acquire fail!");
        return false;
    }

    if (!LoadPipeline()) {
        CMP_DEBUG_PRINT("pipeline_ load failed!");
        return false;
    }
    SetPlayerState(base::playback_state_t::LOADED);

    CMP_DEBUG_PRINT("Load Done:");
    return true;
}

bool CameraPlayer::Unload()
{
    CMP_DEBUG_PRINT("unload");
    if (!pipeline_) {
        CMP_DEBUG_PRINT("pipeline_ is null");
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
    if (!pipeline_) {
        CMP_DEBUG_PRINT("pipeline_ is null");
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

bool CameraPlayer::SetDisplayResource(cmp::base::disp_res_t &res)
{
    CMP_DEBUG_PRINT("setDisplayResource planeId:(%d), crtcId:(%d), connId(%d)",
                    res.plane_id, res.crtc_id, res.conn_id);
    planeId_ = res.plane_id;
    crtcId_ = res.crtc_id;
    connId_ = res.conn_id;
    return true;
}

bool CameraPlayer::TakeSnapshot(const std::string& location)
{
    CMP_DEBUG_PRINT("capture path location [ %s ]\n", location.c_str());

    if (!location.empty())
        capture_path_ = location;

    tee_capture_pad_ = gst_element_get_request_pad(tee_, "src_%u");
    if (tee_capture_pad_ == NULL) {
        CMP_DEBUG_PRINT("tee_capture_pad_ is NULL\n");
        return false;
    }

    if (!CreateCaptureElements(tee_capture_pad_)) {
        CMP_DEBUG_PRINT("CreateCaptureElements Failed.\n");
        return false;
    }
    return true;
}

bool CameraPlayer::StartRecord(const std::string& location)
{
    if(!location.empty())
        record_path_ = location;

    tee_record_pad_ = gst_element_get_request_pad(tee_, "src_%u");
    if (tee_record_pad_ == NULL) {
        CMP_DEBUG_PRINT("tee_record_pad_ is NULL\n");
        return false;
    }

    if (!CreateRecordElements(tee_record_pad_)) {
        CMP_DEBUG_PRINT("CreateRecordElements Failed.\n");
        return false;
    }
    return true;
}

bool CameraPlayer::StopRecord()
{
    gst_pad_add_probe(tee_record_pad_, GST_PAD_PROBE_TYPE_IDLE,
                      (GstPadProbeCallback) RecordRemoveProbe, this, NULL);
    return true;
}

void CameraPlayer::Initialize(cmp::service::IService *service)
{
    if (!service)
        return;

    service_ = service;
}

gboolean CameraPlayer::HandleBusMessage(
    GstBus *bus_, GstMessage *message, gpointer user_data)
{
    GstMessageType messageType = GST_MESSAGE_TYPE(message);
    if (messageType != GST_MESSAGE_QOS && messageType != GST_MESSAGE_TAG) {
        CMP_DEBUG_PRINT("Element[ %s ][ %d ][ %s ]",
                        GST_MESSAGE_SRC_NAME(message),
                        messageType, gst_message_type_get_name(messageType));
    }

    CameraPlayer *player = reinterpret_cast<CameraPlayer *>(user_data);

    switch (GST_MESSAGE_TYPE(message)) {
        case GST_MESSAGE_ERROR: {
            base::error_t error = player->HandleErrorMessage(message);
            player->service_->Notify(NOTIFY_ERROR, &error);
            break;
        }

        case GST_MESSAGE_EOS: {
            CMP_DEBUG_PRINT("Got endOfStream");
            player->service_->Notify(NOTIFY_END_OF_STREAM);
            break;
        }

        case GST_MESSAGE_ASYNC_DONE: {
            CMP_DEBUG_PRINT("ASYNC DONE");

            if (!player->load_complete_) {
                player->service_->Notify(NOTIFY_LOAD_COMPLETED);
                player->load_complete_ = true;
            }
            break;
        }

        case GST_STATE_PAUSED: {
            CMP_DEBUG_PRINT("PAUSED");
            player->service_->Notify(NOTIFY_PAUSED);
            break;
        }

        case GST_STATE_PLAYING: {
            CMP_DEBUG_PRINT("PLAYING");
            player->service_->Notify(NOTIFY_PLAYING);
            break;
        }

        case GST_MESSAGE_STATE_CHANGED: {
            GstState oldState = GST_STATE_NULL;
            GstState newState = GST_STATE_NULL;
            gst_message_parse_state_changed(message,
                                            &oldState, &newState, NULL);
            CMP_INFO_PRINT("Element[%s] State changed ...%s -> %s",
                           GST_MESSAGE_SRC_NAME(message),
                           gst_element_state_get_name(oldState),
                           gst_element_state_get_name(newState));
            break;
        }

        case GST_MESSAGE_APPLICATION: {
            const GstStructure *gStruct = gst_message_get_structure(message);

            /* video-info message comes from sink element */
            if (gst_structure_has_name(gStruct, "video-info")) {
                CMP_INFO_PRINT("got video-info message");
                base::video_info_t video_info;
                memset(&video_info, 0, sizeof(base::video_info_t));

                gint width, height, fps_n, fps_d, par_n, par_d;
                gst_structure_get_int(gStruct, "width", &width);
                gst_structure_get_int(gStruct, "height", &height);
                gst_structure_get_fraction(gStruct, "framerate",&fps_n, &fps_d);
                gst_structure_get_int(gStruct, "par_n", &par_n);
                gst_structure_get_int(gStruct, "par_d", &par_d);

                CMP_INFO_PRINT("width[%d], height[%d], framerate[%d/%d],"
                               "pixel_aspect_ratio[%d/%d]", width, height,
                               fps_n, fps_d, par_n, par_d);

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
        default:
            break;
    }

    return true;
}

void CameraPlayer::NotifySourceInfo()
{
    service_->Notify(NOTIFY_SOURCE_INFO, &source_info_);
}

void CameraPlayer::SetGstreamerDebug()
{
    std::string input_file("/etc/g-camera-pipeline/gst_debug.conf");
    pbnjson::JDomParser parser(NULL);

    if (!parser.parseFile(input_file, pbnjson::JSchema::AllSchema(), NULL)) {
        CMP_DEBUG_PRINT("Debug file parsing error");
        return;
    }

    pbnjson::JValue parsed = parser.getDom();
    pbnjson::JValue debug = parsed["gst_debug"];

    int size = debug.arraySize();
    for (int i = 0; i < size; i++) {
        const char *kDebug = "GST_DEBUG";
        const char *kDebugFile = "GST_DEBUG_FILE";
        const char *kDebugDot = "GST_DEBUG_DUMP_DOT_DIR";
        if (debug[i].hasKey(kDebug) && !debug[i][kDebug].asString().empty())
            setenv(kDebug, debug[i][kDebug].asString().c_str(), 1);
        if (debug[i].hasKey(kDebugFile) &&
                !debug[i][kDebugFile].asString().empty())
            setenv(kDebugFile, debug[i][kDebugFile].asString().c_str(), 1);
        if (debug[i].hasKey(kDebugDot) &&
                !debug[i][kDebugDot].asString().empty())
            setenv(kDebugDot, debug[i][kDebugDot].asString().c_str(), 1);
    }
}

void CameraPlayer::ParseOptionString(const std::string& str)
{
    CMP_DEBUG_PRINT("option string: %s", str.c_str());

    pbnjson::JDomParser jdparser;
    if (!jdparser.parse(str, pbnjson::JSchema::AllSchema())) {
        CMP_DEBUG_PRINT("ERROR JDomParser.parse. msg: %s ", str.c_str());
        return;
    }
    pbnjson::JValue parsed = jdparser.getDom();

    uri_ = parsed["uri"].asString();
    if (parsed["options"]["option"].hasKey("display-path")) {
        int32_t display_path =
            parsed["options"]["option"]["display-path"].asNumber<int32_t>();
        display_path_idx_ =
            (display_path >= MAX_NUM_DISPLAY ? 0 : display_path);
    }
    CMP_DEBUG_PRINT("uri: %s, display-path: %d", uri_.c_str(),
                    display_path_idx_);
}

void CameraPlayer::WriteImageToFile(const void *p,int size)
{
    if (capture_path_.empty())
        capture_path_ = std::string(kCaptureImageFile);;

    CMP_DEBUG_PRINT("Capture file :%s\n", capture_path_.c_str());

    FILE *fp;
    if (NULL == (fp = fopen(capture_path_.c_str(), "wb"))) {
        CMP_DEBUG_PRINT("Failed to Open file :%s\n", capture_path_.c_str());
        return;
    }

    fwrite(p, size, 1, fp);
    fclose(fp);
}

bool CameraPlayer::GetSourceInfo()
{
    GError *err = NULL;
    GstDiscoverer* discoverer =
        gst_discoverer_new((GstClockTime)DISCOVER_EXPIRE_TIME, &err);

    base::video_info_t video_stream_info;

    video_stream_info.width = width_;
    video_stream_info.height = height_;
    video_stream_info.codec = 0;
    video_stream_info.frame_rate.num = framerate_;
    video_stream_info.frame_rate.den = 1;

    CMP_DEBUG_PRINT("[video info] width: %d, height: %d, bitRate: %lld, "
        "frameRate: %d/%d", video_stream_info.width, video_stream_info.height,
        video_stream_info.bit_rate, video_stream_info.frame_rate.num,
        video_stream_info.frame_rate.den);

    base::program_info_t program;
    program.video_stream = 1;
    source_info_.programs.push_back(program);

    source_info_.video_streams.push_back(video_stream_info);

    g_clear_error(&err);
    g_object_unref(discoverer);
    return true;
}

bool CameraPlayer::LoadPipeline()
{
    CMP_DEBUG_PRINT("LoadPipeline planeId:%d ", planeId_);
    NotifySourceInfo();
    key_t key = shmkey_;
    context_.shmemKey = key;

    pipeline_ = gst_pipeline_new("camera-player");
    if (!pipeline_) {
        CMP_DEBUG_PRINT("pipeline_ element creation failed.");
        return false;
    }

    if (memtype_ == kMemtypeDevice) {
        source_ = gst_element_factory_make("camsrc", "cam-source");
        if (!source_) {
            CMP_DEBUG_PRINT("source_ element creation failed.");
            return false;
        }
        g_object_set(source_, "device", memsrc_.c_str(), NULL);
        g_object_set(source_, "do-timestamp", true, NULL);
    } else if (memtype_ == kMemtypeShmem) {
        source_ = gst_element_factory_make ("appsrc", "app-source");
        if (!source_) {
            CMP_DEBUG_PRINT("source_ element creation failed.");
            return false;
        }
        g_object_set(source_, "format", GST_FORMAT_TIME, NULL);
        g_object_set(source_, "do-timestamp", true, NULL);
        g_signal_connect(source_, "need-data", G_CALLBACK (FeedData), this);

        if (OpenShmem((SHMEM_HANDLE *)(&(context_.shmemHandle)),
                      context_.shmemKey) != 0) {
            CMP_DEBUG_PRINT("openShmem failed");
            return false;
        }
    } else {
        CMP_DEBUG_PRINT("Invalid memtype_. Not supported!!!");
        return (gst_element_set_state(pipeline_, GST_STATE_NULL));
    }


    tee_ = gst_element_factory_make("tee", "pipeline-tee");
    if (!tee_) {
        CMP_DEBUG_PRINT("tee_ element creation failed.");
        return false;
    }

    if (format_ == kFormatYUV) {
        if (!LoadYUY2Pipeline()) {
            CMP_DEBUG_PRINT("YUY2 pipeline_ load failed!");
            return false;
        }
    } else if (format_ == kFormatJPEG) {
        if (!LoadJPEGPipeline()) {
            CMP_DEBUG_PRINT("JPEG pipeline_ load failed!");
            return false;
        }
    } else {
        CMP_DEBUG_PRINT("Format[%s] not Supported", format_.c_str());
    }

    return gst_element_set_state(pipeline_, GST_STATE_PAUSED);
}

bool CameraPlayer::CreatePreviewBin(GstPad * pad)
{
    preview_queue_ = gst_element_factory_make("queue", "preview-queue");
    if (!preview_queue_) {
        CMP_DEBUG_PRINT("preview_queue_ element creation failed.");
        return false;
    }

    preview_sink_ = gst_element_factory_make("kmssink", "preview-sink");
    if (!preview_sink_) {
        CMP_DEBUG_PRINT("preview_sink_ element creation failed.");
        return false;
    }

    g_object_set(G_OBJECT(preview_sink_), "sync", false, NULL);
#ifdef PLATFORM_RASPBERRYPI3
    g_object_set(G_OBJECT(preview_sink_), "driver-name", "vc4", NULL);
#endif

    if (0 < planeId_) {
        CMP_DEBUG_PRINT("Plane Id setting with %d",planeId_);
        g_object_set(G_OBJECT(preview_sink_), "plane-id", planeId_, NULL);
    }

#ifdef PLATFORM_INTEL_APOLLO_LAKE
    if (0 < crtcId_) {
        CMP_DEBUG_PRINT("Crtc Id setting with %d",crtcId_);
        g_object_set(G_OBJECT(preview_sink_), "crtc-id", crtcId_, NULL);
    }
#endif

    if (0 < connId_) {
        CMP_DEBUG_PRINT("Conn Id setting with %d",connId_);
        g_object_set(G_OBJECT(preview_sink_), "connector-id", connId_, NULL);
    }

    gst_bin_add_many(GST_BIN(pipeline_), preview_queue_, preview_sink_, NULL);
    if (TRUE != gst_element_link_many(decoder_, preview_queue_, preview_sink_,
                                      NULL)) {
        CMP_DEBUG_PRINT ("Elements could not be linked.\n");
        return false;
    }

    return true;
}

bool CameraPlayer::CreateCaptureElements(GstPad *tee_capture_pad)
{
    num_of_images_to_capture_ = kNumOfImages;

    capture_queue_ = gst_element_factory_make("queue", "capture-queue");
    if (!capture_queue_) {
        CMP_DEBUG_PRINT("capture_queue_(%p) Failed", capture_queue_);
        return false;
    }
    capture_sink_ = gst_element_factory_make("appsink", "capture-sink");
    if (!capture_sink_) {
        CMP_DEBUG_PRINT("capture_sink_(%p) Failed", capture_sink_);
        return false;
    }

    g_object_set(G_OBJECT(capture_sink_), "emit-signals", TRUE, "sync", FALSE,
                 NULL);
    g_signal_connect(capture_sink_, "new-sample", G_CALLBACK(GetSample), this);

    gst_bin_add_many(GST_BIN(pipeline_), capture_queue_, capture_sink_, NULL);

    capture_queue_pad_ = gst_element_get_static_pad(capture_queue_, "sink");
    if (!capture_queue_pad_) {
        CMP_DEBUG_PRINT ("Did not get capture queue pad.\n");
        return false;
    }

    if (format_ == kFormatYUV) {
        if (!capture_encoder_) {
            capture_encoder_ = gst_element_factory_make("jpegenc",
                                                        "capture-encoder");
            if (!capture_encoder_) {
                CMP_DEBUG_PRINT("capture_encoder_(%p) Failed", capture_encoder_);
                return false;
            }
        }

        if (TRUE != gst_bin_add(GST_BIN(pipeline_), capture_encoder_)) {
            CMP_DEBUG_PRINT("Element capture_encoder_ could not be added. \n");
            return false;
        }

        if (TRUE != gst_element_link_many(capture_queue_, capture_encoder_,
                                          capture_sink_, NULL)) {
            CMP_DEBUG_PRINT("Elements could not be linked.\n");
            return false;
        }
    } else {
        if (TRUE != gst_element_link_many(capture_queue_, capture_sink_,NULL)) {
            CMP_DEBUG_PRINT("Elements could not be linked.\n");
            return false;
        }
    }

    if (GST_PAD_LINK_OK != gst_pad_link(tee_capture_pad, capture_queue_pad_)) {
        CMP_DEBUG_PRINT ("Capture Tee could not be linked.\n");
        return false;
    }

    if (TRUE != gst_element_sync_state_with_parent(capture_queue_)) {
        CMP_DEBUG_PRINT("Sync state capture_queue_ failed");
        return false;
    }
    if (TRUE != gst_element_sync_state_with_parent(capture_sink_)) {
        CMP_DEBUG_PRINT("Sync state capture_sink_ failed");
        return false;
    }

    if (format_ == kFormatYUV &&
        TRUE != gst_element_sync_state_with_parent(capture_encoder_)) {
        CMP_DEBUG_PRINT("Sync state capture_encoder_ failed");
        return false;
    }
    return true;
}

int CameraPlayer::GetFileIndex(const std::string& record_path)
{
    DIR *dir_handle;
    char * file_name;
    struct dirent *directory;
    bool bFound = 0;
    int fileIndex = 0;
    dir_handle = opendir(record_path.c_str());
    char delim[] = ".";
    if (dir_handle) {
        while ((directory = readdir(dir_handle)) != NULL) {
            file_name = strstr (directory->d_name,"Record");
            if(file_name) {
                bFound = 1;

                file_name = file_name + strlen("Record");
                char *ptr = strtok(file_name, delim);
                fileIndex = atoi(ptr);

                CMP_DEBUG_PRINT("Identified index %d\n", fileIndex);
            }
            else if(bFound) {
                break;
            }
        }
        closedir(dir_handle);
    }
    if(!bFound) {
        CMP_DEBUG_PRINT("Record file not found\n");
    } else {
        fileIndex = fileIndex + 1;
    }
    return fileIndex;
}

bool CameraPlayer::CreateRecordElements(GstPad *tee_record_pad)
{
    record_queue_ = gst_element_factory_make("queue", "record-queue");
    if (!record_queue_) {
        CMP_DEBUG_PRINT("record_queue_(%p) Failed", record_queue_);
        return false;
    }
    record_encoder_ = gst_element_factory_make("omxh264enc", "record-encoder");
    if (!record_encoder_) {
        CMP_DEBUG_PRINT("record_encoder_(%p) Failed", record_encoder_);
        return false;
    }
    record_mux_ = gst_element_factory_make("mpegtsmux", "record-mux");
    if (!record_mux_) {
        CMP_DEBUG_PRINT("record_mux_(%p) Failed", record_mux_);
        return false;
    }
    record_sink_ = gst_element_factory_make("filesink", "record-sink");
    if (!record_sink_) {
        CMP_DEBUG_PRINT("record_sink_(%p) Failed", record_sink_);
        return false;
    }

    char recordfilename[100];
    if (record_path_.empty())
        record_path_ = kRecordPath;

    CMP_DEBUG_PRINT("file Index value: %d", kExistingFileIndex);
    if (kExistingFileIndex == -1) {
        kExistingFileIndex = GetFileIndex(record_path_);
    } else {
        kExistingFileIndex++;
    }
    snprintf(recordfilename, sizeof(recordfilename), "%sRecord%d.ts",
             record_path_.c_str(), kExistingFileIndex);
    g_object_set(G_OBJECT(record_sink_), "location", recordfilename, NULL);
    g_object_set(G_OBJECT(record_sink_), "sync", true, NULL);

    if (format_ == kFormatJPEG) {
        record_decoder_ = gst_element_factory_make("jpegdec", "record-decoder");
        if (!record_decoder_) {
            CMP_DEBUG_PRINT("record_decoder_(%p) Failed", record_decoder_);
            return false;
        }
        gst_bin_add_many(GST_BIN(pipeline_), record_queue_, record_decoder_,
                record_encoder_, record_mux_, record_sink_, NULL);

        if (TRUE != gst_element_link_many(record_queue_, record_decoder_,
                                          record_encoder_, record_mux_,
                                          record_sink_, NULL)) {
            CMP_DEBUG_PRINT ("link capture elements could not be linked.\n");
            return false;
        }

        if (TRUE != gst_element_sync_state_with_parent(record_decoder_)) {
            CMP_DEBUG_PRINT("Sync state failed:%d\n",__LINE__);
            return false;
        }
    }
    else {
        gst_bin_add_many(GST_BIN(pipeline_), record_queue_, record_encoder_,
                         record_mux_, record_sink_, NULL);
        if (TRUE != gst_element_link_many(record_queue_, record_encoder_,
                                          record_mux_, record_sink_, NULL)) {
            CMP_DEBUG_PRINT ("link capture elements could not be linked.\n");
            return false;
        }
    }

    if (TRUE != gst_element_sync_state_with_parent(record_queue_)) {
        CMP_DEBUG_PRINT("Sync state failed:%d\n",__LINE__);
        return false;
    }
    if (TRUE != gst_element_sync_state_with_parent(record_encoder_)) {
        CMP_DEBUG_PRINT("Sync state failed:%d\n",__LINE__);
        return false;
    }
    if (TRUE != gst_element_sync_state_with_parent(record_mux_)) {
        CMP_DEBUG_PRINT("Sync state failed:%d\n",__LINE__);
        return false;
    }
    if (TRUE != gst_element_sync_state_with_parent(record_sink_)) {
        CMP_DEBUG_PRINT("Sync state failed:%d\n",__LINE__);
        return false;
    }

    record_queue_pad_ = gst_element_get_static_pad(record_queue_, "sink");
    if (GST_PAD_LINK_OK != gst_pad_link(tee_record_pad, record_queue_pad_)) {
        CMP_DEBUG_PRINT ("Record Tee could not be linked.\n");
        return false;
    }
    return true;
}

bool CameraPlayer::LoadYUY2Pipeline()
{
    CMP_DEBUG_PRINT("YUY2 FORMAT");

    filter_YUY2_ = gst_element_factory_make("capsfilter", "filter-YUY2");
    if (!filter_YUY2_) {
        CMP_DEBUG_PRINT("filter_YUY2_(%p) Failed", filter_YUY2_);
        return false;
    }
    filter_I420_ = gst_element_factory_make("capsfilter", "filter-I420");
    if (!filter_I420_) {
        CMP_DEBUG_PRINT("filter_I420_(%p) Failed", filter_I420_);
        return false;
    }
    caps_YUY2_ = gst_caps_new_simple("video/x-raw",
                                     "width", G_TYPE_INT, width_,
                                     "height", G_TYPE_INT, height_,
                                     "framerate", GST_TYPE_FRACTION,
                                                  framerate_, 1,
                                     "format", G_TYPE_STRING,
                                               kFormatYUV.c_str(),
                                     NULL);
    caps_I420_ = gst_caps_new_simple("video/x-raw",
                                     "width", G_TYPE_INT, width_,
                                     "height", G_TYPE_INT, height_,
                                     "format", G_TYPE_STRING,
                                                kFormatI420.c_str(),
                                      NULL);
    if (!pipeline_ || !source_) {
        g_printerr ("elements could not be created. Exiting.");
        return false;
    }
    g_object_set(G_OBJECT(filter_YUY2_), "caps", caps_YUY2_, NULL);
    g_object_set(G_OBJECT(filter_I420_), "caps", caps_I420_, NULL);

    vconv_ = gst_element_factory_make("videoconvert", "vconv");
    if (!vconv_) {
        CMP_DEBUG_PRINT("vconv_(%p) Failed", vconv_);
        return false;
    }
    gst_bin_add_many(GST_BIN(pipeline_), source_, filter_YUY2_, vconv_,
                     filter_I420_, tee_, NULL);


    if (TRUE != gst_element_link_many(source_, filter_YUY2_, vconv_, filter_I420_, tee_,
                NULL)) {
        CMP_DEBUG_PRINT ("elements could not be linked.\n");
        return false;
    }

    tee_preview_pad_ = gst_element_get_request_pad(tee_, "src_%u");

    if (CreatePreviewBin(tee_preview_pad_)) {
        bus_ = gst_pipeline_get_bus(GST_PIPELINE(pipeline_));
        gst_bus_add_watch(bus_, CameraPlayer::HandleBusMessage, this);
        gst_object_unref(bus_);
        return true;
    } else {
        CMP_DEBUG_PRINT("CreatePreviewBin Failed.\n");
        gst_element_set_state(pipeline_, GST_STATE_NULL);
        return false;
    }
}

bool CameraPlayer::LoadJPEGPipeline()
{
    CMP_DEBUG_PRINT("JPEG FORMAT");

    parser_ = gst_element_factory_make("jpegparse", "jpeg-parser");
    if (!parser_) {
        CMP_DEBUG_PRINT("tee_ element creation failed.");
        return false;
    }

    decoder_ = gst_element_factory_make("jpegdec", "jpeg-decoder");
    if (!decoder_) {
        CMP_DEBUG_PRINT("tee_ element creation failed.");
        return false;
    }

    filter_JPEG_ = gst_element_factory_make("capsfilter", "filter-JPEG");
    if (!filter_JPEG_) {
        CMP_DEBUG_PRINT("tee_ element creation failed.");
        return false;
    }

    caps_JPEG_ = gst_caps_new_simple("image/jpeg",
                                     "width", G_TYPE_INT, width_,
                                     "height", G_TYPE_INT, height_,
                                     "framerate", GST_TYPE_FRACTION,
                                                  framerate_, 1,
                                     NULL);

    g_object_set(G_OBJECT(filter_JPEG_), "caps", caps_JPEG_, NULL);
    gst_bin_add_many(GST_BIN(pipeline_), source_, filter_JPEG_, parser_,
                     tee_, decoder_, NULL);

    if (TRUE != gst_element_link_many(source_, filter_JPEG_, parser_, tee_,
                                      NULL)) {
        CMP_DEBUG_PRINT("Elements could not be linked.\n");
        return false;
    }

    tee_preview_pad_ = gst_element_get_request_pad(tee_, "src_%u");
    preview_queue_pad_ = gst_element_get_static_pad(decoder_, "sink");
    if (GST_PAD_LINK_OK != gst_pad_link(tee_preview_pad_, preview_queue_pad_)) {
        CMP_DEBUG_PRINT ("Record Tee could not be linked.\n");
        return false;
    }

    if (CreatePreviewBin(tee_preview_pad_)) {
        bus_ = gst_pipeline_get_bus(GST_PIPELINE (pipeline_));
        gst_bus_add_watch(bus_, CameraPlayer::HandleBusMessage, this);
        gst_object_unref(bus_);
        return true;
    } else {
        CMP_DEBUG_PRINT("CreatePreviewBin Failed.\n");
        gst_element_set_state(pipeline_, GST_STATE_NULL);
        return false;
    }
}

int32_t CameraPlayer::ConvertErrorCode(GQuark domain, gint code)
{
    int32_t converted = MEDIA_MSG_ERR_PLAYING;

    if (GST_CORE_ERROR == domain) {
        switch (code) {
            case GST_CORE_ERROR_EVENT:
                converted = MEDIA_MSG__GST_CORE_ERROR_EVENT;
                break;
            default:
                break;
        }
    } else if (GST_LIBRARY_ERROR == domain) {
        // do nothing
    } else if (GST_RESOURCE_ERROR == domain) {
        switch (code) {
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
    } else if (GST_STREAM_ERROR == domain) {
        switch (code) {
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

base::error_t CameraPlayer::HandleErrorMessage(GstMessage *message)
{
    GError *err = NULL;
    gchar *debug_info;
    gst_message_parse_error(message, &err, &debug_info);
    GQuark domain = err->domain;

    base::error_t error;
    error.errorCode = ConvertErrorCode(domain, (gint)err->code);
    error.errorText = g_strdup(err->message);

    CMP_DEBUG_PRINT("[GST_MESSAGE_ERROR][domain:%s][from:%s][code:%d]"
        "[converted:%d][msg:%s]",g_quark_to_string(domain),
        (GST_OBJECT_NAME(GST_MESSAGE_SRC(message))), err->code, error.errorCode,
        err->message);
    CMP_DEBUG_PRINT("Debugging information: %s",
        debug_info ? debug_info : "none");

    g_clear_error(&err);
    g_free(debug_info);

    return error;
}

void
CameraPlayer::FeedData (GstElement * appsrc, guint size, gpointer gdata)
{
    CameraPlayer *player = reinterpret_cast<CameraPlayer *>(gdata);
    unsigned char *data = 0;
    int len = 0;
    ReadShmem(player->context_.shmemHandle, &data, &len);
    GstBuffer *buf = gst_buffer_new_allocate(NULL, len, NULL);
    GstMapInfo writeBufferMap;
    gboolean bcheck = gst_buffer_map(buf, &writeBufferMap, GST_MAP_WRITE);
    memcpy(writeBufferMap.data, data, len);
    gst_buffer_unmap(buf, &writeBufferMap);
    gst_app_src_push_buffer((GstAppSrc*)appsrc, buf);
}

GstFlowReturn CameraPlayer::GetSample (GstAppSink *elt, gpointer data)
{
    CameraPlayer *player = reinterpret_cast<CameraPlayer *>(data);
    if (player->num_of_images_to_capture_ == player->num_of_captured_images_) {
        player->num_of_images_to_capture_ = 0;
        player->num_of_captured_images_ = 0;
        gst_pad_add_probe(player->tee_capture_pad_, GST_PAD_PROBE_TYPE_IDLE ,
                          CaptureRemoveProbe, player, NULL);
    } else {
        GstSample *sample;
        sample = gst_app_sink_pull_sample(GST_APP_SINK(elt));
        if (NULL != sample) {
            GstMapInfo map;
            GstBuffer *buffer;
            buffer = gst_sample_get_buffer(sample);
            gst_buffer_map(buffer, &map, GST_MAP_READ);
            if ((NULL != map.data) && (map.size != 0)) {
                player->WriteImageToFile(map.data, map.size);
            }
            gst_sample_unref(sample);
            gst_buffer_unmap(buffer, &map);
            player->num_of_captured_images_++;
        }
    }
    return GST_FLOW_OK;
}

GstPadProbeReturn
CameraPlayer::CaptureRemoveProbe(
    GstPad * pad, GstPadProbeInfo * info, gpointer user_data)
{
    CameraPlayer *player = reinterpret_cast<CameraPlayer *>(user_data);
    gst_pad_unlink (player->tee_capture_pad_, player->capture_queue_pad_);
    gst_element_release_request_pad(player->tee_, player->tee_capture_pad_);

    gst_object_unref(player->tee_capture_pad_);
    gst_object_unref(player->capture_queue_pad_);

    gst_object_ref(player->capture_queue_);
    if (TRUE != gst_bin_remove(GST_BIN(player->pipeline_),
                               player->capture_queue_)) {
        CMP_DEBUG_PRINT("Failed %d\n\n",__LINE__);
    }

    if (player->format_ == kFormatYUV) {
        if (TRUE != gst_bin_remove(GST_BIN(player->pipeline_),
                                   player->capture_encoder_)) {
            CMP_DEBUG_PRINT("Failed %d\n\n",__LINE__);
        }
    }


    if (TRUE != gst_bin_remove(GST_BIN(player->pipeline_),
                               player->capture_sink_)) {
        CMP_DEBUG_PRINT("Failed %d\n\n",__LINE__);
    }

    player->capture_path_.clear();
    return GST_PAD_PROBE_REMOVE;
}

GstPadProbeReturn
CameraPlayer::RecordRemoveProbe(
    GstPad * pad, GstPadProbeInfo * info, gpointer user_data)
{
    CameraPlayer *player = reinterpret_cast<CameraPlayer *>(user_data);
    gst_pad_unlink(player->tee_record_pad_, player->record_queue_pad_);
    gst_element_release_request_pad(player->tee_, player->tee_record_pad_);

    gst_object_unref(player->tee_record_pad_);
    gst_object_unref(player->record_queue_pad_);

    gst_element_set_state(player->record_queue_, GST_STATE_NULL);
    if (TRUE != gst_bin_remove(GST_BIN(player->pipeline_),
                               player->record_queue_)) {
        CMP_DEBUG_PRINT("Failed %d\n\n",__LINE__);
    }
    gst_object_unref(player->record_queue_);
    player->record_queue_ = NULL;

    if (player->format_ == kFormatJPEG) {
        gst_element_set_state(player->record_decoder_, GST_STATE_NULL);
        if (TRUE != gst_bin_remove(GST_BIN(player->pipeline_),
                                   player->record_decoder_)) {
            CMP_DEBUG_PRINT("Failed %d\n\n",__LINE__);
        }
        gst_object_unref(player->record_decoder_);
        player->record_decoder_ = NULL;
    }

    gst_element_set_state(player->record_encoder_, GST_STATE_NULL);
    if (TRUE != gst_bin_remove(GST_BIN(player->pipeline_),
                               player->record_encoder_)) {
        CMP_DEBUG_PRINT("Failed %d\n\n",__LINE__);
    }
    gst_object_unref(player->record_encoder_);
    player->record_encoder_ = NULL;

    gst_element_set_state(player->record_mux_, GST_STATE_NULL);
    if (TRUE != gst_bin_remove(GST_BIN(player->pipeline_),
                               player->record_mux_)) {
        CMP_DEBUG_PRINT("Failed %d\n\n",__LINE__);
    }
    gst_object_unref(player->record_mux_);
    player->record_mux_ = NULL;

    gst_element_set_state(player->record_sink_, GST_STATE_NULL);
    if (TRUE != gst_bin_remove(GST_BIN(player->pipeline_),
                               player->record_sink_)) {
        CMP_DEBUG_PRINT("Failed %d\n\n",__LINE__);
    }
    gst_object_unref(player->record_sink_);
    player->record_sink_ = NULL;

    player->record_path_.clear();
    return GST_PAD_PROBE_REMOVE;
}

}  // namespace player
}  // namespace cmp
