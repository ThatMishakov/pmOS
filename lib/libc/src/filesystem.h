#ifndef FILESYSTEM_H
#define FILESYSTEM_H
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <pthread.h>
#include <pmos/system.h>

struct File_Descriptor {
    bool used;
    bool reserved;
    uint16_t flags;

    task_group_t filesystem_id;
    uint64_t file_id;
    off_t offset;
    pmos_port_t fs_port;
};

struct Filesystem_Data {
    struct File_Descriptor *descriptors_vector;
    size_t count;
    size_t capacity;

    uint64_t fs_consumer_id;

    pthread_spinlock_t lock;
};

/**
 * @brief Creates a clone of the filesystem data for the given task
 * 
 * This function creates a clone of the filesystem data for the given task. The new data is stored in the new_data pointer.
 * It should typically be called during the fork() call to create a new filesystem data for the child process.
 * 
 * @param new_data Pointer to the new filesystem data
 * @param for_task Task ID to clone the data for
 * @return int 0 on success, -1 on error. Sets errno on error.
 */
int __clone_fs_data(struct Filesystem_Data ** new_data, uint64_t for_task);


#endif