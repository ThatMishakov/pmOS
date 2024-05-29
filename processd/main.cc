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

#include <cassert>
#include <memory>
#include <pmos/ipc.h>
#include <pmos/ports.h>
#include <pmos/system.h>
#include <stdexcept>
#include <stdio.h>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <variant>
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

template<class... Ts> struct overloaded: Ts... {
    using Ts::operator()...;
};

struct Process;
struct Task {
    uint64_t task_id        = 0;
    bool present            = false;
    Process *parent_process = nullptr;

    struct PIDRequest {
        pmos_port_t reply_port;
    };
    struct ParentPIDRequest {
        pmos_port_t reply_port;
    };
    struct PgroupRequest {
        pmos_port_t reply_port;
    };
    struct SetGroupRequest {
        pmos_port_t reply_port;
        int64_t pid;
        int64_t task_group_id;
    };

    using wait_var = std::variant<SetGroupRequest, PIDRequest, ParentPIDRequest, PgroupRequest>;
    std::vector<wait_var> waiting_ports;
};
std::unordered_map<uint64_t, std::unique_ptr<Task>> tasks;

struct Group;
struct Process {
    uint64_t process_id            = 0;
    uint64_t process_task_group_id = 0;
    std::unordered_set<Task *> tasks;
    Group *parent_group = nullptr;

    std::unordered_set<Process *> children;
    Process *parent  = nullptr;
    pid_t parent_pid = 1; // Save the parent pid for when the parent is destroyed

    bool is_group_leader() const;

    // TODO: is_session_leader
};
std::unordered_map<uint64_t, std::unique_ptr<Process>> processes;
std::unordered_map<uint64_t, Process *> process_for_task_group;

struct Group {
    uint64_t group_id = 0;
    std::unordered_set<Process *> processes;
    Process *leader = nullptr;
};
std::unordered_map<uint64_t, std::unique_ptr<Group>> groups;
void erase_process_from_group(Process *p)
{
    auto group = p->parent_group;
    group->processes.erase(p);
    if (p->is_group_leader()) {
        group->leader = nullptr;

        // TODO: Send signals to the group, etc.
    }

    if (group->processes.empty()) {
        groups.erase(group->group_id);
    }
}

bool Process::is_group_leader() const { return parent_group->leader == this; }

Group *get_default_group()
{
    auto it = groups.find(1);
    if (it == groups.end()) {
        auto group      = std::make_unique<Group>();
        auto gp         = group.get();
        group->group_id = 1;
        groups[1]       = std::move(group);
        return gp;
    } else {
        return it->second.get();
    }
}

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

void set_group_reply(pmos_port_t reply_port, int result, pid_t group_id)
{
    IPC_Set_Process_Group_Reply reply;
    reply.type     = IPC_Set_Process_Group_Reply_NUM;
    reply.flags    = 0;
    reply.result   = result;
    reply.group_id = group_id;

    try {
        send_message(reply_port, reply);
    } catch (std::system_error &e) {
        printf("processd: Error %d sending message to port %lu for set_group_reply\n",
               e.code().value(), reply_port);
    }
}

void process_set_group(Task::SetGroupRequest req, Task *t)
{
    assert(t);
    assert(t->parent_process);

    auto p         = t->parent_process;
    pid_t pid      = req.pid == 0 ? p->process_id : req.pid;
    pid_t group_id = req.task_group_id == 0 ? p->process_task_group_id : req.task_group_id;

    if (pid < 1 || group_id < 1) {
        // The value of the pgid argument is less than 0, or is not a value supported by the
        // implementation.
        set_group_reply(req.reply_port, -EINVAL, 0);
        return;
    }

    auto it = process_for_task_group.find(group_id);
    if (it == process_for_task_group.end()) {
        // The value of the pid argument does not match the process ID of the calling process or of
        // a child process of the calling process.
        set_group_reply(req.reply_port, -ESRCH, 0);
        return;
    }

    auto target_process = it->second;

    if (target_process != p && target_process->parent != p) {
        // The value of the pid argument does not match the process ID of the calling process or of
        // a child process of the calling process.
        set_group_reply(req.reply_port, -ESRCH, 0);
        return;
    }

    // if (target_process->is_session_leader()) {
    //     // The process group ID of a process group leader cannot be changed.
    //     set_group_reply(req.reply_port, -EPERM, 0);
    //     return;
    // }

    // Verify session
    // if (p->session != target_process->session) {
    //     // The value of the pid argument matches the process ID of a child process of the calling
    //     process and the child process is not in the same session as the calling process.
    //     set_group_reply(req.reply_port, -EPERM, 0);
    //     return;
    // }

    if (pid == group_id) {
        if (p->parent_group->leader == p) {
            set_group_reply(req.reply_port, 0, group_id);
            return;
        }

        erase_process_from_group(p);

        auto group       = std::make_unique<Group>();
        auto gp          = group.get();
        group->group_id  = group_id;
        groups[group_id] = std::move(group);

        gp->processes.insert(target_process);
        gp->leader = target_process;

        target_process->parent_group = gp;

        set_group_reply(req.reply_port, 0, group_id);
    } else {
        auto it = groups.find(group_id);
        if (it == groups.end()) {
            // Target process group does not exist
            set_group_reply(req.reply_port, -EPERM, 0);
            return;
        }

        // Verify the session

        erase_process_from_group(p);
        it->second->processes.insert(p);
        p->parent_group = it->second.get();

        set_group_reply(req.reply_port, 0, group_id);
    }
}

void process_requests(Task *t)
{
    for (auto port: t->waiting_ports) {
        std::visit(
            overloaded {
                [t](Task::PIDRequest r) {
                    auto pp = t->parent_process;
                    try {
                        IPC_PID_For_Task_Reply reply;
                        reply.type   = IPC_PID_For_Task_Reply_NUM;
                        reply.flags  = 0;
                        reply.result = 0;
                        reply.pid    = pp->process_id;

                        send_message(r.reply_port, reply);
                    } catch (std::system_error &e) {
                        printf(
                            "processd: Error %d sending message to port %lu for waiting task %lu\n",
                            e.code().value(), r.reply_port, t->task_id);
                    }
                },
                [t](Task::ParentPIDRequest r) {
                    auto pp = t->parent_process;
                    try {
                        IPC_PID_For_Task_Reply reply;
                        reply.type   = IPC_PID_For_Task_Reply_NUM;
                        reply.flags  = 0;
                        reply.result = 0;
                        reply.pid    = pp->parent_pid;
                        send_message(r.reply_port, reply);
                    } catch (std::system_error &e) {
                        printf(
                            "processd: Error %d sending message to port %lu for waiting task %lu\n",
                            e.code().value(), r.reply_port, t->task_id);
                    }
                },
                [t](Task::PgroupRequest r) {
                    auto pp = t->parent_process;
                    try {
                        IPC_PID_For_Task_Reply reply;
                        reply.type   = IPC_PID_For_Task_Reply_NUM;
                        reply.flags  = 0;
                        reply.result = 0;
                        reply.pid    = pp->parent_group->group_id;
                        send_message(r.reply_port, reply);
                    } catch (std::system_error &e) {
                        printf(
                            "processd: Error %d sending message to port %lu for waiting task %lu\n",
                            e.code().value(), r.reply_port, t->task_id);
                    }
                },
                [t](Task::SetGroupRequest r) { process_set_group(r, t); }},
            port);
    }
    t->waiting_ports.clear();
}

void register_process(IPC_Register_Process *msg, uint64_t sender)
{
    uint64_t task_id = msg->worker_task_id == 0 ? sender : msg->worker_task_id;
    auto it          = tasks.find(task_id);
    Group *group     = get_default_group();
    Process *pp      = nullptr;
    if (it == tasks.end()) {
        try {
            auto process                   = std::make_unique<Process>();
            pp                             = process.get();
            process->process_id            = next_process_id++;
            process->process_task_group_id = msg->task_group_id;
            processes[process->process_id] = std::move(process);
            pp->parent_group               = group;

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

            group->processes.erase(pp);
            tasks.erase(task_id);
            processes.erase(next_process_id--);
            process_for_task_group.erase(msg->task_group_id);
        }
    } else {
        if (it->second->parent_process == nullptr)
            try {
                auto process                   = std::make_unique<Process>();
                pp                             = process.get();
                process->process_id            = next_process_id++;
                process->process_task_group_id = msg->task_group_id;
                processes[process->process_id] = std::move(process);

                process_for_task_group[msg->task_group_id] = pp;

                it->second->parent_process = pp;
                it->second->present        = true;
                pp->tasks.insert(it->second.get());

                watch_group(msg->task_group_id);

                register_process_reply(msg->reply_port, 0, 0, pp->process_id);

                process_requests(it->second.get());
            } catch (const std::system_error &e) {
                try {
                    register_process_reply(msg->reply_port, 0, -e.code().value(), 0);
                } catch (std::system_error &e) {
                    printf("processd: Error %d sending message to port %lu\n", e.code().value(),
                           msg->reply_port);
                }

                group->processes.erase(pp);
                processes.erase(next_process_id--);
                process_for_task_group.erase(msg->task_group_id);
            }
        else if (it->second->parent_process->process_task_group_id == 0) {
            it->second->parent_process->process_task_group_id = msg->task_group_id;
            process_for_task_group[msg->task_group_id]        = it->second->parent_process;

            watch_group(msg->task_group_id);

            register_process_reply(msg->reply_port, 0, 0, it->second->parent_process->process_id);
        } else if (it->second->parent_process->process_task_group_id != msg->task_group_id) {
            register_process_reply(msg->reply_port, 0, -EEXIST, 0);
        } else {
            try {
                register_process_reply(msg->reply_port, REGISTER_PROCESS_REPLY_FLAG_EXISITING, 0,
                                       it->second->parent_process->process_id);
            } catch (std::system_error &e) {
                printf("processd: Error %d sending message to port %lu for task %lu for "
                       "register_process\n",
                       e.code().value(), msg->reply_port, task_id);
            }
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

    erase_process_from_group(it->second);

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

            process_requests(it2->second.get());
        } else if (it2->second->parent_process != it->second) {
            printf("processd: Task %lu already belongs to another process\n", task_id);
        }
        // Else task is already in the group
    }
}

void pid_for_task(IPC_PID_For_Task *m, uint64_t sender_task_id)
{
    uint64_t task_id = m->task_id == 0 ? sender_task_id : m->task_id;

    auto it = tasks.find(task_id);
    if (it == tasks.end()) {
        if (m->flags & PID_FOR_TASK_WAIT_TO_APPEAR) {
            auto task = std::make_unique<Task>();
            auto tp   = task.get();

            task->task_id  = task_id;
            task->present  = false;
            tasks[task_id] = std::move(task);

            if (m->flags & PID_FOR_TASK_PARENT_PID) {
                tp->waiting_ports.push_back({Task::ParentPIDRequest {m->reply_port}});
            } else if (m->flags & PID_FOR_TASK_GROUP_ID) {
                tp->waiting_ports.push_back({Task::PgroupRequest {m->reply_port}});
            } else {
                tp->waiting_ports.push_back({Task::PIDRequest {m->reply_port}});
            }
        } else {
            IPC_PID_For_Task_Reply reply;
            reply.type   = IPC_PID_For_Task_Reply_NUM;
            reply.flags  = 0;
            reply.result = -ENOENT;
            reply.pid    = 0;

            send_message(m->reply_port, reply);
        }
    } else {
        if (it->second->present) {
            auto task = it->second.get();
            pid_t pid;
            if (m->flags & PID_FOR_TASK_PARENT_PID)
                pid = task->parent_process->parent_pid;
            else if (m->flags & PID_FOR_TASK_GROUP_ID)
                pid = task->parent_process->parent_group->group_id;
            else
                pid = task->parent_process->process_id;
            IPC_PID_For_Task_Reply reply;
            reply.type   = IPC_PID_For_Task_Reply_NUM;
            reply.flags  = 0;
            reply.result = 0;
            reply.pid    = pid;

            send_message(m->reply_port, reply);
        } else {
            if (m->flags & PID_FOR_TASK_PARENT_PID)
                it->second->waiting_ports.push_back(Task::ParentPIDRequest {m->reply_port});
            else if (m->flags & PID_FOR_TASK_GROUP_ID)
                it->second->waiting_ports.push_back(Task::PgroupRequest {m->reply_port});
            else
                it->second->waiting_ports.push_back(Task::PIDRequest {m->reply_port});
        }
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

void set_group(IPC_Set_Process_Group *m, uint64_t task_id)
{
    auto it = tasks.find(task_id);
    if (it == tasks.end()) {
        auto task = std::make_unique<Task>();
        auto tp   = task.get();

        task->task_id        = task_id;
        task->present        = false;
        task->parent_process = nullptr;

        tp->waiting_ports.push_back(Task::SetGroupRequest {m->reply_port, m->pid, m->pgid});
    } else {
        if (it->second->present) {
            process_set_group({m->reply_port, m->pid, m->pgid}, it->second.get());
        } else {
            it->second->waiting_ports.push_back(
                Task::SetGroupRequest {m->reply_port, m->pid, m->pgid});
        }
    }
}

void preregister_process_try_reply(pmos_port_t reply_port, unsigned flags, pid_t result)
{
    IPC_Preregister_Process_Reply reply;
    reply.type   = IPC_Preregister_Process_Reply_NUM;
    reply.flags  = flags;
    reply.result = result;

    try {
        send_message(reply_port, reply);
    } catch (std::system_error &e) {
        printf("processd: Error %d sending message to port %lu for preregister_process\n",
               e.code().value(), reply_port);
    }
}

void preregister_process(IPC_Preregister_Process *m, uint64_t sender_task)
{
    auto it = tasks.find(sender_task);
    if (it == tasks.end()) {
        preregister_process_try_reply(m->reply_port, 0, -ENOENT);
        return;
    }

    auto current = it->second.get();
    auto parent  = current->parent_process;
    if (!current->present) {
        preregister_process_try_reply(m->reply_port, 0, -ENOENT);
        return;
    }

    pid_t parent_pid = m->parent_pid == 0 ? parent->process_id : m->parent_pid;
    if ((parent_pid <= 0) || (uint64_t)parent_pid != parent->process_id) {
        preregister_process_try_reply(m->reply_port, 0, -EPERM);
        return;
    }

    it = tasks.find(m->worker_task_id);
    if (it == tasks.end()) {
        it                  = tasks.emplace(m->worker_task_id, std::make_unique<Task>()).first;
        it->second->task_id = m->worker_task_id;
    } else if (it->second->present) {
        preregister_process_try_reply(m->reply_port, 0, -EEXIST);
        return;
    }
    auto task = it->second.get();

    auto process                   = std::make_unique<Process>();
    auto pp                        = process.get();
    process->process_id            = next_process_id++;
    processes[process->process_id] = std::move(process);

    pp->parent_pid = parent_pid;
    pp->parent     = parent;
    parent->children.insert(pp);

    pp->parent_group = parent->parent_group;
    pp->parent_group->processes.insert(pp);

    pp->tasks.insert(task);
    task->parent_process = pp;
    task->present        = true;

    preregister_process_try_reply(m->reply_port, 0, pp->process_id);

    process_requests(task);
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
                printf("processd: Recieved IPC_Register_Process that is too small from task "
                       "%li of "
                       "size %li\n",
                       msg.sender, msg.size);
                break;
            }

            IPC_Register_Process *m = reinterpret_cast<IPC_Register_Process *>(ipc_msg);
            register_process(m, msg.sender);
            break;
        }
        case IPC_PID_For_Task_NUM: {
            if (msg.size < sizeof(IPC_PID_For_Task)) {
                printf("processd: Recieved IPC_PID_For_Task that is too small from task %li of "
                       "size "
                       "%li\n",
                       msg.sender, msg.size);
                break;
            }

            IPC_PID_For_Task *m = reinterpret_cast<IPC_PID_For_Task *>(ipc_msg);
            pid_for_task(m, msg.sender);
            break;
        }
        case IPC_Set_Process_Group_NUM: {
            if (msg.size < sizeof(IPC_Set_Process_Group)) {
                printf("processd: Recieved IPC_Set_Process_Group that is too small from task %li "
                       "of size "
                       "%li\n",
                       msg.sender, msg.size);
                break;
            }

            IPC_Set_Process_Group *m = reinterpret_cast<IPC_Set_Process_Group *>(ipc_msg);
            set_group(m, msg.sender);
            break;
        }
        case IPC_Preregister_Process_NUM: {
            if (msg.size < sizeof(IPC_Preregister_Process)) {
                printf("processd: Recieved IPC_Preregister_Process that is too small from task %li "
                       "of size "
                       "%li\n",
                       msg.sender, msg.size);
                break;
            }

            IPC_Preregister_Process *m = reinterpret_cast<IPC_Preregister_Process *>(ipc_msg);
            preregister_process(m, msg.sender);
            break;
        }
        case IPC_Kernel_Group_Task_Changed_NUM: {
            if (msg.size < sizeof(IPC_Kernel_Group_Task_Changed)) {
                printf("processd: Recieved IPC_Kernel_Group_Task_Changed that is too small "
                       "from task "
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