#include <pmos/system.h>
#include <kernel/syscalls.h>
#include <kernel/sysinfo.h>

syscall_r __pmos_get_system_info(uint32_t info);

int get_nprocs(void)
{
    return __pmos_get_system_info(SYSINFO_NPROCS).value;
}

int get_nprocs_conf(void)
{
    return __pmos_get_system_info(SYSINFO_NPROCS_CONF).value;
}