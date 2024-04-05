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

#include <pmos/system.h>
#include <kernel/types.h>
#include <pmos/ports.h>
#include <pmos/memory.h>

syscall_r syscall_new_process()
{
    return pmos_syscall(SYSCALL_CREATE_PROCESS);
}

syscall_r start_process(u64 pid, u64 entry)
{
    return pmos_syscall(SYSCALL_START_PROCESS, pid, entry);
}

result_t send_message_port(u64 port, size_t size, const void* message)
{
    return pmos_syscall(SYSCALL_SEND_MSG_PORT, port, size, message).result;
}

result_t get_first_message(char* buff, u64 args, u64 port)
{
    return pmos_syscall(SYSCALL_GET_MESSAGE, buff, args, port).result;
}

result_t syscall_get_message_info(Message_Descriptor* descr, u64 port, uint32_t flags)
{
    return pmos_syscall(SYSCALL_GET_MSG_INFO, descr, port, flags).result;
}

u64 getpid()
{
    return pmos_syscall(SYSCALL_GETPID).value;
}

result_t request_priority(uint64_t priority)
{
    return pmos_syscall(SYSCALL_SET_PRIORITY, priority).value;
}

u64 get_lapic_id()
{
    return pmos_syscall(SYSCALL_GET_LAPIC_ID).value;
}

ports_request_t create_port(pid_t owner, uint64_t flags)
{
    syscall_r r = pmos_syscall(SYSCALL_CREATE_PORT, owner, flags);
    ports_request_t t = {r.result, r.value};
    return t;
}

ports_request_t get_port_by_name(const char *name, u64 length, u32 flags)
{
    syscall_r r = pmos_syscall(SYSCALL_GET_PORT_BY_NAME, name, length, flags);
    ports_request_t t = {r.result, r.value};
    return t;
}

result_t set_interrupt(pmos_port_t port, uint32_t intno, uint32_t flags)
{
    return pmos_syscall(SYSCALL_SET_INTERRUPT, port, intno, flags).result;
}

result_t name_port(pmos_port_t portnum, const char* name, size_t length, u32 flags)
{
    return pmos_syscall(SYSCALL_NAME_PORT, portnum, name, length, flags).result;
}

result_t set_log_port(pmos_port_t port, uint32_t flags)
{
    return pmos_syscall(SYSCALL_SET_LOG_PORT, port, flags).result;
}

mem_request_ret_t create_phys_map_region(uint64_t pid, void *addr_start, size_t size, uint64_t access, void* phys_addr)
{
    syscall_r r = pmos_syscall(SYSCALL_CREATE_PHYS_REGION, pid, addr_start, size, access, phys_addr);
    mem_request_ret_t t = {r.result, (void *)r.value};
    return t;
}

mem_request_ret_t create_normal_region(uint64_t pid, void *addr_start, size_t size, uint64_t access)
{
    syscall_r r = pmos_syscall(SYSCALL_CREATE_NORMAL_REGION, pid, addr_start, size, access);
    mem_request_ret_t t = {r.result, (void *)r.value};
    return t;
}

mem_request_ret_t transfer_region(uint64_t to_page_table, void * region, void * dest, uint64_t flags)
{
    syscall_r r = pmos_syscall(SYSCALL_TRANSFER_REGION, to_page_table, region, dest, flags);
    mem_request_ret_t t = {r.result, (void *)r.value};
    return t;
}

result_t release_region(uint64_t tid, void * region)
{
    return pmos_syscall(SYSCALL_DELETE_REGION, tid, region).result;
}

syscall_r init_stack(uint64_t tid, void * stack_top) 
{
    return pmos_syscall(SYSCALL_INIT_STACK, tid, stack_top);
}

page_table_req_ret_t get_page_table(uint64_t pid)
{
    syscall_r r = pmos_syscall(SYSCALL_GET_PAGE_TABLE, pid);
    page_table_req_ret_t t = {r.result, r.value};
    return t;
}

page_table_req_ret_t asign_page_table(uint64_t pid, uint64_t page_table, uint64_t flags)
{
    syscall_r r = pmos_syscall(SYSCALL_ASIGN_PAGE_TABLE, pid, page_table, flags);
    page_table_req_ret_t t = {r.result, r.value};
    return t;
}

result_t set_segment(uint64_t pid, unsigned segment, void * addr)
{
    return pmos_syscall(SYSCALL_SET_SEGMENT, pid, segment, addr).result;
}

syscall_r create_task_group()
{
    return pmos_syscall(SYSCALL_CREATE_TASK_GROUP);
}

result_t add_task_to_group(uint64_t group, uint64_t task)
{
    return pmos_syscall(SYSCALL_ADD_TASK_TO_GROUP, group, task).result;
}

result_t remove_task_from_group(uint64_t group, uint64_t task)
{
    return pmos_syscall(SYSCALL_REMOVE_TASK_FROM_GROUP, group, task).result;
}

syscall_r is_task_group_member(uint64_t task_id, uint64_t group_id)
{
    return pmos_syscall(SYSCALL_CHECK_IF_TASK_IN_GROUP, task_id, group_id);
}

syscall_r get_segment(uint64_t pid, unsigned segment)
{
    return pmos_syscall(SYSCALL_GET_SEGMENT, pid, segment);
}

result_t syscall_start_process(uint64_t pid, uint64_t entry, uint64_t arg1, uint64_t arg2, uint64_t arg3)
{
    return pmos_syscall(SYSCALL_START_PROCESS, pid, entry, arg1, arg2, arg3).result;
}

syscall_r set_task_group_notifier_mask(uint64_t task_group_id, pmos_port_t port_id, uint64_t new_mask)
{
    return pmos_syscall(SYSCALL_SET_NOTIFY_MASK, task_group_id, port_id, new_mask);
}

int _syscall_exit(unsigned long status1, unsigned long status2)
{
    return pmos_syscall(SYSCALL_EXIT, status1, status2).result;
}

result_t syscall_set_task_name(uint64_t tid, const char* name, size_t name_length)
{
    return pmos_syscall(SYSCALL_SET_TASK_NAME, tid, name, name_length).result;
}

result_t syscall_load_executable(uint64_t tid, uint64_t object_id, uint64_t flags)
{
    return pmos_syscall(SYSCALL_LOAD_EXECUTABLE, tid, object_id, flags).result;
}