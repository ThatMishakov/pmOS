#include "task_group.hh"
#include "tasks.hh"

TaskGroup::~TaskGroup() noexcept
{
    atomic_remove_from_global_map();
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
}

klib::shared_ptr<TaskGroup> TaskGroup::get_task_group_throw(u64 groupno)
{
    Auto_Lock_Scope scope_lock(global_map_lock);

    try {
        const auto group = global_map.at(groupno).lock();
        if (not group)
            throw (Kern_Exception(ERROR_PORT_DOESNT_EXIST, "requested task group does not exist"));

        return group;
    } catch (std::out_of_range) {
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
}
