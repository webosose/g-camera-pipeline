#ifndef SHMEMORY_YUV_PIPELINE_H_
#define SHMEMORY_YUV_PIPELINE_H_

#include "shmemory_pipeline.h"

class ShmemoryYuvPipeline : public ShmemoryPipeline
{
public:
    ShmemoryYuvPipeline() { pipelineType = "YuvPipeline"; }

    bool launch();
};
#endif // SHMEMORY_YUV_PIPELINE_H_
