#include <pmos/system.h>
#include <kernel/types.h>
#include <pmos/ports.h>

syscall_r syscall_get_page(u64 addr)
{
    return syscall(SYSCALL_GET_PAGE, addr);
}

syscall_r map_into_range(u64 pid, u64 page_start, u64 to_addr, u64 nb_pages, u64 mask)
{
    return syscall(SYSCALL_MAP_INTO_RANGE, pid, page_start, to_addr, nb_pages, mask);
}

syscall_r syscall_new_process(uint8_t ring)
{
    return syscall(SYSCALL_CREATE_PROCESS, ring);
}

syscall_r get_page_multi(u64 base, u64 nb_pages)
{
    return syscall(SYSCALL_GET_PAGE_MULTI, base, nb_pages);
}

result_t syscall_release_page_multi(u64 base, u64 nb_pages)
{
    return syscall(SYSCALL_RELEASE_PAGE_MULTI, base, nb_pages).result;
}

syscall_r start_process(u64 pid, u64 entry)
{
    return syscall(SYSCALL_START_PROCESS, pid, entry);
}

syscall_r syscall_map_phys(u64 virt, u64 phys, u64 size, u64 arg)
{
    return syscall(SYSCALL_MAP_PHYS, virt, phys, size, arg);
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