// Copyright (c) 2023 LG Electronics, Inc.
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

#ifndef SRC_HAL_UTILS_CAMSHM_H_
#define SRC_HAL_UTILS_CAMSHM_H_

#include <sys/types.h>

typedef enum SHMEM_STATUS_T_
{
    SHMEM_COMM_OK        = 0x0,
    SHMEM_COMM_FAIL      = -1,
    SHMEM_COMM_OVERFLOW  = -2,
    SHMEM_COMM_NODATA    = -3,
    SHMEM_COMM_TERMINATE = -4,
    SHMEM_COMM_SIZE      = -5,
} SHMEM_STATUS_T;

namespace SYSV
{
typedef void *SHMEM_HANDLE;
}

using namespace SYSV;

extern SHMEM_STATUS_T OpenShmem(SHMEM_HANDLE *phShmem, key_t shmemKey);
extern SHMEM_STATUS_T ReadShmem(SHMEM_HANDLE hShmem, unsigned char **ppData, int *pSize,
                                unsigned char **ppMeta, int *pMetaSize);
extern SHMEM_STATUS_T CloseShmem(SHMEM_HANDLE *phShmem);

#endif // SRC_HAL_UTILS_CAMSHM_H_
