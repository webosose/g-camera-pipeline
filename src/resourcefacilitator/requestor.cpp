// Copyright (c) 2019-2020 LG Electronics, Inc.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
// http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//
// SPDX-License-Identifier: Apache-2.0

#include <string>
#include <set>
#include <utility>
#include <cmath>
#include <base/base.h>
#include <pbnjson.hpp>
#include <ResourceManagerClient.h>

#include <boost/regex.hpp>

#include "resourcefacilitator/requestor.h"
#include "log/log.h"

#define LOGTAG "ResourceRequestor"

#ifdef CMP_DEBUG_PRINT
#undef CMP_DEBUG_PRINT
#endif
#define CMP_DEBUG_PRINT CMP_INFO_PRINT

using namespace std;
using mrc::ResourceCalculator;
using namespace pbnjson;
using namespace cmp::base;

namespace cmp { namespace resource {

// FIXME : temp. set to 0 for request max
#define FAKE_WIDTH_MAX 0
#define FAKE_HEIGHT_MAX 0
#define FAKE_FRAMERATE_MAX 0

ResourceRequestor::ResourceRequestor(const std::string& appId,
                                     const std::string& connectionId)
  : rc_(std::shared_ptr<MRC>(MRC::create())),
    appId_(appId), instanceId_(""),
    cb_(nullptr),
    allowPolicy_(true) {
  try {
    if (connectionId.empty()) {
      umsRMC_ = make_shared<uMediaServer::ResourceManagerClient> ();
      CMP_DEBUG_PRINT("ResourceRequestor creation done");

      umsRMC_->registerPipeline("media", appId_); // only rmc case
      connectionId_ = umsRMC_->getConnectionID(); // after registerPipeline
    }
    else {
      umsRMC_ = make_shared<uMediaServer::ResourceManagerClient> (connectionId);
      connectionId_ = connectionId;
    }
  }
  catch (const std::exception &e) {
    CMP_DEBUG_PRINT("Failed to create ResourceRequestor [%s]", e.what());
    exit(0);
  }

  if (connectionId_.empty()) {
    CMPASSERT(0);
  }

  umsRMC_->registerPolicyActionHandler(
      std::bind(&ResourceRequestor::policyActionHandler,
        this,
        std::placeholders::_1,
        std::placeholders::_2,
        std::placeholders::_3,
        std::placeholders::_4,
        std::placeholders::_5));
  CMP_DEBUG_PRINT("ResourceRequestor creation done");
}

ResourceRequestor::~ResourceRequestor() {
  if (!acquiredResource_.empty()) {
    umsRMC_->release(acquiredResource_);
    acquiredResource_ = "";
  }
}

bool ResourceRequestor::acquireResources(PortResource_t& resourceMMap,
                                         const cmp::base::source_info_t &sourceInfo,
                                         const std::string &display_mode,
                                         const int32_t display_path) {

  mrc::ResourceListOptions finalOptions;

  if (!setSourceInfo(sourceInfo)) {
    CMP_DEBUG_PRINT("Failed to set source info!");
    return false;
  }

  mrc::ResourceListOptions VResource = calcVdecResources();
  if (!VResource.empty()) {
    mrc::concatResourceListOptions(&finalOptions, &VResource);
    CMP_DEBUG_PRINT("VResource size:%lu, %s, %d", VResource.size(),
                                                  VResource[0].front().type.c_str(),
                                                  VResource[0].front().quantity);
  }

  mrc::ResourceListOptions DisplayResource = calcDisplayResource(display_mode);
  if (!DisplayResource.empty()) {
    mrc::concatResourceListOptions(&finalOptions, &DisplayResource);
    CMP_DEBUG_PRINT("DisplayResource size:%lu, %s, %d", DisplayResource.size(),
                                                        DisplayResource[0].front().type.c_str(),
                                                        DisplayResource[0].front().quantity);
  }

  JSchemaFragment input_schema("{}");
  JGenerator serializer(nullptr);
  string payload;
  string response;

  JValue objArray = pbnjson::Array();
  for (auto option : finalOptions) {
    for (auto it : option) {
      JValue obj = pbnjson::Object();
      obj.put("resource", it.type + (it.type == "DISP" ? to_string(display_path) : ""));
      obj.put("qty", it.quantity);
      CMP_DEBUG_PRINT("calculator return : %s, %d", it.type.c_str(), it.quantity);
      objArray << obj;
    }
  }

  if (!serializer.toString(objArray, input_schema, payload)) {
    CMP_DEBUG_PRINT("[%s], fail to serializer to string", __func__);
    return false;
  }

  CMP_DEBUG_PRINT("send acquire to uMediaServer payload:%s", payload.c_str());

  if (!umsRMC_->acquire(payload, response)) {
    CMP_DEBUG_PRINT("fail to acquire!!! response : %s", response.c_str());
    return false;
  }
  CMP_DEBUG_PRINT("acquire response:%s", response.c_str());

  try {
    parsePortInformation(response, resourceMMap);
    parseResources(response, acquiredResource_);
  } catch (const std::runtime_error & err) {
    CMP_DEBUG_PRINT("[%s:%d] err=%s, response:%s",
          __func__, __LINE__, err.what(), response.c_str());
    return false;
  }

  CMP_DEBUG_PRINT("acquired Resource : %s", acquiredResource_.c_str());
  return true;
}

mrc::ResourceListOptions ResourceRequestor::calcVdecResources() {
  mrc::ResourceListOptions VResource;

  if (videoResData_.vcodec != CMP_VIDEO_CODEC_NONE) {
    VResource = rc_->calcVdecResourceOptions((MRC::VideoCodecs)translateVideoCodec(videoResData_.vcodec),
                                             videoResData_.width,
                                             videoResData_.height,
                                             videoResData_.frameRate,
                                             (MRC::ScanType)translateScanType(videoResData_.escanType),
                                             (MRC::_3DType)translate3DType(videoResData_.e3DType));
  }

  return VResource;
}

mrc::ResourceListOptions ResourceRequestor::calcDisplayResource(const std::string &display_mode) {
  mrc::ResourceListOptions DisplayResource;

  if (videoResData_.vcodec != CMP_VIDEO_CODEC_NONE) {
    /* need to change display_mode type from string to enum */
    if (display_mode.compare("PunchThrough") == 0) {
      DisplayResource = rc_->calcDisplayPlaneResourceOptions(mrc::ResourceCalculator::RenderMode::kModePunchThrough);
    } else if (display_mode.compare("Textured") == 0) {
      DisplayResource = rc_->calcDisplayPlaneResourceOptions(mrc::ResourceCalculator::RenderMode::kModeTexture);
    } else {
      CMP_DEBUG_PRINT("Wrong display mode: %s", display_mode.c_str());
    }
  }

  return DisplayResource;
}

bool ResourceRequestor::releaseResource() {
  if (acquiredResource_.empty()) {
    CMP_DEBUG_PRINT("[%s], resource already empty", __func__);
    return true;
  }

  CMP_DEBUG_PRINT("send release to uMediaServer. resource : %s",
                  acquiredResource_.c_str());

  if (!umsRMC_->release(acquiredResource_)) {
    CMP_DEBUG_PRINT("release error : %s", acquiredResource_.c_str());
    return false;
  }

  acquiredResource_ = "";
  return true;
}

bool ResourceRequestor::notifyForeground() const {
  return umsRMC_->notifyForeground();
}

bool ResourceRequestor::notifyBackground() const {
  return umsRMC_->notifyBackground();
}

bool ResourceRequestor::notifyActivity() const {
  return umsRMC_->notifyActivity();
}

bool ResourceRequestor::notifyPipelineStatus(const std::string& status) const {
  umsRMC_->notifyPipelineStatus(status);
  return true;
}

void ResourceRequestor::allowPolicyAction(const bool allow) {
  allowPolicy_ = allow;
}

bool ResourceRequestor::policyActionHandler(const char *action,
                                            const char *resources,
                                            const char *requestorType,
                                            const char *requestorName,
                                            const char *connectionId) {
  CMP_DEBUG_PRINT("action:%s, resources:%s, type:%s, name:%s, id:%s",
      action, resources, requestorType, requestorName, connectionId);

  if (allowPolicy_) {
    if (nullptr != cb_) {
      cb_();
    }
    if (!umsRMC_->release(acquiredResource_)) {
      CMP_DEBUG_PRINT("release error : %s", acquiredResource_.c_str());
      return false;
    }
  }

  return allowPolicy_;
}

bool ResourceRequestor::parsePortInformation(const std::string& payload,
                                             PortResource_t& resourceMMap) {
  pbnjson::JDomParser parser;
  pbnjson::JSchemaFragment input_schema("{}");
  if (!parser.parse(payload, input_schema)) {
    throw std::runtime_error(" : payload parsing failure");
  }

  pbnjson::JValue parsed = parser.getDom();
  if (!parsed.hasKey("resources")) {
    throw std::runtime_error("payload must have \"resources key\"");
  }

  int res_arraysize = parsed["resources"].arraySize();

  for (int i=0; i < res_arraysize; ++i) {
    std::string resourceName = parsed["resources"][i]["resource"].asString();
    int32_t value = parsed["resources"][i]["index"].asNumber<int32_t>();
    resourceMMap.insert(std::make_pair(resourceName, value));
  }

  for (auto& it : resourceMMap) {
    CMP_DEBUG_PRINT("port Resource - %s, : [%d] ", it.first.c_str(), it.second);
  }

  return true;
}

bool ResourceRequestor::parseResources(const std::string& payload,
                                       std::string& resources) {
  pbnjson::JDomParser parser;
  pbnjson::JSchemaFragment input_schema("{}");
  pbnjson::JGenerator serializer(nullptr);

  if (!parser.parse(payload, input_schema)) {
    throw std::runtime_error(" : payload parsing failure");
  }

  pbnjson::JValue parsed = parser.getDom();
  if (!parsed.hasKey("resources")) {
    throw std::runtime_error("payload must have \"resources key\"");
  }

  pbnjson::JValue objArray = pbnjson::Array();
  for (int i=0; i < parsed["resources"].arraySize(); ++i) {
    pbnjson::JValue obj = pbnjson::Object();
    obj.put("resource", parsed["resources"][i]["resource"].asString());
    obj.put("index", parsed["resources"][i]["index"].asNumber<int32_t>());
    objArray << obj;
  }

  if (!serializer.toString(objArray, input_schema, resources)) {
    throw std::runtime_error("failed to serializer toString");
  }

  return true;
}

int ResourceRequestor::translateVideoCodec(const CMP_VIDEO_CODEC vcodec) const {
  MRC::VideoCodec ev = MRC::kVideoEtc;
  switch (vcodec) {
    case CMP_VIDEO_CODEC_NONE:
      ev = MRC::kVideoEtc;    break;
    case CMP_VIDEO_CODEC_H264:
      ev = MRC::kVideoH264;   break;
    case CMP_VIDEO_CODEC_H265:
      ev = MRC::kVideoH265;   break;
    case CMP_VIDEO_CODEC_MPEG2:
      ev = MRC::kVideoMPEG;   break;
    case CMP_VIDEO_CODEC_MPEG4:
      ev = MRC::kVideoMPEG4;  break;
    case CMP_VIDEO_CODEC_VP8:
      ev = MRC::kVideoVP8;    break;
    case CMP_VIDEO_CODEC_VP9:
      ev = MRC::kVideoVP9;    break;
    case CMP_VIDEO_CODEC_MJPEG:
      ev = MRC::kVideoMJPEG;  break;
    default:
      ev = MRC::kVideoH264;   break;
  }

  CMP_DEBUG_PRINT("vcodec[%d] => ev[%d]", vcodec, ev);

  return static_cast<int>(ev);
}

int ResourceRequestor::translateScanType(const int escanType) const {
  MRC::ScanType scan = MRC::kScanProgressive;

  switch (escanType) {
    default:
      break;
  }

  return static_cast<int>(scan);
}

int ResourceRequestor::translate3DType(const int e3DType) const {
  MRC::_3DType my3d = MRC::k3DNone;

  switch (e3DType) {
    default:
      my3d = MRC::k3DNone;
      break;
  }

  return static_cast<int>(my3d);
}

bool ResourceRequestor::setSourceInfo(
    const cmp::base::source_info_t &sourceInfo) {
  if (sourceInfo.video_streams.empty()){
    CMP_DEBUG_PRINT("Video/Audio streams are empty!");
    return false;
  }

  cmp::base::video_info_t video_stream_info = sourceInfo.video_streams.front();
  videoResData_.width = video_stream_info.width;
  videoResData_.height = video_stream_info.height;
  videoResData_.vcodec = (CMP_VIDEO_CODEC)video_stream_info.codec;
  videoResData_.frameRate =
  std::round(static_cast<float>(video_stream_info.frame_rate.num) /
                 static_cast<float>(video_stream_info.frame_rate.den));
  videoResData_.escanType = 0;

  return true;
}

void ResourceRequestor::planeIdHandler(int32_t planePortIdx) {
  CMP_DEBUG_PRINT("planePortIndex = %d", planePortIdx);
  if (nullptr != planeIdCb_) {
    bool res = planeIdCb_(planePortIdx);
    CMP_DEBUG_PRINT("PlanePort[%d] register : %s",
                    planePortIdx, res ? "success!" : "fail!");
  }
}

void ResourceRequestor::setAppId(std::string id) {
  appId_ = id;
}

int32_t ResourceRequestor::getDisplayPath() {
  return umsRMC_->getDisplayID();
}

}  // namespace resource
}  // namespace cmp
