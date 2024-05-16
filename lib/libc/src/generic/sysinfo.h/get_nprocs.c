#include <pmos/system.h>
#include <kernel/syscalls.h>
#include <kernel/sysinfo.h>

int get_nprocs(void)
{
    return pmos_syscall(SYSCALL_GET_SYSTEM_INFO, SYSINFO_NPROCS).value;
}

int get_nprocs_conf(void)
{
    return pmos_syscall(SYSCALL_GET_SYSTEM_INFO, SYSINFO_NPROCS_CONF).value;
}