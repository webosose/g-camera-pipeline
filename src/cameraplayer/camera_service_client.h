#ifndef CAMERA_SERVICE_CLIENT_H_
#define CAMERA_SERVICE_CLIENT_H_

#include <luna-service2/lunaservice.h>
#include <string>
#include <glib.h>

class CameraServiceClient
{
private:
    std::string name_;
    GMainLoop *loop_;
    LSHandle *sh_;
    std::string reply_from_server_;
    int done_;
    int fd_;
    int handle_;
    int pid_;
    static bool cbGetFd(LSHandle*, LSMessage*, void*);
    static bool cbGetReplyMsg(LSHandle*, LSMessage*, void*);
    bool acquireLSHandle();
    bool releaseLSHandle();
    bool call(std::string uri, std::string payload, bool (*cb)(LSHandle*, LSMessage*, void*));
public:
    CameraServiceClient();
    ~CameraServiceClient();
    bool open(std::string cameraId, int pid = -1);
    int startCamera(std::string memtype_ = "shmem");
    int getFd();
    bool stopCamera();
    bool close();
};


#endif /* CAMERA_SERVICE_CLIENT_H_ */