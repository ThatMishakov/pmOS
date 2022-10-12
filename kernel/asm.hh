#ifndef ASM_HH
#define ASM_HH
#include "interrupts.hh"

extern "C" void printc(char c);
extern "C" void tlb_flush();
extern "C" void page_clear(void* page);
extern "C" void loadIDT(IDT_descriptor* IDTdescriptor);

#endif