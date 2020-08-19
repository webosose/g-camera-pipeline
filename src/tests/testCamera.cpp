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

#include <iostream>
#include <vector>
#include "string.h"

int main(int argc, char const *argv[])
{
    FILE *fp;
    char path[1035];
    int option = 1;
    std::string str,mediaId, command;

    std::string load = "luna-send -n 1 luna://com.webos.media/load '{\"uri\":\"camera://com.webos.service.camera2/7010\",\"payload\" : { \"width\" : 640,\"height\":480,\"format\":\"JPEG\",\"framerate\":30, \"memtype\":\"device\",\"memsrc\":\"/dev/video0\"} ,\"type\":\"camera\"}'";
    std::string play = "luna-send -n 1 luna://com.webos.media/play \'{";
    std::string Unload = "luna-send -n 1 luna://com.webos.media/unload \'{";
    std::string takeSnapshot = "luna-send -n 1 luna://com.webos.media/takeCameraSnapshot \'{";
    std::string startRecord = "luna-send -n 1 luna://com.webos.media/startCameraRecord \'{";
    std::string stopRecord = "luna-send -n 1 luna://com.webos.media/stopCameraRecord \'{";

    while(option !=7)
    {
        printf("Enter the option\n");
        printf("1: Load\n2:Play\n3:UnLoad\n4:StartRecord\n5:StopRecord\n6:CaptureImage\n7:Exit");
        scanf("%d",&option);
        printf("Main:%d\n",__LINE__);
        switch(option)
        {
            case 1:
                {
                    /* Open the command for reading. */
                    fp = popen(load.c_str(), "r");
                    if (fp == NULL) {
                        printf("Failed to run command\n" );
                        exit(1);
                    }
                    /* Read the output a line at a time - output it. */
                    while (fgets(path, sizeof(path)-1, fp) != NULL) {
                        str = str + path;
                    }
                    str =  str.substr(str.find_last_of(",") + 1);
                    str.erase(str.find("}"));
                    std::cout<< "str: " << str << std::endl;
                    /* close */
                    pclose(fp);
                    break;
                }
            case 2:
                {
                    command = play + str + "}'";
                    system(command.c_str());
                    command.clear();
                    break;
                }
            case 3:
                {
                    command = Unload + str + "}'";
                    system(command.c_str());
                    str.clear();
                    command.clear();
                    break;
                }
            case 4:
                {
                    command = startRecord + str + ",\"location\":\"/media/internal/\",\"format\":\"h264\"}'";
                    system(command.c_str());
                    command.clear();
                    break;
                }
            case 5:
                {
                    stopRecord = stopRecord + str + "}'";
                    std::cout<< "string stop record: " << stopRecord << std::endl;
                    system(stopRecord.c_str());
                    command.clear();
                    break;
                }
            case 6:
                {
                    command = takeSnapshot + str + ",\"location\":\"/media/internal/\",\"format\" :\"jpg\", \"width\" : 640,\"height\":480,\"pictureQuality\":30}'";
                    system(command.c_str());
                    command.clear();
                    break;
                }
            case 7:
                {
                    return 0;
                }
        }
        continue;
    }

    return 0;
}
