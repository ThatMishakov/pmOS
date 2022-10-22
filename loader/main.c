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
    if (multiboot_magic != MULTIBOOT2_BOOTLOADER_MAGIC) {
        print_str("Not booted by multiboot2 bootloader\n");
        while (1) ;
    }

    /* Initialize everything */
    print_str("Hello from loader!\n");

    // Map multiboot structure into the kernel
    uint64_t multiboot_str_all = multiboot_info_str & ~(uint64_t)0xfff;
    Page_Table_Argumments pta;
    pta.user_access = 1;
    pta.execution_disabled = 1;
    pta.writeable = 1;
    map(multiboot_str_all, multiboot_str_all, pta);
    

    init_mem(multiboot_info_str);

    print_str("Memory bitmap initialized!\n");

    load_kernel(multiboot_info_str);
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