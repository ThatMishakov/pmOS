#ifndef FS_CONSUMER_H
#define FS_CONSUMER_H

#include <stddef.h>
#include <stdint.h>
#include <sys/types.h>
#include <stdbool.h>
#include "string.h"

struct Filesystem;

// Bucket for the hash table. fs->id is the key.
struct consumer_fs_map_node {
    struct consumer_fs_map_node *next;

    struct Filesystem *fs;
    size_t open_files_count;
};

struct consumer_task;
struct fs_consumer {
    // ID of the fs consumer
    uint64_t id;

    // Memory consumption is more important than the lookup speed
    // so use a flat set
    struct consumer_task **consumer_tasks;
    size_t consumer_tasks_count;
    size_t consumer_tasks_capacity;
    #define CONSUMER_TASKS_INITIAL_CAPACITY 4
    #define CONSUMER_TASKS_CAPACITY_MULTIPLIER 2

    // Map of filesystems containing filesystem with some open files
    // Hash table
    struct consumer_fs_map_node **open_filesystem;
    size_t open_filesystem_size;
    size_t open_filesystem_count;
    #define OPEN_FILESYSTEM_INITIAL_SIZE 16
    #define OPEN_FILESYSTEM_SIZE_MULTIPLIER 2
    #define OPEN_FILESYSTEM_MAX_LOAD_FACTOR 3/4

    struct File_Request *requests_head, *requests_tail;
    size_t requests_count;

    struct String path;
};

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

struct fs_consumer_node {
    struct fs_consumer_node *prev, *next;
    struct fs_consumer *fs_consumer;
};

struct consumer_task {
    uint64_t task_id;

    // Linked list fs_consumers
    struct fs_consumer_node *consumers_first, *consumers_last;
};

struct fs_consumer_map {
    // Doubly-linked hash table. fs_consumer->id is the key.
    struct fs_consumer_node **table;
    size_t size;
    size_t count;
    #define FS_CONSUMER_INITIAL_SIZE 16
    #define FS_CONSUMER_SIZE_MULTIPLIER 2
    #define FS_CONSUMER_MAX_LOAD_FACTOR 3/4
    #define FS_CONSUMER_SHRINK_THRESHOLD 1/4
    #define FS_CONSUMER_SHRINK_FACTOR 1/2
};

extern struct fs_consumer_map global_fs_consumers;

/**
 * @brief Registers a consumer task with the fs consumer.
 * 
 * @param task The consumer task to register.
 * @param fs_consumer The fs consumer to register the task with.
 * @return int 0 on success, -1 on failure.
*/
int register_consumer_task(struct consumer_task *task, struct fs_consumer *fs_consumer);

/**
 * @brief References a filesystem with the fs consumer.
 * 
 * This function references an open file within a filesystem, by increasing the open file count. If
 * the filesystem is not already referenced, it will be added to the fs consumer's open filesystem
 * with an open file count of open_count.
 * 
 * @param fs_consumer The fs consumer to reference the filesystem with.
 * @param fs The filesystem to reference.
 * @param open_count The number of open files within the filesystem.
 * @return int 0 on success, -1 on failure.
*/
int reference_open_filesystem(struct fs_consumer *fs_consumer, struct Filesystem *fs, uint64_t open_count);

/**
 * @brief Checks is the task_id is a consumer of the fs_consumer.
 * 
 * @param fs_consumer Filesystem consumer to check.
 * @param task_id Task id to check.
 * @return true Task is a consumer
 * @return false Task is not a consumer
 */
bool is_fs_consumer(struct fs_consumer *consumer, uint64_t task_id);

#endif // FS_CONSUMER_H