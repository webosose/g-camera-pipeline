#include "shmemory_yuv_pipeline.h"
#include "log.h"

bool ShmemoryYuvPipeline::launch()
{
    CMP_LOG_INFO("start");

    // 1. Build pipeline description and launch.
    gchar* content = nullptr;
    std::string pipeline_desc;
    if (g_file_get_contents(camera_pipeline_path.c_str(),
                             &content, nullptr, nullptr))
    {
        pipeline_desc = std::string(content);
        g_free(content);
    }
    else
    {
        pipeline_desc = "appsrc name=src ! v4l2convert";

        //[TODO] Currently, auto PTZ is not supported for the YUV format.

        pipeline_desc += " ! waylandsink";
    }

    CMP_LOG_INFO("pipeline : %s", pipeline_desc.c_str());

    pipeline_ = gst_parse_launch(pipeline_desc.c_str(), NULL);
    if (pipeline_ == NULL)
    {
        CMP_LOG_INFO("Error. Pipeline is NULL");
        return false;
    }

    // 2. Setup src
    auto src = gst_bin_get_by_name(GST_BIN(pipeline_), "src");
    if (src)
    {
        auto caps = gst_caps_new_simple(
            "video/x-raw",
            "framerate", GST_TYPE_FRACTION, framerate_, 1,
            "width", G_TYPE_INT, width_,
            "height", G_TYPE_INT, height_,
            "format", G_TYPE_STRING, "YUY2",
            nullptr);

        g_object_set(src,
                     "is-live", true,
                     "format", GST_FORMAT_TIME,
                     "do-timestamp", true,
                     "caps", caps,
                     nullptr);

        g_signal_connect(
            src, "need-data",
            G_CALLBACK(+[](GstElement *src, guint size, gpointer data) {
                ShmemoryYuvPipeline *p =
                    reinterpret_cast<ShmemoryYuvPipeline *>(data);
                    p->FeedData(src, size);
            }),
            this);
    }

    CMP_LOG_INFO("end");
    return true;
}
