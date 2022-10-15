#include <multiboot2.h>
#include <stdint.h>
#include <misc.h>
#include <io.h>
#include <linker.h>
#include <mem.h>
#include <entry.h>
#include <kernel_loader.h>
#include "syscall.h"
#include <screen.h>
#include "../kernel/common/syscalls.h"

uint32_t multiboot_magic;
uint32_t multiboot_info_str;

void main()
{
    cls();
    if (multiboot_magic != MULTIBOOT2_BOOTLOADER_MAGIC) {
        print_str("Not booted by multiboot2 bootloader\n");
        while (1) ;
    }

    /* Initialize everything */
    print_str("Hello from loader!\n");
    init_mem(multiboot_info_str);

    load_kernel(multiboot_info_str);

    print_str("Back to loader...\n");

    /*
    print_str("Trying syscall(1);\n");
    syscall_r r = syscall(1);
    print_str("--> Recieved: ");
    print_hex(r.result);
    print_str(" ");
    print_hex(r.value);
    print_str("\n");
    */

    print_str("Attempting to create a process...\n");
    syscall_r r = syscall(SYSCALL_CREATE_PROCESS);
    print_str("--> Recieved: ");
    print_hex(r.result);
    print_str(" ");
    print_hex(r.value);
    print_str("\n");

    print_str("Nothing to do. Halting...\n");
    while (1) {
      asm ("hlt");
    }
}