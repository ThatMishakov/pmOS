#pragma once
#include <types.hh>
#include <lib/vector.hh>

/// Initialize PLIC during the system boot
void init_plic();
struct CPU_Info;

struct PLIC {
    volatile u32 *virt_base = nullptr;
    u64 hardware_id         = 0;
    u32 gsi_base            = 0;

    u16 max_priority               = 0;
    u16 external_interrupt_sources = 0;
    u8 plic_id                     = 0;

    klib::vector<CPU_Info *> claimed_by_cpu;
};

// Read PLIC register
u32 plic_read(const PLIC &plic, u32 offset);

// Write PLIC register
void plic_write(const PLIC &plic, u32 offset, u32 value);

// Limit of the interrupt sources
u32 plic_interrupt_limit();

// Enable interrupt for the current hart
void plic_interrupt_enable(u32 interrupt_id);
void plic_interrupt_disable(u32 interrupt_id);

// Sets the priority threshold for the current hart
void plic_set_threshold(u32 threshold);

// Sets the interrupt priority
void plic_set_priority(u32 interrupt_id, u32 priority);

// Claim the interrupt
u32 plic_claim();

// Complete the interrupt
void plic_complete(u32 interrupt_id);

PLIC *get_plic(u32 gsi);

constexpr int PLIC_IE_OFFSET         = 0x02000;
constexpr int PLIC_IE_CONTEXT_STRIDE = 0x80;

constexpr int PLIC_THRESHOLD_OFFSET         = 0x200000;
constexpr int PLIC_THRESHOLD_CONTEXT_STRIDE = 0x1000;

constexpr int PLIC_CLAIM_OFFSET            = 0x200004;
constexpr int PLIC_COMPLETE_OFFSET         = 0x200004;
constexpr int PLIC_COMPLETE_CONTEXT_STRIDE = 0x1000;

constexpr int PLIC_PRIORITY_OFFSET        = 0x00000;
constexpr int PLIC_PRIORITY_SOURCE_STRIDE = 0x4;