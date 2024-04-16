#include "pipeline_factory.h"
#include "log.h"
#include "camera_player.h"
#include "shmemory_jpeg_pipeline.h"
#include "shmemory_yuv_pipeline.h"

std::shared_ptr<CameraPipeline> PipelineFactory::CreatePlayer(const pbnjson::JValue &parsed)
{
    CMP_LOG_INFO("start");

    std::string camera_id, format;
    if (parsed.hasKey("args") && parsed["args"].isArray())
    {
        int i = 0;
        for (ssize_t j = 0; j < parsed["args"].arraySize(); j++)
        {
            if (parsed["args"][i].hasKey("option"))
            {
                if (parsed["args"][i]["option"].hasKey("cameraId")) {
                    camera_id = parsed["args"][i]["option"]["cameraId"].asString();
                }
                if (parsed["args"][i]["option"].hasKey("format")) {
                    format = parsed["args"][i]["option"]["format"].asString();
                }
                break;
            }
            i++;
        }
    }
    else
    {
        if (parsed["options"]["option"].hasKey("cameraId")) {
            camera_id = parsed["options"]["option"]["cameraId"].asString();
        }
        if (parsed["options"]["option"].hasKey("format")) {
            format = parsed["options"]["option"]["format"].asString();
        }
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
