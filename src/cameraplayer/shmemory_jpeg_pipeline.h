#ifndef _SHMEMORY_JPEG_PIPELINE_H_
#define _SHMEMORY_JPEG_PIPELINE_H_

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
#endif // _SHMEMORY_JPEG_PIPELINE_H_
