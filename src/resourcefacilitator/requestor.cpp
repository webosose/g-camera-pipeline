// Copyright (c) 2019 LG Electronics, Inc.
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
#include <resource_calculator.h>
#include <ResourceManagerClient.h>
#include <MDCClient.h>
#include <MDCContentProvider.h>

#include "resourcefacilitator/requestor.h"
#include "log/log.h"

#define LOGTAG "ResourceRequestor"

namespace cmp { namespace resource {

ResourceRequestor::ResourceRequestor(const std::string& appId)
  : rc_(std::shared_ptr<MRC>(MRC::create())),
    cb_(nullptr),
    allowPolicy_(true) {
  this->appId_ = appId;
  ResourceRequestorInit();
}

ResourceRequestor::ResourceRequestor(const std::string& appId, const std::string& connectionId)
  : rc_(std::shared_ptr<MRC>(MRC::create())),
    cb_(nullptr),
    allowPolicy_(true) {
  this->appId_ = appId;
  this->connectionId_ = connectionId;
  ResourceRequestorInit(connectionId);
}

void ResourceRequestor::ResourceRequestorInit(const std::string& connectionId) {
  umsRMC_ = std::make_shared<uMediaServer::ResourceManagerClient> (connectionId);
  umsMDCCR_ = std::make_shared<MDCContentProvider> (connectionId);
  if ("" == connectionId) {
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
}

void ResourceRequestor::ResourceRequestorInit() {
  umsRMC_ = std::make_shared<uMediaServer::ResourceManagerClient> ();
  umsMDC_ = std::make_shared<MDCClient> ();

  umsRMC_->registerPipeline("media");           // only rmc case
  connectionId_ = umsRMC_->getConnectionID();   // after registerPipeline

  umsMDC_->registerMedia(connectionId_, appId_);

  umsMDCCR_ = std::make_shared<MDCContentProvider> (connectionId_);

  if ("" == connectionId_) {
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
}

void ResourceRequestor::unregisterWithMDC() {
  if (!umsMDC_->unregisterMedia()) {
    CMP_DEBUG_PRINT("MDC unregister error");
  }
}

bool ResourceRequestor::endOfStream() {
  return umsMDC_->updatePlaybackState(PLAYBACK_STOPPED);
}

ResourceRequestor::~ResourceRequestor() {
  if (!acquiredResource_.empty()) {
    umsRMC_->release(acquiredResource_);
    acquiredResource_ = "";
  }
}

bool ResourceRequestor::acquireResources(void* meta, PortResource_t& resourceMMap, cmp::base::disp_res_t & res, const int32_t display_path) {

  mrc::ResourceList AResource;
  mrc::ResourceList audioOptions;
  mrc::ResourceListOptions VResource;
  mrc::ResourceListOptions DisplayResource;
  mrc::ResourceListOptions finalOptions;

  VResource = rc_->calcVdecResourceOptions((MRC::VideoCodecs)translateVideoCodec(videoResData_.vcodec),
      videoResData_.width,
      videoResData_.height,
      videoResData_.frameRate,
      (MRC::ScanType)translateScanType(videoResData_.escanType),
      (MRC::_3DType)translate3DType(videoResData_.e3DType));
  CMP_DEBUG_PRINT("VResource size:%d, %s, %d",
        VResource.size(), VResource[0].front().type.c_str(), VResource[0].front().quantity);
  finalOptions.push_back(audioOptions);
  mrc::concatResourceListOptions(&finalOptions, &VResource);
  DisplayResource = rc_->calcDisplayPlaneResourceOptions(mrc::ResourceCalculator::RenderMode::kModePunchThrough);
  mrc::concatResourceListOptions(&finalOptions, &DisplayResource);

  pbnjson::JSchemaFragment input_schema("{}");
  pbnjson::JGenerator serializer(NULL);
  std::string payload;
  std::string response;

  pbnjson::JValue objArray = pbnjson::Array();
  for (auto option : finalOptions) {
    for (auto it : option) {
      pbnjson::JValue obj = pbnjson::Object();
      obj.put("resource", it.type + (it.type == "DISP" ? std::to_string(display_path) : ""));
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
    parsePortInformation(response, resourceMMap, res);
    parseResources(response, acquiredResource_);
  } catch (const std::runtime_error & err) {
    CMP_DEBUG_PRINT("[%s:%d] err=%s, response:%s",
          __func__, __LINE__, err.what(), response.c_str());
    return false;
  }

  CMP_DEBUG_PRINT("acquired Resource : %s", acquiredResource_.c_str());
  return true;
}

bool ResourceRequestor::releaseResource() {
  if (acquiredResource_.empty()) {
    CMP_DEBUG_PRINT("[%s], resource already empty", __func__);
    return true;
  }

  CMP_DEBUG_PRINT("send release to uMediaServer. resource : %s", acquiredResource_.c_str());

  if (!umsRMC_->release(acquiredResource_)) {
    CMP_DEBUG_PRINT("release error : %s", acquiredResource_.c_str());
    return false;
  }

  acquiredResource_ = "";
  return true;
}

bool ResourceRequestor::notifyForeground() const {
  umsRMC_->notifyForeground();
  return true;
}

bool ResourceRequestor::notifyBackground() const {
  umsRMC_->notifyBackground();
  return true;
}

bool ResourceRequestor::notifyActivity() const {
  umsRMC_->notifyActivity();
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
  CMP_DEBUG_PRINT("policyActionHandler action:%s, resources:%s, type:%s, name:%s, id:%s",
        action, resources, requestorType, requestorName, connectionId);
  if (allowPolicy_) {
    if (nullptr != cb_) {
      cb_();
    }
    if (!umsRMC_->release(acquiredResource_)) {
      CMP_DEBUG_PRINT("release error in policyActionHandler: %s", acquiredResource_.c_str());
      return false;
    }
  }

  return allowPolicy_;
}

bool ResourceRequestor::parsePortInformation(const std::string& payload, PortResource_t& resourceMMap, cmp::base::disp_res_t & res) {
  pbnjson::JDomParser parser;
  pbnjson::JSchemaFragment input_schema("{}");
  if (!parser.parse(payload, input_schema)) {
    throw std::runtime_error("payload parsing failure during parsePortInformation");
  }

  pbnjson::JValue parsed = parser.getDom();
  if (!parsed.hasKey("resources")) {
    throw std::runtime_error("payload must have \"resources key\"");
  }

  int res_arraysize = parsed["resources"].arraySize();

  for (int i=0; i < res_arraysize; ++i) {
    std::string resource = parsed["resources"][i]["resource"].asString();
    int32_t value = parsed["resources"][i]["index"].asNumber<int32_t>();
    if (resource.find("DISP") != std::string::npos) {
      res.plane_id = parsed["resources"][i]["display_attr"]["plane-id"].asNumber<int32_t>();
      res.crtc_id = parsed["resources"][i]["display_attr"]["crtc-id"].asNumber<int32_t>();
      res.conn_id = parsed["resources"][i]["display_attr"]["conn-id"].asNumber<int32_t>();
    }
    resourceMMap.insert(std::make_pair(resource, value));
  }

  for (auto& it : resourceMMap) {
    CMP_DEBUG_PRINT("port Resource - %s, : [%d] ", it.first.c_str(), it.second);
  }

  return true;
}

bool ResourceRequestor::parseResources(const std::string& payload, std::string& resources) {
  pbnjson::JDomParser parser;
  pbnjson::JSchemaFragment input_schema("{}");
  pbnjson::JGenerator serializer(NULL);

  if (!parser.parse(payload, input_schema)) {
    throw std::runtime_error("payload parsing failure during parseResources");
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
    throw std::runtime_error("fail to serializer toString during parseResources");
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
      ev = MRC::kVideoMPEG4;   break;
    case CMP_VIDEO_CODEC_VP8:
      ev = MRC::kVideoVP8;    break;
    case CMP_VIDEO_CODEC_VP9:
      ev = MRC::kVideoVP9;    break;
    case CMP_VIDEO_CODEC_MJPEG:
      ev = MRC::kVideoMJPEG;  break;
    default:
      ev = MRC::kVideoH264;
      break;
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

bool ResourceRequestor::setVideoDisplayWindow(const long left, const long top,
    const long width, const long height,
    const bool isFullScreen) const {
  if (isFullScreen)
    return umsMDC_->switchToFullscreen() ? false :true;

  return umsMDC_->setDisplayWindow(window_t(left, top, width, height)) ? false :true;
}

bool ResourceRequestor::setVideoCustomDisplayWindow(const long src_left, const long src_top,
    const long src_width, const long src_height,
    const long dst_left, const long dst_top,
    const long dst_width, const long dst_height,
    const bool isFullScreen) const {
  if (isFullScreen)
    return umsMDC_->switchToFullscreen() ? false :true;

  return umsMDC_->setDisplayWindow(window_t(src_left, src_top, src_width, src_height),
      window_t(dst_left, dst_top, dst_width, dst_height)) ? false :true;
}

bool ResourceRequestor::mediaContentReady(bool state) {
  return umsMDCCR_->mediaContentReady(state);
}
bool ResourceRequestor::setVideoInfo(const cmp::base::video_info_t &videoInfo) {
  video_info_.width = videoInfo.width;
  video_info_.height = videoInfo.height;
  video_info_.frame_rate.num = videoInfo.frame_rate.num;
  video_info_.frame_rate.den = videoInfo.frame_rate.den;
  video_info_.codec = videoInfo.codec;
  video_info_.bit_rate = videoInfo.bit_rate;
  CMP_INFO_PRINT("setting videoSize[ %d, %d ]", video_info_.width, video_info_.height);

  return umsMDCCR_->setVideoInfo(video_info_);
}

bool ResourceRequestor::setSourceInfo(const cmp::base::source_info_t &sourceInfo) {
  cmp::base::video_info_t video_stream_info = sourceInfo.video_streams.front();

  videoResData_.width = video_stream_info.width;
  videoResData_.height = video_stream_info.height;
  videoResData_.vcodec = CMP_VIDEO_CODEC_MJPEG;
  videoResData_.frameRate = std::round(static_cast<float>(video_stream_info.frame_rate.num) /
                                       static_cast<float>(video_stream_info.frame_rate.den));
  return true;
}



void ResourceRequestor::planeIdHandler(int32_t planePortIdx) {
  CMP_DEBUG_PRINT("planePortIndex = %d", planePortIdx);
  if (nullptr != planeIdCb_) {
    bool res = planeIdCb_(planePortIdx);
    CMP_DEBUG_PRINT("PlanePort[%d] register : %s", planePortIdx, res ? "success!" : "fail!");
  }
}
void ResourceRequestor::setAppId(std::string id) {
  appId_ = id;
}
}  // namespace resource
}  // namespace cmp
