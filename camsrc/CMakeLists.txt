# Copyright (c) 2019-2021 LG Electronics, Inc.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
# http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#
# SPDX-License-Identifier: Apache-2.0

find_package(PkgConfig REQUIRED)
#camsrc

pkg_check_modules(GLIB2 REQUIRED glib-2.0)
include_directories(${GLIB2_INCLUDE_DIRS})
link_directories(${GLIB2_LIBRARY_DIRS})

pkg_check_modules(GSTPLAYER gstreamer-player-1.0 REQUIRED)
include_directories(${GSTPLAYER_INCLUDE_DIRS})
link_directories(${GSTPLAYER_LIBRARY_DIRS})

pkg_check_modules(GSTAPP gstreamer-app-1.0 REQUIRED)
include_directories(${GSTAPP_INCLUDE_DIRS})
link_directories(${GSTAPP_LIBRARY_DIRS})

pkg_check_modules(GSTBASE gstreamer-base-1.0 REQUIRED)
include_directories(${GSTBASE_INCLUDE_DIRS})
link_directories(${GSTBASE_LIBRARY_DIRS})

pkg_check_modules(GSTALLOCATORS gstreamer-allocators-1.0 REQUIRED)
include_directories(${GSTALLOCATORS_INCLUDE_DIRS})
link_directories(${GSTALLOCATORS_LIBRARY_DIRS})

pkg_check_modules(GSTREAMER gstreamer-1.0 REQUIRED)
include_directories(${GSTREAMER_INCLUDE_DIRS})
link_directories(${GSTREAMER_LIBRARY_DIRS})

pkg_check_modules(GSTPBUTIL gstreamer-pbutils-1.0 REQUIRED)
include_directories(${GSTPBUTIL_INCLUDE_DIRS})
link_directories(${GSTPBUTIL_LIBRARY_DIRS})

set(CAMSRC_LIBRARIES
    ${GSTPLAYER_LIBRARIES}
    ${GSTREAMER_LIBRARIES}
    ${GSTPBUTIL_LIBRARIES}
    ${GSTAPP_LIBRARIES}
    ${GSTBASE_LIBRARIES}
    ${GSTALLOCATORS_LIBRARIES}
    ${CMAKE_THREAD_LIBS_INIT}
    ${GLIB2_LIBRARIES}
    ${CMAKE_THREAD_LIBS_INIT}
)

include_directories(${CMAKE_SOURCE_DIR}/camsrc/)

add_library(gstcamsrc SHARED
    gstcamsrc.c
    gstcamsrc.h
    $ENV{PKG_CONFIG_SYSROOT_DIR}/usr/include/camera_hal_if_types.h
    $ENV{PKG_CONFIG_SYSROOT_DIR}/usr/include/camera_hal_if.h
    )

target_link_libraries(gstcamsrc
    ${CAMSRC_LIBRARIES}
    camera_hal
    )

install(TARGETS gstcamsrc DESTINATION /usr/lib/gstreamer-1.0/)
