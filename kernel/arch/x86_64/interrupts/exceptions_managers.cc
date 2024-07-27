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
    logger.printf(" => %%rdi: 0x%h\n", regs.scratch_r.rdi);
    logger.printf(" => %%rsi: 0x%h\n", regs.scratch_r.rsi);
    logger.printf(" => %%rdx: 0x%h\n", regs.scratch_r.rdx);
    logger.printf(" => %%rcx: 0x%h\n", regs.scratch_r.rcx);
    logger.printf(" => %%r8:  0x%h\n", regs.scratch_r.r8);
    logger.printf(" => %%r9:  0x%h\n", regs.scratch_r.r9);
    logger.printf(" => %%rax: 0x%h\n", regs.scratch_r.rax);
    logger.printf(" => %%r10: 0x%h\n", regs.scratch_r.r10);
    logger.printf(" => %%r11: 0x%h\n", regs.scratch_r.r11);

    logger.printf(" => %%rbx: 0x%h\n", regs.preserved_r.rbx);
    logger.printf(" => %%rbp: 0x%h\n", regs.preserved_r.rbp);
    logger.printf(" => %%r12: 0x%h\n", regs.preserved_r.r12);
    logger.printf(" => %%r13: 0x%h\n", regs.preserved_r.r13);
    logger.printf(" => %%r14: 0x%h\n", regs.preserved_r.r14);
    logger.printf(" => %%r15: 0x%h\n", regs.preserved_r.r15);

    logger.printf(" => %%rip: 0x%h\n", regs.e.rip);
    logger.printf(" => %%rsp: 0x%h\n", regs.e.rsp);
    logger.printf(" => %%rflags: 0x%h\n", regs.e.rflags.numb);

    logger.printf(" => %%gs offset: 0x%h\n", regs.seg.gs);
    logger.printf(" => %%fs offset: 0x%h\n", regs.seg.fs);

    logger.printf(" Entry type: %i\n", regs.entry_type);

    logger.printf(" Error code: 0x%h\n", regs.int_err);
}

void print_registers(const klib::shared_ptr<TaskDescriptor> &task, Logger &logger)
{
    if (not task)
        return;

    logger.printf("Registers for task %i (%s)\n", task->task_id, task->name.c_str());
    print_registers(task->regs, logger);
}

Task_Regs kernel_interrupt_regs;
extern "C" void dbg_main()
{
    t_print_bochs("Error! Kernel interrupt!\n");
    print_registers(kernel_interrupt_regs, bochs_logger);
    while (1)
        ;
}

void print_stack_trace(const klib::shared_ptr<TaskDescriptor> &task, Logger &logger)
{
    if (not task)
        return;

    logger.printf("Stack trace:\n");
    u64 *rbp = (u64 *)task->regs.preserved_r.rbp;
    while (rbp) {
        logger.printf(" => 0x%h\n", rbp[1]);
        rbp = (u64 *)rbp[0];
    }
}

extern "C" void deal_with_pagefault_in_kernel()
{
    t_print_bochs("Error: Pagefault inside the kernel! Instr %h %%cr2 0x%h  "
                  "error 0x%h CPU %i\n",
                  get_cpu_struct()->jumpto_from, get_cpu_struct()->pagefault_cr2,
                  get_cpu_struct()->pagefault_error, get_cpu_struct()->cpu_id);
    print_registers(get_cpu_struct()->current_task, bochs_logger);
    print_stack_trace(bochs_logger);

    abort();
}

void kernel_jump_to(void (*function)(void))
{
    CPU_Info *c    = get_cpu_struct();
    c->jumpto_from = c->nested_int_regs.e.rip;
    c->jumpto_to   = (u64)function;

    c->nested_int_regs.e.rip = (u64)&jumpto_func;
}

void print_pt_chain(u64 page, Logger &logger)
{
    PML4E *pml4e = get_pml4e(page, rec_map_index);
    logger.printf("PML4E: %h\n", *((u64 *)pml4e));
    if (not pml4e->present)
        return;

    PDPTE *pdpte = get_pdpe(page, rec_map_index);
    logger.printf("PDPTE: %h\n", *((u64 *)pdpte));
    if (not pdpte->present)
        return;

    PDE *pde = get_pde(page, rec_map_index);
    logger.printf("PDE: %h\n", *((u64 *)pde));
    if (not pde->present)
        return;

    PTE *pte = get_pte(page, rec_map_index);
    logger.printf("PTE: %h\n", *((u64 *)pte));
}

bool is_protection_violation(u64 err_code) noexcept { return err_code & 0x01; }

extern "C" void pagefault_manager()
{
    CPU_Info *c = get_cpu_struct();

    if (c->nested_level) {
        c->pagefault_cr2   = getCR2();
        c->pagefault_error = c->nested_int_regs.int_err;

        kernel_jump_to(deal_with_pagefault_in_kernel);
        return;
    }

    klib::shared_ptr<TaskDescriptor> task = c->current_task;
    u64 err                               = task->regs.int_err;

    // Get the memory location which has caused the fault
    u64 virtual_addr = getCR2();

    // t_print_bochs("Debug: Pagefault %h pid %i (%s) rip %h error %h\n",
    // virtual_addr, task->task_id, task->name.c_str(), task->regs.e.rip, err);

    try {
        if (is_protection_violation(err))
            throw Kern_Exception(-EFAULT, "Page protection violation");

        Auto_Lock_Scope scope_lock(task->page_table->lock);

        auto &regions       = task->page_table->paging_regions;
        const auto it       = regions.get_smaller_or_equal(virtual_addr);
        const auto addr_all = virtual_addr & ~07777;

        // Reads can't really be checked on x86, so don't set read flag
        u64 access_mask = (err & 0x002)       ? Generic_Mem_Region::Writeable
                          : 0 | (err & 0x010) ? Generic_Mem_Region::Executable
                                              : 0;

        if (it != regions.end() and it->is_in_range(virtual_addr)) {
            auto r = it->on_page_fault(access_mask, virtual_addr);
            if (not r)
                task->atomic_block_by_page(addr_all, &task->page_table->blocked_tasks);
        } else {
            throw Kern_Exception(-EFAULT, "pagefault in unknown region");
        }

    } catch (const Kern_Exception &e) {
        t_print_bochs("Debug: Pagefault %h pid %i rip %h error %h returned "
                      "error %i (%s)\n",
                      virtual_addr, task->task_id, task->regs.e.rip, err, e.err_code,
                      e.err_message);
        global_logger.printf("Warning: Pagefault %h pid %i (%s) rip %h error "
                             "%h -> %i killing process...\n",
                             virtual_addr, task->task_id, task->name.c_str(), task->regs.e.rip, err,
                             e.err_code);

        print_pt_chain(virtual_addr, global_logger);
        print_registers(task, global_logger);
        print_stack_trace(task, global_logger);

        task->atomic_kill();
    }
}

extern "C" void sse_exception_manager()
{
    validate_sse();
    get_cpu_struct()->current_task->sse_data.restore_sse();
}

extern "C" void general_protection_fault_manager()
{
    task_ptr task = get_cpu_struct()->current_task;
    // t_print_bochs("!!! General Protection Fault (GP) error %h\n", err);
    global_logger.printf("!!! General Protection Fault (GP) error (segment) %h "
                         "PID %i (%s) RIP %h CS %h... Killing the process\n",
                         task->regs.int_err, task->task_id, task->name.c_str(), task->regs.e.rip,
                         task->regs.e.cs);
    print_registers(get_cpu_struct()->current_task, global_logger);
    print_stack_trace(task, global_logger);
    task->atomic_kill();
}

extern "C" void invalid_opcode_manager()
{
    task_ptr task = get_cpu_struct()->current_task;
    global_logger.printf("!!! Invalid op-code (UD) instr %h\n",
                         get_cpu_struct()->current_task->regs.e.rip);
    task->atomic_kill();
}

extern "C" void stack_segment_fault_manager()
{
    task_ptr task = get_cpu_struct()->current_task;
    t_print_bochs("!!! Stack-Segment Fault error %h RIP %h RSP %h PID %h (%s)\n",
                  task->regs.int_err, task->regs.e.rip, task->regs.e.rsp, task->task_id,
                  task->name.c_str());
    global_logger.printf("!!! Stack-Segment Fault error %h RIP %h RSP %h\n", task->regs.int_err,
                         task->regs.e.rip, task->regs.e.rsp);
    task->atomic_kill();
}

extern "C" void double_fault_manager()
{
    task_ptr task = get_cpu_struct()->current_task;
    t_print_bochs("!!! Double Fault error %h RIP %h RSP %h PID %h (%s)\n", task->regs.int_err,
                  task->regs.e.rip, task->regs.e.rsp, task->task_id, task->name.c_str());
    global_logger.printf("!!! Double Fault error %h RIP %h RSP %h PID %h (%s)\n",
                         task->regs.int_err, task->regs.e.rip, task->regs.e.rsp, task->task_id,
                         task->name.c_str());
    task->atomic_kill();
}

void breakpoint_manager()
{
    global_logger.printf("Warning: hit a breakpoint but it's not implemented\n");
}