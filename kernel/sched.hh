#pragma once
#include "interrupts.hh"

struct TaskPermissions {
};

struct TaskDescriptor {
    Interrupt_Register_Frame regs;
    TaskPermissions perm;
    uint64_t page_table;
    TaskDescriptor* q_next;
};

struct sched_pqueue {
    TaskDescriptor* first = nullptr;
    TaskDescriptor* last = nullptr;
};

extern TaskDescriptor* current_task;

void init_scheduling();