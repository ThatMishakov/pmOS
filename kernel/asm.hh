#ifndef ASM_HH
#define ASM_HH

extern "C" void printc(char c);
extern "C" void tlb_flush();
extern "C" void page_clear(void* page);
extern "C" void memset(uint64_t* dir, uint64_t size_qwords);
extern "C" void loadTSS(uint16_t selector);

#endif