#pragma once
#include <types.hh>

// Initializes per_cpu structures and idle process
void init_per_cpu();

// A routine for starting a CPU
extern "C" void cpu_start_routine();
extern "C" void cpu_startup_entry();

ReturnStr<u64> cpu_configure(u64, u64);