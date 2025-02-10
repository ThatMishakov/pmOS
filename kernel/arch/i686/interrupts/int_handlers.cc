#include "gdt.hh"

#include <kern_logger/kern_logger.hh>
#include <paging/x86_paging.hh>
#include <processes/tasks.hh>
#include <sched/sched.hh>
#include <types.hh>
#include <x86_asm.hh>

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
    // Order of the (improvised) pushal
    u32 edi;
    u32 esi;
    u32 ebp;
    u32 esp;
    u32 ebx;
    u32 edx;
    u32 ecx;
    u32 eax;

    u32 eip;
    u32 cs;
    u32 eflags;
};

static void print_kernel_registers(kernel_registers_context *c, u32 error_code)
{
    serial_logger.printf("Kernel registers:\n");
    serial_logger.printf("  %eax: 0x%x\n"
                         "  %ebx: 0x%x\n"
                         "  %ecx: 0x%x\n"
                         "  %edx: 0x%x\n"
                         "  %esp: 0x%x\n"
                         "  %ebp: 0x%x\n"
                         "  %esi: 0x%x\n"
                         "  %edi: 0x%x\n"
                         "  error_code: 0x%x\n"
                         "  eip: 0x%x\n"
                         "  cs: 0x%x\n"
                         "  eflags: 0x%x\n",
                         c->eax, c->ebx, c->ecx + 16, c->edx, c->esp, c->ebp, c->esi, c->edi,
                         error_code, c->eip, c->cs, c->eflags);
}

extern bool use_pae;
extern u32 idle_cr3;

extern bool page_mapped(void *pagefault_cr2, ulong err);

struct stack_frame;
void print_stack_trace(Logger &logger, stack_frame *s);

extern "C" void page_fault_handler(kernel_registers_context *ctx, u32 err)
{
    auto virtual_addr = (void *)getCR2();

    if (ctx) {
        // attempt to resolve it
        if (!use_pae) {
            Temp_Mapper_Obj<u32> idle_mapper(request_temp_mapper());
            u32 *idle_pd = idle_mapper.map(idle_cr3 & 0xfffff000);
            Temp_Mapper_Obj<u32> cr3_mapper(request_temp_mapper());
            u32 *cr3_pd = cr3_mapper.map(getCR3() & 0xfffff000);

            auto idx = (ulong)virtual_addr >> 22;
            if ((idle_pd[idx] & PAGE_PRESENT) and not(cr3_pd[idx] & PAGE_PRESENT)) {
                cr3_pd[idx] = idle_pd[idx];
                return;
            }

            if (page_mapped((void *)virtual_addr, err))
                return;
        }

        CPU_Info *c = get_cpu_struct();
        if (c->jumpto_func) {
            // This pagefault is normal...
            ctx->eip = (u32)c->jumpto_func;
            ctx->eax = c->jumpto_arg;
            ctx->ecx = (ulong)virtual_addr;
            return;
        }

        serial_logger.printf("Kernel page fault at 0x%x\n", virtual_addr);
        print_kernel_registers(ctx, err);

        print_stack_trace(serial_logger, (stack_frame *)ctx->ebp);

        panic("Kernel pagefault");
    }

    CPU_Info *c          = get_cpu_struct();
    TaskDescriptor *task = c->current_task;

    auto result = [&]() -> kresult_t {
        if (virtual_addr >= task->page_table->user_addr_max())
            return -EFAULT;

        Auto_Lock_Scope scope_lock(task->page_table->lock);

        auto &regions       = task->page_table->paging_regions;
        const auto it       = regions.get_smaller_or_equal(virtual_addr);
        const auto addr_all = (void *)((ulong)virtual_addr & ~07777);

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
        serial_logger.printf("Debug: Pagefault %x pid %li (%s) rip %x error %x returned "
                             "error %li\n",
                             virtual_addr, task->task_id, task->name.c_str(),
                             task->regs.program_counter(), err, result);
        global_logger.printf("Warning: Pagefault %h pid %li (%s) rip %x error "
                             "%x -> %i killing process...\n",
                             virtual_addr, task->task_id, task->name.c_str(),
                             task->regs.program_counter(), err, result);

        // print_registers(task, global_logger);
        //  print_stack_trace(task, global_logger);

        task->atomic_kill();
    }
}

extern "C" void general_protection_fault_handler(kernel_registers_context *ctx, u32 err)
{
    if (ctx) {
        print_kernel_registers(ctx, err);
        panic("Kernel general protection fault error 0x%x", err);
    }

    auto task = get_cpu_struct()->current_task;
    serial_logger.printf("!!! General Protection Fault (GP) error (segment) 0x%x "
                         "PID %li (%s) RIP %h CS %h... Killing the process\n",
                         err, task->task_id, task->name.c_str(), task->regs.program_counter(),
                         task->regs.cs);
    // print_registers(get_cpu_struct()->current_task, serial_logger);
    global_logger.printf("!!! General Protection Fault (GP) error (segment) 0x%x "
                         "PID %li (%s) RIP %h CS %h... Killing the process\n",
                         err, task->task_id, task->name.c_str(), task->regs.program_counter(),
                         task->regs.cs);
    task->atomic_kill();
}

extern "C" void sse_exception_manager()
{
    validate_sse();
    get_cpu_struct()->current_task->sse_data.restore_sse();
}