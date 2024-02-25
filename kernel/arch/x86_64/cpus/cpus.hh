#pragma once
#include <types.hh>
#include <lib/vector.hh>
#include <sched/sched.hh>
#include <memory/palloc.hh>

/**
 * This structure holds the data to identify the CPUs. CPU_Info is the structure which is local to each CPU
 * and can be also accessed by get_cpu_info() and lapic_id is an ID of LAPIC used for Inter-Processor Communication
 * and interrupts.
 */
struct CPU_Desc {
    CPU_Info* local_info;
    u32 lapic_id;
};

/**
 * This vector holds the data of all of the CPUs
 */
extern klib::vector<CPU_Desc> cpus;

/**
 * This function is called upon the kernel initialization, once for each CPU.
 * Its function is to allocate local CPU_Infos, initialize idle processors, stacks
 * and other per-CPU structures
 */
void init_per_cpu();

// A routine for starting a CPU
extern "C" void cpu_start_routine();
extern "C" void cpu_startup_entry();

u64 cpu_configure(u64, u64);