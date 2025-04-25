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

#include <kernel/types.h>
#include <pmos/memory.h>
#include <pmos/ports.h>
#include <pmos/system.h>

#ifdef __i386__
    #define __32BITSYSCALL
#endif

#ifdef __32BITSYSCALL
syscall_r __pmos_syscall32_0words(unsigned syscall);
syscall_r __pmos_syscall32_1words(unsigned syscall, ...);
syscall_r __pmos_syscall32_2words(unsigned syscall, ...);
syscall_r __pmos_syscall32_3words(unsigned syscall, ...);
syscall_r __pmos_syscall32_4words(unsigned syscall, ...);
syscall_r __pmos_syscall32_5words(unsigned syscall, ...);
syscall_r __pmos_syscall32_6words(unsigned syscall, ...);
syscall_r __pmos_syscall32_7words(unsigned syscall, ...);
syscall_r __pmos_syscall32_8words(unsigned syscall, ...);
#endif

syscall_r syscall_new_process()
{
#ifdef __32BITSYSCALL
    return __pmos_syscall32_0words(SYSCALL_CREATE_PROCESS);
#else
    return pmos_syscall(SYSCALL_CREATE_PROCESS);
#endif
}

syscall_r start_process(uint64_t pid, ulong entry)
{
#ifdef __32BITSYSCALL
    return __pmos_syscall32_3words(SYSCALL_START_PROCESS, pid, entry);
#else
    return pmos_syscall(SYSCALL_START_PROCESS, pid, entry);
#endif
}

result_t send_message_port(uint64_t port, size_t size, const void *message)
{
#ifdef __32BITSYSCALL
    return __pmos_syscall32_4words(SYSCALL_SEND_MSG_PORT, port, size, (unsigned)message).result;
#else
    return pmos_syscall(SYSCALL_SEND_MSG_PORT, port, size, message).result;
#endif
}

#define SEND_MESSAGE_EXTENDED (1 << 16)
result_t send_message_port2(pmos_port_t port, mem_object_t object_id, size_t size, const void *message, unsigned flags)
{
    // Rationale behind this: pointer width changes between 32 and 64 bit systems, so the 64 bit object_id needs to
    // be passed before the pointers and size. The second syscall is so that the first one can fit into registers
    // on 32 bit x86.
#ifdef __32BITSYSCALL
    return __pmos_syscall32_6words(SYSCALL_SEND_MSG_PORT | SEND_MESSAGE_EXTENDED | (flags << 8), port, object_id, size, (unsigned)message).result;
#else
    return pmos_syscall(SYSCALL_SEND_MSG_PORT | SEND_MESSAGE_EXTENDED | (flags << 8), port, object_id, size, message).result;
#endif
}

result_t get_first_message(char *buff, uint32_t args, uint64_t port)
{
#ifdef __32BITSYSCALL
    return __pmos_syscall32_3words(SYSCALL_GET_MESSAGE | (args << 8), port, (unsigned)buff).result;
#else
    return pmos_syscall(SYSCALL_GET_MESSAGE | (args << 8), port, buff).result;
#endif
}

result_t syscall_get_message_info(Message_Descriptor *descr, uint64_t port, uint32_t flags)
{
#ifdef __32BITSYSCALL
    return __pmos_syscall32_3words(SYSCALL_GET_MSG_INFO | (flags << 8), port, (unsigned)descr)
        .result;
#else
    return pmos_syscall(SYSCALL_GET_MSG_INFO | (flags << 8), port, descr).result;
#endif
}

uint64_t get_task_id()
{
#ifdef __32BITSYSCALL
    return __pmos_syscall32_0words(SYSCALL_GET_TASK_ID).value;
#else
    return pmos_syscall(SYSCALL_GET_TASK_ID).value;
#endif
}

result_t request_priority(uint32_t priority)
{
#ifdef __32BITSYSCALL
    return __pmos_syscall32_1words(SYSCALL_SET_PRIORITY, priority).value;
#else
    return pmos_syscall(SYSCALL_SET_PRIORITY, priority).value;
#endif
}

syscall_r get_lapic_id(uint32_t cpu_id)
{
#ifdef __32BITSYSCALL
    return __pmos_syscall32_1words(SYSCALL_GET_LAPIC_ID, cpu_id);
#else
    return pmos_syscall(SYSCALL_GET_LAPIC_ID, cpu_id);
#endif
}

ports_request_t create_port(pid_t owner, uint32_t flags)
{
#ifdef __32BITSYSCALL
    syscall_r r = __pmos_syscall32_2words(SYSCALL_CREATE_PORT | (flags << 8), owner);
#else
    syscall_r r = pmos_syscall(SYSCALL_CREATE_PORT | (flags << 8), owner);
#endif

    ports_request_t t = {r.result, r.value};
    return t;
}

result_t pmos_delete_port(pmos_port_t port)
{
#ifdef __32BITSYSCALL
    return __pmos_syscall32_2words(SYSCALL_DELETE_PORT, port).result;
#else
    return pmos_syscall(SYSCALL_DELETE_PORT, port).result;
#endif
}

syscall_r __pmos_get_system_info(uint32_t info)
{
#ifdef __32BITSYSCALL
    return __pmos_syscall32_1words(SYSCALL_GET_SYSTEM_INFO, info);
#else
    return pmos_syscall(SYSCALL_GET_SYSTEM_INFO, info);
#endif
}

syscall_r __pmos_syscall_set_attr(uint64_t pid, uint32_t attr, unsigned long value)
{
#ifdef __32BITSYSCALL
    return __pmos_syscall32_4words(SYSCALL_SET_ATTR, pid, attr, value);
#else
    return pmos_syscall(SYSCALL_SET_ATTR, pid, attr, value);
#endif
}

result_t __pmos_request_timer(pmos_port_t port, uint64_t ns, uint64_t extra)
{
#ifdef __32BITSYSCALL
    return __pmos_syscall32_6words(SYSCALL_REQUEST_TIMER, port, ns, extra).result;
#else
    return pmos_syscall(SYSCALL_REQUEST_TIMER, port, ns, extra).result;
#endif
}

// ports_request_t get_port_by_name(const char *name, size_t length, uint32_t flags)
// {
// #ifdef __32BITSYSCALL
//     syscall_r r =
//         __pmos_syscall32_2words(SYSCALL_GET_PORT_BY_NAME | (flags << 8), (unsigned)name, length);
// #else
//     syscall_r r = pmos_syscall(SYSCALL_GET_PORT_BY_NAME | (flags << 8), name, length);
// #endif
//     ports_request_t t = {r.result, r.value};
//     return t;
// }

syscall_r set_interrupt(pmos_port_t port, uint32_t intno, uint32_t flags)
{
#ifdef __32BITSYSCALL
    return __pmos_syscall32_3words(SYSCALL_SET_INTERRUPT | (flags << 8), port, intno);
#else
    return pmos_syscall(SYSCALL_SET_INTERRUPT | (flags << 8), port, intno);
#endif
}

// result_t name_port(pmos_port_t portnum, const char *name, size_t length, uint32_t flags)
// {
// #ifdef __32BITSYSCALL
//     return __pmos_syscall32_4words(SYSCALL_NAME_PORT | (flags << 8), portnum, (unsigned)name,
//                                    length)
//         .result;
// #else
//     return pmos_syscall(SYSCALL_NAME_PORT | (portnum << 8), portnum, name, length).result;
// #endif
// }

result_t set_log_port(pmos_port_t port, uint32_t flags)
{
#ifdef __32BITSYSCALL
    return __pmos_syscall32_2words(SYSCALL_SET_LOG_PORT | (flags << 8), port).result;
#else
    return pmos_syscall(SYSCALL_SET_LOG_PORT | (flags << 8), port).result;
#endif
}

mem_request_ret_t create_phys_map_region(uint64_t pid, void *addr_start, size_t size,
                                         uint32_t access, uint64_t phys_addr)
{
#ifdef __32BITSYSCALL
    syscall_r r = __pmos_syscall32_6words(SYSCALL_CREATE_PHYS_REGION | (access << 8), pid,
                                          phys_addr, addr_start, size);
#else
    syscall_r r =
        pmos_syscall(SYSCALL_CREATE_PHYS_REGION | (access << 8), pid, phys_addr, addr_start, size);
#endif
    mem_request_ret_t t = {r.result, (void *)r.value};
    return t;
}

mem_request_ret_t create_normal_region(uint64_t pid, void *addr_start, size_t size, uint32_t access)
{
#ifdef __32BITSYSCALL
    syscall_r r = __pmos_syscall32_4words(SYSCALL_CREATE_NORMAL_REGION | (access << 8), pid,
                                          addr_start, size);
#else
    syscall_r r = pmos_syscall(SYSCALL_CREATE_NORMAL_REGION | (access << 8), pid, addr_start, size);
#endif
    mem_request_ret_t t = {r.result, (void *)r.value};
    return t;
}

mem_request_ret_t transfer_region(uint64_t to_page_table, void *region, void *dest, uint32_t flags)
{
#ifdef __32BITSYSCALL
    syscall_r r = __pmos_syscall32_4words(SYSCALL_TRANSFER_REGION | (flags << 8), to_page_table,
                                          region, dest);
#else
    syscall_r r = pmos_syscall(SYSCALL_TRANSFER_REGION | (flags << 8), to_page_table, region, dest);
#endif
    mem_request_ret_t t = {r.result, (void *)r.value};
    return t;
}

mem_request_ret_t map_mem_object(uint64_t page_table_id, void *addr_start, size_t size,
                                 uint32_t access, mem_object_t object_id, uint64_t offset)
{
#ifdef __32BITSYSCALL
    syscall_r r = __pmos_syscall32_8words(SYSCALL_MAP_MEM_OBJECT | (access << 8), page_table_id,
                                          object_id, offset, addr_start, size);
#else
    syscall_r r = pmos_syscall(SYSCALL_MAP_MEM_OBJECT | (access << 8), page_table_id, object_id,
                               offset, addr_start, size);
#endif
    mem_request_ret_t t = {r.result, (void *)r.value};
    return t;
}

result_t release_region(uint64_t tid, void *region)
{
#ifdef __32BITSYSCALL
    return __pmos_syscall32_3words(SYSCALL_DELETE_REGION, tid, region).result;
#else
    return pmos_syscall(SYSCALL_DELETE_REGION, tid, region).result;
#endif
}

syscall_r init_stack(uint64_t tid, void *stack_top)
{
#ifdef __32BITSYSCALL
    return __pmos_syscall32_3words(SYSCALL_INIT_STACK, tid, (unsigned)stack_top);
#else
    return pmos_syscall(SYSCALL_INIT_STACK, tid, stack_top);
#endif
}

page_table_req_ret_t get_page_table(uint64_t pid)
{
#ifdef __32BITSYSCALL
    syscall_r r = __pmos_syscall32_2words(SYSCALL_GET_PAGE_TABLE, pid);
#else
    syscall_r r = pmos_syscall(SYSCALL_GET_PAGE_TABLE, pid);
#endif
    page_table_req_ret_t t = {r.result, r.value};
    return t;
}

page_table_req_ret_t asign_page_table(uint64_t pid, uint64_t page_table, uint64_t flags)
{
#ifdef __32BITSYSCALL
    syscall_r r = __pmos_syscall32_4words(SYSCALL_ASIGN_PAGE_TABLE | (flags << 8), pid, page_table);
#else
    syscall_r r = pmos_syscall(SYSCALL_ASIGN_PAGE_TABLE | (flags << 8), pid, page_table);
#endif
    page_table_req_ret_t t = {r.result, r.value};
    return t;
}

result_t set_registers(uint64_t pid, unsigned segment, void *addr)
{
#ifdef __32BITSYSCALL
    return __pmos_syscall32_4words(SYSCALL_SET_REGISTERS, pid, segment, (unsigned)addr).result;
#else
    return pmos_syscall(SYSCALL_SET_REGISTERS, pid, segment, addr).result;
#endif
}

syscall_r create_task_group()
{
#ifdef __32BITSYSCALL
    return __pmos_syscall32_0words(SYSCALL_CREATE_TASK_GROUP);
#else
    return pmos_syscall(SYSCALL_CREATE_TASK_GROUP);
#endif
}

result_t add_task_to_group(uint64_t group, uint64_t task)
{
#ifdef __32BITSYSCALL
    return __pmos_syscall32_4words(SYSCALL_ADD_TASK_TO_GROUP, group, task).result;
#else
    return pmos_syscall(SYSCALL_ADD_TASK_TO_GROUP, group, task).result;
#endif
}

result_t remove_task_from_group(uint64_t group, uint64_t task)
{
#ifdef __32BITSYSCALL
    return __pmos_syscall32_4words(SYSCALL_REMOVE_TASK_FROM_GROUP, group, task).result;
#else
    return pmos_syscall(SYSCALL_REMOVE_TASK_FROM_GROUP, group, task).result;
#endif
}

syscall_r is_task_group_member(uint64_t task_id, uint64_t group_id)
{
#ifdef __32BITSYSCALL
    return __pmos_syscall32_4words(SYSCALL_CHECK_IF_TASK_IN_GROUP, task_id, group_id);
#else
    return pmos_syscall(SYSCALL_CHECK_IF_TASK_IN_GROUP, task_id, group_id);
#endif
}

result_t get_registers(uint64_t pid, unsigned segment, void *addr)
{
#ifdef __32BITSYSCALL
    return __pmos_syscall32_4words(SYSCALL_GET_REGISTERS, pid, segment, (unsigned)addr).result;
#else
    return pmos_syscall(SYSCALL_GET_REGISTERS, pid, segment, addr).result;
#endif
}

result_t syscall_start_process(uint64_t pid, unsigned long entry, unsigned long arg1,
                               unsigned long arg2, unsigned long arg3)
{
#ifdef __32BITSYSCALL
    return __pmos_syscall32_6words(SYSCALL_START_PROCESS, pid, entry, arg1, arg2, arg3).result;
#else
    return pmos_syscall(SYSCALL_START_PROCESS, pid, entry, arg1, arg2, arg3).result;
#endif
}

syscall_r set_task_group_notifier_mask(uint64_t task_group_id, pmos_port_t port_id,
                                       uint32_t new_mask, uint32_t flags)
{
#ifdef __32BITSYSCALL
    return __pmos_syscall32_5words(SYSCALL_SET_NOTIFY_MASK | (flags << 8), task_group_id, port_id,
                                   new_mask);
#else
    return pmos_syscall(SYSCALL_SET_NOTIFY_MASK | (flags << 8), task_group_id, port_id, new_mask);
#endif
}

int _syscall_exit(unsigned long status1, unsigned long status2)
{
#ifdef __32BITSYSCALL
    return __pmos_syscall32_2words(SYSCALL_EXIT, status1, status2).result;
#else
    return pmos_syscall(SYSCALL_EXIT, status1, status2).result;
#endif
}

result_t syscall_set_task_name(uint64_t tid, const char *name, size_t name_length)
{
#ifdef __32BITSYSCALL
    return __pmos_syscall32_4words(SYSCALL_SET_TASK_NAME, tid, (unsigned)name, name_length).result;
#else
    return pmos_syscall(SYSCALL_SET_TASK_NAME, tid, name, name_length).result;
#endif
}

result_t syscall_load_executable(uint64_t tid, uint64_t object_id, unsigned flags)
{
#ifdef __32BITSYSCALL
    return __pmos_syscall32_4words(SYSCALL_LOAD_EXECUTABLE | (flags << 8), tid, object_id).result;
#else
    return pmos_syscall(SYSCALL_LOAD_EXECUTABLE | (flags << 8), tid, object_id).result;
#endif
}

result_t set_affinity(uint64_t tid, uint32_t cpu_id, unsigned flags)
{
#ifdef __32BITSYSCALL
    return __pmos_syscall32_3words(SYSCALL_SET_AFFINITY | (flags << 8), tid, cpu_id).result;
#else
    return pmos_syscall(SYSCALL_SET_AFFINITY | (flags << 8), tid, cpu_id).result;
#endif
}

result_t complete_interrupt(uint32_t intno)
{
#ifdef __32BITSYSCALL
    return __pmos_syscall32_1words(SYSCALL_COMPLETE_INTERRUPT, intno).result;
#else
    return pmos_syscall(SYSCALL_COMPLETE_INTERRUPT, intno).result;
#endif
}

result_t pmos_yield()
{
#ifdef __32BITSYSCALL
    return __pmos_syscall32_0words(SYSCALL_YIELD).result;
#else
    return pmos_syscall(SYSCALL_YIELD).result;
#endif
}

syscall_r pmos_get_time(unsigned mode)
{
#ifdef __32BITSYSCALL
    return __pmos_syscall32_1words(SYSCALL_GET_TIME, mode);
#else
    return pmos_syscall(SYSCALL_GET_TIME, mode);
#endif
}

result_t syscall_kill_task(uint64_t tid)
{
#ifdef __32BITSYSCALL
    return __pmos_syscall32_2words(SYSCALL_KILL_TASK, tid).result;
#else
    return pmos_syscall(SYSCALL_KILL_TASK, tid).result;
#endif
}

// result_t request_named_port(const char *name, size_t name_length, pmos_port_t reply_port,
//                             unsigned flags)
// {
// #ifdef __32BITSYSCALL
//     return __pmos_syscall32_4words(SYSCALL_REQUEST_NAMED_PORT | (flags << 8), reply_port,
//                                    (unsigned)name, name_length)
//         .result;
// #else
//     return pmos_syscall(SYSCALL_REQUEST_NAMED_PORT | (flags << 8), reply_port, name, name_length)
//         .result;
// #endif
// }

result_t pause_task(uint64_t tid)
{
#ifdef __32BITSYSCALL
    return __pmos_syscall32_2words(SYSCALL_PAUSE_TASK, tid).result;
#else
    return pmos_syscall(SYSCALL_PAUSE_TASK, tid).result;
#endif
}

result_t resume_task(uint64_t tid)
{
#ifdef __32BITSYSCALL
    return __pmos_syscall32_2words(SYSCALL_RESUME_TASK, tid).result;
#else
    return pmos_syscall(SYSCALL_RESUME_TASK, tid).result;
#endif
}

result_t release_memory_range(uint64_t task_id, void *start, size_t size)
{
#ifdef __32BITSYSCALL
    return __pmos_syscall32_4words(SYSCALL_UNMAP_RANGE, task_id, (unsigned)start, size).result;
#else
    return pmos_syscall(SYSCALL_UNMAP_RANGE, task_id, start, size).result;
#endif
}

phys_addr_request_t get_page_phys_address(uint64_t task_id, void *region, uint64_t flags)
{
#ifdef __32BITSYSCALL
    syscall_r r =
        __pmos_syscall32_4words(SYSCALL_GET_PAGE_ADDRESS, task_id, (unsigned)region, flags);
#else
    syscall_r r = pmos_syscall(SYSCALL_GET_PAGE_ADDRESS, task_id, region, flags);
#endif
    phys_addr_request_t t = {r.result, r.value};
    return t;
}

mem_object_request_ret_t create_mem_object(size_t size, uint32_t flags)
{
#ifdef __32BITSYSCALL
    syscall_r r = __pmos_syscall32_4words(SYSCALL_CREATE_MEM_OBJECT | (flags << 8), size);
#else
    syscall_r r = pmos_syscall(SYSCALL_CREATE_MEM_OBJECT | (flags << 8), size);
#endif
    mem_object_request_ret_t t = {r.result, r.value};
    return t;
}

phys_addr_request_t get_page_phys_address_from_object(mem_object_t object_id, uint64_t offset,
                                                      unsigned flags)
{
#ifdef __32BITSYSCALL
    syscall_r r = __pmos_syscall32_4words(SYSCALL_MEM_OBJECT_GET_PAGE_ADDRESS | (flags << 8),
                                          object_id, offset);
#else
    syscall_r r =
        pmos_syscall(SYSCALL_MEM_OBJECT_GET_PAGE_ADDRESS | (flags << 8), object_id, offset);
#endif
    phys_addr_request_t t = {r.result, r.value};
    return t;
}

result_t release_mem_object(mem_object_t object_id, unsigned flags)
{
#ifdef __32BITSYSCALL
    return __pmos_syscall32_3words(SYSCALL_RELEASE_MEM_OBJECT | (flags << 8), object_id).result;
#else
    return pmos_syscall(SYSCALL_RELEASE_MEM_OBJECT | (flags << 8), object_id).result;
#endif
}

pmos_int_r allocate_interrupt(uint32_t gsi, uint32_t flags)
{
    syscall_r result;
#ifdef __32BITSYSCALL
    result = __pmos_syscall32_1words(SYSCALL_ALLOCATE_INTERRUPT | (flags << 8), gsi);
#else
    result = pmos_syscall(SYSCALL_ALLOCATE_INTERRUPT | (flags << 8), gsi);
#endif
    return (pmos_int_r) {
        .result = result.result,
        .cpu = result.value & 0xffffffff,
        .vector = result.value >> 32,
    };
}

result_t set_port0(pmos_port_t port)
{
    syscall_r result;
    #ifdef __32BITSYSCALL
    result = __pmos_syscall32_2words(SYSCALL_SET_PORT0, port);
#else
    result = pmos_syscall(SYSCALL_SET_PORT0, port);
#endif
    return result.result;
}

syscall_r set_namespace(uint64_t new_id, unsigned type)
{
    #ifdef __32BITSYSCALL
    return __pmos_syscall32_3words(SYSCALL_SET_NAMESPACE, new_id, type);
    #else
    return pmos_syscall(SYSCALL_SET_NAMESPACE, new_id, type);
    #endif
}

syscall_r create_right(uint64_t port_id, pmos_right_t *id_in_reciever, unsigned flags)
{
    #ifdef __32BITSYSCALL
    return __pmos_syscall32_3words(SYSCALL_CREATE_RIGHT | (flags << 8), port_id, id_in_reciever);
    #else
    return pmos_syscall(SYSCALL_CREATE_RIGHT | (flags << 8), port_id, id_in_reciever);
    #endif
}