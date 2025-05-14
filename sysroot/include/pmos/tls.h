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

#ifndef _TLS_H
#define _TLS_H 1
#include <pmos/ports.h>
#include <stdint.h>

#define __DECLARE_STACK_T
#include "../__posix_types.h"

/**
 * @brief Internal TLS structure
 *
 * An internal structure, used to hold Thread Local Storage data (created by the compiler to support
 * variables with __thread keyword).
 */
typedef struct TLS_Data {
    uint64_t memsz; ///< Memory size in bytes holding the TLS data
    uint64_t align; ///< Memory alignment

    uint64_t filesz; ///< The size of the data. If filesz < memsz, the rest is initialized to 0.
    unsigned char data[0]; ///< TLS data, to which the __thread variables are intiialized upon the
                           ///< program first start and thread creation.
} TLS_Data;

/// Thread local data for all threads
/// It is passed by the kernel and initialized during the program entry and is then copied for each
/// thread.
extern TLS_Data *__global_tls_data;

/**
 * @brief Node of the atexit list
 *
 * This is an internal structure, used to hold the atexit list, for __cxa_thread_atexit_impl()
 * function. It should not be used directly.
 */
struct __atexit_list_entry {
    struct __atexit_list_entry *next;
    void (*destructor_func)(void *);
    void *arg;
    void *dso_handle;
};

/// @brief Thread status
///
/// These constants control the status of the thread, defining whether uthread structure and other
/// thread data is destroyed on thread exit (when the thread is detached) or not (when the thread is
/// not detached). Since the kernel does not manage threads, the thread data is managed by the
/// C/pthread library in the userspace.
enum {
    __THREAD_STATUS_RUNNING  = 0,
    __THREAD_STATUS_DETACHED = 1,
    __THREAD_STATUS_JOINING  = 2,
    __THREAD_STATUS_FINISHED = 3,
};

/**
 * @brief Structure holging the thread local data
 *
 * This structure holds the internal data stored needed for correct working of the treads.
 * tls_data variable holds the end of the region which is used by the __thread variabled.
 * self must point to itself. In x86, self must be loaded into %gs or %fs registors depending on the
 * architecture.
 */
struct uthread {
    unsigned char tls_data[0];
    struct uthread *self;
    void *stack_top;
    size_t stack_size;
    void *return_value; // A.k.a. uint64_t
    struct __atexit_list_entry *atexit_list_head;
    int thread_status;
    // TODO: Store errno here instead of __thread
    pmos_port_t join_notify_port; // Port used to notify the thread that does pthread_join() that
                                  // the thread has exited

    struct {
        void *data_ptr;
        size_t generation;
    } *dtv;
    size_t dtv_size;

    pmos_right_t fs_right;
    pmos_right_t pipe_right;
    pmos_port_t cmd_reply_port;

    int signal_stack_spinlock;
    stack_t signal_stack;

    uint64_t pending_signals;
    uint64_t sigmask;

    uint64_t thread_task_id;

    struct uthread *next, *prev;
};

/**
 * @brief Returns thread pointer for the given uthread
 *
 * This function gets the appropriate thread pointer for the given uthread address. This function is
 * arch specific and depends on the TLS structures variants.
 */
void *__thread_pointer_from_uthread(struct uthread *uthread);

struct uthread *__find_uthread(uint64_t tid);

/**
 * Returns TLS uthread pointer for the current thread
 */
struct uthread *__get_tls();

#if defined(__cplusplus)
extern "C" {
#endif

#if defined(__cplusplus)
} /* extern "C" */
#endif

#endif // _TLS_H