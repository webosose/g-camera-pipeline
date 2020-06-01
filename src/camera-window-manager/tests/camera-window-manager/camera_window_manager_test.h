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

#include <gtest/gtest.h>
#include <iostream>
#include "camera_window_manager.h"
#include "api_call_checker.h"

class LSMCameraWindowManagerTest1 : public ::testing::Test
{
protected:
    LSM::CameraWindowManager CameraWindowManager;
};

TEST_F(LSMCameraWindowManagerTest1, RegisterAndUnregister)
{
    //Arrange
    bool expected          = true;
    const char *windowID   = "TEST_WINDOW_ID";
    const char *pipelineID = "TEST_PIPELINE_ID";

    //Act
    bool actual = CameraWindowManager.registerID(windowID, pipelineID);

    //Assert
    EXPECT_EQ(expected, actual);

    //Act
    actual = CameraWindowManager.unregisterID();

    //Assert
    EXPECT_EQ(expected, actual);
}

class LSMCameraWindowManagerTest2 : public ::testing::Test
{
protected:
    void SetUp(void)
    {
        const char *windowID   = "TEST_WINDOW_ID";
        const char *pipelineID = "TEST_PIPELINE_ID";
        CameraWindowManager.registerID(windowID, pipelineID);
    }

    void TearDown(void)
    {
        CameraWindowManager.unregisterID();
    }

    LSM::CameraWindowManager CameraWindowManager;
};

TEST_F(LSMCameraWindowManagerTest2, Punchthrough)
{
    //Arrange
    clearCallLog();
    bool expected = true;

    //Act
    CameraWindowManager.attachPunchThrough();
    bool actual = isAPICalled("Wayland::Importer::attachPunchThrough");

    //Assert
    EXPECT_EQ(expected, actual);

    //Act
    CameraWindowManager.detachPunchThrough();
    actual = isAPICalled("Wayland::Importer::detachPunchThrough");

    //Assert
    EXPECT_EQ(expected, actual);
}

TEST_F(LSMCameraWindowManagerTest2, Surface)
{
    //Arrange
    clearCallLog();
    bool expected = true;

    //Act
    CameraWindowManager.attachSurface();
    bool actual = isAPICalled("Wayland::Importer::attachSurface");

    //Assert
    EXPECT_EQ(expected, actual);

    //Act
    CameraWindowManager.detachSurface();
    actual = isAPICalled("Wayland::Importer::detachSurface");

    //Assert
    EXPECT_EQ(expected, actual);
}

TEST_F(LSMCameraWindowManagerTest2, GetDisplay)
{
    //Arrange

    //Act
    struct wl_display *actual = CameraWindowManager.getDisplay();

    //Assert
    EXPECT_TRUE(nullptr != actual);
}

TEST_F(LSMCameraWindowManagerTest2, GetSurface)
{
    //Arrange

    //Act
    struct wl_surface *actual = CameraWindowManager.getSurface();

    //Assert
    EXPECT_TRUE(nullptr != actual);
}
