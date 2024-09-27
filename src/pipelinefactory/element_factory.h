#ifndef ELEMENT_FACTORY_H_
#define ELEMENT_FACTORY_H_

#include <string>
extern "C"
{
#include <gst/gst.h>
}
#include <pbnjson.hpp>

class ElementFactory
{
    static void SetProperty(GstElement *element, const pbnjson::JValue &prop,
                            const pbnjson::JValue &value);

public:
    static std::string GetPreferredElementName(const std::string &pipelineType,
                                               const std::string &elementTypeName);
    static void SetProperties(const std::string &pipelineType, GstElement *element,
                              const std::string &elementTypeName);
};
#endif // ELEMENT_FACTORY_H_
