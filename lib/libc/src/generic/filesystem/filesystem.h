#ifndef FILESYSTEM_H
#define FILESYSTEM_H
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <pthread.h>
#include <pmos/system.h>

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
    bool reserved;
    uint8_t flags;
    uint8_t type;
    off_t offset;

    const struct Filesystem_Adaptor * adaptor;
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
 * @param exclusive If true, no locks would be acquired and it would be assumed that the caller has exclusive access to the data
 * @return int 0 on success, -1 on error. Sets errno on error.
 */
int __clone_fs_data(struct Filesystem_Data ** new_data, uint64_t for_task, bool exclusive);


#endif