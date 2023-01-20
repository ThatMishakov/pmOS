#pragma once
#include <types.hh>

using priority_t = u16;
using quantum_t = u64;


constexpr priority_t sched_queues_levels = 16;
constexpr priority_t background_priority = sched_queues_levels - 1;
constexpr priority_t idle_priority = sched_queues_levels;