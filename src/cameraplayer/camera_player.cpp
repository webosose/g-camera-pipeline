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

#include "camera_player.h"
#include "camshm.h"
#include "cam_posixshm.h"
#include "parser/parser.h"
#include <log/log.h>
#include <sys/time.h>
#include <pbnjson.hpp>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/sem.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <unistd.h>
#include <iostream>
#include <poll.h>
#include <pthread.h>
#include <sys/mman.h>

#ifdef CMP_DEBUG_PRINT
#undef CMP_DEBUG_PRINT
#endif
#define CMP_DEBUG_PRINT CMP_INFO_PRINT

#define DISCOVER_EXPIRE_TIME (10 * GST_SECOND)
#define DELAY_5SEC 5000
#define TIMER_ID_NULL 0

static int posixshm_fd = -1;
LSHandle* handle = nullptr;
bool getFdReq = false;
bool getFdReceived = false;
int bCallback = 1;
GMainLoop* mainLoop = g_main_loop_new(nullptr, false);
const int kNumOfImages = 1;
const std::string kFormatYUV = "YUY2";
const std::string kFormatJPEG = "JPEG";
const std::string kFormatI420 = "I420";
const std::string kModePreview = "preview";
const std::string kModeCapture = "capture";
const std::string kModeRecord = "record";
const std::string kMemtypeDevice = "device";
const std::string kMemtypeShmem = "shmem";
const std::string kMemtypePosixShm = "posixshm";
const std::string kCaptureImagePath = "/tmp/";
const std::string kRecordPath = "/media/internal/";
const std::string kFileFormatMP4 = "MP4";
const std::string kFileFormatAVI = "AVI";

namespace cmp { namespace player {
guint  CameraPlayer::mCameraServiceCbTimerID = TIMER_ID_NULL;

CameraPlayer::CameraPlayer():
    media_id_(""),
    display_path_(CMP_DEFAULT_DISPLAY),
    cbFunction_(nullptr),
    planeId_(-1),
    width_(0),
    height_(0),
    handle_(0),
    framerate_(0),
    iomode_(0),
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
    filter_NV12_(NULL),
    filter_I420_(NULL),
    filter_JPEG_(NULL),
    filter_RGB_(NULL),
    vconv_(NULL),
    record_convert_(NULL),
    preview_decoder_(NULL),
    preview_parser_(NULL),
    preview_encoder_(NULL),
    preview_convert_(NULL),
    tee_(NULL),
    capture_queue_(NULL),
    capture_encoder_(NULL),
    capture_sink_(NULL),
    record_queue_(NULL),
    record_encoder_(NULL),
    record_parse_(NULL),
    record_decoder_(NULL),
    record_mux_(NULL),
    record_audio_src_(NULL),
    record_sink_(NULL),
    preview_queue_(NULL),
    record_audio_queue_(NULL),
    record_audio_convert_(NULL),
    record_audio_encoder_(NULL),
    record_audio_encoder_pad_(NULL),
    record_video_queue_pad_(NULL),
    record_audio_mux_pad_(NULL),
    record_video_mux_pad_(NULL),
    record_video_queue_(NULL),
    preview_sink_(NULL),
    tee_preview_pad_(NULL),
    preview_ghost_sinkpad_(NULL),
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
    caps_NV12_(NULL),
    caps_I420_(NULL),
    caps_JPEG_(NULL),
    caps_RGB_(NULL),
    service_(NULL),
    load_complete_(false),
    display_mode_("Default"),
    window_id_("") {
    CMP_DEBUG_PRINT(" this[%p]", this);
}

CameraPlayer::~CameraPlayer()
{
    CMP_DEBUG_PRINT(" this[%p]", this);

    if (pipeline_ != NULL) {
        Unload();
    }
    gst_deinit();
}

bool CameraPlayer::attachSurface(bool allow_no_window) {
    if (!window_id_.empty()) {
        if (!lsm_camera_window_manager_.registerID(window_id_.c_str(), NULL)) {
            CMP_DEBUG_PRINT("register id to LSM failed!");
            return false;
        }
        if (!lsm_camera_window_manager_.attachSurface()) {
            CMP_DEBUG_PRINT("attach surface to LSM failed!");
            return false;
        }
        return true;
    } else {
        CMP_DEBUG_PRINT("window id is empty!");
        bool ret = allow_no_window ? true : false;
        return ret;
    }
}

bool CameraPlayer::detachSurface() {
    if (!window_id_.empty()) {
        if (!lsm_camera_window_manager_.detachSurface()) {
            CMP_DEBUG_PRINT("detach surface to LSM failed!");
            return false;
        }
        if (!lsm_camera_window_manager_.unregisterID()) {
            CMP_DEBUG_PRINT("unregister id to LSM failed!");
            return false;
        }
    } else {
        CMP_DEBUG_PRINT("window id is empty!");
    }
    return true;
}

void CameraPlayer::RegisterCbFunction(CALLBACK_T callBackFunction)
{
    cbFunction_ = callBackFunction;
}

void CameraPlayer::ParseOptionString(const std::string& options)
{
    CMP_DEBUG_PRINT("option string: %s", options.c_str());
    pbnjson::JDomParser jdparser;
    if (!jdparser.parse(options, pbnjson::JSchema::AllSchema())) {
        CMP_DEBUG_PRINT("ERROR JDomParser.parse. msg: %s ", options.c_str());
        return;
    }
    pbnjson::JValue parsed = jdparser.getDom();

    if(parsed.hasKey("uri")) {
        uri_ = parsed["uri"].asString();
    } else {
        CMP_DEBUG_PRINT("UMS_INTERNAL_API_VERSION is not version 2.");
        CMP_DEBUG_PRINT("Please check the UMS_INTERNAL_API_VERSION in ums.");
        CMPASSERT(0);
    }

    if (parsed["options"]["option"].hasKey("displayPath")) {
        int32_t display_path = parsed["options"]["option"]["displayPath"].asNumber<int32_t>();
        display_path_ = (display_path > CMP_SECONDARY_DISPLAY ? 0 : display_path);
    }
    if (parsed["options"]["option"].hasKey("windowId")) {
        window_id_ = parsed["options"]["option"]["windowId"].asString();
    }
    if (parsed["options"]["option"].hasKey("handle")) {
        handle_ = parsed["options"]["option"]["handle"].asNumber<int>();
    }
    if (parsed["options"]["option"].hasKey("videoDisplayMode")) {
        display_mode_ = parsed["options"]["option"]["videoDisplayMode"].asString();
    }
    if (parsed["options"]["option"].hasKey("format")) {
        format_ = parsed["options"]["option"]["format"].asString();
    }
    if (parsed["options"]["option"].hasKey("width")) {
        width_ = parsed["options"]["option"]["width"].asNumber<int>();
    }
    if (parsed["options"]["option"].hasKey("height")) {
        height_ = parsed["options"]["option"]["height"].asNumber<int>();
    }
    if (parsed["options"]["option"].hasKey("frameRate")) {
        framerate_ = parsed["options"]["option"]["frameRate"].asNumber<int>();
    }
    if (parsed["options"]["option"].hasKey("memType")) {
        memtype_ = parsed["options"]["option"]["memType"].asString();
    }
    if (parsed["options"]["option"].hasKey("iomode")) {
        iomode_ = parsed["options"]["option"]["iomode"].asNumber<int>();
    }
    if (parsed["options"]["option"].hasKey("memSrc")) {
        memsrc_ = parsed["options"]["option"]["memSrc"].asString();
    }

    CMP_DEBUG_PRINT("uri: %s, display-path: %d, window_id: %s, display_mode: %s",
            uri_.c_str(), display_path_, window_id_.c_str(), display_mode_.c_str());
}

static bool getFdCb(LSHandle *lsHandle, LSMessage *message, void *user_data)
{
    struct stat sb;
    jerror *error = NULL;
    LSError lserror;
    const char *payload = LSMessageGetPayload(message);
    jvalue_ref jin_obj = jdom_create(j_cstr_to_buffer(payload), jschema_all(), &error);

    getFdReceived = true;
    int fd=0;

    LS::Message ls_message(message);
    LS::PayloadRef payload_ref = ls_message.accessPayload();
    fd = payload_ref.getFd();
    if (fd)
        posixshm_fd = dup(fd);

    bCallback = 0;
    CMP_DEBUG_PRINT("fd received in callback is : %d", posixshm_fd);
    if (!LSUnregister(handle, &lserror))
    {
        CMP_DEBUG_PRINT("LS LSUnRegister failed ");
        LSErrorPrint(&lserror, stderr);
        return false;
    }
    return true;
}

gboolean CameraPlayer::CameraServiceCbTimerCallback(void* data)
{
    CMP_DEBUG_PRINT("inside timeout : CameraServiceCbTimerCallback ");
    CameraPlayer *player = reinterpret_cast<CameraPlayer *>(data);
    // check whether camera service call is requested or not.
    if ( !getFdReq )
        return FALSE;

    // requested camera service for Fd. Wait till the callback is received.
    if ( getFdReq )
    {
        // start timer again to wait for callback
        if (!getFdReceived)
            player->CameraServiceCbTimerReset();
        else
        {
            if (mCameraServiceCbTimerID)
            {
                CMP_DEBUG_PRINT("Timer will be removed in timeout");
                g_source_remove (mCameraServiceCbTimerID);
                mCameraServiceCbTimerID = TIMER_ID_NULL;
            }
            player->LoadPlayer();
        }
    }

    return FALSE;
}

void CameraPlayer::CameraServiceCbTimerReset()
{
    if (!getFdReq)
        return;

    if (TIMER_ID_NULL == mCameraServiceCbTimerID )
    {
        mCameraServiceCbTimerID = g_timeout_add (DELAY_5SEC, CameraServiceCbTimerCallback, this );
    }
    else
    {
        if ( g_source_remove (mCameraServiceCbTimerID))
        {
            mCameraServiceCbTimerID = g_timeout_add (DELAY_5SEC, CameraServiceCbTimerCallback, this );
        }
    }
}

bool CameraPlayer::subscribeToCameraService()
{
    int retval = 0;
    char buffer[50];
    const std::string cstr_payload = "handle";
    int ret = 0;
    std::string result;

    LSError lserror;
    LSErrorInit(&lserror);

    if (!LSRegister("com.webos.pipeline", &handle, &lserror))
    {
        CMP_DEBUG_PRINT("LS Register failed ");
        LSErrorPrint(&lserror, stderr);
        return false;
    }

    if (!LSGmainAttach(handle, mainLoop, &lserror))
    {
        LSErrorPrint(&lserror, stderr);
        return false;
    }

    sprintf(buffer,"{\"handle\":%d}", handle_);
    CMP_DEBUG_PRINT("result is %s",buffer);

    retval = LSCall(handle, "luna://com.webos.service.camera2/getFd", buffer, getFdCb, NULL, NULL,
            &lserror);
    if (retval)
        getFdReq = true;
    CMP_DEBUG_PRINT("Req sent to camera service for getFd = %d", getFdReq);
    CameraServiceCbTimerReset();
    return true;
}

bool CameraPlayer::Load(const std::string& str)
{
    CMPASSERT(!str.empty());
    CMP_DEBUG_PRINT("load: %s", str.c_str());

    ParseOptionString(str);
    CMP_DEBUG_PRINT("format_ : %s", format_.c_str());
    CMP_DEBUG_PRINT("width_ : %d", width_);
    CMP_DEBUG_PRINT("height_ : %d", height_);
    CMP_DEBUG_PRINT("framerate_: %d", framerate_);
    CMP_DEBUG_PRINT("memtype_ : %s", memtype_.c_str());
    CMP_DEBUG_PRINT("iomode_ : %d", iomode_);
    CMP_DEBUG_PRINT("memsrc_ : %s", memsrc_.c_str());
    CMP_DEBUG_PRINT("posixshm_fd : %d", posixshm_fd);
    if (kMemtypePosixShm == memtype_)
        subscribeToCameraService();
    else
        LoadPlayer();
    return true;
}

bool CameraPlayer::LoadPlayer ()
{
    SetGstreamerDebug();
    gst_init(NULL, NULL);
    gst_pb_utils_init();
    // Temporary setting
    display_mode_ = std::string("Textured");

    if (!GetSourceInfo())
    {
        CMP_DEBUG_PRINT("get source information failed!");
        return false;
    }

    ACQUIRE_RESOURCE_INFO_T resource_info;
    resource_info.sourceInfo = &source_info_;
    resource_info.displayMode = const_cast<char*>(display_mode_.c_str());
    resource_info.result = true;

    if (cbFunction_)
        cbFunction_(CMP_NOTIFY_ACQUIRE_RESOURCE, display_path_, nullptr, static_cast<void*>(&resource_info));

    if (!resource_info.result)
    {
        CMP_DEBUG_PRINT("resouce acquire fail!");
        return false;
    }

    if (!attachSurface(true))
    {
        CMP_DEBUG_PRINT("attachSurface() failed");
        return false;
    }

    if (!LoadPipeline())
    {
        CMP_DEBUG_PRINT("pipeline load failed!");
        FreeLoadPipelineElements();
        return false;
    }

    SetPlayerState(base::playback_state_t::LOADED);

    CMP_DEBUG_PRINT("Load Done: %s", uri_.c_str());
    return true;

}

void CameraPlayer::PauseInternalSync()
{
    /* NOTE: Internally Pipeline state changing to pause and then to NULL
     *       on unload API */
    CMP_DEBUG_PRINT("Change pipeline state to PAUSE");
    gst_element_set_state(pipeline_, GST_STATE_PAUSED);

    GstState state; GstState pending;
    GstStateChangeReturn status = gst_element_get_state(pipeline_, &state, &pending, -1);
    CMP_DEBUG_PRINT("Status of pipeline state change to pause = %d", status);

    if ( (GST_STATE_CHANGE_SUCCESS == status) && (GST_STATE_PAUSED == state) )
        CMP_DEBUG_PRINT("Pipeline state change to PAUSE is success");
    else
        CMP_DEBUG_PRINT("Pipeline state change to PAUSE is filed");
}

bool CameraPlayer::Unload()
{
    CMP_DEBUG_PRINT("unload");
    if (!pipeline_)
    {
        CMP_DEBUG_PRINT("pipeline_ is null");
        return false;
    }

    /* change the pipeline state to PAUSE internally and then to NULL */
    PauseInternalSync();

    gst_element_set_state(pipeline_, GST_STATE_NULL);
    gst_object_unref(GST_OBJECT(pipeline_));
    pipeline_ = NULL;

    SetPlayerState(base::playback_state_t::STOPPED);

    if (!detachSurface())
    {
        CMP_DEBUG_PRINT("detachSurface() failed");
        return false;
    }

    if (cbFunction_)
        cbFunction_(CMP_NOTIFY_UNLOAD_COMPLETED, 0, nullptr, nullptr);

    return true;
}

bool CameraPlayer::Play()
{
    CMP_DEBUG_PRINT("play");
    if (!pipeline_)
    {
        CMP_DEBUG_PRINT("pipeline_ is null");
        return false;
    }
    if (!gst_element_set_state(pipeline_, GST_STATE_PLAYING))
    {
        CMP_DEBUG_PRINT("set GST_STATE_PLAYING failed!!!");
        return false;
    }

    SetPlayerState(base::playback_state_t::PLAYING);

    if (cbFunction_)
        cbFunction_(CMP_NOTIFY_PLAYING, 0, nullptr, nullptr);

    return true;
}

bool CameraPlayer::TakeSnapshot(const std::string& location)
{
    CMP_DEBUG_PRINT(" CameraPlayer::TakeSnapshot location:%s\n ",location.c_str());

    if (!location.empty())
        capture_path_ = location;

    tee_capture_pad_ = gst_element_get_request_pad(tee_, "src_%u");
    if (tee_capture_pad_ == NULL)
    {
        CMP_DEBUG_PRINT("tee_capture_pad_ is NULL\n");
        return false;
    }
    if (!CreateCaptureElements(tee_capture_pad_))
    {
        CMP_DEBUG_PRINT("CreateCaptureElements Failed.\n");
        FreeCaptureElements();
        return false;
    }
    return true;
}

bool CameraPlayer::StartRecord(const std::string& location, const std::string& format,
                               bool audio, const std::string& audioSrc)
{
    if (!location.empty())
        record_path_ = location;

    tee_record_pad_ = gst_element_get_request_pad(tee_, "src_%u");
    if (tee_record_pad_ == NULL)
    {
        CMP_DEBUG_PRINT("tee_record_pad_ is NULL\n");
        return false;
    }
    if((format.compare(kFileFormatMP4) == 0) || (format.compare(kFileFormatAVI) == 0) )
    {
        CMP_DEBUG_PRINT("startCameraRecord - Supported format");
        if(audio == true)
        {
            CMP_DEBUG_PRINT("startCameraRecord - Supported format with audio");
            if (!CreateAudioRecordElements(audioSrc, record_audio_encoder_pad_))
            {
                CMP_DEBUG_PRINT("CreateAudioRecordElements Failed.\n");
                FreeRecordElements();
                return false;
            }
        }
        CMP_DEBUG_PRINT("startCameraRecord - record video");
        if (!CreateRecordElements(tee_record_pad_, record_audio_encoder_pad_, format))
        {
            CMP_DEBUG_PRINT("CreateRecordElements Failed.\n");
            FreeRecordElements();
            return false;
        }
    }
    else
    {
        CMP_DEBUG_PRINT("startCameraRecord - Un Supported format");
        return false;
    }
    return true;
}

bool CameraPlayer::StopRecord()
{
    gst_pad_add_probe(tee_record_pad_, GST_PAD_PROBE_TYPE_IDLE,
            (GstPadProbeCallback)RecordRemoveProbe, this, NULL);
    return true;
}

gboolean CameraPlayer::HandleBusMessage(
        GstBus *bus_, GstMessage *message, gpointer user_data)
{
    GstMessageType messageType = GST_MESSAGE_TYPE(message);
    if (messageType != GST_MESSAGE_QOS && messageType != GST_MESSAGE_TAG)
    {
        CMP_DEBUG_PRINT("Element[ %s ][ %d ][ %s ]",
                GST_MESSAGE_SRC_NAME(message),
                messageType, gst_message_type_get_name(messageType));
    }

    CameraPlayer *player = reinterpret_cast<CameraPlayer *>(user_data);
    switch (GST_MESSAGE_TYPE(message))
    {
        case GST_MESSAGE_ERROR:
            {
                base::error_t error = player->HandleErrorMessage(message);
                if (player->cbFunction_)
                    player->cbFunction_(CMP_NOTIFY_ERROR, 0, nullptr, &error);
                break;
            }

        case GST_MESSAGE_EOS:
            {
                CMP_DEBUG_PRINT("Got endOfStream");
                if (player->cbFunction_)
                    player->cbFunction_(CMP_NOTIFY_END_OF_STREAM, 0, nullptr, nullptr);
                break;
            }

        case GST_MESSAGE_ASYNC_DONE:
            {
                CMP_DEBUG_PRINT("ASYNC DONE");
                auto notify_case = CMP_NOTIFY_MAX;
                if (!player->load_complete_)
                {
                    player->cbFunction_(CMP_NOTIFY_LOAD_COMPLETED, 0, nullptr, nullptr);
                    player->load_complete_ = true;
                }
                break;
            }

        case GST_STATE_PAUSED:
            {
                CMP_DEBUG_PRINT("PAUSED");
                if (player->cbFunction_)
                    player->cbFunction_(CMP_NOTIFY_PAUSED, 0, nullptr, nullptr);
                break;
            }

        case GST_STATE_PLAYING:
            {
                CMP_DEBUG_PRINT("PLAYING");
                if (player->cbFunction_)
                    player->cbFunction_(CMP_NOTIFY_PLAYING, 0, nullptr, nullptr);
                break;
            }

        case GST_MESSAGE_STATE_CHANGED:
            {
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

                    CMP_INFO_PRINT("width[%d], height[%d], framerate[%d/%d],"
                            "pixel_aspect_ratio[%d/%d]", width, height,
                            fps_n, fps_d, par_n, par_d);

                    video_info.width = width;
                    video_info.height = height;
                    video_info.frame_rate.num = fps_n;
                    video_info.frame_rate.den = fps_d;
                    // TODO: we already know this info. but it's not used now.
                    video_info.bit_rate = 0;
                    video_info.codec = 0;

                    if (player->cbFunction_)
                        player->cbFunction_(CMP_NOTIFY_VIDEO_INFO, 0, nullptr, &video_info);
                }
                else if (gst_structure_has_name(gStruct, "request-resource"))
                {
                    CMP_INFO_PRINT("got request-resource message");
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
    // TODO(anonymous): Support multiple video/audio stream case
    if (cbFunction_)
        cbFunction_(CMP_NOTIFY_SOURCE_INFO, 0, nullptr, &source_info_);
}

void CameraPlayer::SetGstreamerDebug()
{
    std::string input_file("/etc/g-camera-pipeline/gst_debug.conf");

    pbnjson::JDomParser parser;
    if (!parser.parseFile(input_file, pbnjson::JSchema::AllSchema(), 0, NULL))
    {
        CMP_DEBUG_PRINT("Debug file parsing error");
        return;
    }

    pbnjson::JValue parsed = parser.getDom();
    pbnjson::JValue debug = parsed["gst_debug"];

    int size = debug.arraySize();
    for (int i = 0; i < size; i++)
    {
        const char *kDebug = "GST_DEBUG";
        const char *kDebugFile = "GST_DEBUG_FILE";
        const char *kDebugDot = "GST_DEBUG_DUMP_DOT_DIR";
        if (debug[i].hasKey(kDebug) && !debug[i][kDebug].asString().empty())
            setenv(kDebug, debug[i][kDebug].asString().c_str(), 1);
        if (debug[i].hasKey(kDebugFile) && !debug[i][kDebugFile].asString().empty())
            setenv(kDebugFile, debug[i][kDebugFile].asString().c_str(), 1);
        if (debug[i].hasKey(kDebugDot) && !debug[i][kDebugDot].asString().empty())
            setenv(kDebugDot, debug[i][kDebugDot].asString().c_str(), 1);
    }
}

void CameraPlayer::WriteImageToFile(const void *p,int size)
{
    CMP_DEBUG_PRINT("CameraPlayer::WriteImageToFile capture_path_=%s",capture_path_.c_str());
    if (capture_path_.empty())
    {
        CMP_DEBUG_PRINT("capture_path_ empty");
        capture_path_ = std::string(kCaptureImagePath);
    }

    std::size_t pos = capture_path_.rfind('.');
    if (pos != std::string::npos)
    {
        CMP_DEBUG_PRINT("capture_path_ is with file name ");
    }
    else
    {
        CMP_DEBUG_PRINT("capture_path_ doe not have file name");

        time_t t_ = time(NULL);
        tm *timePtr_ = localtime(&t_);
        struct timeval tmnow_;
        gettimeofday(&tmnow_, NULL);

        char image_name[100] = {};
        snprintf(image_name, 100, "Capture%02d%02d%02d-%02d%02d%02d%02d.jpeg", timePtr_->tm_mday,
                (timePtr_->tm_mon) + 1, (timePtr_->tm_year) + 1900, (timePtr_->tm_hour),
                (timePtr_->tm_min), (timePtr_->tm_sec), ((int)tmnow_.tv_usec) / 10000);
        CMP_DEBUG_PRINT("writeImageToFile image_name : %s\n", image_name);

        capture_path_ = capture_path_ + image_name;
    }
    CMP_DEBUG_PRINT("writeImageToFile path : %s\n", capture_path_.c_str());

    FILE *fp = fopen(capture_path_.c_str(), "wb");
    if (NULL == fp)
    {
        CMP_DEBUG_PRINT("File %s Open Failed", capture_path_.c_str());
        return;
    }
    CMP_DEBUG_PRINT("File Open Success");
    fwrite(p, size, 1, fp);
    fclose(fp);
}

bool CameraPlayer::GetSourceInfo()
{
    base::video_info_t video_stream_info = {};

    video_stream_info.width = width_;
    video_stream_info.height = height_;
    video_stream_info.decode = CMP_VIDEO_CODEC_MJPEG;
    video_stream_info.encode = CMP_VIDEO_CODEC_H264;
    video_stream_info.frame_rate.num = framerate_;
    video_stream_info.frame_rate.den = 1;
    CMP_DEBUG_PRINT("[video info] width: %d, height: %d, frameRate: %d/%d",
            video_stream_info.width, video_stream_info.height,
            video_stream_info.frame_rate.num, video_stream_info.frame_rate.den);

    base::program_info_t program;
    program.video_stream = 1;
    source_info_.programs.push_back(program);

    source_info_.video_streams.push_back(video_stream_info);

    return true;
}

bool CameraPlayer::LoadPipeline()
{
    CMP_DEBUG_PRINT("LoadPipeline planeId:%d ", planeId_);
    NotifySourceInfo();

    pipeline_ = gst_pipeline_new("camera-player");
    if (!pipeline_)
    {
        CMP_DEBUG_PRINT("pipeline_ element creation failed.");
        return false;
    }

    if (memtype_ == kMemtypeDevice)
    {
        source_   = gst_element_factory_make("camsrc", "cam-source");
        if (!source_)
        {
            CMP_DEBUG_PRINT("source_ element creation failed.");
            return false;
        }
        g_object_set(source_, "device", memsrc_.c_str(), NULL);
        g_object_set(source_, "do-timestamp", true, NULL);
        g_object_set(source_, "iomode", iomode_, NULL);
    }
    else if (memtype_ == kMemtypeShmem)
    {
        context_.key = atoi(memsrc_.c_str());
        if (OpenShmem((SHMEM_HANDLE *)(&(context_.shmemHandle)),
                    context_.key) != 0)
        {
            CMP_DEBUG_PRINT("openShmem failed");
            return false;
        }
        source_ = gst_element_factory_make ("appsrc", "app-source");
        if (!source_)
        {
            CMP_DEBUG_PRINT("source_ element creation failed.");
            return false;
        }
        g_object_set(source_, "format", GST_FORMAT_TIME, NULL);
        g_object_set(source_, "do-timestamp", true, NULL);
        g_signal_connect(source_, "need-data", G_CALLBACK (FeedData), this);
    }
    else if (memtype_ == kMemtypePosixShm)
    {
        if (OpenPosixShmem((SHMEM_HANDLE *)(&(context_.shmemHandle)),
                    posixshm_fd) != 0)
        {
            CMP_DEBUG_PRINT("openPosixShmem failed");
            return false;
        }
        source_ = gst_element_factory_make ("appsrc", "app-source");
        if (!source_)
        {
            CMP_DEBUG_PRINT("source_ element creation failed.");
            return false;
        }
        g_object_set(source_, "format", GST_FORMAT_TIME, NULL);
        g_object_set(source_, "do-timestamp", true, NULL);
        g_signal_connect(source_, "need-data", G_CALLBACK (FeedPosixData), this);
    }
    else
    {
        CMP_DEBUG_PRINT("Invalid memtype_. Not supported!!!");
        return (gst_element_set_state(pipeline_, GST_STATE_NULL));
    }

    tee_ = gst_element_factory_make("tee", "pipeline-tee");
    if (!tee_)
    {
        CMP_DEBUG_PRINT("tee_ element creation failed.");
        return false;
    }

    if (format_ == kFormatYUV)
    {
        if (!LoadYUY2Pipeline())
        {
            CMP_DEBUG_PRINT("YUY2 pipeline_ load failed!");
            return false;
        }
    }
    else if (format_ == kFormatJPEG)
    {
        if (!LoadJPEGPipeline()) {
            CMP_DEBUG_PRINT("JPEG pipeline_ load failed!");
            return false;
        }
    }
    else
    {
        CMP_DEBUG_PRINT("Format[%s] not Supported", format_.c_str());
    }

    return gst_element_set_state(pipeline_, GST_STATE_PAUSED);
}

bool CameraPlayer::CreatePreviewBin(GstPad * pad)
{
#ifdef PLATFORM_QEMUX86
    vconv_ = gst_element_factory_make("videoconvert", "vconv");
#else
    vconv_ = gst_element_factory_make("v4l2convert", "vconv");
#endif
    if (!vconv_)
    {
        CMP_DEBUG_PRINT("vconv_(%p) Failed", vconv_);
        return false;
    }

    preview_sink_ = gst_element_factory_make("waylandsink", "preview-sink");
    if (!preview_sink_)
    {
        CMP_DEBUG_PRINT("preview_sink_ element creation failed.");
        return false;
    }
    g_object_set(G_OBJECT(preview_sink_), "sync", false, NULL);
#ifndef PLATFORM_QEMUX86
    g_object_set(G_OBJECT(preview_sink_), "use-drmbuf", true, NULL);
#endif

    if (!gst_bin_add(GST_BIN(pipeline_), preview_sink_))
    {
        CMP_DEBUG_PRINT ("convert could not be added.\n");
        return false;
    }
    if (!gst_bin_add(GST_BIN(pipeline_), vconv_))
    {
        CMP_DEBUG_PRINT ("convert could not be added.\n");
        return false;
    }
    if (format_ == kFormatJPEG)
    {
        filter_RGB_ = gst_element_factory_make("capsfilter", "filter-RGB");
        if (!filter_RGB_)
        {
            CMP_DEBUG_PRINT("filter_ element creation failed.");
            return false;
        }
        caps_RGB_ = gst_caps_new_simple("video/x-raw",
                "format", G_TYPE_STRING, "RGB16",
                NULL);
        g_object_set(G_OBJECT(filter_RGB_), "caps", caps_RGB_, NULL);
        if (!gst_bin_add(GST_BIN(pipeline_), filter_RGB_))
        {
            CMP_DEBUG_PRINT ("convert could not be added.\n");
            return false;
        }
        if (TRUE !=  gst_element_link_many(decoder_, vconv_, filter_RGB_, preview_sink_, NULL))
        {
            CMP_DEBUG_PRINT ("Elements could not be linked.\n");
            return false;
        }
    }
    else
    {
        preview_queue_pad_ = gst_element_get_static_pad(vconv_, "sink");
        if (!preview_queue_pad_)
        {
            CMP_DEBUG_PRINT ("Did not get capture queue pad.\n");
            return false;
        }
        CMP_DEBUG_PRINT ("Tee preview pad: %p\n",pad);
        CMP_DEBUG_PRINT ("preview_queue_pad_: %p\n",preview_queue_pad_);
#ifndef PLATFORM_QEMUX86
        filter_RGB_ = gst_element_factory_make("capsfilter", "filter-RGB");
        if (!filter_RGB_)
        {
            CMP_DEBUG_PRINT("filter_ element creation failed.");
            return false;
        }
        caps_RGB_ = gst_caps_new_simple("video/x-raw",
                "format", G_TYPE_STRING, "RGB16",
                NULL);
        g_object_set(G_OBJECT(filter_RGB_), "caps", caps_RGB_, NULL);
#endif
        if (GST_PAD_LINK_OK != gst_pad_link(pad, preview_queue_pad_))
        {
            CMP_DEBUG_PRINT ("preview_queue_pad_ could not be linked.\n");
            return false;
        }
#ifndef PLATFORM_QEMUX86
        if (! gst_bin_add(GST_BIN(pipeline_), filter_RGB_))
        {
            CMP_DEBUG_PRINT ("filter_RGB_ could not be added.\n");
            return false;
        }
        if (TRUE !=  gst_element_link(vconv_,filter_RGB_))
        {
            CMP_DEBUG_PRINT ("Elements could not be linked.\n");
            return false;
        }
        if (TRUE !=  gst_element_link(filter_RGB_,preview_sink_))
        {
            CMP_DEBUG_PRINT ("Elements could not be linked.\n");
            return false;
        }
#else
        if (TRUE !=  gst_element_link(vconv_,preview_sink_))
        {
            CMP_DEBUG_PRINT ("Elements could not be linked.\n");
            return false;
        }
#endif
        return true;
    }
    return true;
}

bool CameraPlayer::CreateCaptureElements(GstPad* tee_capture_pad)
{
    CMP_DEBUG_PRINT(" CameraPlayer::CreateCaptureElements \n ");
    num_of_images_to_capture_ = kNumOfImages;

    capture_queue_ = gst_element_factory_make("queue", "capture-queue");
    if (!capture_queue_)
    {
        CMP_DEBUG_PRINT("capture_queue_(%p) Failed", capture_queue_);
        return false;
    }
    capture_sink_ = gst_element_factory_make("appsink", "capture-sink");
    if (!capture_sink_)
    {
        CMP_DEBUG_PRINT("capture_sink_(%p) Failed", capture_sink_);
        return false;
    }
    CMP_DEBUG_PRINT(" CameraPlayer::CreateCaptureElements, queue & appsink created \n ");

    g_object_set(G_OBJECT(capture_sink_), "emit-signals", TRUE, "sync", FALSE, NULL);
    g_signal_connect(capture_sink_, "new-sample", G_CALLBACK(GetSample), this);

    gst_bin_add_many(GST_BIN(pipeline_), capture_queue_, capture_sink_, NULL);
    CMP_DEBUG_PRINT(" CameraPlayer::CreateCaptureElements, added to bin  \n ");

    capture_queue_pad_ = gst_element_get_static_pad(capture_queue_, "sink");
    if (!capture_queue_pad_)
    {
        CMP_DEBUG_PRINT ("Did not get capture queue pad.\n");
        return false;
    }
    CMP_DEBUG_PRINT(" CameraPlayer::CreateCaptureElements, capture queue pad done \n ");

    if (format_ == kFormatYUV )
    {
        if (!capture_encoder_)
        {
            capture_encoder_ = gst_element_factory_make("jpegenc",
                    "capture-encoder");
            if (!capture_encoder_)
            {
                CMP_DEBUG_PRINT("capture_encoder_(%p) Failed", capture_encoder_);
                return false;
            }
        }
        CMP_DEBUG_PRINT(" CameraPlayer::CreateCaptureElements,capture_encoder done  \n ");

        if (TRUE != gst_bin_add(GST_BIN(pipeline_), capture_encoder_))
        {
            CMP_DEBUG_PRINT("Element capture_encoder_ could not be added. \n");
            return false;
        }

        if (TRUE != gst_element_link_many(capture_queue_, capture_encoder_,
                    capture_sink_, NULL))
        {
            CMP_DEBUG_PRINT("Elements could not be linked.\n");
            return false;
        }
    }
    else
    {
        if (TRUE != gst_element_link_many(capture_queue_, capture_sink_,NULL))
        {
            CMP_DEBUG_PRINT("Elements could not be linked.\n");
            return false;
        }
    }
    CMP_DEBUG_PRINT(" CameraPlayer::CreateCaptureElements, elements linked \n ");

    if (GST_PAD_LINK_OK != gst_pad_link(tee_capture_pad, capture_queue_pad_))
    {
        CMP_DEBUG_PRINT ("Capture Tee could not be linked.\n");
        return false;
    }

    if (TRUE != gst_element_sync_state_with_parent(capture_queue_))
    {
        CMP_DEBUG_PRINT("Sync state capture_queue_ failed");
        return false;
    }
    if (TRUE != gst_element_sync_state_with_parent(capture_sink_))
    {
        CMP_DEBUG_PRINT("Sync state capture_sink_ failed");
        return false;
    }
    CMP_DEBUG_PRINT(" CameraPlayer::CreateCaptureElementsi, sync with parents done\n ");

    if (format_ == kFormatYUV &&
            TRUE != gst_element_sync_state_with_parent(capture_encoder_))
    {
        CMP_DEBUG_PRINT("Sync state capture_encoder_ failed");
        return false;
    }
    CMP_DEBUG_PRINT(" CameraPlayer::CreateCaptureElements End \n ");

    return true;
}

bool CameraPlayer::CreateRecordElements(GstPad* tee_record_pad,
                                        GstPad* record_audio_encoder_pad,
                                        const std::string& fileFormat)
{
    char recordfilename[100] = {};
    if (record_path_.empty())
        record_path_ = kRecordPath;

    time_t t_ = time(NULL);
    tm *timePtr_ = localtime(&t_);
    struct timeval tmnow_;
    gettimeofday(&tmnow_, NULL);

    record_queue_ = gst_element_factory_make ("queue", "record-queue");
    if (!record_queue_)
    {
        CMP_DEBUG_PRINT("record_queue_(%p) Failed", record_queue_);
        return false;
    }
    record_video_queue_ = gst_element_factory_make ("queue", "record-video-queue");
    if (!record_video_queue_)
    {
        CMP_DEBUG_PRINT("record_video_queue_(%p) Failed", record_video_queue_);
        return false;
    }
    g_object_set(G_OBJECT(record_video_queue_), "max-size-time", 700, NULL);
#ifdef PLATFORM_QEMUX86
    record_encoder_ = gst_element_factory_make ("avenc_mjpeg", "record-encoder");
#else
    record_encoder_ = gst_element_factory_make ("v4l2h264enc", "record-encoder");
#endif
    if (!record_encoder_)
    {
        CMP_DEBUG_PRINT("record_encoder_(%p) Failed", record_encoder_);
        return false;
    }
#ifndef PLATFORM_QEMUX86
    filter_NV12_ = gst_element_factory_make("capsfilter", "filter-NV");
    if (!filter_NV12_)
    {
        CMP_DEBUG_PRINT("filter_ element creation failed.");
        return false;
    }
    caps_NV12_ = gst_caps_new_simple("video/x-raw",
            "format", G_TYPE_STRING, "NV12",
            NULL);
    g_object_set(G_OBJECT(filter_NV12_), "caps", caps_NV12_, NULL);
#endif
    record_convert_ = gst_element_factory_make("videoconvert", "record-convert");
    if (!record_convert_)
    {
        CMP_DEBUG_PRINT("record_convert_(%p) Failed", record_convert_);
        return false;
    }
    if(fileFormat == kFileFormatMP4)
    {
        record_mux_ = gst_element_factory_make("qtmux", "record-mux");
        if (!record_mux_) {
           CMP_DEBUG_PRINT("record_mux_(%p) Failed", record_mux_);
            return false;
        }
        snprintf(recordfilename, sizeof(recordfilename), "%sRecord%02d%02d%02d-%02d%02d%02d%02d.mp4", record_path_.c_str(), timePtr_->tm_mday,
                (timePtr_->tm_mon) + 1, (timePtr_->tm_year) + 1900, (timePtr_->tm_hour),
                (timePtr_->tm_min), (timePtr_->tm_sec), ((int)tmnow_.tv_usec) / 10000);

    }
    else if (fileFormat == kFileFormatAVI)
    {
        if (format_ == kFormatJPEG)
            record_mux_ = gst_element_factory_make("avimux", "record-mux");
        else
            record_mux_ = gst_element_factory_make("matroskamux", "record-mux");
        if (!record_mux_) {
           CMP_DEBUG_PRINT("record_mux_(%p) Failed", record_mux_);
            return false;
        }
        snprintf(recordfilename, sizeof(recordfilename), "%sRecord%02d%02d%02d-%02d%02d%02d%02d.avi", record_path_.c_str(), timePtr_->tm_mday,
                (timePtr_->tm_mon) + 1, (timePtr_->tm_year) + 1900, (timePtr_->tm_hour),
                (timePtr_->tm_min), (timePtr_->tm_sec), ((int)tmnow_.tv_usec) / 10000);
    }
    else
    {
        CMP_DEBUG_PRINT("Format %s is not supported", fileFormat);
        return false;
    }
    record_sink_ = gst_element_factory_make("filesink", "record-sink");
    if (!record_sink_)
    {
        CMP_DEBUG_PRINT("record_sink_(%p) Failed", record_sink_);
        return false;
    }
#ifndef PLATFORM_QEMUX86
    record_parse_ = gst_element_factory_make("h264parse", "record-parser");
    if (!record_parse_)
    {
        CMP_DEBUG_PRINT("record_parse_(%p) Failed", record_parse_);
        return false;
    }
    gst_bin_add(GST_BIN(pipeline_), filter_NV12_);
    gst_bin_add(GST_BIN(pipeline_), record_parse_);
#endif
    g_object_set(G_OBJECT(record_sink_), "location", recordfilename, NULL);
    if (format_ == kFormatYUV)
        g_object_set(G_OBJECT(record_sink_), "sync", true, NULL);
    gst_bin_add_many(GST_BIN(pipeline_), record_queue_, record_convert_, record_encoder_,
            record_video_queue_, record_mux_, record_sink_, NULL);

    if (format_ == kFormatJPEG)
    {
        record_decoder_ = gst_element_factory_make("jpegdec", "record-decoder");
        if (!record_decoder_)
        {
            CMP_DEBUG_PRINT("record_decoder_(%p) Failed", record_decoder_);
            return false;
        }
        if (TRUE != gst_bin_add(GST_BIN(pipeline_), record_decoder_))
        {
            CMP_DEBUG_PRINT ("gst_bin_add failed.\n");
            return false;
        }

        if (TRUE != gst_element_link_many(record_queue_, record_decoder_, record_convert_, NULL))
        {
            CMP_DEBUG_PRINT ("link capture elements could not be linked.\n");
            return false;
        }
    }
    else
    {
        if (TRUE != gst_element_link_many(record_queue_, record_convert_, NULL))
        {
            CMP_DEBUG_PRINT ("link capture elements could not be linked queue & convert \n");
            return false;
        }
    }
#ifndef PLATFORM_QEMUX86
    if (TRUE != gst_element_link(record_convert_, filter_NV12_)) {
        CMP_DEBUG_PRINT ("link capture elements could not be linked - covert & filter_NV12 \n");
        return false;
    }
    if (TRUE != gst_element_link(filter_NV12_, record_encoder_)) {
        CMP_DEBUG_PRINT ("link capture elements could not be linked filter_NV12 & encoder \n");
        return false;
    }
    if (TRUE != gst_element_link_many(record_encoder_, record_parse_,
                                       record_video_queue_, NULL))
    {
        CMP_DEBUG_PRINT ("link capture elements could not be linked - encoder & parse \n");
        return false;
    }
#else
    if (TRUE != gst_element_link(record_convert_, record_encoder_)) {
        CMP_DEBUG_PRINT ("link capture elements could not be linked converter & encoder \n");
        return false;
    }
    if (TRUE != gst_element_link_many(record_encoder_,
                                       record_video_queue_, NULL))
    {
        CMP_DEBUG_PRINT ("link capture elements could not be linked - encoder & video_queue \n");
        return false;
    }
#endif
    record_video_queue_pad_ = gst_element_get_static_pad(record_video_queue_, "src");
    if (!record_video_queue_pad_)
    {
        CMP_DEBUG_PRINT ("static pad failed for record video queue \n");
        return false;
    }

    record_video_mux_pad_ = gst_element_get_request_pad(record_mux_, "video_%u");
    if (!record_video_mux_pad_)
    {
        CMP_DEBUG_PRINT ("request pad failed for video record avimux \n");
        return false;
    }
    if (GST_PAD_LINK_OK != gst_pad_link(record_video_queue_pad_, record_video_mux_pad_))
    {
        CMP_DEBUG_PRINT ("pad linking failed for record video queue and record avimux \n");
        return false;
    }

    if (record_audio_encoder_pad != NULL)
    {
        record_audio_mux_pad_ = gst_element_get_request_pad(record_mux_, "audio_%u");
        if (!record_audio_mux_pad_)
        {
            CMP_DEBUG_PRINT ("request pad failed for audio record mux \n");
            return false;
        }
        if (GST_PAD_LINK_OK != gst_pad_link(record_audio_encoder_pad, record_audio_mux_pad_))
        {
            CMP_DEBUG_PRINT ("pad linking failed for record audio queue and record avimux \n");
            return false;
        }
    }
    if (TRUE != gst_element_link_many(record_mux_, record_sink_, NULL))
    {
        CMP_DEBUG_PRINT ("link capture elements could not be linked - mux to sink \n");
        return false;
    }
    if (format_ == kFormatJPEG)
    {
        if (TRUE != gst_element_sync_state_with_parent(record_decoder_))
        {
            CMP_DEBUG_PRINT("Sync state failed:%d\n",__LINE__);
            return false;
        }
    }
    if (record_audio_encoder_pad != NULL)
    {
        if (TRUE != gst_element_sync_state_with_parent(record_audio_src_))
        {
            CMP_DEBUG_PRINT("Sync state failed:%d\n",__LINE__);
            return false;
        }
        if (TRUE != gst_element_sync_state_with_parent(record_audio_queue_))
        {
            CMP_DEBUG_PRINT("Sync state failed:%d\n",__LINE__);
            return false;
        }
        if (TRUE != gst_element_sync_state_with_parent(record_audio_convert_))
        {
            CMP_DEBUG_PRINT("Sync state failed:%d\n",__LINE__);
            return false;
        }
        if (TRUE != gst_element_sync_state_with_parent(record_audio_encoder_))
        {
            CMP_DEBUG_PRINT("Sync state failed:%d\n",__LINE__);
            return false;
        }
    }

    if (TRUE != gst_element_sync_state_with_parent(record_convert_))
    {
        CMP_DEBUG_PRINT("Sync state failed:%d\n",__LINE__);
        return false;
    }
    if (TRUE != gst_element_sync_state_with_parent(record_queue_))
    {
        CMP_DEBUG_PRINT("Sync state failed:%d\n",__LINE__);
        return false;
    }
#ifndef PLATFORM_QEMUX86
    if (TRUE != gst_element_sync_state_with_parent(record_parse_))
    {
        CMP_DEBUG_PRINT("Sync state failed:%d\n",__LINE__);
        return false;
    }
#endif
    if (TRUE != gst_element_sync_state_with_parent(record_video_queue_))
    {
        CMP_DEBUG_PRINT("Sync state failed:%d\n",__LINE__);
        return false;
    }
    if (TRUE != gst_element_sync_state_with_parent(record_mux_))
    {
        CMP_DEBUG_PRINT("Sync state failed:%d\n",__LINE__);
        return false;
    }

    if (TRUE != gst_element_sync_state_with_parent(record_encoder_))
    {
        CMP_DEBUG_PRINT("Sync state failed:%d\n",__LINE__);
        return false;
    }
#ifndef PLATFORM_QEMUX86
    if (TRUE != gst_element_sync_state_with_parent(filter_NV12_))
    {
        CMP_DEBUG_PRINT("Sync state failed:%d\n",__LINE__);
        return false;
    }
#endif
    if (TRUE != gst_element_sync_state_with_parent(record_sink_))
    {
        CMP_DEBUG_PRINT("Sync state failed:%d\n",__LINE__);
        return false;
    }

    record_queue_pad_ = gst_element_get_static_pad(record_queue_, "sink");
    if (!record_queue_pad_)
    {
        CMP_DEBUG_PRINT ("Did not get record_queue_pad_ queue pad.\n");
        return false;
    }
    if (GST_PAD_LINK_OK != gst_pad_link(tee_record_pad, record_queue_pad_))
    {
        CMP_DEBUG_PRINT ("Record Tee could not be linked.\n");
        return false;
    }

    return true;
}

bool CameraPlayer::CreateAudioRecordElements(const std::string& audioSrc, GstPad* tee_record_pad)
{
    record_audio_src_ = gst_element_factory_make("pulsesrc", "record-audio-src");
    if (!record_audio_src_)
    {
        CMP_DEBUG_PRINT("record_audio_src_(%p) Failed", record_audio_src_);
        return false;
    }
    g_object_set(G_OBJECT(record_audio_src_), "do-timestamp", true, NULL);

    CMP_DEBUG_PRINT ("AudioSrc provided is %s, length = %d", audioSrc.c_str(), audioSrc.length());
    if(audioSrc.compare("") != 0 && audioSrc.length() >0 )
    {
       CMP_DEBUG_PRINT ("Set audioSrc device name to pulsesrc eleemnt");
       g_object_set(G_OBJECT(record_audio_src_), "device", audioSrc.c_str(), NULL);
    }

    record_audio_queue_ = gst_element_factory_make ("queue", "record-audio-queue");
    if (!record_audio_queue_)
    {
        CMP_DEBUG_PRINT("record_audio_queue_(%p) Failed", record_audio_queue_);
        return false;
    }

    record_audio_convert_ = gst_element_factory_make ("audioconvert", "audio-convert");
    if (!record_audio_convert_)
    {
        CMP_DEBUG_PRINT("record_audio_convert_(%p) Failed", record_audio_convert_);
        return false;
    }

    record_audio_encoder_ = gst_element_factory_make ("avenc_aac", "audio-encoder");
    if (!record_audio_encoder_)
    {
        CMP_DEBUG_PRINT("record_audio_encoder_(%p) Failed", record_audio_encoder_);
        return false;
    }

    gst_bin_add_many(GST_BIN(pipeline_), record_audio_src_, record_audio_queue_,
                     record_audio_convert_, record_audio_encoder_, NULL);

    if (TRUE != gst_element_link_many(record_audio_src_, record_audio_queue_,
                                      record_audio_convert_, record_audio_encoder_, NULL))
    {
        CMP_DEBUG_PRINT ("link capture elements could not be linked - audio src to encoder \n");
        return false;
    }

    record_audio_encoder_pad_ = gst_element_get_static_pad(record_audio_encoder_, "src");
    if (!record_audio_encoder_pad_)
    {
        CMP_DEBUG_PRINT ("static pad failed for record audio encoder \n");
        return false;
    }
    return true;
}

GstBusSyncReply CameraPlayer::HandleSyncBusMessage(GstBus * bus,
        GstMessage * msg, gpointer data)
{
    // This handler will be invoked synchronously, don't process any application
    // message handling here
    LSM::CameraWindowManager *CameraWindowManager = static_cast<LSM::CameraWindowManager*>(data);

    switch (GST_MESSAGE_TYPE (msg))
    {
        case GST_MESSAGE_NEED_CONTEXT:
            {
                const gchar *type = nullptr;
                gst_message_parse_context_type(msg, &type);
                if (g_strcmp0 (type, waylandDisplayHandleContextType) != 0) {
                    break;
                }
                CMP_DEBUG_PRINT("Set a wayland display handle : %p", CameraWindowManager->getDisplay());
                if (CameraWindowManager->getDisplay()) {
                    GstContext *context = gst_context_new(waylandDisplayHandleContextType, TRUE);
                    gst_structure_set(gst_context_writable_structure (context),
                            "handle", G_TYPE_POINTER, CameraWindowManager->getDisplay(), nullptr);
                    gst_element_set_context(GST_ELEMENT(GST_MESSAGE_SRC(msg)), context);
                }
                goto drop;
            }
        case GST_MESSAGE_ELEMENT:
            {
                if (!gst_is_video_overlay_prepare_window_handle_message(msg)) {
                    break;
                }
                CMP_DEBUG_PRINT("Set wayland window handle : %p", CameraWindowManager->getSurface());
                if (CameraWindowManager->getSurface()) {
                    GstVideoOverlay *videoOverlay = GST_VIDEO_OVERLAY(GST_MESSAGE_SRC(msg));
                    gst_video_overlay_set_window_handle(videoOverlay,
                            (guintptr)(CameraWindowManager->getSurface()));

                    gint video_disp_height = 0;
                    gint video_disp_width = 0;
                    CameraWindowManager->getVideoSize(video_disp_width, video_disp_height);
                    if (video_disp_width && video_disp_height) {
                        gint display_x = (1920 - video_disp_width) / 2;
                        gint display_y = (1080 - video_disp_height) / 2;
                        CMP_DEBUG_PRINT("Set render rectangle :(%d, %d, %d, %d)",
                                display_x, display_y, video_disp_width, video_disp_height);
                        gst_video_overlay_set_render_rectangle(videoOverlay,
                                display_x, display_y, video_disp_width, video_disp_height);

                        gst_video_overlay_expose(videoOverlay);
                    }
                }
                goto drop;
            }
        default:
            break;
    }

    return GST_BUS_PASS;

drop:
    gst_message_unref(msg);
    return GST_BUS_DROP;
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

    parser_ = gst_element_factory_make("rawvideoparse", "parser");
    if (!parser_) {
        CMP_DEBUG_PRINT("parser_(%p) Failed", parser_);
        return false;
    }
    g_object_set(G_OBJECT(parser_), "format", GST_VIDEO_FORMAT_YUY2 , NULL);
    g_object_set(G_OBJECT(parser_), "width", width_ , NULL);
    g_object_set(G_OBJECT(parser_), "height", height_ , NULL);

    gst_bin_add_many(GST_BIN(pipeline_), source_, filter_YUY2_, parser_, tee_, NULL);

    if (TRUE != gst_element_link_many(source_, filter_YUY2_, parser_, tee_, NULL)) {
        CMP_DEBUG_PRINT("Elements could not be linked.\n");
        return false;
    }
    tee_preview_pad_ = gst_element_get_request_pad(tee_, "src_%u");

    if (CreatePreviewBin(tee_preview_pad_)) {
        bus_ = gst_pipeline_get_bus(GST_PIPELINE (pipeline_));
        gst_bus_add_watch(bus_, CameraPlayer::HandleBusMessage, this);
        gst_bus_set_sync_handler(bus_, CameraPlayer::HandleSyncBusMessage,
                &lsm_camera_window_manager_, NULL);
        gst_object_unref(bus_);
        return true;
    } else {
        CMP_DEBUG_PRINT("CreatePreviewBin Failed.\n");
        FreePreviewBinElements();
        gst_element_set_state (pipeline_, GST_STATE_NULL);
        return false;
    }
}

bool CameraPlayer::LoadJPEGPipeline()
{
    CMP_DEBUG_PRINT("JPEG FORMAT");

    preview_queue_ = gst_element_factory_make("queue2", "preview-queue");
    if (!preview_queue_) {
        CMP_DEBUG_PRINT("preview_queue_ element creation failed.");
        return false;
    }

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
            "framerate", GST_TYPE_FRACTION, framerate_, 1,
            NULL);

    g_object_set(G_OBJECT(filter_JPEG_), "caps", caps_JPEG_, NULL);

    gst_bin_add_many(GST_BIN(pipeline_), source_, filter_JPEG_, parser_, tee_,
            decoder_, NULL);

    if (TRUE != gst_element_link_many(source_, filter_JPEG_, parser_, tee_, NULL)) {
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
        gst_bus_set_sync_handler(bus_, CameraPlayer::HandleSyncBusMessage,
                &lsm_camera_window_manager_, NULL);
        gst_object_unref(bus_);
        return true;
    } else {
        CMP_DEBUG_PRINT("CreatePreviewBin Failed.\n");
        FreePreviewBinElements();
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
                converted = CMP_MSG__GST_CORE_ERROR_EVENT;
                break;
            default:
                break;
        }
    } else if (GST_LIBRARY_ERROR == domain) {
        // do nothing
    } else if (GST_RESOURCE_ERROR == domain) {
        switch (code) {
            case GST_RESOURCE_ERROR_SETTINGS:
                converted = CMP_MSG__GST_RESOURCE_ERROR_SETTINGS;
                break;
            case GST_RESOURCE_ERROR_NOT_FOUND:
                converted = CMP_MSG__GST_RESOURCE_ERROR_NOT_FOUND;
                break;
            case GST_RESOURCE_ERROR_OPEN_READ:
                converted = CMP_MSG__GST_RESOURCE_ERROR_OPEN_READ;
                break;
            case GST_RESOURCE_ERROR_READ:
                converted = CMP_MSG__GST_RESOURCE_ERROR_READ;
                break;
            default:
                break;
        }
    } else if (GST_STREAM_ERROR == domain) {
        switch (code) {
            case GST_STREAM_ERROR_TYPE_NOT_FOUND:
                converted = CMP_MSG__GST_STREAM_ERROR_TYPE_NOT_FOUND;
                break;
            case GST_STREAM_ERROR_DEMUX:
                converted = CMP_MSG__GST_STREAM_ERROR_DEMUX;
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
    CMP_DEBUG_PRINT("Debug information: %s", debug_info ? debug_info : "none");

    g_clear_error(&err);
    g_free(debug_info);

    return error;
}

void CameraPlayer::FeedData (GstElement * appsrc, guint size, gpointer gdata)
{
    CameraPlayer *player = reinterpret_cast<CameraPlayer *>(gdata);
    unsigned char *data = 0;
    int len = 0;
    while (len == 0)
    {
        ReadShmem(player->context_.shmemHandle, &data, &len);
    }
    GstBuffer *buf = gst_buffer_new_allocate(NULL, len, NULL);
    GstMapInfo writeBufferMap;
    gboolean bcheck = gst_buffer_map(buf, &writeBufferMap, GST_MAP_WRITE);
    memcpy(writeBufferMap.data, data, len);
    gst_buffer_unmap(buf, &writeBufferMap);
    gst_app_src_push_buffer((GstAppSrc*)appsrc, buf);
}

void CameraPlayer::FeedPosixData (GstElement * appsrc, guint size, gpointer gdata)
{
    CameraPlayer *player = reinterpret_cast<CameraPlayer *>(gdata);
    unsigned char *data = 0;
    int len = 0;
    while (len == 0)
    {
        ReadPosixShmem(player->context_.shmemHandle, &data, &len);
    }
    GstBuffer *buf = gst_buffer_new_allocate(NULL, len, NULL);
    GstMapInfo writeBufferMap;
    gboolean bcheck = gst_buffer_map(buf, &writeBufferMap, GST_MAP_WRITE);
    memcpy(writeBufferMap.data, data, len);
    gst_buffer_unmap(buf, &writeBufferMap);
    gst_app_src_push_buffer((GstAppSrc*)appsrc, buf);
}

void CameraPlayer::FreeLoadPipelineElements ()
{
    if (source_)
        delete source_;
    if (tee_)
        delete tee_;
    if (filter_YUY2_)
        delete filter_YUY2_;
    if (filter_I420_)
        delete filter_I420_;
    if (preview_queue_)
        delete preview_queue_;
    if (parser_)
        delete parser_;
    if (decoder_)
        delete decoder_;
    if (filter_JPEG_)
        delete filter_JPEG_;
}

void CameraPlayer::FreeCaptureElements ()
{
    if (capture_queue_)
        delete capture_queue_;
    if (capture_sink_)
        delete capture_sink_;
    if (decoder_)
        delete decoder_;
    if (capture_encoder_)
        delete capture_encoder_;
}

void CameraPlayer::FreeRecordElements ()
{
    if (record_queue_)
        delete record_queue_;
    if (record_encoder_)
        delete record_encoder_;
    if (filter_NV12_)
        delete filter_NV12_;
    if (record_convert_)
        delete record_convert_;
    if (record_mux_)
        delete record_mux_;
    if (record_sink_)
        delete record_sink_;
    if (record_decoder_)
        delete record_decoder_;
    if (record_audio_convert_)
        delete record_audio_convert_;
    if (record_audio_encoder_)
        delete record_audio_encoder_;
    if (record_audio_queue_)
        delete record_audio_queue_;
    if (record_audio_src_)
        delete record_audio_src_;
    if (record_video_queue_)
        delete record_video_queue_;
    if (record_parse_)
        delete record_parse_;
}

void CameraPlayer::FreePreviewBinElements ()
{
    if (vconv_)
        delete vconv_;
    if (preview_sink_)
        delete preview_sink_;
    if (filter_RGB_)
        delete filter_RGB_;
}

GstFlowReturn CameraPlayer::GetSample(GstAppSink *elt, gpointer data)
{
    CameraPlayer *player = reinterpret_cast<CameraPlayer*>(data);
    if (player->num_of_images_to_capture_ == player->num_of_captured_images_) {
        player->num_of_images_to_capture_ = 0;
        player->num_of_captured_images_ = 0;
        gst_pad_add_probe(player->tee_capture_pad_, GST_PAD_PROBE_TYPE_IDLE ,
                CaptureRemoveProbe, player, NULL);
    } else {
        GstSample *sample;
        sample = gst_app_sink_pull_sample(GST_APP_SINK (elt));
        if (NULL != sample) {
            GstMapInfo map;
            GstBuffer *buffer;
            buffer = gst_sample_get_buffer(sample);
            gst_buffer_map(buffer, &map, GST_MAP_READ);
            if ((NULL != map.data) && (map.size != 0)) {
                player->WriteImageToFile(map.data,map.size);
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
    gst_pad_unlink(player->tee_capture_pad_, player->capture_queue_pad_);
    gst_element_release_request_pad(player->tee_, player->tee_capture_pad_);

    gst_object_unref(player->tee_capture_pad_);
    gst_object_unref(player->capture_queue_pad_);

    if (TRUE != gst_bin_remove(GST_BIN(player->pipeline_),
                player->capture_queue_)) {
        CMP_DEBUG_PRINT("Failed %d\n\n",__LINE__);
    }
    gst_element_set_state(player->capture_queue_, GST_STATE_NULL);
    gst_object_unref(player->capture_queue_);
    player->capture_queue_ = NULL;

    if (player->format_ == kFormatYUV) {
        if (TRUE != gst_bin_remove(GST_BIN(player->pipeline_),
                    player->capture_encoder_)) {
            CMP_DEBUG_PRINT("Failed %d\n\n",__LINE__);
        }
        gst_element_set_state(player->capture_encoder_, GST_STATE_NULL);
        gst_object_unref(player->capture_encoder_);
        player->capture_encoder_ = NULL;
    }

    if (TRUE != gst_bin_remove(GST_BIN(player->pipeline_),
                player->capture_sink_)) {
        CMP_DEBUG_PRINT("Failed %d\n\n",__LINE__);
    }
    gst_element_set_state(player->capture_sink_, GST_STATE_NULL);
    gst_object_unref(player->capture_sink_);
    player->capture_sink_ = NULL;

    player->capture_path_.clear();
    return GST_PAD_PROBE_REMOVE;
}

GstPadProbeReturn
CameraPlayer::RecordRemoveProbe(
        GstPad * pad, GstPadProbeInfo * info, gpointer user_data)
{
    CameraPlayer *player = reinterpret_cast<CameraPlayer *>(user_data);
    gst_pad_unlink(player->tee_record_pad_, player->record_queue_pad_);
    gst_pad_send_event(player->record_video_mux_pad_, gst_event_new_eos());

    if (player->record_audio_encoder_pad_ != NULL)
    {
       gst_pad_unlink(player->record_audio_encoder_pad_, player->record_audio_mux_pad_);
       gst_pad_send_event(player->record_audio_mux_pad_, gst_event_new_eos());
       gst_element_release_request_pad(player->record_mux_, player->record_audio_mux_pad_);
       gst_object_unref(player->record_audio_mux_pad_);
       gst_object_unref(player->record_audio_encoder_pad_);
    }

    gst_element_release_request_pad(player->tee_, player->tee_record_pad_);
    gst_element_release_request_pad(player->record_mux_, player->record_video_mux_pad_);
    gst_object_unref(player->tee_record_pad_);
    gst_object_unref(player->record_queue_pad_);
    gst_object_unref(player->record_video_queue_pad_);
    gst_object_unref(player->record_video_mux_pad_);

    if (player->record_audio_encoder_pad_ != NULL)
    {
        gst_element_set_state(player->record_audio_encoder_, GST_STATE_NULL);
        if (TRUE != gst_bin_remove(GST_BIN(player->pipeline_),
                    player->record_audio_encoder_)) {
            CMP_DEBUG_PRINT("Failed %d\n\n",__LINE__);
        }
        gst_object_unref(player->record_audio_encoder_);
        player->record_audio_encoder_ = NULL;

        gst_element_set_state(player->record_audio_convert_, GST_STATE_NULL);
        if (TRUE != gst_bin_remove(GST_BIN(player->pipeline_),
                    player->record_audio_convert_)) {
            CMP_DEBUG_PRINT("Failed %d\n\n",__LINE__);
        }
        gst_object_unref(player->record_audio_convert_);
        player->record_audio_convert_ = NULL;

        gst_element_set_state(player->record_audio_queue_, GST_STATE_NULL);
        if (TRUE != gst_bin_remove(GST_BIN(player->pipeline_),
                    player->record_audio_queue_)) {
            CMP_DEBUG_PRINT("Failed %d\n\n",__LINE__);
        }
        gst_object_unref(player->record_audio_queue_);
        player->record_audio_queue_ = NULL;

        gst_element_set_state(player->record_audio_src_, GST_STATE_NULL);
        if (TRUE != gst_bin_remove(GST_BIN(player->pipeline_),
                    player->record_audio_src_)) {
            CMP_DEBUG_PRINT("Failed %d\n\n",__LINE__);
        }
        gst_object_unref(player->record_audio_src_);
        player->record_audio_src_ = NULL;

        player->record_audio_mux_pad_ = NULL;
        player->record_audio_encoder_pad_ = NULL;
    }
    gst_element_set_state(player->record_queue_, GST_STATE_NULL);
    if (TRUE != gst_bin_remove(GST_BIN(player->pipeline_),
                player->record_queue_)) {
        CMP_DEBUG_PRINT("Failed %d\n\n",__LINE__);
    }
    gst_object_unref(player->record_queue_);
    player->record_queue_ = NULL;

    gst_element_set_state(player->record_video_queue_, GST_STATE_NULL);
    if (TRUE != gst_bin_remove(GST_BIN(player->pipeline_),
                player->record_video_queue_)) {
        CMP_DEBUG_PRINT("Failed %d\n\n",__LINE__);
    }
    gst_object_unref(player->record_video_queue_);
    player->record_video_queue_ = NULL;

    if (player->format_ == kFormatJPEG) {
        gst_element_set_state(player->record_decoder_,GST_STATE_NULL);
        if (TRUE != gst_bin_remove(GST_BIN(player->pipeline_),
                    player->record_decoder_)) {
            CMP_DEBUG_PRINT("Failed %d\n\n",__LINE__);
        }
        gst_object_unref(player->record_decoder_);
        player->record_decoder_ = NULL;
    }

    gst_element_set_state(player->record_encoder_,GST_STATE_NULL);
    if (TRUE != gst_bin_remove(GST_BIN(player->pipeline_),
                player->record_encoder_)) {
        CMP_DEBUG_PRINT("Failed %d\n\n",__LINE__);
    }
    gst_object_unref(player->record_encoder_);
    player->record_encoder_ = NULL;

    if(player->record_parse_ != NULL)
    {
        gst_element_set_state(player->record_parse_,GST_STATE_NULL);
        if (TRUE != gst_bin_remove(GST_BIN(player->pipeline_),
                    player->record_parse_)) {
            CMP_DEBUG_PRINT("Failed %d\n\n",__LINE__);
        }
        gst_object_unref(player->record_parse_);
        player->record_parse_ = NULL;
    }

    gst_element_set_state(player->record_convert_, GST_STATE_NULL);
    if (TRUE != gst_bin_remove(GST_BIN(player->pipeline_),
                player->record_convert_)) {
        CMP_DEBUG_PRINT("Failed %d\n\n",__LINE__);
    }
    gst_object_unref(player->record_convert_);
    player->record_convert_ = NULL;

    if(player->filter_NV12_ != NULL)
    {
        gst_element_set_state(player->filter_NV12_, GST_STATE_NULL);
        if (TRUE != gst_bin_remove(GST_BIN(player->pipeline_),
                    player->filter_NV12_)) {
            CMP_DEBUG_PRINT("Failed %d\n\n",__LINE__);
        }
        gst_object_unref(player->filter_NV12_);
        player->filter_NV12_ = NULL;
    }

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
}
}
