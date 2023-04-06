#include "ipi.hh"
#include <types.hh>
#include <sched/sched.hh>
#include <interrupts/apic.hh>


void ipi_invalidate_tlb_routine()
{
    setCR3(getCR3());

    apic_eoi();
}

void signal_tlb_shootdown()
{
    send_ipi_fixed_others(ipi_invalidate_tlb_int_vec);
}

void reschedule_isr()
{
    reschedule();
    apic_eoi();
}