#pragma once
#include "interrupts.hh"
#include "lib/vector.hh"
#include "messaging.hh"
#include "lib/splay_tree_map.hh"

using PID = uint64_t;

struct TaskPermissions {
};

struct TaskDescriptor {
    Interrupt_Register_Frame regs;
    TaskPermissions perm;
    uint64_t page_table;
    TaskDescriptor* q_next;
    TaskDescriptor* q_prev;
    PID pid;
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

void init_scheduling();

using sched_map = Splay_Tree_Map<PID, TaskDescriptor*>;

// Creates a process structure and returns its pid
uint64_t create_process();