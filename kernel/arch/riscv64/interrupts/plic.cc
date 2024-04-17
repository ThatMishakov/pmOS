#include <types.hh>
#include "plic.hh"
#include <scheduling.hh>
#include <kern_logger/kern_logger.hh>
#include <memory/virtmem.hh>
#include <memory/paging.hh>
#include <acpi/acpi.h>
#include <sched/sched.hh>

u32 plic_read(const PLIC &plic, u32 offset)
{
    return plic.virt_base[offset >> 2];
}

void plic_write(const PLIC &plic, u32 offset, u32 value)
{
    plic.virt_base[offset >> 2] = value;
}

MADT_PLIC_entry * get_plic_entry(int index = 0)
{
    MADT * m = get_madt();
    if (m == nullptr)
        return nullptr;

    u32 offset = sizeof(MADT);
    u32 length = m->header.length;
    int count = 0;
    while (offset < length) {
        MADT_PLIC_entry * e = (MADT_PLIC_entry *)((char *)m + offset);
        if (e->header.type == MADT_PLIC_ENTRY_TYPE and count++ == index)
            return e;

        offset += e->header.length;
    }

    return nullptr;
}

void * map_plic(u64 base, size_t size)
{
    void * const virt_base = kernel_space_allocator.virtmem_alloc(size >> 12);
    if (virt_base == nullptr) {
        serial_logger.printf("Error: could not allocate memory for PLIC\n");
        return nullptr;
    }

    const Page_Table_Argumments arg = {
        .readable = true,
        .writeable = true,
        .user_access = false,
        .global = true,
        .execution_disabled = true,
        .extra = 0,
        .cache_policy = Memory_Type::IONoCache
    };

    map_kernel_pages(base, (size_t)virt_base, size, arg);

    return virt_base;
}

// TODO: Support more than 1 PLIC
PLIC system_plic;

void init_plic()
{
    // TODO: Only one PLIC is supported at the moment
    MADT_PLIC_entry *e = get_plic_entry();
    if (e == nullptr) {
        serial_logger.printf("Error: could not initialize PLIC. MADT PLIC entry in MADT table not found\n");
        return;
    }

    const auto base = e->plic_address;
    const auto size = e->plic_size;
    const auto virt_base = map_plic(base, size);

    system_plic.virt_base = reinterpret_cast<volatile u32 *>(virt_base);
    system_plic.hardware_id = e->hardware_id;
    system_plic.gsi_base = e->global_sys_int_vec_base;
    system_plic.external_interrupt_sources = e->total_ext_int_sources_supported;
    system_plic.plic_id = e->plic_id;
    system_plic.max_priority = e->max_priority;

    // Print debug information
    serial_logger.printf("PLIC: base: 0x%lx, size: 0x%lx, virt_base: 0x%lx, hardware_id: %d, gsi_base: %d, external_interrupt_sources: %d, plic_id: %d, max_priority: %d\n",
                         base, size, (u64)virt_base, system_plic.hardware_id, system_plic.gsi_base, system_plic.external_interrupt_sources, system_plic.plic_id, system_plic.max_priority);
}

void plic_interrupt_enable(u32 interrupt_id)
{
    // Set priority 1 (will do for now)
    plic_set_priority(interrupt_id, 1);

    const auto c = get_cpu_struct();
    const u16 context_id = c->eic_id & 0xffff;

    const u32 offset = PLIC_IE_OFFSET + (context_id * PLIC_IE_CONTEXT_STRIDE) + (interrupt_id / 32) * 4;
    const u32 shift = interrupt_id % 32;
    const u32 mask = 1 << shift;

    const u32 reg = plic_read(system_plic, offset);
    plic_write(system_plic, offset, reg | mask);
}

u32 plic_interrupt_limit()
{
    return system_plic.external_interrupt_sources;
}

void plic_set_threshold(u32 threshold)
{
    const auto c = get_cpu_struct();
    const u16 context_id = c->eic_id & 0xffff;

    const u32 offset = PLIC_THRESHOLD_OFFSET + (context_id * PLIC_THRESHOLD_CONTEXT_STRIDE);
    plic_write(system_plic, offset, threshold);
}

void plic_set_priority(u32 interrupt_id, u32 priority)
{
    const u32 offset = PLIC_PRIORITY_OFFSET + interrupt_id * PLIC_PRIORITY_SOURCE_STRIDE;
    plic_write(system_plic, offset, priority);
}