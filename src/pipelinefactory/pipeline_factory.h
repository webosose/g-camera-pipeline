#ifndef _PIPELINE_FACTORY_H_
#define _PIPELINE_FACTORY_H_

#include <memory>
#include <string>
#include <pbnjson.hpp>

class CameraPipeline;
class PipelineFactory {
public:
    static std::shared_ptr<CameraPipeline> CreatePlayer(const pbnjson::JValue &parsed);
};
#endif // _PIPELINE_FACTORY_H_
