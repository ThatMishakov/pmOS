#ifndef IPC_QUEUE_H
#define IPC_QUEUE_H

#include <stdint.h>
#include "filesystem_struct.h"

extern read_func __ipc_queue_read;
extern write_func __ipc_queue_write;
extern clone_func __ipc_queue_clone;
extern close_func __ipc_queue_close;
extern fstat_func __ipc_queue_fstat;
extern isatty_func __ipc_queue_isatty;
extern isseekable_func __ipc_queue_isseekable;
extern filesize_func __ipc_queue_filesize;
extern free_func __ipc_queue_free;

static struct Filesystem_Adaptor __ipc_queue_adaptor = {
    .read = &__ipc_queue_read,
    .write = &__ipc_queue_write,
    .clone = &__ipc_queue_clone,
    .close = &__ipc_queue_close,
    .fstat = &__ipc_queue_fstat,
    .isatty = &__ipc_queue_isatty,
    .isseekable = &__ipc_queue_isseekable,
    .filesize = &__ipc_queue_filesize,
    .free = &__ipc_queue_free,
};

#endif // IPC_QUEUE_H