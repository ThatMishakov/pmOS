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

// Checks the mask and unblocks the task if needed
void unblock_if_needed(const klib::shared_ptr<TaskDescriptor>& p, u64 reason);

// Blocks task with a mask *mask*
ReturnStr<u64> block_task(const klib::shared_ptr<TaskDescriptor>& task, u64 mask);

struct TaskDescriptor;

class sched_queue {
public:
    void atomic_push_front(const klib::shared_ptr<TaskDescriptor>& desc) noexcept;
    void atomic_push_back(const klib::shared_ptr<TaskDescriptor>& desc) noexcept;

    // Returns front task or null pointer if empty
    klib::shared_ptr<TaskDescriptor> pop_front() noexcept;

    bool atomic_is_empty() noexcept;
private:
    klib::shared_ptr<TaskDescriptor> first;
    klib::shared_ptr<TaskDescriptor> last;
    Spinlock lock;
};

extern sched_queue blocked;
extern sched_queue uninit;
extern sched_queue dead;



struct CPU_Info {
    CPU_Info* self = this; // 0
    Stack* kernel_stack = nullptr; // 8
    klib::shared_ptr<TaskDescriptor> current_task = klib::shared_ptr<TaskDescriptor>(); // 24
    klib::shared_ptr<TaskDescriptor> next_task = klib::shared_ptr<TaskDescriptor>(); // 40

    klib::shared_ptr<TaskDescriptor> idle_task = klib::shared_ptr<TaskDescriptor>();

    klib::array<sched_queue, 16> sched_queues;
};

// static CPU_Info* const GSRELATIVE per_cpu = 0; // clang ignores GSRELATIVE for no apparent reason
extern "C" CPU_Info* get_cpu_struct();

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
void sched_periodic();

// Starts the scheduler
void start_scheduler();

// Pushes current processos to the back of sheduling queues and finds a new one to execute
void evict(const klib::shared_ptr<TaskDescriptor>&);