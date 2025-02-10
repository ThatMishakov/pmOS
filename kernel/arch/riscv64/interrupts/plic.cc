#include "plic.hh"

#include <acpi/acpi.h>
#include <dtb/dtb.hh>
#include <kern_logger/kern_logger.hh>
#include <memory/paging.hh>
#include <memory/vmm.hh>
#include <sched/sched.hh>
#include <scheduling.hh>
#include <smoldtb.h>
#include <types.hh>

using namespace kernel;

u32 plic_read(const PLIC &plic, u32 offset) { return plic.virt_base[offset >> 2]; }

void plic_write(const PLIC &plic, u32 offset, u32 value) { plic.virt_base[offset >> 2] = value; }

MADT_PLIC_entry *get_plic_entry(int index = 0)
{
    MADT *m = get_madt();
    if (m == nullptr)
        return nullptr;

    u32 offset = sizeof(MADT);
    u32 length = m->header.length;
    int count  = 0;
    while (offset < length) {
        MADT_PLIC_entry *e = (MADT_PLIC_entry *)((char *)m + offset);
        if (e->header.type == MADT_PLIC_ENTRY_TYPE and count++ == index)
            return e;

        offset += e->header.length;
    }

    return nullptr;
}

void *map_plic(u64 base, size_t size)
{
    void *const virt_base = vmm::kernel_space_allocator.virtmem_alloc(size >> 12);
    if (virt_base == nullptr) {
        serial_logger.printf("Error: could not allocate memory for PLIC\n");
        return nullptr;
    }

    const Page_Table_Argumments arg = {.readable           = true,
                                       .writeable          = true,
                                       .user_access        = false,
                                       .global             = true,
                                       .execution_disabled = true,
                                       .extra              = 0,
                                       .cache_policy       = Memory_Type::IONoCache};

    map_kernel_pages(base, virt_base, size, arg);

    return virt_base;
}

// TODO: Support more than 1 PLIC
PLIC system_plic;

bool acpi_init_plic()
{
    // TODO: Only one PLIC is supported at the moment

    MADT_PLIC_entry *e = get_plic_entry();
    if (e == nullptr) {
        return false;
    }

    const auto base      = e->plic_address;
    const auto size      = e->plic_size;
    const auto virt_base = map_plic(base, size);

    system_plic.virt_base                  = reinterpret_cast<volatile u32 *>(virt_base);
    system_plic.hardware_id                = e->hardware_id;
    system_plic.gsi_base                   = e->global_sys_int_vec_base;
    system_plic.external_interrupt_sources = e->total_ext_int_sources_supported;
    system_plic.plic_id                    = e->plic_id;
    system_plic.max_priority               = e->max_priority;

    return true;
}

dtb_node *dtb_get_plic_node();

bool dtb_init_plic()
{
    if (not have_dtb())
        return false;

    auto plic = dtb_get_plic_node();
    if (plic == nullptr)
        return false;

    size_t address_cells, size_cells;
    auto parent = dtb_get_parent(plic);

    auto prop = dtb_find_prop(parent, "#address-cells");
    if (prop == nullptr) {
        serial_logger.printf("Could not find prop \"#address-cells\" in DTB plic node\n");
        return false;
    }
    dtb_read_prop_values(prop, 1, &address_cells);

    prop = dtb_find_prop(parent, "#size-cells");
    if (prop == nullptr) {
        serial_logger.printf("Could not find prop \"#size-cells\" in DTB plic node\n");
        return false;
    }
    dtb_read_prop_values(prop, 1, &size_cells);

    prop = dtb_find_prop(plic, "reg");
    if (not prop) {
        serial_logger.printf("Could not find prop \"reg\" in DTB plic node\n");
        return false;
    }
    dtb_pair reg;
    dtb_read_prop_pairs(prop, {address_cells, size_cells}, &reg);

    prop = dtb_find_prop(plic, "riscv,ndev");
    if (not prop) {
        serial_logger.printf("Could not find prop \"riscv,ndev\" in DTB plic node\n");
        return false;
    }
    size_t ndev = 0;
    dtb_read_prop_values(prop, 1, &ndev);

    const u64 base       = reg.a;
    const u64 size       = reg.b;
    const auto virt_base = map_plic(base, size);
    serial_logger.printf("Mapping PLIC at %x size %x -> %x\n", base, size, virt_base);

    system_plic.virt_base   = reinterpret_cast<volatile u32 *>(virt_base);
    system_plic.hardware_id = 0;
    system_plic.gsi_base    = 0; // TODO: Don't know if it's 0 or it's has to be read from DTB
    system_plic.external_interrupt_sources = ndev;
    system_plic.plic_id                    = 0;
    system_plic.max_priority               = -1; // TODO: Have to take a look at it again

    return true;
}

void init_plic()
{
    bool initialized_plic = false;
    do {
        initialized_plic = acpi_init_plic();
        if (initialized_plic)
            continue;

        initialized_plic = dtb_init_plic();
    } while (0);

    if (not initialized_plic) {
        serial_logger.printf("Failed to initialize PLIC\n");
        return;
    }

    // Print debug information
    serial_logger.printf("PLIC: base: 0x%lx, size: 0x%lx, virt_base: 0x%lx, hardware_id: %d, "
                         "gsi_base: %d, external_interrupt_sources: %d, plic_id: %d, "
                         "max_priority: %d\n",
                         0, 0, (u64)system_plic.virt_base, system_plic.hardware_id,
                         system_plic.gsi_base, system_plic.external_interrupt_sources,
                         system_plic.plic_id, system_plic.max_priority);
}

void interrupt_enable(u32 interrupt_id) { plic_interrupt_enable(interrupt_id); }

void plic_interrupt_enable(u32 interrupt_id)
{
    // Set priority 1 (will do for now)
    plic_set_priority(interrupt_id, 1);

    const auto c         = get_cpu_struct();
    const u16 context_id = c->eic_id & 0xffff;

    const u32 offset =
        PLIC_IE_OFFSET + (context_id * PLIC_IE_CONTEXT_STRIDE) + (interrupt_id / 32) * 4;
    const u32 shift = interrupt_id % 32;
    const u32 mask  = 1 << shift;

    const u32 reg = plic_read(system_plic, offset);
    plic_write(system_plic, offset, reg | mask);
}

void interrupt_disable(u32 interrupt_id) { plic_interrupt_disable(interrupt_id); }

void plic_interrupt_disable(u32 interrupt_id)
{
    const auto c         = get_cpu_struct();
    const u16 context_id = c->eic_id & 0xffff;

    const u32 offset =
        PLIC_IE_OFFSET + (context_id * PLIC_IE_CONTEXT_STRIDE) + (interrupt_id / 32) * 4;
    const u32 shift = interrupt_id % 32;
    const u32 mask  = 1 << shift;

    const u32 reg = plic_read(system_plic, offset);
    plic_write(system_plic, offset, reg & ~mask);
}

u32 plic_interrupt_limit() { return system_plic.external_interrupt_sources; }

u32 interrupt_min() { return 0; }
u32 interrupt_max() { return plic_interrupt_limit() - 1; }

void plic_set_threshold(u32 threshold)
{
    const auto c         = get_cpu_struct();
    const u16 context_id = c->eic_id & 0xffff;

    if (system_plic.virt_base == nullptr)
        return;

    const u32 offset = PLIC_THRESHOLD_OFFSET + (context_id * PLIC_THRESHOLD_CONTEXT_STRIDE);
    plic_write(system_plic, offset, threshold);
}

void plic_set_priority(u32 interrupt_id, u32 priority)
{
    const u32 offset = PLIC_PRIORITY_OFFSET + interrupt_id * PLIC_PRIORITY_SOURCE_STRIDE;
    plic_write(system_plic, offset, priority);
}

u32 plic_claim()
{
    const auto c         = get_cpu_struct();
    const u16 context_id = c->eic_id & 0xffff;

    const u32 offset = PLIC_CLAIM_OFFSET + (context_id * PLIC_COMPLETE_CONTEXT_STRIDE);
    return plic_read(system_plic, offset);
}

void interrupt_complete(u32 interrupt_id) { plic_complete(interrupt_id); }

void plic_complete(u32 interrupt_id)
{
    const auto c         = get_cpu_struct();
    const u16 context_id = c->eic_id & 0xffff;

    const u32 offset = PLIC_COMPLETE_OFFSET + (context_id * PLIC_COMPLETE_CONTEXT_STRIDE);
    plic_write(system_plic, offset, interrupt_id);
}
