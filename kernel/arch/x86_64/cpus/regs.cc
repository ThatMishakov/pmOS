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
            return (task->regs.preserved_r.rbx & 0xffffffff) | task->regs.scratch_r.rcx << 32;
        case 1:
            return (task->regs.scratch_r.rdx & 0xffffffff) | task->regs.preserved_r.rbp << 32;
        default:
            assert(!"Too many arguments");
        }
    } else {
        return syscall_arg(task, arg, 0);
    }
    return 0;
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

SyscallError::operator int() const
{
    return task->regs.scratch_r.rax;
}

void syscall_success(TaskDescriptor *task) { syscall_ret_low(task, 0); }

bool TaskDescriptor::is_32bit() const { return regs.e.cs == R3_LEGACY_CODE_SEGMENT; }

kresult_t TaskDescriptor::set_32bit()
{
    assert(!page_table);
    assert(status == TaskStatus::TASK_UNINIT);

    regs.e.cs = R3_LEGACY_CODE_SEGMENT;

    return 0;
}