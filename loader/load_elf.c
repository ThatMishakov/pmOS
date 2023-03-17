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
    ELF_64bit* elf_virt_addr;
    uint64_t page_table;
    void * tls_virt;
};

extern uint64_t loader_port;

#define alignup(size, alignment) (size%alignment ? size + (alignment - size%alignment) : size)

uint64_t load_elf(struct task_list_node* n, uint8_t ring)
{
    ELF_64bit *elf_h = n->elf_virt_addr;
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
            n->tls_virt = req.virt_addr;
        }
    }

    syscall(SYSCALL_INIT_STACK, pid, 0);

    return pid;
}

struct task_list_node *get_by_page_table(uint64_t page_table);

void *scratch_page = NULL;

void react_alloc_page(IPC_Kernel_Alloc_Page *req)
{
    struct task_list_node *n = get_by_page_table(req->page_table_id);

    if (n == NULL)
        return;

    ELF_64bit *elf_h = n->elf_virt_addr;

    ELF_PHeader_64 * elf_pheader = (ELF_PHeader_64 *)((uint64_t)elf_h + elf_h->program_header);
    int elf_pheader_entries = elf_h->program_header_entries;

    ELF_PHeader_64 * entry = NULL;
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

            if (req->page_addr >= vaddr_all && req->page_addr < vaddr_end) {
                entry = p;
                break;
            }
                
        }
    }

    if (entry == NULL)
        return;

    uint64_t phys_loc = (uint64_t)elf_h + entry->p_offset;
    uint64_t vaddr = entry->p_vaddr;
    uint64_t vaddr_all = vaddr & ~0xfffUL;
    uint64_t size = entry->p_filesz;
    uint64_t memsz = entry->p_memsz;
    uint64_t size_all = memsz + (vaddr - vaddr_all);
    //uint64_t pages = (memsz >> 12) + ((memsz) & 0xfff ? 1 : 0);
    uint64_t vaddr_end = vaddr + memsz;
                vaddr_end = (vaddr_end & ~0xfffUL) + (vaddr_end & 0xfff ? 0x1000 : 0);
    uint64_t pages = (vaddr_end - vaddr_all) >> 12;

    if (scratch_page == NULL) {
        mem_request_ret_t  r = create_normal_region(0, 0, 4096, PROT_READ | PROT_WRITE);
        if (r.result != SUCCESS) {
            print_str("Loader: could not request a scratch page. Error: ");
            print_hex(r.result);
            print_str("\n");
            return;
        }
        scratch_page = r.virt_addr;
    }

    uint64_t page_end = req->page_addr + 4096;

    uint64_t addr_start = (vaddr > req->page_addr ? vaddr : req->page_addr);
    void *dest_ptr = (void * )(addr_start - req->page_addr + (uint64_t)scratch_page);
    void *source_ptr = (void *)(addr_start - vaddr + phys_loc);

    uint64_t file_end = vaddr + size;
    uint64_t copy_end = page_end > file_end ? file_end : page_end;
    uint64_t copy_size = copy_end > addr_start ? copy_end - addr_start : 0;

    memcpy(dest_ptr, source_ptr, copy_size);

    result_t result = provide_page(req->page_table_id, req->page_addr, (uint64_t)scratch_page, 0);
    if (result != SUCCESS) {
        print_str("Loader: could not provide a page. Error: ");
        print_hex(result);
        print_str("\n");
    }
}
