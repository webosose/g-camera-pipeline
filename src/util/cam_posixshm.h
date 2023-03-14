// Copyright (c) 2021-2023 LG Electronics, Inc.
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

#ifndef SRC_HAL_UTILS_POCAMSHM_H_
#define SRC_HAL_UTILS_POCAMSHM_H_

typedef enum _POSHMEM_STATUS_T
{
    POSHMEM_COMM_OK        = 0x0,
    POSHMEM_COMM_FAIL      = -1,
    POSHMEM_COMM_OVERFLOW  = -2,
    POSHMEM_COMM_NODATA    = -3,
    POSHMEM_COMM_TERMINATE = -4,
} POSHMEM_STATUS_T;

typedef void * SHMEM_HANDLE;

extern POSHMEM_STATUS_T OpenPosixShmem(SHMEM_HANDLE *phShmem, int fd);
extern POSHMEM_STATUS_T ReadPosixShmem(SHMEM_HANDLE hShmem, unsigned char **ppData, int *pSize,
                                       unsigned char **ppMeta, int *pMetaSize);
extern POSHMEM_STATUS_T ReadPosixShmemEx(SHMEM_HANDLE hShmem, unsigned char **ppData, int *pSize,
                                         unsigned char **ppMeta, int *pMetaSize,
                                         unsigned char **ppExtraData, int *pExtraSize);
extern POSHMEM_STATUS_T ReadPosixLastShmem(SHMEM_HANDLE hShmem, unsigned char **ppData, int *pSize,
                                           unsigned char **ppMeta, int *pMetaSize);
extern POSHMEM_STATUS_T ReadPosixLastShmemEx(SHMEM_HANDLE hShmem, unsigned char **ppData,
                                          unsigned char **ppMeta, int *pMetaSize,
                                          int *pSize, unsigned char **ppExtraData, int *pExtraSize);

#endif //SRC_HAL_UTILS_POCAMSHM_H_
