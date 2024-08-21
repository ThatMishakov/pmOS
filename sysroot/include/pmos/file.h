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

#ifndef __PMOS_FILE_H
#define __PMOS_FILE_H
#include "ipc.h"
#include "system.h"

/// pmOS memory object identificator
typedef unsigned long mem_object_t;

struct pmOS_File {
    pmos_port_t file_handler;
    mem_object_t memory_object;
};

/**
 * @brief Opens a file
 *
 * This function sends a request to the VFS server and opens a file descriptor. The file is an
 * abstract type which, among other things, might point to a file, folder, block device or others.
 * '\' is used as a file deliminer.
 *
 * @param desc File descriptor to be filled. If the execution is not successfull, it shall not
 * change.
 * @param name Name of the file. This string can contain any characters and '\0' is not considered
 * to be a string terminator.
 * @param length Length of the string.
 * @param flags Flags for opening the file.
 * @return Result of the execution. SUCCESS macro indicates that the operation was successfull.
 * Otherwise, the returned number indicates the error number.
 */
result_t pmos_open_file(pmOS_File *desc, const unsigned char *name, size_t length, uint64_t flags);

#endif