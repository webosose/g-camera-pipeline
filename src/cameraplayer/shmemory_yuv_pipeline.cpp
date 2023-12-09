#include "shmemory_yuv_pipeline.h"
#include "log.h"
#include "element_factory.h"

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
        pipeline_desc = "appsrc name=src";

        std::string element = ElementFactory::GetPreferredElementName(pipelineType, "video-converter");
        if (!element.empty())
            pipeline_desc += " ! " + element + " name=conv";

#ifdef PTZ_ENABLED
        pipeline_desc += " ! videocrop name=preview-video-crop";
#endif

        pipeline_desc += " ! video/x-raw, format=RGB16";
        pipeline_desc += " ! videoscale ! video/x-raw, width=" + std::to_string(width_) + ", height=" + std::to_string(height_);
        pipeline_desc += " ! tee name=t";

        element = ElementFactory::GetPreferredElementName(pipelineType, "video-sink");
        if (!element.empty())
            pipeline_desc += " t. ! queue ! " + element + " name=sink";

        std::string socketPath = "/tmp/" + camera_id_;
        deleteSocketIfExists(socketPath);

        if (access(socketPath.c_str(),F_OK) == -1) // File is not accessible
        {
            CMP_LOG_INFO("add shmsink %s", socketPath.c_str());
            pipeline_desc += " t. ! queue ! shmsink sync=false socket-path=" + socketPath + " wait-for-connection=false shm_size=10000000";
        }
    }

    CMP_LOG_INFO("pipeline : %s", pipeline_desc.c_str());

    pipeline_ = gst_parse_launch(pipeline_desc.c_str(), NULL);
    if (pipeline_ == NULL)
    {
        CMP_LOG_ERROR("Error. Pipeline is NULL");
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

    // 2. Setup converter
    auto conv = gst_bin_get_by_name(GST_BIN(pipeline_), "conv");
    if (conv)
    {
        ElementFactory::SetProperties(pipelineType, conv, "video-converter");
    }

    // 3. Setup sink
    auto sink = gst_bin_get_by_name(GST_BIN(pipeline_), "sink");
    if (sink)
    {
        ElementFactory::SetProperties(pipelineType, sink, "video-sink");
    }

    CMP_LOG_INFO("end");
    return true;
}
