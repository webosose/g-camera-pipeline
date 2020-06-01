// Copyright (c) 2020 LG Electronics, Inc.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
// http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or
// implied.
// See the License for the specific language governing permissions and
// limitations under the License.

// SPDX-License-Identifier: Apache-2.0

#ifndef _LSM_CameraWindowManager_H_
#define _LSM_CameraWindowManager_H_

#include <glib.h>
#include <memory>

struct wl_display;
struct wl_surface;

namespace Wayland
{
class Foreign;
class Importer;
class Surface;
}

namespace LSM
{

class CameraWindowManager
{
public:
    CameraWindowManager(void);
    ~CameraWindowManager(void);

    bool registerID(const char *windowID, const char *pipelineID);
    bool unregisterID(void);

    bool attachPunchThrough(void);
    bool detachPunchThrough(void);

    bool attachSurface(void);
    bool detachSurface(void);

    struct wl_display *getDisplay(void);
    struct wl_surface *getSurface(void);

    void getVideoSize(gint &width, gint &height);
    void setVideoSize(gint width, gint height);

private:
    //Disallow copy and assign
    CameraWindowManager(const CameraWindowManager &);
    void operator=(const CameraWindowManager &);

    gint video_width = 0;
    gint video_height = 0;

    std::shared_ptr<Wayland::Foreign> foreign;
    std::shared_ptr<Wayland::Surface> surface;
    std::shared_ptr<Wayland::Importer> importer;

    bool isRegistered;
};

} //namespace LSM

#endif //_LSM_CameraWindowManager_H_
