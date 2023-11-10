#include "shmemory_jpeg_pipeline.h"
#include "log.h"

bool ShmemoryJpegPipeline::launch()
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
        pipeline_desc = "appsrc name=src ! jpegdec ! v4l2convert";
#ifdef PTZ_ENABLED
        pipeline_desc += " ! videocrop name=preview-video-crop";
#endif
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
            "image/jpeg",
            "framerate", GST_TYPE_FRACTION, framerate_, 1,
            "width", G_TYPE_INT, width_,
            "height", G_TYPE_INT, height_,
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
                ShmemoryJpegPipeline *p =
                    reinterpret_cast<ShmemoryJpegPipeline *>(data);
                    p->FeedData(src, size);
            }),
            this);
    }

    CMP_LOG_INFO("end");
    return true;
}
