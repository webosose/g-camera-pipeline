#ifndef _SHMEMORY_YUV_PIPELINE_H_
#define _SHMEMORY_YUV_PIPELINE_H_

#include "shmemory_pipeline.h"

class ShmemoryYuvPipeline : public ShmemoryPipeline
{
public:
    bool launch();
};
#endif // _SHMEMORY_YUV_PIPELINE_H_
