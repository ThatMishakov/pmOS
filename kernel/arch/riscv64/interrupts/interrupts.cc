#include <kern_logger/kern_logger.hh>
#include <cpus/csr.hh>
#include <sched/sched.hh>
#include "interrupts.hh"

extern "C" void handle_interrupt();

struct fp_s {
    u64 fp;
    u64 ra;
};

u64 get_fp()
{
    u64 fp;
    asm volatile("add %0, x0, fp" : "=r"(fp));
    return fp;
}

extern "C" void print_stack_trace()
{
    u64 fp = get_fp();
    fp_s *current = (fp_s*)fp - 1;
    serial_logger.printf("Stack trace:\n");
    while (1) {
        serial_logger.printf("  0x%x fp 0x%x\n", current->ra, current->fp);
        if (current->fp == 0 or ((i64)current->fp > 0)) {
            break;
        }
        current = (fp_s*)current->fp - 1;    
    }
}

void page_fault(u64 virt_addr, u64 scause)
{
    const u64 page = virt_addr & ~0xfffUL;

    u64 access_type = 0;
    switch (scause) {
    case INSTRUCTION_ACCESS_FAULT:
        access_type = Generic_Mem_Region::Executable;
        break;
    case LOAD_PAGE_FAULT:
        access_type = Generic_Mem_Region::Readable;
        break;
    case STORE_AMO_PAGE_FAULT:
        access_type = Generic_Mem_Region::Writeable;
        break;
    }

    auto task = get_cpu_struct()->current_task;
    auto page_table = task->page_table;

    try {
        Auto_Lock_Scope lock(page_table->lock);

        auto& regions = page_table->paging_regions;
        const auto it = regions.get_smaller_or_equal(page);
        if (it != regions.end() and it->second->is_in_range(virt_addr)) {
            serial_logger.printf("Pagefault in region %s\n", it->second->name.c_str());
            auto r = it->second->on_page_fault(access_type, virt_addr);
            if (not r)
                task->atomic_block_by_page(page, &task->page_table->blocked_tasks);
        } else
            throw Kern_Exception(ERROR_PAGE_NOT_ALLOCATED, "pagefault in unknown region");
    } catch (const Kern_Exception& e) {
        serial_logger.printf("Warning: Pagefault %h pid %i (%s) rip %h error %h -> %i killing process...\n", virt_addr, task->pid, task->name.c_str(), task->regs.pc, scause, e.err_code);
        task->atomic_kill();
    }
}

void handle_interrupt()
{
    u64 scause, stval;
    get_scause_stval(&scause, &stval);

    serial_logger.printf("Recieved an interrupt! scause: 0x%x stval: 0x%x pc 0x%x\n", scause, stval, get_cpu_struct()->current_task->regs.program_counter());

    auto c = get_cpu_struct();
    if (c->nested_level > 1) {
        serial_logger.printf("!!! kernel interrupt !!!\n");
        print_stack_trace();
        while (1) ;
    }

    if ((i64)scause < 0) {
        serial_logger.printf("interrupt!\n");
        while (1) ;
    } else {
        switch (scause) {
            case INSTRUCTION_PAGE_FAULT:
            case LOAD_PAGE_FAULT:
            case STORE_AMO_PAGE_FAULT:
                page_fault(stval, scause);
                break;
            default:
                serial_logger.printf("Unknown interrupt: 0x%x\n", scause);
                while (1) ;
        }
    }
}