#include <uacpi/acpi.h>
#include <uacpi/tables.h>
#include <cpus/sse.hh>
#include <interrupts/apic.hh>
#include <interrupts/gdt.hh>
#include <interrupts/interrupts.hh>
#include <interrupts/ioapic.hh>
#include <interrupts/pic.hh>
#include <kern_logger/kern_logger.hh>
#include <memory/vmm.hh>
#include <paging/x86_temp_mapper.hh>
#include <processes/tasks.hh>
#include <sched/sched.hh>
#include <x86_asm.hh>
#include <x86_utils.hh>
#include <pmos/utility/scope_guard.hh>
#include <syscall.hh>
#include <pmos/containers/set.hh>
#include <time/timers.hh>

using namespace kernel;
using namespace kernel::x86;
using namespace kernel::sched;
using namespace kernel::log;
using namespace kernel::proc;
using namespace kernel::x86::interrupts;
using namespace kernel::x86::paging;
using namespace kernel::x86::interrupts::lapic;
using namespace kernel::paging;

#ifdef __i386__
using namespace kernel::ia32::interrupts;
using namespace kernel::ia32::paging;
#endif

constexpr sched::CPU_Info __seg_gs const *c = nullptr;
kernel::sched::CPU_Info *sched::get_cpu_struct() { return c->self; }

extern "C" void double_fault_isr();

void smp_wake_everyone_else_up();

namespace kernel::sched
{
extern bool cpu_struct_works;
}

bool setup_stacks(kernel::sched::CPU_Info *c);

bool use_smap = false;
bool use_smep = false;



extern "C" void allow_access_user()
{
    stac();
}

extern "C" void disallow_access_user()
{
    clac();
}

void detect_protections()
{
    
    auto c = cpuid(0x0);
    u32 max_cpuid_leaf = c.eax;

    if (max_cpuid_leaf >= 0x07) {
        auto c = cpuid2(0x07, 0);
        if (c.ebx & (1 << 7)) {
            use_smep = true;
            serial_logger.printf("Using SMEP in kernel...\n");
        }

        if (c.ebx & (1 << 20)) {
            use_smap = true;
            serial_logger.printf("Using SMAP in kernel...\n");
        }
    }
}

void enable_protections()
{
    auto c = getCR4();
    if (use_smap)
        c |= (1 << 21);
    if (use_smep)
        c |= (1 << 20);
    setCR4(c);

    if (use_smap)
        clac();
}

void init_per_cpu(u64 lapic_id)
{
    sse::detect_sse();
    detect_protections();

    CPU_Info *c = new CPU_Info();
    if (!c)
        panic("Couldn't allocate memory for CPU_Info\n");

    #ifdef __i386__
    kernel::ia32::interrupts::gdt_set_cpulocal(c);
    loadGDT(&c->cpu_gdt);
    #else
    loadGDT(&c->cpu_gdt);
    write_msr(0xC0000101, (u64)c);
    #endif

    sched::cpu_struct_works = true;

    if (!setup_stacks(c))
        panic("Failed to setup stacks\n");

    loadTSS(TSS_SEGMENT);

    c->lapic_id = lapic_id;

    if (!cpus.push_back(c))
        panic("Failed to reserve memory for cpus vector in init_per_cpu()\n");
    c->cpu_id = cpus.size() - 1;

    serial_logger.printf("Initializing idle task\n");

    auto r = init_idle(c);
    if (r)
        panic("Failed to initialize idle task: %i\n", r);

    auto t = proc::TaskGroup::create_for_task(c->idle_task);
    if (!t.success())
        panic("Failed to create task group for kernel: %i\n", t.result);
    proc::kernel_tasks = t.val;

    c->current_task = c->idle_task;
    c->idle_task->page_table->apply_cpu(c);

    serial_logger.printf("Idle task initialized\n");

    program_syscall();
    set_idt();
    enable_apic();
    sse::enable_sse();
    enable_protections();

    void *temp_mapper_start = vmm::kernel_space_allocator.virtmem_alloc_aligned(16, 4);
    if (!temp_mapper_start)
        panic("Failed to allocate memory for temp_mapper_start\n");

    c->temp_mapper = create_temp_mapper(temp_mapper_start, getCR3());
    #ifdef __i386__
    if (!c->temp_mapper)
        panic("Failed to create temp mapper\n");
    #endif
}

extern int kernel_pt_active_cpus_count[2];

extern "C" void smp_main(CPU_Info *c)
{
    serial_logger.printf("Entered CPU %i\n", c->cpu_id);

    #ifdef __i386__
    kernel::ia32::interrupts::gdt_set_cpulocal(c);
    loadGDT(&c->cpu_gdt);
    #else
    loadGDT(&c->cpu_gdt);
    write_msr(0xC0000101, (u64)c);
    #endif

    #ifdef __i386__
    unbusyTSS(&c->cpu_gdt);
    #endif

    loadTSS(TSS_SEGMENT);
    program_syscall();
    set_idt();
    sse::enable_sse();
    enable_protections();

    if (c->to_restore_on_wakeup) {
        c->current_task->regs = *c->to_restore_on_wakeup;
        c->to_restore_on_wakeup.reset();
    }
    c->current_task->page_table->apply_cpu(c);
    c->current_task->page_table->apply();
    c->current_task->after_task_switch();

    assert(c->kernel_pt_generation == -1);
    c->kernel_pt_generation = __atomic_load_n(&kernel_pt_generation, __ATOMIC_ACQUIRE);
    __atomic_add_fetch(&kernel_pt_active_cpus_count[c->kernel_pt_generation], 1, __ATOMIC_RELAXED);
    c->online = true;

    if (__atomic_load_n(&c->ipi_mask, __ATOMIC_ACQUIRE))
        c->ipi_reschedule();
}

extern bool serial_initiated;
extern bool serial_functional;

extern "C" void acpi_main()
{
    serial_initiated = false;
    serial_logger.printf("Printing from C++ after waking up! (LAPIC ID %x)\n", get_lapic_id());
    setCR3(idle_cr3);

    auto c = cpus[0];
    init_PIC();

    #ifdef __i386__
    kernel::ia32::interrupts::gdt_set_cpulocal(c);
    loadGDT(&c->cpu_gdt);
    unbusyTSS(&c->cpu_gdt);
    #else
    loadGDT(&c->cpu_gdt);
    write_msr(0xC0000101, (u64)c);
    c->cpu_gdt.tss_descriptor.access = 0x89;
    #endif

    loadTSS(TSS_SEGMENT);
    program_syscall();
    set_idt();
    enable_apic();
    sse::enable_sse();
    enable_protections();

    if (c->to_restore_on_wakeup) {
        c->current_task->regs = *c->to_restore_on_wakeup;
        c->to_restore_on_wakeup.reset();
    }
    c->current_task->page_table->apply_cpu(c);
    c->current_task->page_table->apply();
    c->current_task->after_task_switch();

    assert(c->kernel_pt_generation == -1);
    c->kernel_pt_generation = __atomic_load_n(&kernel_pt_generation, __ATOMIC_ACQUIRE);
    __atomic_add_fetch(&kernel_pt_active_cpus_count[c->kernel_pt_generation], 1, __ATOMIC_RELAXED);
    c->online = true;

    if (__atomic_load_n(&c->ipi_mask, __ATOMIC_ACQUIRE))
        c->ipi_reschedule();

    smp_wake_everyone_else_up();
}

void init_acpi_trampoline();

CPU_Info *prepare_cpu(u32 lapic_id)
{
    serial_logger.printf("Found CPU LAPIC %x\n", lapic_id);

    CPU_Info *c = new CPU_Info();
    if (!c)
        panic("Couldn't allocate memory for CPU_Info\n");

    if (!setup_stacks(c))
        panic("Failed to set up stack for AP");

    c->lapic_id             = lapic_id;
    c->kernel_pt_generation = -1;

    if (!cpus.push_back(c))
        panic("Failed to reserve memory for cpus vector in init_per_cpu()\n");
    c->cpu_id = cpus.size() - 1;

    auto r = init_idle(c);
    if (r)
        panic("Failed to initialize idle task: %i\n", r);
    c->current_task = c->idle_task;

    assert(proc::kernel_tasks);
    if (auto t = proc::kernel_tasks->atomic_register_task(c->idle_task); t)
        panic("Failed to add idle task to the kernel process group: %i\n", t);

    c->online = false;

    void *temp_mapper_start = vmm::kernel_space_allocator.virtmem_alloc_aligned(16, 4);
    if (!temp_mapper_start)
        panic("Failed to allocate memory for temp_mapper_start\n");

    c->temp_mapper = create_temp_mapper(temp_mapper_start, getCR3());
    #ifdef __i386__
    if (!c->temp_mapper)
        panic("Failed to create temp mapper\n");
    #endif

    return c;
}

static bool have_online_capable_bit = false;
static void setup_online_capable()
{
    struct acpi_fadt *fadt;
    auto res = uacpi_table_fadt(&fadt);
    if (res != UACPI_STATUS_OK)
        panic("No FADT!");

    have_online_capable_bit = (fadt->hdr.revision > 6) or 
        (fadt->hdr.revision == 6 and fadt->fadt_minor_verison >= 3);
}

static bool cpu_usable(u32 flags)
{
    if (flags & ACPI_PIC_ENABLED)
        return true;

    if (!have_online_capable_bit)
        return false;

    return flags & ACPI_PIC_ONLINE_CAPABLE;
}

constexpr u32 LAPIC_ID_NONE = 0xff;
constexpr u32 x2APIC_ID_NONE = 0xffffffff;

void init_smp()
{
    setup_online_capable();

    uint32_t my_lapic_id = get_lapic_id();
    uacpi_table madt;
    auto res = uacpi_table_find_by_signature(ACPI_MADT_SIGNATURE, &madt);
    if (res != UACPI_STATUS_OK) {
        panic("No MADT");
    }
    auto guard = pmos::utility::make_scope_guard([&]{
        uacpi_table_unref(&madt);
    });
    
    acpi_madt *m = (acpi_madt *)madt.ptr; 

    serial_logger.printf("Initializing SMP\n");

    pmos::containers::set<u32> lapic_ids;
    if (lapic_ids.insert_noexcept(my_lapic_id).first == lapic_ids.end())
        panic("Couldn't insert my lapic id into set");

    bool have_xapic_entries = false;
    res = uacpi_for_each_subtable(madt.hdr, sizeof(struct acpi_madt), [](auto ctx, auto hdr) -> uacpi_iteration_decision {
        bool &have_xapic_entries = *(bool *)ctx;

        if (hdr->type == ACPI_MADT_ENTRY_TYPE_LAPIC) {
            struct acpi_madt_lapic *lapic = (struct acpi_madt_lapic*)hdr;

            if (!cpu_usable(lapic->flags))
                return UACPI_ITERATION_DECISION_CONTINUE;

            if (lapic->id == LAPIC_ID_NONE)
                return UACPI_ITERATION_DECISION_CONTINUE;

            have_xapic_entries = true;
            return UACPI_ITERATION_DECISION_BREAK;
        }
        return UACPI_ITERATION_DECISION_CONTINUE;
    }, (void *)&have_xapic_entries);
    if (res != UACPI_STATUS_OK)
        panic("uacpi_for_each_subtable error");

    u32 offset = sizeof(acpi_madt);
    u32 length = m->hdr.length;
    while (offset < length) {
        acpi_entry_hdr *e = (acpi_entry_hdr *)((char *)m + offset);
        offset += e->length;

        if (e->type == ACPI_MADT_ENTRY_TYPE_LAPIC) {
            acpi_madt_lapic *ee = (acpi_madt_lapic *)e;
            uint32_t e_id        = ee->id;

            if (ee->id == LAPIC_ID_NONE)
                continue;

            if (!cpu_usable(ee->flags))
                continue;

            auto res = lapic_ids.insert_noexcept(e_id);
            if (res.first == lapic_ids.end())
                panic("Couldn't allocate memory for set in CPU initialization");

            if (!res.second)
                continue;

            prepare_cpu(e_id);
        } else if (e->type == ACPI_MADT_ENTRY_TYPE_LOCAL_X2APIC) {
            acpi_madt_x2apic *ee = (acpi_madt_x2apic *)e;
            uint32_t e_id        = ee->id;

            if (apic_mode != APICMode::X2APIC)
                continue;

            if (have_xapic_entries and e_id <= 0xff)
                // Ignore this and use XAPIC entry
                continue;

            if (e_id == x2APIC_ID_NONE)
                continue;

            if (!cpu_usable(ee->flags))
                continue;

            auto res = lapic_ids.insert_noexcept(e_id);
            if (res.first == lapic_ids.end())
                panic("Couldn't allocate memory for set in CPU initialization");

            // Duplicate entry
            if (!res.second)
                continue;

            prepare_cpu(e_id);
        } else if (e->type == ACPI_MADT_ENTRY_TYPE_LSAPIC) {
            panic("Found LSAPIC entry in MADT!");
        }
    }

    smp_wake_everyone_else_up();
}

void init_scheduling_on_bsp()
{
    serial_logger.printf("Initializing APIC\n");
    prepare_apic_bsp();

    serial_logger.printf("Initializing interrupts and timers\n");
    time::init_timers();
    init_interrupts();

    serial_logger.printf("Initializing per-CPU structures\n");
    init_per_cpu(get_lapic_id());

    serial_logger.printf("Initializing I/O APICs\n");
    IOAPIC::init_ioapics();

    time::init_after_lapic();

    serial_logger.printf("Initializing ACPI trampoline\n");
    init_acpi_trampoline();

    serial_logger.printf("Scheduling initialized\n");
}

extern u8 init_vec_begin;
extern u8 acpi_trampoline;
extern u8 init_vec_end;

// Relocations
extern ulong init_vec_jump_pmode;
extern ulong smp_trampoline_gdtr_addr;
extern ulong acpi_vec_jump_pmode;

// Variables
extern ulong smp_trampoline_cr3;
extern u32 smp_trampoline_trampoilne_flags;

constexpr u32 SMP_TRAMPOLINE_ENABLE_PAE = 0b001;
constexpr u32 SMP_TRAMPOLINE_ENABLE_NX  = 0b010;
constexpr u32 SMP_TRAMPOLINE_5_LVL_PAGING = 0x100;

pmm::phys_page_t acpi_trampoline_page = 0;

static bool have_acpi_startup = false;
ReturnStr<u32> acpi_wakeup_vec()
{
    if (!have_acpi_startup)
        return Error(-ENOENT);

    return Success(
        (u32)(acpi_trampoline_page + (char *)&acpi_trampoline - (char *)&init_vec_begin));
}
void init_acpi_trampoline()
{
    auto page = pmm::alloc_pages(1, 0, kernel::pmm::AllocPolicy::ISA);
    if (!page) {
        panic("Kernel error: could not allocate memory for ACPI trampoline");
    }

    acpi_trampoline_page = page->get_phys_addr();
    have_acpi_startup = true;

    init_vec_jump_pmode += acpi_trampoline_page;
    smp_trampoline_gdtr_addr += acpi_trampoline_page;
    acpi_vec_jump_pmode += acpi_trampoline_page;

    auto [result, new_cr3] = create_empty_cr3();
    if (result)
        panic("Failed to allocate cr3 for SMP trampoline");
    smp_trampoline_cr3       = new_cr3;

    Page_Table_Arguments pta = {
        .readable           = true,
        .writeable          = true,
        .user_access        = false,
        .execution_disabled = false,
    };

    #ifdef __i386__
    if (use_pae) {
        smp_trampoline_trampoilne_flags |= SMP_TRAMPOLINE_ENABLE_PAE;
    }
    if (support_nx)
        smp_trampoline_trampoilne_flags |= SMP_TRAMPOLINE_ENABLE_NX;
    #else
    // if (use_5lvl_paging)
    //     smp_trampoline_trampoilne_flags |= SMP_TRAMPOLINE_5_LVL_PAGING;
    #endif

    result = map_page(new_cr3, acpi_trampoline_page, (void *)acpi_trampoline_page, pta);
    if (result)
        panic("Failed to ID map init page");

    Temp_Mapper_Obj<char> t(request_temp_mapper());
    char *ptr = t.map(acpi_trampoline_page);
    memcpy(ptr, &init_vec_begin, (char *)&init_vec_end - (char *)&init_vec_begin);

    serial_logger.printf("Initialized SMP/ACPI trampoline vector at %x (%x)\n",
                         (u32)acpi_trampoline_page, acpi_wakeup_vec().val);
}