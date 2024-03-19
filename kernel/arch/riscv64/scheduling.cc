#include <sched/sched.hh>
#include <memory/mem.hh>
#include <memory/virtmem.hh>
#include <paging/riscv64_paging.hh>
#include <types.hh>
#include <kern_logger/kern_logger.hh>
#include <acpi/acpi.hh>
#include <acpi/acpi.h>
#include <memory/malloc.hh>
#include <cpus/timer.hh>
#include <interrupts/interrupts.hh>

#include <cpus/floating_point.hh>

extern klib::shared_ptr<Arch_Page_Table> idle_page_table;

extern "C" void set_cpu_struct(CPU_Info *);

// Direct mode
// I find this more convenient, as it allows to save the context in a single assembly
// routine and then deal with interrupts in C++.
static const u8 STVEC_MODE = 0x0;

extern "C" void isr(void);
void program_stvec() {
    const u64 stvel_val = (u64)isr | (u64)STVEC_MODE;
    asm volatile("csrw stvec, %0" : : "r"(stvel_val) : "memory");
}

void set_sscratch(u64 scratch) {
    asm volatile("csrw sscratch, %0" : : "r"(scratch) : "memory");
}

RCHT * get_rhct()
{
    static RCHT * rhct_virt = nullptr;
    static bool have_acpi = true;

    if (rhct_virt == nullptr and have_acpi) {
        u64 rhct_phys = get_table(0x54434852); // RHCT
        if (rhct_phys == 0) {
            have_acpi = false;
            return nullptr;
        }

        ACPISDTHeader h;
        copy_from_phys(rhct_phys, &h, sizeof(h));

        rhct_virt = (RCHT*)malloc(h.length);
        copy_from_phys(rhct_phys, rhct_virt, h.length);
    }

    return rhct_virt;
}

void initialize_timer()
{
    if (timer_needs_initialization()) {
        RCHT * rhct = get_rhct();
        if (rhct == nullptr) {
            serial_logger.printf("Error: could not initialize timer. RHCT table not found\n");
            return;
        }

        serial_logger.printf("Info: time base frequency: %i\n", rhct->time_base_frequency);

        set_timer_frequency(rhct->time_base_frequency);
    }
}

MADT * get_madt()
{
    static MADT * rhct_virt = nullptr;
    static bool have_acpi = true;

    if (rhct_virt == nullptr and have_acpi) {
        u64 rhct_phys = get_table(0x43495041); // APIC (because why not)
        if (rhct_phys == 0) {
            have_acpi = false;
            return nullptr;
        }

        ACPISDTHeader h;
        copy_from_phys(rhct_phys, &h, sizeof(h));

        rhct_virt = (MADT*)malloc(h.length);
        copy_from_phys(rhct_phys, rhct_virt, h.length);
    }

    return rhct_virt;
}

ReturnStr<u32> get_apic_processor_uid(u64 hart_id)
{
    MADT * m = get_madt();
    if (m == nullptr)
        return {ERROR_GENERAL, 0};

    // Find the RINTC
    u32 offset = sizeof(MADT);
    u32 length = m->header.length;
    while (offset < length) {
        MADT_RINTC_entry * e = (MADT_RINTC_entry *)((char *)m + offset);
        if (e->header.type == MADT_RINTC_ENTRY_TYPE and e->hart_id == hart_id)
            return {SUCCESS, e->acpi_processor_id};

        offset += e->header.length;
    }

    return {ERROR_GENERAL, 0};
}

// TODO: Think about return type
ReturnStr<klib::string> get_isa_string(u64 hart_id)
{
    auto apic_id = get_apic_processor_uid(hart_id);
    if (apic_id.result != SUCCESS)
        return {apic_id.result, {}};

    RCHT * rhct = get_rhct();
    if (rhct == nullptr) {
        serial_logger.printf("Could not get rhct\n");
        return {ERROR_GENERAL, {}};
    }

    const u32 size = rhct->h.length;

    // Find the right hart info node
    u32 offset = sizeof(RCHT);
    RCHT_HART_INFO_node * n = nullptr;
    while (offset < size) {
        RCHT_HART_INFO_node * t = (RCHT_HART_INFO_node *)((char *)rhct + offset);
        if (t->header.type == RCHT_HART_INFO_NODE and t->acpi_processor_uid == apic_id.val) {
            n = t;
            break;
        }

        offset += t->header.length;
    }

    if (n == nullptr) {
        serial_logger.printf("Could not find hart node info for ACPI processor UID %i\n", apic_id.val);
        return {ERROR_GENERAL, {}};
    }

    // Find ISA string
    for (u16 i = 0; i < n->offsets_count; ++i) {
        RCHT_ISA_STRING_node * node = (RCHT_ISA_STRING_node *)((char *)rhct + n->offsets[i]);
        if (node->header.type == RHCT_ISA_STRING_NODE) {
            return {SUCCESS, klib::string(node->string, node->string_length)};
        }
    }

    serial_logger.printf("Could not find ISA string for ACPI processor UID %i\n", apic_id.val);
    return {ERROR_GENERAL, {}};
}

void initialize_fp(const klib::string & isa_string)
{
    // Find the maximum supported floating point extension
    auto c = isa_string.c_str();
    FloatingPointSize max = FloatingPointSize::None;
    while (*c != '\0' and *c != '_') {
        switch (*c) {
            case 'd':
                if (max < FloatingPointSize::DoublePrecision)
                    max = FloatingPointSize::DoublePrecision;
                break;
            case 'f':
                if (max < FloatingPointSize::SinglePrecision)
                    max = FloatingPointSize::SinglePrecision;
                break;
            case 'q':
                if (max < FloatingPointSize::QuadPrecision)
                    max = FloatingPointSize::QuadPrecision;
                break;
        }
        ++c;
    }

    serial_logger.printf("Floating point register size: %i\n", fp_register_size(max));
    max_supported_fp_level = max;
}

void init_scheduling()
{
    serial_logger.printf("Initializing scheduling\n");

    CPU_Info * i = new CPU_Info();
    i->kernel_stack_top = i->kernel_stack.get_stack_top();

    void * temp_mapper_start = virtmem_alloc_aligned(16, 4);
    i->temp_mapper = RISCV64_Temp_Mapper(temp_mapper_start, idle_page_table->get_root());

    set_cpu_struct(i);

    // The registers are either stored in TaskDescriptor of U-mode originating interrupts, or in the CPU_Info structure
    // Thusm sscratch always holds CPU_Info struct, with pointers to both locations
    // On interrupt entry, it gets swapped with the userspace or old thread pointer, the registers get saved, and the kernel
    // one gets loaded back.
    set_sscratch((u64)i);

    program_stvec();

    initialize_timer();

    // Set ISA string
    // hart id 0 should always be present
    auto s = get_isa_string(0);
    if (s.result == SUCCESS) {
        i->isa_string = klib::forward<klib::string>(s.val);
        serial_logger.printf("ISA string: %s\n", i->isa_string.c_str());
    }

    initialize_fp(i->isa_string);

    // Enable interrupts
    const u64 mask = (1 << TIMER_INTERRUPT);
    asm volatile("csrs sie, %0" : : "r"(mask) : "memory");

    serial_logger.printf("Initializing idle task\n");

    init_idle();
    i->current_task = i->idle_task;

    serial_logger.printf("Scheduling initialized\n");
}