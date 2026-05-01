#include <uacpi/tables.h>
#include <uacpi/acpi.h>
#include <errno.h>
#include <interrupts/interrupt_handler.hh>
#include <kern_logger/kern_logger.hh>
#include <lib/vector.hh>
#include <loongarch_asm.hh>
#include <memory/paging.hh>
#include <memory/vmm.hh>
#include <pmos/io.h>
#include <pmos/ipc.h>
#include <sched/sched.hh>
#include <types.hh>
#include <csr.hh>
#include <pmos/utility/scope_guard.hh>

using namespace kernel;
using namespace kernel::sched;
using namespace kernel::log;
using namespace kernel::interrupts;

// constexpr u32 BIOPIC_GSI_COUNT = 256;

struct PICAllocation;

enum class IntControllerClass {
    None,
    LIOPIC,
    EIOPIC,
};

IntControllerClass interrupt_model = IntControllerClass::LIOPIC;

struct ExtIntC {
    virtual ~ExtIntC() = default;

    virtual void interrupt_enable(u32 controller_vector) = 0;
    virtual void interrupt_complete(u32 interrupt)       = 0;
};

struct BIOPIC: ExtIntC {
    // TODO
    u64 base_address;
    void *virt_address;
    u16 gsi_base;
    u16 hardware_id;
    Spinlock lock;

    static constexpr u32 ints_count = 64;

    std::array<PICAllocation *, ints_count> mappings = {};

    virtual void interrupt_enable(u32 controller_vector) override;
    void initialize();

    static constexpr u32 INT_MASK      = 0x020;
    static constexpr u32 HTMSI_EN      = 0x040;
    static constexpr u32 INTEDGE       = 0x060;
    static constexpr u32 INTCLR        = 0x080;
    static constexpr u32 ROUTE_ENTRY_0 = 0x100;
    static constexpr u32 HTMSI_VECTOR0 = 0x200;

    inline u32 gsi_limit() { return gsi_base + ints_count; }
    void set_mapping(u32 gsi, unsigned idx, CPU_Info *c, bool edge_triggered);
    virtual void interrupt_complete(u32 interrupt) override;
};

struct PICAllocation: InterruptHandler {
    ExtIntC *controller   = nullptr;
    u32 controller_vector = 0;
    bool edge_triggered   = false;
};

constexpr size_t INTERRUPT_MAPPINGS_SIZE = 256;
struct EIOPIC {
    std::array<PICAllocation, INTERRUPT_MAPPINGS_SIZE> interrupt_mappings;

    u64 node_map = 0;
    bool present: 1 = false;
    bool initialized : 1 = false;
    u8 cascade_vector = 0;
    u8 node_id = 0;
    u8 version = 0;

    Spinlock init_lock;
};

constexpr size_t max_controllers = 8;

static std::array<EIOPIC, 8> eiopics;

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
constexpr unsigned perCore_EXT_IOI    = 0x1800;

static Spinlock interrupts_lock;
void interrupts::interrupt_enable(InterruptHandler *handler)
{
    auto h = static_cast<PICAllocation *>(handler);

    Auto_Lock_Scope l(interrupts_lock);
    auto c = h->parent_cpu;
    auto i = h - &c->parent_eiopic->interrupt_mappings[0];
    assert(h->parent_cpu == get_cpu_struct());

    switch (interrupt_model) {
    // case IntControllerClass::LIOPIC: {
    //     // Enable interrupts
    //     u32 mask = (1 << i);
    //     u32 val  = iocsr_read32(PIC_Intenset);
    //     iocsr_write32(mask | val, PIC_Intenset);

    //     // Vector 0
    //     u8 entry = 1 << c->cpu_physical_id;
    //     iocsr_write32(entry, PIC_Entry0 + i);
    // } break;
    case IntControllerClass::EIOPIC: {
        // Set the right CPU
        // For some reason, QEMU wants 32 bit accesses (it's possible the spec mandates it, and I've
        // missed it)
        u8 entry    = 1 << c->cpu_physical_id;
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
        assert(h->controller);

        h->controller->interrupt_enable(h->controller_vector);
    } break;
    default:
        assert(!"interrupt_enable with unknown interrupt_model");
    }
}

void interrupts::interrupt_complete(InterruptHandler *handler)
{
    auto h = static_cast<PICAllocation *>(handler);
    auto c = get_cpu_struct();

    assert(h->parent_cpu == get_cpu_struct());
    assert(h->controller);
    h->controller->interrupt_complete(h->controller_vector);
    auto i = h - &c->parent_eiopic->interrupt_mappings[0];

    csrxchg32<loongarch::csr::ECFG>(-1U, 1 << (i/sizeof(uint32_t) + 2));
}

klib::vector<BIOPIC *> biopics;

static void biopic_push(BIOPIC *biopic)
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

void BIOPIC::initialize()
{
    u32 *base = (u32 *)virt_address;
    // ACPI reports hardware ID 0 for some reason
    // u32 id    = mmio_readl(base + 0);
    // id >>= 24;
    // assert(hardware_id == id);

    // Mask interrupts
    mmio_writel(base + INT_MASK / sizeof(uint32_t), -1U);
    mmio_writel(base + INT_MASK / sizeof(uint32_t) + 1, -1U);

    // u8 intnum = mmio_readl(base + 1) >> 16;
    // serial_logger.printf("BIOPIC intnum %x\n", intnum);
}

void BIOPIC::interrupt_complete(u32 vector)
{
    assert(vector > gsi_base);
    vector -= gsi_base;

    assert(vector < 64);

    u32 *base = (u32 *)virt_address;

    u32 mask   = 1 << (vector % sizeof(uint32_t));
    u32 offset = vector / sizeof(uint32_t);

    mmio_writel(base + INTCLR / sizeof(uint32_t) + offset, mask);
}

void BIOPIC::interrupt_enable(u32 gsi)
{
    u32 base = gsi - gsi_base;
    assert(base < 64);

    int idx  = base / 32;
    u32 mask = 1 << base % 32;

    u32 *ptr = (u32 *)virt_address;

    // Clear interrupt
    mmio_writel(ptr + INTCLR / sizeof(uint32_t) + idx, mask);

    Auto_Lock_Scope l(lock);
    u32 val = mmio_readl(ptr + INT_MASK / sizeof(u32) + idx);
    val &= ~mask;
    mmio_writel(ptr + INT_MASK / sizeof(u32) + idx, val);
}

void BIOPIC::set_mapping(u32 gsi, unsigned idx, CPU_Info *, bool edge_triggered)
{
    u32 base = gsi - gsi_base;
    assert(base < 64);

    u32 half = base / 32;
    u32 mask = 1 << (base % 32);
    u32 *ptr = (u32 *)virt_address;

    Auto_Lock_Scope l(lock);

    // Enable HT interrupt message
    u32 val = mmio_readl(ptr + HTMSI_EN / sizeof(uint32_t) + half);
    val |= mask;
    mmio_writel(ptr + HTMSI_EN / sizeof(uint32_t) + half, val);

    // Edge trigger
    val = mmio_readl(ptr + INTEDGE / sizeof(uint32_t) + half);
    if (edge_triggered)
        val |= mask;
    else
        val &= ~mask;
    mmio_writel(ptr + INTEDGE / sizeof(uint32_t) + half, val);

    // TODO: Route entries

    // HT vector
    mmio_writeb((u8 *)virt_address + HTMSI_VECTOR0 + base, idx);
}

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

void init_eiopic()
{
    auto c = get_cpu_struct();
    assert(c);

    auto eiopic = c->parent_eiopic;
    assert(eiopic);

    // Lock here is intentional, vs. pthread_once or whatever
    Auto_Lock_Scope l(eiopic->init_lock);

    if (eiopic->initialized)
        return;

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
    iocsr_write64(0x00 | 0x100 | 0x20000 | 0x3000000 | 0x400000000 | 0x50000000000 |
                        0x6000000000000 | 0x700000000000000,
                    EXT_IOImap_base);

    eiopic->initialized = true;
}

EIOPIC *eiopic_for_cpu(CPU_Info *i)
{
    assert(i);
    assert(i->cpu_physical_id < 64);

    for (auto &e: eiopics) {
        if (!e.present)
            continue;

        if (e.node_map & ((u64)1 << i->cpu_physical_id))
            return &e;
    }

    return nullptr;
}

void set_eiopic_for_cpu(CPU_Info *i)
{
    auto e = eiopic_for_cpu(i);
    if (!e)
        panic("No EIOPIC for CPU %u (%u)!", i->cpu_id, i->cpu_physical_id);

    i->parent_eiopic = e;
}

void init_interrupts()
{
    uacpi_table m;
    auto res = uacpi_table_find_by_signature(ACPI_MADT_SIGNATURE, &m);
    if (res != UACPI_STATUS_OK) {
        log::serial_logger.printf("Couldn't get MADT table...\n");
        return;
    }
    auto guard = pmos::utility::make_scope_guard([&]{
        uacpi_table_unref(&m);
    });

    acpi_madt *madt = (acpi_madt *)m.ptr;


    interrupt_model = IntControllerClass::EIOPIC;
    
    unsigned i = 0;
    while (auto e = (acpi_madt_eio_pic *)get_madt_entry(madt, ACPI_MADT_ENTRY_TYPE_EIO_PIC, i)) {
        auto &eiopic = eiopics[i];

        eiopic.node_map = e->node_map;
        eiopic.node_id = e->node;
        eiopic.version = e->version;
        eiopic.cascade_vector = e->cascade_vector;
        eiopic.present = true;

        serial_logger.printf("Found EIO PIC %i, node_map %lx, node %u version %u cascade_vector %u\n", i, e->node_map, e->node, e->version, e->cascade_vector);

        ++i;
    } 
    
    if (i == 0) {
        panic("Did no find EIOPIC, other interrupt controllers not implemented");
    }

    int idx = 0;
    acpi_madt_bio_pic *be;
    while ((be = (acpi_madt_bio_pic *)get_madt_entry(madt, ACPI_MADT_ENTRY_TYPE_BIO_PIC, idx++))) {
        auto biopic = new BIOPIC();
        if (!biopic)
            panic("Failed to allocate memory for BIO PIC");

        constexpr u64 PAGE_MASK = PAGE_SIZE - 1;

        biopic->base_address = be->address;
        biopic->gsi_base     = be->gsi_base;
        auto offset          = biopic->base_address & PAGE_MASK;
        auto size            = be->size + offset;
        auto size_aligned    = (size + PAGE_MASK) & ~PAGE_MASK;

        auto addr = vmm::kernel_space_allocator.virtmem_alloc(size_aligned / PAGE_SIZE);
        if (!addr)
            panic("Failed to allocate virtual memory window for BIO PIC");

        const paging::Page_Table_Arguments arg = {.readable           = true,
                                           .writeable          = true,
                                           .user_access        = false,
                                           .global             = true,
                                           .execution_disabled = true,
                                           .extra              = PAGING_FLAG_NOFREE,
                                           .cache_policy       = paging::Memory_Type::IONoCache};

        auto result = map_kernel_pages(biopic->base_address & ~PAGE_MASK, addr, size_aligned, arg);
        if (result)
            panic("Failed to map BIO PIC into kernel\n");

        biopic->virt_address = (char *)addr + offset;
        biopic->hardware_id  = be->hardware_id;

        biopic_push(biopic);

        biopic->initialize();

        serial_logger.printf("Initialized BIO PIC at %lx\n", biopic->virt_address);
    }
}

BIOPIC *get_biopic(u32 gsi)
{
    // TODO: Do a binary search instead...
    for (auto e: biopics)
        if (e->gsi_base <= gsi and e->gsi_limit() > gsi)
            return e;

    return nullptr;
}

ReturnStr<InterruptHandler *> interrupts::allocate_or_get_handler(u32 gsi, bool edge_triggered, bool)
{
    auto biopic = get_biopic(gsi);
    if (!biopic)
        return Error(-ENOENT);

    Auto_Lock_Scope l(interrupts_lock);

    auto all = biopic->mappings[gsi - biopic->gsi_base];
    if (all)
        return Success(all);

    // Find least loaded CPU
    auto *cpu = cpus[0];
    for (size_t i = 1; i < cpus.size(); ++i) {
        if (cpus[i]->allocated_int_count < cpu->allocated_int_count)
            cpu = cpus[i];
    }

    auto &interrupt_mappings = cpu->parent_eiopic->interrupt_mappings;

    // Find unused slot
    u32 idx = 0;
    for (; idx < INTERRUPT_MAPPINGS_SIZE; ++idx) {
        if (!interrupt_mappings[idx].parent_cpu)
            break;
    }

    if (idx == INTERRUPT_MAPPINGS_SIZE)
        return Error(-ENOMEM);

    interrupt_mappings[idx].parent_cpu        = cpu;
    interrupt_mappings[idx].controller        = biopic;
    interrupt_mappings[idx].controller_vector = gsi;
    interrupt_mappings[idx].edge_triggered    = edge_triggered;

    biopic->mappings[gsi - biopic->gsi_base] = &interrupt_mappings[idx];
    biopic->set_mapping(gsi, idx, cpu, edge_triggered);

    cpu->allocated_int_count++;

    return &interrupt_mappings[idx];
}

u32 get_irq(unsigned sector)
{
    switch (interrupt_model) {
    case IntControllerClass::LIOPIC:
        panic("LIOPIC not implemented\n");
    case IntControllerClass::EIOPIC: {
        // TODO: Support non-vectored interrupts

        assert(sector < 8);

        u32 val = iocsr_read32(perCore_EXT_IOI + sector * 4);
        if (val == 0)
            return -1U;

        return sector * 32 + __builtin_ctz(val);
    } break;
    default:
        break;
    }

    assert(false);
    return -1U;
}
static void ack_interrupt(u32 irq)
{
    switch (interrupt_model) {
    case IntControllerClass::EIOPIC: {
        assert(irq < 256);
        u32 reg  = irq / 32;
        u32 mask = 1 << irq % 32;
        iocsr_write32(mask, perCore_EXT_IOI + reg * 4);
    } break;
    default:
        panic("LIOPIC not implemented\n");
    }
}

void handle_hardware_interrupt(u32 estat)
{
    auto c = get_cpu_struct();

    assert(estat);
    u32 sector = __builtin_ctz(estat >> 2);

    u32 irq = get_irq(sector);
    if (irq == -1U)
        // Spurious interrupt?!
        return;

    ack_interrupt(irq);
    auto &mapping = c->parent_eiopic->interrupt_mappings[irq];

    // Disable interrupts
    csrxchg32<loongarch::csr::ECFG>(0, 1 << (sector + 2));

    // Send the interrupt
    if (mapping.send_interrupt_notification() != NotificationResult::Success) {
        interrupt_disable(&mapping);
        interrupt_complete(&mapping);
    }
}

void interrupts::interrupt_disable(InterruptHandler *handler) {
    auto h = static_cast<PICAllocation *>(handler);
    auto c = get_cpu_struct();
    auto interrupt_id = h - &c->parent_eiopic->interrupt_mappings[0];

    switch (interrupt_model) {
        case IntControllerClass::EIOPIC: {
            Auto_Lock_Scope l(interrupts_lock);

            size_t base = interrupt_id / 64;
            u64 enable  = iocsr_read64(EXT_IOIen_base + base * 8);
            enable &= ~((u64)1 << (interrupt_id % 64));
            iocsr_write64(enable, EXT_IOIen_base + base * 8);
        } break;
    default:
        assert(false);
    }
}