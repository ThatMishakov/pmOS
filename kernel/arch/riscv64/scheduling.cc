#include <sched/sched.hh>
#include <memory/mem.hh>
#include <memory/virtmem.hh>
#include <paging/riscv64_paging.hh>
#include <types.hh>

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

void init_scheduling() {
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


}