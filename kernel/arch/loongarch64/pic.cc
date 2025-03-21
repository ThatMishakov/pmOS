#include <acpi/acpi.h>
#include <acpi/acpi.hh>
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

using namespace kernel;

// constexpr u32 BIOPIC_GSI_COUNT = 256;

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

    virtual void interrupt_enable(u32 controller_vector) override;
    void initialize();

    static constexpr u32 INT_MASK      = 0x020;
    static constexpr u32 HTMSI_EN      = 0x040;
    static constexpr u32 INTEDGE       = 0x060;
    static constexpr u32 INTCLR        = 0x080;
    static constexpr u32 ROUTE_ENTRY_0 = 0x100;
    static constexpr u32 HTMSI_VECTOR0 = 0x200;

    inline u32 gsi_limit() { return gsi_base + 64; }
    void set_mapping(u32 gsi, unsigned idx, CPU_Info *c, bool edge_triggered);
    virtual void interrupt_complete(u32 interrupt) override;
};

struct PICAllocation {
    CPU_Info *cpu         = nullptr;
    ExtIntC *controller   = nullptr;
    u32 controller_vector = 0;
    bool edge_triggered   = false;
};

constexpr size_t INTERRUPT_MAPPINGS_SIZE = 256;
PICAllocation interrupt_mappings[INTERRUPT_MAPPINGS_SIZE];

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
constexpr unsigned CORE0_EXT_IOIsr    = 0x1800;

Spinlock interrupts_lock;
kresult_t interrupt_enable(u32 i)
{
    auto c = get_cpu_struct();
    assert(i < 256);

    auto [cpu, controller, gsi, _] = interrupt_mappings[i];

    Auto_Lock_Scope l(interrupts_lock);
    if (cpu == nullptr)
        return -ENOENT;

    if (cpu != c)
        return -EBADF;

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
        assert(controller);

        controller->interrupt_enable(gsi);
    } break;
    default:
        assert(!"interrupt_enable with unknown interrupt_model");
    }

    return 0;
}

void interrupt_complete(u32 interrupt)
{
    assert(interrupt < 256);
    auto [cpu, controller, vector, _] = interrupt_mappings[interrupt];
    assert(cpu == get_cpu_struct());
    assert(controller);
    controller->interrupt_complete(vector);

    csrxchg32<loongarch::csr::ECFG>(-1U, 1 << (interrupt/sizeof(uint32_t) + 2));
}

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
        iocsr_write64(0x00 | 0x100 | 0x20000 | 0x3000000 | 0x400000000 | 0x50000000000 |
                          0x6000000000000 | 0x700000000000000,
                      EXT_IOImap_base);
    } else {
        panic("Did no find EIOPIC, other interrupt controllers not implemented");
    }

    int idx = 0;
    auto be = get_biopic_entry(idx);
    while (be) {
        auto biopic = new BIOPIC();
        if (!biopic)
            panic("Failed to allocate memory for BIO PIC");

        constexpr u64 PAGE_MASK = PAGE_SIZE - 1;

        biopic->base_address = be->base_address;
        biopic->gsi_base     = be->gsi_base;
        auto offset          = be->base_address & PAGE_MASK;
        auto size            = be->size + offset;
        auto size_aligned    = (size + PAGE_MASK) & ~PAGE_MASK;

        auto addr = vmm::kernel_space_allocator.virtmem_alloc(size_aligned / PAGE_SIZE);
        if (!addr)
            panic("Failed to allocate virtual memory window for BIO PIC");

        const Page_Table_Argumments arg = {.readable           = true,
                                           .writeable          = true,
                                           .user_access        = false,
                                           .global             = true,
                                           .execution_disabled = true,
                                           .extra              = PAGING_FLAG_NOFREE,
                                           .cache_policy       = Memory_Type::IONoCache};

        auto result = map_kernel_pages(be->base_address & ~PAGE_MASK, addr, size_aligned, arg);
        if (result)
            panic("Failed to map BIO PIC into kernel\n");

        biopic->virt_address = (char *)addr + offset;
        biopic->hardware_id  = be->hardware_id;

        biopic_push(biopic);

        biopic->initialize();

        serial_logger.printf("Initialized BIO PIC at %lx\n", biopic->virt_address);

        be = get_biopic_entry(++idx);
    }
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

BIOPIC *get_biopic(u32 gsi)
{
    // TODO: Do a binary search instead...
    for (auto e: biopics)
        if (e->gsi_base <= gsi and e->gsi_limit() > gsi)
            return e;

    return nullptr;
}

ReturnStr<std::pair<CPU_Info *, u32>> allocate_interrupt_single(u32 gsi, bool edge_triggered, bool)
{
    auto biopic = get_biopic(gsi);
    if (!biopic)
        return Error(-ENOENT);

    Auto_Lock_Scope l(interrupts_lock);

    for (u32 i = 0; i < INTERRUPT_MAPPINGS_SIZE; ++i) {
        if (interrupt_mappings[i].controller == biopic and
            interrupt_mappings[i].controller_vector == gsi)
            return interrupt_mappings[i].edge_triggered == edge_triggered
                       ? Success(std::pair {interrupt_mappings[i].cpu, i})
                       : Error(-EEXIST);
    }

    // Find least loaded CPU
    auto *cpu = cpus[0];
    for (size_t i = 1; i < cpus.size(); ++i) {
        if (cpus[i]->int_handlers.allocated_int_count < cpu->int_handlers.allocated_int_count)
            cpu = cpus[i];
    }

    // Find unused slot
    u32 idx = 0;
    for (; idx < INTERRUPT_MAPPINGS_SIZE; ++idx) {
        if (!interrupt_mappings[idx].cpu)
            break;
    }

    if (idx == INTERRUPT_MAPPINGS_SIZE)
        return Error(-ENOMEM);

    interrupt_mappings[idx].cpu               = cpu;
    interrupt_mappings[idx].controller        = biopic;
    interrupt_mappings[idx].controller_vector = gsi;
    interrupt_mappings[idx].edge_triggered    = edge_triggered;

    biopic->set_mapping(gsi, idx, cpu, edge_triggered);

    return ReturnStr<std::pair<CPU_Info *, u32>>::success(std::pair(cpu, idx));
}

u32 get_irq(unsigned sector)
{
    auto c = get_cpu_struct();

    switch (interrupt_model) {
    case IntControllerClass::LIOPIC:
        panic("LIOPIC not implemented\n");
    case IntControllerClass::EIOPIC: {
        // TODO: Support non-vectored interrupts
        u32 core_base = CORE0_EXT_IOIsr + 0x100 * c->cpu_physical_id;

        assert(sector < 8);

        u32 val = iocsr_read32(core_base + sector * 4);
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
void ack_interrupt(u32 irq)
{
    auto c = get_cpu_struct();

    switch (interrupt_model) {
    case IntControllerClass::EIOPIC: {
        u32 core_base = CORE0_EXT_IOIsr + 0x100 * c->cpu_physical_id;

        assert(irq < 256);
        u32 reg  = irq / 32;
        u32 mask = 1 << irq % 32;
        iocsr_write32(mask, core_base + reg * 4);
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
    if (irq == -1U) {
        panic("Unknown interrupt\n");
    }

    ack_interrupt(irq);

    auto handler = c->int_handlers.get_handler(irq);
    if (!handler) {
        interrupt_disable(irq);
        return;
    }

    // Disable interrupts
    csrxchg32<loongarch::csr::ECFG>(0, 1 << (sector + 2));

    // Send the interrupt to the port
    auto port = handler->port;
    bool sent = false;
    if (port) {
        IPC_Kernel_Interrupt kmsg = {IPC_Kernel_Interrupt_NUM, irq, c->cpu_id};
        auto result = port->atomic_send_from_system(reinterpret_cast<char *>(&kmsg), sizeof(kmsg));

        if (result == 0)
            sent = true;
        else
            serial_logger.printf("Error: %i\n", result);
    }

    if (!sent) {
        interrupt_disable(irq);
        interrupt_complete(irq);
 
        c->int_handlers.remove_handler(irq);
    } else {
        handler->active = true;
    }
}

void interrupt_disable(u32 interrupt_id) {
    assert(interrupt_id < 256);

    switch (interrupt_model) {
        case IntControllerClass::EIOPIC: {
            Auto_Lock_Scope l(interrupts_lock);

            size_t base = interrupt_id / 64;
            u64 enable  = iocsr_read64(EXT_IOIen_base + base * 8);
            enable &= ~(1UL << (interrupt_id % 64));
            iocsr_write64(enable, EXT_IOIen_base + base * 8);
        } break;
    default:
        assert(false);
    }
}