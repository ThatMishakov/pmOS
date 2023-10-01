#include <pthread.h>
#include <pmos/memory.h>
#include <errno.h>

// Defined in libc/src/pthread/threads.c
extern uint64_t __active_threads;

// Defined in libc/src/libc_helpers.c
extern struct uthread * __prepare_tls(void * stack_top, size_t stack_size);
extern void __release_tls(struct uthread * u);

// Defined in ./pthread_entry.S
extern void __pthread_entry_point(void *(*start_routine) (void *), void * arg) __attribute__((noreturn));

// Defined in libc/src/filesystem.c
extern int __share_fs_data(uint64_t tid);

int pthread_create(pthread_t * thread, const pthread_attr_t * attr, void *(*start_routine) (void *), void * arg)
{
    // Create new task
    syscall_r r = syscall_new_process(3);
    if (r.result != 0) {
        errno = -r.result;
        return -1;
    }
    uint64_t thread_task_id = r.value;

    // Assign a page table
    page_table_req_ret_t thread_table = asign_page_table(thread_task_id, PAGE_TABLE_SELF, PAGE_TABLE_ASIGN);
    if (thread_table.result != 0) {
        // TODO: Kill process
        errno = -thread_table.result;
        return -1;
    }
    // Kernel counts references to page table and it can't be freed explicitly

    // Share filesystem
    int result = __share_fs_data(thread_task_id);
    if (result != 0) {
        // TODO: Kill process
        errno = -result;
        return -1;
    }
    // When the thread exits, the kernel will remove the task from the group

    // Create stack
    size_t stack_size = 0x200000; // 2 MiB; TODO: Respect attr
    mem_request_ret_t stack = create_normal_region(0, NULL, 0x200000, 1 | 2);
    if (stack.result != SUCCESS) {
        // TODO: Kill process
        errno = -stack.result;
        return -1;
    }
    uintptr_t stack_top = (uintptr_t)stack.virt_addr + stack_size;
    // The stack is not freed by release_tls() since the thread goes stackless and releases it itself when it exits
    // after releasing the TLS (when pthread is not detached; since it can't be done without the stack).
    // The kernel does not manage stacks, so on error, the stack needs to be freed manually.

    // Set the stack
    syscall_r r_result = init_stack(thread_task_id, (void *)stack_top);
    if (r_result.result != SUCCESS) {
        release_region(PID_SELF, stack.virt_addr);
        // TODO: Kill process
        errno = -r_result.result;
        return -1;
    }

    // Prepare TLS
    struct uthread *u = __prepare_tls((void *)stack_top, stack_size);
    if (u == NULL) {
        release_region(PID_SELF, stack.virt_addr);
        // TODO: Kill process
        // errno should be set by prepare_tls
        return -1;
    }

    result_t s_result = set_segment(thread_task_id, SEGMENT_FS, u);
    // This should never fail but just in case
    if (s_result != SUCCESS) {
        release_region(PID_SELF, stack.virt_addr);
        __release_tls(u);
        // TODO: Kill process
        errno = -s_result;
        return -1;
    }

    __atomic_fetch_add(&__active_threads, 1, __ATOMIC_SEQ_CST);

    // Fire the task!
    s_result = syscall_start_process(thread_task_id, (u64)__pthread_entry_point, (u64)start_routine, (u64)arg, 0);
    if (s_result != 0) {
        __atomic_fetch_sub(&__active_threads, 1, __ATOMIC_SEQ_CST);
        release_region(PID_SELF, stack.virt_addr);
        __release_tls(u);
        // TODO: Kill process
        errno = -s_result;
        return -1;
    }

    if (thread != NULL)
        *thread = thread_task_id;

    return 0;
}