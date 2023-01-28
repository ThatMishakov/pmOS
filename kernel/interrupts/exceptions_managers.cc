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

extern "C" void pagefault_manager()
{
    klib::shared_ptr<TaskDescriptor> task = get_cpu_struct()->current_task;

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