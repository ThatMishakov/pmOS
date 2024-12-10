#include <assert.h>
#include <processes/syscalls.hh>
#include <processes/tasks.hh>

static ulong call_flags(TaskDescriptor *task) { return task->regs.scratch_r.rdi; }

unsigned syscall_number(TaskDescriptor *task) { return call_flags(task) & 0xff; }

ulong syscall_flags(TaskDescriptor *task) { return call_flags(task) >> 8; }

ulong syscall_arg(TaskDescriptor *task, int arg, int args64before)
{
    if (task->is_32bit()) {
        // TODO
        return 0;
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
        return 0;
    }
}

static void syscall_ret_low(TaskDescriptor *task, i64 value) { task->regs.scratch_r.rax = value; }
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
        // TODO
        return 0;
    } else {
        return syscall_arg(task, arg, 0);
    }
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