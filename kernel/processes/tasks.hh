#pragma once
#include <types.hh>
#include <lib/memory.hh>
#include <interrupts/interrupts.hh>
#include <messaging/messaging.hh>
#include <memory/paging.hh>
#include <sched/defs.hh>
#include <cpus/sse.hh>

using PID = u64;

struct TaskPermissions {
};

enum Process_Status: u64 {
    PROCESS_RUNNING = 0,
    PROCESS_RUNNING_IN_SYSTEM = 1,
    PROCESS_READY,
    PROCESS_BLOCKED,
    PROCESS_UNINIT,
    PROCESS_SPECIAL, // Would be used by idle or system tasks
    PROCESS_DEAD
};

struct Task_Attributes {
    bool debug_syscalls = false;
};

class sched_queue;

struct TaskDescriptor {
    // Basic process stuff
    Task_Regs regs;
    PID pid;
    Page_Table page_table; // 192
    u64 entry_mode = 0; // 200

    TaskPermissions perm;

    // Attributes of the task
    Task_Attributes attr;

    // Messaging
    Message_storage messages;
    Ports_storage ports;
    Spinlock messaging_lock;
    u64 unblock_mask = ~0;

    // Scheduling info
    klib::shared_ptr<TaskDescriptor> queue_next = nullptr;
    klib::shared_ptr<TaskDescriptor> queue_prev = nullptr;
    sched_queue *parent_queue = nullptr;
    Process_Status status;
    priority_t priority = 8;
    Spinlock sched_lock;

    // Inits stack
    ReturnStr<u64> init_stack();

    // Returns 0 if there are no unblocking events pending. Otherwise returns 0.
    u64 check_unblock_immediately();

    // Sets the entry point to the task
    inline void set_entry_point(u64 entry)
    {
        this->regs.e.rip = entry;
    }

    enum Type {
        Normal,
        System,
        Idle,
    } type = Normal;

    u64 ret_hi = 0;
    u64 ret_lo = 0;

    SSE_Data sse_data;
};

using task_ptr = klib::shared_ptr<TaskDescriptor>;


// Creates a process structure and returns its pid
ReturnStr<klib::shared_ptr<TaskDescriptor>> create_process(u16 ring = 3);

// Inits an idle process
void init_idle();

// A map of all the tasks
using sched_map = klib::splay_tree_map<PID, klib::shared_ptr<TaskDescriptor>>;
extern sched_map tasks_map;
extern Spinlock tasks_map_lock;

// Assigns unused PID
PID assign_pid();

// Returns true if the process with pid exists, false otherwise
inline bool exists_process(u64 pid)
{
    tasks_map_lock.lock();
    bool exists = tasks_map.count(pid) == 1;
    tasks_map_lock.unlock();
    return exists;
}

// Returns true if the process with pid is uninitialized
bool is_uninited(const klib::shared_ptr<const TaskDescriptor>&);

// Gets a task descriptor of the process with pid
inline klib::shared_ptr<TaskDescriptor> get_task(u64 pid)
{
    Auto_Lock_Scope scope_lock(tasks_map_lock);
    return tasks_map.get_copy_or_default(pid);
}

// Inits the task
void init_task(const klib::shared_ptr<TaskDescriptor>& task);