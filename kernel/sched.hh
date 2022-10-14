#pragma once
#include "interrupts.hh"
#include "lib/vector.hh"
#include "messaging.hh"

struct TaskPermissions {
};

struct TaskDescriptor {
    Interrupt_Register_Frame regs;
    TaskPermissions perm;
    uint64_t page_table;
    TaskDescriptor* q_next;
    TaskDescriptor* q_prev;
    uint64_t pid;
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

// Creates a process structure and returns its pid
uint64_t create_process();