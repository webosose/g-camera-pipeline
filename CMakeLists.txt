# Copyright (c) 2019-2023 LG Electronics, Inc.
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

cmake_minimum_required(VERSION 2.8)

project(g-camera-pipeline)

option(AUTO_PTZ "Enable/Disable auto ptz feature" OFF)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++14 -lrt")

if(NOT DEFINED WEBOS_INSTALL_ROOT)
  set(WEBOS_INSTALL_ROOT /usr/local/webos/)
endif()

if(NOT DEFINED WEBOS_INSTALL_SBINDIR)
  set(WEBOS_INSTALL_SBINDIR /usr/sbin)
endif()

set(CMAKE_INSTALL_PREFIX ${WEBOS_INSTALL_ROOT}usr)

if (NOT DEFINED ENV{PKG_CONFIG_PATH})
  set(ENV{PKG_CONFIG_PATH} ${WEBOS_INSTALL_ROOT}usr/share/pkgconfig)
endif()

file(GLOB_RECURSE HEADERS "*.h*")
add_custom_target(show_headers SOURCES ${HEADERS})

find_package(PkgConfig REQUIRED)

pkg_check_modules(GSTPLAYER gstreamer-player-1.0 REQUIRED)
pkg_check_modules(GSTREAMER gstreamer-1.0 REQUIRED)

set(GSTREAMER_INCLUDE_DIRS ${GSTPLAYER_INCLUDE_DIRS} ${GSTREAMER_INCLUDE_DIRS})
set(GSTREAMER_LIBRARIES ${GSTPLAYER_LIBRARIES} ${GSTREAMER_LIBRARIES})

include_directories(${GSTREAMER_INCLUDE_DIRS})
link_directories(${GSTREAMER_LIBRARIES})


pkg_check_modules(MEDIA_RESOURE_CALCULATOR media-resource-calculator REQUIRED)
set(MEDIA_RESOURE_CALCULATOR_INCLUDE_DIRS ${MEDIA_RESOURE_CALCULATOR_INCLUDE_DIRS})
set(MEDIA_RESOURE_CALCULATOR_LIBRARIES ${MEDIA_RESOURE_CALCULATOR_LIBRARIES})

include_directories(${MEDIA_RESOURE_CALCULATOR_INCLUDE_DIRS})
link_directories(${MEDIA_RESOURE_CALCULATOR_LIBRARIES})

if(${WEBOS_TARGET_MACHINE} STREQUAL "qemux86" OR ${WEBOS_TARGET_MACHINE} STREQUAL "qemux86-64")
    add_definitions(-DPLATFORM_QEMUX86)
else()
    add_definitions(-DPTZ_ENABLED)
    set(AUTO_PTZ ON CACHE BOOL "" FORCE)
endif()

add_subdirectory(src)
add_subdirectory(camsrc)
add_subdirectory(files)
