#ifndef ASM_HH
#define ASM_HH

extern "C" void printc(char c);
extern "C" void tlb_flush();
extern "C" void page_clear(void* page);

#endif