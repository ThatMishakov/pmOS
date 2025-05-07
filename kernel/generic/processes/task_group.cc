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

#include "task_group.hh"

#include "tasks.hh"

#include <kern_logger/kern_logger.hh>
#include <pmos/ipc.h>
#include <pmos/utility/scope_guard.hh>

namespace kernel::proc
{

void TaskGroup::destroy()
{
    atomic_remove_from_global_map();

    for (const auto &p: notifier_ports) {
        auto ptr = ipc::Port::atomic_get_port(p.first);
        if (ptr) {
            if (p.second.action_mask & NotifierPort::ACTION_MASK_ON_DESTROY) {
                IPC_Kernel_Group_Destroyed msg = {
                    .type          = IPC_Kernel_Group_Destroyed_NUM,
                    .flags         = 0,
                    .task_group_id = id,
                };

                ptr->atomic_send_from_system(reinterpret_cast<char *>(&msg), sizeof(msg));
            }

            Auto_Lock_Scope l(ptr->notifier_ports_lock);
            ptr->notifier_ports.erase(this);
        }
    }

    auto first_right = [this] {
        Auto_Lock_Scope l(rights_lock);
        auto r = rights.begin();
        return r == rights.end() ? nullptr : &*r;
    };

    while (auto r = first_right())
        r->destroy(this);

    rcu_head.rcu_func = [](void *self, bool) {
        TaskGroup *t = reinterpret_cast<TaskGroup *>(reinterpret_cast<char *>(self) -
                                                     offsetof(TaskGroup, rcu_head));
        delete t;
    };
    sched::get_cpu_struct()->heap_rcu_cpu.push(&rcu_head);
}

bool TaskGroup::atomic_has_task(u64 id) const noexcept
{
    Auto_Lock_Scope lock(tasks_lock);
    return tasks.count(id) == 1;
}

void TaskGroup::atomic_remove_from_global_map() noexcept
{
    Auto_Lock_Scope lock(global_map_lock);
    global_map.erase(this);
}

void TaskGroup::atomic_add_to_global_map()
{
    Auto_Lock_Scope lock(global_map_lock);
    global_map.insert(this);
}

ReturnStr<TaskGroup *> TaskGroup::create_for_task(TaskDescriptor *task)
{
    assert(task);

    auto group        = new TaskGroup();
    auto delete_error = pmos::utility::make_scope_guard([group] { delete group; });

    if (!group) [[unlikely]]
        return Error(-ENOMEM);

    group->atomic_add_to_global_map();
    pmos::utility::scope_guard guard([=] { group->destroy(); });

    {
        Auto_Lock_Scope lock(task->sched_lock);
        auto s = task->status;
        if (s == TaskStatus::TASK_DYING || s == TaskStatus::TASK_DEAD)
            return nullptr;

        auto t = task->task_groups.insert(group);
        if (!t.second)
            return nullptr;
    }

    bool inserted = false;

    {
        Auto_Lock_Scope l(group->tasks_lock);
        auto r   = group->tasks.insert({task->task_id, task});
        inserted = r.second;
    }

    if (!inserted) {
        Auto_Lock_Scope lock(task->sched_lock);
        task->task_groups.erase(group);
        return Error(-ENOMEM);
    }

    guard.dismiss();
    delete_error.dismiss();

    return Success(group);
}

kresult_t TaskGroup::atomic_register_task(TaskDescriptor *task)
{
    bool inserted = false;
    {
        Auto_Lock_Scope lock(task->sched_lock);
        auto s = task->status;
        if (s == TaskStatus::TASK_DYING || s == TaskStatus::TASK_DEAD)
            return -ENOENT;

        auto t = task->task_groups.insert(this);
        if (t.first == task->task_groups.end())
            return -ENOMEM;
        inserted = t.second;
    }

    if (not inserted)
        return -EEXIST;

    auto guard = pmos::utility::make_scope_guard([&] {
        Auto_Lock_Scope lock(task->sched_lock);
        task->task_groups.erase(this);
    });

    {
        Auto_Lock_Scope lock(tasks_lock);
        if (!alive())
            return -ESRCH;

        auto r = tasks.insert({task->task_id, task});
        if (r.first == tasks.end())
            return -ENOMEM;
    }

    {
        Auto_Lock_Scope lock(notifier_ports_lock);
        for (const auto &p: notifier_ports) {
            if (p.second.action_mask & NotifierPort::ACTION_MASK_ON_ADD_TASK) {
                auto ptr = ipc::Port::atomic_get_port(p.first);
                if (ptr) {
                    IPC_Kernel_Group_Task_Changed msg = {
                        .type          = IPC_Kernel_Group_Task_Changed_NUM,
                        .flags         = 0,
                        .event_type    = Event_Group_Task_Added_NUM,
                        .task_group_id = id,
                        .task_id       = task->task_id,
                    };

                    auto result =
                        ptr->atomic_send_from_system(reinterpret_cast<char *>(&msg), sizeof(msg));
                    if (result) {
                        log::global_logger.printf("Warning: could not send message to port %i on "
                                                  "atomic_register_task: %i\n",
                                                  p.first, result);
                    }
                }
            }
        }
    }

    guard.dismiss();
    return 0;
}

kresult_t TaskGroup::atomic_remove_task(TaskDescriptor *task)
{
    {
        Auto_Lock_Scope lock(tasks_lock);
        if (tasks.count(task->task_id) == 0)
            return -ENOENT;

        tasks.erase(task->task_id);

        auto expected = this;
        task->rights_namespace.compare_exchange_strong(expected, nullptr, std::memory_order::release,
                                                       std::memory_order::consume);

        if (!alive())
            destroy();
    }

    // TODO: It looks like this doesn't work

    {
        Auto_Lock_Scope lock(notifier_ports_lock);
        for (const auto &p: notifier_ports) {
            if (p.second.action_mask & NotifierPort::ACTION_MASK_ON_REMOVE_TASK) {
                auto ptr = ipc::Port::atomic_get_port(p.first);
                if (ptr) {
                    IPC_Kernel_Group_Task_Changed msg = {
                        .type          = IPC_Kernel_Group_Task_Changed_NUM,
                        .flags         = 0,
                        .event_type    = Event_Group_Task_Removed_NUM,
                        .task_group_id = id,
                        .task_id       = task->task_id,
                    };

                    auto r =
                        ptr->atomic_send_from_system(reinterpret_cast<char *>(&msg), sizeof(msg));
                    if (r) {
                        log::global_logger.printf("Warning: could not send message to port %i on "
                                                  "atomic_remove_task: %i\n",
                                                  p.first, r);
                    }
                }
            }
        }
    }

    {
        Auto_Lock_Scope lock(task->sched_lock);
        task->task_groups.erase(this);
    }

    return 0;
}

ReturnStr<u32> TaskGroup::atomic_change_notifier_mask(ipc::Port *port, u32 mask, u32 flags)
{
    if (!atomic_alive())
        return -ESRCH;

    u32 old_mask      = 0;
    const u64 port_id = port->portno;

    mask &= NotifierPort::ACTION_MASK_ALL;

    {
        Auto_Lock_Scope l(notifier_ports_lock);

        const auto count = notifier_ports.count(port_id);

        if (count == 0) {
            // No port. Insert it
            old_mask = 0;
            if (mask == 0)
                return old_mask;

            auto t = notifier_ports.insert_noexcept({port_id, {mask}});
            if (t.first == notifier_ports.end())
                return Error(-ENOMEM);

            auto guard = pmos::utility::make_scope_guard([&] { notifier_ports.erase(t.first); });

            Auto_Lock_Scope l(port->notifier_ports_lock);
            auto res = port->notifier_ports.insert_noexcept(this);
            if (not res.second)
                return Error(-ENOMEM);

            guard.dismiss();
        } else {
            auto p = notifier_ports.find(port_id);
            assert(p != notifier_ports.end());

            old_mask              = p->second.action_mask;
            p->second.action_mask = mask;

            if (mask == 0) {
                notifier_ports.erase(port_id);
                Auto_Lock_Scope l(port->notifier_ports_lock);
                port->notifier_ports.erase(this);
            }
        }
    }

    if (flags & 0x01) {

        Auto_Lock_Scope tasks_l(tasks_lock);

        for (const auto &t: tasks) {
            auto task = t.second;
            if (not task)
                continue;
            IPC_Kernel_Group_Task_Changed msg = {
                .type          = IPC_Kernel_Group_Task_Changed_NUM,
                .flags         = 0,
                .event_type    = Event_Group_Task_Added_NUM,
                .task_group_id = id,
                .task_id       = task->task_id,
            };

            auto result =
                port->atomic_send_from_system(reinterpret_cast<char *>(&msg), sizeof(msg));
            if (result) {
                log::global_logger.printf("Warning: could not send message to port %i on "
                                          "atomic_change_notifier_mask: %i\n",
                                          port_id, result);
            }
        }
    }
    return old_mask;
}

TaskGroup *TaskGroup::get_task_group(u64 id)
{
    Auto_Lock_Scope lock(global_map_lock);
    auto p = global_map.find(id);
    if (p == global_map.end())
        return nullptr;

    return p;
}

bool TaskGroup::alive() const noexcept { return !tasks.empty(); }
bool TaskGroup::atomic_alive() const noexcept
{
    Auto_Lock_Scope l(tasks_lock);
    return alive();
}

bool TaskGroup::task_in_group(u64 id) const
{
    return tasks.count(id);
}

ipc::Right *TaskGroup::atomic_get_right(u64 right_id)
{
    Auto_Lock_Scope l(rights_lock);
    auto it = rights.find(right_id);
    return it == rights.end() ? nullptr : &*it;
}

u64 TaskGroup::atomic_new_right_id()
{
    Auto_Lock_Scope l(rights_lock);
    return ++current_right_id;
}

TaskGroup *kernel_tasks = nullptr;

} // namespace kernel::proc