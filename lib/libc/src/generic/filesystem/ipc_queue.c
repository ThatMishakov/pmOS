#include "ipc_queue.h"
#include <errno.h>
#include <pmos/ipc.h>
#include <string.h>
#include <kernel/errors.h>
#include <pmos/system.h>
#include <pmos/ports.h>
#include "filesystem.h"
#include <stdio.h>
#include <sys/stat.h>
#include <stdlib.h>
#include <assert.h>

ssize_t __ipc_queue_read(void * file_data, uint64_t consumer_id, void * buf, size_t count, size_t offset) {
    // TODO: Not implemented
    errno = ENOSYS;
    return -1;
}

static ssize_t write_ipc_queue(pmos_port_t port, const void *buf, size_t size) {
    static const int buffer_size = 252;
    const char * str = buf;

    struct {
        uint32_t type;
        char buffer[buffer_size];
    } desc;

    desc.type = IPC_Write_Plain_NUM;
    int s = 0;

    result_t result = SUCCESS;

    for (int i = 0; i < size; i += buffer_size) {
        int size_s = size - i > buffer_size ? buffer_size : size - i;
        memcpy(desc.buffer, &str[i], size_s);

        int k = send_message_port(port, size_s + sizeof(uint32_t), (const char*)(&desc));

        if (k == SUCCESS) {
            s += size_s;
        } else {
            if (s == 0) {
                errno = -k;
                result = EOF;
            }

            break;
        }

        // TODO: If the file was sent partially, return the number of bytes sent and not EOF
        result = result == SUCCESS ? s : EOF;
    }

    return result;
}

ssize_t __ipc_queue_write(void * file_data, uint64_t consumer_id, const void * buf, size_t count, size_t /* offset */) {
    struct IPC_Queue * q = (struct IPC_Queue *)file_data;

    pmos_port_t queue_port = q->port;
    const char * port_name = q->name;
    if (queue_port == INVALID_PORT) {
        ports_request_t port_req = get_port_by_name(port_name, strlen(port_name), 0);
        if (port_req.result != SUCCESS) {
            errno = -port_req.result;
            return -1;
        }

        queue_port = port_req.port;
        q->port = queue_port;
    }

    return write_ipc_queue(queue_port, buf, count);
}

int __ipc_queue_clone(void * file_data, uint64_t /* unused consumer_id */, void * new_data, uint64_t /* unused new_new_consumer_id */) {
    struct IPC_Queue * q = (struct IPC_Queue *)file_data;

    char * new_name = strdup(q->name);
    if (new_name == NULL) {
        // Appropriate errno is set by strdup
        // errno = ENOMEM;
        return -1;
    }

    struct IPC_Queue * new_q = (struct IPC_Queue *)new_data;
    new_q->name = new_name;
    new_q->port = q->port;

    return 0;
}

int __ipc_queue_close(void * file_data, uint64_t consumer_id) {
    // Nothind to do here; call free just in case
    __ipc_queue_free(file_data, consumer_id);
    return 0;
}

int __ipc_queue_fstat(void * file_data, uint64_t /* unused consumer_id */, struct stat * statbuf) {
    struct IPC_Queue * q = (struct IPC_Queue *)file_data;

    statbuf->st_mode = S_IFIFO;
    statbuf->st_size = 0;
    statbuf->st_blksize = 0;
    statbuf->st_blocks = 0;
    statbuf->st_nlink = 1;
    statbuf->st_uid = 0;
    statbuf->st_gid = 0;
    statbuf->st_dev = 0;
    statbuf->st_ino = 0;
    statbuf->st_rdev = 0;
    statbuf->st_atim = (struct timespec){};
    statbuf->st_mtim = (struct timespec){};
    statbuf->st_ctim = (struct timespec){};

    return 0;
}

int __ipc_queue_isatty(void * file_data, uint64_t /* unused consumer_id */) {
    return 0;
}

int __ipc_queue_isseekable(void * file_data, uint64_t /* unused consumer_id */) {
    return 0;
}

ssize_t __ipc_queue_filesize(void * file_data, uint64_t /* unused consumer_id */) {
    return 0;
}

void __ipc_queue_free(void * file_data, uint64_t /* unused consumer_id */) {
    struct IPC_Queue * q = (struct IPC_Queue *)file_data;

    if (q->name != NULL) {
        free(q->name);
        q->name = NULL;
    }

    q->port = INVALID_PORT;
}

int __set_desc_queue(void * file_data, uint64_t consumer_id, const char * name) {
    assert(file_data != NULL);

    struct IPC_Queue * q = (struct IPC_Queue *)file_data;

    char * name_copy = strdup(name);
    if (name_copy == NULL) {
        // Appropriate errno is set by strdup
        // errno = ENOMEM;
        return -1;
    }

    q->name = name_copy;
    q->port = INVALID_PORT;

    return 0;
}
