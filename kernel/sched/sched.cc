#include "sched.hh"
#include <asm.hh>
#include <memory/paging.hh>
#include <types.hh>
#include <kernel/errors.h>
#include <linker.hh>
#include "processes/idle.hh"
#include <asm.hh>
#include <kernel/com.h>
#include <kernel/block.h>
#include <misc.hh>
#include <interrupts/apic.hh>
#include <cpus/cpus.hh>
#include <lib/memory.hh>
#include "timers.hh"
#include <cpus/sse.hh>
#include <interrupts/apic.hh>

sched_queue blocked;
sched_queue uninit;
sched_queue dead_queue;

Spinlock tasks_map_lock;
sched_map tasks_map;

PID pid = 1;

void init_scheduling()
{
    init_per_cpu();
    
    klib::shared_ptr<TaskDescriptor> current_task = TaskDescriptor::create();
    get_cpu_struct()->current_task = current_task;

    current_task->register_page_table(Page_Table::init_from_phys(getCR3()));

    auto result = current_task->page_table->create_phys_region(0x1000, GB(4), Page_Table::Readable | Page_Table::Writeable | Page_Table::Executable, true, "init_default_map", 0x1000);
    if (result.result != SUCCESS) {
        t_print_bochs("Error: Could not assign the page table to the first process. Error %i\n", result.result);
        return;
    }

    current_task->pid = pid++;    
    tasks_map.insert({current_task->pid, klib::move(current_task)});
}

DECLARE_LOCK(assign_pid);

PID assign_pid()
{
    LOCK(assign_pid)

    PID pid_p = pid++;

    UNLOCK(assign_pid)

   return pid_p; 
}

ReturnStr<u64> block_current_task(const klib::shared_ptr<Generic_Port>& ptr)
{
    const klib::shared_ptr<TaskDescriptor>& task = get_cpu_struct()->current_task;

    Auto_Lock_Scope scope_lock(task->sched_lock);

    task->status = Process_Status::PROCESS_BLOCKED;
    task->blocked_by = ptr;
    task->parent_queue = &blocked;

    {
        Auto_Lock_Scope scope_l(blocked.lock);
        blocked.push_back(task);
    }

    // Task switch
    find_new_process();

    return {SUCCESS, 0};
}


kresult_t TaskDescriptor::atomic_block_by_page(u64 page)
{
    if (status == PROCESS_BLOCKED)
        return ERROR_ALREADY_BLOCKED;

    Auto_Lock_Scope scope_lock(sched_lock);
    
    status = Process_Status::PROCESS_BLOCKED;
    page_blocked_by = page;

    klib::shared_ptr<TaskDescriptor> self = weak_self.lock();

    if (get_cpu_struct()->current_task == self) {
        find_new_process();
    } else if (parent_queue) {
        Auto_Lock_Scope scope_l(parent_queue->lock);
        parent_queue->erase(self);
    }
    
    {
        Auto_Lock_Scope scope_l(blocked.lock);
        blocked.push_back(self);
    }

    return SUCCESS;
}

void switch_to_task(const klib::shared_ptr<TaskDescriptor>& task)
{
    CPU_Info *c = get_cpu_struct();
    if (c->current_task->page_table != task->page_table) {
        setCR3(task->page_table->get_cr3());
    }

    save_segments(c->current_task);

    if (sse_is_valid()) {
        //t_print_bochs("Saving SSE registers for PID %h\n", c->current_task->pid);
        c->current_task->sse_data.save_sse();
        invalidate_sse();
    }

    // Change task
    task->status = Process_Status::PROCESS_RUNNING;
    c->current_task = task;

    restore_segments(c->current_task);

    start_timer_ticks(calculate_timer_ticks(task));
}

void save_segments(const klib::shared_ptr<TaskDescriptor>& task)
{
    task->regs.seg.gs = read_msr(0xC0000102); // KernelGSBase
    task->regs.seg.fs = read_msr(0xC0000100); // FSBase
}

void restore_segments(const klib::shared_ptr<TaskDescriptor>& task)
{
    write_msr(0xC0000102, task->regs.seg.gs); // KernelGSBase
    write_msr(0xC0000100, task->regs.seg.fs); // FSBase
}

bool unblock_if_needed(const klib::shared_ptr<TaskDescriptor>& p, const klib::shared_ptr<Generic_Port>& ptr)
{
    bool unblocked = false;
    Auto_Lock_Scope scope_lock(p->sched_lock);

    if (p->status != PROCESS_BLOCKED)
        return unblocked;

    if (p->page_blocked_by != 0)
        return unblocked;

    if (ptr and p->blocked_by.lock() == ptr) {
        unblocked = true;

        p->parent_queue->erase(p);
        p->parent_queue = nullptr;

        klib::shared_ptr<TaskDescriptor> current_task = get_cpu_struct()->current_task;
        if (current_task->priority > p->priority) {
            Auto_Lock_Scope scope_l(current_task->sched_lock);

            switch_to_task(p);

            push_ready(current_task);
        } else {
            push_ready(p);
        }
    }
    return unblocked;
}

// u64 TaskDescriptor::check_unblock_immediately(u64 reason, u64 extra)
// {
//     u64 mask = 0x01ULL << (reason - 1);

//     if ((this->unblock_mask & mask) and (this->block_extra == extra)) {
//         if (not this->messages.empty()) return reason;
//     }

//     return 0;
// }

void push_ready(const klib::shared_ptr<TaskDescriptor>& p)
{
    p->status = PROCESS_READY;

    priority_t priority = p->priority;
    CPU_Info* cpu_str = get_cpu_struct();

    if (priority < cpu_str->sched_queues.size()) {
        sched_queue* queue = &cpu_str->sched_queues[priority];
        p->parent_queue = queue;

        Auto_Lock_Scope lock(queue->lock);
        queue->push_back(p);
    } else {
        p->parent_queue = nullptr;
    }
}

void sched_periodic()
{
    // TODO: Replace with more sophisticated algorithm. Will definitely need to be redone once we have multi-cpu support

    CPU_Info* c = get_cpu_struct(); 

    klib::shared_ptr<TaskDescriptor> current = c->current_task;
    klib::shared_ptr<TaskDescriptor> next = c->atomic_get_front_priority(current->priority);

    if (next) {
        Auto_Lock_Scope_Double lock(current->sched_lock, next->sched_lock);

        current->status = Process_Status::PROCESS_READY;

        switch_to_task(next);

        push_ready(current);
    } else {
        start_timer_ticks(calculate_timer_ticks(current));
    }

    apic_eoi();
}

void start_scheduler()
{
    CPU_Info* s = get_cpu_struct();
    const klib::shared_ptr<TaskDescriptor>& t = s->current_task;

    start_timer_ticks(calculate_timer_ticks(t));
}

void reschedule()
{
    // TODO
}

u32 calculate_timer_ticks(const klib::shared_ptr<TaskDescriptor>& task)
{
    return assign_quantum_on_priority(task->priority)*ticks_per_1_ms;
}

klib::shared_ptr<TaskDescriptor> CPU_Info::atomic_pick_highest_priority()
{
    for (unsigned i = 0; i < sched_queues.size(); ++i) {
        auto& queue = sched_queues[i];

        Auto_Lock_Scope(queue.lock);

        klib::shared_ptr<TaskDescriptor> task = queue.pop_front();
        if (task != klib::shared_ptr<TaskDescriptor>(nullptr)) return task;
    }

    return nullptr;
}

void find_new_process()
{
    CPU_Info& cpu_str = *get_cpu_struct();

    klib::shared_ptr<TaskDescriptor> next_task = cpu_str.atomic_pick_highest_priority();

    if (not next_task)
        next_task = cpu_str.idle_task;

    switch_to_task(next_task);
}

klib::shared_ptr<TaskDescriptor> sched_queue::pop_front() noexcept
{
    klib::shared_ptr<TaskDescriptor> ptr = first;

    if (first != klib::shared_ptr<TaskDescriptor>(nullptr))
        erase(ptr);

    return ptr;
}

void sched_queue::push_back(const klib::shared_ptr<TaskDescriptor>& desc) noexcept
{
    if (first == klib::shared_ptr<TaskDescriptor>(nullptr)) {
        first = desc;
        last = desc;
        desc->queue_prev = nullptr;
    } else {
        last->queue_next = desc;
        desc->queue_prev = last;
        last = desc;
    }

    desc->queue_next = nullptr;
    desc->parent_queue = this;
}

void sched_queue::push_front(const klib::shared_ptr<TaskDescriptor>& desc) noexcept
{
    if (first == klib::shared_ptr<TaskDescriptor>(nullptr)) {
        first = desc;
        last = desc;
        desc->queue_next = nullptr;
    } else {
        desc->queue_next = first;
        first->queue_prev = desc;
        first = desc;
    }

    desc->queue_prev = nullptr;
    desc->parent_queue = this;
}

void sched_queue::erase(const klib::shared_ptr<TaskDescriptor>& desc) noexcept
{
    if (desc->queue_prev) {
        desc->queue_prev->queue_next = desc->queue_next;
    } else {
        first = desc->queue_next;
    }

    if (desc->queue_next) {
        desc->queue_next->queue_prev = desc->queue_prev;
    } else {
        last = desc->queue_prev;
    }

    desc->queue_prev = nullptr;
    desc->queue_next = nullptr;
    desc->parent_queue = nullptr;
}

quantum_t assign_quantum_on_priority(priority_t priority)
{
    static const quantum_t quantums[16] = {50, 50, 20, 20, 10, 10, 10, 5, 5, 5, 5, 5, 5, 5, 5, 5};

    if (priority < 16)
        return quantums[priority];

    return 0;
}

klib::shared_ptr<TaskDescriptor> CPU_Info::atomic_get_front_priority(priority_t priority)
{
    if (priority < sched_queues.size()) {
        sched_queue& queue = sched_queues[priority];

        Auto_Lock_Scope lock (queue.lock);

        return queue.pop_front();
    }

    return nullptr;
}

void request_repeat_syscall(const klib::shared_ptr<TaskDescriptor>& task)
{
    if (task->regs.entry_type != 3) {
        task->regs.saved_entry_type = task->regs.entry_type;
        task->regs.entry_type = 3;
    }
}

