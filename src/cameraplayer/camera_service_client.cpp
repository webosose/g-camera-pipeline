#include "camera_service_client.h"
#include <luna-service2/lunaservice.hpp>
#include <pbnjson.hpp>
#include <log/log.h>

#ifdef CMP_DEBUG_PRINT
#undef CMP_DEBUG_PRINT
#endif
#define CMP_DEBUG_PRINT CMP_INFO_PRINT

static pbnjson::JValue convertStringToJson(const char *rawData)
{
    pbnjson::JInput input(rawData);
    pbnjson::JSchema schema = pbnjson::JSchemaFragment("{}");
    pbnjson::JDomParser parser;
    if (!parser.parse(input, schema))
    {
        return pbnjson::JValue();
    }
    return parser.getDom();
}

CameraServiceClient::CameraServiceClient() :
    name_(""),
    loop_(nullptr),
    sh_(nullptr),
    reply_from_server_(""),
    done_(0),
    fd_(-1),
    handle_(-1),
    pid_(-1)
{
    acquireLSHandle();
}

CameraServiceClient::~CameraServiceClient()
{
    releaseLSHandle();
}

bool CameraServiceClient::acquireLSHandle()
{
    if (!sh_)
    {
        LSError lserror;
        LSErrorInit(&lserror);

        name_ = "com.webos.pipeline.ipc._" + std::to_string(getpid());
        if (!LSRegister(name_.c_str(), &sh_, &lserror))
        {
            CMP_DEBUG_PRINT("CameraServiceClient::acquireLSHandle() FAIL");

            LSErrorPrint(&lserror, stderr);
            LSErrorFree(&lserror);
            return false;
        }

        loop_ = g_main_loop_new(NULL, FALSE);

        if (!LSGmainAttach(sh_, loop_, &lserror))
        {
            LSErrorPrint(&lserror, stderr);
            g_main_loop_unref(loop_);
            loop_ = nullptr;
            if (!LSUnregister(sh_, &lserror))
            {
                LSErrorPrint(&lserror, stderr);
                LSErrorFree(&lserror);
                return false;
            }
            sh_ = nullptr;
            return false;
        }
    }

    CMP_DEBUG_PRINT("CameraServiceClient::acquireLSHandle() OK");

    return true;
}

bool CameraServiceClient::releaseLSHandle()
{
    if (loop_)
    {
        g_main_loop_quit(loop_);
        g_main_loop_unref(loop_);
        loop_ = nullptr;
    }
    if (sh_)
    {
        LSError lserror;
        LSErrorInit(&lserror);
        if (!LSUnregister(sh_, &lserror))
        {
            LSErrorPrint(&lserror, stderr);
            LSErrorFree(&lserror);
            return false;
        }
        LSErrorFree(&lserror);
        sh_ = nullptr;
    }
    return true;
}

bool CameraServiceClient::call(std::string uri, std::string payload, bool (*cb)(LSHandle*, LSMessage*, void*))
{
    done_ = 0;

    LSError lserror;
    LSErrorInit(&lserror);

    GMainContext *context = g_main_loop_get_context(loop_);

    if (!LSCall(sh_, uri.c_str(), payload.c_str(), cb, this, NULL, &lserror))
    {
        LSErrorPrint(&lserror, stderr);
        LSErrorFree(&lserror);
        return false;
    }

    LSErrorFree(&lserror);

    while (!done_)
    {
        g_main_context_iteration(context, false);
        usleep(100);
    }
    return true;
}

bool CameraServiceClient::cbGetReplyMsg(LSHandle *sh, LSMessage *msg, void *ctx)
{
    CMP_DEBUG_PRINT("CameraServiceClient::cbGetReplyMsg() entered.");

    CameraServiceClient *caller = static_cast<CameraServiceClient*>(ctx);
    const char *str = LSMessageGetPayload(msg);
    caller->reply_from_server_ = str ? str : "";

    CMP_DEBUG_PRINT("reply_from_server: %s", caller->reply_from_server_.c_str());

    caller->done_ = 1;
    return true;
}

bool CameraServiceClient::cbGetFd(LSHandle *sh, LSMessage *msg, void *ctx)
{
    CameraServiceClient *caller = static_cast<CameraServiceClient*>(ctx);
    const char *str = LSMessageGetPayload(msg);
    caller->reply_from_server_ = str ? str : "";
    int fd = -1;
    LS::Message ls_message(msg);
    LS::PayloadRef payload_ref = ls_message.accessPayload();
    fd = payload_ref.getFd();
    if (fd)
    {
        caller->fd_ = dup(fd);
    }
    caller->done_ = 1;
    return true;
}

bool CameraServiceClient::open(std::string cameraId)
{
    CMP_DEBUG_PRINT("CameraServiceClient::open() entered ...");
    if (handle_ != -1)
    {
        return false;
    }

    std::string payload = "{\"id\":\"" + cameraId + "\", \"mode\": \"secondary\"";
    if (pid_ > 0)
    {
        payload += + ", \"pid\":" + std::to_string(pid_);
    }
    payload += "}";

    CMP_DEBUG_PRINT("payload : %s", payload.c_str());

    if (!call("luna://com.webos.service.camera2/open", payload, cbGetReplyMsg))
    {
        return false;
    }
    pbnjson::JValue parsed = convertStringToJson(reply_from_server_.c_str());
    if (parsed["returnValue"].asBool() == false)
    {
        return false;
    }
    handle_ = parsed["handle"].asNumber<int>();

    CMP_DEBUG_PRINT("CameraServiceClient::open : handle = %d ", handle_);

    return true;
}

int CameraServiceClient::startPreview(std::string memtype)
{
    if (handle_ == -1)
    {
        return -1;
    }

    std::string payload = "{\"handle\":" + std::to_string(handle_) +
        ",\"params\": {\"source\": \"0\", \"type\":";
    if (memtype == "shmem")
    {
        payload += "\"sharedmemory\"}}";
    }
    else if (memtype == "posixshm")
    {
        payload += "\"posixshm\"}}";
    }
    else
    {
        return -1;
    }
    if (!call("luna://com.webos.service.camera2/startPreview", payload, cbGetReplyMsg))
    {
        return -1;
    }
    pbnjson::JValue parsed = convertStringToJson(reply_from_server_.c_str());
    if (parsed["returnValue"].asBool() == false)
    {
        return -1;
    }
    return parsed["key"].asNumber<int>();
}

bool CameraServiceClient::stopPreview()
{
    if (handle_ == -1)
    {
        return true;
    }

    std::string payload = "{\"handle\":" + std::to_string(handle_) + "}";
    if (!call("luna://com.webos.service.camera2/stopPreview", payload, cbGetReplyMsg))
    {
        return false;
    }
    pbnjson::JValue parsed = convertStringToJson(reply_from_server_.c_str());
    return  parsed["returnValue"].asBool();
}

int CameraServiceClient::getFd()
{
    if (handle_ == -1)
    {
        return -1;
    }
    std::string payload = "{\"handle\":" + std::to_string(handle_) + "}";
    if (!call("luna://com.webos.service.camera2/getFd", payload, cbGetFd))
    {
        return -1;
    }
    return fd_;
}

bool CameraServiceClient::close()
{
    CMP_DEBUG_PRINT("CameraServiceClient::close() entered ...");

    if (handle_ == -1)
    {
        return true;
    }

    std::string payload = "{\"handle\":" + std::to_string(handle_);
    if (pid_ > 0)
    {
        payload += + ", \"pid\":" + std::to_string(pid_);
    }
    payload += "}";
    if (!call("luna://com.webos.service.camera2/close", payload, cbGetReplyMsg))
    {
        return -1;
    }
    pbnjson::JValue parsed = convertStringToJson(reply_from_server_.c_str());

    CMP_DEBUG_PRINT("CameraServiceClient::close : handle = %d ", handle_);

    return parsed["returnValue"].asBool();;
}

