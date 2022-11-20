#pragma once
#include <types.hh>

#define APIC_REG_LAPIC_ID        0x20
#define APIC_REG_TPR             0x80
#define APIC_REG_EOI             0xb0
#define APIC_REG_LDR             0xd0
#define APIC_REG_DFR             0xe0
#define APIC_REG_SPURIOUS_INT    0xf0
#define APIC_ISR_REG_START       0x100
#define APIC_IRR_REG_START       0x200
#define APIC_ICR_LOW             0x300
#define APIC_ICR_HIGH            0x310
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
u32 get_lapic_id();

void enable_apic();
void map_apic();
void prepare_apic();
void discover_apic_freq();

void apic_eoi();

#define IA32_APIC_BASE_MSR 0x1B
u64 cpu_get_apic_base();
void cpu_set_apic_base(u64 base);

extern u32 ticks_per_1_ms;
void apic_one_shot(u32 ms);
void apic_one_shot_ticks(u32 ticks);
u32 apic_get_remaining_ticks();

struct ICR {
    u8 vector         :8  = 0; 
    u8 delivery_mode  :3  = 0;
    u8 dest_mode      :1  = 0;
    u8 deliv_status   :1  = 0; // Read only
    u8 reserved0      :1  = 0;
    u8 level          :1  = 1;
    u8 trigger_mode   :1  = 0;
    u8 reserved1      :2  = 0;
    u8 dest_shorthand :2  = 0;
    u16 reserved2     :12 = 0;
    u32 reserved3     :24 = 0;
    u8 dest_field     :8  = 0;
} PACKED ALIGNED(4);

void write_ICR(ICR);
ICR read_ICR();

ReturnStr<u64> lapic_configure(u64 opt, u64 arg);

void broadcast_init_ipi();
void broadcast_sipi(u8 vector);
void send_ipi_fixed(u8 vector, u8 dest);

#define APIC_DELIVERY_START_UP    0b110
#define APIC_DELIVERY_INIT        0b100

void smart_eoi(u8 intno);
