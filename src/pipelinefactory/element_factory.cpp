#include "element_factory.h"
#include "log.h"

static const std::string gst_element_json_path = "/etc/g-camera-pipeline/gst_elements.conf";

std::string ElementFactory::GetPreferredElementName(const std::string &pipelineType,
                                                    const std::string &elementTypeName)
{
    std::string elementName = "";
    pbnjson::JValue root    = pbnjson::JDomParser::fromFile(gst_element_json_path.c_str());
    if (!root.isObject())
    {
        CMP_LOG_INFO("Gst element file parsing error");
        return elementName;
    }

    pbnjson::JValue gstElements = root["gst_elements"];
    for (const auto &elements : gstElements.items())
    {
        if (elements.hasKey("pipeline-type") &&
            pipelineType == elements["pipeline-type"].asString())
        {
            if (elements.hasKey(elementTypeName) && elements[elementTypeName].hasKey("name"))
            {
                elementName = elements[elementTypeName]["name"].asString();
                CMP_LOG_INFO("[%s] %s: %s", pipelineType.c_str(), elementTypeName.c_str(),
                             elementName.c_str());
            }
            else
            {
                CMP_LOG_INFO("elementTypeName: %s is not exist", elementTypeName.c_str());
            }
            break;
        }
    }

    return elementName;
}

void ElementFactory::SetProperties(const std::string &pipelineType, GstElement *element,
                                   const std::string &elementTypeName)
{
    pbnjson::JValue root = pbnjson::JDomParser::fromFile(gst_element_json_path.c_str());
    if (!root.isObject())
    {
        CMP_LOG_INFO("Gst element file parsing error");
        return;
    }

    pbnjson::JValue gstElements = root["gst_elements"];
    for (const auto &elements : gstElements.items())
    {
        if (elements.hasKey("pipeline-type") &&
            pipelineType == elements["pipeline-type"].asString())
        {
            if (elements.hasKey(elementTypeName) && elements[elementTypeName].hasKey("name"))
            {
                if (elements[elementTypeName].hasKey("properties"))
                {
                    for (const auto &it : elements[elementTypeName]["properties"].children())
                    {
                        SetProperty(element, it.first, it.second);
                    }
                }
            }
            break;
        }
    }
}

void ElementFactory::SetProperty(GstElement *element, const pbnjson::JValue &prop,
                                 const pbnjson::JValue &value)
{
    const pbnjson::JValue &objProp  = prop;
    const pbnjson::JValue &objValue = value;
    if (objProp.isString())
    {
        std::string strProp      = objProp.asString();
        const gchar *elementName = gst_element_get_name(element);
        if (objValue.isNumber())
        {
            gint32 num = objValue.asNumber<gint32>();
            CMP_LOG_INFO("[%s] %s: %d", elementName, strProp.c_str(), num);
            g_object_set(G_OBJECT(element), strProp.c_str(), num, nullptr);
        }
        else if (objValue.isString())
        {
            CMP_LOG_INFO("[%s] %s: %s", elementName, strProp.c_str(), objValue.asString().c_str());
            g_object_set(G_OBJECT(element), strProp.c_str(), objValue.asString().c_str(), nullptr);
        }
        else if (objValue.isBoolean())
        {
            CMP_LOG_INFO("[%s] %s: %s", elementName, strProp.c_str(),
                         objValue.asBool() ? "true" : "false");
            g_object_set(G_OBJECT(element), strProp.c_str(), objValue.asBool(), nullptr);
        }
        else
        {
            CMP_LOG_INFO("Please check the value type of %s", strProp.c_str());
        }
    }
    else
    {
        CMP_LOG_INFO("A property name should be string");
    }
}
