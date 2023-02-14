#pragma once
#include <interrupts/interrupts.hh>
#include <lib/vector.hh>
#include <messaging/messaging.hh>
#include <lib/splay_tree_map.hh>
#include <types.hh>
#include <lib/stack.hh>
#include <lib/array.hh>
#include <lib/memory.hh>
#include <processes/tasks.hh>
#include "defs.hh"

// Checks the mask and unblocks the task if needed
bool unblock_if_needed(const klib::shared_ptr<TaskDescriptor>& p, u64 reason, u64 extra);

// Blocks task with a mask *mask*
ReturnStr<u64> block_task(const klib::shared_ptr<TaskDescriptor>& task, u64 mask, u64 imm_reason, u64 extra, bool check_unblock_immediate);

struct TaskDescriptor;

class sched_queue {
public:
    void push_front(const klib::shared_ptr<TaskDescriptor>& desc) noexcept;
    void push_back(const klib::shared_ptr<TaskDescriptor>& desc) noexcept;
    void erase(const klib::shared_ptr<TaskDescriptor>& desc) noexcept;

    // Returns front task or null pointer if empty
    klib::shared_ptr<TaskDescriptor> pop_front() noexcept;

    bool is_empty() noexcept;
    Spinlock lock;
private:
    klib::shared_ptr<TaskDescriptor> first;
    klib::shared_ptr<TaskDescriptor> last;
};

extern sched_queue blocked;
extern sched_queue uninit;
extern sched_queue dead_queue;



struct CPU_Info {
    CPU_Info* self = this; // 0
    u64* kernel_stack_top = nullptr; // 8
    u64 temp_var = 0; // 16
    klib::shared_ptr<TaskDescriptor> current_task = klib::shared_ptr<TaskDescriptor>(); // 24
    u64 nested_level = 0; // 40
    u64 jumpto_from = 0;  // 48
    u64 jumpto_to   = 0;  // 56
    Task_Regs nested_int_regs; // 64

    GDT cpu_gdt;

    klib::array<sched_queue, sched_queues_levels> sched_queues;
    klib::shared_ptr<TaskDescriptor> idle_task = klib::shared_ptr<TaskDescriptor>();

    u32 timer_val = 0;
    u32 lapic_id = 0;

    u64 pagefault_cr2 = 0;
    u64 pagefault_error = 0;

    Kernel_Stack_Pointer kernel_stack;
    Kernel_Stack_Pointer idle_stack;

    Kernel_Stack_Pointer nmi_stack;
    Kernel_Stack_Pointer machine_check_stack;
    Kernel_Stack_Pointer double_fault_stack;

    klib::shared_ptr<TaskDescriptor> atomic_pick_highest_priority();
    klib::shared_ptr<TaskDescriptor> atomic_get_front_priority(priority_t);
    klib::shared_ptr<TaskDescriptor> atomic_pick_lowest_priority(unsigned max_priority = 2);
};

quantum_t assign_quantum_on_priority(priority_t);

u32 calculate_timer_ticks(const klib::shared_ptr<TaskDescriptor>& task);

// static CPU_Info* const GSRELATIVE per_cpu = 0; // clang ignores GSRELATIVE for no apparent reason
extern "C" CPU_Info* get_cpu_struct();

inline klib::shared_ptr<TaskDescriptor> get_current_task()
{
    return get_cpu_struct()->current_task;
}

// Adds the task to the appropriate ready queue
void push_ready(const klib::shared_ptr<TaskDescriptor>& p);

// Initializes scheduling structures during the kernel initialization
void init_scheduling();

// Finds a ready process and switches to it
void task_switch();

// Finds and switches to a new process (e.g. if the current is blocked or executing for too long)
void find_new_process();

// Kills the task
void kill(const klib::shared_ptr<TaskDescriptor>& t);

// To be called from the clock routine
extern "C" void sched_periodic();

// Starts the scheduler
void start_scheduler();

// Pushes current processos to the back of sheduling queues
void evict(const klib::shared_ptr<TaskDescriptor>&);

// Switches to this process
void switch_to_task(const klib::shared_ptr<TaskDescriptor>& task);

// Saves and restores GSBase and FSBase
void save_segments(const klib::shared_ptr<TaskDescriptor>& task);
void restore_segments(const klib::shared_ptr<TaskDescriptor>& task);

// Reschedules the tasks
extern "C" void reschedule();