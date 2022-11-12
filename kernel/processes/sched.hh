#pragma once
#include <interrupts/interrupts.hh>
#include <lib/vector.hh>
#include <messaging/messaging.hh>
#include <lib/splay_tree_map.hh>
#include <types.hh>
#include <lib/stack.hh>

using PID = u64;

struct TaskPermissions {
};

enum Process_Status {
    PROCESS_RUNNING,
    PROCESS_READY,
    PROCESS_BLOCKED,
    PROCESS_UNINIT,
    PROCESS_DEAD
};

struct Task_Attributes {
    bool debug_syscalls = false;
};

struct sched_pqueue;

struct TaskDescriptor {
    // Basic process stuff
    Task_Regs regs;
    PID pid;
    u64 page_table;
    TaskPermissions perm;
    Process_Status status;

    // Scheduling lists
    TaskDescriptor* q_next = nullptr;
    TaskDescriptor* q_prev = nullptr;
    sched_pqueue* parrent = nullptr;

    // Messaging
    Message_storage messages;
    Ports_storage ports;

    Spinlock lock;

    u64 unblock_mask = ~0;

    // Return state
    u64 ret_hi;
    u64 ret_lo;

    // Arguments
    klib::vector<u8> args;

    // Inits stack
    ReturnStr<u64> init_stack();

    // Blocks the process
    ReturnStr<u64> block(u64 mask);

    // Checks the mask and unblocks the process if needed
    void unblock_if_needed(u64 reason);

    // Returns 0 if there are no unblocking events pending. Otherwise returns 0.
    u64 check_unblock_immediately();

    // Switches to this process
    void switch_to();

    // Initializes uninited task
    void init_task();

    // Sets the entry point to the task
    inline void set_entry_point(u64 entry)
    {
        this->regs.e.rip = entry;
    }

    // Attributes of the task
    Task_Attributes attr;
};

struct CPU_Info {
    CPU_Info* self = this;
    Stack* kernel_stack = nullptr;
    TaskDescriptor* current_task = nullptr;
    TaskDescriptor* next_task = nullptr;
    u64 release_old_cr3 = 0;
};

// static CPU_Info* const GSRELATIVE per_cpu = 0; // clang ignores GSRELATIVE for no apparent reason
extern "C" CPU_Info* get_cpu_struct();

struct sched_pqueue {
    TaskDescriptor* first = nullptr;
    TaskDescriptor* last = nullptr;

    void push_back(TaskDescriptor*);
    void push_front(TaskDescriptor*);
    void erase(TaskDescriptor*);

    bool empty() const;

    TaskDescriptor* pop_front();
    TaskDescriptor* get_first();
};

// Initializes scheduling structures during the kernel initialization
void init_scheduling();

// Assigns unused PID
PID assign_pid();

using sched_map = klib::splay_tree_map<PID, TaskDescriptor*>;
extern sched_map s_map;

// Creates a process structure and returns its pid
ReturnStr<u64> create_process(u16 ring = 3);

// Inits an idle process
void init_idle();

// Finds a ready process and switches to it
void task_switch();

// Finds and switches to a new process (e.g. if the current is blocked or executing for too long)
void find_new_process();

// Returns true if the process with pid exists, false otherwise
inline bool exists_process(u64 pid)
{
    return s_map.count(pid) == 1;
}

// Returns true if the process with pid is uninitialized
bool is_uninited(u64 pid);

// Gets a task descriptor of the process with pid
inline TaskDescriptor* get_task(u64 pid)
{
    return s_map.at(pid);
}

// Kills the task
void kill(TaskDescriptor* t);