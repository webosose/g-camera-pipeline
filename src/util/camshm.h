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
typedef enum _SHMEM_STATUS_T
{
    SHMEM_COMM_OK        = 0x0,
    SHMEM_COMM_FAIL      = -1,
    SHMEM_COMM_OVERFLOW  = -2,
    SHMEM_COMM_NODATA    = -3,
    SHMEM_COMM_TERMINATE = -4,
    SHMEM_COMM_SIZE      = -5,
} SHMEM_STATUS_T;

typedef void * SHMEM_HANDLE;

extern SHMEM_STATUS_T CreateShmem(SHMEM_HANDLE *phShmem, key_t *pShmemKey, int unitSize,
                                  int metaSize, int unitNum);
extern SHMEM_STATUS_T CreateShmemEx(SHMEM_HANDLE *phShmem, key_t *pShmemKey, int unitSize,
                                    int metaSize, int unitNum, int extraSize);
extern SHMEM_STATUS_T OpenShmem(SHMEM_HANDLE *phShmem, key_t shmemKey);
extern SHMEM_STATUS_T ReadShmem(SHMEM_HANDLE hShmem, unsigned char **ppData, int *pSize,
                                unsigned char **ppMeta, int *pMetaSize);
extern SHMEM_STATUS_T ReadShmemEx(SHMEM_HANDLE hShmem, unsigned char **ppData, int *pSize,
                                  unsigned char **ppMeta, int *pMetaSize,
                                  unsigned char **ppExtraData, int *pExtraSize);
extern SHMEM_STATUS_T ReadLastShmem(SHMEM_HANDLE hShmem, unsigned char **ppData, int *pSize,
                                    unsigned char **ppMeta, int *pMetaSize);
extern SHMEM_STATUS_T ReadLastShmemEx(SHMEM_HANDLE hShmem, unsigned char **ppData, int *pSize,
                                      unsigned char **ppMeta, int *pMetaSize,
                                      unsigned char **ppExtraData, int *pExtraSize);
extern SHMEM_STATUS_T WriteShmem(SHMEM_HANDLE hShmem, unsigned char *pData, int dataSize,
                                 unsigned char *pMeta, int metaSize);
extern SHMEM_STATUS_T WriteShmemEx(SHMEM_HANDLE hShmem, unsigned char *pData, int dataSize,
                                   unsigned char *pMeta, int metaSize, unsigned char *pExtraData,
                                   int extraDataSize);
extern SHMEM_STATUS_T CloseShmem(SHMEM_HANDLE *phShmem);

#endif //SRC_HAL_UTILS_CAMSHM_H_
