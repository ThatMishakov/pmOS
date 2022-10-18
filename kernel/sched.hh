#pragma once
#include "interrupts.hh"
#include "lib/vector.hh"
#include "messaging.hh"
#include "lib/splay_tree_map.hh"

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
    Interrupt_Register_Frame regs;
    PID pid;
    TaskPermissions perm;
    uint64_t page_table;
    Process_Status status;

    // Scheduling lists
    TaskDescriptor* q_next = nullptr;
    TaskDescriptor* q_prev = nullptr;
    sched_pqueue* parrent = nullptr;

    // Massaging
    Message_storage messages;
};

struct sched_pqueue {
    TaskDescriptor* first = nullptr;
    TaskDescriptor* last = nullptr;

    void push_back(TaskDescriptor*);
    void push_front(TaskDescriptor*);
    void erase(TaskDescriptor*);
    TaskDescriptor* pop_front();
    TaskDescriptor* get_first();
};

extern TaskDescriptor* current_task;

// Initializes scheduling structures during the kernel initialization
void init_scheduling();

// Assigns unused PID
PID assign_pid();

using sched_map = Splay_Tree_Map<PID, TaskDescriptor*>;

// Creates a process structure and returns its pid
ReturnStr<uint64_t> create_process(uint16_t ring = 3);

// Creates a stack for the process
kresult_t init_stack(TaskDescriptor* process);

// Inits an idle process
void init_idle();

// Finds a ready process and switches to it
void task_switch();

// Blocks a process and finds another one to execute
kresult_t block_process(TaskDescriptor*);

// Finds and switches to a new process (e.g. if the current is blocked or executing for too long)
void find_new_process();