#include "sched.hh"
#include "asm.hh"

TaskDescriptor* current_task;

sched_pqueue ready;
sched_pqueue blocked;

void init_scheduling()
{
    current_task = new TaskDescriptor;
    current_task->page_table = getCR3();
}
