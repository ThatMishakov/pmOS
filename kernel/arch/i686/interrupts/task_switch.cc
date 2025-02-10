#include "gdt.hh"
#include <processes/tasks.hh>
#include <sched/sched.hh>

void save_segments(TaskDescriptor *task)
{
    auto cpu = get_cpu_struct();
    task->regs.gs = segment_to_base(cpu->cpu_gdt.user_gs);
    task->regs.fs = segment_to_base(cpu->cpu_gdt.user_fs);
}

void restore_segments(TaskDescriptor *task)
{
    auto cpu = get_cpu_struct();
    cpu->cpu_gdt.user_gs = base_to_user_data_segment(task->regs.gs);
    cpu->cpu_gdt.user_fs = base_to_user_data_segment(task->regs.fs);

    asm ("movw %w0, %%fs" : : "rm"(0x33));
}