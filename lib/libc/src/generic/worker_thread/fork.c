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

#include <errno.h>
#include <pmos/memory.h>
#include <pmos/system.h>
#include <pmos/memory.h>
#include <stdint.h>
#include <pmos/tls.h>
#include <pmos/ipc.h>
#include <pmos/ports.h>
#include <stdio.h>
#include <assert.h>

static struct task_register_set child_registers;
static struct uthread *child_uthread;
static struct Filesystem_Data *child_fs_data = NULL;
static long child_segment;

extern void __fork_child_entry_point();

pid_t __fork_preregister_child(uint64_t tid);
int __register_process();
__attribute__((noreturn)) void _syscall_exit(int status);
int __fork_fix_thread(struct uthread *child_uthread, uint64_t thread_tid);
void __destroy_fs_data(struct Filesystem_Data *fs_data);

void fork_reply(pmos_port_t reply_port, pid_t result)
{
    IPC_Request_Fork_Reply reply = {
        .type = IPC_Request_Fork_Reply_NUM,
        .result = result,
    };

    result_t r = send_message_port(reply_port, sizeof(reply), &reply);
    if (r != 0) {
        fprintf(stderr, "Failed to send fork reply to task %ld: %ld\n", reply_port, r);
    }
}


void __libc_fork_inner(uint64_t requester, pmos_port_t reply_port, unsigned long sp)
{
    child_fs_data = NULL;

    // Create new process
    syscall_r r = syscall_new_process();
    if (r.result != 0) {
        fork_reply(reply_port, -r.result);
        return;
    }
    uint64_t child_tid = r.value;

    pid_t child_pid = __fork_preregister_child(child_tid);
    if (child_pid < 0) {
        fork_reply(reply_port, -errno);
        return;
    }

    uint64_t segment;
    // Transfer segments
    r.result = get_registers(TASK_ID_SELF, SEGMENT_GS, &segment);
    if (r.result != 0) {
        syscall_kill_task(child_tid);
        fork_reply(reply_port, -r.result);
        return;
    }
    r.result = set_registers(child_tid, SEGMENT_GS, (void *)segment);
    if (r.result != 0) {
        syscall_kill_task(child_tid);
        fork_reply(reply_port, -r.result);
        return;
    }

    r.result = get_registers(TASK_ID_SELF, SEGMENT_FS, &segment);
    if (r.result != 0) {
        syscall_kill_task(child_tid);
        fork_reply(reply_port, -r.result);
        return;
    }
    r.result = set_registers(child_tid, SEGMENT_FS, (void *)segment);
    if (r.result != 0) {
        syscall_kill_task(child_tid);
        fork_reply(reply_port, -r.result);
        return;
    }

    // Save registers for the child
    r.result = pause_task(requester);
    if (r.result != 0) {
        syscall_kill_task(child_tid);
        fork_reply(reply_port, -r.result);
        return;
    }

    r.result = get_registers(requester, GENERAL_REGS, &child_registers);
    if (r.result != 0) {
        resume_task(requester);
        syscall_kill_task(child_tid);
        fork_reply(reply_port, -r.result);
        return;
    }

    r.result = get_registers(requester, SEGMENT_FS, &child_segment);
    if (r.result != 0) {
        resume_task(requester);
        syscall_kill_task(child_tid);
        fork_reply(reply_port, -r.result);
        return;
    }

    // Save uthread for the child
    child_uthread = __find_uthread(requester);
    if (child_uthread == NULL) {
        resume_task(requester);
        syscall_kill_task(child_tid);
        fork_reply(reply_port, -ENOENT);
        return;
    }

    // Lock filesystem
    __libc_fs_lock_pre_fork();

    // Prepare filesystem
    int result = __clone_fs_data(&child_fs_data, child_tid, true);
    if (result != 0) {
        child_pid = result;
        __libc_malloc_pre_fork();
        // Even though an error ocurred, lock malloc to be unlocked later
        goto end;
    }

    // Lock malloc. Everything depends on it, so it has to be the last thing to be locked
    __libc_malloc_pre_fork();

    // Clone memory
    page_table_req_ret_t child_new_page_table =
        asign_page_table(child_tid, PAGE_TABLE_SELF, PAGE_TABLE_CLONE);
    if (child_new_page_table.result != 0) {
        syscall_kill_task(child_tid);
        child_pid = child_new_page_table.result;
        goto end;
    }

    // Fire new process
    r.result = syscall_start_process(child_tid, (uint64_t)&__fork_child_entry_point,
                                     (uint64_t)sp, 0, 0);
    if (r.result != 0) {
        syscall_kill_task(child_tid);
        child_pid = r.result;
        goto end;
    }

end:
    __libc_malloc_post_fork_parent();

    __libc_fs_unlock_post_fork();

    __destroy_fs_data(child_fs_data);

    fork_reply(reply_port, child_pid);
    resume_task(requester);
}

extern uint64_t process_task_group;
extern pmos_port_t worker_port;

// Defined in libc/src/filesystem.c
extern int __share_fs_data(uint64_t tid);

void __libc_fork_child()
{
    __fixup_worker_uthread();

    __libc_malloc_post_fork_child();

    __libc_fixup_fs_post_fork(child_fs_data);

    // Create the task group
    syscall_r r = create_task_group();
    if (r.result != 0) {
        fprintf(stderr, "pmOS libC: Failed to create task group for process in fork: %li\n", r.result);
        _syscall_exit(-r.result);
    }
    process_task_group = r.value;

    // Get the new worker port
    ports_request_t new_port = create_port(TASK_ID_SELF, 0);
    if (new_port.result != SUCCESS) {
        fprintf(stderr, "pmOS libC: Failed to initialize worker port in fork: %li\n", new_port.result);
        _syscall_exit(-new_port.result);
    }
    worker_port = new_port.port;


    // Register child with processd
    int result = __register_process();
    if (result != 0) {
        fprintf(stderr, "pmOS libC: Failed to register child with processd in fork: %i\n", result);
        _syscall_exit(-result);
    }

    // Create child task
    // TODO: The function does not do what its name says
    r = syscall_new_process();
    if (r.result != 0) {
        fprintf(stderr, "pmOS libC: Failed to create child task in fork: %li\n", r.result);
        _syscall_exit(-r.result);
    }
    uint64_t thread_tid = r.value;

    // Set registers
    r.result = set_registers(r.value, GENERAL_REGS, &child_registers);
    if (r.result != 0) {
        _syscall_exit(-r.result);
    }

    r.result = set_registers(r.value, SEGMENT_FS, (void *)child_segment);
    if (r.result != 0) {
        // This shouldn't fail
        _syscall_exit(-r.result);
    }

    // Set page table
    r.result = asign_page_table(r.value, PAGE_TABLE_SELF, PAGE_TABLE_ASIGN).result;
    if (r.result != 0) {
        fprintf(stderr, "pmOS libC: Failed to set page table in fork: %li\n", r.result);
        _syscall_exit(-r.result);
    }

    // Share filesystem data
    result = __share_fs_data(r.value);
    if (result != 0) {
        fprintf(stderr, "pmOS libC: Failed to share filesystem data in fork: %i\n", result);
        _syscall_exit(-result);
    }

    assert(__fork_fix_thread(child_uthread, thread_tid) != -1);

    resume_task(r.value);

    // Reply to child and resume it
    fork_reply(child_uthread->cmd_reply_port, 0);
}

atfork_fn __static_pre_fork[] = {};

atfork_fn __static_post_fork_parent[] = {};

atfork_fn __static_post_fork_child[] = {};

atfork_fn *__atfork_pre_fork      = NULL;
size_t __atfork_pre_fork_count    = 0;
size_t __atfork_pre_fork_capacity = 0;

atfork_fn *__atfork_post_fork_parent      = NULL;
size_t __atfork_post_fork_parent_count    = 0;
size_t __atfork_post_fork_parent_capacity = 0;

atfork_fn *__atfork_post_fork_child      = NULL;
size_t __atfork_post_fork_child_count    = 0;
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
    __libc_atfork_reverse_run(__static_post_fork_parent,
                              sizeof(__static_post_fork_parent) / sizeof(atfork_fn));
    __libc_atfork_reverse_run(__atfork_post_fork_parent, __atfork_post_fork_parent_count);
}

void __libc_post_fork_child(void)
{
    __libc_atfork_reverse_run(__static_post_fork_child,
                              sizeof(__static_post_fork_child) / sizeof(atfork_fn));
    __libc_atfork_reverse_run(__atfork_post_fork_child, __atfork_post_fork_child_count);
}