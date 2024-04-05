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

#include "waiter_struct.h"
#include <errno.h>
#include <pmos/system.h>
#include <pmos/ports.h>
#include "spin_pause.h"

int __init_waiter_struct(struct __pthread_waiter * waiter_struct) {
    ports_request_t request = create_port(PID_SELF, 0);
    if (request.result != SUCCESS) {
        errno = request.result;
        return -1;
    }

    waiter_struct->notification_port = request.port;
    waiter_struct->next = NULL;
    return 0;
}

void __atomic_add_waiter(struct __pthread_waiter **waiters_list_head, struct __pthread_waiter **waiters_list_tail, struct __pthread_waiter* waiter) {
    waiter->next = NULL;

    // Atomically add waiter to mutex->waiters_list_tail
    struct __pthread_waiter * old_list_tail = __atomic_exchange_n(waiters_list_tail, waiter, __ATOMIC_SEQ_CST);
    if (old_list_tail == NULL) {
        __atomic_store_n(waiters_list_head, waiter, __ATOMIC_SEQ_CST);
    } else {
        __atomic_store_n(&old_list_tail->next, waiter, __ATOMIC_SEQ_CST);
    }
}

struct __pthread_waiter * __try_pop_waiter(struct __pthread_waiter **waiters_list_head, struct __pthread_waiter **waiters_list_tail) {
    struct __pthread_waiter * tail = __atomic_load_n(waiters_list_head, __ATOMIC_SEQ_CST);
    if (tail == NULL) {
        // List is empty
        return NULL;
    }

    struct __pthread_waiter * head = __atomic_load_n(waiters_list_head, __ATOMIC_SEQ_CST);
    while (head == NULL) {
        // List is not empty, but head was not yet set. Wait for it
        __spin_pause();
        head = __atomic_load_n(waiters_list_head, __ATOMIC_SEQ_CST);
    }

    struct __pthread_waiter * next = __atomic_load_n(&head->next, __ATOMIC_SEQ_CST);
    if (next == NULL) {
        // Check if head is the only element or if next is not yet linked
        if (__atomic_compare_exchange_n(waiters_list_tail, &head, NULL, 0, __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST)) {
            // Head was the only element
            // Try to NULL it. If it fails, it means that another thread already overwrote it, which is desirable anyway
            __atomic_compare_exchange_n(waiters_list_head, &head, NULL, 0, __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST);
            return head;
        }

        // Head is not the only element, which means that next cannot not be NULL, so wait for it to be set
        while (next == NULL) {
            __spin_pause();
            next = __atomic_load_n(&head->next, __ATOMIC_SEQ_CST);
        }
    }

    // Pop head
    __atomic_store_n(waiters_list_head, next, __ATOMIC_SEQ_CST);
    return head;
}