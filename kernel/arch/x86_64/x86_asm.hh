#pragma once
#include <types.hh>

extern "C" void bochs_printc(char c);
extern "C" void tlb_flush();
extern "C" void page_clear(void* page);
extern "C" void loadTSS(u16 selector);
extern "C" void set_segment_regs(u16);
extern "C" void invlpg(u64 virtual_addr);

extern "C" u64 read_msr(u32 msr);
extern "C" void write_msr(u32 msr, u64 val);

extern "C" u64 getCR0();
extern "C" void setCR0(u64 cr0);

extern "C" u64 getCR2();
extern "C" void setCR2(u64 cr2);

extern "C" u64 getCR3();
extern "C" void setCR3(u64 cr3);

extern "C" u64 getCR4();
extern "C" void setCR4(u64 cr4);