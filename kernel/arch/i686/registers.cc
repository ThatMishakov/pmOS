#include "registers.hh"

#include <assert.h>
#include <processes/syscalls.hh>

static void syscall_ret_low(TaskDescriptor *task, i64 value)
{
    task->regs.eax = (u64)value & 0xffffffff;
    task->regs.edx = (u64)value >> 32;
}

static void syscall_ret_high(TaskDescriptor *task, u64 value)
{
    task->regs.esi = (u64)value & 0xffffffff;
    task->regs.edi = (u64)value >> 32;
}

static unsigned call_flags(TaskDescriptor *task) { return task->regs.eax; }

unsigned syscall_number(TaskDescriptor *task) { return call_flags(task) & 0xff; }
ulong syscall_flags(TaskDescriptor *task) { return call_flags(task) >> 8; }

u64 SyscallRetval::operator=(u64 value)
{
    syscall_ret_low(task, 0); // SUCCESS
    syscall_ret_high(task, value);
    return value;
}

i64 SyscallError::operator=(i64 value)
{
    assert(value <= 0);
    syscall_ret_low(task, value);
    return value;
}

SyscallError::operator int() const { return -(int)task->regs.eax; }

void syscall_success(TaskDescriptor *task) { syscall_ret_low(task, 0); }

u64 syscall_arg64(TaskDescriptor *task, int arg)
{
    switch (arg) {
    case 0:
        return (u64(task->regs.ecx) << 32) | task->regs.ebx;
    case 1:
        return (u64(task->regs.ebp) << 32) | task->regs.edx;
    default:
        assert(!"Too many arguments");
    }
    return 0;
}

ReturnStr<bool> syscall_arg64_checked(TaskDescriptor *task, int arg, u64 &value)
{
    switch (arg) {
    case 0:
        value = (u64(task->regs.ecx) << 32) | task->regs.ebx;
        break;
    case 1:
        value = (u64(task->regs.ebp) << 32) | task->regs.edx;
        break;
    default:
        return copy_from_user((char *)&value, (char *)task->regs.esp + (arg - 2) * 8, 8);
    }
    return Success(true);
}

ulong syscall_arg(TaskDescriptor *task, int arg, int args64before)
{
    switch (arg + args64before) {
    case 0:
        return task->regs.ebx;
    case 1:
        return task->regs.ecx;
    case 2:
        return task->regs.edx;
    case 3:
        return task->regs.ebp;
    default:
        assert(!"Too many arguments");
    }
    return 0;
}

ReturnStr<bool> syscall_args_checked(TaskDescriptor *task, int arg, int args64before, int count,
                                     ulong *values)
{
    int realargs = arg + args64before;

    for (int i = realargs; i < 4 && i < (realargs + count); i++) {
        values[i - realargs] = syscall_arg(task, i);
    }

    if (realargs + count >= 4) {
        int start = realargs > 4 ? realargs : 4;
        int end   = realargs + count;

        unsigned args[16];
        assert(end - start <= 16);

        auto b = copy_from_user((char *)args, (char *)task->regs.esp + (start - 4) * 4,
                                (end - start) * 4);
        if (!b.success() || !b.val) {
            return b;
        }

        int offset = realargs < 4 ? 4 - realargs : 0;
        for (int i = 0; i < end - start; i++) {
            values[i + offset] = args[i];
        }
    }

    return Success(true);
}