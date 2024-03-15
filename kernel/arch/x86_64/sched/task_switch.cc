#include <processes/tasks.hh>
#include <cpus/sse.hh>
#include <x86_asm.hh>

void save_segments(TaskDescriptor *task)
{
    task->regs.seg.gs = read_msr(0xC0000102); // KernelGSBase
    task->regs.seg.fs = read_msr(0xC0000100); // FSBase
}

void restore_segments(TaskDescriptor *task)
{
    write_msr(0xC0000102, task->regs.seg.gs); // KernelGSBase
    write_msr(0xC0000100, task->regs.seg.fs); // FSBase
}

void TaskDescriptor::before_task_switch() {
    save_segments(this);

    if (sse_is_valid()) {
        //t_print_bochs("Saving SSE registers for PID %h\n", c->current_task->pid);
        sse_data.save_sse();
        invalidate_sse();
    }
}

void TaskDescriptor::after_task_switch() {
    save_segments(this);
}