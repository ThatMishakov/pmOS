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

#ifndef IPC_QUEUE_H
#define IPC_QUEUE_H

#include "filesystem_struct.h"

#include <stdint.h>

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
    .read       = &__ipc_queue_read,
    .write      = &__ipc_queue_write,
    .clone      = &__ipc_queue_clone,
    .close      = &__ipc_queue_close,
    .fstat      = &__ipc_queue_fstat,
    .isatty     = &__ipc_queue_isatty,
    .isseekable = &__ipc_queue_isseekable,
    .filesize   = &__ipc_queue_filesize,
    .free       = &__ipc_queue_free,
};

/// Initializes the descriptor to the IPC queue with the given name
/// @param file_data The file descriptor data
/// @param consumer_id The consumer ID
/// @param name The name of the queue
/// @return int 0 on success, -1 on error
int __set_desc_queue(void *file_data, uint64_t consumer_id, const char *name);

#endif // IPC_QUEUE_H