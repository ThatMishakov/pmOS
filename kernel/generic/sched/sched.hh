/* Copyright (c) 2024, Mikhail Kovalev
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 * 
 * 1. Redistributions of source code must retain the above copyright notice, this
 *    list of conditions and the following disclaimer.
 * 
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 * 
 * 3. Neither the name of the copyright holder nor the names of its
 *    contributors may be used to endorse or promote products derived from
 *    this software without specific prior written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once
#include <lib/vector.hh>
#include <messaging/messaging.hh>
#include <lib/splay_tree_map.hh>
#include <types.hh>
#include <lib/stack.hh>
#include <lib/array.hh>
#include <lib/memory.hh>
#include <processes/tasks.hh>
#include "defs.hh"
#include <memory/temp_mapper.hh>
#include "sched_queue.hh"

#ifdef __x86_64__
#include <paging/x86_temp_mapper.hh>
#include <interrupts/gdt.hh>
#elif  defined(__riscv)
#include <paging/riscv64_temp_mapper.hh>
#include <cpus/floating_point.hh>
#endif

// Checks the mask and unblocks the task if needed
// This function needs to be axed
inline bool unblock_if_needed(const klib::shared_ptr<TaskDescriptor>& p, const klib::shared_ptr<Generic_Port>& compare_blocked_by)
{
    return p->atomic_unblock_if_needed(compare_blocked_by);
}

// Blocks current task, setting blocked_by to *ptr*.
ReturnStr<u64> block_current_task(const klib::shared_ptr<Generic_Port>& ptr);

extern sched_queue blocked;
extern sched_queue uninit;

inline klib::array<sched_queue, sched_queues_levels> global_sched_queues;

struct CPU_Info {
    CPU_Info* self = this; // 0
    u64* kernel_stack_top = nullptr; // 8
    u64 temp_var = 0; // 16
    u64 nested_level = 1; // 24
    klib::shared_ptr<TaskDescriptor> current_task = klib::shared_ptr<TaskDescriptor>(); // 32
    u64 jumpto_from = 0;  // 48
    u64 jumpto_to   = 0;  // 56
    Task_Regs nested_int_regs; // 64

    klib::array<sched_queue, sched_queues_levels> sched_queues;
    klib::shared_ptr<TaskDescriptor> idle_task = klib::shared_ptr<TaskDescriptor>();

    u32 timer_val = 0;

    u64 pagefault_cr2 = 0;
    u64 pagefault_error = 0;

    Kernel_Stack_Pointer kernel_stack;
    Kernel_Stack_Pointer idle_stack;

    #ifdef __x86_64__
    // x86-specific
    Kernel_Stack_Pointer debug_stack;
    Kernel_Stack_Pointer nmi_stack;
    Kernel_Stack_Pointer machine_check_stack;
    Kernel_Stack_Pointer double_fault_stack;
    GDT cpu_gdt;
    #endif

    klib::shared_ptr<TaskDescriptor> atomic_pick_highest_priority(priority_t min = sched_queues_levels - 1);
    klib::shared_ptr<TaskDescriptor> atomic_get_front_priority(priority_t);

    // Temporary memory mapper; This is arch specific
    #ifdef __x86_64__
    x86_PAE_Temp_Mapper temp_mapper;
    #elif defined(__riscv)
    RISCV64_Temp_Mapper temp_mapper;
    #endif

    constexpr static unsigned pthread_once_size = 16;
    klib::array<const void *, pthread_once_size> pthread_once_storage = {};

    u32 cpu_id = 0;

    #ifdef __x86_64__
    u32 lapic_id = 0;
    #endif


    #ifdef __riscv
    u64 hart_id = 0;
    klib::string isa_string;

    u64 last_fp_task = 0;
    FloatingPointState last_fp_state = FloatingPointState::Disabled;
    #endif

    // IMHO this is better than protecting current_task pointer with spinlock
    priority_t current_task_priority = sched_queues_levels;

    void ipi_reschedule(); // nothrow ?
};

/**
 * This vector holds the data of all of the CPUs
 */
extern klib::vector<CPU_Info *> cpus;

quantum_t assign_quantum_on_priority(priority_t);

u32 calculate_timer_ticks(const klib::shared_ptr<TaskDescriptor>& task);

// static CPU_Info* const GSRELATIVE per_cpu = 0; // clang ignores GSRELATIVE for no apparent reason
extern "C" CPU_Info* get_cpu_struct();

inline klib::shared_ptr<TaskDescriptor> get_current_task()
{
    return get_cpu_struct()->current_task;
}

// Adds the task to the appropriate ready queue
void push_ready(const klib::shared_ptr<TaskDescriptor>& p);

// Initializes scheduling structures during the kernel initialization
void init_scheduling();

// Finds a ready process and switches to it
void task_switch();

// Finds and switches to a new process (e.g. if the current is blocked or executing for too long)
void find_new_process();

// To be called from the clock routine
void sched_periodic();

// Starts the scheduler
void start_scheduler();

// Pushes current processos to the back of sheduling queues
void evict(const klib::shared_ptr<TaskDescriptor>&);

// Reschedules the tasks
extern "C" void reschedule();