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
    PROCESS_UNINIT
};

struct TaskDescriptor {
    Interrupt_Register_Frame regs;
    TaskPermissions perm;
    uint64_t page_table;
    TaskDescriptor* q_next;
    TaskDescriptor* q_prev;
    PID pid;
    Process_Status status;
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
ReturnStr<uint64_t> create_process();

// Creates a stack for the process
kresult_t init_stack(TaskDescriptor* process);