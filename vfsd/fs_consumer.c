#include "fs_consumer.h"
#include "filesystem.h"
#include <stdlib.h>
#include <string.h>
#include <pmos/system.h>
#include <pmos/ipc.h>
#include <errno.h>
#include <assert.h>

extern pmos_port_t main_port;

struct fs_consumer_map global_fs_consumers = {
    NULL,
    0,
    0,
};

int register_global_fs_consumer(struct fs_consumer *fs_consumer);
void remove_fs_consumer_global(struct fs_consumer *fs_consumer);

struct fs_consumer * create_fs_consumer(uint64_t consumer_task_group)
{
    struct fs_consumer *fs_consumer = malloc(sizeof(struct fs_consumer));
    if (fs_consumer == NULL) {
        // Could not allocate memory
        return NULL;
    }

    // Initialize the list of filesystems with open files
    fs_consumer->open_filesystem = calloc(sizeof(struct open_filesystem *), OPEN_FILESYSTEM_INITIAL_SIZE);
    if (fs_consumer->open_filesystem == NULL) {
        // Could not allocate memory
        free(fs_consumer);
        return NULL;
    }

    int result = init_null_string(&fs_consumer->path);
    if (result != 0) {
        // Could not allocate memory
        free(fs_consumer->open_filesystem);
        free(fs_consumer);
        return NULL;
    }

    fs_consumer->open_filesystem_size = OPEN_FILESYSTEM_INITIAL_SIZE;
    fs_consumer->open_filesystem_count = 0;

    fs_consumer->requests_head = NULL;
    fs_consumer->requests_tail = NULL;
    fs_consumer->requests_count = 0;

    fs_consumer->id = consumer_task_group;

    result = register_global_fs_consumer(fs_consumer);
    if (result != 0) {
        // Could not register the consumer
        free(fs_consumer->open_filesystem);
        destroy_string(&fs_consumer->path);
        free(fs_consumer);
        return NULL;
    }

    syscall_r r = set_task_group_notifier_mask(consumer_task_group, main_port, NOTIFICATION_MASK_DESTROYED);
    if (r.result != SUCCESS) {
        // Could not set up notifier
        remove_fs_consumer_global(fs_consumer);
        free(fs_consumer->open_filesystem);
        destroy_string(&fs_consumer->path);
        free(fs_consumer);
        return NULL;
    }

    return fs_consumer;
}

void free_buffers_fs_consumer(struct fs_consumer *fs_consumer)
{
    if (fs_consumer->open_filesystem != NULL)
        free(fs_consumer->open_filesystem);

    destroy_string(&fs_consumer->path);
}

// This code is problematic because of potential divisions by zero
int reference_open_filesystem(struct fs_consumer *fs_consumer, struct Filesystem *fs, uint64_t open_count)
{
    if (fs_consumer == NULL || fs == NULL)
        return -1;

    // Find the filesystem in the open map
    size_t index = fs->id % fs_consumer->open_filesystem_size;
    struct consumer_fs_map_node *node = fs_consumer->open_filesystem[index];
    while (node != NULL && node->fs != fs)
        node = node->next;

    if (node != NULL) {
        // Found the filesystem, increment the open count
        node->open_files_count += open_count;
        return 0;
    }

    // The filesystem was not found, add it to the map

    // Resize the hash table as needed
    if (fs_consumer->open_filesystem_count > fs_consumer->open_filesystem_size * OPEN_FILESYSTEM_MAX_LOAD_FACTOR) {
        // Resize the hash table
        size_t new_size = fs_consumer->open_filesystem_size * OPEN_FILESYSTEM_SIZE_MULTIPLIER;
        struct consumer_fs_map_node **new_open_filesystem = malloc(sizeof(struct consumer_fs_map_node *) * new_size);
        if (new_open_filesystem == NULL) {
            // Could not allocate memory
            return -1;
        }

        // Rehash the filesystems
        for (size_t i = 0; i < fs_consumer->open_filesystem_size; i++) {
            node = fs_consumer->open_filesystem[i];
            while (node != NULL) {
                struct consumer_fs_map_node *next = node->next;

                // Rehash the filesystem
                size_t new_index = node->fs->id % new_size;
                node->next = new_open_filesystem[new_index];
                new_open_filesystem[new_index] = node;

                node = next;
            }
        }

        free(fs_consumer->open_filesystem);

        fs_consumer->open_filesystem = new_open_filesystem;
        fs_consumer->open_filesystem_size = new_size;

        index = fs->id % fs_consumer->open_filesystem_size;
    }

    // Allocate a new node
    node = malloc(sizeof(struct consumer_fs_map_node));
    if (node == NULL) {
        // Could not allocate memory
        return -1;
    }

    node->fs = fs;
    node->open_files_count = open_count;
    
    // Add the node to the hash table
    node->next = fs_consumer->open_filesystem[index];
    fs_consumer->open_filesystem[index] = node;
    fs_consumer->open_filesystem_count++;

    return 0;
}

void unreference_open_filesystem(struct fs_consumer *fs_consumer, struct Filesystem *fs, uint64_t close_count)
{
    assert(fs_consumer != NULL && fs != NULL);
    assert(close_count > 0);

    if (fs_consumer->open_filesystem_size == 0)
        return;

    size_t index = fs->id % fs_consumer->open_filesystem_size;

    struct consumer_fs_map_node *node = fs_consumer->open_filesystem[index], *prev = NULL;
    while (node != NULL && node->fs != fs) {
        prev = node;
        node = node->next;
    }

    if (node == NULL)
        return;

    if (node->open_files_count < close_count)
        close_count = node->open_files_count;

    node->open_files_count -= close_count;

    if (node->open_files_count != 0)
        return;
        
    prev == NULL ? (fs_consumer->open_filesystem[index] = node->next) : (prev->next = node->next);
    fs_consumer->open_filesystem_count--;
    free(node);

    if (fs_consumer->open_filesystem_count < fs_consumer->open_filesystem_size * OPEN_FILESYSTEM_SHRINK_FACTOR) {
        size_t new_size = fs_consumer->open_filesystem_size / OPEN_FILESYSTEM_SIZE_MULTIPLIER;
        struct consumer_fs_map_node **new_open_filesystem = calloc(new_size, sizeof(struct consumer_fs_map_node *));

        if (new_open_filesystem == NULL)
            return;

        // Rehash the filesystems
        for (size_t i = 0; i < fs_consumer->open_filesystem_size; i++) {
            node = fs_consumer->open_filesystem[i];
            while (node != NULL) {
                struct consumer_fs_map_node *next = node->next;

                // Rehash the filesystem
                size_t new_index = node->fs->id % new_size;
                node->next = new_open_filesystem[new_index];
                new_open_filesystem[new_index] = node;

                node = next;
            }
        }

        free(fs_consumer->open_filesystem);

        fs_consumer->open_filesystem = new_open_filesystem;
        fs_consumer->open_filesystem_size = new_size;

        index = fs->id % fs_consumer->open_filesystem_size;
    }
}

static int send_create_consumer_reply(pmos_port_t port, uint32_t flags, int32_t result)
{
    IPC_Create_Consumer_Reply reply = {
        .type = IPC_Create_Consumer_Reply_NUM,
        .flags = flags,
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
        size_t new_size = global_fs_consumers.size > 0 ? global_fs_consumers.size * FS_CONSUMER_SIZE_MULTIPLIER : FS_CONSUMER_INITIAL_SIZE;
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
                node->next = new_table[new_hash];
                if (node->next != NULL)
                    node->next->prev = node;
                node->prev = NULL;
                new_table[new_hash] = node;

                node = next;
            }
        }

        free(global_fs_consumers.table);

        global_fs_consumers.table = new_table;
        global_fs_consumers.size = new_size;
    }

    // Allocate a new node
    struct fs_consumer_node *node = malloc(sizeof(struct fs_consumer_node));
    if (node == NULL) {
        // Could not allocate memory
        return -ENOMEM;
    }

    uint64_t index = fs_consumer->id % global_fs_consumers.size;

    node->fs_consumer = fs_consumer;
    node->next = global_fs_consumers.table[index];
    node->prev = NULL;
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

    if (global_fs_consumers.count < global_fs_consumers.size * FS_CONSUMER_SHRINK_FACTOR && global_fs_consumers.size > FS_CONSUMER_INITIAL_SIZE) {
        // Shrink the hash table
        size_t new_size = global_fs_consumers.size * FS_CONSUMER_SHRINK_FACTOR;
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
                node->next = new_table[new_hash];
                if (node->next != NULL)
                    node->next->prev = node;
                node->prev = NULL;
                new_table[new_hash] = node;

                node = next;
            }
        }

        free(global_fs_consumers.table);

        global_fs_consumers.table = new_table;
        global_fs_consumers.size = new_size;
    }
}

void destroy_fs_consumer(struct fs_consumer *fs_consumer)
{
    // Close all open files
    for (size_t i = 0; i < fs_consumer->open_filesystem_size; i++) {
        struct consumer_fs_map_node* node = fs_consumer->open_filesystem[i];
        while (node != NULL) {
            struct consumer_fs_map_node* next = node->next;
            remove_consumer_from_filesystem(node->fs, node->open_files_count, fs_consumer);
            free(node);
            node = next;
        }
    }
    free(fs_consumer->open_filesystem);

    // Remove the fs consumer from the global fs consumers
    remove_fs_consumer_global(fs_consumer);

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
        return send_create_consumer_reply(request->reply_port, request->flags, res.result != SUCCESS ? -(int)res.result : -EPERM);
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

void remove_consumer_from_filesystem(struct Filesystem *fs, uint64_t open_files_count, struct fs_consumer *consumer)
{
    // Not yet implemented
    return;
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