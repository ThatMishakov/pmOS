#include "registers.hh"

#include <assert.h>
#include <processes/syscalls.hh>

using namespace kernel::proc;

namespace kernel::proc::syscalls
{

u64 syscall_arg64(TaskDescriptor *task, int arg)
{
    switch (arg) {
    case 0:
        return (u64(task->regs.a[0]) << 32) | task->regs.d[1];
    case 1:
        return (u64(task->regs.d[2]) << 32) | task->regs.a[1];
    case 2:
        return (u64)task->regs.d[4] << 32 | task->regs.d[3];
    default:
        assert(!"Too many arguments");
    }
    return 0;
}

ReturnStr<bool> syscall_arg64_checked(TaskDescriptor *task, int arg, u64 &value)
{
    value = syscall_arg64(task, arg);
    return Success(true);
}

ulong syscall_arg(TaskDescriptor *task, int arg, int args64before)
{
    switch (arg + args64before) {
    case 0:
        return task->regs.d[0];
    case 1:
        return task->regs.d[1];
    case 2:
        return task->regs.a[0];
    case 3:
        return task->regs.a[1];
    case 4:
        return task->regs.d[2];
    case 5:
        return task->regs.d[3];
    case 6:
        return task->regs.d[4];
    case 7:
        return task->regs.d[5];
    default:
        assert(!"Too many arguments");
    }
    return 0;
}

ReturnStr<bool> syscall_args_checked(TaskDescriptor *task, int arg, int args64before, int count,
                                     ulong *values)
{
    int realargs = arg + args64before;

    for (int i = realargs; i < (realargs + count); i++)
        values[i - realargs] = syscall_arg(task, i);

    return Success(true);
}

unsigned call_flags(TaskDescriptor *task) { return task->regs.d[0]; }

void syscall_ret_low(TaskDescriptor *task, i64 value) {
    task->regs.d[0] = value & 0xFFFFFFFF;
    task->regs.d[1] = (value >> 32) & 0xFFFFFFFF;
}
void syscall_ret_high(TaskDescriptor *task, u64 value) {
    task->regs.a[0] = value & 0xFFFFFFFF;
    task->regs.a[1] = (value >> 32) & 0xFFFFFFFF;
}
i64 syscall_ret_low(TaskDescriptor *task) { return (i64(task->regs.d[1]) << 32) | task->regs.d[0]; }

} // namespace kernel::syscalls