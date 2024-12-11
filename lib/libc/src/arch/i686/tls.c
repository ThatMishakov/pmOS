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

#include <assert.h>
#include <pmos/memory.h>
#include <pmos/memory_flags.h>
#include <pmos/tls.h>
#include <stdbool.h>
#include <string.h>
#include <pthread.h>

void __init_uthread(struct uthread *u, void *stack_top, size_t stack_size);

#define max(x, y)                ((x) > (y) ? (x) : (y))
#define alignup(size, alignment) (size % alignment ? size + (alignment - size % alignment) : size)

struct uthread *__prepare_tls(void *stack_top, size_t stack_size)
{
    bool has_tls = __global_tls_data != NULL;

    size_t memsz  = has_tls ? __global_tls_data->memsz : 0;
    size_t align  = has_tls ? __global_tls_data->align : 0;
    size_t filesz = has_tls ? __global_tls_data->filesz : 0;
    assert(memsz >= filesz && "TLS memsz must be greater than or equal to TLS filesz");

    align                = max(align, _Alignof(struct uthread));
    size_t memsz_aligned = alignup(memsz, align);
    size_t alloc_size    = memsz_aligned + sizeof(struct uthread);

    size_t size_all = alignup(alloc_size, 4096);

    mem_request_ret_t res = create_normal_region(0, 0, size_all, PROT_READ | PROT_WRITE);
    if (res.result != SUCCESS) {
        return NULL;
    }

    unsigned char *tls = (unsigned char *)res.virt_addr + memsz_aligned;

    if (memsz > 0) {
        memcpy(tls - memsz_aligned, __global_tls_data->data, filesz);
    }

    __init_uthread((struct uthread *)tls, stack_top, stack_size);

    return (struct uthread *)tls;
}

extern int pthread_list_spinlock;
// Worker thread is the list head
extern struct uthread *worker_thread;

void __release_tls(struct uthread *u)
{
    if (u == NULL)
        return;

    size_t memsz = __global_tls_data->memsz;
    size_t align = __global_tls_data->align;

    align = max(align, _Alignof(struct uthread));
    // size_t alloc_size = alignup(memsz, align) + sizeof(struct uthread);
    // size_t size_all = alignup(alloc_size, 4096);

    pthread_spin_lock(&pthread_list_spinlock);
    u->prev->next = u->next;
    u->next->prev = u->prev;
    pthread_spin_unlock(&pthread_list_spinlock);


    release_region(TASK_ID_SELF, (void *)u - alignup(memsz, align));
}

void *__get_tp()
{
    if (__global_tls_data == NULL)
        return NULL;

    size_t memsz = __global_tls_data->memsz;
    size_t align = __global_tls_data->align;

    return (char *)__get_tls() - alignup(memsz, align);
}

void *__thread_pointer_from_uthread(struct uthread *uthread) { return uthread; }