#include <assert.h>
#include <processes/syscalls.hh>
#include <processes/tasks.hh>

using namespace kernel::proc;
using namespace kernel::proc::syscalls;

unsigned syscalls::call_flags(TaskDescriptor *task)
{
    return task->regs.rax;
}

ulong syscalls::syscall_arg(TaskDescriptor *task, int arg, int args64before)
{
    if (task->is_32bit()) {
        switch (arg + args64before) {
        case 0:
            return task->regs.rbx;
        case 1:
            return task->regs.rsi;
        case 2:
            return task->regs.rdi;
        case 3:
            return task->regs.rbp;
        default:
            assert(!"Too many arguments");
        }
    } else {
        switch (arg) {
        case 0:
            return task->regs.rdi;
        case 1:
            return task->regs.rsi;
        case 2:
            return task->regs.r10;
        case 3:
            return task->regs.r8;
        case 4:
            return task->regs.r9;
        default:
            assert(!"Too many arguments");
        }
    }
    return 0;
}

void syscalls::syscall_ret_low(TaskDescriptor *task, i64 value)
{
    if (task->is_32bit()) {
        task->regs.rax = value & 0xffffffff;
        task->regs.rbx = value >> 32;
    } else {
        task->regs.rax = value;
    }
}
i64 syscalls::syscall_ret_low(TaskDescriptor *task)
{
    if (task->is_32bit()) {
        return (i64)task->regs.rax | ((i64)task->regs.rbx << 32);
    } else {
        return task->regs.rax;
    }
}

void syscalls::syscall_ret_high(TaskDescriptor *task, u64 value)
{
    if (task->is_32bit()) {
        task->regs.rsi = value & 0xffffffff;
        task->regs.rdi = value >> 32;
    } else {
        task->regs.rdx = value;
    }
}

u64 syscalls::syscall_arg64(TaskDescriptor *task, int arg)
{
    if (task->is_32bit()) {
        switch (arg) {
        case 0:
            return (task->regs.rbx & 0xffffffff) | (task->regs.rsi << 32);
        case 1:
            return (task->regs.rdi & 0xffffffff) | (task->regs.rbp << 32);
        default:
            assert(!"Too many arguments");
        }
    } else {
        return syscall_arg(task, arg, 0);
    }
    return 0;
}

ReturnStr<bool> syscalls::syscall_arg64_checked(TaskDescriptor *task, int arg, u64 &value)
{
    if (task->is_32bit()) {
        switch (arg) {
        case 0:
            value = (task->regs.rbx & 0xffffffff) | (task->regs.rsi << 32);
            break;
        case 1:
            value = (task->regs.rdi & 0xffffffff) | (task->regs.rbp << 32);
            break;
        default:
            return copy_from_user((char *)&value, (char *)task->regs.rsp + (arg - 2) * 8, 8);
        }
    } else {
        value = syscall_arg64(task, arg);
    }
    return Success(true);
}

ReturnStr<bool> syscalls::syscall_args_checked(TaskDescriptor *task, int arg, int args64before,
                                               int count, ulong *values)
{
    if (task->is_32bit()) {
        int realargs = arg + args64before;

        for (int i = realargs; i < 4 && i < (realargs + count); i++) {
            values[i - realargs] = syscall_arg(task, i);
        }

        if (realargs + count >= 4) {
            int start = realargs > 4 ? realargs : 4;
            int end   = realargs + count;

            unsigned args[16];
            assert(end - start <= 16);

            auto b = copy_from_user((char *)args, (char *)task->regs.rsp + (start - 4) * 4,
                                    (end - start) * 4);
            if (!b.success() || !b.val) {
                return b;
            }

            int offset = realargs < 4 ? 4 - realargs : 0;
            for (int i = 0; i < end - start; i++) {
                values[i + offset] = args[i];
            }
        }
    } else {
        for (int i = 0; i < count; i++) {
            values[i] = syscall_arg(task, arg + i, args64before);
        }
    }
    return Success(true);
}

bool TaskDescriptor::is_32bit() const { return regs.cs == R3_LEGACY_CODE_SEGMENT; }

kresult_t TaskDescriptor::set_32bit()
{
    assert(!page_table);
    assert(status == TaskStatus::TASK_UNINIT);

    regs.cs = R3_LEGACY_CODE_SEGMENT;

    return 0;
}