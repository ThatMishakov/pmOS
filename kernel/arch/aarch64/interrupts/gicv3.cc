#include "gicv3.hh"
#include "interrupts.hh"
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
#include <arm_asm.hh>

using namespace kernel;
using namespace kernel::arm::interrupts;

DEFINE_SYSREG(icc_sre_el1)
DEFINE_SYSREG(icc_pmr_el1)
DEFINE_SYSREG(icc_bpr0_el1)
DEFINE_SYSREG(icc_ctlr_el1)
DEFINE_SYSREG(icc_igrpen1_el1)
DEFINE_SYSREG(icc_iar0_el1)
DEFINE_SYSREG(icc_iar1_el1)
DEFINE_SYSREG(icc_eoir0_el1)
DEFINE_SYSREG(icc_eoir1_el1)

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

constexpr size_t GICD_IGROUPR0_IDX = 0x0080 / sizeof(u32);

constexpr size_t GICD_ISENABLER0_IDX = 0x0100 / sizeof(u32);
constexpr size_t GICD_ICENABLER0_IDX = 0x0180 / sizeof(u32);

constexpr size_t GICD_IPRIORITYR0_IDX = 0x0400 / sizeof(u32);

constexpr size_t GICD_ICFGR0_IDX = 0x0C00 / sizeof(u32);

constexpr size_t GICD_IROUTER0_IDX = 0x6000 / sizeof(u32);


constexpr size_t GICR_MEMORY_SIZE = 0x20000;
struct GICR {
    u64 phys_base = 0;
    u32 *virt_base = nullptr;

    u32 affinity = 0;
};
pmos::containers::vector<GICR> gic_redistributors;

constexpr u32 GICR_CTLR_UWP = 1u << 31;

constexpr u32 GICR_WAKER_PS = 1 << 1;
constexpr u32 GICR_WAKER_CA = 1 << 2;

constexpr u32 GICR_TYPER_VLPIS = 1 << 1;
constexpr u32 GICR_TYPER_LAST = 1 << 4;

constexpr size_t GICR_CTLR_IDX = 0x0000 / sizeof(u32);
constexpr size_t GICR_TYPER_IDX = 0x0008 / sizeof(u32);
constexpr size_t GICR_WAKER_IDX = 0x0014 / sizeof(u32);

constexpr size_t GICR_IGROUPR0_IDX = 0x10080 / sizeof(u32);

constexpr size_t GICR_ISENABLER0_IDX = 0x10100 / sizeof(u32);
constexpr size_t GICR_ICENABLER0_IDX = 0x10180 / sizeof(u32);
constexpr size_t GICR_ISPENDR0_IDX = 0x10200 / sizeof(u32);
constexpr size_t GICR_ICPENDR0_IDX = 0x10280 / sizeof(u32);

u64 gicd_irouter_from(u64 pidr)
{
    return pidr & 0xff00ffffff;
}

struct GICHandler: kernel::interrupts::InterruptHandler {
    bool is_allocated() const
    {
        return parent_cpu != nullptr;
    }

    bool edge_triggered = false;
};

pmos::containers::vector<GICHandler> spi_handlers;

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

    auto itlines = itlines_number_normal();
    for (unsigned i = 1; i < itlines; ++i) {
        mmio_writel(gicd_virt_base + GICD_ICENABLER0_IDX + i, 0xffffffff);
    }
    gicd_wait_rwp();

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

    if (!(gicd_ctrl & GICD_CTLR_EnableGrp1)) {
        gicd_ctrl |= GICD_CTLR_EnableGrp1;
        mmio_writel(gicd_virt_base + GICD_CTLR_IDX, gicd_ctrl);
        gicd_wait_rwp();
    }

    auto spi_lines = itlines_number_normal() - 1;
    spi_handlers.resize(spi_lines * 32);
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

GICR get_gicr_for_cpu(u32 affinity)
{
    auto it = std::lower_bound(gic_redistributors.begin(), gic_redistributors.end(), affinity, [](const GICR &a, u32 pidr) {
        return a.affinity < pidr;
    });
    if (it != gic_redistributors.end() && it->affinity == affinity)
        return *it;

    panic("No GICR found for CPU PIDR 0x%x", affinity);

    return {};
}

u32 affinity_from_mpidr(u64 mpidr)
{
    return (mpidr & 0xffffff) | ((mpidr >> 8) & 0xff000000);
}

void disable_spi(u32 spi)
{
    assert(spi >= 32 && spi < 32 + spi_handlers.size());

    u32 idx = spi / 32;
    u32 shift = spi % 32;
    u32 mask = 1u << shift;

    mmio_writel(gicd_virt_base + GICD_ICENABLER0_IDX + idx, mask);
    gicd_wait_rwp();
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
    auto r = get_gicr_for_cpu(affinity_from_mpidr(sched::get_cpu_struct()->mpidr_el1));
    mmio_writel(r.virt_base + GICR_ICPENDR0_IDX, 1u << ppi);
    gicr_wait_uwp(r.virt_base);
}

void kernel::arm::interrupts::gicv3_ppi_enable(u32 ppi)
{
    assert(ppi >= 16 && ppi <= 31);
    auto r = get_gicr_for_cpu(affinity_from_mpidr(sched::get_cpu_struct()->mpidr_el1));

    // Set group 1 for the PPI
    u32 group_idx = 0;
    u32 group_shift = ppi;
    auto group_reg = mmio_readl(r.virt_base + GICR_IGROUPR0_IDX + group_idx);
    group_reg |= (1u << group_shift);
    mmio_writel(r.virt_base + GICR_IGROUPR0_IDX + group_idx, group_reg);
    

    mmio_writel(r.virt_base + GICR_ISENABLER0_IDX, 1u << ppi);
}

void kernel::arm::interrupts::gicv3_ppi_disable(u32 ppi)
{
    assert(ppi >= 16 && ppi <= 31);
    auto r = get_gicr_for_cpu(affinity_from_mpidr(sched::get_cpu_struct()->mpidr_el1));

    mmio_writel(r.virt_base + GICR_ICENABLER0_IDX, 1u << ppi);
    gicr_wait_uwp(r.virt_base);
}

void kernel::arm::interrupts::gicv3_do_cpu_interrupts_init()
{
    auto r = get_gicr_for_cpu(affinity_from_mpidr(sched::get_cpu_struct()->mpidr_el1));
    
    auto waker = mmio_readl(r.virt_base + GICR_WAKER_IDX);
    if (waker & GICR_WAKER_PS) {
        waker &= ~GICR_WAKER_PS;
        mmio_writel(r.virt_base + GICR_WAKER_IDX, waker);
        
        while (mmio_readl(r.virt_base + GICR_WAKER_IDX) & GICR_WAKER_CA)
            __yield();
    }

    auto sre = mrs<icc_sre_el1>();
    if (!(sre & 0x1)) {
        sre |= 0x1;
        msr<icc_sre_el1>(sre);
    }
    isb();

    msr<icc_igrpen1_el1>(0);
    isb();

    auto ctlr = mrs<icc_ctlr_el1>();
    ctlr |= 1 << 0; // CBPR
    ctlr &= ~((ulong)1 << 1); // EOImode
    msr<icc_ctlr_el1>(ctlr);

    msr<icc_bpr0_el1>(7);
    msr<icc_pmr_el1>(0xff);
    isb();

    msr<icc_igrpen1_el1>(1);
}

static Spinlock lock;

ReturnStr<kernel::interrupts::InterruptHandler *> kernel::arm::interrupts::gicv3_get_handler(u32 gsi, bool edge_triggered, bool)
{
    if (gsi < 32)
        return Error(-ENOTSUP);

    auto interrupt = gsi - 32;
    if (interrupt >= spi_handlers.size())
        return Error(-ENODEV);

    Auto_Lock_Scope l(lock);

    auto &handler = spi_handlers[interrupt];
    if (handler.is_allocated())
        return &handler;

    auto current_cpu = sched::cpus[0];
    assert(current_cpu);
    for (size_t i = 1; i < sched::cpus.size(); ++i) {
        auto h1      = sched::cpus[i]->allocated_int_count;
        auto current = current_cpu->allocated_int_count;
        if (h1 < current)
            current_cpu = sched::cpus[i];
    }

    current_cpu->allocated_int_count++;
    handler.parent_cpu = current_cpu;
    handler.edge_triggered = edge_triggered;

    u64 irouter = gicd_irouter_from(current_cpu->mpidr_el1);
    // Route the interrupt
    u32 idx = gsi*2;
    mmio_writel(gicd_virt_base + GICD_IROUTER0_IDX + idx, irouter & 0xffffffff);
    mmio_writel(gicd_virt_base + GICD_IROUTER0_IDX + idx + 1, (irouter >> 32) & 0xffffffff);

    u32 type_idx = gsi / 16;
    u32 type_shift = (gsi % 16) * 2;

    auto type_reg = mmio_readl(gicd_virt_base + GICD_ICFGR0_IDX + type_idx);
    type_reg &= ~(0x3u << type_shift);
    if (edge_triggered)
        type_reg |= (0x2u << type_shift);
    mmio_writel(gicd_virt_base + GICD_ICFGR0_IDX + type_idx, type_reg);

    // Group 1 for everything
    u32 group_idx = gsi / 32;
    u32 group_shift = gsi % 32;
    auto group_reg = mmio_readl(gicd_virt_base + GICD_IGROUPR0_IDX + group_idx);
    group_reg |= (1u << group_shift);
    mmio_writel(gicd_virt_base + GICD_IGROUPR0_IDX + group_idx, group_reg);

    gicd_wait_rwp();

    return Success(&handler);
}

void kernel::arm::interrupts::gicv3_enable(kernel::interrupts::InterruptHandler *handler)
{
    assert(handler);
    auto gic_handler = static_cast<GICHandler *>(handler);
    assert(gic_handler->parent_cpu == sched::get_cpu_struct());

    u32 gsi = (gic_handler - spi_handlers.data()) + 32;
    assert(gsi >= 32 && gsi < 32 + spi_handlers.size());

    u32 idx = gsi / 32;
    u32 shift = gsi % 32;
    u32 mask = 1u << shift;

    mmio_writel(gicd_virt_base + GICD_ISENABLER0_IDX + idx, mask);
    gicd_wait_rwp();
}

void kernel::arm::interrupts::gicv3_disable(kernel::interrupts::InterruptHandler *handler)
{
    assert(handler);
    auto gic_handler = static_cast<GICHandler *>(handler);
    assert(gic_handler->parent_cpu == sched::get_cpu_struct());

    u32 gsi = (gic_handler - spi_handlers.data()) + 32;
    disable_spi(gsi);
}

void kernel::arm::interrupts::gicv3_interrupt_complete(kernel::interrupts::InterruptHandler *handler)
{
    assert(handler);
    auto gic_handler = static_cast<GICHandler *>(handler);
    assert(gic_handler->parent_cpu == sched::get_cpu_struct());

    u32 gsi = (gic_handler - spi_handlers.data()) + 32;
    assert(gsi >= 32 && gsi < 32 + spi_handlers.size());

    msr<icc_eoir1_el1>(gsi);
}

void kernel::arm::interrupts::gicv3_handle_fiq()
{
    panic("GICv3 FIQ not implemented");
}

constexpr unsigned SPURIOUS_IRQ = 1023;

void kernel::arm::interrupts::gicv3_handle_irq()
{
    auto iar = mrs<icc_iar1_el1>();
    u32 gsi = iar & 0x3ff;
    
    if (gsi == SPURIOUS_IRQ) {
        // log::serial_logger.printf("[Kernel] Info: Spurious IRQ received\n");
        return;
    }

    if (gsi < 32) {
        arm::interrupts::handle_ppi_spi(gsi);
        msr<icc_eoir1_el1>(iar);
    } else {
        auto interrupt = gsi - 32;
        if (interrupt >= spi_handlers.size())
            panic("GICv3 IRQ %u out of range", gsi);

        auto &handler = spi_handlers[interrupt];
        if (!handler.is_allocated()) {
            // log::serial_logger.printf("[Kernel] Warning: IRQ %u received but no handler allocated\n", gsi);
            disable_spi(gsi);
            msr<icc_eoir1_el1>(iar);
            return;
        }

        if (handler.send_interrupt_notification() != kernel::interrupts::NotificationResult::Success) {
            disable_spi(gsi);
            msr<icc_eoir1_el1>(iar);
        }
    }
}
