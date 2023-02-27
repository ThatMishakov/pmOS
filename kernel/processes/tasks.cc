#include "tasks.hh"
#include "kernel/errors.h"
#include <sched/sched.hh>
#include "idle.hh"
#include <sched/defs.hh>


ReturnStr<klib::shared_ptr<TaskDescriptor>> create_process(u16 ring)
{
    // Create the structure
    klib::shared_ptr<TaskDescriptor> n = TaskDescriptor::create();
    
    // Assign cs and ss
    switch (ring)
    {
    case 0:
        n->regs.e.cs = R0_CODE_SEGMENT;
        n->regs.e.ss = R0_DATA_SEGMENT;
        break;
    case 3:
        n->regs.e.cs = R3_CODE_SEGMENT;
        n->regs.e.ss = R3_DATA_SEGMENT;
        break;
    default:
        return {static_cast<kresult_t>(ERROR_NOT_SUPPORTED), klib::shared_ptr<TaskDescriptor>()};
        break;
    }

    // Create a new page table
    kresult_t result = n->create_new_page_table();
    if (result != SUCCESS)
        return {result, 0};

    // Assign a pid
    n->pid = assign_pid();

    // Add to the map of processes and to uninit list
    tasks_map_lock.lock();
    tasks_map.insert({n->pid, n});
    tasks_map_lock.unlock();

    Auto_Lock_Scope uninit_lock(uninit.lock);
    uninit.push_back(n);   

    // Success!
    return {SUCCESS, n};
}

ReturnStr<u64> TaskDescriptor::init_stack()
{
    kresult_t r;
    // Prealloc a page for the stack
    u64 stack_end = (u64)KERNEL_ADDR_SPACE; //&_free_to_use;
    u64 stack_size = GB(2);
    u64 stack_page_start = stack_end - stack_size;

    static const klib::string stack_region_name = "default_stack";

    r = this->page_table->atomic_create_normal_region(
        stack_page_start, 
        stack_size, 
        Page_Table::Writeable | Page_Table::Readable, 
        true,
        stack_region_name, 
        -1).result;

    if (r != SUCCESS)
        return {r, 0};

    // Set new rsp
    this->regs.e.rsp = stack_end;

    return {r, this->regs.e.rsp};
}

void init_idle()
{
    ReturnStr<klib::shared_ptr<TaskDescriptor>> i = create_process(0);
    if (i.result != SUCCESS) {
        t_print_bochs("Error: failed to create the idle process! Error: %i\n", i.result);
        return;
    }

    Auto_Lock_Scope lock(i.val->sched_lock);

    sched_queue* idle_parent_queue = i.val->parent_queue;
    if (idle_parent_queue != nullptr) {
        Auto_Lock_Scope q_lock(idle_parent_queue->lock);

        idle_parent_queue->erase(i.val);
    }


    CPU_Info* cpu_str = get_cpu_struct();
    cpu_str->idle_task = i.val;

    i.val->regs.seg.gs = (u64)cpu_str; // Idle has the same %gs as kernel

    // Init stack
    i.val->regs.e.rsp = (u64)cpu_str->idle_stack.get_stack_top();

    i.val->regs.e.rip = (u64)&idle;
    i.val->type = TaskDescriptor::Type::Idle;

    i.val->priority = idle_priority;
}

bool is_uninited(const klib::shared_ptr<const TaskDescriptor>& task)
{
    return task->status == PROCESS_UNINIT;
}

void init_task(const klib::shared_ptr<TaskDescriptor>& task)
{
    task->parent_queue->erase(task);
    task->parent_queue = nullptr;

    klib::shared_ptr<TaskDescriptor> current_task = get_cpu_struct()->current_task;
    if (current_task->priority > task->priority) {
        Auto_Lock_Scope scope_l(current_task->sched_lock);

        switch_to_task(task);

        push_ready(current_task);
    } else {
        push_ready(task);
    }
}

void TaskDescriptor::atomic_kill() // TODO: UNIX Signals
{
    klib::shared_ptr<TaskDescriptor> self = weak_self.lock();

    Auto_Lock_Scope scope_lock(sched_lock);

    sched_queue* task_queue = parent_queue;
    if (task_queue != nullptr) {
        Auto_Lock_Scope queue_lock(task_queue->lock);

        task_queue->erase(self);
        parent_queue = nullptr;
    }

    // Task switch if it's a current process
    CPU_Info* cpu_str = get_cpu_struct();
    if (cpu_str->current_task == self) {
        find_new_process();
    }

    status = PROCESS_DEAD;
    parent_queue = &dead_queue;

    {
        Auto_Lock_Scope queue_lock(dead_queue.lock);
        dead_queue.push_back(self);
    }

    page_table = nullptr;
}

kresult_t TaskDescriptor::create_new_page_table()
{
    Auto_Lock_Scope scope_lock(sched_lock);

    if (status != PROCESS_UNINIT)
        return ERROR_PROCESS_INITED;

    if (page_table)
        return ERROR_HAS_PAGE_TABLE;

    klib::shared_ptr<Page_Table> table = Page_Table::create_empty_page_table();

    Auto_Lock_Scope page_table_lock(table->lock);
    table->owner_tasks.insert(weak_self);
    page_table = table;

    return SUCCESS;
}

kresult_t TaskDescriptor::register_page_table(klib::shared_ptr<Page_Table> table)
{
    Auto_Lock_Scope scope_lock(sched_lock);

    if (status != PROCESS_UNINIT)
        return ERROR_PROCESS_INITED;

    if (page_table)
        return ERROR_HAS_PAGE_TABLE;

    Auto_Lock_Scope page_table_lock(table->lock);
    table->owner_tasks.insert(weak_self);
    page_table = table;

    return SUCCESS;
}