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
#include <pmos/ipc.h>
#include <stdbool.h>
#include <pmos/tls.h>
#include <string.h>

struct task_list_node {
    struct task_list_node *next;
    struct multiboot_tag_module * mod_ptr;
    char * name;
    char * path;
    char * cmdline;
    void * file_virt_addr;
    uint64_t page_table;
    void * tls_virt;
    bool executable;
};


extern uint64_t loader_port;

#define alignup(size, alignment) (size%alignment ? size + (alignment - size%alignment) : size)

uint64_t load_elf(struct task_list_node* n, uint8_t ring)
{
    ELF_64bit *elf_h = n->file_virt_addr;
    if (elf_h->magic != 0x464c457f) {
        print_str("Error: not elf format!\n");
        return 1;
    }


    ELF_PHeader_64 * elf_pheader = (ELF_PHeader_64 *)((uint64_t)elf_h + elf_h->program_header);
    int elf_pheader_entries = elf_h->program_header_entries;

    // get a PID
    syscall_r r = syscall_new_process(ring);
    if (r.result != SUCCESS) {
        print_str("Syscall failed!\n");
        return 1;
    }
    uint64_t pid = r.value;

    n->page_table = asign_page_table(pid, 0, PAGE_TABLE_CREATE).page_table;

    for (int i = 0; i < elf_pheader_entries; ++i) {
        ELF_PHeader_64 * p = &elf_pheader[i];
        if (p->type == ELF_SEGMENT_LOAD) {
            uint64_t phys_loc = (uint64_t)elf_h + p->p_offset;

            uint64_t vaddr = p->p_vaddr;
            uint64_t vaddr_all = vaddr & ~0xfffUL;

            uint64_t start_offset = vaddr - vaddr_all;
            
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

            mem_request_ret_t req = create_normal_region(0, NULL, vaddr_end - vaddr_all, 1 | 2);
            if (req.result != SUCCESS) {
                print_hex(r.result);
                print_str(" !!!\n");
                asm("xchgw %bx, %bx");
            }

            memcpy(((char *)req.virt_addr) + start_offset, phys_loc, size);

            req = transfer_region(n->page_table, req.virt_addr, (void *)vaddr_all, mask | 0x08);
            if (req.result != SUCCESS) {
                print_str("Error: Failed to transfer page region! ");
                print_hex(r.result);
                print_str(" !!!\n");
                asm("xchgw %bx, %bx");
            }
        } else if (p->type == PT_TLS) {
            if (n->tls_virt)
                print_str("Warning: Duplicate tls!\n");

            uint64_t size = sizeof(TLS_Data) + p->p_filesz;
            size = alignup(size, 4096);

            mem_request_ret_t req = create_normal_region(0, 0, size, 1 | 2);

            TLS_Data * d = req.virt_addr;
            d->memsz = p->p_memsz;
            d->filesz = p->p_filesz;
            d->align = p->allignment;   
            memcpy(d->data, (void*)((uint64_t)elf_h + p->p_offset), p->p_filesz);   

            req = transfer_region(n->page_table, req.virt_addr, NULL, 1);
            if (req.result != SUCCESS) {
                print_str("Error: Failed to transfer tls! ");
                print_hex(r.result);
                print_str(" !!!\n");
            }
            n->tls_virt = req.virt_addr;
        }
    }

    syscall(SYSCALL_INIT_STACK, pid, 0);

    return pid;
}

struct task_list_node *get_by_page_table(uint64_t page_table);
