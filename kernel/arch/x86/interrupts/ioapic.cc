#include "ioapic.hh"

#include "apic.hh"

#include <uacpi/acpi.h>
#include <uacpi/tables.h>
#include <interrupts/interrupt_handler.hh>
#include <kern_logger/kern_logger.hh>
#include <memory/paging.hh>
#include <memory/vmm.hh>
#include <pmos/io.h>
#include <types.hh>
#include <utils.hh>

using namespace kernel::x86;
using namespace kernel;
using namespace kernel::x86::interrupts;
using namespace kernel::log;

u32 IOAPIC::read_reg(unsigned offset)
{
    Auto_Lock_Scope l(io_lock);
    mmio_writel(virt_addr, offset);
    return mmio_readl(virt_addr + 4);
}

void IOAPIC::write_reg(unsigned offset, u32 value)
{
    Auto_Lock_Scope l(io_lock);
    mmio_writel(virt_addr, offset);
    mmio_writel(virt_addr + 4, value);
}

static u32 *map_ioapic(u32 phys_addr)
{
    constexpr u32 PAGE_MASK = PAGE_SIZE - 1;

    size_t size     = 0x20;
    unsigned offset = phys_addr & PAGE_MASK;
    size            = (size + offset + PAGE_MASK) & ~PAGE_MASK;

    void *ptr = vmm::kernel_space_allocator.virtmem_alloc(size / PAGE_SIZE);
    if (!ptr)
        panic("Could not map ioapic\n");

    const paging::Page_Table_Arguments arg = {.readable           = true,
                                              .writeable          = true,
                                              .user_access        = false,
                                              .global             = true,
                                              .execution_disabled = true,
                                              .extra              = PAGING_FLAG_NOFREE,
                                              .cache_policy       = paging::Memory_Type::IONoCache};

    auto result = map_kernel_pages(phys_addr & ~PAGE_MASK, ptr, size, arg);
    if (result)
        panic("Failed to map IOAPIC into kernel");

    return (u32 *)((char *)ptr + offset);
}

static klib::vector<IOAPIC *> ioapics;

void IOAPIC::push_global()
{
    if (!ioapics.push_back(nullptr))
        panic("Could not allocate memory for ioapics array\n");

    auto gsi_base = this->int_base;

    size_t i = ioapics.size() - 1;
    for (; i > 0; --i) {
        if (ioapics[i - 1]->int_base < gsi_base) {
            ioapics[i] = ioapics[i - 1];
        } else {
            break;
        }
    }

    ioapics[i] = this;
}

void IOAPIC::write_redir_entry(unsigned idx, u64 value)
{
    unsigned addr = IOREDTBL0 + idx * 2;
    Auto_Lock_Scope l(io_lock);

    mmio_writel(virt_addr, addr + 1);
    mmio_writel(virt_addr + 4, value >> 32);
    mmio_writel(virt_addr, addr);
    mmio_writel(virt_addr + 4, value & 0xffffffff);
}

void IOAPIC::init_ioapics()
{
    uacpi_table m;
    auto res = uacpi_table_find_by_signature(ACPI_MADT_SIGNATURE, &m);
    if (res != UACPI_STATUS_OK) {
        log::serial_logger.printf("Couldn't get MADT table...\n");
        return;
    }

    acpi_madt *madt = (acpi_madt *)m.ptr;

    size_t offset = sizeof(acpi_madt);
    while (offset < madt->hdr.length) {
        acpi_entry_hdr *ee = (acpi_entry_hdr *)((char *)madt + offset);
        offset += ee->length;
        if (ee->type != ACPI_MADT_ENTRY_TYPE_IOAPIC)
            continue;

        acpi_madt_ioapic *e = (acpi_madt_ioapic *)ee;

        auto ioapic = new IOAPIC();
        if (!ioapic)
            panic("Could not allocate memory for IOAPIC");

        serial_logger.printf("Found IOAPIC GSI %x at %lx\n", e->gsi_base, e->address);

        ioapic->phys_addr = e->address;
        ioapic->int_base  = e->gsi_base;
        ioapic->virt_addr = map_ioapic(ioapic->phys_addr);
        if (!ioapic->virt_addr)
            panic("Failed to map IOAPIC\n");

        // A bit of a sanity check...
        u8 ioapic_id      = ioapic->read_reg(regs::IOAPICID) >> 24;
        ioapic->ioapic_id = ioapic_id;
        if (ioapic_id != e->id)
            serial_logger.printf("IOAPIC ID mismatch, IOAPICID: %x, ACPI: %x\n", ioapic_id,
                                 e->id);

        u32 ioapicver    = ioapic->read_reg(regs::IOAPICVER);
        u8 version       = ioapicver & 0xff;
        u8 redir_entries = (ioapicver >> 16) & 0xff;

        ioapic->int_count = redir_entries + 1;
        if (!ioapic->mappings.resize(ioapic->int_count, {}))
            panic("Failed to allocate memory for IOAPIC redirection entries vector");

        // Mask all interrupts
        for (u32 i = 0; i < ioapic->int_count; ++i) {
            ioapic->write_redir_entry(i, 1 << 16);
        }

        ioapic->push_global();

        serial_logger.printf("Kernel: initialized IOAPIC ID %x version %x GSI %i - %i\n", ioapic_id,
                             version, ioapic->int_base, ioapic->int_base + redir_entries);
    }

    uacpi_table_unref(&m);
}

Spinlock int_allocation_lock;

ReturnStr<std::pair<sched::CPU_Info *, u32>>
    IOAPIC::allocate_interrupt_single(u32 gsi, bool edge_triggered, bool active_low)
{
    auto ioapic = IOAPIC::get_ioapic(gsi);
    if (!ioapic)
        return Error(-ENOENT);
    auto apic_base = gsi - ioapic->int_base;

    Auto_Lock_Scope l(int_allocation_lock);

    auto [cpu, vector] = ioapic->mappings[apic_base];
    if (cpu)
        return Success(std::make_pair(cpu, vector));

    auto result = lapic::allocate_interrupt({ioapic, apic_base});
    if (result.success()) {
        auto [cpu, vector] = result.val;

        ioapic->mappings[apic_base] = {cpu, vector};

        // TODO!
        u32 lapic_id = lapic::apic_mode == lapic::APICMode::XAPIC ? cpu->lapic_id : cpu->lapic_id << 24;

        // Set the mapping
        u64 val = ((u64)lapic_id << 32) | (1 << 16) | ((u32)!edge_triggered << 15) |
                  ((u32)active_low << 13) | vector;
        ioapic->write_redir_entry(apic_base, val);

        log::serial_logger.printf("IOAPIC added entry for lapic %x\n", cpu->lapic_id);
    }

    return result;
}

IOAPIC *IOAPIC::get_ioapic(u32 gsi)
{
    for (auto ioapic: ioapics) {
        if (ioapic->int_base <= gsi and ioapic->int_base + ioapic->int_count > gsi)
            return ioapic;
    }

    return nullptr;
}

ReturnStr<std::pair<sched::CPU_Info *, u32>>
    kernel::interrupts::allocate_interrupt_single(u32 gsi, bool edge_triggered, bool active_low)
{
    return IOAPIC::allocate_interrupt_single(gsi, edge_triggered, active_low);
}

void IOAPIC::interrupt_enable(u32 vector)
{
    assert(vector < int_count);

    unsigned addr = IOREDTBL0 + vector * 2;
    Auto_Lock_Scope l(io_lock);

    mmio_writel(virt_addr, addr);
    u32 val = mmio_readl(virt_addr + 4);
    val &= ~(1 << 16);
    mmio_writel(virt_addr + 4, val);
}

void IOAPIC::interrupt_disable(u32 vector)
{
    assert(vector < int_count);

    unsigned addr = IOREDTBL0 + vector * 2;
    Auto_Lock_Scope l(io_lock);

    mmio_writel(virt_addr, addr);
    u32 val = mmio_readl(virt_addr + 4);
    val |= 1 << 16;
    mmio_writel(virt_addr + 4, val);
}