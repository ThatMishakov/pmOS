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

#include <errno.h>
#include <pmos/memory.h>
#include <sys/mman.h>
#include <stdio.h>

void *mmap(void *addr, size_t length, int prot, int flags, int fd, off_t offset)
{
    // TODO: mmap can probably be fully implemented soon!
    (void)fd;
    (void)offset;

    if (!(flags & MAP_ANONYMOUS)) {
        // Not supported
        // (although infrastructure for it is mostly there :) )
        fprintf(stderr, "pmOS libc: mmap: MAP_ANONYMOUS is the only supported flag. Flags: 0x%u\n", flags);
        errno = ENOTSUP;
        return MAP_FAILED;
    }

    if (length == 0) {
        errno = EINVAL;
        return MAP_FAILED;
    }

    // Align length to page size
    size_t aligned_length = (length + 4095) & ~4095UL;

    // TODO: Get rid of magic numbers
    mem_request_ret_t req = create_normal_region(0, addr, aligned_length,
                                                 (prot & 0x07) | (flags & MAP_FIXED ? 0x08 : 0));
    if (req.result != SUCCESS) {
        errno = ENOMEM;
        return MAP_FAILED;
    }

    return (void *)req.virt_addr;
}

// The delete region syscall is not implemented yet
// int munmap(void *addr, size_t length) {
//     mem_request_ret_t req = delete_region((uintptr_t)addr, length);
//     if (req.result != SUCCESS) {
//         errno = EINVAL;
//         return -1;
//     }

//     return 0;
// }

int munmap(void *addr, size_t length)
{
    (void)addr;
    (void)length;

    // Not supported
    errno = ENOTSUP;
    return -1;
}