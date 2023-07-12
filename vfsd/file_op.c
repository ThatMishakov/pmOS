#include "file_op.h"
#include "string.h"
#include "fs_consumer.h"
#include <pmos/ipc.h>
#include <stdlib.h>
#include <errno.h>

uint64_t next_open_request_id = 1;

uint64_t get_next_open_request_id()
{
    return __atomic_fetch_add(&next_open_request_id, 1, __ATOMIC_SEQ_CST);
}

void open_file_send_fail(const struct IPC_Open *request, int error);

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

    struct Open_Request *open_request = malloc(sizeof(struct Open_Request));
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
        free_buffers_open_request(open_request);
        free(open_request);
        return 0;
    }
    uint64_t path_length = request_length - sizeof(struct IPC_Open);

    result = prepare_filename(open_request, consumer, request->path, path_length);
    if (result != 0) {
        open_file_send_fail(request, -result);
        free_buffers_open_request(open_request);
        free(open_request);
        return 0;
    }

    result = bind_request_to_root(open_request);
    if (result != 0) {
        open_file_send_fail(request, -result);
        free_buffers_open_request(open_request);
        free(open_request);
        return 0;
    }

    result = register_request_with_consumer(open_request, consumer);
    if (result != 0) {
        open_file_send_fail(request, -result);
        free_buffers_open_request(open_request);
        free(open_request);
        return 0;
    }

    result = process_request(open_request);
    if (result != 0) {
        open_file_send_fail(request, -result);
        free_buffers_open_request(open_request);
        free(open_request);
        return 0;
    }

    return 0;
}
