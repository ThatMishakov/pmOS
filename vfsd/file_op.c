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

uint64_t next_open_request_id = 1;

uint64_t get_next_open_request_id()
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

    request->reply_port = 0;
    
    

    request->active_file_index = 0;
    request->active_node = NULL;

    request->request_id = get_next_open_request_id();
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

    new_request->request_id = get_next_open_request_id();
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

    uint64_t filesystem_id = request->fs_consumer_id;
    struct fs_consumer *consumer = get_fs_consumer(filesystem_id);
    if (consumer == NULL) {
        open_file_send_fail(request, ENOENT);
        return 0;
    }

    bool is_consumer = is_fs_consumer(consumer, sender);
    if (!is_consumer) {
        open_file_send_fail(request, EPERM);
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

    result = register_request_with_consumer(open_request, consumer);
    if (result != 0) {
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

    mount_request->filesystem = filesystem;

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

extern struct Path_Node * root_node;

int bind_request_to_root(struct File_Request *request)
{
    if (request == NULL)
        return -EINVAL;

    struct Path_Node *root = root_node;
    if (root == NULL)
        return -ENOENT;

    if (request->path.data == NULL)
        return -EINVAL;

    request->active_node = root;
    request->active_file_index = 0;

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

    request->filesystem = filesystem;

        if (filesystem->requests_head == NULL) {
        filesystem->requests_head = request;
        filesystem->requests_tail = request;

        request->consumer_req_prev = NULL;
        request->consumer_req_next = NULL;
    } else {
        request->consumer_req_prev = filesystem->requests_tail;
        request->consumer_req_next = NULL;
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
    // Assert is not yet implemented
    // assert(mount_request->request_type == REQUEST_TYPE_MOUNT);

    IPC_Mount_FS_Reply reply = {
        .type = IPC_Mount_FS_Reply_NUM,
        .result_code = status_code,
        .mountpoint_id = mountpoint_id
    };
    pmos_port_t reply_port = mount_request->reply_port;

    free_buffers_file_request(mount_request);
    free(mount_request);


    result_t result = send_message_port(reply_port, sizeof(reply), (char *)&reply);
    return -result;
}

int process_request(struct File_Request * request)
{
    switch (request->request_type) {
    case REQUEST_TYPE_OPEN_FILE: {
        // Not yet implemented. Fail for a bogus reason

        IPC_Open_Reply reply = {
            .type = IPC_Open_Reply_NUM,
            .result_code = -ENOSYS,
            .fs_flags = 0,
            .filesystem_id = 0,
            .file_id = 0,
            .fs_port = 0,
        };

        pmos_port_t reply_port = request->reply_port;


        free_buffers_file_request(request);
        free(request);
        return -send_message_port(reply_port, sizeof(reply), (char *)&reply);
    }
    case REQUEST_TYPE_MOUNT: {
        bool is_root = request->path.length == 1 && request->path.data[0] == '/';
        if (!is_root)
            // Mounting is only supported on the root directory. Fail for a bogus reason
            return destroy_and_reply_mount_request(request, -ENOSYS, 0);

        // TODO: Walk the tree to find the actual mountpoint.

        // Bind to root node
        if (request->active_node == NULL) {
            int result = bind_request_to_root(request);
            if (result != 0)
                return destroy_and_reply_mount_request(request, result, 0);
        }

        Path_Node *n = request->active_node;
        uint64_t file_id = request->file_id;

        // Root node is a special case: it's assumed it's always present, so if no root filesystem is mounted,
        // its status is unresolved.
        if (request->active_node->file_type != NODE_UNRESOLVED) {
            // Root is already mounted
            return destroy_and_reply_mount_request(request, -EEXIST, 0);
        }

        struct fs_mountpoint *mountpoint = create_mountpoint(request->filesystem, request->active_node);
        if (mountpoint == NULL) {
            return destroy_and_reply_mount_request(request, -ENOMEM, 0);
        }

        int result = destroy_and_reply_mount_request(request, 0, mountpoint->mountpoint_id);
        if (result != 0) {
            destroy_mountpoint(mountpoint, true);
            return result;
        }

        n->owner_mountpoint = mountpoint;
        n->file_type = NODE_DIRECTORY;
        n->file_id = file_id;

        return process_requests_of_node(n);
    }
    default: {
        return -ENOSYS;
    }
    }
}

int process_requests_of_node(struct Path_Node *node)
{
    if (node == NULL)
        return -EINVAL;

    struct File_Request *current = NULL, *previous = NULL;
    while ((current = node->requests_head) != NULL) {
        // When processing, requests must always remove themselves from the list
        // assert(previous == NULL || current != previous);

        process_request(current);
        previous = current;
    }

    return 0;
}
