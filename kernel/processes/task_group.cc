#include "task_group.hh"
#include "tasks.hh"
#include <pmos/ipc.h>

TaskGroup::~TaskGroup() noexcept
{
    atomic_remove_from_global_map();

    for (const auto & p : notifier_ports) {
        const auto &ptr = p.second.port.lock();
        if (ptr) {
            if (p.second.action_mask & NotifierPort::ACTION_MASK_ON_DESTROY) {
                IPC_Kernel_Group_Destroyed msg = {
                    .type = IPC_Kernel_Group_Destroyed_NUM,
                    .flags = 0,
                    .port_group_id = id,
                };

                try {
                    ptr->atomic_send_from_system(reinterpret_cast<char *>(&msg), sizeof(msg));
                } catch (...) {}
            }

            Auto_Lock_Scope l(ptr->notifier_ports_lock);
            ptr->notifier_ports.erase(id);
        }
    }
}

bool TaskGroup::atomic_has_task(u64 id) const noexcept
{
    Auto_Lock_Scope lock(tasks_lock);
    return tasks.count(id) == 1;
}

void TaskGroup::atomic_remove_from_global_map() const noexcept
{
    Auto_Lock_Scope lock(global_map_lock);
    global_map.erase(id);
}

void TaskGroup::atomic_add_to_global_map()
{
    Auto_Lock_Scope lock(global_map_lock);
    global_map.insert({id, weak_from_this()});
}

klib::shared_ptr<TaskGroup> TaskGroup::create()
{
    klib::shared_ptr<TaskGroup> group = klib::unique_ptr<TaskGroup>(new TaskGroup());

    group->atomic_add_to_global_map();

    return group;
}

void TaskGroup::atomic_register_task(klib::shared_ptr<TaskDescriptor> task)
{
    bool inserted = false;
    {
        Auto_Lock_Scope lock(task->task_groups_lock);
        inserted = task->task_groups.insert(shared_from_this()).second;
    }

    if (not inserted)
        throw Kern_Exception(ERROR_NAME_EXISTS, "Task already in group");

    Auto_Lock_Scope lock(tasks_lock);
    try {
        tasks.insert({task->pid, task});
    } catch (...) {
        Auto_Lock_Scope lock(task->task_groups_lock);
        task->task_groups.erase(shared_from_this());
        throw;
    }

    {
        Auto_Lock_Scope lock(notifier_ports_lock);
        for (const auto &p : notifier_ports) {
            if (p.second.action_mask & NotifierPort::ACTION_MASK_ON_ADD_TASK) {
                const auto &ptr = p.second.port.lock();
                if (ptr) {
                    IPC_Kernel_Group_Task_Changed msg = {
                        .type = IPC_Kernel_Group_Task_Changed_NUM,
                        .flags = 0,
                        .event_type = Event_Group_Task_Added_NUM,
                        .task_group_id = id,
                        .task_id = task->pid,
                    };

                    try {
                        ptr->atomic_send_from_system(reinterpret_cast<char *>(&msg), sizeof(msg));
                    } catch (...) {}
                }
            }
        }
    }
}

klib::shared_ptr<TaskGroup> TaskGroup::get_task_group_throw(u64 groupno)
{
    Auto_Lock_Scope scope_lock(global_map_lock);

    try {
        const auto group = global_map.at(groupno).lock();
        if (not group)
            throw (Kern_Exception(ERROR_PORT_DOESNT_EXIST, "requested task group does not exist"));

        return group;
    } catch (const std::out_of_range&) {
        throw (Kern_Exception(ERROR_PORT_DOESNT_EXIST, "requested task group does not exist"));
    }
}

void TaskGroup::atomic_remove_task(const klib::shared_ptr<TaskDescriptor>& task)
{
    {
        Auto_Lock_Scope lock(tasks_lock);
        if (tasks.count(task->pid) == 0)
            throw Kern_Exception(ERROR_PORT_DOESNT_EXIST, "Task not in group");

        tasks.erase(task->pid);
    }

    Auto_Lock_Scope lock(task->task_groups_lock);
    task->task_groups.erase(shared_from_this());

    {
        Auto_Lock_Scope lock(notifier_ports_lock);
        for (const auto &p : notifier_ports) {
            if (p.second.action_mask & NotifierPort::ACTION_MASK_ON_REMOVE_TASK) {
                const auto &ptr = p.second.port.lock();
                if (ptr) {
                    IPC_Kernel_Group_Task_Changed msg = {
                        .type = IPC_Kernel_Group_Task_Changed_NUM,
                        .flags = 0,
                        .event_type = Event_Group_Task_Removed_NUM,
                        .task_group_id = id,
                        .task_id = task->pid,
                    };

                    try {
                        ptr->atomic_send_from_system(reinterpret_cast<char *>(&msg), sizeof(msg));
                    } catch (...) {}
                }
            }
        }
    }
}

u64 TaskGroup::atomic_change_notifier_mask(const klib::shared_ptr<Port>& port, u64 mask)
{
    u64 old_mask = 0;
    const u64 port_id = port->portno;

    mask &= NotifierPort::ACTION_MASK_ALL;

    {
        Auto_Lock_Scope l(notifier_ports_lock);

        const auto count = notifier_ports.count(port->portno);

        if (count == 0) {
            // No port. Insert it
            old_mask = 0;
            if (mask == 0)
                return old_mask;

            auto t = notifier_ports.insert({port_id, {port, mask}});
            try {
                Auto_Lock_Scope l(port->notifier_ports_lock);
                port->notifier_ports.insert({id, weak_from_this()});
            } catch (...) {
                notifier_ports.erase(t.first);
                throw;
            }
        } else {
            auto& p = notifier_ports.at(port_id);
            old_mask = p.action_mask;
            p.action_mask = mask;

            if (mask == 0) {
                notifier_ports.erase(port_id);
                Auto_Lock_Scope l(port->notifier_ports_lock);
                port->notifier_ports.erase(port_id);
            }
        }
    }

    return old_mask;
}