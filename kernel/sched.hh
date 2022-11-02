#pragma once
#include "interrupts.hh"
#include "lib/vector.hh"
#include "messaging.hh"
#include "lib/splay_tree_map.hh"
#include "types.hh"

using PID = uint64_t;

struct TaskPermissions {
};

enum Process_Status {
    PROCESS_RUNNING,
    PROCESS_READY,
    PROCESS_BLOCKED,
    PROCESS_UNINIT,
    PROCESS_DEAD
};

struct sched_pqueue;

struct TaskDescriptor {
    // Basic process stuff
    Task_Regs regs;
    PID pid;
    uint64_t page_table;
    TaskPermissions perm;
    Process_Status status;

    // Scheduling lists
    TaskDescriptor* q_next = nullptr;
    TaskDescriptor* q_prev = nullptr;
    sched_pqueue* parrent = nullptr;

    // Messaging
    Message_storage messages;

    Spinlock lock;

    // Return state
    uint64_t ret_hi;
    uint64_t ret_lo;

    // Arguments
    klib::vector<uint8_t> args;

    // Inits stack
    kresult_t init_stack();

    // Blocks the process
    kresult_t block();

    // Switches to this process
    void switch_to();

    // Initializes uninited task
    void init_task();

    // Sets the entry point to the task
    inline void set_entry_point(uint64_t entry)
    {
        this->regs.e.rip = entry;
    }
};

struct CPU_Info {
    CPU_Info* self = this;
    Stack* kernel_stack = nullptr;
    TaskDescriptor* current_task = nullptr;
    TaskDescriptor* next_task = nullptr;
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
extern sched_map* s_map;

// Creates a process structure and returns its pid
ReturnStr<uint64_t> create_process(uint16_t ring = 3);

// Inits an idle process
void init_idle();

// Finds a ready process and switches to it
void task_switch();

// Finds and switches to a new process (e.g. if the current is blocked or executing for too long)
void find_new_process();

// Returns true if the process with pid exists, false otherwise
inline bool exists_process(uint64_t pid)
{
    return s_map->count(pid) == 1;
}

// Returns true if the process with pid is uninitialized
bool is_uninited(uint64_t pid);

// Gets a task descriptor of the process with pid
inline TaskDescriptor* get_task(uint64_t pid)
{
    return s_map->at(pid);
}

// Kills the task
void kill(TaskDescriptor* t);