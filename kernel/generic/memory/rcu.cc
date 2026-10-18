#include "rcu.hh"
#include <utils.hh>

using namespace kernel::memory;

namespace kernel::sched
{
extern RCU paging_rcu;
extern RCU heap_rcu;
}

void kernel::memory::init_rcu()
{
    // Initialize RCU bitmask
    auto result = sched::paging_rcu.bitmask.resize((kernel::sched::get_cpu_count() + 63) / 64, 0);
    if (!result)
        panic("Failed to initialize RCU bitmask");
    result = sched::heap_rcu.bitmask.resize((kernel::sched::get_cpu_count() + 63) / 64, 0);
    if (!result)
        panic("Failed to initialize RCU bitmask");
}

bool RCU::cpu_bit_set(size_t cpu_id) noexcept
{
    return (bitmask[cpu_id / 64] & ((u64)1 << (cpu_id % 64))) != 0;
}

void RCU::silence_cpu(size_t cpu_id) noexcept { bitmask[cpu_id / 64] &= ~((u64)1 << (cpu_id % 64)); }

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
        if (((i + 1) * sizeof(u64) * 8) <= sched::get_cpu_count()) {
            bitmask[i] = -(u64)1;
        } else {
            int number_of_bits = sched::get_cpu_count() - i * sizeof(u64) * 8;
            bitmask[i]         = ((u64)1 << number_of_bits) - 1;
        }
    }

    generation++;
    highest_generation = generation + 1;
}

void RCU_CPU::quiet(RCU &parent, size_t my_cpu_id)
{
    u64 parent_generation = 0;
    {
        Auto_Lock_Scope l(parent.lock);
    
        if (parent.cpu_bit_set(my_cpu_id)) {
            parent.silence_cpu(my_cpu_id);
            if (parent.generation_complete()) {
                parent.generation++;

                if (parent.generation <= parent.highest_generation) {
                    parent.start_generation();
                }
            }
        }

        parent_generation = parent.generation;
    }

    if (current_callbacks and (parent_generation > this->generation)) {
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
        this->generation = parent.generation + 1;

        if (!parent.generation_complete()) {
            assert(parent.highest_generation <= (generation + 1));
            parent.highest_generation = generation + 1;
        } else {
            parent.start_generation();
        }
    }
}