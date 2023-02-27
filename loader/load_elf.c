#include <load_elf.h>
#include <syscall.h>
#include <kernel/elf.h>
#include <kernel/errors.h>
#include <utils.h>
#include <io.h>
#include <misc.h>
#include <mem.h>
#include <paging.h>
#include <pmos/memory.h>

struct task_list_node {
    struct task_list_node *next;
    struct multiboot_tag_module * mod_ptr;
    ELF_64bit* elf_virt_addr;
    uint64_t page_table;
};

extern uint64_t loader_port;

uint64_t load_elf(struct task_list_node* n, uint8_t ring)
{
    ELF_64bit *elf_h = n->elf_virt_addr;
    if (elf_h->magic != 0x464c457f) {
        print_str("Error: not elf format!\n");
        return 1;
    }


    ELF_PHeader_64 * elf_pheader = (ELF_PHeader_64 *)((uint64_t)elf_h + elf_h->program_header);
    int elf_pheader_entries = elf_h->program_header_entries;

    static const uint64_t memory = 0x800000000;

    // get a PID
    syscall_r r = syscall_new_process(ring);
    if (r.result != SUCCESS) {
        print_str("Syscall failed!\n");
        return 1;
    }
    uint64_t pid = r.value;

    n->page_table = get_page_table(pid).page_table;

    for (int i = 0; i < elf_pheader_entries; ++i) {
        ELF_PHeader_64 * p = &elf_pheader[i];
        if (p->type == ELF_SEGMENT_LOAD) {
            uint64_t phys_loc = (uint64_t)elf_h + p->p_offset;
            uint64_t vaddr = p->p_vaddr;
            uint64_t vaddr_all = vaddr & ~0xfffUL;
            uint64_t size = p->p_filesz;
            uint64_t memsz = p->p_memsz;
            uint64_t size_all = memsz + (vaddr - vaddr_all);
            //uint64_t pages = (memsz >> 12) + ((memsz) & 0xfff ? 1 : 0);
            uint64_t vaddr_end = vaddr + memsz;
                     vaddr_end = (vaddr_end & ~0xfffUL) + (vaddr_end & 0xfff ? 0x1000 : 0);
            uint64_t pages = (vaddr_end - vaddr_all) >> 12;

            char readable = !!(p->flags & ELF_FLAG_READABLE);
            char writeable = !!(p->flags & ELF_FLAG_WRITABLE);
            char executable = !!(p->flags & ELF_FLAG_EXECUTABLE);
            uint64_t mask = (writeable << 1) | (executable << 2) | (readable << 0);
            mem_request_ret_t req = create_managed_region(pid, (void *)vaddr_all, vaddr_end - vaddr_all, mask | 0x08, loader_port);
            if (req.result != SUCCESS) {
                asm("xchgw %bx, %bx");
                print_hex(r.result);
                print_str(" !!!\n");
            }
        }
    }

    syscall(SYSCALL_INIT_STACK, pid, 0);

    return pid;
}
