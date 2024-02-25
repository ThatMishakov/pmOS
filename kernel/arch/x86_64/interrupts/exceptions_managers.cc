#include "exceptions_managers.hh"
#include <utils.hh>
#include <asm.hh>
#include <memory/paging.hh>
#include <processes/syscalls.hh>
#include <processes/tasks.hh>
#include <cpus/sse.hh>
#include <kern_logger/kern_logger.hh>
#include <stdlib.h>

void print_registers(const klib::shared_ptr<TaskDescriptor>& task)
{
    if (not task) {
        t_print_bochs("get_cpu_struct()->current_task == nullptr!\n");
        return;
    }

    const Task_Regs& regs = task->regs;
    t_print_bochs("Current task pid %i (%s). Registers:\n", task->pid, task->name.c_str());
    t_print_bochs(" => %%rdi: 0x%h\n", regs.scratch_r.rdi);
    t_print_bochs(" => %%rsi: 0x%h\n", regs.scratch_r.rsi);
    t_print_bochs(" => %%rdx: 0x%h\n", regs.scratch_r.rdx);
    t_print_bochs(" => %%rcx: 0x%h\n", regs.scratch_r.rcx);
    t_print_bochs(" => %%r8:  0x%h\n", regs.scratch_r.r8);
    t_print_bochs(" => %%r9:  0x%h\n", regs.scratch_r.r9);
    t_print_bochs(" => %%rax: 0x%h\n", regs.scratch_r.rax);
    t_print_bochs(" => %%r10: 0x%h\n", regs.scratch_r.r10);
    t_print_bochs(" => %%r11: 0x%h\n", regs.scratch_r.r11);

    t_print_bochs(" => %%rbx: 0x%h\n", regs.preserved_r.rbx);
    t_print_bochs(" => %%rbp: 0x%h\n", regs.preserved_r.rbp);
    t_print_bochs(" => %%r12: 0x%h\n", regs.preserved_r.r12);
    t_print_bochs(" => %%r13: 0x%h\n", regs.preserved_r.r13);
    t_print_bochs(" => %%r14: 0x%h\n", regs.preserved_r.r14);
    t_print_bochs(" => %%r15: 0x%h\n", regs.preserved_r.r15);

    t_print_bochs(" => %%rip: 0x%h\n", regs.e.rip);
    t_print_bochs(" => %%rsp: 0x%h\n", regs.e.rsp);
    t_print_bochs(" => %%rflags: 0x%h\n", regs.e.rflags.numb);

    t_print_bochs(" => %%gs offset: 0x%h\n", regs.seg.gs);
    t_print_bochs(" => %%fs offset: 0x%h\n", regs.seg.fs);

    t_print_bochs(" Entry type: %i\n", regs.entry_type);
}

void print_stack_trace(const klib::shared_ptr<TaskDescriptor>& task)
{
    if (not task)
        return;

    t_print_bochs("Stack trace:\n");
    u64* rbp = (u64*)task->regs.preserved_r.rbp;
    while (rbp) {
        t_print_bochs(" => 0x%h\n", rbp[1]);
        rbp = (u64*)rbp[0];
    }
}

extern "C" void deal_with_pagefault_in_kernel()
{
    t_print_bochs("Error: Pagefault inside the kernel! Instr %h %%cr2 0x%h  error 0x%h CPU %i\n", get_cpu_struct()->jumpto_from, get_cpu_struct()->pagefault_cr2, get_cpu_struct()->pagefault_error, get_cpu_struct()->cpu_id);
    print_registers(get_cpu_struct()->current_task);
    print_stack_trace(bochs_logger);

    abort();
}

void kernel_jump_to(void (*function)(void))
{
    CPU_Info *c = get_cpu_struct();
    c->jumpto_from = c->nested_int_regs.e.rip;
    c->jumpto_to   = (u64)function;

    c->nested_int_regs.e.rip = (u64)&jumpto_func;
}

void print_pt_chain(u64 page, Logger& logger)
{
    PML4E* pml4e = get_pml4e(page, rec_map_index);
    logger.printf("PML4E: %h\n", *((u64 *)pml4e));
    if (not pml4e->present)
        return;

    PDPTE* pdpte = get_pdpe(page, rec_map_index);
    logger.printf("PDPTE: %h\n", *((u64 *)pdpte));
    if (not pdpte->present)
        return;

    PDE* pde = get_pde(page, rec_map_index);
    logger.printf("PDE: %h\n", *((u64 *)pde));
    if (not pde->present)
        return;

    PTE* pte = get_pte(page, rec_map_index);
    logger.printf("PTE: %h\n", *((u64 *)pte));
}

extern "C" void pagefault_manager()
{
    CPU_Info *c = get_cpu_struct();

    if (c->nested_level) {
        c->pagefault_cr2 = getCR2();
        c->pagefault_error = c->nested_int_regs.int_err;

        kernel_jump_to(deal_with_pagefault_in_kernel);
        return;
    }

    klib::shared_ptr<TaskDescriptor> task = c->current_task;
    u64 err = task->regs.int_err;

    // Get the memory location which has caused the fault
    u64 virtual_addr = getCR2();

    //t_print_bochs("Debug: Pagefault %h pid %i (%s) rip %h error %h\n", virtual_addr, task->pid, task->name.c_str(), task->regs.e.rip, err);

    try {
        Auto_Lock_Scope scope_lock(task->page_table->lock);

        auto& regions = task->page_table->paging_regions;
        const auto it = regions.get_smaller_or_equal(virtual_addr);
        const auto addr_all = virtual_addr & ~07777;

        if (it != regions.end() and it->second->is_in_range(virtual_addr)) {
            auto r = it->second->on_page_fault(err, virtual_addr);
            if (not r)
                task->atomic_block_by_page(addr_all, &task->page_table->blocked_tasks);
        } else {
            throw Kern_Exception(ERROR_PAGE_NOT_ALLOCATED, "pagefault in unknown region");
        }

    } catch (const Kern_Exception& e) {
        t_print_bochs("Debug: Pagefault %h pid %i rip %h error %h returned error %i (%s)\n", virtual_addr, task->pid, task->regs.e.rip, err, e.err_code, e.err_message);
        global_logger.printf("Warning: Pagefault %h pid %i (%s) rip %h error %h -> %i killing process...\n", virtual_addr, task->pid, task->name.c_str(), task->regs.e.rip, err, e.err_code);
        
        print_pt_chain(virtual_addr, global_logger);
        print_registers(task);
        print_stack_trace(task);

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
    //t_print_bochs("!!! General Protection Fault (GP) error %h\n", err);
    global_logger.printf("!!! General Protection Fault (GP) error (segment) %h PID %i (%s) RIP %h CS %h... Killing the process\n", task->regs.int_err, task->pid, task->name.c_str(), task->regs.e.rip, task->regs.e.cs);
    print_registers(get_cpu_struct()->current_task);
    print_stack_trace(task);
    task->atomic_kill();
}

extern "C" void invalid_opcode_manager()
{
    task_ptr task = get_cpu_struct()->current_task;
    global_logger.printf("!!! Invalid op-code (UD) instr %h\n", get_cpu_struct()->current_task->regs.e.rip);
    task->atomic_kill();
}

extern "C" void stack_segment_fault_manager()
{
    task_ptr task = get_cpu_struct()->current_task;
    t_print_bochs("!!! Stack-Segment Fault error %h RIP %h RSP %h PID %h (%s)\n", task->regs.int_err, task->regs.e.rip, task->regs.e.rsp, task->pid, task->name.c_str());
    global_logger.printf("!!! Stack-Segment Fault error %h RIP %h RSP %h\n", task->regs.int_err, task->regs.e.rip, task->regs.e.rsp);
    task->atomic_kill();
}

extern "C" void double_fault_manager()
{
    task_ptr task = get_cpu_struct()->current_task;
    t_print_bochs("!!! Double Fault error %h RIP %h RSP %h PID %h (%s)\n", task->regs.int_err, task->regs.e.rip, task->regs.e.rsp, task->pid, task->name.c_str());
    global_logger.printf("!!! Double Fault error %h RIP %h RSP %h PID %h (%s)\n", task->regs.int_err, task->regs.e.rip, task->regs.e.rsp, task->pid, task->name.c_str());
    task->atomic_kill();
}

void breakpoint_manager()
{
    global_logger.printf("Warning: hit a breakpoint but it's not implemented\n");
}