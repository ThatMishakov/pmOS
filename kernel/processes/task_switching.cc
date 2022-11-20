#include "task_switching.hh"
#include "sched.hh"
#include <asm.hh>
#include <memory/paging.hh>
#include <interrupts/apic.hh>

extern "C" void task_switch()
{
    // TODO: Invalidate SSE stuff here

    CPU_Info* cpu_struct = get_cpu_struct();
    TaskDescriptor* old_task = cpu_struct->current_task;
    cpu_struct->current_task = cpu_struct->next_task;
    cpu_struct->next_task = nullptr;

    u64 cr3 = getCR3();
    if (cpu_struct->current_task->page_table != cr3) {
        setCR3(cpu_struct->current_task->page_table);
    }

    if (old_task != nullptr) {
        switch (old_task->next_status)
        {
        case Process_Status::PROCESS_READY: {
            push_ready(old_task);
            break;
        }    
        case Process_Status::PROCESS_BLOCKED: {
            blocked_s.lock();
            blocked.push_back(old_task);
            blocked_s.unlock();
            break;
        }
        case Process_Status::PROCESS_SPECIAL: {
            // Do nothing
            break;
        };
        default:
            t_print_bochs("!!! Task switch error unknown next [status %h PID %i] to [PID %i] CPU %i\n", old_task->next_status, old_task->pid, cpu_struct->current_task->pid, get_lapic_id() >> 24);
            halt();
            break;
        }
        old_task->status = old_task->next_status;
    }

    if (cpu_struct->release_old_cr3) {
        release_cr3(cr3);
        cpu_struct->release_old_cr3 = 0;
    }
}