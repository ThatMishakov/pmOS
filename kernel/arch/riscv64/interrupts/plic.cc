#include "plic.hh"

#include <acpi/acpi.h>
#include <dtb/dtb.hh>
#include <interrupts/interrupt_handler.hh>
#include <pmos/utility/scope_guard.hh>
#include <kern_logger/kern_logger.hh>
#include <memory/paging.hh>
#include <memory/vmm.hh>
#include <sched/sched.hh>
#include <scheduling.hh>
#include <smoldtb.h>
#include <types.hh>

#include <uacpi/tables.h>
#include <uacpi/acpi.h>

using namespace kernel;
using namespace kernel::log;
dtb_node *dtb_get_plic_node();

static Spinlock lock;

namespace kernel::riscv::interrupts
{

u32 plic_read(const PLIC &plic, u32 offset) { return plic.virt_base[offset >> 2]; }

void plic_write(const PLIC &plic, u32 offset, u32 value) { plic.virt_base[offset >> 2] = value; }


static acpi_entry_hdr *get_madt_entry(acpi_madt *madt, u8 type, int idx)
{
    size_t offset = sizeof(acpi_madt);
    int iidx = 0;
    while (offset < madt->hdr.length) {
        acpi_entry_hdr *ee = (acpi_entry_hdr *)((char *)madt + offset);
        offset += ee->length;
        if (ee->type != type)
            continue;

        if (iidx++ == idx)
            return ee;
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

    const paging::Page_Table_Arguments arg = {.readable           = true,
                                              .writeable          = true,
                                              .user_access        = false,
                                              .global             = true,
                                              .execution_disabled = true,
                                              .extra              = PAGING_FLAG_NOFREE,
                                              .cache_policy       = paging::Memory_Type::IONoCache};

    map_kernel_pages(base, virt_base, size, arg);

    return virt_base;
}

// TODO: Support more than 1 PLIC
PLIC system_plic;

bool acpi_init_plic()
{
    // TODO: Only one PLIC is supported at the moment
    uacpi_table m;
    auto res = uacpi_table_find_by_signature(ACPI_MADT_SIGNATURE, &m);
    if (res != UACPI_STATUS_OK) {
        return false;
    }
    auto guard = pmos::utility::make_scope_guard([&]{
        uacpi_table_unref(&m);
    });

    acpi_madt *madt = (acpi_madt *)m.ptr;  

    acpi_madt_plic *e = (acpi_madt_plic *)get_madt_entry(madt, ACPI_MADT_ENTRY_TYPE_PLIC, 0);

    const auto base      = e->address;
    const auto size      = e->size;
    const auto virt_base = map_plic(base, size);

    system_plic.virt_base                  = reinterpret_cast<volatile u32 *>(virt_base);
    system_plic.hardware_id                = e->hardware_id;
    system_plic.gsi_base                   = e->gsi_base;
    system_plic.external_interrupt_sources = e->sources_count;
    system_plic.plic_id                    = e->id;
    system_plic.max_priority               = e->max_priority;
    if (!system_plic.handlers.resize(e->sources_count, nullptr))
        panic("failed to allocate memory for PLIC\n");

    return true;
}

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

void plic_interrupt_enable(const PLIC &plic, u32 interrupt_id)
{
    // Set priority 1 (will do for now)
    plic_set_priority(interrupt_id, 1);

    const auto c         = sched::get_cpu_struct();
    const u16 context_id = c->eic_id & 0xffff;

    const u32 offset =
        PLIC_IE_OFFSET + (context_id * PLIC_IE_CONTEXT_STRIDE) + (interrupt_id / 32) * 4;
    const u32 shift = interrupt_id % 32;
    const u32 mask  = 1 << shift;

    const u32 reg = plic_read(plic, offset);
    plic_write(plic, offset, reg | mask);
}

void plic_interrupt_disable(const PLIC &plic, u32 interrupt_id)
{
    const auto c         = sched::get_cpu_struct();
    const u16 context_id = c->eic_id & 0xffff;

    const u32 offset =
        PLIC_IE_OFFSET + (context_id * PLIC_IE_CONTEXT_STRIDE) + (interrupt_id / 32) * 4;
    const u32 shift = interrupt_id % 32;
    const u32 mask  = 1 << shift;

    const u32 reg = plic_read(plic, offset);
    plic_write(plic, offset, reg & ~mask);
}

u32 plic_interrupt_limit() { return system_plic.external_interrupt_sources; }

void plic_set_threshold(u32 threshold)
{
    const auto c         = sched::get_cpu_struct();
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
    const auto c         = sched::get_cpu_struct();
    const u16 context_id = c->eic_id & 0xffff;

    const u32 offset = PLIC_CLAIM_OFFSET + (context_id * PLIC_COMPLETE_CONTEXT_STRIDE);
    return plic_read(system_plic, offset);
}

void plic_complete(const PLIC &plic, u32 interrupt_id)
{
    const auto c         = sched::get_cpu_struct();
    const u16 context_id = c->eic_id & 0xffff;

    const u32 offset = PLIC_COMPLETE_OFFSET + (context_id * PLIC_COMPLETE_CONTEXT_STRIDE);
    plic_write(plic, offset, interrupt_id);
}

PLIC *get_plic(u32 gsi)
{
    if (system_plic.gsi_base > gsi)
        return nullptr;

    if (system_plic.gsi_base + system_plic.external_interrupt_sources > gsi)
        return &system_plic;

    return nullptr;
}

PLICHandler *get_plic_handler(u32 gsi)
{
    auto plic = get_plic(gsi);
    if (!plic)
        return nullptr;

    assert(gsi >= plic->gsi_base);
    assert(gsi < plic->gsi_base + plic->external_interrupt_sources);
    auto offset = gsi - plic->gsi_base;
    return __atomic_load_n(&plic->handlers[offset], __ATOMIC_RELAXED);
}

} // namespace kernel::riscv::interrupts

ReturnStr<interrupts::InterruptHandler *> interrupts::allocate_or_get_handler(u32 gsi, bool, bool)
{
    auto plic = riscv::interrupts::get_plic(gsi);
    assert(plic);
    if (!plic)
        return Error(-ENOENT);

    Auto_Lock_Scope l(lock);

    assert(gsi >= plic->gsi_base);
    auto offset = gsi - plic->gsi_base;
    auto e      = plic->handlers[offset];
    if (e)
        return Success(e);

    auto handler = klib::make_unique<riscv::interrupts::PLICHandler>();
    if (!handler)
        return Error(-ENOMEM);

    handler->parent_plic = plic;
    handler->interrupt_id = gsi - plic->gsi_base;

    // Select the least loaded CPU
    auto current_cpu = sched::cpus[0];
    assert(current_cpu);
    for (size_t i = 1; i < sched::cpus.size(); ++i) {
        auto h1      = sched::cpus[i]->allocated_int_count;
        auto current = current_cpu->allocated_int_count;
        if (h1 < current)
            current_cpu = sched::cpus[i];
    }

    current_cpu->allocated_int_count++;
    handler->parent_cpu = current_cpu;

    // Do relaxed atomic store here so that there is no room for the compiler to do
    // since the interrupt handler doesn't take locks when reading this.
    __atomic_store_n(&plic->handlers[offset], handler.get(), __ATOMIC_RELAXED);
    return Success(handler.release());
}

void kernel::interrupts::interrupt_disable(InterruptHandler *handler)
{
    assert(handler);
    auto plic_handler = static_cast<riscv::interrupts::PLICHandler *>(handler);

    assert(plic_handler->parent_plic);
    riscv::interrupts::plic_interrupt_disable(*plic_handler->parent_plic, plic_handler->interrupt_id);
}

void kernel::interrupts::interrupt_complete(InterruptHandler *handler)
{
    assert(handler);
    auto plic_handler = static_cast<riscv::interrupts::PLICHandler *>(handler);

    assert(plic_handler->parent_plic);
    riscv::interrupts::plic_complete(*plic_handler->parent_plic, plic_handler->interrupt_id);
}

void kernel::interrupts::interrupt_enable(InterruptHandler *handler)
{
    assert(handler);
    auto plic_handler = static_cast<riscv::interrupts::PLICHandler *>(handler);

    assert(plic_handler->parent_plic);
    riscv::interrupts::plic_interrupt_enable(*plic_handler->parent_plic, plic_handler->interrupt_id);
}