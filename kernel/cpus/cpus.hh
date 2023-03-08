#pragma once
#include <types.hh>
#include <lib/vector.hh>
#include <sched/sched.hh>
#include <memory/palloc.hh>

struct CPU_Desc {
    CPU_Info* local_info;
    u32 lapic_id;
};

extern klib::vector<CPU_Desc> cpus;

// Initializes per_cpu structures and idle process
void init_per_cpu();

// A routine for starting a CPU
extern "C" void cpu_start_routine();
extern "C" void cpu_startup_entry();

u64 cpu_configure(u64, u64);