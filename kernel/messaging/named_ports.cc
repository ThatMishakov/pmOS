#include "named_ports.hh"
#include <processes/syscalls.hh>
#include <kernel/block.h>

Named_Port_Storage global_named_ports;

kresult_t Notify_Task::do_action(u64 portnum)
{
    if (not did_action) {
        klib::shared_ptr<TaskDescriptor> ptr = task.lock();

        if (ptr) {
            bool unblocked = unblock_if_needed(ptr, PORTNAME_S_NUM, 0);
            if (unblocked)
                    syscall_ret_high(ptr) = portnum;
        }
        did_action = true;
    }
    return SUCCESS;
}