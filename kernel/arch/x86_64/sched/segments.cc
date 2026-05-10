#include <processes/tasks.hh>
#include <x86_asm.hh>

void save_segments(kernel::proc::TaskDescriptor *task)
{
    task->regs.gs = read_msr(0xC0000102); // KernelGSBase
    task->regs.fs = read_msr(0xC0000100); // FSBase
}

void restore_segments(kernel::proc::TaskDescriptor *task)
{
    write_msr(0xC0000102, task->regs.gs); // KernelGSBase
    write_msr(0xC0000100, task->regs.fs); // FSBase
}