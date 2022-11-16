#pragma once

// Initializes per_cpu structures and idle process
void init_per_cpu();

// A routine for starting a CPU
extern "C" void cpu_start_routine();