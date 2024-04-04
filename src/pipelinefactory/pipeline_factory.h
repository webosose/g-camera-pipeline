#ifndef PIPELINE_FACTORY_H_
#define PIPELINE_FACTORY_H_

#include <memory>
#include <string>
#include <pbnjson.hpp>

class CameraPipeline;
class PipelineFactory {
public:
    static std::shared_ptr<CameraPipeline> CreatePlayer(const pbnjson::JValue &parsed);
};
#endif // PIPELINE_FACTORY_H_
