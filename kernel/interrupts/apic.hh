#pragma once
#include <types.hh>

#define APIC_SPURIOUS_INT        0xff

static constexpr u64 apic_base = 0xFEE00000;
extern void* apic_mapped_addr;

void apic_write_reg(u16 index, u32 val);
u32 apic_read_reg(u16 index);

void init_apic();

#define IA32_APIC_BASE_MSR 0x1B
u64 cpu_get_apic_base();
void cpu_set_apic_base(u64 base);