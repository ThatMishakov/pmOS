#include <pmos/system.h>
#include <kernel/types.h>
#include <pmos/ports.h>
#include <pmos/memory.h>

syscall_r syscall_new_process()
{
    return syscall(SYSCALL_CREATE_PROCESS);
}

syscall_r start_process(u64 pid, u64 entry)
{
    return syscall(SYSCALL_START_PROCESS, pid, entry);
}

result_t send_message_port(u64 port, size_t size, const char* message)
{
    return syscall(SYSCALL_SEND_MSG_PORT, port, size, message).result;
}

result_t get_first_message(char* buff, u64 args, u64 port)
{
    return syscall(SYSCALL_GET_MESSAGE, buff, args, port).result;
}

result_t syscall_get_message_info(Message_Descriptor* descr, u64 port, uint32_t flags)
{
    return syscall(SYSCALL_GET_MSG_INFO, descr, port, flags).result;
}

u64 getpid()
{
    return syscall(SYSCALL_GETPID).value;
}

result_t request_priority(uint64_t priority)
{
    return syscall(SYSCALL_SET_PRIORITY, priority).value;
}

u64 get_lapic_id()
{
    return syscall(SYSCALL_GET_LAPIC_ID).value;
}

ports_request_t create_port(pid_t owner, uint64_t flags)
{
    syscall_r r = syscall(SYSCALL_CREATE_PORT, owner, flags);
    ports_request_t t = {r.result, r.value};
    return t;
}

ports_request_t get_port_by_name(const char *name, u64 length, u32 flags)
{
    syscall_r r = syscall(SYSCALL_GET_PORT_BY_NAME, name, length, flags);
    ports_request_t t = {r.result, r.value};
    return t;
}

result_t set_interrupt(pmos_port_t port, uint32_t intno, uint32_t flags)
{
    return syscall(SYSCALL_SET_INTERRUPT, port, intno, flags).result;
}

result_t name_port(pmos_port_t portnum, const char* name, size_t length, u32 flags)
{
    return syscall(SYSCALL_NAME_PORT, portnum, name, length, flags).result;
}

result_t set_log_port(pmos_port_t port, uint32_t flags)
{
    return syscall(SYSCALL_SET_LOG_PORT, port, flags).result;
}

mem_request_ret_t create_phys_map_region(uint64_t pid, void *addr_start, size_t size, uint64_t access, void* phys_addr)
{
    syscall_r r = syscall(SYSCALL_CREATE_PHYS_REGION, pid, addr_start, size, access, phys_addr);
    mem_request_ret_t t = {r.result, (void *)r.value};
    return t;
}

mem_request_ret_t create_managed_region(uint64_t pid, void *addr_start, size_t size, uint64_t access, pmos_port_t port)
{
    syscall_r r = syscall(SYSCALL_CREATE_MANAGED_REGION, pid, addr_start, size, access, port);
    mem_request_ret_t t = {r.result, (void *)r.value};
    return t;
}

mem_request_ret_t create_normal_region(uint64_t pid, void *addr_start, size_t size, uint64_t access)
{
    syscall_r r = syscall(SYSCALL_CREATE_NORMAL_REGION, pid, addr_start, size, access);
    mem_request_ret_t t = {r.result, (void *)r.value};
    return t;
}

mem_request_ret_t transfer_region(uint64_t to_page_table, void * region, void * dest, uint64_t flags)
{
    syscall_r r = syscall(SYSCALL_TRANSFER_REGION, to_page_table, region, dest, flags);
    mem_request_ret_t t = {r.result, (void *)r.value};
    return t;
}

page_table_req_ret_t get_page_table(uint64_t pid)
{
    syscall_r r = syscall(SYSCALL_GET_PAGE_TABLE, pid);
    page_table_req_ret_t t = {r.result, r.value};
    return t;
}

result_t provide_page(uint64_t page_table, uint64_t dest_page, uint64_t source, uint64_t flags)
{
    return syscall(SYSCALL_PROVIDE_PAGE, page_table, dest_page, source, flags).result;
}

result_t set_segment(uint64_t pid, unsigned segment, void * addr)
{
    return syscall(SYSCALL_SET_SEGMENT, pid, segment, addr).result;
}