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

#ifndef FILESYSTEM_FILE_H
#define FILESYSTEM_FILE_H

#include <stdint.h>
#include "filesystem_struct.h"

extern read_func __file_read;
extern write_func __file_write;
extern clone_func __file_clone;
extern close_func __file_close;
extern fstat_func __file_fstat;
extern isatty_func __file_isatty;
extern isseekable_func __file_isseekable;
extern filesize_func __file_filesize;
extern free_func __file_free;

static struct Filesystem_Adaptor __file_adaptor = {
    .read = &__file_read,
    .write = &__file_write,
    .clone = &__file_clone,
    .close = &__file_close,
    .fstat = &__file_fstat,
    .isatty = &__file_isatty,
    .isseekable = &__file_isseekable,
    .filesize = &__file_filesize,
    .free = __file_free,
};

/// @brief Opens a file, filling in the descriptor structure
/// @param path Null terminated string containing the path to the file
/// @param flags Flags to open the file with
/// @param mode Mode to open the file with
/// @param descriptor File descriptor to be filled in
/// @param consumer_id ID of the consumer for which the file is being opened
/// @return 0 on success, -1 on error setting errno
int __open_file(const char * path, int flags, mode_t mode, void * file_data, uint64_t consumer_id);

#endif // FILESYSTEM_FILE_H