#include "gdt.hh"

#include <kern_logger/kern_logger.hh>
#include <types.hh>
#include <x86_asm.hh>
#include <sched/sched.hh>
#include <processes/tasks.hh>

extern "C" void double_fault_handler()
{
    serial_logger.printf("Double fault!!!\n");
    auto tss_selector = str();
    TSS *tss          = getTSS(tss_selector);
    assert(tss);
    TSS *faulting_tss = getTSS(tss->link);
    assert(faulting_tss);

    serial_logger.printf("Faulting TSS: %u\n", tss->link);
    serial_logger.printf("Registers:\n");
    serial_logger.printf("EAX: 0x%x (%u)\n", faulting_tss->eax, faulting_tss->eax);
    serial_logger.printf("EBX: 0x%x (%u)\n", faulting_tss->ebx, faulting_tss->ebx);
    serial_logger.printf("ECX: 0x%x (%u)\n", faulting_tss->ecx, faulting_tss->ecx);
    serial_logger.printf("EDX: 0x%x (%u)\n", faulting_tss->edx, faulting_tss->edx);
    serial_logger.printf("ESI: 0x%x (%u)\n", faulting_tss->esi, faulting_tss->esi);
    serial_logger.printf("EDI: 0x%x (%u)\n", faulting_tss->edi, faulting_tss->edi);
    serial_logger.printf("ESP: 0x%x (%u)\n", faulting_tss->esp, faulting_tss->esp);
    serial_logger.printf("EIP: 0x%x (%u)\n", faulting_tss->eip, faulting_tss->eip);
    serial_logger.printf("CS:  0x%x (%u)\n", faulting_tss->cs, faulting_tss->cs);
    serial_logger.printf("SS:  0x%x (%u)\n", faulting_tss->ss, faulting_tss->ss);
    serial_logger.printf("ES:  0x%x (%u)\n", faulting_tss->es, faulting_tss->es);
    serial_logger.printf("DS:  0x%x (%u)\n", faulting_tss->ds, faulting_tss->ds);
    serial_logger.printf("FS:  0x%x (%u)\n", faulting_tss->fs, faulting_tss->fs);
    serial_logger.printf("GS:  0x%x (%u)\n", faulting_tss->gs, faulting_tss->gs);
    serial_logger.printf("ldt: 0x%x (%u)\n", faulting_tss->ldt, faulting_tss->ldt);
    serial_logger.printf("cr3: 0x%x (%u)\n", faulting_tss->cr3, faulting_tss->cr3);

    panic("panicking idk...");
}

struct kernel_registers_context {
    u32 eax;
    u32 ecx;
    u32 edx;
    u32 ebx;
    u32 ebp;
    u32 esi;
    u32 edi;
    u32 error_code;
    u32 eip;
    u32 cs;
    u32 eflags;
};

extern "C" void page_fault_handler(kernel_registers_context *ctx, u32 err)
{
    if (ctx) {
        panic("Kernel pagefault");
    }

    CPU_Info *c          = get_cpu_struct();
    TaskDescriptor *task = c->current_task;

    auto virtual_addr = getCR2();

    serial_logger.printf("Pagefault task %li addr %x error %x\n", task->task_id, virtual_addr, err);

    auto result = [&]() -> kresult_t {
        if (virtual_addr >= task->page_table->user_addr_max())
            return -EFAULT;

        Auto_Lock_Scope scope_lock(task->page_table->lock);

        auto &regions       = task->page_table->paging_regions;
        const auto it       = regions.get_smaller_or_equal(virtual_addr);
        const auto addr_all = virtual_addr & ~07777;

        ulong access_mask = (err & 0x002)       ? Generic_Mem_Region::Writeable
                            : 0 | (err & 0x010) ? Generic_Mem_Region::Executable
                                                : 0;

        if (it != regions.end() and it->is_in_range(virtual_addr)) {
            auto r = it->on_page_fault(access_mask, virtual_addr);
            if (!r.success())
                return r.result;
            if (!r.val)
                task->atomic_block_by_page(addr_all, &task->page_table->blocked_tasks);

            return 0;
        }
        return -EFAULT;
    }();

    if (result) {
        t_print_bochs("Debug: Pagefault %h pid %i rip %h error %h returned "
                      "error %i\n",
                      virtual_addr, task->task_id, task->regs.program_counter(), err, result);
        global_logger.printf("Warning: Pagefault %h pid %i (%s) rip %h error "
                             "%h -> %i killing process...\n",
                             virtual_addr, task->task_id, task->name.c_str(),
                             task->regs.program_counter(), err, result);

        //print_registers(task, global_logger);
        // print_stack_trace(task, global_logger);

        task->atomic_kill();
    }
}