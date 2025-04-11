#include "rcu.hh"

using namespace kernel::memory;

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
        if (((i + 1) * sizeof(u64) * 8) <= sched::number_of_cpus) {
            bitmask[i] = -1UL;
        } else {
            int number_of_bits = sched::number_of_cpus - i * sizeof(u64) * 8;
            bitmask[i]         = (1UL << number_of_bits) - 1;
        }
    }

    generation++;
    highest_generation = generation + 1;
}

void RCU_CPU::quiet(RCU &parent, size_t my_cpu_id)
{
    if (parent.cpu_bit_set(my_cpu_id)) {
        Auto_Lock_Scope l(parent.lock);

        parent.silence_cpu(my_cpu_id);
        if (parent.generation_complete()) {
            parent.generation++;

            if (parent.generation <= parent.highest_generation) {
                parent.start_generation();
            }
        }
    }

    if (current_callbacks and (parent.generation > generation)) {
        while (current_callbacks) {
            // TODO: This can be delayed, e.g. to be executed while the CPU is idling
            RCU_Head *next = current_callbacks->rcu_next;
            current_callbacks->rcu_func(current_callbacks, next and (next->rcu_func == current_callbacks->rcu_func));
            current_callbacks = next;
        }
    }

    if ((!current_callbacks) and next_callbacks) {
        current_callbacks = next_callbacks;
        next_callbacks    = nullptr;

        Auto_Lock_Scope l(parent.lock);
        generation = parent.generation + 1;

        if (!parent.generation_complete()) {
            assert(parent.highest_generation <= (generation + 1));
            parent.highest_generation = generation + 1;
        } else {
            parent.start_generation();
        }
    }
}