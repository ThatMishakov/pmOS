#include "rcu.hh"

bool RCU::cpu_bit_set(size_t cpu_id) noexcept
{
    return (bitmask[cpu_id / 64] & (1UL << (cpu_id % 64))) != 0;
}

void RCU::silence_cpu(size_t cpu_id) noexcept { bitmask[cpu_id / 64] &= ~(1UL << (cpu_id % 64)); }

bool RCU::generation_complete() noexcept
{
    for (size_t i = 0; i < bitmask.size(); i++) {
        if (bitmask[i] != 0)
            return false;
    }
    return true;
}

void RCU::start_generation() noexcept
{
    for (size_t i = 0; i < bitmask.size(); i++) {
        if ((i + 1) * sizeof(u64) <= number_of_cpus) {
            bitmask[i] = -1UL;
        } else {
            int number_of_bits = number_of_cpus - i * sizeof(u64);
            bitmask[i]         = (1UL << number_of_bits) - 1;
        }
    }

    generation++;
    highest_generation = generation;
}