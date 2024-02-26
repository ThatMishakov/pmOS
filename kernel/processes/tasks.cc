#include "tasks.hh"
#include "kernel/errors.h"
#include <sched/sched.hh>
#include "idle.hh"
#include <sched/defs.hh>
#include <exceptions.hh>


klib::shared_ptr<TaskDescriptor> TaskDescriptor::create_process(u16 ring)
{
    // Create the structure
    klib::shared_ptr<TaskDescriptor> n = TaskDescriptor::create();
    
    // Assign cs and ss
    // X86 specific!!!!!
    // switch (ring)
    // {
    // case 0:
    //     n->regs.e.cs = R0_CODE_SEGMENT;
    //     n->regs.e.ss = R0_DATA_SEGMENT;
    //     break;
    // case 3:
    //     n->regs.e.cs = R3_CODE_SEGMENT;
    //     n->regs.e.ss = R3_DATA_SEGMENT;
    //     break;
    // default:
    //     throw(Kern_Exception(ERROR_NOT_SUPPORTED, "create_process with unsupported ring"));
    //     break;
    // }

    // Assign a pid
    n->pid = assign_pid();

    // Add to the map of processes and to uninit list
    Auto_Lock_Scope l(tasks_map_lock);
    tasks_map.insert({n->pid, n});

    Auto_Lock_Scope uninit_lock(uninit.lock);
    uninit.push_back(n);

    return n;
}

u64 TaskDescriptor::init_stack()
{
    // TODO: Check if page table exists and that task is uninited


    // Prealloc a page for the stack
    u64 stack_end = page_table->user_addr_max(); //&_free_to_use;
    u64 stack_size = GB(2);
    u64 stack_page_start = stack_end - stack_size;

    static const klib::string stack_region_name = "default_stack";

    this->page_table->atomic_create_normal_region(
        stack_page_start, 
        stack_size, 
        Page_Table::Writeable | Page_Table::Readable, 
        true,
        stack_region_name, 
        -1);

    // Set new rsp
    this->regs.stack_pointer() = stack_end;

    return this->regs.stack_pointer();
}

void init_idle()
{
    try {
        // TODO!!!
        //static klib::shared_ptr<Page_Table> idle_page_table = x86_4level_Page_Table::create_empty();

        klib::shared_ptr<TaskDescriptor> i = TaskDescriptor::create_process(0);
        //i->register_page_table(idle_page_table);

        Auto_Lock_Scope lock(i->sched_lock);

        sched_queue* idle_parent_queue = i->parent_queue;
        if (idle_parent_queue != nullptr) {
            Auto_Lock_Scope q_lock(idle_parent_queue->lock);

            idle_parent_queue->erase(i);
        }


        CPU_Info* cpu_str = get_cpu_struct();
        cpu_str->idle_task = i;

        // Idle has the same stack pointer as the kernel
        // Also, one idle is created per CPU
        i->regs.thread_pointer() = (u64)cpu_str;

        // Init stack
        i->regs.stack_pointer() = (u64)cpu_str->idle_stack.get_stack_top();

        i->regs.program_counter() = (u64)&idle;
        i->type = TaskDescriptor::Type::Idle;

        i->priority = idle_priority;
        i->name = "idle";
    } catch (const Kern_Exception& e) {
        t_print_bochs("Error creating idle process: %i (%s)\n", e.err_code, e.err_message);
        throw e;
    }
}

bool TaskDescriptor::is_uninited() const
{
    return status == PROCESS_UNINIT;
}

void TaskDescriptor::init()
{
    klib::shared_ptr<TaskDescriptor> task = weak_self.lock();
    task->parent_queue->atomic_erase(task);

    klib::shared_ptr<TaskDescriptor> current_task = get_cpu_struct()->current_task;
    if (current_task->priority > task->priority) {
        Auto_Lock_Scope scope_l(current_task->sched_lock);

        task->switch_to();

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
    parent_queue = NULL;
}

void TaskDescriptor::create_new_page_table()
{
    Auto_Lock_Scope scope_lock(sched_lock);

    if (status != PROCESS_UNINIT)
        throw Kern_Exception(ERROR_PROCESS_INITED, "Process is already inited");

    if (page_table)
        throw Kern_Exception(ERROR_HAS_PAGE_TABLE, "Process already has a page table");

    // TODO!!!!
    //klib::shared_ptr<Page_Table> table = x86_4level_Page_Table::create_empty();

    //Auto_Lock_Scope page_table_lock(table->lock);
    //table->owner_tasks.insert(weak_self);
    //page_table = table;
}

void TaskDescriptor::register_page_table(klib::shared_ptr<Page_Table> table)
{
    Auto_Lock_Scope scope_lock(sched_lock);

    if (status != PROCESS_UNINIT)
        throw Kern_Exception(ERROR_PROCESS_INITED, "Process is already inited");

    if (page_table)
        throw Kern_Exception(ERROR_HAS_PAGE_TABLE, "Process already has a page table");

    Auto_Lock_Scope page_table_lock(table->lock);
    table->owner_tasks.insert(weak_self);
    page_table = table;
}

// TODO: Arch-specific!!!
// void TaskDescriptor::request_repeat_syscall()
// {
//     if (regs.entry_type != 3) {
//         regs.saved_entry_type = regs.entry_type;
//         regs.entry_type = 3;
//     }
// }

// void TaskDescriptor::pop_repeat_syscall()
// {
//     if (regs.entry_type == 3) {
//         regs.entry_type = regs.saved_entry_type;
//     }
// }