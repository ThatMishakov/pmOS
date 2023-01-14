#pragma once
#include <processes/tasks.hh>

// Pointer to where special kernel structures can be mapped
extern void* unoccupied;

void print_registers(TaskDescriptor*);