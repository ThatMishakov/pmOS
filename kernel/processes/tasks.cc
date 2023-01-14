#include "tasks.hh"
#include "kernel/errors.h"

/*
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
    ReturnStr<u64> k = get_new_pml4();
    if (k.result != SUCCESS) {
        return {k.result, klib::shared_ptr<TaskDescriptor>()};
    }
    n->page_table = k.val;

    // Assign a pid
    n->pid = assign_pid();
    n->status = PROCESS_UNINIT;

    // Add to the map of processes and to uninit list
    tasks_map_lock.lock();
    tasks_map.insert({n->pid, n});
    tasks_map_lock.unlock();

    uninit.atomic_auto_push_back(n);    

    // Success!
    return {SUCCESS, n};
}
*/

// ReturnStr<u64> TaskDescriptor::init_stack()
// {
//     // Switch to the new pml4
//     u64 current_cr3 = getCR3();
//     setCR3(this->page_table);

//     kresult_t r;
//     // Prealloc a page for the stack
//     u64 stack_end = (u64)KERNEL_ADDR_SPACE; //&_free_to_use;
//     u64 stack_page_start = stack_end - KB(4);

//     Page_Table_Argumments arg;
//     arg.writeable = 1;
//     arg.execution_disabled = 1;
//     arg.global = 0;
//     arg.user_access = 1;
//     r = alloc_page_lazy(stack_page_start, arg, LAZY_FLAG_GROW_DOWN);  // TODO: This crashes real machines

//     if (r != SUCCESS) goto fail;

//     // Set new rsp
//     this->regs.e.rsp = stack_end;
// fail:
//     // Load old page table back
//     setCR3(current_cr3);
//     return {r, this->regs.e.rsp};
// }

/*
void init_idle()
{
    ReturnStr<klib::shared_ptr<TaskDescriptor>> i = create_process(0);
    if (i.result != SUCCESS) {
        t_print_bochs("Error: failed to create the idle process!\n");
        return;
    }

    CPU_Info* cpu_str = get_cpu_struct();
    cpu_str->idle_task = i.val;

    // Init stack
    i.val->init_stack();
    i.val->regs.e.rip = (u64)&idle;
    i.val->type = TaskDescriptor::Type::Idle;
    i.val->priority = TaskDescriptor::background_priority;
    Ready_Queues::assign_quantum_on_priority(i.val.get());

    i.val->queue_iterator->atomic_erase_from_parrent();
}
*/

bool is_uninited(const klib::shared_ptr<const TaskDescriptor>& task)
{
    return task->status == PROCESS_UNINIT;
}

// void init_task(const klib::shared_ptr<TaskDescriptor>& task)
// {
//     --- TODO ---
//     Ready_Queues::assign_quantum_on_priority(task.get());

//     CPU_Info* cpu_str = get_cpu_struct();
//     push_ready(task);

//     if (cpu_str->current_task->quantum_ticks == 0)
//         evict(cpu_str->current_task);
// }

// void kill(const klib::shared_ptr<TaskDescriptor>& p) // TODO: UNIX Signals
// {
//     Auto_Lock_Scope scope_lock(p->sched_lock);

//     // Add to dead queue
//     dead.atomic_auto_push_back(p);

//     // Release user pages
//     u64 ptable = p->page_table;
//     free_user_pages(ptable, p->pid);

//     /* --- TODO: This can be simplified now that I use shared_ptr ---*/

//     // Task switch if it's a current process
//     CPU_Info* cpu_str = get_cpu_struct();
//     if (cpu_str->current_task == p) {
//         cpu_str->current_task = nullptr;
//         cpu_str->release_old_cr3 = 1;
//         find_new_process();
//     } else {
//         release_cr3(ptable);
//     }
// }