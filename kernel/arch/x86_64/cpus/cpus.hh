#pragma once

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