#include "gdt.hh"
#include <types.hh>
#include <kern_logger/kern_logger.hh>
#include <x86_asm.hh>

extern "C" void double_fault_handler()
{
    serial_logger.printf("Double fault!!!\n");
    auto tss_selector = str();
    TSS *tss = getTSS(tss_selector);
    assert(tss);
    TSS *faulting_tss = getTSS(tss->link);
    assert(faulting_tss);

    serial_logger.printf("Faulting TSS: %u\n", tss->link);
    serial_logger.printf("Registers:\n");
    serial_logger.printf("EAX: 0x%x (%u)\n", faulting_tss->eax, faulting_tss->eax);
    serial_logger.printf("EBX: 0x%x (%u)\n", faulting_tss->ebx, faulting_tss->ebx);
    serial_logger.printf("ECX: 0x%x (%u)\n", faulting_tss->ecx, faulting_tss->ecx);
    serial_logger.printf("EDX: 0x%x (%u)\n", faulting_tss->edx, faulting_tss->edx);
    serial_logger.printf("ESI: 0x%x (%u)\n", faulting_tss->esi, faulting_tss->esi);
    serial_logger.printf("EDI: 0x%x (%u)\n", faulting_tss->edi, faulting_tss->edi);
    serial_logger.printf("ESP: 0x%x (%u)\n", faulting_tss->esp, faulting_tss->esp);
    serial_logger.printf("EIP: 0x%x (%u)\n", faulting_tss->eip, faulting_tss->eip);
    serial_logger.printf("CS:  0x%x (%u)\n", faulting_tss->cs, faulting_tss->cs);
    serial_logger.printf("SS:  0x%x (%u)\n", faulting_tss->ss, faulting_tss->ss);
    serial_logger.printf("ES:  0x%x (%u)\n", faulting_tss->es, faulting_tss->es);
    serial_logger.printf("DS:  0x%x (%u)\n", faulting_tss->ds, faulting_tss->ds);
    serial_logger.printf("FS:  0x%x (%u)\n", faulting_tss->fs, faulting_tss->fs);
    serial_logger.printf("GS:  0x%x (%u)\n", faulting_tss->gs, faulting_tss->gs);
    serial_logger.printf("ldt: 0x%x (%u)\n", faulting_tss->ldt, faulting_tss->ldt);
    serial_logger.printf("cr3: 0x%x (%u)\n", faulting_tss->cr3, faulting_tss->cr3);

    panic("panicking idk...");
}