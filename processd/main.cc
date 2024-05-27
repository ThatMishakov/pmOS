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

#include <memory>
#include <pmos/ipc.h>
#include <pmos/ports.h>
#include <pmos/system.h>
#include <stdexcept>
#include <stdio.h>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

void send_message(pmos_port_t port, const auto &data)
{
    auto r = send_message_port(port, sizeof(data), reinterpret_cast<const void *>(&data));
    if (r != SUCCESS)
        throw std::system_error(-r, std::system_category());
}

pmos_port_t main_port = []() -> auto {
    ports_request_t request = create_port(TASK_ID_SELF, 0);
    if (request.result != SUCCESS)
        throw std::runtime_error("Failed to create port");
    return request.port;
}();

const std::string processd_port_name = "/pmos/processd";

struct Process;
struct Task {
    uint64_t task_id        = 0;
    bool present            = false;
    Process *parent_process = nullptr;
};
std::unordered_map<uint64_t, std::unique_ptr<Task>> tasks;

struct Process {
    uint64_t process_id            = 0;
    uint64_t process_task_group_id = 0;
    std::unordered_set<Task *> tasks;
};
std::unordered_map<uint64_t, std::unique_ptr<Process>> processes;
std::unordered_map<uint64_t, Process *> process_for_task_group;

void register_process_reply(pmos_port_t reply_port, unsigned flags, int result, uint64_t process_id)
{
    IPC_Register_Process_Reply reply;
    reply.type   = IPC_Register_Process_Reply_NUM;
    reply.flags  = flags;
    reply.result = result;
    reply.pid    = process_id;

    send_message(reply_port, reply);
}

void watch_group(uint64_t task_group_id)
{
    syscall_r r = set_task_group_notifier_mask(task_group_id, main_port,
                                               NOTIFICATION_MASK_DESTROYED |
                                                   NOTIFICATION_MASK_ON_REMOVE_TASK |
                                                   NOTIFICATION_MASK_ON_ADD_TASK,
                                               NOTIFY_FOR_EXISTING_TASKS);
    if (r.result != SUCCESS)
        throw std::system_error(-r.result, std::system_category());
}

// Reserve PID 1
pid_t next_process_id = 2;

void register_process(IPC_Register_Process *msg, uint64_t sender)
{
    uint64_t task_id = msg->worker_task_id == 0 ? sender : msg->worker_task_id;
    auto it          = tasks.find(task_id);
    if (it == tasks.end()) {
        try {
            auto process                   = std::make_unique<Process>();
            auto pp                        = process.get();
            process->process_id            = next_process_id++;
            process->process_task_group_id = msg->task_group_id;
            processes[process->process_id] = std::move(process);

            process_for_task_group[msg->task_group_id] = pp;

            auto task            = std::make_unique<Task>();
            auto tp              = task.get();
            task->task_id        = task_id;
            task->present        = true;
            task->parent_process = pp;
            tasks[task_id]       = std::move(task);

            pp->tasks.insert(tp);

            watch_group(msg->task_group_id);

            register_process_reply(msg->reply_port, 0, 0, pp->process_id);
        } catch (const std::system_error &e) {
            try {
                register_process_reply(msg->reply_port, 0, -e.code().value(), 0);
            } catch (...) {
            }

            tasks.erase(task_id);
            processes.erase(next_process_id--);
            process_for_task_group.erase(msg->task_group_id);
        }
    } else {
        // TODO
        try {
            register_process_reply(msg->reply_port, 0, -EEXIST, 0);
        } catch (...) {
        }
    }
}

void destroy_process(uint64_t task_group_id)
{
    auto it = process_for_task_group.find(task_group_id);
    if (it == process_for_task_group.end()) {
        printf("processd: Task group %lu not found when destroying a process\n", task_group_id);
        return;
    }

    // TODO: Do the notification stuff, etc.

    for (auto p: it->second->tasks) {
        tasks.erase(p->task_id);
    }
    processes.erase(it->second->process_id);
    process_for_task_group.erase(it);
}

void add_task_to_process(uint64_t task_group_id, uint64_t task_id)
{
    // Find task group
    auto it = process_for_task_group.find(task_group_id);
    if (it == process_for_task_group.end()) {
        printf("processd: Task group %lu not found\n", task_group_id);
        return;
    }

    auto it2 = tasks.find(task_id);
    if (it2 == tasks.end()) {
        auto task = std::make_unique<Task>();
        auto tp   = task.get();

        task->task_id        = task_id;
        task->present        = true;
        task->parent_process = it->second;
        tasks[task_id]       = std::move(task);

        it->second->tasks.insert(tp);
    } else {
        if (it2->second->parent_process == nullptr) {
            // TODO: Notify waiting tasks, etc.
            it2->second->parent_process = it->second;
            it2->second->present        = true;
            it->second->tasks.insert(it2->second.get());
        } else if (it2->second->parent_process != it->second) {
            printf("processd: Task %lu already belongs to another process\n", task_id);
        }
        // Else task is already in the group
    }
}

void remove_task_from_process(uint64_t task_group_id, uint64_t task_id)
{
    auto it = tasks.find(task_id);
    if (it == tasks.end()) {
        printf("processd: Task %lu not found\n", task_id);
        return;
    }
    auto task = it->second.get();

    if (task->parent_process == nullptr) {
        printf("processd: Task %lu does not belong to any process\n", task_id);
        return;
    }

    if (task->parent_process->process_task_group_id != task_group_id) {
        printf("processd: Task %lu does not belong to group %lu\n", task_id, task_group_id);
        return;
    }

    task->parent_process->tasks.erase(task);
    tasks.erase(task_id);
}

int main()
{
    {
        result_t r =
            name_port(main_port, processd_port_name.c_str(), processd_port_name.length(), 0);
        if (r != SUCCESS) {
            std::string error = "processd: Error " + std::to_string(r) + " naming port\n";
            fputs(error.c_str(), stderr);
        }
    }

    while (1) {
        Message_Descriptor msg;
        syscall_get_message_info(&msg, main_port, 0);

        std::unique_ptr<char[]> msg_buff = std::make_unique<char[]>(msg.size);
        get_first_message(msg_buff.get(), 0, main_port);

        if (msg.size < sizeof(IPC_Generic_Msg)) {
            printf("Warning: recieved very small message\n");
            break;
        }

        IPC_Generic_Msg *ipc_msg = reinterpret_cast<IPC_Generic_Msg *>(msg_buff.get());
        switch (ipc_msg->type) {
        case IPC_Register_Process_NUM: {
            if (msg.size < sizeof(IPC_Register_Process)) {
                printf("processd: Recieved IPC_Register_Process that is too small from task %li of "
                       "size %li\n",
                       msg.sender, msg.size);
                break;
            }

            IPC_Register_Process *m = reinterpret_cast<IPC_Register_Process *>(ipc_msg);
            register_process(m, msg.sender);
            break;
        }
        case IPC_Kernel_Group_Task_Changed_NUM: {
            if (msg.size < sizeof(IPC_Kernel_Group_Task_Changed)) {
                printf(
                    "processd: Recieved IPC_Kernel_Group_Task_Changed that is too small from task "
                    "%li of size %li\n",
                    msg.sender, msg.size);
                break;
            }

            IPC_Kernel_Group_Task_Changed *m =
                reinterpret_cast<IPC_Kernel_Group_Task_Changed *>(ipc_msg);
            if (m->event_type == Event_Group_Task_Added_NUM) {
                add_task_to_process(m->task_group_id, m->task_id);
            } else if (m->event_type == Event_Group_Task_Removed_NUM) {
                remove_task_from_process(m->task_group_id, m->task_id);
            }
            break;
        }
        case IPC_Kernel_Group_Destroyed_NUM: {
            if (msg.size < sizeof(IPC_Kernel_Group_Destroyed)) {
                printf("processd: Recieved IPC_Kernel_Group_Destroyed that is too small from task "
                       "%li of size %li\n",
                       msg.sender, msg.size);
                break;
            }

            IPC_Kernel_Group_Destroyed *msg =
                reinterpret_cast<IPC_Kernel_Group_Destroyed *>(ipc_msg);
            destroy_process(msg->task_group_id);
            break;
        }
        default:
            printf("processd: Unknown message type %d\n", ipc_msg->type);
            break;
        }
    }
}