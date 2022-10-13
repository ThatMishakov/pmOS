#include <multiboot2.h>
#include <stdint.h>
#include <misc.h>
#include <io.h>
#include <linker.h>
#include <mem.h>
#include <entry.h>
#include <kernel_loader.h>

uint32_t multiboot_magic;
uint32_t multiboot_info_str;

void main()
{
    if (multiboot_magic != MULTIBOOT2_BOOTLOADER_MAGIC) {
        print_str("Not booted by multiboot2 bootloader\n");
        while (1) ;
    }

    /* Initialize everything */
    print_str("Hello from loader!\n");
    init_mem(multiboot_info_str);

    load_kernel(multiboot_info_str);

    print_str("Nothing to do. Halting...\n");
    while (1) {
      asm ("hlt");
    }
}