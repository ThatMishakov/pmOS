#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
#include <errno.h>
#include <pmos/system.h>
#include <pthread.h>
#include <pmos/ports.h>
#include <kernel/errors.h>
#include <pmos/ipc.h>
#include <pmos/helpers.h>
#include <unistd.h>
#include <stdio.h>
#include "filesystem.h"
#include <assert.h>
#include "../fork/fork.h"
#include "filesystem_struct.h"
#include "file.h"
#include "ipc_queue.h"

static struct Filesystem_Data * fs_data = NULL;
static pthread_spinlock_t fs_data_lock;

// Creates and initializes new Filesystem_Data. Returns its pointer on success, NULL otherwise, setting errno to the appropriate code.
struct Filesystem_Data *init_filesystem();

// Reserves a file descriptor and returns its id. Returns -1 on error.
static int64_t reserve_descriptor(struct Filesystem_Data* fs_data) {
    pthread_spin_lock(&fs_data->lock);

    // Check if the descriptors vector needs to be resized
    if (fs_data->count == fs_data->capacity) {
        // Double the capacity of the descriptors vector
        size_t new_capacity = (fs_data->capacity == 0) ? 4 : fs_data->capacity * 2;
        struct File_Descriptor* new_vector = realloc(fs_data->descriptors_vector, new_capacity * sizeof(struct File_Descriptor));
        if (new_vector == NULL) {
            pthread_spin_unlock(&fs_data->lock);
            // Handle memory allocation error
            return -1;
        }

        // Zero out memory
        memset(&new_vector[fs_data->capacity], 0, new_capacity - fs_data->capacity);

        fs_data->descriptors_vector = new_vector;
        fs_data->capacity = new_capacity;
    }

    // Find an available descriptor
    int64_t descriptor_id = -1;
    for (size_t i = 0; i < fs_data->capacity; ++i) {
        if (!fs_data->descriptors_vector[i].used) {
            descriptor_id = i;
            break;
        }
    }

    if (descriptor_id == -1) {
        // Error
        pthread_spin_unlock(&fs_data->lock);
        return -1;
    }

    // Mark the descriptor as reserved
    fs_data->descriptors_vector[descriptor_id].used = true;
    fs_data->descriptors_vector[descriptor_id].reserved = true;

    pthread_spin_unlock(&fs_data->lock);
    return descriptor_id;
}

// Releases a reserved descriptor. Returns 0 on success and -1 on error.
static int release_descriptor(struct Filesystem_Data *fs_data, int64_t descriptor) {
    if (fs_data == NULL || descriptor < 0 || descriptor >= fs_data->capacity) {
        // Invalid fs_data or descriptor index
        return -1;
    }

    // Lock the descriptor to ensure thread-safety
    pthread_spin_lock(&fs_data->lock);

    struct File_Descriptor *des = &fs_data->descriptors_vector[descriptor];

    if (!des->used) {
        // Descriptor must be used

        // Unlock the descriptor
        pthread_spin_unlock(&fs_data->lock);

        // Return error
        return -1;
    }

    if (!des->reserved) {
        // Descriptor must be reserved

        // Unlock the descriptor
        pthread_spin_unlock(&fs_data->lock);

        // Return error
        return -1;
    }

    des->adaptor->free(des, fs_data->fs_consumer_id);

    // Mark the descriptor as not used and not reserved
    des->used = false;
    des->reserved = false;

    // Reset any flags or other data associated with the descriptor

    // Unlock the descriptor
    pthread_spin_unlock(&fs_data->lock);
}

// Requests a port of the filesystem daemon
pmos_port_t request_filesystem_port()
{
    static const char filesystem_port_name[] = "/pmos/vfsd";
    ports_request_t port_req = get_port_by_name(filesystem_port_name, strlen(filesystem_port_name), 0);
    if (port_req.result != SUCCESS) {
        // Handle error
        return INVALID_PORT;
    }

    return port_req.port;
}

// Global thread-local variable to cache the filesystem port
static __thread pmos_port_t fs_port = INVALID_PORT;

// Global thread-local variable to save a reply port
static __thread pmos_port_t fs_cmd_reply_port = INVALID_PORT;

pmos_port_t get_filesytem_port() {
    const char * port_name = "filesystem";
    ports_request_t request = get_port_by_name(port_name, strlen(port_name), 0);

    if (request.result != SUCCESS) {
        // Handle error
        return INVALID_PORT;
    }

    return request.port;
}

int __share_fs_data(uint64_t tid) {
    // Double-checked locking to initialize fs_data if it is NULL
    if (fs_data == NULL) {
        pthread_spin_lock(&fs_data_lock);
        if (fs_data == NULL) {
            struct Filesystem_Data *new_data = init_filesystem();
            if (new_data == NULL) {
                pthread_spin_unlock(&fs_data_lock);
                return -1;
            }
            fs_data = new_data;
        }
        pthread_spin_unlock(&fs_data_lock);
    }

    result_t r = add_task_to_group(tid, fs_data->fs_consumer_id);
    if (r != SUCCESS) {
        errno = -r;
        return -1;
    }

    return 0;
}

int open(const char* path, int flags) {
    // Double-checked locking to initialize fs_data if it is NULL
    if (fs_data == NULL) {
        pthread_spin_lock(&fs_data_lock);
        if (fs_data == NULL) {
            struct Filesystem_Data *new_data = init_filesystem();
            if (new_data == NULL) {
                pthread_spin_unlock(&fs_data_lock);
                return -1;
            }
            fs_data = new_data;
        }
        pthread_spin_unlock(&fs_data_lock);
    }

    int64_t descriptor = reserve_descriptor(fs_data);
    if (descriptor < 0) {
        errno = ENFILE; // Set errno to appropriate error code
        return -1;
    }

    const struct Filesystem_Adaptor * file_adaptor = &__file_adaptor;
    union File_Data data;
    int result_code = __open_file(path, flags, 0, &data, fs_data->fs_consumer_id);

    if (result_code < 0) {
        // Handle error: Failed to open the file
        release_descriptor(fs_data, descriptor);
        errno = -result_code; // Set errno to appropriate error code (negative value)
        return -1;
    }

    result_code = pthread_spin_lock(&fs_data->lock);
    if (result_code != SUCCESS) {
        // Handle error: Failed to lock the filesystem data
        release_descriptor(fs_data, descriptor);
        file_adaptor->close(&data, fs_data->fs_consumer_id);
        file_adaptor->free(&data, fs_data->fs_consumer_id);
        // errno = result_code; // Set errno to appropriate error code
        return -1;
    }

    fs_data->descriptors_vector[descriptor].type = DESCRIPTOR_FILE;
    fs_data->descriptors_vector[descriptor].used = true;
    fs_data->descriptors_vector[descriptor].reserved = false;
    fs_data->descriptors_vector[descriptor].flags = flags;
    fs_data->descriptors_vector[descriptor].offset = 0;
    fs_data->descriptors_vector[descriptor].adaptor = file_adaptor;
    fs_data->descriptors_vector[descriptor].data = data;

    pthread_spin_unlock(&fs_data->lock);

    // Return the file descriptor
    return descriptor;
}

ssize_t __read_internal(long int fd, void *buf, size_t count, bool should_seek, size_t _offset) {
    // Double-checked locking to initialize fs_data if it is NULL
    if (fs_data == NULL) {
        errno = EBADF; // Bad file descriptor
        return -1;
    }

    // Lock the file descriptor to ensure thread-safety
    pthread_spin_lock(&fs_data->lock);

    // Check if the file descriptor is valid
    if (fd < 0 || fd >= fs_data->capacity) {
        pthread_spin_unlock(&fs_data->lock);
        errno = EBADF; // Bad file descriptor
        return -1;
    }

    // Obtain the file descriptor data
    struct File_Descriptor file_desc = fs_data->descriptors_vector[fd];

    // Unlock the file descriptor
    pthread_spin_unlock(&fs_data->lock);

    // Check if the file descriptor is in use and reserved
    if (!file_desc.used || file_desc.reserved) {
        errno = EBADF; // Bad file descriptor
        return -1;
    }

    size_t offset = should_seek ? file_desc.offset : _offset;
    bool is_seekable = file_desc.adaptor->isseekable(&file_desc.data, fs_data->fs_consumer_id);

    count = file_desc.adaptor->read(&file_desc.data, fs_data->fs_consumer_id, buf, count, offset);

    if (count < 0)
        // Error
        return -1;

    if (!is_seekable || !should_seek)
        // Don't update the offset
        return count;

    // Add count to the file descriptor's offset
    pthread_spin_lock(&fs_data->lock);

    // Check if the file descriptor is valid
    if (fd < 0 || fd >= fs_data->capacity || memcmp(&fs_data->descriptors_vector[fd], &file_desc, sizeof(struct File_Descriptor)) != 0) {
        // POSIX says that the behavior is undefined if multiple threads use the same file descriptor
        // Be nice and don't crash the program but don't update the offset
        pthread_spin_unlock(&fs_data->lock);
        return count;
    }

    // Update the file descriptor's offset
    fs_data->descriptors_vector[fd].offset = offset + count;
    pthread_spin_unlock(&fs_data->lock);

    // Return the number of bytes read
    return count;
}

ssize_t read(int fd, void *buf, size_t count) {
    return __read_internal(fd, buf, count, true, 0);
}

ssize_t pread(int fd, void *buf, size_t count, off_t offset) {
    return __read_internal(fd, buf, count, false, offset);
}

static int set_desc_queue(struct Filesystem_Data *fs_data, size_t descriptor_id, const char *name) {
    assert(fs_data != NULL);

    if (descriptor_id < 0 || descriptor_id >= fs_data->capacity) {
        // Invalid descriptor ID
        errno = EBADF;
        return -1;
    }

    // Don't lock the descriptor because this is called during initialization

    // Check if the descriptor is used
    if (fs_data->descriptors_vector[descriptor_id].used || fs_data->descriptors_vector[descriptor_id].reserved) {
        // Descriptor must not be used
        errno = EBADF;
        return -1;
    }

    struct File_Descriptor *des = &fs_data->descriptors_vector[descriptor_id];

    int result = __set_desc_queue(&des->data, fs_data->fs_consumer_id, name);
    if (result != 0)
        return result;

    des->type = DESCRIPTOR_IPC_QUEUE;
    des->used = true;
    des->reserved = false;
    des->flags = 0;
    des->adaptor = &__ipc_queue_adaptor;

    return 0;
}

static void free_descriptor(struct File_Descriptor *des) {
    assert(des != NULL);

    if (des->used && !des->reserved) {
        des->adaptor->close(&des->data, fs_data->fs_consumer_id);
        des->adaptor->free(&des->data, fs_data->fs_consumer_id);

        des->used = false;
        des->reserved = false;
    }
}

struct Filesystem_Data *init_filesystem() {
    // Check if reply port exists
    if (fs_cmd_reply_port == INVALID_PORT) {
        // Create a new port for the current thread
        ports_request_t port_request = create_port(PID_SELF, 0);
        if (port_request.result != SUCCESS) {
            // Handle error: Failed to create the port
            return NULL;
        }

        // Save the created port in the thread-local variable
        fs_cmd_reply_port = port_request.port;
    }

    struct Filesystem_Data *new_fs_data = malloc(sizeof(struct Filesystem_Data));
    if (new_fs_data == NULL) {
        // Handle memory allocation error
        errno = ENOMEM;
        return NULL;
    }

    static const size_t initial_capacity = 8;
    struct File_Descriptor *new_descriptors_vector = calloc(initial_capacity, sizeof(struct File_Descriptor));
    if (new_descriptors_vector == NULL) {
        // Handle memory allocation error
        free(new_fs_data);
        errno = ENOMEM;
        return NULL;
    }

    new_fs_data->descriptors_vector = new_descriptors_vector;
    new_fs_data->count = 0;
    new_fs_data->capacity = initial_capacity;

    int result = set_desc_queue(new_fs_data, 1, "/pmos/stdout");
    if (result != SUCCESS) {
        // Handle error
        free(new_fs_data->descriptors_vector);
        free(new_fs_data);
        // errno is set by set_desc_queue
        return NULL;
    }

    result = set_desc_queue(new_fs_data, 2, "/pmos/stderr");
    if (result != SUCCESS) {
        // Handle error
        free_descriptor(&new_fs_data->descriptors_vector[0]);
        free(new_fs_data->descriptors_vector);
        free(new_fs_data);
        // errno is set by set_desc_queue
        return NULL;
    }

    result = set_desc_queue(new_fs_data, 0, "/pmos/stdin");
    if (result != SUCCESS) {
        // Handle error
        free_descriptor(&new_fs_data->descriptors_vector[0]);
        free_descriptor(&new_fs_data->descriptors_vector[1]);
        free(new_fs_data->descriptors_vector);
        free(new_fs_data);
        // errno is set by set_desc_queue
        return NULL;
    }

    // Initialize the spinlock
    result = pthread_spin_init(&new_fs_data->lock, PTHREAD_PROCESS_PRIVATE);
    if (result != 0) {
        // Handle error
        free_descriptor(&new_fs_data->descriptors_vector[0]);
        free_descriptor(&new_fs_data->descriptors_vector[1]);
        free_descriptor(&new_fs_data->descriptors_vector[2]);
        free(new_fs_data->descriptors_vector);
        free(new_fs_data);
        // errno is set by pthread_spin_init
        return NULL;
    }

    // Create a new task group
    syscall_r sys_result = create_task_group();
    if (sys_result.result != SUCCESS) {
        // Handle error: Failed to create the task group
        free_descriptor(&new_fs_data->descriptors_vector[0]);
        free_descriptor(&new_fs_data->descriptors_vector[1]);
        free_descriptor(&new_fs_data->descriptors_vector[2]);
        free(new_fs_data->descriptors_vector);
        pthread_spin_destroy(&new_fs_data->lock);
        free(new_fs_data);
        errno = -sys_result.result;
        return NULL;
    }

    new_fs_data->fs_consumer_id = sys_result.value;

    return new_fs_data;
}

/// Destroys the filesystem data structure. Assumes that all of the processes inside the group have left it.
/// @param fs_data Filesystem data to be destroyed
static void destroy_filesystem(struct Filesystem_Data * fs_data)
{
    if (fs_data == NULL)
        return;

    // Destroy the spinlock
    pthread_spin_destroy(&fs_data->lock);

    if (fs_data->descriptors_vector != NULL) {
        for (size_t i = 0; i < fs_data->capacity; ++i) {
            if (fs_data->descriptors_vector[i].used) {
                fs_data->descriptors_vector[i].adaptor->free(&fs_data->descriptors_vector[i], fs_data->fs_consumer_id);
            }
        }

        free(fs_data->descriptors_vector);
    }

    free(fs_data);
}

int __close_internal(long int filedes) {
    if (filedes < 0) {
        errno = EBADF;
        return -1;
    }

    if (fs_data == NULL) {
        errno = EBADF;
        return -1;
    }

    int result = pthread_spin_lock(&fs_data->lock);
    if (result != SUCCESS) {
        errno = result;
        return -1;
    }

    if (fs_data->capacity <= filedes) {
        pthread_spin_unlock(&fs_data->lock);
        errno = EBADF;
        return -1;
    }

    struct File_Descriptor *des = &fs_data->descriptors_vector[filedes];

    if (!des->used || des->reserved) {
        pthread_spin_unlock(&fs_data->lock);
        errno = EBADF;
        return -1;
    }

    struct File_Descriptor copy = *des;
    uint64_t fs_consumer_id = fs_data->fs_consumer_id;

    des->used = false;
    
    pthread_spin_unlock(&fs_data->lock);

    result = copy.adaptor->close(&copy.data, fs_consumer_id);
    // Even if the close() function fails, we still need to free the descriptor
    copy.adaptor->free(&copy.data, fs_consumer_id);

    return result;
}

int close(int filedes) {
    return __close_internal(filedes);
}

off_t __lseek_internal(long int fd, off_t offset, int whence)
{
    if (fs_data == NULL) {
        // Filesystem is not initialized; No files opened
        errno = EBADF;
        return -1;
    }

    int result = pthread_spin_lock(&fs_data->lock);
    if (result != SUCCESS) {
        errno = result;
        return -1;
    }

    if (fd < 0 || fs_data->capacity <= fd) {
        pthread_spin_unlock(&fs_data->lock);
        errno = EBADF;
        return -1;
    }

    struct File_Descriptor *des = &fs_data->descriptors_vector[fd];

    if (des->used == false || des->reserved == true) {
        pthread_spin_unlock(&fs_data->lock);
        errno = EBADF;
        return -1;
    }

    if (!des->adaptor->isseekable(&des->data, fs_data->fs_consumer_id)) {
        pthread_spin_unlock(&fs_data->lock);
        errno = ESPIPE;
        return -1;
    }

    switch (whence) {
        case SEEK_SET:
            if (offset < 0) {
                pthread_spin_unlock(&fs_data->lock);
                errno = EINVAL;
                return -1;
            }
            des->offset = offset;
            break;
        case SEEK_CUR:
            if (des->offset + offset < 0) {
                pthread_spin_unlock(&fs_data->lock);
                errno = EINVAL;
                return -1;
            }
            des->offset += offset;
            break;
        case SEEK_END:
            // Not implemented
            pthread_spin_unlock(&fs_data->lock);
            errno = EINVAL;
            return -1;
        default:
            pthread_spin_unlock(&fs_data->lock);
            errno = EINVAL;
            return -1;
    }

    off_t result_offset = des->offset;
    pthread_spin_unlock(&fs_data->lock);
    return result_offset;
}

off_t lseek(int fd, off_t offset, int whence) {
    return __lseek_internal(fd, offset, whence);
}

int vfsd_send_persistant(size_t msg_size, const void *message)
{
    result_t k_result = fs_port != INVALID_PORT ? 
        send_message_port(fs_port, msg_size, (char *)message) 
        : ERROR_PORT_DOESNT_EXIST;

    int fail_count = 0;
    while (k_result == ERROR_PORT_DOESNT_EXIST && fail_count < 5) {
        // Request the port of the filesystem daemon
        pmos_port_t fs_port = request_filesystem_port();
        if (fs_port == INVALID_PORT) {
            // Handle error: Failed to obtain the filesystem port
            errno = EIO; // Set errno to appropriate error code
            return -1;
        }

        // Retry sending IPC_Open message to the filesystem daemon
        k_result = send_message_port(fs_port, msg_size, (char *)message);
        ++fail_count;
    }

    return k_result == SUCCESS ? 0 : -1;
}

int __clone_fs_data(struct Filesystem_Data ** new_data, uint64_t for_task, bool exclusive)
{
    assert(new_data != NULL);

    if (fs_data == NULL) {
        *new_data = NULL;
        return 0;
    }

    // Check if reply port exists
    if (fs_cmd_reply_port == INVALID_PORT) {
        // Create a new port for the current thread
        ports_request_t port_request = create_port(PID_SELF, 0);
        if (port_request.result != SUCCESS) {
            // Handle error: Failed to create the port
            return -1;
        }

        // Save the created port in the thread-local variable
        fs_cmd_reply_port = port_request.port;
    }

    struct Filesystem_Data * new_fs_data = init_filesystem();
    if (new_fs_data == NULL) {
        // errno is set by init_filesystem
        return -1;
    }

    result_t r = add_task_to_group(for_task, new_fs_data->fs_consumer_id);
    if (r != SUCCESS) {
        remove_task_from_group(for_task, new_fs_data->fs_consumer_id);
        destroy_filesystem(new_fs_data);
        errno = -r;
        return -1;
    }

    int result;
    if (!exclusive) {
        result = pthread_spin_lock(&fs_data->lock);
        if (result != SUCCESS) {
            remove_task_from_group(for_task, new_fs_data->fs_consumer_id);
            destroy_filesystem(new_fs_data);
            // errno is set by pthread_spin_lock
            return -1;
        }
    }


    // Make sure there is enough space in the new vector
    if (new_fs_data->capacity < fs_data->capacity) {
        // Resize the vector
        size_t capacity = new_fs_data->capacity;

        struct File_Descriptor * new_vector = realloc(new_fs_data->descriptors_vector, fs_data->capacity * sizeof(struct File_Descriptor));
        if (new_vector == NULL) {
            remove_task_from_group(for_task, new_fs_data->fs_consumer_id);
            destroy_filesystem(new_fs_data);

            if (!exclusive)
                pthread_spin_unlock(&fs_data->lock);
            
            errno = ENOMEM;
            return -1;
        }
        new_fs_data->descriptors_vector = new_vector;
        new_fs_data->capacity = fs_data->capacity;
        for (size_t i = capacity; i < new_fs_data->capacity; ++i) {
            new_fs_data->descriptors_vector[i].used = false;
            new_fs_data->descriptors_vector[i].reserved = false;
        }
    }

    // Clone tasks
    size_t i = 0;
    for (i = 0; i < fs_data->capacity; ++i) {
        if (fs_data->descriptors_vector[i].used == false || fs_data->descriptors_vector[i].reserved == true) {
            // If the vector is reserved, it means that the file descriptor was being opened by another thread.
            // Since it's a race condition, don't clone it.
            new_fs_data->descriptors_vector[i].used = false;
            new_fs_data->descriptors_vector[i].reserved = false;
            continue;
        }

        struct File_Descriptor fd = fs_data->descriptors_vector[i];
        struct File_Descriptor *new_fd = &new_fs_data->descriptors_vector[i];

        new_fd->type = fd.type;
        new_fd->used = true;
        new_fd->reserved = false;
        new_fd->flags = fd.flags;
        new_fd->offset = fd.offset;
        new_fd->adaptor = fd.adaptor;

        result = fd.adaptor->clone(&fd.data, fs_data->fs_consumer_id, &new_fd->data, new_fs_data->fs_consumer_id);
        if (result != SUCCESS) {
            // Handle error
            remove_task_from_group(for_task, new_fs_data->fs_consumer_id);
            remove_task_from_group(PID_SELF, new_fs_data->fs_consumer_id);
            destroy_filesystem(new_fs_data);

            if (!exclusive)
                pthread_spin_unlock(&fs_data->lock);

            errno = -result;
            return -1;
        }

        new_fs_data->count++;   
    }

    for (; i < new_fs_data->capacity; ++i) {
        new_fs_data->descriptors_vector[i].used = false;
        new_fs_data->descriptors_vector[i].reserved = false;
    }

    if (!exclusive)
        pthread_spin_unlock(&fs_data->lock);

    // Remove self from the group
    remove_task_from_group(PID_SELF, new_fs_data->fs_consumer_id);

    *new_data = new_fs_data;
    return 0;
}

void __libc_fixup_fs_post_fork(struct fork_for_child * child_data)
{
    if (fs_data != NULL)
        destroy_filesystem(fs_data);
    else
        pthread_spin_unlock(&fs_data_lock);

    fs_data = child_data->fs_data;

    // Fixup the reply port
    fs_cmd_reply_port = INVALID_PORT;
}

void __libc_fs_lock_pre_fork()
{
    if (fs_data == NULL) {
        pthread_spin_lock(&fs_data_lock);

        if (fs_data == NULL)
            return;
            
        pthread_spin_unlock(&fs_data_lock);
    }

    pthread_spin_lock(&fs_data->lock);
}

void __libc_fs_unlock_post_fork()
{
    if (fs_data == NULL)
        pthread_spin_unlock(&fs_data_lock);
    else
        pthread_spin_unlock(&fs_data->lock);
}

ssize_t __write_internal(long int fd, const void * buf, size_t count, size_t _offset, bool inc_offset) {
    // Double-checked locking to initialize fs_data if it is NULL
    if (fs_data == NULL) {
        pthread_spin_lock(&fs_data_lock);
        if (fs_data == NULL) {
            struct Filesystem_Data *new_fs_data = init_filesystem();
            if (new_fs_data == NULL) {
                pthread_spin_unlock(&fs_data_lock);
                return -1;
            }
            fs_data = new_fs_data;
        }
        pthread_spin_unlock(&fs_data_lock);
    }

    // Lock the file descriptor to ensure thread-safety
    pthread_spin_lock(&fs_data->lock);

    // Check if the file descriptor is valid
    if (fd < 0 || fd >= fs_data->capacity) {
        errno = EBADF; // Bad file descriptor
        return -1;
    }

    // Obtain the file descriptor data
    struct File_Descriptor file_desc = fs_data->descriptors_vector[fd];

    // Unlock the file descriptor
    pthread_spin_unlock(&fs_data->lock);

    // Check if the file descriptor is in use and reserved
    if (!file_desc.used || file_desc.reserved) {
        errno = EBADF; // Bad file descriptor
        return -1;
    }

    size_t offset = inc_offset ? file_desc.offset : _offset;
    bool is_seekable = file_desc.adaptor->isseekable(&file_desc.data, fs_data->fs_consumer_id);
    bool seek = is_seekable && inc_offset;
    union File_Data data_copy = file_desc.data;

    count = file_desc.adaptor->write(&file_desc.data, fs_data->fs_consumer_id, buf, count, offset);

    bool changed = memcmp(&data_copy, &file_desc.data, sizeof(union File_Data)) != 0;

    if (count < 0)
        // Error
        return -1;

    if (!seek && !changed)
        // Don't update the offset and port
        return count;

    // Add count to the file descriptor's offset
    pthread_spin_lock(&fs_data->lock);

    // Check if the file descriptor is valid
    if (fd < 0 || fd >= fs_data->capacity || memcmp(&fs_data->descriptors_vector[fd], &file_desc, sizeof(struct File_Descriptor)) != 0) {
        // POSIX says that the behavior is undefined if multiple threads use the same file descriptor
        // Be nice and don't crash the program but don't update the offset
        pthread_spin_unlock(&fs_data->lock);
        return count;
    }

    // Update the file descriptor's offset
    if (seek)
        fs_data->descriptors_vector[fd].offset = offset + count;

    if (changed)
        fs_data->descriptors_vector[fd].data = file_desc.data;

    pthread_spin_unlock(&fs_data->lock);

    // Return the number of bytes read
    return count;
}

ssize_t write(int fd, const void * buf, size_t count)
{
    return __write_internal(fd, buf, count, 0, true);
}