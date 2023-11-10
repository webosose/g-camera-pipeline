#include "pipeline_factory.h"
#include "log.h"
#include "camera_player.h"

std::shared_ptr<CameraPipeline> PipelineFactory::CreatePlayer(const pbnjson::JValue &parsed)
{
    CMP_LOG_INFO("start");

    return std::make_shared<cmp::player::CameraPlayer>();
}
