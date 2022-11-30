#include <pmos/system.h>
#include <kernel/attributes.h>
#include <unistd.h>
#include <stdint.h>


int pmos_request_io_permission()
{
    // TODO: Since kernel doesn't support permissions yet, I can just make a syscall
    // for it but this will need to be changes once that is implemented

    pid_t my_pid = getpid();

    uint64_t result = syscall(SYSCALL_SET_ATTR, my_pid, ATTR_ALLOW_PORT, 1).result;
    return result ? -1 : 0;
}