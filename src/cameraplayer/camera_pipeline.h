#ifndef CAMERA_PIPELINE_H_
#define CAMERA_PIPELINE_H_

#include <string>
extern "C"
{
#include <gst/gst.h>
}
#include <functional>

using CALLBACK_T =
    std::function<void(const gint type, const gint64 numValue, const gchar *strValue, void *udata)>;

class CameraPipeline
{
public:
    virtual bool Load(const std::string &msg)       = 0;
    virtual bool Unload()                           = 0;
    virtual bool Play()                             = 0;
    virtual void RegisterCbFunction(CALLBACK_T cbf) = 0;

    virtual bool TakeSnapshot(const std::string &location) { return true; }
    virtual bool StartRecord(const std::string &location, const std::string &format, bool audio,
                             const std::string &audioSrc)
    {
        return true;
    }
    virtual bool StopRecord() { return true; }
};

#endif // CAMERA_PIPELINE_H_
