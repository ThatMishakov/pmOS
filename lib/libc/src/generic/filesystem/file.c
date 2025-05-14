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

#include "file.h"

#include "filesystem.h"

#include <errno.h>
#include <limits.h>
#include <pmos/helpers.h>
#include <pmos/ipc.h>
#include <pmos/ports.h>
#include <pmos/tls.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/uio.h>

int vfsd_send_persistant(size_t msg_size, const void *message);

// TODO: Change to other function
pmos_port_t prepare_reply_port()
{
    struct uthread *ut = __get_tls();
    if (ut->cmd_reply_port == INVALID_PORT) {
        // Create a new port for the current thread
        ports_request_t port_request = create_port(TASK_ID_SELF, 0);
        if (port_request.result != SUCCESS) {
            fprintf(stderr, "pmOS libC: prepare_reply_port() failed to create port: %s\n",
                    strerror(-port_request.result));
            errno = EIO;
        } else
            ut->cmd_reply_port = port_request.port;
    }

    return ut->cmd_reply_port;
}

static pmos_right_t request_filesystem_right()
{
    static const char filesystem_port_name[] = "/pmos/vfsd";
    right_request_t right_req =
        get_right_by_name(filesystem_port_name, strlen(filesystem_port_name), 0);
    if (right_req.result != SUCCESS) {
        return INVALID_PORT;
    }

    return right_req.right;
}

static pmos_right_t get_fs_right()
{
    struct uthread *u = __get_tls();

    if (u->fs_right)
        return u->fs_right;

    return (u->fs_right = request_filesystem_right());
}

static void fs_right_invalid(pmos_right_t right)
{
    __atomic_compare_exchange_n(&__get_tls()->fs_right, &right, INVALID_RIGHT, false,
                                __ATOMIC_RELAXED, __ATOMIC_RELAXED);
}

ssize_t __file_read(void *file_data, uint64_t consumer_id, void *buf, size_t size, size_t offset)
{
    // Check if ssize_t can represent size
    // This is necessary since, although very implausible with 64 bit ints, size_t value might be
    // passed which would produce overflow when converted to ssize_t
    if (size > SSIZE_MAX) {
        errno = EOVERFLOW; // Value too large for defined data type
        return -1;
    }

    pmos_port_t fs_cmd_reply_port = prepare_reply_port();
    if (fs_cmd_reply_port == INVALID_PORT) {
        return -1;
    }

    struct File *file = (struct File *)file_data;

    // Initialize the message type, flags, file ID, offset, count, and reply channel
    IPC_Read message = {
        .type           = IPC_Read_NUM,
        .flags          = 0,
        .file_id        = file->file_id,
        .fs_consumer_id = consumer_id,
        .start_offset   = offset,
        .max_size       = size,
        .reply_port     = fs_cmd_reply_port,
    };

    size_t message_size = sizeof(IPC_Read);

    // Send the IPC_Read message to the filesystem daemon
    result_t result = send_message_port(file->fs_port, message_size, (const char *)&message);
    if (result != SUCCESS) {
        errno = EIO; // I/O error
        return -1;
    }

    // Receive the reply message containing the result and data
    Message_Descriptor reply_descr;
    IPC_Generic_Msg *reply_msg;
    result = get_message(&reply_descr, (unsigned char **)&reply_msg, fs_cmd_reply_port, NULL, NULL);
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
    int result_code       = reply->result_code;

    if (result_code < 0) {
        free(reply_msg);
        errno = -result_code; // Set errno to appropriate error code (negative value)
        return -1;
    }

    ssize_t count = 0;

    count = reply_descr.size - offsetof(IPC_Read_Reply, data);

    // Copy the data from the reply message to the output buffer
    memcpy(buf, reply->data, count);

    // Free the memory for the reply message
    free(reply_msg);

    return count;
}

// int write_file(uint64_t file_id, pmos_port_t fs_port, size_t offset, size_t count, const void
// *buf) {
//     // Check if reply port exists
//     if (fs_cmd_reply_port == INVALID_PORT) {
//         // Create a new port for the current thread
//         ports_request_t port_request = create_port(TASK_ID_SELF, 0);
//         if (port_request.result != SUCCESS) {
//             // Handle error: Failed to create the port
//             return -1;
//         }

//         // Save the created port in the thread-local variable
//         fs_cmd_reply_port = port_request.port;
//     }

//     // Initialize the message type, flags, file ID, offset, count, and reply channel
//     IPC_Write message = {
//         .type = IPC_Write_NUM,
//         .flags = 0,
//         .file_id = file_id,
//         .fs_consumer_id = fs_data->fs_consumer_id,
//         .start_offset = offset,
//         .max_size = count,
//         .reply_port = fs_cmd_reply_port,
//     };

//     size_t message_size = sizeof(IPC_Write) + count;

//     // Copy the data into the message
//     memcpy(message.data, buf, count);

//     // Send the IPC_Write message to the filesystem daemon
//     result_t result = send_message_port(fs_port, message_size, (const char *)&message);

//     if (result != SUCCESS) {
//         errno = EIO; // I/O error
//         return -1;
//     }

//     // Receive the reply message containing the result and data
//     Message_Descriptor reply_descr;
//     IPC_Generic_Msg * reply_msg;
//     result = get_message(&reply_descr, (unsigned char **)&reply_msg, fs_cmd_reply_port);
//     if (result != SUCCESS) {
//         errno = EIO; // I/O error
//         return -1;
//     }

//     // Verify that the reply message is of type IPC_Write_Reply
//     if (reply_msg->type != IPC_Write_Reply_NUM) {
//         errno = EIO; // I/O error
//         return -1;
//     }

//     // Read the result code from the reply message
//     IPC_Write_Reply *reply = (IPC_Write_Reply *)reply_msg;
//     int result_code = reply->result_code;

//     if (result_code < 0) {
//         free(reply_msg);
//         errno = -result_code; // Set errno to appropriate error code (negative value)
//         return -1;
//     }

//     count = reply_descr.size - offsetof(IPC_Write_Reply, data);

//     // Free the memory for the reply message
//     free(reply_msg);

//     return count;
// }

ssize_t __file_write(void *file_data, uint64_t consumer_id, const void *buf, size_t size,
                     size_t offset)
{
    ssize_t count              = -1;
    IPC_Generic_Msg *reply_msg = NULL;
    if (size > SSIZE_MAX) {
        errno = EOVERFLOW;
        return -1;
    }

    pmos_port_t reply_port = prepare_reply_port();
    if (reply_port == INVALID_PORT) {
        return -1;
    }

    struct File *file     = (struct File *)file_data;
    const size_t msg_size = sizeof(IPC_Write) + size;

    uint64_t buff[1 + (msg_size - 1) / sizeof(uint64_t)];
    IPC_Write *message      = (void *)buff;
    message->type           = IPC_Write_NUM;
    message->flags          = 0;
    message->file_id        = file->file_id;
    message->fs_consumer_id = consumer_id;
    message->offset         = offset;
    message->reply_port     = reply_port;

    memcpy(message->data, buf, size);

    result_t k_result = send_message_port(file->fs_port, msg_size, (const char *)message);
    if (k_result != SUCCESS) {
        errno = EIO;
        goto error;
    }

    Message_Descriptor reply_descr;
    k_result = get_message(&reply_descr, (unsigned char **)&reply_msg, reply_port, NULL, NULL);
    if (k_result != SUCCESS) {
        errno = EIO;
        goto error;
    }

    if (reply_msg->type != IPC_Write_Reply_NUM) {
        errno = EIO;
        goto error;
    }

    IPC_Write_Reply *reply = (IPC_Write_Reply *)reply_msg;
    if (reply->result_code < 0) {
        errno = -reply->result_code;
        goto error;
    }

    count = reply->bytes_written;
error:
    free(reply_msg);
    return count;
}

ssize_t __file_writev(void *file_data, uint64_t consumer_id, const struct iovec *iov, int iovcnt,
                      size_t offset)
{
    ssize_t count              = -1;
    IPC_Generic_Msg *reply_msg = NULL;

    pmos_port_t reply_port = prepare_reply_port();
    if (reply_port == INVALID_PORT) {
        return -1;
    }

    // TODO: This is problematic...
    struct File *file = (struct File *)file_data;
    for (int i = 0; i < iovcnt; i++) {
        const size_t msg_size = sizeof(IPC_Write) + iov[i].iov_len;

        uint64_t buff[1 + (msg_size - 1) / sizeof(uint64_t)];
        IPC_Write *message      = (void *)buff;
        message->type           = IPC_Write_NUM;
        message->flags          = 0;
        message->file_id        = file->file_id;
        message->fs_consumer_id = consumer_id;
        message->offset         = offset + count;
        message->reply_port     = reply_port;

        memcpy(message->data, iov[i].iov_base, iov[i].iov_len);

        result_t k_result = send_message_port(file->fs_port, msg_size, (const char *)message);
        if (k_result != SUCCESS) {
            errno = EIO;
            goto error;
        }

        Message_Descriptor reply_descr;
        k_result = get_message(&reply_descr, (unsigned char **)&reply_msg, reply_port, NULL, NULL);
        if (k_result != SUCCESS) {
            errno = EIO;
            goto error;
        }

        if (reply_msg->type != IPC_Write_Reply_NUM) {
            errno = EIO;
            goto error;
        }

        IPC_Write_Reply *reply = (IPC_Write_Reply *)reply_msg;
        if (reply->result_code < 0) {
            errno = -reply->result_code;
            goto error;
        }

        count += reply->bytes_written;
        continue;
    error:
        free(reply_msg);
        return count;
    }

    return count;
}

int __file_clone(void *file_data, uint64_t consumer_id, void *new_data, uint64_t new_consumer_id)
{
    pmos_port_t fs_cmd_reply_port = prepare_reply_port();
    if (fs_cmd_reply_port == INVALID_PORT) {
        return -1;
    }
    struct File *file = (struct File *)file_data;

    // Clone the entry in the filesystem daemon
    // This has to be done as filesystem daemons are free to implement their own
    // file descriptor mapping schemes, which might envolve creating new file ids
    // for each open entry, even if they point to the same file. I have considered
    // shared references, but this complicates the things a lot and creates
    // unnecessary problems with file descriptor ownership, especially with stuff
    // like fork, spawn, exec, etc. and creates race conditions and whatnot.
    //
    // I feel like this sacrifices a bit of memory consumption (which is arguable
    // because of malloc overhead) for a faster implementation and more flexibility.
    IPC_Dup request = {
        .type            = IPC_Dup_NUM,
        .flags           = IPC_FLAG_REGISTER_IF_NOT_FOUND,
        .file_id         = file->file_id,
        .reply_port      = fs_cmd_reply_port,
        .fs_consumer_id  = consumer_id,
        .filesystem_id   = file->filesystem_id,
        .new_consumer_id = new_consumer_id,
    };

    // Send the IPC_Dup message to the filesystem daemon
    int result = send_message_port(file->fs_port, sizeof(request), (const char *)&request);
    if (result != 0) {
        errno = -result;
        return -2;
    }

    // Receive the reply message containing the result
    Message_Descriptor reply_descr;
    IPC_Generic_Msg *reply_msg;
    result = get_message(&reply_descr, (unsigned char **)&reply_msg, fs_cmd_reply_port, NULL, NULL);
    if (result != SUCCESS) {
        // Handle error: Failed to receive the reply message
        errno = -result;
        return -2;
    }

    // Verify that the reply message is of type IPC_Open_Reply
    if (reply_msg->type != IPC_Dup_Reply_NUM) {
        // Handle error: Unexpected reply message type
        free(reply_msg);
        errno = EIO;
        return -2;
    }

    // Read the result code from the reply message
    IPC_Dup_Reply *reply = (IPC_Dup_Reply *)reply_msg;
    if (reply->result_code < 0) {
        errno = -reply->result_code;
        free(reply_msg);
        return -1;
    }

    struct File *new_file = (struct File *)new_data;

    new_file->file_id       = reply->file_id;
    new_file->filesystem_id = file->filesystem_id;
    new_file->fs_port       = file->fs_port;

    // Free the memory for the reply message
    free(reply_msg);

    return 0;
}

int __file_fstat(void *file_data, uint64_t consumer_id, struct stat *statbuf)
{
    // Not yet implemented
    errno = ENOSYS; // Function not implemented
    return -1;
}

int __file_isseekable(void *, uint64_t)
{
    // TODO: This is an assumption. Redo it once more sophisticated file types are introduced
    return 1;
}

int __file_isatty(void *, uint64_t) { return 0; }

// ssize_t __file_filesize(void * file_data, uint64_t consumer_id) {
//     struct File *file = (struct File *)file_data;

//     // Initialize the message type, flags, file ID, offset, count, and reply channel
//     IPC_Filesize message = {
//         .type = IPC_Filesize_NUM,
//         .flags = 0,
//         .file_id = file->file_id,
//         .fs_consumer_id = consumer_id,
//         .filesystem_id = file->filesystem_id,
//         .reply_port = fs_cmd_reply_port,
//     };

//     size_t message_size = sizeof(IPC_Filesize);

//     // Send the IPC_Filesize message to the filesystem daemon
//     result_t result = send_message_port(file->fs_port, message_size, (const char *)&message);

//     if (result != SUCCESS) {
//         errno = EIO; // I/O error
//         return -1;
//     }

//     // Receive the reply message containing the result and data
//     Message_Descriptor reply_descr;
//     IPC_Generic_Msg * reply_msg;
//     result = get_message(&reply_descr, (unsigned char **)&reply_msg, fs_cmd_reply_port);
//     if (result != SUCCESS) {
//         errno = EIO; // I/O error
//         return -1;
//     }

//     // Verify that the reply message is of type IPC_Filesize_Reply
//     if (reply_msg->type != IPC_Filesize_Reply_NUM) {
//         errno = EIO; // I/O error
//         return -1;
//     }

//     // Read the result code from the reply message
//     IPC_Filesize_Reply *reply = (IPC_Filesize_Reply *)reply_msg;
//     int result_code = reply->result_code;

//     if (result_code < 0) {
//         free(reply_msg);
//         errno = -result_code; // Set errno to appropriate error code (negative value)
//         return -1;
//     }

//     ssize_t count = 0;

//     count = reply->size;

//     // Free the memory for the reply message
//     free(reply_msg);

//     return count;
// }

ssize_t __file_filesize(void *file_data, uint64_t consumer_id)
{
    // Not yet implemented
    errno = ENOSYS; // Function not implemented
    return -1;
}

int __file_close(void *file_data, uint64_t consumer_id)
{
    struct File *file = (struct File *)file_data;

    IPC_Close message = {
        .type           = IPC_Close_NUM,
        .flags          = 0,
        .fs_consumer_id = consumer_id,
        .filesystem_id  = file->filesystem_id,
        .file_id        = file->file_id,
    };

    // Send the IPC_Close message to the filesystem daemon
    result_t k_result = send_message_port(file->fs_port, sizeof(message), (const char *)&message);
    if (k_result != SUCCESS) {
        // Failed to send the IPC_Close message
        errno = -k_result;
        return -1;
    }

    return 0;
}

void __file_free(void *, uint64_t)
{
    // No dynamic memory associated with the file data so nothing to do here
}

int __open_file(const char *path, int flags, mode_t mode, void *file_data, uint64_t consumer_id)
{
    struct File *file = (struct File *)file_data;

    // TODO: Mode is ignored for now

    pmos_right_t right = get_fs_right();
    if (!right) {
        errno = EIO;
        return -1;
    }

    uint64_t fs_cmd_reply_port = prepare_reply_port();
    if (fs_cmd_reply_port == INVALID_PORT) {
        errno = EIO;
        return -1;
    }

    // Calculate the required size for the IPC_Open message (including the variable-sized path and
    // excluding NULL character)
    size_t path_length  = strlen(path);
    size_t message_size = sizeof(IPC_Open) + path_length;

    // Allocate memory for the IPC_Open message
    IPC_Open *message = malloc(message_size);
    if (message == NULL) {
        // Handle memory allocation error
        errno = ENOMEM; // Set errno to appropriate error code
        return -1;
    }

    // Set the message type, flags, and reply port
    message->num = IPC_Open_NUM;
    message->flags =
        IPC_FLAG_REGISTER_IF_NOT_FOUND; // Set appropriate flags based on the 'flags' argument
    message->fs_consumer_id = consumer_id;

    // Copy the path string into the message
    memcpy(message->path, path, path_length);

    // Send the IPC_Open message to the filesystem daemon
    right_request_t result =
        send_message_right(right, fs_cmd_reply_port, message, message_size, NULL, 0);

    int fail_count = 0;
    while (result.result == -ESRCH && fail_count < 5) {
        // Request the port of the filesystem daemon
        fs_right_invalid(right);
        if ((right = get_fs_right())) {
            // Handle error: Failed to obtain the filesystem port
            free(message); // Free the allocated message memory
            errno = EIO;   // Set errno to appropriate error code
            return -1;
        }

        // Retry sending IPC_Open message to the filesystem daemon
        result = send_message_right(right, fs_cmd_reply_port, message, message_size, NULL, 0);
        ++fail_count;
    }

    free(message); // Free the allocated message memory

    if (result.result != SUCCESS) {
        errno = EIO; // Set errno to appropriate error code
        return -1;
    }

    // Receive the reply message containing the result
    Message_Descriptor reply_descr;
    IPC_Generic_Msg *reply_msg;
    result_t get_result =
        get_message(&reply_descr, (unsigned char **)&reply_msg, fs_cmd_reply_port, NULL, NULL);
    if (get_result != SUCCESS) {
        errno = EIO; // Set errno to appropriate error code
        return -1;
    }

    // Verify that the reply message is of type IPC_Open_Reply
    if (reply_msg->type != IPC_Open_Reply_NUM) {
        free(reply_msg);
        errno = EIO;
        return -1;
    }

    // Read the result code from the reply message
    IPC_Open_Reply *reply = (IPC_Open_Reply *)reply_msg;
    int result_code       = reply->result_code;

    if (result_code < 0) {
        free(reply_msg);
        errno = -result_code;
        return -1;
    }

    file->file_id       = reply->file_id;
    file->filesystem_id = reply->filesystem_id;
    file->fs_port       = reply->fs_port;

    free(reply_msg);

    return 0;
}

int vfsd_send_persistant(size_t msg_size, const void *message)
{
    struct uthread *current = __get_tls();
    if (!current)
        return -1;

    pmos_right_t right = get_fs_right();
    if (!right) {
        errno = EIO;
        return -1;
    }

    uint64_t fs_cmd_reply_port = prepare_reply_port();
    if (fs_cmd_reply_port == INVALID_PORT) {
        errno = EIO;
        return -1;
    }

    right_request_t result =
        send_message_right(right, fs_cmd_reply_port, message, msg_size, NULL, 0);

    int fail_count = 0;
    while (result.result == -ESRCH && fail_count < 5) {
        // Request the port of the filesystem daemon
        fs_right_invalid(right);
        if ((right = get_fs_right())) {
            errno = EIO;
            return -1;
        }

        result = send_message_right(right, fs_cmd_reply_port, message, msg_size, NULL, 0);
        ++fail_count;
    }

    return 0;
}

pmos_right_t get_pipe_right()
{
    struct uthread *ut = __get_tls();

    static const char pipe_port_name[] = "/pmos/piped";
    if (ut->pipe_right == INVALID_RIGHT) {
        right_request_t right_req = get_right_by_name(pipe_port_name, strlen(pipe_port_name), 0);
        if (right_req.result != SUCCESS) {
            // Handle error
            return INVALID_RIGHT;
        }

        ut->pipe_right = right_req.right;
    }

    return ut->pipe_right;
}

int __create_pipe(void *file_data, uint64_t consumer_id)
{
    union File_Data *file = file_data;

    pmos_port_t reply_port = prepare_reply_port();
    if (reply_port == INVALID_PORT) {
        return -1;
    }

    pmos_right_t pipe_right = get_pipe_right();
    if (pipe_right == INVALID_PORT) {
        return -1;
    }

    IPC_Pipe_Open message = {
        .type           = IPC_Pipe_Open_NUM,
        .flags          = 0,
        .fs_consumer_id = consumer_id,
    };

    result_t k_result =
        send_message_right(pipe_right, reply_port, (const char *)&message, sizeof(message), NULL, 0)
            .result;
    if (k_result != SUCCESS) {
        errno = -k_result;
        return -1;
    }

    Message_Descriptor reply_descr;
    IPC_Generic_Msg *reply_msg;
    k_result = get_message(&reply_descr, (unsigned char **)&reply_msg, reply_port, NULL, NULL);
    if (k_result != SUCCESS) {
        errno = -k_result;
        return -1;
    }

    if (reply_msg->type != IPC_Pipe_Open_Reply_NUM) {
        fprintf(stderr,
                "pmOS libC: Unexpected reply message type: %x task %lx sender %lx port %lx\n",
                reply_msg->type, get_task_id(), reply_descr.sender, reply_port);
        errno = EIO;
        free(reply_msg);
        return -1;
    }

    IPC_Pipe_Open_Reply *reply = (IPC_Pipe_Open_Reply *)reply_msg;
    if (reply->result_code < 0) {
        errno = -reply->result_code;
        free(reply_msg);
        return -1;
    }

    file[0].file.file_id       = reply->reader_id;
    file[0].file.filesystem_id = reply->filesystem_id;
    file[0].file.fs_port       = reply->pipe_port;

    file[1].file.file_id       = reply->writer_id;
    file[1].file.filesystem_id = reply->filesystem_id;
    file[1].file.fs_port       = reply->pipe_port;

    free(reply_msg);
    return 0;
}