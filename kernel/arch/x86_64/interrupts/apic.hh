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

#define APIC_TMR_INT             0xfc     
#define LVT_INT0                 0xfd
#define LVT_INT1                 0xfe 
#define APIC_SPURIOUS_INT        0xff 
#define APIC_DUMMY_ISR           0xff   

#define APIC_LVT_MASK            0x10000

/// Bit in the APIC MSR register, indicating that it is enabled
#define APIC_ENABLE (1 << 11)

/// The base of the APIC MMIO register
static constexpr u64 apic_base = 0xFEE00000;

/// Virtual address where APIC register is to be mapped
extern void* apic_mapped_addr;

/**
 * @brief Function used for writing a given APIC register.
 * 
 * APICs registers 4 byte memory-mapped, aligned to 16 bytes. This is a little helper
 * function which writes the valued to the correcs offsets against apic_mapped_addr
 * 
 * @param index Index of the register where the value is to be written
 * @param val Value to be written
 */
void apic_write_reg(u16 index, u32 val);

/**
 * @brief Read the specified LAPIC register
 * 
 * @param index Index of the register
 * @return Value of the register
 * @see apic_write_reg()
 */
u32 apic_read_reg(u16 index);

/**
 * @brief Get the ID of LAPIC of the current CPU
 * 
 * @return LAPIC id
 * @see apic_write_reg()
 */
u32 get_lapic_id();

/**
 * @brief Enables the LAPIC and sets the appropriate interrupt vectors
 */
void enable_apic();

/**
 * @brief Maps the LAPIC register to the virtual memory
 * 
 * This function maps the LAPIC register physical address to the kernel's virtual space (since we don't use identity mappings).
 * Note that this is only needed to be done once as different CPUs are set up to have the same (default) LAPIC register physical
 * address.
 */
void map_apic();

/**
 * @brief Disables PIC and maps LAPIC to the kernel virtual memory.
 */
void prepare_apic();

/**
 * @brief Discovers LAPIC frequency using PIT
 * 
 * Each LAPIC has a (per-CPU) built in timer which is used for scheduling and some other kernel duties. However, its tick speed
 * is dependent on one of the CPU frequencies and thus is it machine dependent and there is way to tell know it without some external
 * reference. Fortunatelly, x86 has an old (supposedly external) PIT timer which is oscillating at a standart known frequency of
 * 1,193,182 Hz. Although it is clunky and not very usefull for multi-processor scheduling, it is vary handy to find the approximate
 * LAPIC frequency. This function sets up PIT to fire after a known interval and, counting how many ticks have been counted by the LAPIC
 * timer, measures its frequency to later be used for kernel duties.
 */
void discover_apic_freq();

/**
 * @brief Sends an EOI to LAPIC
 * 
 * This routine signals End Of Interrupt of the current CPU, signalling that the interrupt processing has been finished and that new interrupts are
 * ready to be accepted. This is acomplished by writing some value (for example, 0) to APIC_REG_EOI register.
 */
void apic_eoi();

/// MSR used for LAPIC configuration
#define IA32_APIC_BASE_MSR 0x1B

/// Returns the LAPIC base 
u64 cpu_get_apic_base();

/// Sets the LAPIC base MSR register to *base*
void cpu_set_apic_base(u64 base);

/// @brief Ticks that LAPIC timer does per 1 ms
/// @see discover_apic_freq()
extern u32 ticks_per_1_ms;

/// @brief Fires a single shot of LAPIC timer
/// @param ms Time in milliseconds before the interrupt would be fired
void apic_one_shot(u32 ms);

/// @brief Fires a single shot of LAPIC timer
/// @param ticks Number of ticks to recieving the interrupt
/// @see ticks_per_1_ms
void apic_one_shot_ticks(u32 ticks);

/// @brief Gets the ticks remaining untill the interrupt
/// @return The remaining number of ticks
u32 apic_get_remaining_ticks();

/** @brief LAPIC ICR structure
 */
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

/// @brief Writes ICR to the LAPIC
/// @param  ICR structure to be written
void write_ICR(ICR);

/// @brief Reads ICR from LAPIC
ICR read_ICR();

/**
 * @brief Configures different LAPIC functions
 * 
 * This function configures different LAPIC options. Its name is misleading and I am not going to document it.
 */
u64 lapic_configure(u64 opt, u64 arg);

/// @brief Broadcasts INIT IPI to the other processes
void broadcast_init_ipi();

/// @brief Broadcasrs SIPI to other processors
/// @param vector Vector to which it will be recieved (from where the CPU will start executing instructions)
void broadcast_sipi(u8 vector);

/// @brief Sends a fixed IPI
/// @param vector Destination interrupt vector (which interrupt will be recieved)
/// @param dest Destination LAPIC ID
void send_ipi_fixed(u8 vector, u32 dest);

/// @brief Sends an IPI with *others* shorthand
/// @param vector Destination interrupt vector (which interrupt will be recieved)
void send_ipi_fixed_others(u8 vector);

#define APIC_DELIVERY_START_UP    0b110
#define APIC_DELIVERY_INIT        0b100

extern "C" void lvt0_int_routine();
extern "C" void lvt1_int_routine();
