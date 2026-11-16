#include "interrupts.hh"
#include <utils.hh>
#include <uacpi/tables.h>
#include <uacpi/acpi.h>
#include "gicv3.hh"
#include <kern_logger/kern_logger.hh>
#include <pmos/utility/scope_guard.hh>
#include <interrupts/interrupt_handler.hh>
#include <sched/sched.hh>

enum class InterruptControllerType {
    GICv3,
    GICv4,
    AIC,
    Unknown
};

using namespace kernel::arm::interrupts;
using namespace kernel;

namespace {

InterruptControllerType controller = InterruptControllerType::Unknown;

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

bool init_interrupts_acpi()
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

    switch (e->gic_version) {
    case 0x01 ... 0x02:
        panic("System has GICv1 or GICv2 which is not yet supported\n");
        break;
    case 0x03 ... 0x04:
        init_gicv3();
        controller = InterruptControllerType::GICv3;
        break;
    default:
        panic("Unsupported GIC version %u\n", e->gic_version);
        break;
    }

    return true;
}

void ipi_send(sched::CPU_Info *cpu, u32 interrupt)
{
    switch (controller) {
    case InterruptControllerType::GICv3:
        gicv3_spi_send(cpu->mpidr_el1, interrupt);
        break;
    default:
        panic("Unknown interrupt controller type %u\n", (u32)controller);
    }
}

}

void kernel::arm::interrupts::init_interrupt_controllers()
{
    bool success = init_interrupts_acpi();
    if (success)
        return;

    // TODO: DTs

    panic("Could not initialize interrupt controllers with ACPI, DTs not implemented\n");
}

void kernel::arm::interrupts::ppi_clear(u32 ppi)
{
    switch (controller) {
    case InterruptControllerType::GICv3:
        gicv3_ppi_clear(ppi);
        break;
    default:
        panic("Unknown interrupt controller type %u\n", (u32)controller);
    }
}

void kernel::arm::interrupts::ppi_enable(u32 ppi)
{
    switch (controller) {
    case InterruptControllerType::GICv3:
        gicv3_ppi_enable(ppi);
        break;
    default:
        panic("Unknown interrupt controller type %u\n", (u32)controller);
    }
}

void kernel::arm::interrupts::ppi_disable(u32 ppi)
{
    switch (controller) {
    case InterruptControllerType::GICv3:
        gicv3_ppi_disable(ppi);
        break;
    default:
        panic("Unknown interrupt controller type %u\n", (u32)controller);
    }
}

void interrupts::interrupt_complete(interrupts::InterruptHandler *handler)
{
    switch (controller) {
    case InterruptControllerType::GICv3:
        gicv3_interrupt_complete(handler);
        break;
    default:
        panic("Unknown interrupt controller type %u\n", (u32)controller);
    }
}

void interrupts::interrupt_disable(interrupts::InterruptHandler *handler)
{
    switch (controller) {
    case InterruptControllerType::GICv3:
        gicv3_disable(handler);
        break;
    default:
        panic("Unknown interrupt controller type %u\n", (u32)controller);
    }
}

void interrupts::interrupt_enable(interrupts::InterruptHandler *handler)
{
    switch (controller) {
    case InterruptControllerType::GICv3:
        gicv3_enable(handler);
        break;
    default:
        panic("Unknown interrupt controller type %u\n", (u32)controller);
    }
}

ReturnStr<interrupts::InterruptHandler *> interrupts::allocate_or_get_handler(u32 gsi, bool edge_triggered, bool active_low)
{
    switch (controller) {
    case InterruptControllerType::GICv3:
        return gicv3_get_handler(gsi, edge_triggered, active_low);
    default:
        panic("Unknown interrupt controller type %u\n", (u32)controller);
    }
}

void arm::interrupts::do_cpu_interrupts_init()
{
    switch (controller) {
    case InterruptControllerType::GICv3:
        gicv3_do_cpu_interrupts_init();
        break;
    default:
        panic("Unknown interrupt controller type %u\n", (u32)controller);
    }
}

extern "C" void handle_user_irq()
{
    switch (controller) {
    case InterruptControllerType::GICv3:
        gicv3_handle_irq();
        break;
    default:
        panic("Unknown interrupt controller type %u\n", (u32)controller);
    }

    sched::handle_scheduling();
}

extern "C" void handle_user_fiq()
{
    switch (controller) {
    case InterruptControllerType::GICv3:
        gicv3_handle_fiq();
        break;
    default:
        panic("Unknown interrupt controller type %u\n", (u32)controller);
    }

    sched::handle_scheduling();
}

std::array<void (*)(), 16> arm::interrupts::ppi_handlers = {};

void arm::interrupts::handle_ppi_spi(unsigned interrupt)
{
    assert(interrupt < 32);

    if (interrupt < 16) {
        panic("PPI %u unhandled!", interrupt);
    }

    interrupt -= 16;
    auto handler = ppi_handlers[interrupt];
    if (!handler) {
        log::serial_logger.printf("[Kernel] Warning: PPI %u received but no handler allocated\n", interrupt + 16);
        ppi_disable(interrupt + 16);
        return;
    }

    handler();
}

void sched::CPU_Info::ipi_reschedule()
{
    __atomic_fetch_or(&ipi_mask, IPI_RESCHEDULE, __ATOMIC_RELEASE);
    ipi_send(this, 0x00);
}

void sched::CPU_Info::ipi_cpu_park()
{
    __atomic_or_fetch(&ipi_mask, IPI_CPU_PARK, __ATOMIC_RELEASE);
    ipi_send(this, 2);
}

void sched::CPU_Info::ipi_get_attention()
{
    __atomic_or_fetch(&ipi_mask, IPI_GET_ATTENTION, __ATOMIC_RELEASE);
    ipi_send(this, 3);
}