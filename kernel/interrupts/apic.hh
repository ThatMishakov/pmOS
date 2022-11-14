#pragma once
#include <types.hh>

#define APIC_REG_LAPIC_ID        0x20
#define APIC_REG_TPR             0x80
#define APIC_REG_LDR             0xd0
#define APIC_REG_DFR             0xe0
#define APIC_REG_SPURIOUS_INT    0xf0
#define APIC_REG_LVT_TMR         0x320
#define APIC_REG_LVT_INT0        0x350
#define APIC_REG_LVT_INT1        0x360
#define APIC_REG_TMRINITCNT      0x380
#define APIC_REG_TMRCURRCNT      0x390
#define APIC_REG_TMRDIV	         0x3e0

#define APIC_SPURIOUS_INT        0xff
#define LVT_INT0                 0xfc
#define LVT_INT1                 0xfe
#define APIC_DUMMY_ISR           0xfd
#define APIC_TMR_INT             0xfb          

#define APIC_LVT_MASK            0x10000


#define APIC_ENABLE (1 << 11)

static constexpr u64 apic_base = 0xFEE00000;
extern void* apic_mapped_addr;

void apic_write_reg(u16 index, u32 val);
u32 apic_read_reg(u16 index);

void enable_apic();
void map_apic();
void init_apic();
void discover_apic_freq();

#define IA32_APIC_BASE_MSR 0x1B
u64 cpu_get_apic_base();
void cpu_set_apic_base(u64 base);

extern u32 ticks_per_1_ms;
void apic_one_shot(u32 ms);