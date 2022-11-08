#ifndef ASM_HH
#define ASM_HH

extern "C" void printc(char c);
extern "C" void tlb_flush();
extern "C" void page_clear(void* page);
extern "C" void memset(u64* dir, u64 size_qwords);
extern "C" void loadTSS(u16 selector);
extern "C" u64 getCR3();
extern "C" u64 setCR3(u64 cr3);
extern "C" u64 getCR2();
extern "C" void set_segment_regs(u16);
extern "C" void invlpg(u64 virtual_addr);
extern "C" u64 read_msr(u32 msr);
extern "C" void write_msr(u32 msr, u64 val);

#endif