#include <processes/syscalls.hh>
#include <processes/tasks.hh>

using namespace kernel::proc;

bool TaskDescriptor::is_32bit() const { return false; }

ulong syscalls::syscall_arg(TaskDescriptor *d, int number, int)
{
    switch (number) {
    case 0:
        return d->regs.syscall_arg1();
    case 1:
        return d->regs.syscall_arg2();
    case 2:
        return d->regs.syscall_arg3();
    case 3:
        return d->regs.syscall_arg4();
    case 4:
        return d->regs.syscall_arg5();
    case 5:
        return d->regs.syscall_arg6();
    default:
        assert(!"Invalid syscall argument number");
        return 0;
    }
}

u64 syscalls::syscall_arg64(TaskDescriptor *d, int i)
{
    switch (i) {
    case 0:
        return d->regs.syscall_arg1();
    case 1:
        return d->regs.syscall_arg2();
    case 2:
        return d->regs.syscall_arg3();
    case 3:
        return d->regs.syscall_arg4();
    case 4:
        return d->regs.syscall_arg5();
    case 5:
        return d->regs.syscall_arg6();
    default:
        assert(!"Invalid syscall argument number");
        return 0;
    }
}

ReturnStr<bool> syscalls::syscall_arg64_checked(TaskDescriptor *d, int i, unsigned long &result)
{
    switch (i) {
    case 0:
        result = d->regs.syscall_arg1();
        return Success(true);
    case 1:
        result = d->regs.syscall_arg2();
        return Success(true);
    case 2:
        result = d->regs.syscall_arg3();
        return Success(true);
    case 3:
        result = d->regs.syscall_arg4();
        return Success(true);
    case 4:
        result = d->regs.syscall_arg5();
        return Success(true);
    case 5:
        result = d->regs.syscall_arg6();
        return Success(true);
    default:
        assert(!"Invalid syscall argument number");
        return Error(-EINVAL);
    }
}

ReturnStr<bool> syscalls::syscall_args_checked(TaskDescriptor *d, int i, int l, int count, unsigned long *out)
{
    for (int ii = 0; ii < count; ++ii) {
        out[ii] = syscall_arg(d, i + ii, l);
    }
    return Success(true);
}

void syscalls::syscall_ret_low(TaskDescriptor *task, i64 value)
{
    task->regs.a0 = static_cast<u64>(value);
}
i64 syscalls::syscall_ret_low(TaskDescriptor *task)
{
    return static_cast<i64>(task->regs.a0);
}

void syscalls::syscall_ret_high(TaskDescriptor *task, u64 value)
{
    task->regs.a1 = value;
}

unsigned syscalls::call_flags(TaskDescriptor *task)
{
    return task->regs.a0;
}