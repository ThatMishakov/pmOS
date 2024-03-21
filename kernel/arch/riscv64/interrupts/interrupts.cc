#include <kern_logger/kern_logger.hh>
#include <cpus/csr.hh>
#include <sched/sched.hh>
#include "interrupts.hh"
#include <assert.h>
#include <utils.hh>
#include <cpus/csr.hh>
#include <cpus/floating_point.hh>

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
            //serial_logger.printf("Pagefault in region %s\n", it->second->name.c_str());
            auto r = it->second->on_page_fault(access_type, virt_addr);
            if (not r)
                task->atomic_block_by_page(page, &task->page_table->blocked_tasks);
        } else
            throw Kern_Exception(ERROR_PAGE_NOT_ALLOCATED, "pagefault in unknown region");
    } catch (const Kern_Exception& e) {
        serial_logger.printf("Warning: Pagefault %h pid %i (%s) rip %h error %h -> %i killing process...\n", virt_addr, task->pid, task->name.c_str(), task->regs.pc, scause, e.err_code);
        global_logger.printf("Warning: Pagefault %h pid %i (%s) pc %h error %h -> %i killing process...\n", virt_addr, task->pid, task->name.c_str(), task->regs.pc, scause, e.err_code);
        task->atomic_kill();
    }
}

bool instruction_is_csr(u32 instruction)
{
    // SYSTEM
    return (instruction & 0x7f) == 0x73
        and ((instruction >> 12) & 0x3) != 0x0;
}
bool instruction_is_fp_op(u32 instruction)
{
    // MADD, MSUB, NMSUB, NMADD and OP-FP
    return (instruction & 0x63) == 0x43 and (instruction & 0x1c) != 0x18 and (instruction & 0x1c) != 0x14;
}
bool instruction_is_fp_ld_st(u32 instruction)
{
    // C extension encoding (16 bit)
    bool compressed = ((instruction&0x3) == 0b00 or (instruction&0x3) == 0b10) and ((instruction & 0x6000) == 0x2000);
    // 32 bit encoding
    bool full = (instruction & 0x7f) == 0x07 or (instruction & 0x7f) == 0x27;
    return compressed or full;
}
bool instruction_is_fp_csr(u32 instruction)
{
    u32 csr_imm = instruction >> 20;
    return instruction_is_csr(instruction) and
        (csr_imm == FFLAGS
        or csr_imm == FRM
        or csr_imm == FCSR);
}


void illegal_instruction(u32 instruction)
{
    auto task = get_cpu_struct()->current_task;

    try {
        if (instruction == 0) {
            bool b = copy_from_user((char *)&instruction, (char *)task->regs.pc, sizeof(instruction));
            if (not b) {
                task->atomic_block_by_page(task->regs.pc, &task->page_table->blocked_tasks);
                return;
            }
        }

        if (instruction_is_fp_csr(instruction)) {
            if (not fp_is_supported())
                throw Kern_Exception(ERROR_INSTRUCTION_UNAVAILABLE, "Floating point not supported");
            
            restore_fp_state(task.get());
        } else if (instruction_is_fp_op(instruction) or instruction_is_fp_ld_st(instruction)) {
            if (not fp_is_supported())
                throw Kern_Exception(ERROR_INSTRUCTION_UNAVAILABLE, "Floating point not supported");

            if (get_fp_state() != FloatingPointState::Disabled) // FP is enabled but instruction is illegal -> not supported
                throw Kern_Exception(ERROR_INSTRUCTION_UNAVAILABLE, "Floating point width not supported");

            restore_fp_state(task.get());
        } else {
            // What is this
            throw Kern_Exception(ERROR_BAD_INSTRUCTION, "Illegal instruction");
        }
    } catch (const Kern_Exception& e) {
        serial_logger.printf("Warning: %s pid %i (%s) pc %h instr 0x%h -> %i killing process...\n", e.err_message, task->pid, task->name.c_str(), task->regs.pc, instruction, e.err_code);
        global_logger.printf("Warning: %s pid %i (%s) pc %h instr 0x%h -> %i killing process...\n", e.err_message, task->pid, task->name.c_str(), task->regs.pc, instruction, e.err_code);
        task->atomic_kill();
    }
}

extern "C" void syscall_handler();

void service_timer_interrupt()
{
    sched_periodic();
    // Pending timer interrupt is cleared by SBI
}

void handle_interrupt()
{
    u64 scause, stval;
    get_scause_stval(&scause, &stval);

    //serial_logger.printf("Recieved an interrupt! scause: 0x%x stval: 0x%x pc 0x%x task %i (%s)\n", scause, stval, get_cpu_struct()->current_task->regs.program_counter(), get_cpu_struct()->current_task->pid, get_cpu_struct()->current_task->name.c_str());

    auto c = get_cpu_struct();
    if (c->nested_level > 1) {
        serial_logger.printf("!!! kernel interrupt !!!\n");
        serial_logger.printf("nested level: %i\n", c->nested_level);
        serial_logger.printf("scause: 0x%x\n", scause);
        serial_logger.printf("stval: 0x%x\n", stval);
        serial_logger.printf("pc: 0x%x\n", c->current_task->regs.program_counter());
        print_stack_trace();
        while (1) ;
    }

    if ((i64)scause < 0) {
        // Clear last bit
        u64 intno = scause & ~(1UL << 63);
        switch (intno)
        {
        case TIMER_INTERRUPT:
            service_timer_interrupt();
            break;
        
        default:
            serial_logger.printf("Unknown interrupt: 0x%x\n", scause);
            while (1) ;
            break;
        }
    } else {
        switch (scause) {
            case ILLEGAL_INSTRUCTION:
                illegal_instruction(stval);
                break;
            case INSTRUCTION_PAGE_FAULT:
            case LOAD_PAGE_FAULT:
            case STORE_AMO_PAGE_FAULT:
                page_fault(stval, scause);
                break;
            case ENV_CALL_FROM_U_MODE:
                // Advance the program counter to the next instruction
                c->current_task->regs.program_counter() += 4;
                syscall_handler();
                break;
            default:
                serial_logger.printf("Unknown interrupt: 0x%x\n", scause);
                while (1) ;
        }
    }

    while (c->current_task->regs.syscall_restart != 0) {
        syscall_handler();
    }

    assert(c->nested_level == 1);
}