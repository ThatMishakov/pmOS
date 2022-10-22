#include "misc.hh"
#include "linker.hh"
#include "utils.hh"

void* unoccupied = (void*)&_free_after_kernel;

void print_registers(TaskDescriptor* r)
{
    t_print("ss: %h ", r->regs.ss);
    t_print("rsp: %h ", r->regs.rsp);
    t_print("rflags: %h ", r->regs.rflags);
    t_print("cs: %h ", r->regs.cs);
    t_print("rip: %h ", r->regs.rip);
    t_print("err: %h ", r->regs.err);
    t_print("intno: %h ", r->regs.intno);
    t_print("rax: %h ", r->regs.rax);
    t_print("rbx: %h ", r->regs.rbx);
    t_print("rcx: %h ", r->regs.rcx);
    t_print("rdx: %h ", r->regs.rdx);
    t_print("rsi: %h ", r->regs.rsi);
    t_print("rdi: %h ", r->regs.rdi);
    t_print("rbp: %h ", r->regs.rbp);
    t_print("r8: %h ", r->regs.r8);
    t_print("r9: %h ", r->regs.r9);
    t_print("r10: %h ", r->regs.r10);
    t_print("r11: %h ", r->regs.r11);
    t_print("r12: %h ", r->regs.r12);
    t_print("r13: %h ", r->regs.r13);
    t_print("r14: %h ", r->regs.r14);
    t_print("r15: %h\n", r->regs.r15);
}