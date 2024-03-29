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

include(FindPkgConfig)

pkg_check_modules(GLIB2 REQUIRED glib-2.0)
include_directories(${GLIB2_INCLUDE_DIRS})
link_directories(${GLIB2_LIBRARY_DIRS})

pkg_check_modules(GSTPLAYER gstreamer-player-1.0 REQUIRED)
include_directories(${GSTPLAYER_INCLUDE_DIRS})
link_directories(${GSTPLAYER_LIBRARY_DIRS})

pkg_check_modules(GSTAPP gstreamer-app-1.0 REQUIRED)
include_directories(${GSTAPP_INCLUDE_DIRS})
link_directories(${GSTAPP_LIBRARY_DIRS})

pkg_check_modules(GSTREAMER gstreamer-1.0 REQUIRED)
include_directories(${GSTREAMER_INCLUDE_DIRS})
link_directories(${GSTREAMER_LIBRARY_DIRS})

pkg_check_modules(GSTPBUTIL gstreamer-pbutils-1.0 REQUIRED)
include_directories(${GSTPBUTIL_INCLUDE_DIRS})
link_directories(${GSTPBUTIL_LIBRARY_DIRS})

pkg_check_modules(LUNASERVICE luna-service2 REQUIRED)
include_directories(${LUNASERVICE_INCLUDE_DIRS})
link_directories(${LUNASERVICE_LIBRARY_DIRS})

pkg_check_modules(PBNJSON pbnjson_cpp REQUIRED)
include_directories(${PBNJSON_INCLUDE_DIRS})
link_directories(${PBNJSON_LIBRARY_DIRS})

pkg_check_modules(RESOURCE-CALCULATOR REQUIRED media-resource-calculator)
include_directories(${RESOURCE-CALCULATOR_INCLUDE_DIRS})
find_package(Threads REQUIRED)
find_package(PkgConfig REQUIRED)

find_package(Boost REQUIRED COMPONENTS regex)
include_directories(${Boost_INCLUDE_DIRS})
link_directories(${Boost_LIBRARY_DIRS})

pkg_check_modules(PMLOGLIB QUIET PmLogLib)
if (PMLOGLIB_FOUND)
  set(PMLOG_LIBRARIES PmLogLib)
endif (PMLOGLIB_FOUND)

set(GSTAPP_LIB gstapp-1.0)
set(G-CAMERA-PIPELINE_LIB cmp-player)

set(G-CAMERA-PIPELINE_HEADERS
    camera_player.h
    ../base/base.h
    ../log/log.h
    ../cameraservice/camera_service.h
    ../resourcefacilitator/requestor.h
    ../util/camshm.h
    ../util/cam_posixshm.h
   )

if (AUTO_PTZ)
list(APPEND G-CAMERA_PIPELINE_HEADERS
    ../postProcess/drawbox.hpp
    ../postProcess/EmptySolution.hpp
    ../postProcess/FaceDataManager.hpp
    ../postProcess/FacePtzSolution.hpp
    ../postProcess/FpsCalc.hpp
    ../postProcess/IPostProcessSolution.hpp
    ../postProcess/PostProcessSolution.hpp
    ../postProcess/PTZMovingParam.hpp
    ../postProcess/SmoothSlidingController.hpp
    ../postProcess/video_crop.hpp
    )
endif()

set(G-CAMERA-PIPELINE_SRC
    camera_player.cpp
    ../parser/parser.cpp
    ../parser/serializer.cpp
    ../log/log.cpp
    ../cameraservice/camera_service.cpp
    ../resourcefacilitator/requestor.cpp
    ../util/camshm.cpp
    ../util/cam_posixshm.cpp
    camera_service_client.cpp
    signal_listener.cpp
    )

if (AUTO_PTZ)
list(APPEND G-CAMERA-PIPELINE_SRC
    ../postProcess/drawbox.cpp
    ../postProcess/EmptySolution.cpp
    ../postProcess/FaceDataManager.cpp
    ../postProcess/FacePtzSolution.cpp
    ../postProcess/FpsCalc.cpp
    ../postProcess/PostProcessSolution.cpp
    ../postProcess/PTZMovingParam.cpp
    ../postProcess/SmoothSlidingController.cpp
    ../postProcess/video_crop.cpp
   )
endif()

set(G-CAMERA-PIPELINE_LIBRARIES
    ums_connector
    ${Boost_REGEX_LIBRARY}
    ${GSTPLAYER_LIBRARIES}
    ${GSTREAMER_LIBRARIES}
    ${GSTPBUTIL_LIBRARIES}
    ${GSTAPP_LIBRARIES}
    ${CMAKE_THREAD_LIBS_INIT}
    ${GLIB2_LIBRARIES}
    ${CMAKE_THREAD_LIBS_INIT}
    ${PBNJSON_LIBRARIES}
    ${PMLOG_LIBRARIES}
    ${LUNASERVICE_LDFLAGS}
    ${RESOURCE-CALCULATOR_LDFLAGS}
    resource_mgr_client
    resource_mgr_client_c
    ${GSTAPP_LIB}
    )

add_library(${G-CAMERA-PIPELINE_LIB} SHARED ${G-CAMERA-PIPELINE_SRC})
target_link_libraries(${G-CAMERA-PIPELINE_LIB}
    ${G-CAMERA-PIPELINE_LIBRARIES}
    ${RESOURCE-CALCULATOR_LDFLAGS}
    resource_mgr_client
    resource_mgr_client_c
    camera-window-manager
    )

if(${WEBOS_TARGET_MACHINE} STREQUAL "raspberrypi4" OR ${WEBOS_TARGET_MACHINE} STREQUAL "raspberrypi4-64" OR
   ${WEBOS_TARGET_MACHINE} STREQUAL "raspberrypi3" OR ${WEBOS_TARGET_MACHINE} STREQUAL "raspberrypi3-64")
  message(STATUS "Using config & debug file for raspberrypi4")
  install(FILES ./raspberrypi/gst_debug.conf DESTINATION /etc/g-camera-pipeline)
  install(FILES ./raspberrypi/gst_elements.conf DESTINATION /etc/g-camera-pipeline)
elseif (${WEBOS_TARGET_MACHINE} STREQUAL "qemux86" OR ${WEBOS_TARGET_MACHINE} STREQUAL "qemux86-64")
  message(STATUS "Using config & debug file for qemux86")
  install(FILES ./qemux86/gst_debug.conf DESTINATION /etc/g-camera-pipeline)
  install(FILES ./qemux86/gst_elements.conf DESTINATION /etc/g-camera-pipeline)
else()
  message(FATAL_ERROR the "Check WEBOS_TARGET_MACHINE: " ${WEBOS_TARGET_MACHINE})
endif()

install(TARGETS cmp-player DESTINATION lib)
install(FILES ${G-CAMERA-PIPELINE_HEADERS} DESTINATION include/cmp)
