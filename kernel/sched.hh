#pragma once
#include "interrupts.hh"
#include "lists.hh"

struct TaskDescriptor {
    TaskPermissions perm;
    uint64_t page_table;
    Interrupt_Stack_Frame stack;
};