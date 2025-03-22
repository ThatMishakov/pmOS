/* Copyright (c) 2024, Mikhail Kovalev
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 * this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 *
 * 3. Neither the name of the copyright holder nor the names of its
 *    contributors may be used to endorse or promote products derived from
 *    this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include <acpi/acpi.h>
#include <acpi/acpi.hh>
#include <cpus/floating_point.hh>
#include <cpus/timer.hh>
#include <dtb/dtb.hh>
#include <interrupts/interrupts.hh>
#include <interrupts/plic.hh>
#include <kern_logger/kern_logger.hh>
#include <memory/malloc.hh>
#include <memory/vmm.hh>
#include <paging/riscv64_paging.hh>
#include <sched/sched.hh>
#include <smoldtb.h>
#include <types.hh>
#include <paging/arch_paging.hh>
#include <processes/tasks.hh>

using namespace kernel;

extern klib::shared_ptr<Arch_Page_Table> idle_page_table;

extern "C" void set_cpu_struct(CPU_Info *);

// Direct mode
// I find this more convenient, as it allows to save the context in a single
// assembly routine and then deal with interrupts in C++.
static const u8 STVEC_MODE = 0x0;

extern "C" void isr(void);
void program_stvec()
{
    const u64 stvel_val = (u64)isr | (u64)STVEC_MODE;
    asm volatile("csrw stvec, %0" : : "r"(stvel_val) : "memory");
}

void set_sscratch(u64 scratch) { asm volatile("csrw sscratch, %0" : : "r"(scratch) : "memory"); }

RCHT *get_rhct()
{
    static RCHT *rhct_virt = nullptr;
    static bool have_acpi  = true;

    if (rhct_virt == nullptr and have_acpi) {
        u64 rhct_phys = get_table(0x54434852); // RHCT
        if (rhct_phys == 0) {
            have_acpi = false;
            return nullptr;
        }

        ACPISDTHeader h;
        copy_from_phys(rhct_phys, &h, sizeof(h));

        rhct_virt = (RCHT *)malloc(h.length);
        copy_from_phys(rhct_phys, rhct_virt, h.length);
    }

    return rhct_virt;
}

void initialize_timer()
{
    if (timer_needs_initialization()) {
        u64 time_base_frequency = 0;

        do {
            RCHT *rhct = get_rhct();
            if (rhct != nullptr) {
                time_base_frequency = rhct->time_base_frequency;
            } else if (have_dtb()) {
                auto node = dtb_find("/cpus");
                if (!node) {
                    serial_logger.printf("Could not find /cpus DTB node\n");
                    continue;
                }

                auto prop = dtb_find_prop(node, "timebase-frequency");
                if (!prop) {
                    serial_logger.printf("Could not find timebase-frequency property\n");
                    continue;
                }

                size_t freq = 0;
                dtb_read_prop_values(prop, 1, &freq);
                time_base_frequency = freq;
            } else {
                serial_logger.printf("Could not get RHCT or DTB\n");
                break;
            }
        } while (false);

        if (time_base_frequency == 0) {
            serial_logger.printf("Could not get time base frequency\n");
            return;
        }

        global_logger.printf("[Kernel] Info: time base frequency: %i\n", time_base_frequency);
        serial_logger.printf("Info: time base frequency: %i\n", time_base_frequency);

        set_timer_frequency(time_base_frequency);
    }
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

MADT_RINTC_entry *acpi_get_hart_rtnic(u64 hart_id)
{
    MADT *m = get_madt();
    if (m == nullptr)
        return nullptr;

    // Find the RINTC
    u32 offset = sizeof(MADT);
    u32 length = m->header.length;
    while (offset < length) {
        MADT_RINTC_entry *e = (MADT_RINTC_entry *)((char *)m + offset);
        if (e->header.type == MADT_RINTC_ENTRY_TYPE and e->hart_id == hart_id)
            return e;

        offset += e->header.length;
    }

    return nullptr;
}

static dtb_node *find_cpu(u32 hart_id)
{
    auto node = dtb_find("/cpus");
    if (!node)
        return nullptr;

    auto child = dtb_get_child(node);
    while (child) {
        // TODO: Potentially, it might be necessary to check the compatible
        // property
        // TODO: Instead of iterating, cpu@<reg> should be used

        size_t reg = 0;
        auto prop  = dtb_find_prop(child, "reg");
        if (prop) {
            dtb_read_prop_values(prop, 1, &reg);
            if (reg == hart_id)
                return child;
        }

        child = dtb_get_sibling(child);
    }

    return nullptr;
}

// TODO: Move this to somewhere else...
dtb_node *dtb_get_plic_node()
{
    auto plic = dtb_find_compatible(nullptr, "sifive,plic-1.0.0");
    if (plic == nullptr)
        plic = dtb_find_compatible(nullptr, "riscv,plic0");

    return plic;
}

ReturnStr<u32> dtb_get_hart_rtnic_id(u64 hart_id)
{
    if (not have_dtb()) {
        serial_logger.printf("No DTB found\n");
        return {-ENOTSUP, 0};
    }

    auto cpu = find_cpu(hart_id);
    if (cpu == nullptr) {
        serial_logger.printf("Could not find CPU DTB node for hart id %i\n", hart_id);
        return {-ENOTSUP, 0};
    }

    auto intc = dtb_find_child(cpu, "interrupt-controller");
    if (intc == nullptr) {
        serial_logger.printf("Could not find interrupt-controller node for hart id %i\n", hart_id);
        return {-ENOTSUP, 0};
    }

    // I have no idea if I am doing it right, but from reading Linux source and
    // from what I could find this is what has to be done...
    size_t phandle = 0;
    auto prop      = dtb_find_prop(intc, "phandle");
    auto count     = dtb_read_prop_values(prop, 1, &phandle);
    if (count != 1) {
        serial_logger.printf("Could not read phandle property for interrupt-controller node\n");
        return {-ENOTSUP, 0};
    }

    auto plic = dtb_get_plic_node();

    if (plic == nullptr) {
        serial_logger.printf("Could not find PLIC node for hart id %i\n", hart_id);
        return {-ENOTSUP, 0};
    }

    prop = dtb_find_prop(plic, "#interrupt-cells");
    if (prop == nullptr) {
        serial_logger.printf("Could not find #interrupt-cells property for PLIC node\n");
        return {-ENOTSUP, 0};
    }

    size_t cells = 0;
    dtb_read_prop_values(prop, 1, &cells);
    if (cells != 1) {
        // cells > 1 might be right, but I have no idea what its meaning would
        // be
        serial_logger.printf("Unexpected #interrupt-cells value: %i (wanted 1)\n", cells);
        return {-ENOTSUP, 0};
    }

    prop = dtb_find_prop(plic, "interrupts-extended");
    if (prop == nullptr) {
        serial_logger.printf("Could not find interrupts-extended property for PLIC node\n");
        return {-ENOTSUP, 0};
    }

    count = dtb_read_prop_pairs(prop, {1, 1}, nullptr);
    klib::unique_ptr<dtb_pair[]> pairs(new dtb_pair[count]);
    dtb_read_prop_pairs(prop, {1, 1}, pairs.get());
    for (size_t i = 0; i < count; ++i) {
        if (pairs[i].a == phandle and pairs[i].b == IRQ_S_EXT)
            return {0, (u32)i};
    }

    return {-ENOTSUP, 0};
}

ReturnStr<u32> get_hart_rtnic_id(u64 hart_id)
{
    auto e = acpi_get_hart_rtnic(hart_id);
    if (e)
        return {0, e->external_interrupt_controller_id};

    return dtb_get_hart_rtnic_id(hart_id);
}

ReturnStr<u32> get_apic_processor_uid(u64 hart_id)
{
    auto e = acpi_get_hart_rtnic(hart_id);
    if (e)
        return {0, e->acpi_processor_id};

    return {-ENOTSUP, 0};
}

// TODO: Think about return type
ReturnStr<klib::string> acpi_get_isa_string(u64 hart_id)
{
    auto apic_id = get_apic_processor_uid(hart_id);
    if (apic_id.result != 0)
        return {apic_id.result, {}};

    RCHT *rhct = get_rhct();
    if (rhct == nullptr) {
        serial_logger.printf("Could not get rhct\n");
        return {-ENOTSUP, {}};
    }

    const u32 size = rhct->h.length;

    // Find the right hart info node
    u32 offset             = sizeof(RCHT);
    RCHT_HART_INFO_node *n = nullptr;
    while (offset < size) {
        RCHT_HART_INFO_node *t = (RCHT_HART_INFO_node *)((char *)rhct + offset);
        if (t->header.type == RCHT_HART_INFO_NODE and t->acpi_processor_uid == apic_id.val) {
            n = t;
            break;
        }

        offset += t->header.length;
    }

    if (n == nullptr) {
        serial_logger.printf("Could not find hart node info for ACPI processor UID %i\n",
                             apic_id.val);
        return {-ENOTSUP, {}};
    }

    // Find ISA string
    for (u16 i = 0; i < n->offsets_count; ++i) {
        RCHT_ISA_STRING_node *node = (RCHT_ISA_STRING_node *)((char *)rhct + n->offsets[i]);
        if (node->header.type == RHCT_ISA_STRING_NODE) {
            auto s = klib::string(node->string, node->string_length);
            return {0, klib::move(s)};
        }
    }

    serial_logger.printf("Could not find ISA string for ACPI processor UID %i\n", apic_id.val);
    return {-ENOTSUP, {}};
}

ReturnStr<klib::string> dtb_get_isa_string(u64 hart_id)
{
    if (not have_dtb())
        return {-ENOTSUP, {}};

    auto cpu = find_cpu(hart_id);
    if (cpu == nullptr) {
        serial_logger.printf("Could not find CPU DTB node for hart id %i\n", hart_id);
        return {-ENOTSUP, {}};
    }

    auto prop = dtb_find_prop(cpu, "riscv,isa");
    if (prop == nullptr) {
        serial_logger.printf("Could not find ISA string property for hart id %i\n", hart_id);
        return {-ENOTSUP, {}};
    }

    auto str = dtb_read_string(prop, 0);
    auto s = klib::string(str);
    return {0, klib::move(s)};
}

ReturnStr<klib::string> get_isa_string(u64 hart_id)
{
    ReturnStr<klib::string> ret;
    ret = acpi_get_isa_string(hart_id);
    if (ret.result == 0)
        return ret;

    ret = dtb_get_isa_string(hart_id);
    return ret;
}

void initialize_fp(const klib::string &isa_string)
{
    // Find the maximum supported floating point extension
    auto c                = isa_string.c_str();
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

    serial_logger.printf("[Kernel] Floating point register size: %i\n", fp_register_size(max));
    global_logger.printf("Floating point register size: %i\n", fp_register_size(max));
    max_supported_fp_level = max;
}

register CPU_Info *cpu_struct __asm__("tp");
void set_cpu_struct(CPU_Info *i)
{
    cpu_struct = i;
}

extern "C" CPU_Info *get_cpu_struct()
{
    return cpu_struct;
}

extern bool cpu_struct_works;

void init_scheduling(u64 hart_id)
{
    CPU_Info *i         = new CPU_Info();
    if (!i)
        panic("Could not allocate CPU_Info struct\n");

    i->kernel_stack_top = i->kernel_stack.get_stack_top();
    i->hart_id          = hart_id;
    void *temp_mapper_start = vmm::kernel_space_allocator.virtmem_alloc_aligned(16, 4);
    i->temp_mapper          = RISCV64_Temp_Mapper(temp_mapper_start, idle_page_table->get_root());

    set_cpu_struct(i);

    // The registers are either stored in TaskDescriptor of U-mode originating
    // interrupts, or in the CPU_Info structure Thusm sscratch always holds
    // CPU_Info struct, with pointers to both locations On interrupt entry, it
    // gets swapped with the userspace or old thread pointer, the registers get
    // saved, and the kernel one gets loaded back.
    set_sscratch((u64)i);

    cpu_struct_works = true;

    program_stvec();

    if (!cpus.push_back(i))
        panic("Could not add CPU_Info struct to cpus vector\n");

    initialize_timer();

    // Set ISA string
    // hart id 0 should always be present
    auto s = get_isa_string(hart_id);
    if (s.result == 0) {
        i->isa_string = klib::forward<klib::string>(s.val);
        global_logger.printf("[Kernel] ISA string: %s\n", i->isa_string.c_str());
        serial_logger.printf("ISA string: %s\n", i->isa_string.c_str());
    }

    // Set EIC ID
    auto e = get_hart_rtnic_id(hart_id);
    if (e.result != 0) {
        serial_logger.printf("Could not get EIC ID: %i\n", e.result);
    } else {
        i->eic_id = e.val;
        // global_logger.printf("[Kernel] EIC ID: %i\n", i->eic_id);
        // serial_logger.printf("EIC ID: %i\n", i->eic_id);
    }

    initialize_fp(i->isa_string);
    set_fp_state(FloatingPointState::Disabled);

    // Enable interrupts
    const u64 mask = (1 << TIMER_INTERRUPT) | (1 << EXTERNAL_INTERRUPT) | (1 << SOFTWARE_INTERRUPT);
    asm volatile("csrs sie, %0" : : "r"(mask) : "memory");

    serial_logger.printf("Initializing idle task\n");

    auto idle = init_idle(i);
    if (idle != 0)
        panic("Failed to initialize idle task: %i\n", idle);
    
    i->current_task = i->idle_task;
    i->idle_task->page_table->apply_cpu(i);

    serial_logger.printf("Initializing PLIC...\n");
    init_plic();

    // Enable all interrupts
    plic_set_threshold(0);

    serial_logger.printf("Scheduling initialized\n");
}

u64 satp_bootstrap_value = 0;

klib::vector<u64> initialize_cpus(const klib::vector<u64> &hartids)
{
    u64 satp;
    asm volatile("csrr %0, satp" : "=r"(satp));
    satp_bootstrap_value = satp;

    if (!cpus.reserve(hartids.size() + 1))
        panic("Failed to reserve memory for cpus vector in initialize_cpus()\n");

    klib::vector<u64> temp_vals;
    if (!temp_vals.reserve(hartids.size()))
        panic("Failed to reserve memory for temp_vals vector\n");

    for (auto hart_id: hartids) {
        CPU_Info *i         = new CPU_Info();
        if (!i)
            panic("Could not allocate CPU_Info struct\n");
        
        i->kernel_stack_top = i->kernel_stack.get_stack_top();
        i->hart_id          = hart_id;

        void *temp_mapper_start = vmm::kernel_space_allocator.virtmem_alloc_aligned(16, 4);
        i->temp_mapper = RISCV64_Temp_Mapper(temp_mapper_start, idle_page_table->get_root());

        cpus.push_back(i);
        i->cpu_id = cpus.size() - 1;

        auto s = get_isa_string(hart_id);
        if (s.result == 0) {
            i->isa_string = klib::forward<klib::string>(s.val);
            global_logger.printf("[Kernel] ISA string: %s\n", i->isa_string.c_str());
            serial_logger.printf("ISA string: %s\n", i->isa_string.c_str());
        }

        auto e = get_hart_rtnic_id(hart_id);
        if (e.result != 0) {
            serial_logger.printf("Could not get EIC ID: %i\n", e.result);
        } else {
            i->eic_id = e.val;
            // global_logger.printf("[Kernel] EIC ID: %i\n", i->eic_id);
            // serial_logger.printf("EIC ID: %i\n", i->eic_id);
        }

        auto idle = init_idle(i);
        if (idle != 0)
            panic("Failed to initialize idle task: %i\n", idle);
        
        i->current_task = i->idle_task;

        u64 *stack_top = (u64 *)i->kernel_stack.get_stack_top();
        stack_top[-1]  = (u64)i;

        temp_vals.push_back((u64)stack_top);
    }

    return temp_vals;
}

extern "C" void _cpu_bootstrap_entry(void *limine_data);

void *get_cpu_start_func() { return (void *)_cpu_bootstrap_entry; }

extern size_t booted_cpus;
extern bool boot_barrier_start;

extern "C" void bootstrap_entry(CPU_Info *i)
{
    set_cpu_struct(i);
    set_sscratch((u64)i);
    program_stvec();

    i->idle_task->page_table->apply_cpu(i);

    // Enable interrupts
    const u64 mask = (1 << TIMER_INTERRUPT) | (1 << EXTERNAL_INTERRUPT) | (1 << SOFTWARE_INTERRUPT);
    asm volatile("csrs sie, %0" : : "r"(mask) : "memory");

    plic_set_threshold(0);

    // Start the timer (otherwise, the CPU will likely idle forever)
    start_timer(10 /* ms */);
    reschedule();

    serial_logger.printf("CPU %i (hart %i) initialized!\n", i->cpu_id, i->hart_id);

    __atomic_add_fetch(&booted_cpus, 1, __ATOMIC_SEQ_CST);

    // Wait for bootstrap hart to do the final initialization
    while (!__atomic_load_n(&boot_barrier_start, __ATOMIC_SEQ_CST))
        ;

    serial_logger.printf("CPU %i (hart %i) entering userspace/idle...\n", i->cpu_id, i->hart_id);
}

bool TaskDescriptor::is_kernel_task() const
{
    return is_system;
}