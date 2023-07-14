#include "fs_consumer.h"
#include "filesystem.h"
#include <stdlib.h>

int init_fs_consumer(struct fs_consumer *fs_consumer)
{
    static uint64_t next_consumer_id = 1;

    // Assign a unique ID to the consumer
    fs_consumer->id = __atomic_fetch_add(&next_consumer_id, 1, __ATOMIC_SEQ_CST);

    // Allocate consumer tasks set
    fs_consumer->consumer_tasks = malloc(sizeof(struct consumer_task *) * CONSUMER_TASKS_INITIAL_CAPACITY);
    if (fs_consumer->consumer_tasks == NULL) {
        // Could not allocate memory
        return -1;
    }

    fs_consumer->consumer_tasks_capacity = CONSUMER_TASKS_INITIAL_CAPACITY;
    fs_consumer->consumer_tasks_count = 0;

    // Initialize the list of filesystems with open files
    fs_consumer->open_filesystem = malloc(sizeof(struct open_filesystem *) * OPEN_FILESYSTEM_INITIAL_SIZE);
    if (fs_consumer->open_filesystem == NULL) {
        // Could not allocate memory
        free(fs_consumer->consumer_tasks);
        return -1;
    }

    int result = init_null_string(&fs_consumer->path);
    if (result != 0) {
        // Could not allocate memory
        free(fs_consumer->consumer_tasks);
        return result;
    }

    fs_consumer->open_filesystem_size = OPEN_FILESYSTEM_INITIAL_SIZE;
    fs_consumer->open_filesystem_count = 0;

    fs_consumer->requests_head = NULL;
    fs_consumer->requests_tail = NULL;
    fs_consumer->requests_count = 0;

    return 0;
}

void free_buffers_fs_consumer(struct fs_consumer *fs_consumer)
{
    if (fs_consumer->consumer_tasks != NULL)
        free(fs_consumer->consumer_tasks);

    if (fs_consumer->open_filesystem != NULL)
        free(fs_consumer->open_filesystem);

    destroy_string(&fs_consumer->path);
}

int register_consumer_task(struct consumer_task *task, struct fs_consumer *fs_consumer)
{
    if (task == NULL || fs_consumer == NULL)
        return -1;

    // Allocate a new linked list node
    struct fs_consumer_node *node = malloc(sizeof(struct fs_consumer_node));
    if (node == NULL) {
        // Could not allocate memory
        return -1;
    }

    if (fs_consumer->consumer_tasks_count == fs_consumer->consumer_tasks_capacity) {
        // Resize the array
        size_t new_capacity = fs_consumer->consumer_tasks_capacity * CONSUMER_TASKS_CAPACITY_MULTIPLIER;
        struct consumer_task **new_consumer_tasks = realloc(fs_consumer->consumer_tasks, sizeof(struct consumer_task *) * new_capacity);
        if (new_consumer_tasks == NULL) {
            // Could not allocate memory
            free(node);
            return -1;
        }

        fs_consumer->consumer_tasks = new_consumer_tasks;
        fs_consumer->consumer_tasks_capacity = new_capacity;
    }

    // Add the task to the array
    fs_consumer->consumer_tasks[fs_consumer->consumer_tasks_count++] = task;

    // Add the node to the linked list
    node->prev = task->consumers_last;
    node->next = NULL;
    task->consumers_last = node;

    return 0;
}

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

bool is_fs_consumer(struct fs_consumer *fs_consumer, uint64_t id)
{
    if (fs_consumer == NULL)
        return false;

    // Binary search consumer tasks
    size_t left = 0;
    size_t right = fs_consumer->consumer_tasks_count;
    while (left < right) {
        size_t mid = (left + right) / 2;
        if (fs_consumer->consumer_tasks[mid]->task_id < id)
            left = mid + 1;
        else
            right = mid;
    }

    return left < fs_consumer->consumer_tasks_count && fs_consumer->consumer_tasks[left]->task_id == id;
}

struct fs_consumer_map global_fs_consumers = {
    NULL,
    0,
    0,
};

__attribute__((constructor)) void init_global_fs_consumers()
{
    const uint64_t initial_size = FS_CONSUMER_INITIAL_SIZE;

    global_fs_consumers.table = calloc(sizeof(struct fs_consumer_node *), initial_size);
    if (global_fs_consumers.table == NULL) {
        // Could not allocate memory
        exit(1);
    }

    global_fs_consumers.size = initial_size;
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