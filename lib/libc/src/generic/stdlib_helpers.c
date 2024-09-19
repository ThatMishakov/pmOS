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
#include <pmos/ipc.h>
#include <pmos/load_data.h>
#include <pmos/memory.h>
#include <pmos/ports.h>
#include <pmos/tls.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdio_internal.h>
#include <stdlib.h>
#include <string.h>

TLS_Data *__global_tls_data = NULL;

#define max(x, y)                (x > y ? x : y)
#define alignup(size, alignment) (size % alignment ? size + (alignment - size % alignment) : size)

struct uthread *__get_tls();
void __release_tls(struct uthread *u);
struct uthread *__prepare_tls(void *stack_top, size_t stack_size);

void __init_uthread(struct uthread *u, void *stack_top, size_t stack_size)
{
    u->self             = u;
    u->stack_top        = stack_top;
    u->stack_size       = stack_size;
    u->return_value     = NULL;
    u->atexit_list_head = NULL;
    u->thread_status    = __THREAD_STATUS_RUNNING;
    u->join_notify_port = INVALID_PORT;
    u->next             = u;
    u->prev             = u;
}

void __thread_exit_destroy_tls()
{
    struct uthread *u = __get_tls();

    if (u->thread_status == __THREAD_STATUS_RUNNING) {
        bool not_detached = __sync_bool_compare_and_swap(&u->thread_status, __THREAD_STATUS_RUNNING,
                                                         __THREAD_STATUS_FINISHED);
        if (not_detached)
            // Let caller get the return value and release the TLS
            // If pthread_detach is called at this point, it would be what does it
            return;
    }

    switch (u->thread_status) {
    case __THREAD_STATUS_DETACHED:
        __release_tls(u);
        break;
    case __THREAD_STATUS_JOINING: {
        IPC_Thread_Finished msg = {
            .type      = IPC_Thread_Finished_NUM,
            .flags     = 0,
            .thread_id = (uint64_t)u->self,
        };

        // If it fails there is nothing that can be done
        // Ideally, kernel should guarantee that once the port is created, sending
        // messages will always succeed
        send_message_port(u->join_notify_port, sizeof(msg), (char *)&msg);
        break;
    }
    default:
        fprintf(stderr, "pmOS libC: Invalid thread status: %i\n", u->thread_status);
        assert(false && "Invalid thread status");
        break;
    }
}

void __libc_init_hook() __attribute__((weak));

// TODO: This does not look good
void *__load_data_kernel       = 0;
size_t __load_data_size_kernel = 0;

static void init_tls_first_time(void *load_data, size_t load_data_size, TLS_Data *d)
{
    __load_data_kernel      = load_data;
    __load_data_size_kernel = load_data_size;

    __global_tls_data = d;

    struct load_tag_stack_descriptor *s = (struct load_tag_stack_descriptor *)get_load_tag(
        LOAD_TAG_STACK_DESCRIPTOR, load_data, load_data_size);
    if (d == NULL)
        return; // TODO: Panic

    struct uthread *u = __prepare_tls((void *)s->stack_top, s->stack_size);
    if (u == NULL)
        return; // TODO: Panic

    void *thread_pointer = __thread_pointer_from_uthread(u);
    set_registers(0, SEGMENT_FS, thread_pointer);

    if (__libc_init_hook)
        __libc_init_hook();
}

void __init_stdio();

// defined in pthread/threads.c
extern uint64_t __active_threads;

#ifdef __riscv
long __get_gp();
long __libc_gp = 0;
#endif

/// @brief Initializes the standard library
///
/// This function initializes the standard library and is the first thing called in _start function
/// (in crt0.S), before calling the constructors and main(). It is responsible for initializing the
/// TLS, file descriptors, arguments and environment variables, since it is not managed by kernel.
/// The dtaa is passed by the caller as a memory region mapped to the process's address space,
/// containing various load_tags containing the descriptors.
/// @param load_data Pointer to the memory region containing the load tags
/// @param load_data_size Size of the memory region containing the load tags
/// @param d Page containing the TLS data
void init_std_lib(void *load_data, size_t load_data_size, TLS_Data *d)
{
    #ifdef __riscv
    __libc_gp = __get_gp();
    #endif

    init_tls_first_time(load_data, load_data_size, d);

    __active_threads = 1;

    __init_stdio();
}

struct load_tag_generic *get_load_tag(uint32_t tag, void *load_data, size_t load_data_size)
{
    struct load_tag_generic *t = load_data;
    while (t->tag != LOAD_TAG_CLOSE) {
        if (t->tag == tag)
            return t;
        t = (struct load_tag_generic *)((unsigned char *)t + t->offset_to_next);
        if ((unsigned char *)t >= (unsigned char *)load_data + load_data_size)
            return NULL;
    }
    return NULL;
}