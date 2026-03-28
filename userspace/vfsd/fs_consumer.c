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

#include "fs_consumer.h"

#include "filesystem.h"

#include <assert.h>
#include <errno.h>
#include <pmos/ipc.h>
#include <pmos/system.h>
#include <stdlib.h>
#include <string.h>

extern pmos_port_t main_port;

struct fs_consumer_map global_fs_consumers = {
    NULL,
    0,
    0,
};

int register_global_fs_consumer(struct fs_consumer *fs_consumer);
void remove_fs_consumer_global(struct fs_consumer *fs_consumer);

struct fs_consumer *create_fs_consumer(uint64_t consumer_task_group)
{
    struct String path              = INIT_STRING;
    struct fs_consumer *fs_consumer = malloc(sizeof(struct fs_consumer));
    if (fs_consumer == NULL) {
        // Could not allocate memory
        return NULL;
    }

    int result = init_null_string(&path);
    if (result != 0)
        // Could not allocate memory
        goto error;

    fs_consumer->requests_head  = NULL;
    fs_consumer->requests_tail  = NULL;
    fs_consumer->requests_count = 0;

    fs_consumer->id = consumer_task_group;

    result = register_global_fs_consumer(fs_consumer);
    if (result != 0)
        goto error;

    syscall_r r =
        set_task_group_notifier_mask(consumer_task_group, main_port, NOTIFICATION_MASK_DESTROYED, 0);
    if (r.result != SUCCESS)
        // Could not set up notifier
        goto error;

    fs_consumer->path = path;
    return fs_consumer;
error:
    remove_fs_consumer_global(fs_consumer);
    free(fs_consumer);
    destroy_string(&path);
    return NULL;
}

void free_buffers_fs_consumer(struct fs_consumer *fs_consumer)
{
    destroy_string(&fs_consumer->path);
}

static int send_create_consumer_reply(pmos_port_t port, uint32_t flags, int32_t result)
{
    IPC_Create_Consumer_Reply reply = {
        .type        = IPC_Create_Consumer_Reply_NUM,
        .flags       = flags,
        .result_code = result,
    };

    return -send_message_port(port, sizeof(IPC_Create_Consumer_Reply), (const char *)&reply);
}

/// Registers a consumer task in the global map
int register_global_fs_consumer(struct fs_consumer *fs_consumer)
{
    // Resize the hash table as needed
    if (global_fs_consumers.count >= global_fs_consumers.size * FS_CONSUMER_MAX_LOAD_FACTOR) {
        // Resize the hash table
        size_t new_size                     = global_fs_consumers.size > 0
                                                  ? global_fs_consumers.size * FS_CONSUMER_SIZE_MULTIPLIER
                                                  : FS_CONSUMER_INITIAL_SIZE;
        struct fs_consumer_node **new_table = calloc(sizeof(struct fs_consumer_node *), new_size);
        if (new_table == NULL) {
            // Could not allocate memory
            return -ENOMEM;
        }

        // Rehash the consumer tasks
        for (size_t i = 0; i < global_fs_consumers.size; i++) {
            struct fs_consumer_node *node = global_fs_consumers.table[i];
            while (node != NULL) {
                struct fs_consumer_node *next = node->next;

                // Rehash the consumer task
                uint64_t new_hash = node->fs_consumer->id % new_size;
                node->next        = new_table[new_hash];
                if (node->next != NULL)
                    node->next->prev = node;
                node->prev          = NULL;
                new_table[new_hash] = node;

                node = next;
            }
        }

        free(global_fs_consumers.table);

        global_fs_consumers.table = new_table;
        global_fs_consumers.size  = new_size;
    }

    // Allocate a new node
    struct fs_consumer_node *node = malloc(sizeof(struct fs_consumer_node));
    if (node == NULL) {
        // Could not allocate memory
        return -ENOMEM;
    }

    uint64_t index = fs_consumer->id % global_fs_consumers.size;

    node->fs_consumer = fs_consumer;
    node->next        = global_fs_consumers.table[index];
    node->prev        = NULL;
    if (node->next != NULL)
        node->next->prev = node;
    global_fs_consumers.table[index] = node;

    global_fs_consumers.count++;
    return 0;
}

void remove_fs_consumer_global(struct fs_consumer *fs_consumer)
{
    if (global_fs_consumers.table == NULL)
        return;

    uint64_t index = fs_consumer->id % global_fs_consumers.size;

    struct fs_consumer_node *node = global_fs_consumers.table[index];
    while (node != NULL && node->fs_consumer != fs_consumer)
        node = node->next;

    if (node == NULL)
        return;

    if (node->prev == NULL)
        global_fs_consumers.table[index] = node->next;
    else
        node->prev->next = node->next;

    if (node->next != NULL)
        node->next->prev = node->prev;

    free(node);

    global_fs_consumers.count--;

    if (global_fs_consumers.count < global_fs_consumers.size * FS_CONSUMER_SHRINK_FACTOR &&
        global_fs_consumers.size > FS_CONSUMER_INITIAL_SIZE) {
        // Shrink the hash table
        size_t new_size                     = global_fs_consumers.size * FS_CONSUMER_SHRINK_FACTOR;
        struct fs_consumer_node **new_table = calloc(sizeof(struct fs_consumer_node *), new_size);
        if (new_table == NULL) {
            // Could not allocate memory
            return;
        }

        // Rehash the consumer tasks
        for (size_t i = 0; i < global_fs_consumers.size; i++) {
            struct fs_consumer_node *node = global_fs_consumers.table[i];
            while (node != NULL) {
                struct fs_consumer_node *next = node->next;

                // Rehash the consumer task
                uint64_t new_hash = node->fs_consumer->id % new_size;
                node->next        = new_table[new_hash];
                if (node->next != NULL)
                    node->next->prev = node;
                node->prev          = NULL;
                new_table[new_hash] = node;

                node = next;
            }
        }

        free(global_fs_consumers.table);

        global_fs_consumers.table = new_table;
        global_fs_consumers.size  = new_size;
    }
}

void destroy_fs_consumer(struct fs_consumer *fs_consumer)
{
    // Remove the fs consumer from the global fs consumers
    remove_fs_consumer_global(fs_consumer);

    // Free the buffers
    free_buffers_fs_consumer(fs_consumer);

    free(fs_consumer);
}

bool is_fs_consumer(struct fs_consumer *fs_consumer, uint64_t id)
{
    syscall_r res = is_task_group_member(id, fs_consumer->id);
    return res.result == SUCCESS && res.value == 1;
}

int create_consumer(const IPC_Create_Consumer *request, uint64_t sender_task_id, size_t)
{
    syscall_r res = is_task_group_member(sender_task_id, request->task_group_id);
    if (res.result != 0 || !res.value) {
        // Task group does not exist
        return send_create_consumer_reply(request->reply_port, request->flags,
                                          res.result != SUCCESS ? -(int)res.result : -EPERM);
    }

    struct fs_consumer *fs_consumer = get_fs_consumer(request->task_group_id);
    if (fs_consumer != NULL) {
        // Consumer already exists
        return send_create_consumer_reply(request->reply_port, request->flags, -EEXIST);
    }

    // Create a new fs_consumer
    fs_consumer = create_fs_consumer(request->task_group_id);
    if (fs_consumer == NULL) {
        // Could not allocate memory
        return send_create_consumer_reply(request->reply_port, request->flags, -ENOMEM);
    }

    // Send reply
    return send_create_consumer_reply(request->reply_port, request->flags, 0);
}

struct fs_consumer *get_fs_consumer(uint64_t consumer_id)
{
    if (global_fs_consumers.table == NULL)
        return NULL;

    uint64_t hash = consumer_id % global_fs_consumers.size;

    struct fs_consumer_node *curr_node = global_fs_consumers.table[hash];
    while (curr_node != NULL && curr_node->fs_consumer->id != consumer_id)
        curr_node = curr_node->next;

    return curr_node == NULL ? NULL : curr_node->fs_consumer;
}