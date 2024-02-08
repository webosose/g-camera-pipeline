#include "shmemory_pipeline.h"
#include "log.h"
#include "message.h"
#include <system_error>
#include <pbnjson.hpp>
#include "camera_service_client.h"
#include "cam_posixshm.h"
#include "signal_listener.h"

#ifdef PTZ_ENABLED
#include "FacePtzSolution.hpp"
#endif

ShmemoryPipeline::ShmemoryPipeline()
{
    CMP_LOG_INFO("start");

    GMainContext *c = g_main_context_new();
    loop_           = g_main_loop_new(c, false);

    try
    {
        loopThread_ = std::make_shared<std::thread>(g_main_loop_run, loop_);
    }
    catch (const std::system_error &e)
    {
        CMP_LOG_ERROR("Caught a system_error with code %d meaning %s", e.code().value(), e.what());
    }

    pthread_setname_np(loopThread_->native_handle(), "player-pipeline");

    while (!g_main_loop_is_running(loop_))
    {
    }
    g_main_context_unref(c);

    CMP_LOG_INFO("end");
}

ShmemoryPipeline::~ShmemoryPipeline()
{
    CMP_LOG_INFO("start");

    Unload();

    g_main_loop_quit(loop_);
    if (loopThread_->joinable())
    {
        try
        {
            loopThread_->join();
        }
        catch (const std::system_error &e)
        {
            CMP_LOG_ERROR("Caught a system_error with code %d meaning %s", e.code().value(), e.what());
        }
    }
    g_main_loop_unref(loop_);

    CMP_LOG_INFO("end");
}

bool ShmemoryPipeline::Load(const std::string& msg)
{
    CMPASSERT(!msg.empty());
    CMP_LOG_INFO("start: %s",  msg.c_str());

    SetGstreamerDebug();
    gst_init(NULL, NULL);

    ParseOptionString(msg);
    CMP_LOG_INFO("format_ : %s", format_.c_str());
    CMP_LOG_INFO("width_ : %d", width_);
    CMP_LOG_INFO("height_ : %d", height_);
    CMP_LOG_INFO("framerate_: %d", framerate_);
    CMP_LOG_INFO("memtype_ : %s", memtype_.c_str());
    CMP_LOG_INFO("memsrc_ : %s", memsrc_.c_str());
    CMP_LOG_INFO("camera_id_ : %s", camera_id_.c_str());

    if (!GetSourceInfo())
    {
        CMP_LOG_ERROR("get source information failed!");
        return false;
    }

    if (!acquireResource())
    {
        CMP_LOG_ERROR("resouce acquire failed!");
        return false;
    }

    NotifySourceInfo();

    if (!attachSurface(true))
    {
        CMP_LOG_ERROR("attachSurface() failed");
        return false;
    }

    createSignalListener();

    if (openShmemory())
    {
        CMP_LOG_ERROR("openShmemory() failed");
        return false;
    }

    // 0. Check sanity.
    if (pipeline_ != nullptr)
    {
        CMP_LOG_ERROR("Error. pipeline already exists.");
        return false;
    }

    // 1. Build pipeline and launch.
    if (!launch())
    {
        CMP_LOG_ERROR("Pipeline launch fail");
        return false;
    }

#ifdef PTZ_ENABLED
    postProcessSolution_ = std::make_shared<FacePtzSolution>();
    postProcessSolution_->setParam(PARAM_ID_WIDTH, (void *)&width_);
    postProcessSolution_->setParam(PARAM_ID_HEIGHT,(void *)&height_);
    postProcessSolution_->setParam(PARAM_ID_CROP_OBJ, (void *)pipeline_);
#endif

    // 2. Get Bus.
    if (!addBus())
    {
        Unload();
        return false;
    }

    auto bus = gst_element_get_bus(pipeline_);
    gst_bus_set_sync_handler(
        bus,
        (GstBusSyncHandler) + [](GstBus *bus, GstMessage *message, gpointer data) -> GstBusSyncReply
        {
            ShmemoryPipeline *p = static_cast<ShmemoryPipeline *>(data);
            return p->handleBusSyncMessage(bus, message);
        },
        this, nullptr);

    gst_object_unref(bus);

    // 3. Ready for playback.
    Pause();

    CMP_LOG_INFO("end");
    return true;
}

bool ShmemoryPipeline::Unload()
{
    CMP_LOG_INFO("start");

    if (pipeline_ == nullptr)
    {
        CMP_LOG_WARNING("Pipeline is not loaded.");
        return false;
    }

    CMP_LOG_INFO("Unload pipeline");
    bool ret = gst_element_set_state(pipeline_, GST_STATE_NULL);
    if (ret == GST_STATE_CHANGE_FAILURE)
    {
        CMP_LOG_WARNING("Failed to change pipeline state to NULL");
        CMP_LOG_WARNING("Keep doing rest of unload procedure.");
    }

    GstState state;
    gst_element_get_state(pipeline_, &state, nullptr, GST_CLOCK_TIME_NONE);
    CMP_LOG_INFO("state = %s", gst_element_state_get_name(state));
    if (state == GST_STATE_NULL)
    {
        CMP_LOG_INFO("Unload completed");
    }

    auto bus = gst_pipeline_get_bus(GST_PIPELINE(pipeline_));
    if (bus != nullptr)
    {
        gst_bus_set_sync_handler(bus, nullptr, nullptr, nullptr);

        if (remBus())
            gst_bus_remove_watch(bus);

        gst_object_unref(bus);
    }

    CMP_LOG_INFO("Delete pipeline");
    gst_object_unref(pipeline_);
    pipeline_ = nullptr;

    if (closeShmemory())
    {
        CMP_LOG_ERROR("closeShmemory() failed");
        return false;
    }

    if (!detachSurface())
    {
        CMP_LOG_ERROR("detachSurface() failed");
        return false;
    }

    if (cs_client_)
    {
        cs_client_->stopCamera();
        cs_client_->close();
    }

    CMP_LOG_INFO("end");
    return true;
}

bool ShmemoryPipeline::Play()
{
    CMP_LOG_INFO("start");
    if (pipeline_ != nullptr &&
        gst_element_set_state(pipeline_, GST_STATE_PLAYING) == GST_STATE_CHANGE_FAILURE)
    {
        CMP_LOG_ERROR("Failed to change pipeline state to PLAYING");
        return false;
    }

    CMP_LOG_INFO("end");
    return true;
}

bool ShmemoryPipeline::Pause()
{
    CMP_LOG_INFO("start");
    if (pipeline_ != nullptr &&
        gst_element_set_state(pipeline_, GST_STATE_PAUSED) == GST_STATE_CHANGE_FAILURE)
    {
        CMP_LOG_ERROR("Failed to change pipeline state to PAUSED");
        return false;
    }

    CMP_LOG_INFO("end");
    return true;
}

void ShmemoryPipeline::RegisterCbFunction(CALLBACK_T cbf)
{
    CMP_LOG_INFO("start");
    cbFunction_ = std::move(cbf);
}

bool ShmemoryPipeline::addBus()
{
    if (pipeline_ == nullptr)
        return false;
    CMP_LOG_INFO("start");

    auto bus = gst_element_get_bus(pipeline_);
    if (!bus)
    {
        CMP_LOG_ERROR("Error. Fail gst_elememt_get_bus!");
        return false;
    }

    auto s = gst_bus_create_watch(bus);
    g_source_set_callback(
        s,
        (GSourceFunc) + [](GstBus *bus, GstMessage *msg, gpointer data) -> gboolean
        {
            ShmemoryPipeline *p = static_cast<ShmemoryPipeline *>(data);
            return p->handleBusMessage(bus, msg);
        },
        this, nullptr);
    g_source_attach(s, g_main_loop_get_context(loop_));
    busId_ = g_source_get_id(s);
    g_source_unref(s);

    gst_object_unref(bus);

    CMP_LOG_INFO("end");
    return true;
}

bool ShmemoryPipeline::remBus()
{
    if (busId_ == 0)
        return false;
    CMP_LOG_INFO("start");

    GSource *s = g_main_context_find_source_by_id(g_main_loop_get_context(loop_), busId_);
    if (s != nullptr)
        g_source_destroy(s);

    busId_ = 0;

    CMP_LOG_INFO("end");
    return true;
}

bool ShmemoryPipeline::attachSurface(bool allow_no_window) {
    CMP_LOG_INFO("start");

    if (!window_id_.empty()) {
        if (!lsm_camera_window_manager_.registerID(window_id_.c_str(), NULL)) {
            CMP_LOG_ERROR("register id to LSM failed!");
            return false;
        }
        if (!lsm_camera_window_manager_.attachSurface()) {
            CMP_LOG_ERROR("attach surface to LSM failed!");
            return false;
        }
        CMP_LOG_INFO("end");
        return true;
    } else {
        CMP_LOG_ERROR("window id is empty!");
        bool ret = allow_no_window ? true : false;
        return ret;
    }
}

bool ShmemoryPipeline::detachSurface() {
    CMP_LOG_INFO("start");

    if (!window_id_.empty()) {
        if (!lsm_camera_window_manager_.detachSurface()) {
            CMP_LOG_ERROR("detach surface to LSM failed!");
            return false;
        }
        if (!lsm_camera_window_manager_.unregisterID()) {
            CMP_LOG_ERROR("unregister id to LSM failed!");
            return false;
        }
    } else {
        CMP_LOG_ERROR("window id is empty!");
    }

     CMP_LOG_INFO("end");
    return true;
}

bool ShmemoryPipeline::acquireResource()
{
    ACQUIRE_RESOURCE_INFO_T resource_info;
    resource_info.sourceInfo = &source_info_;
    resource_info.displayMode = const_cast<char*>(display_mode_.c_str());
    resource_info.result = true;

    if (cbFunction_)
        cbFunction_(CMP_NOTIFY_ACQUIRE_RESOURCE, display_path_, nullptr, static_cast<void*>(&resource_info));

    if (!resource_info.result)
    {
        CMP_LOG_ERROR("resouce acquire fail!");
        return false;
    }

    return true;
}

bool ShmemoryPipeline::GetSourceInfo()
{
    base::video_info_t video_stream_info = {};

    video_stream_info.width = width_;
    video_stream_info.height = height_;
    video_stream_info.decode = CMP_VIDEO_CODEC_MJPEG;
    video_stream_info.frame_rate.num = framerate_;
    video_stream_info.frame_rate.den = 1;
    CMP_LOG_INFO("[video info] width: %d, height: %d, frameRate: %d/%d",
            video_stream_info.width, video_stream_info.height,
            video_stream_info.frame_rate.num, video_stream_info.frame_rate.den);

    base::program_info_t program;
    program.video_stream = 1;
    source_info_.programs.push_back(program);

    source_info_.video_streams.push_back(video_stream_info);

    return true;
}

void ShmemoryPipeline::NotifySourceInfo()
{
    // TODO(anonymous): Support multiple video/audio stream case
    if (cbFunction_)
        cbFunction_(CMP_NOTIFY_SOURCE_INFO, 0, nullptr, &source_info_);
}

bool ShmemoryPipeline::handleBusMessage(GstBus *bus, GstMessage *msg)
{
    auto msgType = GST_MESSAGE_TYPE(msg);
    if (msgType != GST_MESSAGE_QOS && msgType != GST_MESSAGE_TAG)
    {
        CMP_LOG_INFO("Element[ %s ][ %d ][ %s ]", GST_MESSAGE_SRC_NAME(msg), msgType,
              gst_message_type_get_name(msgType));
    }

    switch (GST_MESSAGE_TYPE(msg))
    {
        case GST_MESSAGE_ERROR:
        {
            CMP_LOG_ERROR("Got Error");
            base::error_t error = HandleErrorMessage(msg);
            if (cbFunction_)
                cbFunction_(CMP_NOTIFY_ERROR, 0, nullptr, &error);
            GST_DEBUG_BIN_TO_DOT_FILE(GST_BIN(pipeline_), GST_DEBUG_GRAPH_SHOW_VERBOSE, "gcp_error");
            break;
        }
        case GST_MESSAGE_EOS:
        {
            CMP_LOG_INFO("Got EOS");
            if (cbFunction_)
                cbFunction_(CMP_NOTIFY_END_OF_STREAM, 0, nullptr, nullptr);
            break;
        }
        case GST_MESSAGE_ASYNC_DONE:
        {
            CMP_LOG_INFO("Got AsyncDone");
            break;
        }
        case GST_MESSAGE_STATE_CHANGED:
        {
            if (GST_MESSAGE_SRC(msg) != GST_OBJECT_CAST(pipeline_))
                break;

            GstState oldState = GST_STATE_NULL;
            GstState newState = GST_STATE_NULL;
            gst_message_parse_state_changed(msg, &oldState, &newState, nullptr);
            CMP_LOG_INFO("Element[%s] State changed ...%s -> %s", GST_MESSAGE_SRC_NAME(msg),
                  gst_element_state_get_name(oldState), gst_element_state_get_name(newState));

            if (newState == GST_STATE_PAUSED && oldState < GST_STATE_PAUSED)
            {
                CMP_LOG_INFO("post loadcompleted event");
                if (cbFunction_)
                    cbFunction_(CMP_NOTIFY_LOAD_COMPLETED, 0, nullptr, nullptr);
            }
            else if (newState == GST_STATE_PLAYING)
            {
                CMP_LOG_INFO("post playing event");
                if (cbFunction_)
                    cbFunction_(CMP_NOTIFY_PLAYING, 0, nullptr, nullptr);
                GST_DEBUG_BIN_TO_DOT_FILE(GST_BIN(pipeline_), GST_DEBUG_GRAPH_SHOW_VERBOSE, "gcp_play");
            }
            else if (newState == GST_STATE_PAUSED && oldState == GST_STATE_PLAYING)
            {
                CMP_LOG_INFO("post paused event");
                if (cbFunction_)
                    cbFunction_(CMP_NOTIFY_PAUSED, 0, nullptr, nullptr);
            }
            //[TODO] Can not post this becaus UMS kills the process first.
            else if (newState == GST_STATE_NULL && oldState >= GST_STATE_PAUSED )
            {
                CMP_LOG_INFO("post unloadcompleted event");
                if (cbFunction_)
                    cbFunction_(CMP_NOTIFY_UNLOAD_COMPLETED, 0, nullptr, nullptr);
            }
            break;
        }
        case GST_MESSAGE_APPLICATION:
        {
            const GstStructure *gStruct = gst_message_get_structure(msg);

            /* video-info message comes from sink element */
            if (gst_structure_has_name(gStruct, "video-info"))
            {
                CMP_LOG_INFO("got video-info message");
                base::video_info_t video_info;
                memset(&video_info, 0, sizeof(base::video_info_t));
                gint width, height, fps_n, fps_d, par_n = -1, par_d = -1;
                gst_structure_get_int(gStruct, "width", &width);
                gst_structure_get_int(gStruct, "height", &height);
                gst_structure_get_fraction(gStruct, "framerate", &fps_n, &fps_d);
                gst_structure_get_int(gStruct, "par_n", &par_n);
                gst_structure_get_int(gStruct, "par_d", &par_d);

                CMP_LOG_INFO("width[%d], height[%d], framerate[%d/%d],"
                        "pixel_aspect_ratio[%d/%d]", width, height,
                        fps_n, fps_d, par_n, par_d);

                video_info.width = width;
                video_info.height = height;
                video_info.frame_rate.num = fps_n;
                video_info.frame_rate.den = fps_d;
                // TODO: we already know this info. but it's not used now.
                video_info.bit_rate = 0;
                video_info.codec = 0;

                if (cbFunction_)
                    cbFunction_(CMP_NOTIFY_VIDEO_INFO, 0, nullptr, &video_info);
            }
            else if (gst_structure_has_name(gStruct, "request-resource"))
            {
                CMP_LOG_INFO("got request-resource message");
            }
            break;
        }
        default:
            break;
    }

    return true;
}

GstBusSyncReply ShmemoryPipeline::handleBusSyncMessage(GstBus *bus,
                                                          GstMessage *msg)
{
    // This handler will be invoked synchronously, don't process any application
    // message handling here

    static constexpr char const *waylandDisplayHandleContextType =
        "GstWaylandDisplayHandleContextType";

    switch (GST_MESSAGE_TYPE (msg))
    {
        case GST_MESSAGE_NEED_CONTEXT:
            {
                const gchar *type = nullptr;
                gst_message_parse_context_type(msg, &type);
                if (g_strcmp0 (type, waylandDisplayHandleContextType) != 0) {
                    break;
                }
                CMP_LOG_INFO("Set a wayland display handle : %p", lsm_camera_window_manager_.getDisplay());
                if (lsm_camera_window_manager_.getDisplay()) {
                    GstContext *context = gst_context_new(waylandDisplayHandleContextType, TRUE);
                    gst_structure_set(gst_context_writable_structure (context),
                            "handle", G_TYPE_POINTER, lsm_camera_window_manager_.getDisplay(), nullptr);
                    gst_element_set_context(GST_ELEMENT(GST_MESSAGE_SRC(msg)), context);
                }
                goto drop;
            }
        case GST_MESSAGE_ELEMENT:
            {
                if (!gst_is_video_overlay_prepare_window_handle_message(msg)) {
                    break;
                }
                CMP_LOG_INFO("Set wayland window handle : %p", lsm_camera_window_manager_.getSurface());
                if (lsm_camera_window_manager_.getSurface()) {
                    GstVideoOverlay *videoOverlay = GST_VIDEO_OVERLAY(GST_MESSAGE_SRC(msg));
                    gst_video_overlay_set_window_handle(videoOverlay,
                            (guintptr)(lsm_camera_window_manager_.getSurface()));

                    gint video_disp_height = 0;
                    gint video_disp_width = 0;
                    lsm_camera_window_manager_.getVideoSize(video_disp_width, video_disp_height);
                    if (video_disp_width && video_disp_height) {
                        gint display_x = (1920 - video_disp_width) / 2;
                        gint display_y = (1080 - video_disp_height) / 2;
                        CMP_LOG_INFO("Set render rectangle :(%d, %d, %d, %d)",
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

void ShmemoryPipeline::FeedData (GstElement * appsrc, guint size)
{
    unsigned char *data = 0;
    int len = 0;
    unsigned char *meta; int meta_len;
    static GstClockTime timestamp = 0;

    if (shm_listener_) shm_listener_->wait();

    if(readShmemory(context_.shmemHandle, &data, &len, &meta, &meta_len) != 0)
    {
        CMP_LOG_ERROR("shared memory read fail");
        return;
    }

    GstBuffer *buf = gst_buffer_new_wrapped_full(GST_MEMORY_FLAG_READONLY, data, len, 0, len, NULL, NULL);
    GST_BUFFER_PTS (buf) = timestamp;
    GST_BUFFER_DURATION (buf) = gst_util_uint64_scale_int (1, GST_SECOND, framerate_);
    timestamp += GST_BUFFER_DURATION (buf);
    gst_app_src_push_buffer((GstAppSrc*)appsrc, buf);

#ifdef PTZ_ENABLED
    if (postProcessSolution_)
    {
        CMP_LOG_DEBUG("meta len = %d bytes", meta_len);
        postProcessSolution_->pushMetaData(meta, meta_len);
        postProcessSolution_->doPostProcess();
    }
#endif

    show_frame();
}

void ShmemoryPipeline::ParseOptionString(const std::string& options)
{
    CMP_LOG_INFO("option string: %s", options.c_str());
    pbnjson::JDomParser jdparser;
    if (!jdparser.parse(options, pbnjson::JSchema::AllSchema())) {
        CMP_LOG_INFO("ERROR JDomParser.parse. msg: %s ", options.c_str());
        return;
    }
    pbnjson::JValue parsed = jdparser.getDom();

    if (parsed.hasKey("args") && parsed["args"].isArray())
    {
        if (parsed["args"].arraySize() > 0) {
            uri_ = parsed["args"][0].asString();
        }
        for (ssize_t i = 1; i < parsed["args"].arraySize(); i++)
        {
            if (parsed["args"][i].hasKey("option"))
            {
                if (parsed["args"][i]["option"].hasKey("displayPath")) {
                    int32_t display_path = parsed["args"][i]["option"]["displayPath"].asNumber<int32_t>();
                    display_path_ = (display_path > CMP_SECONDARY_DISPLAY ? 0 : display_path);
                }
                if (parsed["args"][i]["option"].hasKey("windowId")) {
                    window_id_ = parsed["args"][i]["option"]["windowId"].asString();
                }
                if (parsed["args"][i]["option"].hasKey("handle")) {
                    handle_ = parsed["args"][i]["option"]["handle"].asNumber<int>();
                }
                if (parsed["args"][i]["option"].hasKey("videoDisplayMode")) {
                    display_mode_ = parsed["args"][i]["option"]["videoDisplayMode"].asString();
                }
                if (parsed["args"][i]["option"].hasKey("format")) {
                    format_ = parsed["args"][i]["option"]["format"].asString();
                }
                if (parsed["args"][i]["option"].hasKey("width")) {
                    width_ = parsed["args"][i]["option"]["width"].asNumber<int>();
                }
                if (parsed["args"][i]["option"].hasKey("height")) {
                    height_ = parsed["args"][i]["option"]["height"].asNumber<int>();
                }
                if (parsed["args"][i]["option"].hasKey("frameRate")) {
                    framerate_ = parsed["args"][i]["option"]["frameRate"].asNumber<int>();
                }
                if (parsed["args"][i]["option"].hasKey("memType")) {
                    memtype_ = parsed["args"][i]["option"]["memType"].asString();
                }
                if (parsed["args"][i]["option"].hasKey("memSrc")) {
                    memsrc_ = parsed["args"][i]["option"]["memSrc"].asString();
                }
                if (parsed["args"][i]["option"].hasKey("cameraId")) {
                    camera_id_ = parsed["args"][i]["option"]["cameraId"].asString();
                }
                break;
            }
        }
    }
    else
    {
        if(parsed.hasKey("uri")) {
            uri_ = parsed["uri"].asString();
        } else {
            CMP_LOG_ERROR("UMS_INTERNAL_API_VERSION is not version 2.");
            CMP_LOG_ERROR("Please check the UMS_INTERNAL_API_VERSION in ums.");
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
        if (parsed["options"]["option"].hasKey("memSrc")) {
            memsrc_ = parsed["options"]["option"]["memSrc"].asString();
        }
        if (parsed["options"]["option"].hasKey("cameraId")) {
            camera_id_ = parsed["options"]["option"]["cameraId"].asString();
        }
    }

    CMP_LOG_INFO("uri: %s, display-path: %d, window_id: %s, display_mode: %s",
            uri_.c_str(), display_path_, window_id_.c_str(), display_mode_.c_str());
}

void ShmemoryPipeline::SetGstreamerDebug()
{
    pbnjson::JValue parsed = pbnjson::JDomParser::fromFile("/etc/g-camera-pipeline/gst_debug.conf");
    if (!parsed.isObject()) {
        CMP_LOG_ERROR("Gst debug file parsing error");
    }

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

int32_t ShmemoryPipeline::ConvertErrorCode(GQuark domain, gint code)
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

base::error_t ShmemoryPipeline::HandleErrorMessage(GstMessage *message)
{
    GError *err = NULL;
    gchar *debug_info;
    gst_message_parse_error(message, &err, &debug_info);
    GQuark domain = err->domain;

    base::error_t error;
    error.errorCode = ConvertErrorCode(domain, (gint)err->code);
    error.errorText = g_strdup(err->message)? g_strdup(err->message) : "";

    CMP_LOG_INFO("[GST_MESSAGE_ERROR][domain:%s][from:%s][code:%d]"
            "[converted:%d][msg:%s]",g_quark_to_string(domain),
            (GST_OBJECT_NAME(GST_MESSAGE_SRC(message))), err->code, error.errorCode,
            err->message);
    CMP_LOG_INFO("Debug information: %s", debug_info ? debug_info : "none");

    g_clear_error(&err);
    g_free(debug_info);

    return error;
}

void ShmemoryPipeline::show_frame()
{
    ++frame_counter;

    auto currentTime = std::chrono::steady_clock::now();
    auto elapsedTime = std::chrono::duration_cast<std::chrono::seconds>(
                           currentTime - startTime)
                           .count();

    if (elapsedTime >= 1)
    {
        CMP_LOG_INFO("fps: %d", frame_counter);
        startTime = currentTime;
        frame_counter = 0;
    }
}

int ShmemoryPipeline::openShmemory()
{
    CMP_LOG_INFO("start");

    if (memtype_ == kMemtypeShmem)
    {
        context_.key = atoi(memsrc_.c_str());
        return OpenShmem((SHMEM_HANDLE *)(&(context_.shmemHandle)), context_.key);

    }
    else if (memtype_ == kMemtypePosixShm)
    {
        CMP_LOG_INFO("posixshm_fd = %d", posixshm_fd);
        if (posixshm_fd > 0)
        {
            return OpenPosixShmem((SHMEM_HANDLE *)(&(context_.shmemHandle)), posixshm_fd);
        }
    }

    return -1;
}

int ShmemoryPipeline::closeShmemory()
{
    CMP_LOG_INFO("start");

    if (memtype_ == kMemtypeShmem)
    {
        return CloseShmem((SHMEM_HANDLE *)(&(context_.shmemHandle)));
    }
    else if (memtype_ == kMemtypePosixShm)
    {
        return ClosePosixShmem((SHMEM_HANDLE *)(&(context_.shmemHandle)), "", posixshm_fd);
    }

    return -1;
}

int ShmemoryPipeline::readShmemory(SHMEM_HANDLE hShmem, unsigned char **ppData, int *pSize,
                                   unsigned char **ppMeta, int *pMetaSize)
{
    if (memtype_ == kMemtypeShmem)
    {
        return ReadShmem(hShmem, ppData, pSize, ppMeta, pMetaSize);
    }
    else if (memtype_ == kMemtypePosixShm)
    {
        return ReadPosixShmem(hShmem, ppData, pSize, ppMeta, pMetaSize);
    }

    return -1;
}

bool ShmemoryPipeline::createSignalListener()
{
    int pid = -1;
    cs_client_ = std::make_unique<CameraServiceClient>();
    shm_listener_ = std::make_unique<SignalListener>();

    if (shm_listener_)
    {
        CMP_LOG_INFO("shm_listener_ creation OK");
        shm_listener_->initialize(SIGUSR1);
        pid = shm_listener_->run();
    }
    CMP_LOG_INFO("pid : %d", pid);
    if (cs_client_->open(camera_id_, pid))
    {
        int key = cs_client_->startCamera(memtype_);
        if (key == atoi(memsrc_.c_str()))
        {
            if (memtype_ == kMemtypePosixShm)
            {
                posixshm_fd = cs_client_->getFd();
            }
        }
    }

    return true;
}

int ShmemoryPipeline::getProcessCount(const std::string& file_path)
{
    std::string command = "lsof | grep " + file_path + " | wc -l";
    FILE* fp = popen(command.c_str(), "r");
    if (fp == nullptr) {
        CMP_LOG_ERROR("Error executing command");
        return -1;
    }

    char buffer[1024];
    int process_count = 0;
    if (fgets(buffer, sizeof(buffer), fp) != nullptr) {
        process_count = std::stoi(buffer);
    }

    pclose(fp);

    CMP_LOG_INFO("%d processes have opened %s", process_count, file_path.c_str());
    return process_count;
}

bool ShmemoryPipeline::deleteSocketIfExists(const std::string& socketPath)
{
    CMP_LOG_INFO("%s", socketPath.c_str());

    if (access(socketPath.c_str(), F_OK) == 0)
    {
        if (getProcessCount(socketPath) == 0)
        {
            CMP_LOG_WARNING("unlink %s", socketPath.c_str());
            unlink(socketPath.c_str());
        }
    }

    return true;
}
