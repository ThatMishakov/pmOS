#include <multiboot2.h>
#include <stdint.h>
#include <misc.h>
#include <io.h>
#include <linker.h>
#include <mem.h>
#include <entry.h>
#include <kernel_loader.h>
#include <syscall.h>
#include <screen.h>
#include "../kernel/common/syscalls.h"
#include <load_elf.h>

uint64_t multiboot_magic;
uint64_t multiboot_info_str;

void main()
{
    asm("xchgw %bx, %bx");
    if (multiboot_magic != MULTIBOOT2_BOOTLOADER_MAGIC) {
        print_str("Not booted by multiboot2 bootloader\n");
        while (1) ;
    }
    asm("xchgw %bx, %bx");

    /* Initialize everything */
    print_str("Hello from loader!\n");
    print_str("The rest is broken! Halting...\n");

    while (1)
    {
      asm("hlt");
    }
    

    init_mem(multiboot_info_str);

    load_kernel(multiboot_info_str);
    uint64_t addr = (uint64_t)0xdeadbeef0;
    uint64_t result = get_page(addr & ~((uint64_t)0xfff)).result;
    print_str("Getting a page to check syscalls ");
    print_hex(addr);
    print_str(" -> ");
    print_hex(result);
    print_str(" \n");
    uint64_t* ptr = (uint64_t*)addr;
    ptr[0] = 0xcafebabe;

    print_str("Loading modules...\n");
    for (struct multiboot_tag * tag = (struct multiboot_tag *) (multiboot_info_str + 8); tag->type != MULTIBOOT_TAG_TYPE_END;
      tag = (struct multiboot_tag *) ((multiboot_uint8_t *) tag + ((tag->size + 7) & ~7))) {
        if (tag->type == MULTIBOOT_TAG_TYPE_MODULE) {
            if (!str_starts_with(((struct multiboot_tag_module *)tag)->cmdline, "kernel")) {
                struct multiboot_tag_module * mod = (struct multiboot_tag_module *)tag;
                print_str(" --> loading ");
                print_str(mod->cmdline);
                print_str("\n");
                load_elf(mod->mod_start, 3);
            }
        }
      }

    print_str("Everything seems ok. Nothing to do. Exiting...\n");
    syscall(SYSCALL_EXIT);
}