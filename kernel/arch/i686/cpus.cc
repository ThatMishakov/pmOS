#include <sched/sched.hh>
#include <x86_asm.hh>
#include <interrupts/gdt.hh>
#include <paging/x86_paging.hh>

using namespace kernel::sched;
using namespace kernel::x86::paging;

extern "C" void double_fault_isr();

bool setup_stacks(CPU_Info *c)
{
    if (!c->kernel_stack)
        return false;

    c->kernel_stack_top = c->kernel_stack.get_stack_top();
    c->tss.esp0         = (u32)c->kernel_stack_top;

    c->cpu_gdt.tss = tss_to_base(&c->tss);

    if (!c->double_fault_stack)
        return false;

    c->double_fault_tss.esp = (u32)c->double_fault_stack.get_stack_top();
    c->double_fault_tss.eip = (u32)double_fault_isr;
    c->double_fault_tss.cr3 = idle_cr3;

    c->cpu_gdt.double_fault_tss = tss_to_base(&c->double_fault_tss);

    return true;
}