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

struct File_Descriptor {
    bool used;
    bool reserved;
    uint16_t flags;

    uint64_t filesystem_id;
    uint64_t file_id;
    off_t offset;
    pmos_port_t fs_port;
};

struct Filesystem_Data {
    struct File_Descriptor *descriptors_vector;
    size_t count;
    size_t capacity;

    size_t refcount;

    uint64_t fs_consumer_id;

    pthread_spinlock_t lock;
};

static struct Filesystem_Data * fs_data = NULL;
static pthread_spinlock_t fs_data_lock;

// Reserves a file descriptor and returns its id. Returns -1 on error.
static int64_t reserve_descriptor(struct Filesystem_Data* fs_data) {
    pthread_spin_lock(&fs_data->lock);

    // Check if the descriptors vector needs to be resized
    if (fs_data->count == fs_data->capacity) {
        // Double the capacity of the descriptors vector
        size_t new_capacity = (fs_data->capacity == 0) ? 1 : fs_data->capacity * 2;
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
    for (size_t i = 0; i < fs_data->count; ++i) {
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


// Fills a reserved descriptor. Returns 0 on success and -1 on error.
int fill_descriptor(struct Filesystem_Data *fs_data, int64_t descriptor_id, uint16_t flags, uint64_t filesystem_id, pmos_port_t filesystem_port) {
    if (fs_data == NULL || descriptor_id < 0 || descriptor_id >= fs_data->capacity) {
        // Invalid fs_data or descriptor index
        return -1;
    }

    // Lock the descriptor to ensure thread-safety
    pthread_spin_lock(&fs_data->lock);

    if (!fs_data->descriptors_vector[descriptor_id].used) {
        // Descriptor must be used

        // Unlock the descriptor
        pthread_spin_unlock(&fs_data->lock);

        // Return error
        return -1;
    }

    if (fs_data->descriptors_vector[descriptor_id].reserved) {
        // Descriptor must not be reserved

        // Unlock the descriptor
        pthread_spin_unlock(&fs_data->lock);

        // Return error
        return -1;
    }

    // Fill in the necessary information in the descriptor
    fs_data->descriptors_vector[descriptor_id].flags = flags;
    fs_data->descriptors_vector[descriptor_id].filesystem_id = filesystem_id;
    fs_data->descriptors_vector[descriptor_id].fs_port = filesystem_port;
    fs_data->descriptors_vector[descriptor_id].offset = 0;

    // Mark the descriptor as not reserved
    fs_data->descriptors_vector[descriptor_id].reserved = false;

    // Unlock the descriptor
    pthread_spin_unlock(&fs_data->lock);

    return 0; // Success
}

// Releases a reserved descriptor. Returns 0 on success and -1 on error.
static int release_descriptor(struct Filesystem_Data *fs_data, int64_t descriptor) {
    if (fs_data == NULL || descriptor < 0 || descriptor >= fs_data->capacity) {
        // Invalid fs_data or descriptor index
        return -1;
    }

    // Lock the descriptor to ensure thread-safety
    pthread_spin_lock(&fs_data->lock);

    if (!fs_data->descriptors_vector[descriptor].used) {
        // Descriptor must be used

        // Unlock the descriptor
        pthread_spin_unlock(&fs_data->lock);

        // Return error
        return -1;
    }

    if (!fs_data->descriptors_vector[descriptor].reserved) {
        // Descriptor must be reserved

        // Unlock the descriptor
        pthread_spin_unlock(&fs_data->lock);

        // Return error
        return -1;
    }

    // Mark the descriptor as not used and not reserved
    fs_data->descriptors_vector[descriptor].used = false;
    fs_data->descriptors_vector[descriptor].reserved = false;

    // Reset any flags or other data associated with the descriptor

    // Unlock the descriptor
    pthread_spin_unlock(&fs_data->lock);
}

// Initializes fs_data variable. Returns 0 on success, -1 otherwise, setting errno to the appropriate code.
int init_filesystem();

// Requests a port of the filesystem daemon
pmos_port_t request_filesystem_port();

// Global thread-local variable to cache the filesystem port
static __thread pmos_port_t fs_port = INVALID_PORT;

// Global thread-local variable to save a reply port
static __thread pmos_port_t fs_cmd_reply_port = INVALID_PORT;

pmos_port_t get_filesytem_port() {
    const char * port_name = "filesystem";
    ports_request_t request = get_port_by_name(port_name, strlen(port_name), NULL);

    if (request.result != SUCCESS) {
        // Handle error
        return INVALID_PORT;
    }

    return request.port;
}

int open(const char* path, int flags) {
    // Double-checked locking to initialize fs_data if it is NULL
    if (fs_data == NULL) {
        pthread_spin_lock(&fs_data_lock);
        if (fs_data == NULL) {
            int init_result = init_filesystem();
            if (init_result != 0) {
                pthread_spin_unlock(&fs_data_lock);
                return -1;
            }
        }
        pthread_spin_unlock(&fs_data_lock);
    }

    int64_t descriptor = reserve_descriptor(fs_data);
    if (descriptor < 0) {
        errno = ENFILE; // Set errno to appropriate error code
        return -1;
    }

    // Check if the filesystem port is already cached
    if (fs_port == INVALID_PORT) {
        // Request the port of the filesystem daemon
        fs_port = request_filesystem_port();
        if (fs_port == INVALID_PORT) {
            // Handle error: Failed to obtain the filesystem port
            release_descriptor(fs_data, descriptor);
            return -1;
        }
    }

    // Check if reply port exists
    if (fs_cmd_reply_port == INVALID_PORT) {
        // Create a new port for the current thread
        ports_request_t port_request = create_port(PID_SELF, 0);
        if (port_request.result != SUCCESS) {
            // Handle error: Failed to create the port
            release_descriptor(fs_data, descriptor);
            return -1;
        }

        // Save the created port in the thread-local variable
        fs_cmd_reply_port = port_request.port;
    }

    // Calculate the required size for the IPC_Open message (including the variable-sized path and excluding NULL character)
    size_t path_length = strlen(path);
    size_t message_size = sizeof(IPC_Open) + path_length;

    // Allocate memory for the IPC_Open message
    IPC_Open* message = malloc(message_size);
    if (message == NULL) {
        // Handle memory allocation error
        release_descriptor(fs_data, descriptor);
        errno = ENOMEM; // Set errno to appropriate error code
        return -1;
    }

    // Set the message type, flags, and reply port
    message->num = IPC_Open_NUM;
    message->flags = 0; // Set appropriate flags based on the 'flags' argument
    message->reply_port = fs_cmd_reply_port; // Use the reply port
    message->fs_consumer_id = fs_data->fs_consumer_id;

    // Copy the path string into the message
    memcpy(message->path, path, path_length);

    // Send the IPC_Open message to the filesystem daemon
    result_t result = send_message_port(fs_port, message_size, (const char*)message);

    int fail_count = 0;
    while (result == ERROR_PORT_DOESNT_EXIST && fail_count < 5) {
        // Request the port of the filesystem daemon
        pmos_port_t fs_port = request_filesystem_port();
        if (fs_port == INVALID_PORT) {
            // Handle error: Failed to obtain the filesystem port
            free(message); // Free the allocated message memory
            release_descriptor(fs_data, descriptor);
            errno = EIO; // Set errno to appropriate error code
            return -1;
        }

        // Retry sending IPC_Open message to the filesystem daemon
        result = send_message_port(fs_port, message_size, (const char*)message);
        ++fail_count;
    }

    free(message); // Free the allocated message memory

    if (result != SUCCESS) {
        // Handle error: Failed to send the IPC_Open message
        release_descriptor(fs_data, descriptor);
        errno = EIO; // Set errno to appropriate error code
        return -1;
    }

    // Receive the reply message containing the result
    Message_Descriptor reply_descr;
    IPC_Generic_Msg * reply_msg;
    result = get_message(&reply_descr, (unsigned char **)&reply_msg, fs_cmd_reply_port);
    if (result != SUCCESS) {
        // Handle error: Failed to receive the reply message
        release_descriptor(fs_data, descriptor);
        errno = EIO; // Set errno to appropriate error code
        return -1;
    }

    // Verify that the reply message is of type IPC_Open_Reply
    if (reply_msg->type != IPC_Open_Reply_NUM) {
        // Handle error: Unexpected reply message type
        release_descriptor(fs_data, descriptor);
        free(reply_msg);
        errno = EIO; // Set errno to appropriate error code
        return -1;
    }

    // Read the result code from the reply message
    IPC_Open_Reply* reply = (IPC_Open_Reply*)reply_msg;
    int result_code = reply->result_code;

    if (result_code < 0) {
        // Handle error: Failed to open the file
        release_descriptor(fs_data, descriptor);
        free(reply_msg);
        errno = -result_code; // Set errno to appropriate error code (negative value)
        return -1;
    }

    int fill_result = fill_descriptor(fs_data, descriptor, reply->fs_flags, reply->filesystem_id, reply->fs_port);
    if (fill_result < 0) {
        // Handle error
        free(reply_msg);
        errno = EIO; // Set errno to appropriate error code
        return -1;
    }

    free(reply_msg);
    
    // Return the file descriptor
    return descriptor;
}

ssize_t pread(int fd, void *buf, size_t count, off_t offset) {
    // Double-checked locking to initialize fs_data if it is NULL
    if (fs_data == NULL) {
        pthread_spin_lock(&fs_data_lock);
        if (fs_data == NULL) {
            int init_result = init_filesystem();
            if (init_result != 0) {
                pthread_spin_unlock(&fs_data_lock);
                return -1;
            }
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
    struct File_Descriptor *file_desc = &fs_data->descriptors_vector[fd];

    // Check if the file descriptor is in use and reserved
    if (!file_desc->used || !file_desc->reserved) {
        errno = EBADF; // Bad file descriptor
        return -1;
    }

    // Extract the necessary information from the file descriptor
    uint64_t file_id = file_desc->file_id;
    pmos_port_t fs_port = file_desc->fs_port;

    // Unlock the file descriptor
    pthread_spin_unlock(&fs_data->lock);

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

    // Initialize the message type, flags, file ID, offset, count, and reply channel
    IPC_Read message = {
        .type = IPC_Read_NUM,
        .flags = 0,
        .file_id = file_id,
        .fs_consumer_id = fs_data->fs_consumer_id,
        .start_offset = offset,
        .max_size = count,
        .reply_chan = fs_cmd_reply_port,
    };

    size_t message_size = sizeof(IPC_Read);

    // Send the IPC_Read message to the filesystem daemon
    result_t result = send_message_port(fs_port, message_size, (const char *)&message);

    if (result != SUCCESS) {
        errno = EIO; // I/O error
        return -1;
    }

    // Receive the reply message containing the result and data
    Message_Descriptor reply_descr;
    IPC_Generic_Msg * reply_msg;
    result = get_message(&reply_descr, (unsigned char **)&reply_msg, fs_cmd_reply_port);
    if (result != SUCCESS) {
        errno = EIO; // I/O error
        return -1;
    }

    // Verify that the reply message is of type IPC_Read_Reply
    if (reply_msg->type != IPC_Read_Reply_NUM) {
        errno = EIO; // I/O error
        return -1;
    }

    // Read the result code from the reply message
    IPC_Read_Reply *reply = (IPC_Read_Reply *)reply_msg;
    int result_code = reply->result_code;

    if (result_code < 0) {
        free(reply_msg);
        errno = -result_code; // Set errno to appropriate error code (negative value)
        return -1;
    }

    // Copy the data from the reply message to the output buffer
    memcpy(buf, reply->data, count);

    // Free the memory for the reply message
    free(reply_msg);

    // Return the number of bytes read
    return count;
}