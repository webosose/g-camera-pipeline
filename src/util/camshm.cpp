// Copyright (c) 2019-2023 LG Electronics, Inc.
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

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/sem.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include "camshm.h"
#include "log.h"

//#define SHMEM_COMM_DEBUG

// constants

#define CAMSHKEY 7010

#define SHMEM_HEADER_SIZE (6 * sizeof(int))
#define SHMEM_LENGTH_SIZE sizeof(int)

enum
{
    MODE_OPEN,
    MODE_CREATE
};

enum
{
    READ_FIRST,
    READ_LAST
};

// structure define

union semun
{
    int val;
    struct semid_ds *buf;
    unsigned short *array;
    struct seminfo *__buf;
};

typedef enum _SHMEM_MARK_T
{
    SHMEM_COMM_MARK_NORMAL    = 0x0,
    SHMEM_COMM_MARK_RESET     = 0x1,
    SHMEM_COMM_MARK_TERMINATE = 0x2
} SHMEM_MARK_T;

/* shared memory structure
 4 bytes             : write_index
 4 bytes             : read_index
 4 bytes             : unit_size
 4 bytes             : meta_size
 4 bytes             : unit_num
 4 bytes             : mark
 4 bytes  *unit_num  : length data
 unit_size*unit_num  : data
 4 bytes  *unit_num  : length meta
 meta_size*unit_num  : meta
 4 bytes             : extra_size
 extra_size*unit_num : extra data
 */

typedef struct _SHMEM_COMM_T
{
    int shmem_id;
    int sema_id;

    /*shared memory overhead*/
    int *write_index;
    int *read_index;
    int *unit_size;
    int *meta_size;
    int *unit_num;
    SHMEM_MARK_T *mark;

    unsigned int *length_buf;
    unsigned char *data_buf;

    unsigned int *length_meta;
    unsigned char *data_meta;

    int *extra_size;
    unsigned char *extra_buf;
} SHMEM_COMM_T;

//  <<Shmem shape : frame_count : 8, extra_size : sizeof(int)) >>
//      +---------+---------+----------------
//      |         | 4 bytes | write_index
//      |         +---------+----------------
//      |         | 4 bytes | read_index
//      |HEADER   +---------+----------------
//      |24 bytes | 4 bytes | unit_size
//      |         +---------+----------------
//      |         | 4 bytes | meta_size
//      |         +---------+----------------
//      |         | 4 bytes | unit_num
//      |         +---------+----------------
//      |         | 4 bytes | mark
//      +---------+---------+---------------- (length_buf)
//      |         | 4 bytes | frame_size[0]
//      |         +---------+----------------
//      |LENGTH   | 4 bytes | ...
//      |32 bytes +---------+----------------
//      |         | 4 bytes | frame_size[7]
//      +---------+---------+---------------- (data_buf)
//      |         | x bytes | frame_buf[0]
//      |         +---------+----------------
//      |DATA     | x bytes | ...
//      |x*8 bytes+---------+----------------
//      |         | x bytes | frame_buf[7]
//      +---------+---------+---------------- (length_meta)
//      |         | 4 bytes | meta_size[0]
//      |         +---------+----------------
//      |LENGTH   | 4 bytes | ...
//      |32 bytes +---------+----------------
//      |         | 4 bytes | meta_size[7]
//      +---------+---------+---------------- (data_meta)
//      |         | y bytes | meta_buf[0]
//      |         +---------+----------------
//      |META     | y bytes | ...
//      |y*8 bytes+---------+----------------
//      |         | y bytes | meta_buf[7]
//      +---------+---------+----------------
//      |EXTRA SZ | 4 bytes | extra_size
//      +---------+---------+---------------- (extra_buf)
//      |         | 4 bytes | extra_buf[0]
//      |         +---------+----------------
//      |EXTRA BUF| 4 bytes | ...
//      |4*8 bytes+---------+----------------
//      |         | 4 bytes | extra_buf[7]
//      +---------+---------+----------------
//
// TOTAL = HEADER(24) +
//         LENGTH(sizeof(int) * unit_num) + DATA(unit_size * unit_num) +
//         LENGTH(sizeof(int) * unit_num) + DATA(meta_size * unit_num) +
//         EXTRA_SZ(sizeof(int)) + EXTRA_BUF(extra_size * unit_num))

SHMEM_STATUS_T _OpenShmem(SHMEM_HANDLE *phShmem, key_t *pShmemKey, int unitSize, int metaSize,
                          int unitNum, int extraSize, int nOpenMode);
SHMEM_STATUS_T _ReadShmem(SHMEM_HANDLE hShmem, unsigned char **ppData, int *pSize,
                          unsigned char **ppMeta, int *pMetaSize, unsigned char **ppExtraData,
                          int *pExtraSize, int readMode);

// Internal Functions

/*shared memory protocol*/
static int lockShmem(SHMEM_COMM_T *shmem_buffer)
{
    struct sembuf sema_buffer;

    if (shmem_buffer == NULL)
    {
        CMP_LOG_ERROR("Invalid argument");
        return -1;
    }

    sema_buffer.sem_num = 0;
    sema_buffer.sem_op = -1;
    sema_buffer.sem_flg = 0;

    return semop(shmem_buffer->sema_id, &sema_buffer, 1);
}

static int unlockShmem(SHMEM_COMM_T *shmem_buffer)
{
    struct sembuf sema_buffer;

    if (shmem_buffer == NULL)
    {
        CMP_LOG_ERROR("Invalid argument");
        return -1;
    }

    sema_buffer.sem_num = 0;
    sema_buffer.sem_op = 1;
    sema_buffer.sem_flg = 0;

    return semop(shmem_buffer->sema_id, &sema_buffer, 1);
}

static int resetShmem(SHMEM_COMM_T *shmem_buffer)
{
    union semun se;

    if (shmem_buffer == NULL)
    {
        CMP_LOG_ERROR("Invalid argument");
        return -1;
    }

    se.val = 0;
    return semctl(shmem_buffer->sema_id, 0, SETVAL, se);
}

// API functions

extern SHMEM_STATUS_T OpenShmem(SHMEM_HANDLE *phShmem, key_t shmemKey)
{
    return _OpenShmem(phShmem, &shmemKey, 0, 0, 0, 0, MODE_OPEN);
}

SHMEM_STATUS_T _OpenShmem(SHMEM_HANDLE *phShmem, key_t *pShmemKey, int unitSize, int metaSize,
                          int unitNum, int extraSize, int nOpenMode)
{
    SHMEM_COMM_T *pShmemBuffer;
    unsigned char *pSharedmem;
    key_t shmemKey;
    int shmemSize = 0;
    int shmemMode = 0666;
    struct shmid_ds shm_stat;

    *phShmem = (SHMEM_HANDLE) calloc(1, sizeof(SHMEM_COMM_T));
    pShmemBuffer = (SHMEM_COMM_T *) *phShmem;
    if (pShmemBuffer == NULL) {
        CMP_LOG_INFO("pShmemBuffer is null");
        return SHMEM_COMM_FAIL;
    }

    CMP_LOG_INFO("hShmem = %p, pKey = %p, nOpenMode=%d, unitSize=%d, unitNum=%d",
            *phShmem, pShmemKey, nOpenMode, unitSize, unitNum);

    if (nOpenMode == MODE_CREATE)
    {
        for (shmemKey = CAMSHKEY; shmemKey < 0xFFFF; shmemKey++)
        {
            pShmemBuffer->shmem_id = shmget((key_t) shmemKey, 0, 0666);
            if (pShmemBuffer->shmem_id == -1 && errno == ENOENT)
                break;
        }
        *pShmemKey = shmemKey;
        shmemSize = SHMEM_HEADER_SIZE + (unitSize + SHMEM_LENGTH_SIZE) * unitNum + sizeof(int)
                + extraSize * unitNum;
        shmemMode |= IPC_CREAT | IPC_EXCL;
    }
    else
    {
        shmemKey = *pShmemKey;
    }

    CMP_LOG_INFO("shmem_key=%d\r", shmemKey);

    pShmemBuffer->shmem_id = shmget((key_t) shmemKey, shmemSize, shmemMode);
    if (pShmemBuffer->shmem_id == -1)
    {
        CMP_LOG_ERROR("Can't open shared memory: %s", strerror(errno));
        free(pShmemBuffer);
        return SHMEM_COMM_FAIL;
    }

    CMP_LOG_INFO("shared memory created/opened successfully!");

    pSharedmem                = (unsigned char *) shmat(pShmemBuffer->shmem_id, NULL, 0);
    pShmemBuffer->write_index = (int *) (pSharedmem + sizeof(int) * 0);
    pShmemBuffer->read_index  = (int *) (pSharedmem + sizeof(int) * 1);
    pShmemBuffer->unit_size   = (int *) (pSharedmem + sizeof(int) * 2);
    pShmemBuffer->meta_size   = (int *) (pSharedmem + sizeof(int) * 3);
    pShmemBuffer->unit_num    = (int *) (pSharedmem + sizeof(int) * 4);
    pShmemBuffer->mark        = (SHMEM_MARK_T *) (pSharedmem + sizeof(int) * 5);

    if (nOpenMode == MODE_OPEN || (pShmemBuffer->sema_id = semget(shmemKey, 1, shmemMode)) == -1)
    {
#ifdef SHMEM_COMM_DEBUG
        if (nOpenMode == MODE_CREATE)
        CMP_LOG_ERROR("Failed to create semaphore : %s", strerror(errno));
#endif
        if ((pShmemBuffer->sema_id = semget((key_t) shmemKey, 1, 0666)) == -1)
        {
            CMP_LOG_ERROR("Failed to get semaphore : %s", strerror(errno));
            free(pShmemBuffer);
            return SHMEM_COMM_FAIL;
        }
    }

    if (nOpenMode == MODE_CREATE)
    {
        *pShmemBuffer->unit_size = unitSize;
        *pShmemBuffer->meta_size = metaSize;
        *pShmemBuffer->unit_num  = unitNum;
    }
    size_t length_buf_offset = sizeof(int) * 6;

    size_t data_buf_offset = SHMEM_HEADER_SIZE + (SHMEM_LENGTH_SIZE) * (*pShmemBuffer->unit_num);

    size_t length_meta_offset =
        SHMEM_HEADER_SIZE +
        ((*pShmemBuffer->unit_size) + SHMEM_LENGTH_SIZE) * (*pShmemBuffer->unit_num);

    size_t data_meta_offset =
        SHMEM_HEADER_SIZE +
        ((*pShmemBuffer->unit_size) + SHMEM_LENGTH_SIZE) * (*pShmemBuffer->unit_num) +
        (SHMEM_LENGTH_SIZE) * (*pShmemBuffer->unit_num);

    size_t extra_size_offset =
        SHMEM_HEADER_SIZE +
        ((*pShmemBuffer->unit_size) + SHMEM_LENGTH_SIZE) * (*pShmemBuffer->unit_num) +
        ((*pShmemBuffer->meta_size) + SHMEM_LENGTH_SIZE) * (*pShmemBuffer->unit_num);

    size_t extra_buf_offset =
        SHMEM_HEADER_SIZE +
        ((*pShmemBuffer->unit_size) + SHMEM_LENGTH_SIZE) * (*pShmemBuffer->unit_num) +
        ((*pShmemBuffer->meta_size) + SHMEM_LENGTH_SIZE) * (*pShmemBuffer->unit_num) + sizeof(int);

    pShmemBuffer->length_buf = (unsigned int *)(pSharedmem + length_buf_offset);

    pShmemBuffer->data_buf = pSharedmem + data_buf_offset;

    pShmemBuffer->length_meta = (unsigned int *)(pSharedmem + length_meta_offset);

    pShmemBuffer->data_meta = pSharedmem + data_meta_offset;

    pShmemBuffer->extra_size = NULL;
    pShmemBuffer->extra_buf  = NULL;

    if (shmctl(pShmemBuffer->shmem_id, IPC_STAT, &shm_stat) != -1)
    {
#ifdef SHMEM_COMM_DEBUG
        CMP_LOG_INFO("shm_stat.shm_nattch=%d", (int)shm_stat.shm_nattch);
        if (shm_stat.shm_nattch == 1)
            CMP_LOG_INFO("we are the first client");

        CMP_LOG_INFO("shared memory size = %d", shm_stat.shm_segsz);
#endif
        // shared momory size larger than total, we use extra data
        if (shm_stat.shm_segsz > extra_size_offset)
        {
            pShmemBuffer->extra_size = (int *)(pSharedmem + extra_size_offset);
            pShmemBuffer->extra_buf  = pSharedmem + extra_buf_offset;
        }
        else
        {
            pShmemBuffer->extra_size = NULL;
            pShmemBuffer->extra_buf  = NULL;
        }
    }

    if (nOpenMode == MODE_CREATE && pShmemBuffer->extra_size != NULL)
    {
        *pShmemBuffer->extra_size = extraSize;
    }

    *pShmemBuffer->mark = SHMEM_COMM_MARK_NORMAL;
    //Until the writter starts to write both write index and read index are
    //set to -1 . So the reader can get to know that the writter has not
    //started to write yet
    *pShmemBuffer->write_index = -1;
    *pShmemBuffer->read_index  = -1;

    resetShmem(pShmemBuffer);

    CMP_LOG_INFO("unitSize = %d, SHMEM_LENGTH_SIZE = %d, unit_num = %d", *pShmemBuffer->unit_size,
                SHMEM_LENGTH_SIZE, *pShmemBuffer->unit_num);
    CMP_LOG_INFO("shared memory opened successfully! : shmem_id=%d, sema_id=%d",
            pShmemBuffer->shmem_id, pShmemBuffer->sema_id);
    return SHMEM_COMM_OK;
}

SHMEM_STATUS_T ReadShmem(SHMEM_HANDLE hShmem, unsigned char **ppData, int *pSize,
                         unsigned char **ppMeta, int *pMetaSize)
{
    return _ReadShmem(hShmem, ppData, pSize, ppMeta, pMetaSize, NULL, NULL, READ_FIRST);
}

SHMEM_STATUS_T _ReadShmem(SHMEM_HANDLE hShmem, unsigned char **ppData, int *pSize,
                          unsigned char **ppMeta, int *pMetaSize, unsigned char **ppExtraData,
                          int *pExtraSize, int readMode)
{
    SHMEM_COMM_T *shmem_buffer = (SHMEM_COMM_T *) hShmem;
    int lread_index;
    unsigned char *read_addr;
    int size;
    static bool first_read;

    first_read = false;
    if (!shmem_buffer)
    {
        CMP_LOG_ERROR("shmem buffer is NULL");
        return SHMEM_COMM_FAIL;
    }
    lread_index = *shmem_buffer->write_index;

    do
    {
#ifdef SHMEM_COMM_DEBUG
        int sem_count;
        sem_count = semctl(shmem_buffer->sema_id, 0, GETVAL, 0);
        CMP_LOG_INFO("sem_count=%d", sem_count);
#endif
        if (-1 != *shmem_buffer->write_index)
        {
            if (*shmem_buffer->write_index == 0)
            {
                if (0 == first_read)
                {
                    first_read = 1;
                    continue;
                }
                else
                {
                    lread_index = *shmem_buffer->unit_num - 1;
                }
            }
            else
            {
                lread_index = *shmem_buffer->write_index - 1;
            }
            size = *(int*) (shmem_buffer->length_buf + lread_index);

            if ((size == 0) || (size > *shmem_buffer->unit_size))
            {
                CMP_LOG_ERROR("size error(%d)!", size);
                return SHMEM_COMM_SIZE;
            }

            read_addr = shmem_buffer->data_buf + (lread_index) * (*shmem_buffer->unit_size);
            *ppData   = read_addr;
            *pSize    = size;

            size       = *(int *)(shmem_buffer->length_meta + lread_index);
            read_addr  = shmem_buffer->data_meta + (lread_index) * (*shmem_buffer->meta_size);
            *ppMeta    = read_addr;
            *pMetaSize = size;

            if (NULL != ppExtraData && NULL != pExtraSize)
            {
                *ppExtraData = shmem_buffer->extra_buf
                        + (lread_index) * (*shmem_buffer->extra_size);
                *pExtraSize = *shmem_buffer->extra_size;
            }
        }

        break;
    } while (1);

    return SHMEM_COMM_OK;
}

SHMEM_STATUS_T CloseShmem(SHMEM_HANDLE *phShmem)
{
    void *shmem_addr;
    struct shmid_ds shm_stat;
    SHMEM_COMM_T *shmem_buffer;
    CMP_LOG_INFO("start");

    shmem_buffer = (SHMEM_COMM_T *) *phShmem;

    if (!shmem_buffer)
    {
        CMP_LOG_ERROR("shmem_bufer is NULL");
        return SHMEM_COMM_FAIL;
    }

    shmem_addr = shmem_buffer->write_index;
    shmdt(shmem_addr);

    resetShmem(shmem_buffer);
    unlockShmem(shmem_buffer);

    if (shmctl(shmem_buffer->shmem_id, IPC_STAT, &shm_stat) != -1)
    {
        CMP_LOG_INFO("shm_stat.shm_nattch=%d", (int)shm_stat.shm_nattch);

        if (shm_stat.shm_nattch == 0)
        {
            CMP_LOG_INFO("This is the only attached client");
            semctl(shmem_buffer->sema_id, 0, IPC_RMID, NULL);
            shmctl(shmem_buffer->shmem_id, IPC_RMID, NULL);
        }
    }

    free(shmem_buffer);
    shmem_buffer = NULL;
    CMP_LOG_INFO("end");
    return SHMEM_COMM_OK;
}
