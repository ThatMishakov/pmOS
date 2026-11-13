#include "gicv3.hh"
#include <utils.hh>
#include <uacpi/tables.h>
#include <uacpi/acpi.h>
#include <kern_logger/kern_logger.hh>
#include <memory/vmm.hh>
#include <memory/paging.hh>
#include <pmos/utility/scope_guard.hh>
#include <pmos/io.h>
#include <pmos/containers/vector.hh>
#include <arm_acle.h>
#include <algorithm>
#include <sched/sched.hh>

using namespace kernel;
using namespace kernel::arm::interrupts;

namespace {


constexpr size_t GICD_MEMORY_SIZE = 0x10000; 
u64 gicd_phys_base = 0;
u32 gicd_id = 0;
u32 *gicd_virt_base = nullptr;

u32 gicd_typer = 0;

constexpr u32 GICD_CTLR_EnableGrp0 = 1 << 0;
constexpr u32 GICD_CTLR_EnableGrp1 = 1 << 1;
// constexpr u32 GICD_CTRL_EnableGrp1S = 1 << 2;
constexpr u32 GICD_CTLR_ARE = 1 << 4;
// constexpr u32 GICD_CTLR_ARE_NS = 1 << 5;
constexpr u32 GICD_CTLR_DS = 1 << 6;
constexpr u32 GICD_CTLR_RWP = 1u << 31;

constexpr size_t GICD_CTLR_IDX = 0x0000 / sizeof(u32);
constexpr size_t GICD_TYPER_IDX = 0x0004 / sizeof(u32);

constexpr size_t GICD_ISENABLER0_IDX = 0x0100 / sizeof(u32);
constexpr size_t GICD_ICENABLER0_IDX = 0x0180 / sizeof(u32);


constexpr size_t GICR_MEMORY_SIZE = 0x20000;
struct GICR {
    u64 phys_base = 0;
    u32 *virt_base = nullptr;

    u32 affinity = 0;
};
pmos::containers::vector<GICR> gic_redistributors;

constexpr u32 GICR_CTLR_UWP = 1u << 31;

constexpr u32 GICR_TYPER_VLPIS = 1 << 1;
constexpr u32 GICR_TYPER_LAST = 1 << 4;

constexpr size_t GICR_CTLR_IDX = 0x0000 / sizeof(u32);
constexpr size_t GICR_TYPER_IDX = 0x0008 / sizeof(u32);

constexpr size_t GICR_ISENABLER0_IDX = 0x10100 / sizeof(u32);
constexpr size_t GICR_ICENABLER0_IDX = 0x10180 / sizeof(u32);
constexpr size_t GICR_ISPENDR0_IDX = 0x10200 / sizeof(u32);
constexpr size_t GICR_ICPENDR0_IDX = 0x10280 / sizeof(u32);

unsigned itlines_number_normal()
{
    return (gicd_typer & 0x1f) + 1;
}

u32 *gic_map(u64 phys_addr, size_t size)
{
    assert(size >= PAGE_SIZE);

    auto addr = vmm::kernel_space_allocator.virtmem_alloc(size / PAGE_SIZE);
    if (!addr)
        panic("Failed to allocate virtual memory window for GICD, phys_addr %lx size %x", phys_addr, size);

    const paging::Page_Table_Arguments arg = {
        .readable           = true,
        .writeable          = true,
        .user_access        = false,
        .global             = true,
        .execution_disabled = true,
        .extra              = PAGING_FLAG_NOFREE,
        .cache_policy       = paging::Memory_Type::IOPosted
    };

    auto result = map_kernel_pages(phys_addr, addr, size, arg);
    if (result)
        panic("Failed to map GIC into kernel");

    return static_cast<u32 *>(addr);
}

acpi_entry_hdr *get_madt_entry(acpi_madt *madt, u8 type, int idx)
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

void enumerate_redistributors(u64 phys_base, size_t size)
{
    log::serial_logger.printf("[Kernel] Info: Enumerating GICR at physical address 0x%lx with size %x\n", phys_base, size);

    while (size > 0) {
        GICR r;
        r.phys_base = phys_base;
        r.virt_base = gic_map(r.phys_base, GICR_MEMORY_SIZE);
     
        u32 typer_low = mmio_readl(r.virt_base + GICR_TYPER_IDX);
        r.affinity = mmio_readl(r.virt_base + GICR_TYPER_IDX + 1);

        gic_redistributors.push_back(r);

        log::serial_logger.printf("[Kernel] Info: GICR found at physical address 0x%lx with CPU PIDR 0x%x typer 0x%x\n", r.phys_base, r.affinity, typer_low);

        size_t rsize = typer_low & GICR_TYPER_VLPIS ? 0x40000 : 0x20000;
        if (size > rsize) {
            size -= rsize;
            phys_base += rsize;
        } else {
            break;
        }

        if (typer_low & GICR_TYPER_LAST)
            break;
    }
}

void gicd_wait_rwp()
{
    while (mmio_readl(gicd_virt_base + GICD_CTLR_IDX) & GICD_CTLR_RWP)
        __yield();
}

void gicr_wait_uwp(u32 *virt_base)
{
    while (mmio_readl(virt_base + GICR_CTLR_IDX) & GICR_CTLR_UWP)
        __yield();
}

void gicd_do_init()
{
    gicd_typer = mmio_readl(gicd_virt_base + GICD_TYPER_IDX);
    log::serial_logger.printf("[Kernel] Info: GICD found at physical address 0x%lx with ID %u typer 0x%x\n", gicd_phys_base, gicd_id, gicd_typer);

    // // Disable SPIs
    // auto itlines = itlines_number_normal();
    // for (unsigned i = 1; i < itlines; ++i) {
    //     mmio_writel(gicd_virt_base + GICD_ICENABLER0_IDX + i, 0xffffffff);
    // }
}

void do_setup()
{
    auto gicd_ctrl = mmio_readl(gicd_virt_base + GICD_CTLR_IDX);
    if (!(gicd_ctrl & GICD_CTLR_ARE)) {
        if (gicd_ctrl & GICD_CTLR_DS)
            gicd_ctrl &= ~GICD_CTLR_EnableGrp0;
        gicd_ctrl &= ~GICD_CTLR_EnableGrp1;

        mmio_writel(gicd_virt_base + GICD_CTLR_IDX, gicd_ctrl);
        gicd_wait_rwp();

        mmio_writel(gicd_virt_base + GICD_CTLR_IDX, gicd_ctrl | GICD_CTLR_ARE);
        gicd_wait_rwp();

        gicd_ctrl = mmio_readl(gicd_virt_base + GICD_CTLR_IDX);
        if (!(gicd_ctrl & GICD_CTLR_ARE)) {
            panic("GICD ARE not set!");
        }
    }

    for (auto &r: gic_redistributors) {
        // Clear interrupts...

        const u32 typer_low = mmio_readl(r.virt_base + GICR_TYPER_IDX);
        const unsigned extra_ppi_banks = (typer_low >> 27) & 0x1fu;
        const unsigned iters = 1 + extra_ppi_banks;
        for (unsigned i = 0; i < iters; ++i) {
            mmio_writel(r.virt_base + GICR_ICENABLER0_IDX + i, 0xffffffff);
        }

        gicr_wait_uwp(r.virt_base);
    }
}

bool init_acpi()
{
    uacpi_table m;
    auto res = uacpi_table_find_by_signature(ACPI_MADT_SIGNATURE, &m);
    if (res != UACPI_STATUS_OK) {
        log::serial_logger.printf("Couldn't get MADT table...\n");
        return false;
    }
    auto guard = pmos::utility::make_scope_guard([&]{
        uacpi_table_unref(&m);
    });

    acpi_madt *madt = (acpi_madt *)m.ptr;

    auto e = (acpi_madt_gicd *)get_madt_entry(madt, ACPI_MADT_ENTRY_TYPE_GICD, 0);
    if (!e) {
        log::serial_logger.printf("Couldn't get GICD entry from MADT table...\n");
        return false;
    }

    assert(e->gic_version == 3 || e->gic_version == 4);
    gicd_phys_base = e->address;
    gicd_id = e->id;

    gicd_virt_base = gic_map(gicd_phys_base, GICD_MEMORY_SIZE);

    gicd_do_init();

    unsigned i = 0;
    while (auto e = (acpi_madt_gicr *)get_madt_entry(madt, ACPI_MADT_ENTRY_TYPE_GICR, i)) {
        enumerate_redistributors(e->address, e->length);
        ++i;
    }

    if (i == 0) {
        panic("No GICR table, enumerating by GICC is not yet implemented");
    }

    std::sort(gic_redistributors.begin(), gic_redistributors.end(), [](const GICR &a, const GICR &b) {
        return a.affinity < b.affinity;
    });

    do_setup();

    return true;
}

GICR &get_gicr_for_cpu(u32 affinity)
{
    auto it = std::lower_bound(gic_redistributors.begin(), gic_redistributors.end(), affinity, [](const GICR &a, u32 pidr) {
        return a.affinity < pidr;
    });
    if (it != gic_redistributors.end() && it->affinity == affinity)
        return *it;

    panic("No GICR found for CPU PIDR 0x%x", affinity);
}

u32 affinity_from_mpidr(u64 mpidr)
{
    return (mpidr & 0xffffff) | (mpidr >> 8) & 0xff000000;
}

} // namespace

void kernel::arm::interrupts::init_gicv3()
{
    if (init_acpi())
        return;

    panic("Failed to initialize GICv3 interrupt controller with ACPI, DTs not implemented");
}

void kernel::arm::interrupts::gicv3_ppi_clear(u32 ppi)
{
    auto &r = get_gicr_for_cpu(affinity_from_mpidr(sched::get_cpu_struct()->mpidr_el1));
    mmio_writel(r.virt_base + GICR_ICPENDR0_IDX, 1u << ppi);
    gicr_wait_uwp(r.virt_base);
}

void kernel::arm::interrupts::gicv3_ppi_enable(u32 ppi)
{
    assert(ppi >= 16 && ppi <= 31);
    auto &r = get_gicr_for_cpu(affinity_from_mpidr(sched::get_cpu_struct()->mpidr_el1));
    mmio_writel(r.virt_base + GICR_ISENABLER0_IDX, 1u << ppi);
}