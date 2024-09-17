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

#include "named_ports.hh"

#include <kern_logger/kern_logger.hh>
#include <kernel/block.h>
#include <pmos/ipc.h>
#include <processes/syscalls.hh>
#include <processes/tasks.hh>

Named_Port_Storage global_named_ports;

void Notify_Task::do_action(Port *, [[maybe_unused]] const klib::string &name)
{
    // TODO: This is probably a race condition
    if (not did_action) {
        auto task = get_task(task_id);
        if (task)
            unblock_if_needed(task, parent_port);
        did_action = true;
    }
}

void Send_Message::do_action(Port *p, const klib::string &name)
{
    if (not did_action) {
        auto ptr = Port::atomic_get_port(port_id);

        if (ptr) {
            size_t msg_size = sizeof(IPC_Kernel_Named_Port_Notification) + name.length();

            klib::vector<char> vec;
            if (!vec.resize(msg_size))
                throw Kern_Exception(-ENOMEM, "Send_Message::do_action: failed to reserve memory");

            IPC_Kernel_Named_Port_Notification *ipc_ptr =
                (IPC_Kernel_Named_Port_Notification *)&vec.front();

            ipc_ptr->type     = IPC_Kernel_Named_Port_Notification_NUM;
            ipc_ptr->reserved = 0;
            ipc_ptr->port_num = p->portno;

            memcpy(ipc_ptr->port_name, name.c_str(), name.length());

            Auto_Lock_Scope scope_lock(ptr->lock);
            ptr->send_from_system(klib::move(vec));
        } else {
            serial_logger.printf("Send_Message::do_action: port is dead\n");
        }

        did_action = true;
    }
}

Notify_Task::Notify_Task(TaskDescriptor *t, Generic_Port *parent_port) noexcept
    : task_id(t->task_id), parent_port(parent_port) {};