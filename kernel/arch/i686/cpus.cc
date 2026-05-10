#include <sched/sched.hh>
#include <x86_asm.hh>
#include <interrupts/gdt.hh>
#include <paging/x86_paging.hh>

using namespace kernel::sched;
using namespace kernel::x86::paging;
using namespace kernel::ia32::interrupts;

extern "C" void double_fault_isr();

bool setup_stacks(CPU_Info *c)
{
    if (!c->kernel_stack)
        return false;

    TSS *tss = (TSS *)c->tss_virt;
    *tss = {};

    c->kernel_stack_top = c->kernel_stack.get_stack_top();
    tss->esp0         = (u32)c->kernel_stack_top;

    c->cpu_gdt.tss = tss_to_base(tss, PAGE_SIZE - 1);

    if (!c->double_fault_stack)
        return false;

    c->double_fault_tss.esp = (u32)c->double_fault_stack.get_stack_top();
    c->double_fault_tss.eip = (u32)double_fault_isr;
    c->double_fault_tss.cr3 = idle_cr3;

    c->cpu_gdt.double_fault_tss = tss_to_base(&c->double_fault_tss, sizeof(TSS) - 1);

    return true;
}

namespace kernel::x86::gdt {

void io_bitmap_enable()
{
    auto c = sched::get_cpu_struct();
    c->cpu_gdt.tss = tss_to_base((TSS *)c->tss_virt, PAGE_SIZE*3 - 1);
    loadTSS(TSS_SEGMENT);
}

void io_bitmap_disable()
{
    auto c = sched::get_cpu_struct();
    c->cpu_gdt.tss = tss_to_base((TSS *)c->tss_virt, PAGE_SIZE - 1);
    loadTSS(TSS_SEGMENT);
}

}