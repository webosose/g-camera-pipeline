#ifndef SHMEMORY_JPEG_PIPELINE_H_
#define SHMEMORY_JPEG_PIPELINE_H_

#include "shmemory_pipeline.h"

class ShmemoryJpegPipeline : public ShmemoryPipeline
{
public:
    ShmemoryJpegPipeline()
    {
        pipelineType = "JpegPipeline";
    }

    bool launch();
};
#endif // SHMEMORY_JPEG_PIPELINE_H_
