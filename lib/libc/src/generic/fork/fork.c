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

    // Lock filesystem
    __libc_fs_lock_pre_fork();


    // Prepare filesystem
    int result = __clone_fs_data(&child_data->fs_data, child_tid, true);
    if (result != 0) {
        child_tid = -1;
        __libc_malloc_pre_fork();
        // Even though an error ocurred, lock malloc to be unlocked later
        goto end;
    }

    // Lock malloc. Everything depends on it, so it has to be the last thing to be locked
    __libc_malloc_pre_fork();

    // Clone memory
    page_table_req_ret_t child_new_page_table = asign_page_table(child_tid, PAGE_TABLE_SELF, PAGE_TABLE_CLONE);
    if (child_new_page_table.result != 0) {
        // TODO: Kill process
        errno = -child_new_page_table.result;
        child_tid = -1;
        goto end;
    }

    // Fire new process
    r.result = syscall_start_process(child_tid, (uint64_t)&__fork_child_entry_point, (uint64_t)child_data, 0, 0);
    if (r.result != 0) {
        // TODO: Kill process
        errno = -r.result;
        child_tid = -1;
        goto end;
    }

end:
    __libc_malloc_post_fork_parent();

    __libc_fs_unlock_post_fork();

    // Call atfork handlers
    __libc_post_fork_parent();

    return child_tid;
}

uint64_t __libc_fork_child(struct fork_for_child * child_data)
{
    __libc_malloc_post_fork_child();

    __libc_fixup_fs_post_fork(child_data);

    // Call atfork handlers
    __libc_post_fork_child();

    // Return
    return 0;
}

atfork_fn __static_pre_fork[] = {
};

atfork_fn __static_post_fork_parent[] = {
};

atfork_fn __static_post_fork_child[] = {
};

atfork_fn *__atfork_pre_fork = NULL;
size_t __atfork_pre_fork_count = 0;
size_t __atfork_pre_fork_capacity = 0;

atfork_fn *__atfork_post_fork_parent = NULL;
size_t __atfork_post_fork_parent_count = 0;
size_t __atfork_post_fork_parent_capacity = 0;

atfork_fn *__atfork_post_fork_child = NULL;
size_t __atfork_post_fork_child_count = 0;
size_t __atfork_post_fork_child_capacity = 0;

void __libc_atfork_run(atfork_fn *fns, size_t count)
{
    for (size_t i = 0; i < count; i++) {
        fns[i]();
    }
}

void __libc_atfork_reverse_run(atfork_fn *fns, size_t count)
{
    for (size_t i = count; i > 0; i--) {
        fns[i - 1]();
    }
}

void __libc_pre_fork(void)
{
    __libc_atfork_run(__atfork_pre_fork, __atfork_pre_fork_count);
    __libc_atfork_run(__static_pre_fork, sizeof(__static_pre_fork) / sizeof(atfork_fn));
}

void __libc_post_fork_parent(void)
{
    __libc_atfork_reverse_run(__static_post_fork_parent, sizeof(__static_post_fork_parent) / sizeof(atfork_fn));
    __libc_atfork_reverse_run(__atfork_post_fork_parent, __atfork_post_fork_parent_count);
}

void __libc_post_fork_child(void)
{
    __libc_atfork_reverse_run(__static_post_fork_child, sizeof(__static_post_fork_child) / sizeof(atfork_fn));
    __libc_atfork_reverse_run(__atfork_post_fork_child, __atfork_post_fork_child_count);
}