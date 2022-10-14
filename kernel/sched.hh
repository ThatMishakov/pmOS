#pragma once
#include "interrupts.hh"

struct TaskPermissions {
};

struct TaskDescriptor {
    Interrupt_Register_Frame regs;
    TaskPermissions perm;
    uint64_t page_table;
    TaskDescriptor* q_next;
    TaskDescriptor* q_prev;
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