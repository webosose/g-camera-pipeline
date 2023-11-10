#include "pipeline_factory.h"
#include "log.h"
#include "camera_player.h"
#include "shmemory_jpeg_pipeline.h"
#include "shmemory_yuv_pipeline.h"

std::shared_ptr<CameraPipeline> PipelineFactory::CreatePlayer(const pbnjson::JValue &parsed)
{
    CMP_LOG_INFO("start");

    std::string camera_id, format;
    if (parsed["options"]["option"].hasKey("cameraId")) {
        camera_id = parsed["options"]["option"]["cameraId"].asString();
    }
    if (parsed["options"]["option"].hasKey("format")) {
        format = parsed["options"]["option"]["format"].asString();
    }

    if (!camera_id.empty())
    {
        if (format == "JPEG")
        {
            return std::make_shared<ShmemoryJpegPipeline>();
        }
        else if (format == "YUY2")
        {
            return std::make_shared<ShmemoryYuvPipeline>();
        }
    }

    return std::make_shared<cmp::player::CameraPlayer>();
}
