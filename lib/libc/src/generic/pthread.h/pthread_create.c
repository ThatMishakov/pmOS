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
#include <pmos/tls.h>
#include <pmos/system.h>
#include <pthread.h>

// Defined in libc/src/pthread/threads.c
extern uint64_t __active_threads;

// Defined in libc/src/libc_helpers.c
extern struct uthread *__prepare_tls(void *stack_top, size_t stack_size);
extern void __release_tls(struct uthread *u);

// Defined in ./pthread_entry.S
extern void __pthread_entry_point(void *(*start_routine)(void *), void *arg)
    __attribute__((noreturn));

// Defined in libc/src/filesystem.c
extern int __share_fs_data(uint64_t tid);

extern uint64_t process_task_group;

int pthread_list_spinlock = 0;
// Worker thread is also the list head
extern struct uthread *worker_thread;
int no_new_threads = 0;

int pthread_create(pthread_t *thread, const pthread_attr_t *attr, void *(*start_routine)(void *),
                   void *arg)
{
    // Create stack
    size_t stack_size       = 0x200000; // 2 MiB; TODO: Respect attr
    mem_request_ret_t stack = create_normal_region(0, NULL, 0x200000, 1 | 2 | CREATE_FLAG_COW);
    if (stack.result != SUCCESS) {
        errno = -stack.result;
        return -1;
    }
    uintptr_t stack_top = (uintptr_t)stack.virt_addr + stack_size;
    // The stack is not freed by release_tls() since the thread goes stackless and releases it
    // itself when it exits after releasing the TLS (when pthread is not detached; since it can't be
    // done without the stack). The kernel does not manage stacks, so on error, the stack needs to
    // be freed manually.

    // Prepare TLS
    struct uthread *u = __prepare_tls((void *)stack_top, stack_size);
    if (u == NULL) {
        release_region(TASK_ID_SELF, stack.virt_addr);
        // errno should be set by prepare_tls
        return -1;
    }

    // Create the new task locking list of threads so that the worker thread doesn't loose it if between
    // the creation of new task and adding it to the list current thread dies
    pthread_spin_lock(&pthread_list_spinlock);
    // No new threads are allowed, likely because the process is terminating
    if (no_new_threads) {
        pthread_spin_unlock(&pthread_list_spinlock);
        release_region(TASK_ID_SELF, stack.virt_addr);
        __release_tls(u);
        errno = EAGAIN;
        return -1;
    }

    // Create new task
    syscall_r r = syscall_new_process();
    if (r.result != 0) {
        errno = -r.result;
        pthread_spin_unlock(&pthread_list_spinlock);
        release_region(TASK_ID_SELF, stack.virt_addr);
        __release_tls(u);
        return -1;
    }
    u->thread_task_id = r.value;

    // Add the new task to the list
    u->next = worker_thread;
    u->prev = worker_thread->prev;
    worker_thread->prev->next = u;
    worker_thread->prev       = u;

    pthread_spin_unlock(&pthread_list_spinlock);

    // Add to the process task group
    result_t add_result = add_task_to_group(u->thread_task_id, process_task_group);
    if (add_result != SUCCESS) {
        syscall_kill_task(u->thread_task_id);
        release_region(TASK_ID_SELF, stack.virt_addr);
        __release_tls(u);
        errno = -add_result;
        return -1;
    }

    // Assign a page table
    page_table_req_ret_t thread_table =
        asign_page_table(u->thread_task_id, PAGE_TABLE_SELF, PAGE_TABLE_ASIGN);
    if (thread_table.result != 0) {
        syscall_kill_task(u->thread_task_id);
        release_region(TASK_ID_SELF, stack.virt_addr);
        __release_tls(u);
        errno = -thread_table.result;
        return -1;
    }
    // Kernel counts references to page table and it can't be freed explicitly

    // Set the stack
    syscall_r r_result = init_stack(u->thread_task_id, (void *)stack_top);
    if (r_result.result != SUCCESS) {
        release_region(TASK_ID_SELF, stack.virt_addr);
        __release_tls(u);
        syscall_kill_task(u->thread_task_id);
        errno = -r_result.result;
        return -1;
    }


    // Share filesystem
    int result = __share_fs_data(u->thread_task_id);
    if (result != 0) {
        syscall_kill_task(u->thread_task_id);
        release_region(TASK_ID_SELF, stack.virt_addr);
        __release_tls(u);
        errno = -result;
        return -1;
    }
    // When the thread exits, the kernel will remove the task from the group

    #ifdef __i386__
    result_t s_result = set_registers(u->thread_task_id, SEGMENT_GS, __thread_pointer_from_uthread(u));
    #else
    result_t s_result = set_registers(u->thread_task_id, SEGMENT_FS, __thread_pointer_from_uthread(u));
    #endif
    // This should never fail but just in case
    if (s_result != SUCCESS) {
        release_region(TASK_ID_SELF, stack.virt_addr);
        __release_tls(u);
        syscall_kill_task(u->thread_task_id);
        errno = -s_result;
        return -1;
    }

    __atomic_fetch_add(&__active_threads, 1, __ATOMIC_SEQ_CST);

    // Fire the task!
    s_result = syscall_start_process(u->thread_task_id, (u64)__pthread_entry_point, (u64)start_routine,
                                     (u64)arg, 0);
    if (s_result != 0) {
        __atomic_fetch_sub(&__active_threads, 1, __ATOMIC_SEQ_CST);
        release_region(TASK_ID_SELF, stack.virt_addr);
        __release_tls(u);
        syscall_kill_task(u->thread_task_id);
        errno = -s_result;
        return -1;
    }

    if (thread != NULL)
        *thread = (void *)u;

    return 0;
}

extern void __pthread_exit(void *retval) __attribute__((noreturn));

__attribute__((noreturn)) void pthread_exit(void *retval) { __pthread_exit(retval); }