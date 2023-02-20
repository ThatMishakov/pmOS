#include "named_ports.hh"
#include <processes/syscalls.hh>
#include <kernel/block.h>
#include <pmos/ipc.h>

Named_Port_Storage global_named_ports;

kresult_t Notify_Task::do_action(const klib::shared_ptr<Port>& port, [[maybe_unused]] const klib::string& name)
{
    if (not did_action) {
        klib::shared_ptr<TaskDescriptor> ptr = task.lock();

        if (ptr) {
            unblock_if_needed(ptr, parent_port.lock());
        }
        did_action = true;
    }
    return SUCCESS;
}

kresult_t Send_Message::do_action(const klib::shared_ptr<Port>& p, const klib::string& name)
{
    kresult_t result = SUCCESS;
    if (not did_action) {
        klib::shared_ptr<Port> ptr = port.lock();

        if (ptr) {
            size_t msg_size = sizeof(IPC_Kernel_Named_Port_Notification) + name.length();

            klib::vector<char> vec(msg_size);
            IPC_Kernel_Named_Port_Notification* ipc_ptr = (IPC_Kernel_Named_Port_Notification *)&vec.front();

            ipc_ptr->type = IPC_Kernel_Named_Port_Notification_NUM;
            ipc_ptr->reserved = 0;
            ipc_ptr->port_num = p->portno;
            
            memcpy(ipc_ptr->port_name, name.c_str(), name.length());

            Auto_Lock_Scope scope_lock(ptr->lock);
            result = ptr->send_from_system(klib::move(vec));
        } else {
            result = ERROR_PORT_CLOSED;
        }

        did_action = true;
    }
    return result;
        
}