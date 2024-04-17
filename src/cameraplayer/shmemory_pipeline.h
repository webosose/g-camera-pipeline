#ifndef SHMEMORY_PIPELINE_H_
#define SHMEMORY_PIPELINE_H_

#include "camera_pipeline.h"
#include <memory>
#include <thread>
#include <camera_window_manager.h>
#include "base.h"
#include "camera_types.h"
#include "camshm.h"

static const std::string camera_pipeline_path = "/etc/g-camera-pipeline/camera_pipeline";

using namespace cmp;

#ifdef PTZ_ENABLED
namespace cmp {
    class IPostProcessSolution;
}
#endif

class SignalListener;
class CameraServiceClient;
class ShmemoryPipeline : public CameraPipeline
{
    typedef struct GstAppSrcContext_
    {
        SHMEM_HANDLE shmemHandle;
        gint streamingAllowState;
        int key;
        gint isStreaming;
        gint isFirstCallback;
        GstAppSrc *appsrc;
    } GstAppSrcContext;

    typedef struct ACQUIRE_RESOURCE_INFO {
        base::source_info_t* sourceInfo;
        const char *displayMode;
        gboolean result;
    } ACQUIRE_RESOURCE_INFO_T;

    GMainLoop *loop_{nullptr};
    std::shared_ptr<std::thread> loopThread_;
    uint32_t busId_{0};
    CALLBACK_T cbFunction_{nullptr};

    /* GAV Features */
    LSM::CameraWindowManager lsm_camera_window_manager_;
    std::string display_mode_;
    std::string window_id_;

    int frame_counter  = 0;
    std::chrono::steady_clock::time_point startTime = std::chrono::steady_clock::now();

    GstAppSrcContext context_{NULL,1,0,0,FALSE,NULL};
    base::source_info_t source_info_;
    int posixshm_fd = -1;

    const std::string kMemtypeShmem = "shmem";
    const std::string kMemtypePosixShm = "posixshm";

#ifdef PTZ_ENABLED
    std::shared_ptr<IPostProcessSolution> postProcessSolution_;
#endif

    std::unique_ptr<SignalListener> shm_listener_;
    std::unique_ptr<CameraServiceClient> cs_client_;

    bool Pause();
    bool attachSurface(bool allow_no_window = false);
    bool detachSurface();
    bool acquireResource();
    bool GetSourceInfo();
    void NotifySourceInfo();
    void ParseOptionString(const std::string& options);
    void SetGstreamerDebug();
    void show_frame();
    bool createSignalListener();
    int32_t ConvertErrorCode(GQuark domain, gint code);
    base::error_t HandleErrorMessage(GstMessage *message);
    bool handleBusMessage(GstBus *bus, GstMessage *msg);
    GstBusSyncReply handleBusSyncMessage(GstBus *bus, GstMessage *msg);
    bool addBus();
    bool remBus();
    bool unloadImpl();

    int openShmemory();
    int closeShmemory();
    int readShmemory(SHMEM_HANDLE hShmem, unsigned char **ppData, int *pSize,
                     unsigned char **ppMeta, int *pMetaSize);

public:
    ShmemoryPipeline();
    virtual ~ShmemoryPipeline();

    bool Load(const std::string& msg) override;
    bool Unload() override;
    bool Play() override;
    void RegisterCbFunction(CALLBACK_T cbf);
    virtual bool launch() = 0;

protected:
    int32_t width_{0}, height_{0}, framerate_{0}, display_path_{CMP_DEFAULT_DISPLAY}, handle_{0};
    std::string uri_, memtype_, memsrc_, format_, camera_id_;
    bool primary{false};

    void FeedData(GstElement * appsrc, guint size);
    void deleteSocketIfExists(const std::string& socketPath);

    GstElement *pipeline_{nullptr};
    std::string pipelineType;
};

#endif // SHMEMORY_PIPELINE_H_
