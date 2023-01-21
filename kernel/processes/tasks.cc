#include "tasks.hh"
#include "kernel/errors.h"
#include <sched/sched.hh>
#include <sched/task_switching.hh>
#include "idle.hh"
#include <sched/defs.hh>


ReturnStr<klib::shared_ptr<TaskDescriptor>> create_process(u16 ring)
{
    // Create the structure
    klib::shared_ptr<TaskDescriptor> n = klib::make_shared<TaskDescriptor>();
    
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
    // TODO: Exceptions
    n->page_table = Page_Table::get_new_page_table();

    // Assign a pid
    n->pid = assign_pid();
    n->status = PROCESS_UNINIT;

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
    // Switch to the new pml4
    u64 current_cr3 = getCR3();

    Auto_Lock_Scope page_lock(this->page_table.shared_str->lock);
    setCR3(this->page_table.get_cr3());

    kresult_t r;
    // Prealloc a page for the stack
    u64 stack_end = (u64)KERNEL_ADDR_SPACE; //&_free_to_use;
    u64 stack_page_start = stack_end - KB(4);

    Page_Table_Argumments arg;
    arg.writeable = 1;
    arg.execution_disabled = 1;
    arg.global = 0;
    arg.user_access = 1;
    r = alloc_page_lazy(stack_page_start, arg, LAZY_FLAG_GROW_DOWN);  // TODO: This crashes real machines

    if (r != SUCCESS) goto fail;

    // Set new rsp
    this->regs.e.rsp = stack_end;
fail:
    // Load old page table back
    setCR3(current_cr3);
    return {r, this->regs.e.rsp};
}

void init_idle()
{
    ReturnStr<klib::shared_ptr<TaskDescriptor>> i = create_process(0);
    if (i.result != SUCCESS) {
        t_print_bochs("Error: failed to create the idle process!\n");
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

    // Init stack
    i.val->init_stack();
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

void kill(const klib::shared_ptr<TaskDescriptor>& p) // TODO: UNIX Signals
{
    Auto_Lock_Scope scope_lock(p->sched_lock);

    sched_queue* task_queue = p->parent_queue;
    if (task_queue != nullptr) {
        Auto_Lock_Scope queue_lock(task_queue->lock);

        task_queue->erase(p);
        p->parent_queue = nullptr;
    }

    // Task switch if it's a current process
    CPU_Info* cpu_str = get_cpu_struct();
    if (cpu_str->current_task == p) {
        find_new_process();
    }

    p->status = PROCESS_DEAD;
    p->parent_queue = &dead_queue;

    {
        Auto_Lock_Scope queue_lock(dead_queue.lock);
        dead_queue.push_back(p);
    }
}