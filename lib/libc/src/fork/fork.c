#include "fork.h"
#include "atfork.h"
#include <pmos/system.h>
#include <pmos/memory.h>
#include <errno.h>
#include <stdint.h>

extern void __fork_child_entry_point();

uint64_t __libc_fork_inner(struct fork_for_child * child_data)
{
    // Create new process
    syscall_r r = syscall_new_process(3);
    if (r.result != 0) {
        errno = -r.result;
        return -1;
    }
    uint64_t child_tid = r.value;

    // Prepare filesystem
    int result = __clone_fs_data(&child_data->fs_data, child_tid);
    if (result != 0) {
        return -1;
    }

    // Transfer segments
    r = get_segment(PID_SELF, SEGMENT_GS);
    if (r.result != 0) {
        // TODO: Kill process
        errno = -r.result;
        return -1;
    }
    r.result = set_segment(child_tid, SEGMENT_GS, (void*)r.value);
    if (r.result != 0) {
        // TODO: Kill process
        errno = -r.result;
        return -1;
    }

    r = get_segment(PID_SELF, SEGMENT_FS);
    if (r.result != 0) {
        // TODO: Kill process
        errno = -r.result;
        return -1;
    }
    r.result = set_segment(child_tid, SEGMENT_FS, (void*)r.value);
    if (r.result != 0) {
        // TODO: Kill process
        errno = -r.result;
        return -1;
    }

    // Call atfork handlers
    __libc_pre_fork();

    // Clone memory
    page_table_req_ret_t child_new_page_table = asign_page_table(child_tid, PAGE_TABLE_SELF, PAGE_TABLE_CLONE);
    if (child_new_page_table.result != 0) {
        // TODO: Kill process
        errno = -child_new_page_table.result;
        return -1;
    }

    // Fire new process
    r.result = syscall_start_process(child_tid, (uint64_t)&__fork_child_entry_point, (uint64_t)child_data, 0, 0);
    if (r.result != 0) {
        // TODO: Kill process
        errno = -r.result;
        return -1;
    }

    // Call atfork handlers
    __libc_post_fork_parent();

    return child_tid;
}

uint64_t __libc_fork_child(struct fork_for_child * child_data)
{
    __libc_fixup_fs_post_fork(child_data);

    // Call atfork handlers
    __libc_post_fork_child();

    // Return
    return 0;
}