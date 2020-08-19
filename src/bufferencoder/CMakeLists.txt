# Copyright (c) 2020 LG Electronics, Inc.
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
set(ENCODER-PIPELINE_LIB buffer-encoder)

set(ENCODER-PIPELINE_HEADERS
    buffer_encoder.h
    ../base/base.h
    ../log/log.h
)

set(ENCODER-PIPELINE_SRC
    buffer_encoder.cpp
    ../parser/parser.cpp
    ../parser/serializer.cpp
    ../log/log.cpp
)

set(ENCODER-PIPELINE_LIBRARIES
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
    ${GSTAPP_LIB}
)

add_library(${ENCODER-PIPELINE_LIB} SHARED ${ENCODER-PIPELINE_SRC})

target_link_libraries(${ENCODER-PIPELINE_LIB}
    ${ENCODER-PIPELINE_LIBRARIES}
)

install(TARGETS buffer-encoder DESTINATION lib)
install(FILES ${ENCODER-PIPELINE_HEADERS} DESTINATION include/cmp)
