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

find_package(PkgConfig REQUIRED)

pkg_check_modules(PMLOGLIB QUIET PmLogLib)
if (PMLOGLIB_FOUND)
  set(PMLOG_LIBRARIES PmLogLib)
endif (PMLOGLIB_FOUND)

include_directories(${CMAKE_CURRENT_BINARY_DIR})

include_directories(.)

include_directories(./base)

include_directories(./cameraservice)

include_directories(./log)

include_directories(./util)

if(AUTO_PTZ)
include_directories(./postProcess)
endif()

include_directories(./cameraplayer)

include_directories(./mediaencoderclient)

include_directories(./bufferencoder)

include_directories(./camera-window-manager/include)

add_subdirectory(bufferencoder)
add_subdirectory(mediaencoderclient)
add_subdirectory(cameraplayer)
add_subdirectory(camera-window-manager)
add_subdirectory(tests)

find_package(Threads REQUIRED)

add_executable(g-camera-pipeline main.cpp)
target_link_libraries(g-camera-pipeline cmp-player ${PMLOG_LIBRARIES})
install(TARGETS g-camera-pipeline DESTINATION ${WEBOS_INSTALL_SBINDIR})
