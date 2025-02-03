/* Copyright (c) 2024, Mikhail Kovalev
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 * this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 *
 * 3. Neither the name of the copyright holder nor the names of its
 *    contributors may be used to endorse or promote products derived from
 *    this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include "exceptions_managers.hh"

#include <cpus/sse.hh>
#include <kern_logger/kern_logger.hh>
#include <memory/paging.hh>
#include <processes/syscalls.hh>
#include <processes/tasks.hh>
#include <stdlib.h>
#include <utils.hh>
#include <x86_asm.hh>

void print_registers(const Task_Regs &regs, Logger &logger)
{
    // logger.printf(" => %%rdi: 0x%h\n", regs.scratch_r.rdi);
    // logger.printf(" => %%rsi: 0x%h\n", regs.scratch_r.rsi);
    // logger.printf(" => %%rdx: 0x%h\n", regs.scratch_r.rdx);
    // logger.printf(" => %%rcx: 0x%h\n", regs.scratch_r.rcx);
    // logger.printf(" => %%r8:  0x%h\n", regs.scratch_r.r8);
    // logger.printf(" => %%r9:  0x%h\n", regs.scratch_r.r9);
    // logger.printf(" => %%rax: 0x%h\n", regs.scratch_r.rax);
    // logger.printf(" => %%r10: 0x%h\n", regs.scratch_r.r10);
    // logger.printf(" => %%r11: 0x%h\n", regs.scratch_r.r11);

    // logger.printf(" => %%rbx: 0x%h\n", regs.preserved_r.rbx);
    // logger.printf(" => %%rbp: 0x%h\n", regs.preserved_r.rbp);
    // logger.printf(" => %%r12: 0x%h\n", regs.preserved_r.r12);
    // logger.printf(" => %%r13: 0x%h\n", regs.preserved_r.r13);
    // logger.printf(" => %%r14: 0x%h\n", regs.preserved_r.r14);
    // logger.printf(" => %%r15: 0x%h\n", regs.preserved_r.r15);

    // logger.printf(" => %%rip: 0x%h\n", regs.e.rip);
    // logger.printf(" => %%rsp: 0x%h\n", regs.e.rsp);
    // logger.printf(" => %%rflags: 0x%h\n", regs.e.rflags.numb);

    // logger.printf(" => %%gs offset: 0x%h\n", regs.seg.gs);
    // logger.printf(" => %%fs offset: 0x%h\n", regs.seg.fs);

    logger.printf(" Entry type: %i\n", regs.entry_type);

    // logger.printf(" Error code: 0x%h\n", regs.int_err);
}

void print_registers(TaskDescriptor *task, Logger &logger)
{
    if (not task)
        return;

    logger.printf("Registers for task %i (%s)\n", task->task_id, task->name.c_str());
    print_registers(task->regs, logger);
}

Task_Regs kernel_interrupt_regs;
extern void *_kernel_start;

void print_kernel_int_stack_trace(Logger &logger)
{
    logger.printf("Kernel stack trace:\n");
    logger.printf("Womp womp");
    // u64 *rbp = (u64 *)kernel_interrupt_regs.preserved_r.rbp;
    // while (rbp) {
    //     auto a    = rbp[1];
    //     auto filt = (u64)a - (u64)&_kernel_start + kernel_static;
    //     logger.printf(" => 0x%h (0x%h)\n", a, filt);
    //     rbp = (u64 *)rbp[0];
    // }
}

extern "C" void dbg_main(long code)
{
    t_print_bochs("Error! Kernel interrupt! %i\n", code);
    // t_print_bochs("Registers:\n");
    // t_print_bochs(" => %%rdi: 0x%h\n", kernel_interrupt_regs.scratch_r.rdi);
    // t_print_bochs(" => %%rsi: 0x%h\n", kernel_interrupt_regs.scratch_r.rsi);
    // t_print_bochs(" => %%rdx: 0x%h\n", kernel_interrupt_regs.scratch_r.rdx);
    // t_print_bochs(" => %%rcx: 0x%h\n", kernel_interrupt_regs.scratch_r.rcx);
    // t_print_bochs(" => %%r8:  0x%h\n", kernel_interrupt_regs.scratch_r.r8);
    // t_print_bochs(" => %%r9:  0x%h\n", kernel_interrupt_regs.scratch_r.r9);
    // t_print_bochs(" => %%rax: 0x%h\n", kernel_interrupt_regs.scratch_r.rax);
    // t_print_bochs(" => %%r10: 0x%h\n", kernel_interrupt_regs.scratch_r.r10);
    // t_print_bochs(" => %%r11: 0x%h\n", kernel_interrupt_regs.scratch_r.r11);
    // t_print_bochs(" => %%rbx: 0x%h\n", kernel_interrupt_regs.preserved_r.rbx);
    // t_print_bochs(" => %%rbp: 0x%h\n", kernel_interrupt_regs.preserved_r.rbp);
    // t_print_bochs(" => %%r12: 0x%h\n", kernel_interrupt_regs.preserved_r.r12);
    // t_print_bochs(" => %%r13: 0x%h\n", kernel_interrupt_regs.preserved_r.r13);
    // t_print_bochs(" => %%r14: 0x%h\n", kernel_interrupt_regs.preserved_r.r14);
    // t_print_bochs(" => %%r15: 0x%h\n", kernel_interrupt_regs.preserved_r.r15);
    // t_print_bochs(" => %%rip: 0x%h\n", kernel_interrupt_regs.e.rip);
    // t_print_bochs(" => %%rsp: 0x%h\n", kernel_interrupt_regs.e.rsp);
    // t_print_bochs(" => %%rflags: 0x%h\n", kernel_interrupt_regs.e.rflags.numb);
    // print_registers(kernel_interrupt_regs, bochs_logger);
    // print_kernel_int_stack_trace(bochs_logger);
    abort();
}

void print_stack_trace(TaskDescriptor *task, Logger &logger)
{
    if (not task)
        return;

    logger.printf("Stack trace:\n");
    ulong *rbp = (ulong *)task->regs.xbp();
    while (rbp) {
        logger.printf(" => 0x%h\n", rbp[1]);
        rbp = (ulong *)rbp[0];
    }
}

// extern "C" void deal_with_pagefault_in_kernel()
// {
//     t_print_bochs("Error: Pagefault inside the kernel! Instr %h %%cr2 0x%h  "
//                   "error 0x%h CPU %i\n",
//                   get_cpu_struct()->jumpto_from, get_cpu_struct()->pagefault_cr2,
//                   get_cpu_struct()->pagefault_error, get_cpu_struct()->cpu_id);
//     print_registers(get_cpu_struct()->current_task, bochs_logger);
//     print_stack_trace(bochs_logger);

//     abort();
// }

// void kernel_jump_to(void (*function)(void))
// {
//     CPU_Info *c    = get_cpu_struct();
//     c->jumpto_from = c->nested_int_regs.program_counter();
//     c->jumpto_to   = (ulong)function;

//     c->nested_int_regs.program_counter() = (ulong)&jumpto_func;
// }

extern "C" void pagefault_manager(NestedIntContext *kernel_ctx, ulong err)
{
    CPU_Info *c = get_cpu_struct();

    if (kernel_ctx) {
        c->pagefault_cr2   = getCR2();
        panic("Pagefault in kernel");

        // c->pagefault_error = c->nested_int_regs.int_err;

        // kernel_jump_to(deal_with_pagefault_in_kernel);
        return;
    }

    TaskDescriptor *task = c->current_task;

    // Get the memory location which has caused the fault
    auto virtual_addr = getCR2();

    // t_print_bochs("Debug: Pagefault %h pid %i (%s) rip %h error %h\n",
    // virtual_addr, task->task_id, task->name.c_str(), task->regs.program_counter(), err);

    auto result = [&]() -> kresult_t {
        Auto_Lock_Scope scope_lock(task->page_table->lock);

        auto &regions       = task->page_table->paging_regions;
        const auto it       = regions.get_smaller_or_equal(virtual_addr);
        const auto addr_all = virtual_addr & ~07777;

        // Reads can't really be checked on x86, so don't set read flag
        ulong access_mask = (err & 0x002)       ? Generic_Mem_Region::Writeable
                            : 0 | (err & 0x010) ? Generic_Mem_Region::Executable
                                                : 0;

        if (it != regions.end() and it->is_in_range(virtual_addr)) {
            auto r = it->on_page_fault(access_mask, virtual_addr);
            if (!r.success())
                return r.result;
            if (!r.val)
                task->atomic_block_by_page(addr_all, &task->page_table->blocked_tasks);

            return 0;
        }
        return -EFAULT;
    }();

    if (result) {
        t_print_bochs("Debug: Pagefault %h pid %i rip %h error %h returned "
                      "error %i\n",
                      virtual_addr, task->task_id, task->regs.program_counter(), err, result);
        global_logger.printf("Warning: Pagefault %h pid %i (%s) rip %h error "
                             "%h -> %i killing process...\n",
                             virtual_addr, task->task_id, task->name.c_str(), task->regs.program_counter(), err,
                             result);

        print_registers(task, global_logger);
        // print_stack_trace(task, global_logger);

        task->atomic_kill();
    }
}

extern "C" void sse_exception_manager()
{
    validate_sse();
    get_cpu_struct()->current_task->sse_data.restore_sse();
}

void print_kernel_regs(NestedIntContext *kernel_ctx)
{
    serial_logger.printf("Kernel registers:\n");
    serial_logger.printf(" => %%rax: 0x%lx\n", kernel_ctx->rax);
    serial_logger.printf(" => %%rbx: 0x%lx\n", kernel_ctx->rbx);
    serial_logger.printf(" => %%rcx: 0x%lx\n", kernel_ctx->rcx);
    serial_logger.printf(" => %%rdx: 0x%lx\n", kernel_ctx->rdx);
    serial_logger.printf(" => %%rsi: 0x%lx\n", kernel_ctx->rsi);
    serial_logger.printf(" => %%rdi: 0x%lx\n", kernel_ctx->rdi);
    serial_logger.printf(" => %%rbp: 0x%lx\n", kernel_ctx->rbp);
    serial_logger.printf(" => %%r8: 0x%lx\n", kernel_ctx->r8);
    serial_logger.printf(" => %%r9: 0x%lx\n", kernel_ctx->r9);
    serial_logger.printf(" => %%r10: 0x%lx\n", kernel_ctx->r10);
    serial_logger.printf(" => %%r11: 0x%lx\n", kernel_ctx->r11);
    serial_logger.printf(" => %%r12: 0x%lx\n", kernel_ctx->r12);
    serial_logger.printf(" => %%r13: 0x%lx\n", kernel_ctx->r13);
    serial_logger.printf(" => %%r14: 0x%lx\n", kernel_ctx->r14);
    serial_logger.printf(" => %%r15: 0x%lx\n", kernel_ctx->r15);
    serial_logger.printf(" => %%rip: 0x%lx\n", kernel_ctx->rip);
    serial_logger.printf(" => %%rsp: 0x%lx\n", kernel_ctx->rsp);
    serial_logger.printf(" => %%rflags: 0x%lx\n", kernel_ctx->rflags);
    serial_logger.printf(" => %%cs: 0x%lx\n", kernel_ctx->cs);
    serial_logger.printf(" => %%ss: 0x%lx\n", kernel_ctx->ss);
}

extern "C" void general_protection_fault_manager(NestedIntContext *kernel_ctx, ulong err)
{
    if (kernel_ctx) {
        print_kernel_regs(kernel_ctx);
        panic("General protection fault in kernel, error %x\n", err);
        return;
    }

    task_ptr task = get_cpu_struct()->current_task;
    serial_logger.printf("!!! General Protection Fault (GP) error (segment) %h "
                         "PID %i (%s) RIP %h CS %h... Killing the process\n",
                         task->regs.int_err, task->task_id, task->name.c_str(), task->regs.program_counter(),
                         1234); // task->regs.cs
    print_registers(get_cpu_struct()->current_task, serial_logger);

    global_logger.printf("!!! General Protection Fault (GP) error (segment) %h "
                         "PID %i (%s) RIP %h CS %h... Killing the process\n",
                         task->regs.int_err, task->task_id, task->name.c_str(), task->regs.program_counter(),
                         1234);
    print_registers(get_cpu_struct()->current_task, global_logger);
    // print_stack_trace(task, global_logger);
    task->atomic_kill();
}

extern "C" void overflow_manager()
{
    task_ptr task = get_cpu_struct()->current_task;
    serial_logger.printf("!!! Overflow error %h RIP %h RSP %h PID %h (%s)\n", task->regs.int_err,
                         task->regs.program_counter(), task->regs.stack_pointer(), task->task_id, task->name.c_str());
    global_logger.printf("!!! Overflow error %h RIP %h RSP %h PID %h (%s)\n", task->regs.int_err,
                         task->regs.program_counter(), task->regs.stack_pointer(), task->task_id, task->name.c_str());
    task->atomic_kill();
}

extern "C" void simd_fp_exception_manager()
{
    task_ptr task = get_cpu_struct()->current_task;
    t_print_bochs("!!! SIMD FP Exception error %h RIP %h RSP %h PID %h (%s)\n", task->regs.int_err,
                  task->regs.program_counter(), task->regs.stack_pointer(), task->task_id, task->name.c_str());
    global_logger.printf("!!! SIMD FP Exception error %h RIP %h RSP %h PID %h (%s)\n",
                         task->regs.int_err, task->regs.program_counter(), task->regs.stack_pointer(), task->task_id,
                         task->name.c_str());
    task->atomic_kill();
}

extern "C" void invalid_opcode_manager()
{
    task_ptr task = get_cpu_struct()->current_task;
    serial_logger.printf("!!! Invalid op-code (UD) instr %h task %i (%s)\n",
                         get_cpu_struct()->current_task->regs.program_counter(), task->task_id,
                         task->name.c_str());
    global_logger.printf("!!! Invalid op-code (UD) instr %h\n",
                         get_cpu_struct()->current_task->regs.program_counter());
    task->atomic_kill();
}

extern "C" void stack_segment_fault_manager()
{
    task_ptr task = get_cpu_struct()->current_task;
    t_print_bochs("!!! Stack-Segment Fault error %h RIP %h RSP %h PID %h (%s)\n",
                  task->regs.int_err, task->regs.program_counter(), task->regs.stack_pointer(), task->task_id,
                  task->name.c_str());
    global_logger.printf("!!! Stack-Segment Fault error %h RIP %h RSP %h\n", task->regs.int_err,
                         task->regs.program_counter(), task->regs.stack_pointer());
    task->atomic_kill();
}

extern "C" void double_fault_manager(NestedIntContext *kernel_ctx, ulong err)
{
    panic("double fault!\n");
    task_ptr task = get_cpu_struct()->current_task;
    t_print_bochs("!!! Double Fault error %h RIP %h RSP %h PID %h (%s)\n", task->regs.int_err,
                  task->regs.program_counter(), task->regs.stack_pointer(), task->task_id, task->name.c_str());
    global_logger.printf("!!! Double Fault error %h RIP %h RSP %h PID %h (%s)\n",
                         task->regs.int_err, task->regs.program_counter(), task->regs.stack_pointer(), task->task_id,
                         task->name.c_str());
    task->atomic_kill();
}

void breakpoint_manager(NestedIntContext *kernel_ctx, ulong err)
{
    global_logger.printf("Warning: hit a breakpoint but it's not implemented\n");
}