#include "task_switching.hh"
#include "sched.hh"
#include <asm.hh>
#include <memory/paging.hh>

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

    if (cpu_struct->release_old_cr3) {
        release_cr3(cr3);
        cpu_struct->release_old_cr3 = 0;
    }
}