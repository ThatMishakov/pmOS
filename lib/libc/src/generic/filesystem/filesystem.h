/* Copyright (c) 2024, Mikhail Kovalev
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice, this
 *    list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 *
 * 3. Neither the name of the copyright holder nor the names of its
 *    contributors may be used to endorse or promote products derived from
 *    this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef FILESYSTEM_H
#define FILESYSTEM_H
#include <pmos/system.h>
#include <pthread.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

struct File {
    task_group_t filesystem_id;
    uint64_t file_id;
    pmos_port_t fs_port;
};

struct IPC_Queue {
    pmos_port_t port;
    char *name;
};

union File_Data {
    struct File file;
    struct IPC_Queue ipc_queue;
};

struct File_Descriptor {
    bool used;
    uint8_t flags;
    uint8_t type;
    off_t offset;

    const struct Filesystem_Adaptor *adaptor;
    union File_Data data;
};

enum Desctiptor_Type {
    DESCRIPTOR_FILE,
    DESCRIPTOR_LOGGER,
    DESCRIPTOR_IPC_QUEUE,
};

struct Filesystem_Data {
    struct File_Descriptor *descriptors_vector;
    size_t count;
    size_t capacity;
    size_t reserved_count;

    uint64_t fs_consumer_id;

    pthread_spinlock_t lock;
};

/**
 * @brief Creates a clone of the filesystem data for the given task
 *
 * This function creates a clone of the filesystem data for the given task. The new data is stored
 * in the new_data pointer. It should typically be called during the fork() call to create a new
 * filesystem data for the child process.
 *
 * @param new_data Pointer to the new filesystem data
 * @param for_task Task ID to clone the data for
 * @param exclusive If true, no locks would be acquired and it would be assumed that the caller has
 * exclusive access to the data
 * @return int 0 on success, -1 on error. Sets errno on error.
 */
int __clone_fs_data(struct Filesystem_Data **new_data, uint64_t for_task, bool exclusive);

#endif