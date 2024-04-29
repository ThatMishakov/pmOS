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

#include "file_op.h"
#include "string.h"
#include "fs_consumer.h"
#include <pmos/ipc.h>
#include <stdlib.h>
#include <errno.h>
#include "path_node.h"
#include <pmos/system.h>
#include <string.h>
#include "filesystem.h"
#include <assert.h>
#include <stdio.h>

static int ipc_dup_send_fail(pmos_port_t reply_port, int result);

extern pmos_port_t main_port;

uint64_t next_open_request_id = 1;

struct File_Request_Map global_requests_map = {};

uint64_t get_next_request_id()
{
    return __atomic_fetch_add(&next_open_request_id, 1, __ATOMIC_SEQ_CST);
}

int open_file_send_fail(const struct IPC_Open *request, int error)
{
    IPC_Open_Reply reply = {
        .type = IPC_Open_Reply_NUM,
        .result_code = -error,
        .fs_flags = 0,
        .filesystem_id = 0,
        .file_id = 0,
        .fs_port = 0,
    };

    result_t result = send_message_port(request->reply_port, sizeof(reply), (char *)&reply);
    return -result;
}

int init_open_request(struct File_Request *request)
{
    if (request == NULL) {
        return -1;
    }

    int result = init_null_string(&request->path);
    if (result != 0) {
        return result;
    }

    request->path_node_next = NULL;
    request->path_node_prev = NULL;

    request->consumer_req_next = NULL;
    request->consumer_req_prev = NULL;
    request->consumer = NULL;

    request->fs_req_next = NULL;
    request->fs_req_prev = NULL;
    request->filesystem = NULL;

    request->reply_port = 0;
    
    

    request->next_path_node_index = 0;
    request->active_node = NULL;

    request->request_id = get_next_request_id();
    request->request_type = REQUEST_TYPE_OPEN_FILE;

    return 0;
}

struct File_Request *create_mount_request()
{
    struct File_Request *new_request = calloc(sizeof(struct File_Request), 1);
    if (new_request == NULL)
        return NULL;

    int result = init_null_string(&new_request->path);
    if (result != 0) {
        free(new_request);
        return NULL;
    }

    new_request->request_id = get_next_request_id();
    new_request->request_type = REQUEST_TYPE_MOUNT;

    return new_request;
}

void free_buffers_file_request(struct File_Request *request)
{
    if (request == NULL) {
        return;
    }

    destroy_string(&request->path);
}

int open_file(const struct IPC_Open *request, uint64_t sender, uint64_t request_length)
{
    if (request == NULL)
        return -1;

    // Check if the sender is a member of the task group
    syscall_r res = is_task_group_member(sender, request->fs_consumer_id);
    if (res.result != SUCCESS || !res.value) {
        open_file_send_fail(request, EPERM);
        return 0;
    }

    bool register_if_not_found = request->flags & IPC_FLAG_REGISTER_IF_NOT_FOUND;

    uint64_t filesystem_id = request->fs_consumer_id;
    struct fs_consumer *consumer = get_fs_consumer(filesystem_id);
    if (consumer == NULL && register_if_not_found) {
        consumer = create_fs_consumer(request->fs_consumer_id);
        if (consumer == NULL) {
            open_file_send_fail(request, ENOMEM);
            return 0;
        }
    } else if (consumer == NULL) {
        open_file_send_fail(request, ENOENT);
        return 0;
    }

    struct File_Request *open_request = malloc(sizeof(struct File_Request));
    if (open_request == NULL) {
        open_file_send_fail(request, ENOMEM);
        return 0;
    }

    int result = init_open_request(open_request);
    if (result != 0) {
        open_file_send_fail(request, -result);
        free(open_request);
        return 0;
    }

    if (request_length < sizeof(struct IPC_Open)) {
        free_buffers_file_request(open_request);
        free(open_request);
        return 0;
    }
    uint64_t path_length = request_length - sizeof(struct IPC_Open);

    result = prepare_filename(open_request, consumer->path.data, consumer->path.capacity, request->path, path_length);
    if (result != 0) {
        open_file_send_fail(request, -result);
        free_buffers_file_request(open_request);
        free(open_request);
        return 0;
    }

    open_request->reply_port = request->reply_port;

    result = bind_request_to_root(open_request);
    if (result != 0) {
        open_file_send_fail(request, -result);
        free_buffers_file_request(open_request);
        free(open_request);
        return 0;
    }

    result = register_request_with_consumer(open_request, consumer);
    if (result != 0) {
        remove_request_from_path_node(open_request);
        open_file_send_fail(request, -result);
        free_buffers_file_request(open_request);
        free(open_request);
        return 0;
    }

    return process_request(open_request);
}

int mount_filesystem(const struct IPC_Mount_FS *request, uint64_t sender, uint64_t request_length)
{
    if (request == NULL)
        return -EINVAL;

    uint64_t filesystem_id = request->filesystem_id;
    struct Filesystem *filesystem = get_filesystem(filesystem_id);
    if (filesystem == NULL) {
        IPC_Mount_FS_Reply reply = {
            .type = IPC_Mount_FS_Reply_NUM,
            .result_code = -ENOENT,
            .mountpoint_id = 0,
        };

        result_t result = send_message_port(request->reply_port, sizeof(reply), (char *)&reply);
        return -result;
    }

    struct fs_task *task = get_task_from_filesystem(filesystem, sender);
    if (task == NULL) {
        // Task is not registered with the filesystem
        IPC_Mount_FS_Reply reply = {
            .type = IPC_Mount_FS_Reply_NUM,
            .result_code = -EPERM,
            .mountpoint_id = 0,
        };

        result_t result = send_message_port(request->reply_port, sizeof(reply), (char *)&reply);
        return -result;
    }

    struct File_Request *mount_request = create_mount_request();
    if (request == NULL) {
        IPC_Mount_FS_Reply reply = {
            .type = IPC_Mount_FS_Reply_NUM,
            .result_code = -ENOMEM,
            .mountpoint_id = 0,
        };

        result_t result = send_message_port(request->reply_port, sizeof(reply), (char *)&reply);
        return -result;
    }

    mount_request->reply_port = request->reply_port;

    size_t path_length = request_length - sizeof(struct IPC_Mount_FS);
    const char * fs_path = "/";
    int result = prepare_filename(mount_request, fs_path, strlen(fs_path), request->mount_path, path_length);
    if (result != 0) {
        free_buffers_file_request(mount_request);
        free(mount_request);

        IPC_Mount_FS_Reply reply = {
            .type = IPC_Mount_FS_Reply_NUM,
            .result_code = -result,
            .mountpoint_id = 0,
        };

        result_t result = send_message_port(request->reply_port, sizeof(reply), (char *)&reply);
        return -result;
    }

    result = register_request_with_filesystem(mount_request, filesystem);
    if (result != 0) {
        free_buffers_file_request(mount_request);
        free(mount_request);

        IPC_Mount_FS_Reply reply = {
            .type = IPC_Mount_FS_Reply_NUM,
            .result_code = -result,
            .mountpoint_id = 0,
        };

        result_t result = send_message_port(request->reply_port, sizeof(reply), (char *)&reply);
        return -result;
    }

    return process_request(mount_request);
}

int bind_request_to_root(struct File_Request *request)
{
    if (request == NULL)
        return -EINVAL;

    if (request->path.data == NULL)
        return -EINVAL;

    int result = add_request_to_path_node(request, &root);
    if (result != 0)
        return result;

    request->active_node = &root;
    request->next_path_node_index = 0;

    return 0;
}

int register_request_with_consumer(struct File_Request *request, struct fs_consumer *consumer)
{
    if (request == NULL || consumer == NULL)
        return -EINVAL;

    request->consumer = consumer;

    // Push request to consumer's open requests list
    if (consumer->requests_head == NULL) {
        consumer->requests_head = request;
        consumer->requests_tail = request;

        request->consumer_req_prev = NULL;
        request->consumer_req_next = NULL;
    } else {
        request->consumer_req_prev = consumer->requests_tail;
        request->consumer_req_next = NULL;
        consumer->requests_tail = request;
    }
    consumer->requests_count++;

    return 0;
}

int register_request_with_filesystem(struct File_Request *request, struct Filesystem *filesystem)
{
    if (request == NULL || filesystem == NULL)
        return -EINVAL;

    assert("Request already registsered with filesystem" && request->filesystem == NULL);

    request->filesystem = filesystem;

    if (filesystem->requests_head == NULL) {
        filesystem->requests_head = request;
        filesystem->requests_tail = request;

        request->fs_req_prev = NULL;
        request->fs_req_next = NULL;
    } else {
        request->fs_req_prev = filesystem->requests_tail;
        request->fs_req_next = NULL;
        filesystem->requests_tail = request;
    }
    filesystem->requests_count++;

    return 0;
}

int prepare_filename(struct File_Request *request, const char *consumer_path_data, size_t consumer_path_length, const char *file_path, size_t path_length) {
    if (request == NULL) {
        return -EINVAL;
    }

    if (path_length == 0) {
        // Empty file path, use consumer's path
        if (consumer_path_length == 0) {
            // Consumer's path is also empty, return an error
            return -EINVAL; // Invalid argument
        }

        // Clear the existing contents of request->path
        erase_string(&request->path, 0, request->path.length);

        if (append_string(&request->path, consumer_path_data, consumer_path_length) == -1)
            return -ENOMEM; // Failed to append consumer's path
    } else if (path_length > 0 && file_path[0] == '/') {
        // Absolute path, copy as-is
        // Clear the existing contents of request->path
        erase_string(&request->path, 0, request->path.length);

        if (append_string(&request->path, file_path, path_length) == -1)
            return -ENOMEM; // Failed to append string
    } else {
        // Relative path, append to consumer's path

        // Clear the existing contents of request->path
        erase_string(&request->path, 0, request->path.length);

        if (append_string(&request->path, consumer_path_data, consumer_path_length) == -1)
            return -ENOMEM; // Failed to append consumer's path

        if (consumer_path_length == 0 || consumer_path_data[consumer_path_length - 1] != '/') {
            if (append_string(&request->path, "/", 1) == -1)
                return -ENOMEM; // Failed to append directory separator
        }

        if (append_string(&request->path, file_path, path_length) == -1)
            return -ENOMEM; // Failed to append file path
    }

    return 0; // Success
}

static int destroy_and_reply_mount_request(struct File_Request * mount_request, int status_code, uint64_t mountpoint_id)
{
    assert(mount_request->request_type == REQUEST_TYPE_MOUNT);

    IPC_Mount_FS_Reply reply = {
        .type = IPC_Mount_FS_Reply_NUM,
        .result_code = status_code,
        .mountpoint_id = mountpoint_id
    };
    pmos_port_t reply_port = mount_request->reply_port;

    unregister_request_from_parent(mount_request);

    free_buffers_file_request(mount_request);
    free(mount_request);


    result_t result = send_message_port(reply_port, sizeof(reply), (char *)&reply);
    return -result;
}

int process_request(struct File_Request * request)
{
    struct Path_Node *active_node = request->active_node;
    if (active_node != NULL)
        remove_request_from_path_node(request);

    int result = 0;

    switch (request->request_type) {
    case REQUEST_TYPE_OPEN_FILE: {
        assert(active_node != NULL);

        switch (active_node->file_type) {
            case NODE_UNRESOLVED: {
                // Push request to the node's unresolved requests list
                result = add_request_to_path_node(request, active_node);
                if (result != 0) {
                    // Failed to register request
                    IPC_Open_Reply reply = {
                        .type = IPC_Open_Reply_NUM,
                        .result_code = result,
                        .fs_flags = 0,
                        .filesystem_id = 0,
                        .file_id = 0,
                        .fs_port = 0,
                    };

                    pmos_port_t reply_port = request->reply_port;
                    result = -send_message_port(reply_port, sizeof(reply), (char *)&reply);
                    break;
                }

                // Wait for the request to be resolved
                return 0;
            } break;
            case NODE_DIRECTORY: {
                // Advance to the node
                size_t path_index = request->next_path_node_index;
                while (path_index < request->path.length && request->path.data[path_index] == '/')
                    path_index++;

                if (path_index == request->path.length) {
                    // Reached the end of the path
                    // This is a directory and the file was wanted, so fail
                    IPC_Open_Reply reply = {
                        .type = IPC_Open_Reply_NUM,
                        .result_code = -EISDIR,
                        .fs_flags = 0,
                        .filesystem_id = 0,
                        .file_id = 0,
                        .fs_port = 0,
                    };

                    uint64_t reply_port = request->reply_port;
                    result = -send_message_port(reply_port, sizeof(reply), (char *)&reply);
                    break;
                }

                // Advance to the next node
                size_t next_index = path_index + 1;

                while (next_index < request->path.length && request->path.data[next_index] != '/')
                    next_index++;

                size_t node_name_length = next_index - path_index;

                if (node_name_length == 1 && memcmp(request->path.data + path_index, ".", 1) == 0) {
                    // Reference to the current directory
                    request->next_path_node_index = next_index;
                    result = add_request_to_path_node(request, active_node);
                    if (result != 0) {
                        IPC_Open_Reply reply = {
                            .type = IPC_Open_Reply_NUM,
                            .result_code = result,
                            .fs_flags = 0,
                            .filesystem_id = 0,
                            .file_id = 0,
                            .fs_port = 0,
                        };

                        result = -send_message_port(request->reply_port, sizeof(reply), (char *)&reply);
                        break;
                    }
                    
                    return process_request(request);
                } else if (node_name_length == 2 && memcmp(request->path.data + path_index, "..", 2) == 0) {
                    // Reference to a parent directory
                    assert(active_node->parent != NULL);

                    request->next_path_node_index = next_index;
                    result = add_request_to_path_node(request, active_node->parent);
                    if (result != 0) {
                        IPC_Open_Reply reply = {
                            .type = IPC_Open_Reply_NUM,
                            .result_code = -EISDIR,
                            .fs_flags = 0,
                            .filesystem_id = 0,
                            .file_id = 0,
                            .fs_port = 0,
                        };

                        result = -send_message_port(request->reply_port, sizeof(reply), (char *)&reply);
                        break;
                    }

                    return process_request(request);
                } else {
                    // Resolve the child node
                    Path_Node *child_node = NULL;
                    result = path_node_resolve_child(&child_node, active_node, (unsigned char *)request->path.data + path_index, node_name_length);
                    if (result != 0) {
                        IPC_Open_Reply reply = {
                            .type = IPC_Open_Reply_NUM,
                            .result_code = result,
                            .fs_flags = 0,
                            .filesystem_id = 0,
                            .file_id = 0,
                            .fs_port = 0,
                        };

                        result = -send_message_port(request->reply_port, sizeof(reply), (char *)&reply);
                        break;
                    }

                    request->next_path_node_index = next_index;
                    result = add_request_to_path_node(request, child_node);
                    if (result != 0) {
                        IPC_Open_Reply reply = {
                            .type = IPC_Open_Reply_NUM,
                            .result_code = result,
                            .fs_flags = 0,
                            .filesystem_id = 0,
                            .file_id = 0,
                            .fs_port = 0,
                        };

                        result = -send_message_port(request->reply_port, sizeof(reply), (char *)&reply);
                        break;
                    }

                    return process_request(request);
                }
            } break;
            case NODE_FILE: {
                // Reached the end of the path
                if (request->next_path_node_index != request->path.length) {
                    // This is a file and the directory was wanted, so fail
                    IPC_Open_Reply reply = {
                        .type = IPC_Open_Reply_NUM,
                        .result_code = -ENOENT,
                        .fs_flags = 0,
                        .filesystem_id = 0,
                        .file_id = 0,
                        .fs_port = 0,
                    };

                    result = -send_message_port(request->reply_port, sizeof(reply), (char *)&reply);
                    break;
                }

                result = register_request_with_filesystem(request, active_node->owner_mountpoint->fs);
                if (result != 0) {
                    IPC_Open_Reply reply = {
                        .type = IPC_Open_Reply_NUM,
                        .result_code = result,
                        .fs_flags = 0,
                        .filesystem_id = 0,
                        .file_id = 0,
                        .fs_port = 0,
                    };

                    result = -send_message_port(request->reply_port, sizeof(reply), (char *)&reply);
                    break;
                }

                request->request_type = REQUEST_TYPE_OPEN_FILE_RESOLVED;
                result = add_request_to_map(&global_requests_map, request);
                if (result != 0) {
                    IPC_Open_Reply reply = {
                        .type = IPC_Open_Reply_NUM,
                        .result_code = result,
                        .fs_flags = 0,
                        .filesystem_id = 0,
                        .file_id = 0,
                        .fs_port = 0,
                    };

                    result = -send_message_port(request->reply_port, sizeof(reply), (char *)&reply);
                    break;
                }

                IPC_FS_Open msg_request = {
                    .type = IPC_FS_Open_NUM,
                    .flags = 0,
                    .reply_port = main_port,
                    .fs_consumer_id = request->consumer->id,
                    .file_id = active_node->file_id,
                    .operation_id = request->request_id,
                };
                    
                result = -send_message_port(active_node->owner_mountpoint->fs->command_port, sizeof(msg_request), (char *)&msg_request);
                if (result == 0)
                    return 0;

                break;
            } break;
                default: {
                // Not implemented

                IPC_Open_Reply reply = {
                    .type = IPC_Open_Reply_NUM,
                    .result_code = -ENOSYS,
                    .fs_flags = 0,
                    .filesystem_id = 0,
                    .file_id = 0,
                    .fs_port = 0,
                };

                pmos_port_t reply_port = request->reply_port;
                result = -send_message_port(reply_port, sizeof(reply), (char *)&reply);
                break;
            } break;
        }
        
        break;
    }
    case REQUEST_TYPE_MOUNT: {
        bool is_root = request->path.length == 1 && request->path.data[0] == '/';
        if (!is_root)
            // Mounting is only supported on the root directory. Fail for a bogus reason
            return destroy_and_reply_mount_request(request, -ENOSYS, 0);

        // TODO: Walk the tree to find the actual mountpoint.

        // Bind to root node
        if (active_node == NULL) {
            active_node = &root;
            request->path_node_next = 0;
        }
        uint64_t file_id = request->file_id;

        // Root node is a special case: it's assumed it's always present, so if no root filesystem is mounted,
        // its status is unresolved.
        if (active_node->file_type != NODE_UNRESOLVED) {
            // Root is already mounted
            return destroy_and_reply_mount_request(request, -EEXIST, 0);
        }

        struct fs_mountpoint *mountpoint = create_mountpoint(request->filesystem, active_node);
        if (mountpoint == NULL) {
            return destroy_and_reply_mount_request(request, -ENOMEM, 0);
        }

        int result = destroy_and_reply_mount_request(request, 0, mountpoint->mountpoint_id);
        if (result != 0) {
            destroy_mountpoint(mountpoint, true);
            return result;
        }

        active_node->owner_mountpoint = mountpoint;
        active_node->file_type = NODE_DIRECTORY;
        active_node->file_id = file_id;

        return process_requests_of_node(active_node);
    }
    case REQUEST_TYPE_RESOLVE_PATH:
        // Ignore it
        result = 0;
        break;
    default: {
        result = -ENOSYS;
        break;
    }
    }

    remove_request_from_global_map(request);

    unregister_request_from_parent(request);
    
    free_buffers_file_request(request);
    free(request);

    return result;
}

int process_requests_of_node(struct Path_Node *node)
{
    if (node == NULL)
        return -EINVAL;

    struct File_Request *current = NULL, *previous = NULL;
    while ((current = node->requests_head) != NULL) {
        // When processing, requests must always remove themselves from the list
        assert(previous == NULL || current != previous);

        process_request(current);
        previous = current;
    }

    return 0;
}

void fail_and_destroy_request(struct File_Request *request, int error_code)
{
    if (request == NULL)
        return;

    remove_request_from_path_node(request);

    switch (request->request_type) {
    case REQUEST_TYPE_OPEN_FILE: {
        IPC_Open_Reply reply = {
            .type = IPC_Open_Reply_NUM,
            .result_code = error_code,
            .fs_flags = 0,
            .filesystem_id = 0,
            .file_id = 0,
            .fs_port = 0,
        };

        pmos_port_t reply_port = request->reply_port;

        send_message_port(reply_port, sizeof(reply), (char *)&reply);
        break;
    }
    case REQUEST_TYPE_MOUNT: {
        IPC_Mount_FS_Reply reply = {
            .type = IPC_Mount_FS_Reply_NUM,
            .result_code = error_code,
            .mountpoint_id = 0
        };

        pmos_port_t reply_port = request->reply_port;

        send_message_port(reply_port, sizeof(reply), (char *)&reply);
        break;
    }
    case REQUEST_TYPE_OPEN_FILE_RESOLVED: {
        break;
    }
    case REQUEST_TYPE_RESOLVE_PATH: {
        // Ignore it
        break;
    }
    case REQUEST_TYPE_UNKNOWN: {
        // Do nothing
        break;
    }
    case REQUEST_TYPE_DUP: {
        ipc_dup_send_fail(request->reply_port, error_code);
        break;
    }
    }

    remove_request_from_global_map(request);

    unregister_request_from_parent(request);
    
    free_buffers_file_request(request);
    free(request);
}

void unregister_request_from_parent(struct File_Request *request)
{
    if (request == NULL)
        return;

    if (request->consumer != NULL) {
        struct fs_consumer *consumer = request->consumer;
        if (request->consumer_req_prev == NULL) {
            assert(consumer->requests_head == request);
            consumer->requests_head = request->consumer_req_next;
        } else {
            assert(consumer->requests_head != request && request->consumer_req_prev != NULL && request->consumer_req_prev->consumer_req_next == request);
            request->consumer_req_prev->consumer_req_next = request->consumer_req_next;
        }

        if (request->consumer_req_next == NULL) {
            assert(consumer->requests_tail == request);
            consumer->requests_tail = request->consumer_req_prev;
        } else {
            assert(consumer->requests_tail != request && request->consumer_req_next != NULL && request->consumer_req_next->consumer_req_prev == request);
            request->consumer_req_next->consumer_req_prev = request->consumer_req_prev;
        }

        request->consumer = NULL;
    }

    if (request->filesystem != NULL) {
        struct Filesystem *fs = request->filesystem;

        if (request->fs_req_prev == NULL) {
            assert(fs->requests_head == request);
            fs->requests_head = request->fs_req_next;
        } else {
            assert(fs->requests_head != request && request->fs_req_prev != NULL && request->fs_req_prev->fs_req_next == request);
            request->fs_req_prev->fs_req_next = request->fs_req_next;
        }

        if (request->fs_req_next == NULL) {
            assert(fs->requests_tail == request);
            fs->requests_tail = request->fs_req_prev;
        } else {
            assert(fs->requests_tail != request && request->fs_req_next != NULL && request->fs_req_next->fs_req_prev == request);
            request->fs_req_next->fs_req_prev = request->fs_req_prev;
        }

        request->filesystem = NULL;
    }
}

int add_request_to_path_node(struct File_Request *request, struct Path_Node *n)
{
    if (request == NULL || n == NULL)
        return -EINVAL;

    assert(request->active_node == NULL);

    request->path_node_next = NULL;
    request->path_node_prev = NULL;

    if (n->requests_head == NULL) {
        assert(n->requests_tail == NULL);
        n->requests_head = request;
        n->requests_tail = request;
    } else {
        assert(n->requests_tail != NULL);
        n->requests_tail->path_node_next = request;
        request->path_node_prev = n->requests_tail;
        n->requests_tail = request;
    }

    request->active_node = n;

    return 0;
}

void remove_request_from_global_map(struct File_Request *request)
{
    if (request == NULL)
        return;

    if (!is_request_in_global_map(request))
        return;

    remove_request_from_map(&global_requests_map, request);
}

bool is_request_in_global_map(struct File_Request *request)
{
    assert(request != NULL);

    switch (request->request_type) {
    case REQUEST_TYPE_OPEN_FILE:
    case REQUEST_TYPE_MOUNT:
        return false;
    case REQUEST_TYPE_DUP:
    case REQUEST_TYPE_RESOLVE_PATH:
    case REQUEST_TYPE_OPEN_FILE_RESOLVED:
    case REQUEST_TYPE_UNKNOWN:
        // Unknown requests might be in the map
        return true;
    }

    // Should never happen
    return true;
}

void remove_request_from_map(struct File_Request_Map *map, struct File_Request *request)
{
    if (map == NULL || request == NULL)
        return;

    if (map->hash_vector == NULL || map->vector_size == 0)
        return;

    size_t index = request->request_id % map->vector_size;
    struct File_Request_Node *node = map->hash_vector[index];
    while (node != NULL) {
        if (node->next != NULL && node->next->request == request) {
            struct File_Request_Node *next = node->next;
            node->next = next->next;
            free(next);
            map->nodes_count--;
            break;
        }
        node = node->next;
    }

    if (map->vector_size > FILE_REQUEST_HASH_INITIAL_SIZE && map->nodes_count < map->vector_size * FILE_REQUEST_HASH_SHRINK_THRESHOLD) {
        size_t new_size = map->vector_size / 2;
        struct File_Request_Node **new_vector = calloc(new_size, sizeof(struct File_Request_Node *));
        if (new_vector == NULL)
            return;

        for (size_t i = 0; i < map->vector_size; i++) {
            struct File_Request_Node *node = map->hash_vector[i];
            while (node != NULL) {
                struct File_Request_Node *next = node->next;
                size_t new_index = node->request->request_id % new_size;
                node->next = new_vector[new_index];
                new_vector[new_index] = node;
                node = next;
            }
        }

        free(map->hash_vector);
        map->hash_vector = new_vector;
        map->vector_size = new_size;
    }
}

struct File_Request *create_resolve_path_request(struct Path_Node *child_node)
{
    struct File_Request *new_request = calloc(1, sizeof(struct File_Request));
    if (new_request == NULL)
        return NULL;

    new_request->request_type = REQUEST_TYPE_RESOLVE_PATH;
    new_request->request_id = get_next_request_id();

    int result = add_request_to_map(&global_requests_map, new_request);
    if (result < 0) {
        free(new_request);
        return NULL;
    }

    result = register_request_with_filesystem(new_request, child_node->parent->owner_mountpoint->fs);
    if (result < 0) {
        remove_request_from_map(&global_requests_map, new_request);
        free(new_request);
        return NULL;
    }

    result = add_request_to_path_node(new_request, child_node);
    if (result < 0) {
        remove_request_from_map(&global_requests_map, new_request);
        unregister_request_from_parent(new_request);
        free(new_request);
        return NULL;
    }

    return new_request;
}

int add_request_to_map(struct File_Request_Map *map, struct File_Request *request)
{
    if (map == NULL || request == NULL)
        return -EINVAL;

    if (map->hash_vector == NULL || map->vector_size < FILE_REQUEST_HASH_INITIAL_SIZE || map->vector_size * FILE_REQUEST_HASH_LOAD_FACTOR < map->nodes_count) {
        size_t new_size = map->vector_size == 0 ? FILE_REQUEST_HASH_INITIAL_SIZE : map->vector_size * 2;
        struct File_Request_Node **new_vector = calloc(new_size, sizeof(struct File_Request_Node *));
        if (new_vector == NULL)
            return -ENOMEM;

        for (size_t i = 0; i < map->vector_size; i++) {
            struct File_Request_Node *node = map->hash_vector[i];
            while (node != NULL) {
                struct File_Request_Node *next = node->next;
                size_t new_index = node->request->request_id % new_size;
                node->next = new_vector[new_index];
                new_vector[new_index] = node;
                node = next;
            }
        }

        free(map->hash_vector);
        map->hash_vector = new_vector;
        map->vector_size = new_size;
    }

    struct File_Request_Node *new_node = calloc(1, sizeof(struct File_Request_Node));
    if (new_node == NULL)
        return -ENOMEM;

    size_t index = request->request_id % map->vector_size;
    new_node->next = map->hash_vector[index];
    new_node->request = request;
    map->hash_vector[index] = new_node;
    map->nodes_count++;

    return 0;
}

int react_resolve_path_reply(struct IPC_FS_Resolve_Path_Reply *message, size_t sender, uint64_t message_length)
{
    if (message == NULL)
        return -EINVAL;

    if (message_length < sizeof(struct IPC_FS_Resolve_Path_Reply))
        return -EINVAL;

    struct File_Request *request = get_request_from_global_map(message->operation_id);
    if (request == NULL) {
        printf("[VFSd] Warning: recieved IPC_FS_Resolve_Path_Reply from task %ld with unknown request ID %ld\n", sender, message->operation_id);
        return -ENOENT;
    }

    // Check that the sender is the part of the filesystem that we expect
    bool is_registered = is_task_registered_with_filesystem(request->filesystem, sender);
    if (!is_registered) {
        printf("[VFSd] Warning: recieved IPC_FS_Resolve_Path_Reply from task %ld that is not registered with the filesystem %ld\n", sender, request->filesystem->id);
        return -EPERM;
    }

    int result = message->result_code;
    if (result == 0) {
        if (!is_file_type_valid(message->file_type)) {
            // File type is not supported
            result = -ENOSYS;
            goto fail;
        }

        request->active_node->file_type = message->file_type;
        request->active_node->file_id = message->file_id;
        // TODO: File size

        struct Path_Node *node = request->active_node;

        remove_request_from_path_node(request);
        remove_request_from_map(&global_requests_map, request);
        unregister_request_from_parent(request);
        free_buffers_file_request(request);
        free(request);

        process_requests_of_node(node);
        return 0;
    }
fail:

    destroy_path_node_with_error(request->active_node, result);

    return 0;
}

int react_ipc_fs_open_reply(struct IPC_FS_Open_Reply *message, size_t sender, uint64_t message_length)
{
    if (message == NULL)
        return -EINVAL;

    if (message_length < sizeof(struct IPC_FS_Open_Reply))
        return -EINVAL;

    struct File_Request *request = get_request_from_global_map(message->operation_id);
    if (request == NULL) {
        printf("[VFSd] Warning: recieved IPC_FS_Open_Reply from task %ld with unknown request ID %ld\n", sender, message->operation_id);
        return -ENOENT;
    }

    if (request->request_type != REQUEST_TYPE_OPEN_FILE_RESOLVED) {
        printf("[VFSd] Warning: recieved IPC_FS_Open_Reply from task %ld with request ID %ld that is not of type REQUEST_TYPE_OPEN_FILE_RESOLVED\n", sender, message->operation_id);
        return -EINVAL;
    }

    // Check that the sender is the part of the filesystem that we expect
    bool is_registered = is_task_registered_with_filesystem(request->filesystem, sender);
    if (!is_registered) {
        printf("[VFSd] Warning: recieved IPC_FS_Open_Reply from task %ld that is not registered with the filesystem %ld\n", sender, request->filesystem->id);
        return -EPERM;
    }

    IPC_Open_Reply reply_msg = {
        .type = IPC_Open_Reply_NUM,
        .result_code = message->result_code,
        .file_id = message->file_id,
        .filesystem_id = message->result_code < 0 ? 0 : request->filesystem->id,
        .fs_port = message->file_port,
        .fs_flags = 0,
    };

    result_t send_result = send_message_port(request->reply_port, sizeof(reply_msg), (char *)&reply_msg);

    // TODO: Send Close if the file was oppened but the reply failed

    remove_request_from_map(&global_requests_map, request);
    unregister_request_from_parent(request);
    free_buffers_file_request(request);
    free(request);

    return 0;
}

int react_ipc_fs_dup_reply(struct IPC_FS_Dup_Reply *message, size_t sender, uint64_t message_length)
{
    if (message == NULL)
        return -EINVAL;

    if (message_length < sizeof(struct IPC_FS_Dup_Reply))
        return -EINVAL;

    struct File_Request *request = get_request_from_global_map(message->operation_id);
    if (request == NULL) {
        printf("[VFSd] Warning: recieved IPC_FS_Dup_Reply from task %ld with unknown request ID %ld\n", sender, message->operation_id);
        return -ENOENT;
    }

    if (request->request_type != REQUEST_TYPE_DUP) {
        printf("[VFSd] Warning: recieved IPC_FS_Dup_Reply from task %ld with request ID %ld that is not of type REQUEST_TYPE_DUP\n", sender, message->operation_id);
        return -EINVAL;
    }

    // Check that the sender is the part of the filesystem that we expect
    bool is_registered = is_task_registered_with_filesystem(request->filesystem, sender);
    if (!is_registered) {
        printf("[VFSd] Warning: recieved IPC_FS_Dup_Reply from task %ld that is not registered with the filesystem %ld\n", sender, request->filesystem->id);
        return -EPERM;
    }

    IPC_Dup_Reply reply_msg = {
        .type = IPC_Dup_Reply_NUM,
        .result_code = message->result_code,
        .file_id = message->result_code < 0 ? 0 : request->filesystem->id,
        .fs_port = message->file_port,
        .filesystem_id = message->result_code < 0 ? 0 : request->filesystem->id,
    };

    result_t send_result = send_message_port(request->reply_port, sizeof(reply_msg), (char *)&reply_msg);
    // TODO

    remove_request_from_map(&global_requests_map, request);
    unregister_request_from_parent(request);
    free_buffers_file_request(request);
    free(request);

    return 0;
}

struct File_Request *get_request_from_global_map(uint64_t id)
{
    return get_request_from_map(&global_requests_map, id);
}

struct File_Request *get_request_from_map(struct File_Request_Map *map, uint64_t id)
{
    if (map == NULL)
        return NULL;

    if (map->nodes_count == 0)
        return NULL;

    assert(map->hash_vector != NULL && map->vector_size > 0);

    size_t index = id % map->vector_size;
    struct File_Request_Node *node = map->hash_vector[index];
    while (node != NULL) {
        if (node->request->request_id == id)
            return node->request;
        node = node->next;
    }

    return NULL;
}

static int ipc_dup_send_fail(pmos_port_t reply_port, int result)
{
    IPC_Dup_Reply reply = {
        .type = IPC_Dup_Reply_NUM,
        .result_code = result,
        .file_id = 0,
        .fs_port = 0,
        .filesystem_id = 0,
    };

    return -send_message_port(reply_port, sizeof(reply), (char *)&reply);
}

int react_ipc_dup(struct IPC_Dup *message, size_t sender, uint64_t message_length)
{
    if (message == NULL)
        return -EINVAL;

    if (message_length < sizeof(struct IPC_Dup))
        return -EINVAL;

    // Check if the sender is a member of the task group
    syscall_r res = is_task_group_member(sender, message->fs_consumer_id);
    if (res.result != SUCCESS || !res.value) {
        return ipc_dup_send_fail(message->reply_port, -EPERM);
    }
    res = is_task_group_member(sender, message->new_consumer_id);
    if (res.result != SUCCESS || !res.value) {
        return ipc_dup_send_fail(message->reply_port, -EPERM);
    }

    bool create_new = message->flags & IPC_FLAG_REGISTER_IF_NOT_FOUND;

    struct fs_consumer *from_consumer = get_fs_consumer(message->fs_consumer_id);
    if (from_consumer == NULL)
        // If the consumer was not registered, it has no files open
        return ipc_dup_send_fail(message->reply_port, -ENOENT);

    struct fs_consumer *to_consumer = get_fs_consumer(message->new_consumer_id);
    if (to_consumer == NULL && create_new) {
        to_consumer = create_fs_consumer(message->new_consumer_id);
        if (to_consumer == NULL)
            return ipc_dup_send_fail(message->reply_port, -ENOMEM);
    } else if (to_consumer == NULL)
        return ipc_dup_send_fail(message->reply_port, -EINVAL);

    struct Filesystem *fs = get_filesystem(message->filesystem_id);
    if (fs == NULL)
        return ipc_dup_send_fail(message->reply_port, -EINVAL);

    struct File_Request *request = calloc(1, sizeof(struct File_Request));
    if (request == NULL)
        return ipc_dup_send_fail(message->reply_port, -errno);

    request->request_type = REQUEST_TYPE_DUP;
    request->request_id = get_next_request_id();
    request->reply_port = message->reply_port;

    int result = add_request_to_map(&global_requests_map, request);
    if (result < 0) {
        free(request);
        return ipc_dup_send_fail(message->reply_port, result);
    }

    result = register_request_with_consumer(request, to_consumer);
    if (result < 0) {
        remove_request_from_map(&global_requests_map, request);
        free(request);
        return ipc_dup_send_fail(message->reply_port, result);
    }

    result = register_request_with_filesystem(request, fs);
    if (result < 0) {
        remove_request_from_map(&global_requests_map, request);
        unregister_request_from_parent(request);
        free(request);
        return ipc_dup_send_fail(message->reply_port, result);
    }

    IPC_FS_Dup msg_request = {
        .type = IPC_FS_Dup_NUM,
        .flags = message->flags,
        .reply_port = main_port,
        .file_id = message->file_id,
        .fs_consumer_id = from_consumer->id,
        .new_consumer_id = to_consumer->id,
        .operation_id = request->request_id,
        .filesystem_id = fs->id,
    };

    result = -send_message_port(fs->command_port, sizeof(msg_request), (char *)&msg_request);
    if (result == 0)
        return 0;
    else {
        remove_request_from_map(&global_requests_map, request);
        unregister_request_from_parent(request);
        free(request);
        return ipc_dup_send_fail(message->reply_port, result);
    }
}

int react_ipc_close(struct IPC_Close *message, size_t sender, uint64_t message_length)
{
    if (message == NULL)
        return -EINVAL;

    if (message_length < sizeof(struct IPC_Close))
        return -EINVAL;

    struct fs_consumer *consumer = get_fs_consumer(message->fs_consumer_id);
    if (consumer == NULL)
        // It's an error, but the reply is not sent
        return 0;

    bool is_consumer = is_fs_consumer(consumer, sender);
    if (!is_consumer) {
        printf("[VFSd] Warning: recieved IPC_Close from task %ld that is not registered with the consumer %ld\n", sender, consumer->id);
        return 0;
    }

    struct Filesystem *fs = get_filesystem(message->filesystem_id);
    if (fs == NULL)
        // It's an error, but the reply is not sent
        return 0;

    IPC_FS_Close close_message = {
        .type = IPC_FS_Close_NUM,
        .flags = message->flags,
        .filesystem_id = message->filesystem_id,
        .file_id = message->file_id,
    };

    return -send_message_port(fs->command_port, sizeof(close_message), (char *)&close_message);
}