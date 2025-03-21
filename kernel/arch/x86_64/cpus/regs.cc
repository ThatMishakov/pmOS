#include <assert.h>
#include <processes/syscalls.hh>
#include <processes/tasks.hh>

static ulong call_flags(TaskDescriptor *task)
{
    if (task->is_32bit())
        return task->regs.scratch_r.rax;
    else
        return task->regs.scratch_r.rdi;
}

ulong syscall_flags_reg(TaskDescriptor *task)
{
    return call_flags(task);
}

unsigned syscall_number(TaskDescriptor *task) { return call_flags(task) & 0xff; }

ulong syscall_flags(TaskDescriptor *task) { return call_flags(task) >> 8; }

ulong syscall_arg(TaskDescriptor *task, int arg, int args64before)
{
    if (task->is_32bit()) {
        switch (arg + args64before) {
        case 0:
            return task->regs.preserved_r.rbx;
        case 1:
            return task->regs.scratch_r.rcx;
        case 2:
            return task->regs.scratch_r.rdx;
        case 3:
            return task->regs.preserved_r.rbp;
        default:
            assert(!"Too many arguments");
        }
    } else {
        switch (arg) {
        case 0:
            return task->regs.scratch_r.rsi;
        case 1:
            return task->regs.scratch_r.rdx;
        case 2:
            return task->regs.scratch_r.rcx;
        case 3:
            return task->regs.scratch_r.r8;
        case 4:
            return task->regs.scratch_r.r9;
        default:
            assert(!"Too many arguments");
        }
    }
    return 0;
}

static void syscall_ret_low(TaskDescriptor *task, i64 value)
{
    if (task->is_32bit()) {
        task->regs.scratch_r.rax = value & 0xffffffff;
        task->regs.scratch_r.rdx = value >> 32;
    } else {
        task->regs.scratch_r.rax = value;
    }
}
static void syscall_ret_high(TaskDescriptor *task, u64 value)
{
    if (task->is_32bit()) {
        task->regs.scratch_r.rsi = value & 0xffffffff;
        task->regs.scratch_r.rdi = value >> 32;
    } else {
        task->regs.scratch_r.rdx = value;
    }
}

u64 syscall_arg64(TaskDescriptor *task, int arg)
{
    if (task->is_32bit()) {
        switch (arg) {
        case 0:
            return (task->regs.preserved_r.rbx & 0xffffffff) | (task->regs.scratch_r.rcx << 32);
        case 1:
            return (task->regs.scratch_r.rdx & 0xffffffff) | (task->regs.preserved_r.rbp << 32);
        default:
            assert(!"Too many arguments");
        }
    } else {
        return syscall_arg(task, arg, 0);
    }
    return 0;
}

ReturnStr<bool> syscall_arg64_checked(TaskDescriptor *task, int arg, u64 &value)
{
    if (task->is_32bit()) {
        switch (arg) {
        case 0:
            value = (task->regs.preserved_r.rbx & 0xffffffff) | (task->regs.scratch_r.rcx << 32);
            break;
        case 1:
            value = (task->regs.scratch_r.rdx & 0xffffffff) | (task->regs.preserved_r.rbp << 32);
            break;
        default:
            return copy_from_user((char *)&value, (char *)task->regs.e.rsp + (arg - 2) * 8, 8);
        }
    } else {
        value = syscall_arg64(task, arg);
    }
    return Success(true);
}

ReturnStr<bool> syscall_args_checked(TaskDescriptor *task, int arg, int args64before, int count,
                                     ulong *values)
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

            auto b = copy_from_user((char *)args, (char *)task->regs.e.rsp + (start - 4) * 4,
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

SyscallError::operator int() const { return task->regs.scratch_r.rax; }

void syscall_success(TaskDescriptor *task) { syscall_ret_low(task, 0); }

bool TaskDescriptor::is_32bit() const { return regs.e.cs == R3_LEGACY_CODE_SEGMENT; }

kresult_t TaskDescriptor::set_32bit()
{
    assert(!page_table);
    assert(status == TaskStatus::TASK_UNINIT);

    regs.e.cs = R3_LEGACY_CODE_SEGMENT;

    return 0;
}