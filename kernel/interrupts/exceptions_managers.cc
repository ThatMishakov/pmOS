#include "exceptions_managers.hh"
#include <utils.hh>
#include <asm.hh>
#include <memory/paging.hh>
#include <processes/syscalls.hh>
#include <processes/tasks.hh>
#include <cpus/sse.hh>

void register_exceptions(IDT& idt)
{
    idt.register_isr(invalid_opcode_num, &invalid_opcode_isr, interrupt_gate_type, 0, 0);
    idt.register_isr(sse_exception_num, &sse_exception_isr, interrupt_gate_type, 0, 0);
    idt.register_isr(stack_segment_fault_num, &stack_segment_fault_isr, interrupt_gate_type, 0, 0);
    idt.register_isr(general_protection_fault_num, &general_protection_fault_isr, interrupt_gate_type, 0, 0);
    idt.register_isr(pagefault_num, &pagefault_isr, interrupt_gate_type, 0, 0);
}

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

extern "C" void deal_with_pagefault_in_kernel()
{
    t_print_bochs("Error: Pagefault inside the kernel! Instr %h %%cr2 0x%h  error 0x%h\n", get_cpu_struct()->jumpto_from, get_cpu_struct()->pagefault_cr2, get_cpu_struct()->pagefault_error);
    print_registers(get_cpu_struct()->current_task);
    print_stack_trace();
    while (1)
    {
        asm("hlt");
    }
}

void kernel_jump_to(void (*function)(void))
{
    CPU_Info *c = get_cpu_struct();
    c->jumpto_from = c->nested_int_regs.e.rip;
    c->jumpto_to   = (u64)function;

    c->nested_int_regs.e.rip = (u64)&jumpto_func;
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
    Interrupt_Stackframe* int_s = &task->regs.e;

    // Get the memory location which has caused the fault
    u64 virtual_addr = getCR2();

    // Get the page
    u64 page = virtual_addr&~0xfff;

    // Get page type
    Page_Types type = page_type(page);

    //t_print_bochs("Debug: Pagefault %h pid %i rip %h error %h\n", virtual_addr, get_cpu_struct()->current_task->pid, int_s->rip, err);

    switch (type) {
    case Page_Types::LAZY_ALLOC: // Lazilly allocated page caused the fault
        //t_print_bochs("delayed allocation\n");
        get_lazy_page(page);
        break;
    case Page_Types::UNALLOCATED: // Page not allocated
        t_print_bochs("Warning: Pagefault %h pid %i rip %h error %h -> unallocated... killing process...\n", virtual_addr, get_cpu_struct()->current_task->pid, int_s->rip, err);
        syscall_exit(4, 0);
        break;
    default:
        // Not implemented
        t_print_bochs("Debug: Pagefault %h pid %i rip %h error %h -> %h not implemented. Halting...\n", virtual_addr, get_cpu_struct()->current_task->pid, int_s->rip, err, type);
        halt();
        break;
    }
}

extern "C" void sse_exception_manager()
{
    validate_sse();
    get_cpu_struct()->current_task->sse_data.restore_sse();
}

extern "C" void general_protection_fault_manager()
{
    const task_ptr& task = get_cpu_struct()->current_task;
    //t_print_bochs("!!! General Protection Fault (GP) error %h\n", err);
    t_print_bochs("!!! General Protection Fault (GP) error (segment) %h PID %i RIP %h CS %h... Killing the process\n", task->regs.int_err, task->pid, task->regs.e.rip, task->regs.e.cs);
    syscall_exit(4, 0);
}

extern "C" void invalid_opcode_manager()
{
    t_print_bochs("!!! Invalid op-code (UD) instr %h\n", get_cpu_struct()->current_task->regs.e.rip);
    halt();
}

extern "C" void stack_segment_fault_manager()
{
    const task_ptr& task = get_cpu_struct()->current_task;
    t_print_bochs("!!! Stack-Segment Fault error %h RIP %h RSP %h\n", task->regs.int_err, task->regs.e.rip, task->regs.e.rsp);
    t_print("!!! Stack-Segment Fault error %h RIP %h RSP %h\n", task->regs.int_err, task->regs.e.rip, task->regs.e.rsp);
    syscall_exit(4, 0);
}