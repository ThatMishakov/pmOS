#include "misc.hh"
#include "linker.hh"
#include "utils.hh"

void* unoccupied = (void*)&_free_after_kernel;

void print_registers(TaskDescriptor* r)
{
    t_print("ss: %h\n", r->regs.ss);
    t_print("rsp: %h\n", r->regs.rsp);
    t_print("rflags: %h\n", r->regs.rflags);
    t_print("cs: %h\n", r->regs.cs);
    t_print("rip: %h\n", r->regs.rip);
    t_print("err: %h\n", r->regs.err);
    t_print("intno: %h\n", r->regs.intno);
    t_print("rax: %h\n", r->regs.rax);
    t_print("rbx: %h\n", r->regs.rbx);
    t_print("rcx: %h\n", r->regs.rcx);
    t_print("rdx: %h\n", r->regs.rdx);
    t_print("rsi: %h\n", r->regs.rsi);
    t_print("rdi: %h\n", r->regs.rdi);
    t_print("rbp: %h\n", r->regs.rbp);
    t_print("r8: %h\n", r->regs.r8);
    t_print("r9: %h\n", r->regs.r9);
    t_print("r10: %h\n", r->regs.r10);
    t_print("r11: %h\n", r->regs.r11);
    t_print("r12: %h\n", r->regs.r12);
    t_print("r13: %h\n", r->regs.r13);
    t_print("r14: %h\n", r->regs.r14);
    t_print("r15: %h\n", r->regs.r15);
}