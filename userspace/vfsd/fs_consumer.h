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

#ifndef FS_CONSUMER_H
#define FS_CONSUMER_H

#include "string.h"

#include <pmos/ipc.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <sys/types.h>

struct Filesystem;

// Bucket for the hash table. fs->id is the key.
struct consumer_fs_map_node {
    struct consumer_fs_map_node *next;

    struct Filesystem *fs;
    size_t open_files_count;
};

struct fs_consumer {
    // ID of the fs consumer
    uint64_t id;

    struct File_Request *requests_head, *requests_tail;
    size_t requests_count;

    struct String path;
};

/// @brief Creates a new FS consumer with the given task group.
///
/// This function creates a new fs consumer with the given task group.
/// It does not check if a fs consumer with the given task group already exists.
/// @param consumer_task_group The task group of the consumer
/// @return The created fs consumer, or NULL on failure.
struct fs_consumer *create_fs_consumer(uint64_t consumer_task_group);

/// @brief Initializes a fs consumer.
/// @param fs_consumer The fs consumer to initialize.
/// @return 0 on success, -1 on failure.
int init_fs_consumer(struct fs_consumer *fs_consumer);

/// @brief Frees the memory buffers of a fs consumer
/// @param fs_consumer The fs_consumer which's buffers to free.
void free_buffers_fs_consumer(struct fs_consumer *fs_consumer);

/// @brief Gets a fs consumer by its ID.
/// @param consumer_id The ID of the fs consumer.
/// @return The fs consumer with the given ID, or NULL if no such fs consumer exists.
struct fs_consumer *get_fs_consumer(uint64_t consumer_id);

/// @brief Checks if a task is a fs consumer.
/// @param consumer THe consumer to check.
/// @param task_id The ID of the task to check.
/// @return True if the task is a fs consumer, false otherwise.
bool is_fs_consumer(struct fs_consumer *fs_consumer, uint64_t id);

struct fs_consumer_node {
    struct fs_consumer_node *prev, *next;
    struct fs_consumer *fs_consumer;
};

extern struct fs_consumer_map {
    // Doubly-linked hash table. fs_consumer->id is the key.
    struct fs_consumer_node **table;
    size_t size;
    size_t count;
#define FS_CONSUMER_INITIAL_SIZE     16
#define FS_CONSUMER_SIZE_MULTIPLIER  2
#define FS_CONSUMER_MAX_LOAD_FACTOR  3 / 4
#define FS_CONSUMER_SHRINK_THRESHOLD 1 / 4
#define FS_CONSUMER_SHRINK_FACTOR    1 / 2
} global_fs_consumers;

/**
 * @brief Create a filesystem consumer
 *
 * @param request Request message
 * @param sender_task_id Sender of the message
 * @param request_size Size of the message
 * @return int 0 on success, negative on failure
 */
int create_consumer(const IPC_Create_Consumer *request, uint64_t sender_task_id,
                    size_t request_size);

#endif // FS_CONSUMER_H