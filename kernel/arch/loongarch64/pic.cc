#include <acpi/acpi.h>
#include <acpi/acpi.hh>
#include <errno.h>
#include <interrupts/interrupt_handler.hh>
#include <kern_logger/kern_logger.hh>
#include <lib/vector.hh>
#include <loongarch_asm.hh>
#include <memory/paging.hh>
#include <memory/vmm.hh>
#include <sched/sched.hh>
#include <types.hh>

using namespace kernel;

constexpr u32 BIOPIC_GSI_COUNT = 256;

enum class IntControllerClass {
    None,
    LIOPIC,
    EIOPIC,
};

IntControllerClass interrupt_model = IntControllerClass::LIOPIC;

CPU_Info *interrupt_mappings[256] = {nullptr};

constexpr unsigned OTHER_FUNCTION_CONF = 0x0420;

constexpr u64 EXT_INT_en = 1UL << 48;
constexpr u64 INT_encode = 1UL << 49;

constexpr unsigned PIC_Intisr       = 0x1420;
constexpr unsigned PIC_Inten        = 0x1424;
constexpr unsigned PIC_Intenset     = 0x1428;
constexpr unsigned PIC_Intenclr     = 0x142c;
constexpr unsigned PIC_Intedge      = 0x1434;
constexpr unsigned PIC_CORE0_INTISR = 0x1440;
constexpr unsigned PIC_CORE1_INTISR = 0x1448;
constexpr unsigned PIC_CORE2_INTISR = 0x1450;
constexpr unsigned PIC_CORE3_INTISR = 0x1458;

constexpr unsigned PIC_Entry0 = 0x1400;

constexpr unsigned EXT_IOIen_base     = 0x1600;
constexpr unsigned EXT_IOIbounce_base = 0x1680;
constexpr unsigned EXT_IOImap_base    = 0x14C0;
constexpr unsigned EXT_IOImap_Core0   = 0x1C00;

struct BIOPIC {
    // TODO
    u64 base_address;
    void *virt_address;
    u16 gsi_base;
    u16 hadrware_id;
};

Spinlock interrupts_lock;
kresult_t interrupt_enable(u32 i)
{
    auto c = get_cpu_struct();
    assert(i < 256);

    Auto_Lock_Scope l(interrupts_lock);
    if (interrupt_mappings[i] == c)
        return 0;

    if (interrupt_mappings[i] != nullptr)
        return -EEXIST;

    interrupt_mappings[i] = c;

    switch (interrupt_model) {
    case IntControllerClass::LIOPIC: {
        // Vector 0
        u8 entry    = 1 << c->cpu_physical_id;
        u32 val     = iocsr_read32(PIC_Entry0 + i & ~0x3);
        auto offset = (i & 0x3) * 8;
        val &= ~(0xff << offset);
        val |= entry << offset;
        iocsr_write32(val, PIC_Entry0 + i & ~0x3);

        // Enable interrupts
        u32 mask = (1 << i);
        iocsr_write32(mask, PIC_Intenset);
    } break;
    case IntControllerClass::EIOPIC: {
        // Set the right CPU
        // For some reason, QEMU wants 32 bit accesses (it's possible the spec mandates it, and I've
        // missed it)
        u8 entry = 1 << c->cpu_physical_id;
        u32 val     = iocsr_read32(EXT_IOImap_Core0 + i & ~0x3);
        auto offset = (i & 0x3) * 8;
        val &= ~(0xff << offset);
        val |= entry << offset;
        iocsr_write32(val, EXT_IOImap_Core0 + i & ~0x3);

        // Enable intrrupt
        size_t base = i / 64;
        u64 enable  = iocsr_read64(EXT_IOIen_base + base * 8);
        enable |= 1UL << (i % 64);
        iocsr_write64(enable, EXT_IOIen_base + base * 8);
    } break;
    default:
        assert(!"interrupt_enable with unknown interrupt_model");
    }

    return 0;
}

void interrupt_complete(u32) { panic("interrupt complete not implemented"); }

MADT *get_madt()
{
    static MADT *rhct_virt = nullptr;
    static bool have_acpi  = true;

    if (rhct_virt == nullptr and have_acpi) {
        u64 rhct_phys = get_table(0x43495041); // APIC (because why not)
        if (rhct_phys == 0) {
            have_acpi = false;
            return nullptr;
        }

        ACPISDTHeader h;
        copy_from_phys(rhct_phys, &h, sizeof(h));

        rhct_virt = (MADT *)malloc(h.length);
        copy_from_phys(rhct_phys, rhct_virt, h.length);
    }

    return rhct_virt;
}

MADT_LIOPIC_entry *get_liopic_entry(int index = 0)
{
    MADT *m = get_madt();
    if (m == nullptr)
        return nullptr;

    u32 offset = sizeof(MADT);
    u32 length = m->header.length;
    int count  = 0;
    while (offset < length) {
        MADT_LIOPIC_entry *e = (MADT_LIOPIC_entry *)((char *)m + offset);
        if (e->header.type == MADT_LIOPIC_ENTRY_TYPE and count++ == index)
            return e;

        offset += e->header.length;
    }

    return nullptr;
}

MADT_EIOPIC_entry *get_eiopic_entry(int index = 0)
{
    MADT *m = get_madt();
    if (m == nullptr)
        return nullptr;

    u32 offset = sizeof(MADT);
    u32 length = m->header.length;
    int count  = 0;
    while (offset < length) {
        MADT_EIOPIC_entry *e = (MADT_EIOPIC_entry *)((char *)m + offset);
        if (e->header.type == MADT_EIOPIC_ENTRY_TYPE and count++ == index)
            return e;

        offset += e->header.length;
    }

    return nullptr;
}

MADT_BIOPIC_entry *get_biopic_entry(int index = 0)
{
    MADT *m = get_madt();
    if (m == nullptr)
        return nullptr;

    u32 offset = sizeof(MADT);
    u32 length = m->header.length;
    int count  = 0;
    while (offset < length) {
        MADT_BIOPIC_entry *e = (MADT_BIOPIC_entry *)((char *)m + offset);
        if (e->header.type == MADT_BIOPIC_ENTRY_TYPE and count++ == index)
            return e;

        offset += e->header.length;
    }

    return nullptr;
}

klib::vector<BIOPIC *> biopics;

void biopic_push(BIOPIC *biopic)
{
    if (!biopics.push_back(nullptr))
        panic("Could not allocate memory for biopics array\n");

    auto gsi_base = biopic->gsi_base;

    size_t i = biopics.size() - 1;
    for (; i > 0; --i) {
        if (biopics[i - 1]->gsi_base < gsi_base) {
            biopics[i] = biopics[i - 1];
        } else {
            break;
        }
    }

    biopics[i] = biopic;
}

void init_interrupts()
{
    auto e = get_eiopic_entry();
    if (e) {
        interrupt_model = IntControllerClass::EIOPIC;

        // Enable IOCSR
        auto val = iocsr_read64(OTHER_FUNCTION_CONF);
        val |= EXT_INT_en | INT_encode;
        iocsr_write64(val, OTHER_FUNCTION_CONF);

        // Mask everything
        for (size_t i = 0; i < 4; ++i) {
            iocsr_write64(0, EXT_IOIen_base + i * 8);
            iocsr_write64(0, EXT_IOIbounce_base + i * 8);
        }

        // EIO PIC has 8 groups of interrupts, and there are 8 external vectors, so route groups to
        // those
        iocsr_write64(0x00 | 0x100 | 0x20000 | 0x3000000 | 0x400000000 | 0x50000000000 | 0x6000000000000 | 0x700000000000000,
                      EXT_IOImap_base);

        return;
    }

    panic("EIO PIC not found, other interrupt controllers not implemented\n");

    // serial_logger.printf("EIPOIC: cascade_vector %x node %x map %lx\n", e->cascade_vector,
    // e->node, e->node_map);

    // panic("panic");

    // auto e = get_liopic_entry();
    // panic("e: %lx\n", e);
}

// TODO
u32 interrupt_min()
{
    switch (interrupt_model) {
    default:
        return 0;
    }
}

u32 interrupt_limint()
{
    switch (interrupt_model) {
    case IntControllerClass::None:
        return 0;
    case IntControllerClass::LIOPIC:
        return 32;
    case IntControllerClass::EIOPIC:
        return 256;
    }

    assert(false);
    return 0;
}