#include <multiboot2.h>
#include <stdint.h>
#include <misc.h>
#include <io.h>
#include <linker.h>
#include <mem.h>
#include <entry.h>
#include <kernel_loader.h>
#include <syscall.h>
#include <kernel/syscalls.h>
#include <kernel/errors.h>
#include <kernel/block.h>
#include <kernel/messaging.h>
#include <load_elf.h>
#include <paging.h>
#include <syscall.h>
#include <utils.h>
#include <acpi.h>
#include <args.h>

uint64_t multiboot_magic;
uint64_t multiboot_info_str;

void main()
{
    if (multiboot_magic != MULTIBOOT2_BOOTLOADER_MAGIC) {
        print_str("Not booted by multiboot2 bootloader\n");
        while (1) ;
    }

    Page_Table_Argumments pta;
    pta.writeable = 1;
    pta.user_access = 1;
    pta.execution_disabled = 1;
    pta.extra = 0b010;

    /* Initialize everything */
    print_str("Hello from loader!\n");

    print_str("Info: nx_bit ");
    print_hex(nx_enabled);
    print_str("\n");

    // Map multiboot structure into the kernel
    uint64_t multiboot_str_all = multiboot_info_str & ~(uint64_t)0xfff;

    map(multiboot_str_all, multiboot_str_all, pta);
    
    init_mem(multiboot_info_str);

    //print_str("Memory bitmap initialized!\n");

    load_kernel(multiboot_info_str);

    set_print_syscalls();

    init_acpi(multiboot_info_str);

    //start_cpus();

    print_str("Loading modules...\n");

    uint64_t terminal_pid = 0;
    for (struct multiboot_tag * tag = (struct multiboot_tag *) (multiboot_info_str + 8); tag->type != MULTIBOOT_TAG_TYPE_END;
        tag = (struct multiboot_tag *) ((multiboot_uint8_t *) tag + ((tag->size + 7) & ~7))) {
            if (tag->type == MULTIBOOT_TAG_TYPE_MODULE) {
                if (str_starts_with(((struct multiboot_tag_module *)tag)->cmdline, "terminald")) {
                    struct multiboot_tag_module * mod = (struct multiboot_tag_module *)tag;
                    print_str(" --> loading ");
                    print_str(mod->cmdline);
                    print_str("\n");
                    static uint64_t virt_addr = 549755813888;
                    uint64_t phys_start = (uint64_t)mod->mod_start & ~(uint64_t)0xfff;
                    uint64_t phys_end = (uint64_t)mod->mod_end;
                    uint64_t nb_pages = (phys_end - phys_start) >> 12;
                    if (phys_end & 0xfff) nb_pages += 1;
                    map_phys(virt_addr, phys_start, nb_pages, 0x3);
                    ELF_64bit* e = (ELF_64bit*)((uint64_t)mod->mod_start - phys_start + virt_addr);
                    uint64_t pid = load_elf(e, 3);
                    terminal_pid = pid;
                    syscall(SYSCALL_SET_ATTR, 1, 1);
                    start_process(pid, e->program_entry, 0, 0, 0);
                }
            }
        }

    syscall(SYSCALL_SET_PORT_DEFAULT, 1, terminal_pid, 1);

    for (struct multiboot_tag * tag = (struct multiboot_tag *) (multiboot_info_str + 8); tag->type != MULTIBOOT_TAG_TYPE_END;
        tag = (struct multiboot_tag *) ((multiboot_uint8_t *) tag + ((tag->size + 7) & ~7))) {
            if (tag->type == MULTIBOOT_TAG_TYPE_MODULE) {
                if (str_starts_with(((struct multiboot_tag_module *)tag)->cmdline, "processd")) {
                    struct multiboot_tag_module * mod = (struct multiboot_tag_module *)tag;
                    print_str(" --> loading ");
                    print_str(mod->cmdline);
                    print_str("\n");
                    static uint64_t virt_addr = 549755813888 + 0x1000000;
                    uint64_t phys_start = (uint64_t)mod->mod_start & ~(uint64_t)0xfff;
                    uint64_t phys_end = (uint64_t)mod->mod_end;
                    uint64_t nb_pages = (phys_end - phys_start) >> 12;
                    if (phys_end & 0xfff) nb_pages += 1;
                    map_phys(virt_addr, phys_start, nb_pages, 0x3);
                    ELF_64bit* e = (ELF_64bit*)((uint64_t)mod->mod_start - phys_start + virt_addr);
                    uint64_t pid = load_elf(e, 3);
                    start_process(pid, e->program_entry, 0, 0, 0);
                }
            }
        }

    for (struct multiboot_tag * tag = (struct multiboot_tag *) (multiboot_info_str + 8); tag->type != MULTIBOOT_TAG_TYPE_END;
        tag = (struct multiboot_tag *) ((multiboot_uint8_t *) tag + ((tag->size + 7) & ~7))) {
            if (tag->type == MULTIBOOT_TAG_TYPE_MODULE) {
                if (str_starts_with(((struct multiboot_tag_module *)tag)->cmdline, "devicesd")) {
                    struct multiboot_tag_module * mod = (struct multiboot_tag_module *)tag;
                    print_str(" --> loading ");
                    print_str(mod->cmdline);
                    print_str("\n");
                    static uint64_t virt_addr = 549755813888 + 0x2000000;
                    uint64_t phys_start = (uint64_t)mod->mod_start & ~(uint64_t)0xfff;
                    uint64_t phys_end = (uint64_t)mod->mod_end;
                    uint64_t nb_pages = (phys_end - phys_start) >> 12;
                    if (phys_end & 0xfff) nb_pages += 1;
                    map_phys(virt_addr, phys_start, nb_pages, 0x3);
                    ELF_64bit* e = (ELF_64bit*)((uint64_t)mod->mod_start - phys_start + virt_addr);
                    uint64_t pid = load_elf(e, 3);

                    get_page_multi(virt_addr + 0x300000, 1);
                    uint64_t virt_page = TB(2);
                    Args_List_Header *a = (Args_List_Header*)(virt_addr + 0x300000);
                    a->size = 0;
                    a->flags = 0x01;
                    a->pages = 1;
                    a->p = 0;

                    push_arg(a, "devicesd");
                    if (rsdp_desc != 0) {
                        char buff[10];
                        uint_to_string(rsdp_desc, 10, buff);

                        push_arg(a, "--rsdp-desc");
                        push_arg(a, buff);
                    } 

                    if (rsdp20_desc != 0) {
                        char buff[10];
                        uint_to_string(rsdp20_desc, 10, buff);

                        push_arg(a, "--rsdp20-desc");
                        push_arg(a, buff);
                    } 


                    syscall_r r = map_into_range(pid, virt_addr + 0x300000, virt_page, 1, 0x01);
                    if (r.result != SUCCESS) {
                        asm("xchgw %bx, %bx");
                        print_hex(r.result);
                        print_str(" !!!\n");
                        halt();
                    }

                    start_process(pid, e->program_entry, 0x01, virt_page, 0);
                }
            }
        }

    
    for (struct multiboot_tag * tag = (struct multiboot_tag *) (multiboot_info_str + 8); tag->type != MULTIBOOT_TAG_TYPE_END;
        tag = (struct multiboot_tag *) ((multiboot_uint8_t *) tag + ((tag->size + 7) & ~7))) {
            if (tag->type == MULTIBOOT_TAG_TYPE_MODULE) {
                if (str_starts_with(((struct multiboot_tag_module *)tag)->cmdline, "ps2d")) {
                    struct multiboot_tag_module * mod = (struct multiboot_tag_module *)tag;
                    print_str(" --> loading ");
                    print_str(mod->cmdline);
                    print_str("\n");
                    static uint64_t virt_addr = 549755813888 + 0x3000000;
                    uint64_t phys_start = (uint64_t)mod->mod_start & ~(uint64_t)0xfff;
                    uint64_t phys_end = (uint64_t)mod->mod_end;
                    uint64_t nb_pages = (phys_end - phys_start) >> 12;
                    if (phys_end & 0xfff) nb_pages += 1;
                    map_phys(virt_addr, phys_start, nb_pages, 0x3);
                    ELF_64bit* e = (ELF_64bit*)((uint64_t)mod->mod_start - phys_start + virt_addr);
                    uint64_t pid = load_elf(e, 3);
                    // start_process(pid, e->program_entry, 0, 0, 0);
                }
            }
        }

    init_acpi(multiboot_info_str);

    //print_str("Everything seems ok. Nothing to do. Exiting...\n");

    syscall(SYSCALL_EXIT);
}