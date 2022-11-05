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
#include <kernel/syscalls.h>
#include <kernel/errors.h>
#include <kernel/block.h>
#include <kernel/messaging.h>
#include <load_elf.h>
#include <paging.h>
#include <syscall.h>

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

    map(0xb8000, 0xb8000, pta);
    cls();

    /* Initialize everything */
    print_str("Hello from loader!\n");

    // Map multiboot structure into the kernel
    uint64_t multiboot_str_all = multiboot_info_str & ~(uint64_t)0xfff;
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
                    static uint64_t virt_addr = 549755813888;
                    uint64_t phys_start = (uint64_t)mod->mod_start & ~(uint64_t)0xfff;
                    uint64_t phys_end = (uint64_t)mod->mod_end;
                    uint64_t nb_pages = (phys_end - phys_start) >> 12;
                    if (phys_end & 0xfff) nb_pages += 1;
                    syscall_r p = map_phys(virt_addr, phys_start, nb_pages, 0x3);
                    ELF_64bit* e = (ELF_64bit*)((uint64_t)mod->mod_start - phys_start + virt_addr);
                    load_elf(e, 3);
                }
            }
        }


    print_str("Blocking and recieving a message\n");
    syscall_r r = syscall(SYSCALL_BLOCK, 0x01);
    if (r.result == SUCCESS && r.value == MESSAGE_S_NUM) {
        print_str("Loader: Recieved a message\n");

        Message_Descriptor desc;
        syscall_r s = syscall(SYSCALL_GET_MSG_INFO, &desc);
        if (s.result != SUCCESS) {
            asm("xchgw %bx, %bx");
            while (1) ;
        }

        print_str("Loader: From: ");
        print_hex(desc.sender);
        print_str(" channel ");
        print_hex(desc.sender);
        print_str(" size ");
        print_hex(desc.size);
        print_str("\n");

        char buff[128];
        s = syscall(SYSCALL_GET_MESSAGE, buff, 0);
        if (s.result != SUCCESS) {
            asm("xchgw %bx, %bx");
            while (1) ;
        }

        print_str("Loader: Message content: ");
        print_str(buff);
    }

    print_str("Everything seems ok. Nothing to do. Exiting...\n");

    syscall(SYSCALL_EXIT);
}