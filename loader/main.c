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

typedef struct IPC_ACPI_Request_RSDT {
    uint32_t type;
    uint64_t reply_channel;
} IPC_ACPI_Request_RSDT;

typedef struct IPC_ACPI_RSDT_Reply {
    uint32_t type;
    uint32_t result;
    uint64_t *descriptor;
} IPC_ACPI_RSDT_Reply;

void load_multiboot_module(struct multiboot_tag_module * mod)
{
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

    syscall(SYSCALL_SET_TASK_NAME, pid, mod->cmdline, strlen(mod->cmdline));

    start_process(pid, e->program_entry, 0, 0, 0);
    release_pages(virt_addr, nb_pages);
}

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

    syscall(SYSCALL_SET_PORT_DEFAULT, 2, 2, 1);


    uint64_t terminal_pid = 0;
    for (struct multiboot_tag * tag = (struct multiboot_tag *) (multiboot_info_str + 8); tag->type != MULTIBOOT_TAG_TYPE_END;
        tag = (struct multiboot_tag *) ((multiboot_uint8_t *) tag + ((tag->size + 7) & ~7))) {
            if (tag->type == MULTIBOOT_TAG_TYPE_MODULE) {
                if (!str_starts_with(((struct multiboot_tag_module *)tag)->cmdline, "kernel")) {
                    struct multiboot_tag_module * mod = (struct multiboot_tag_module *)tag;
                    load_multiboot_module(mod);
                }
            }
        }

    // syscall(SYSCALL_SET_PORT_DEFAULT, 1, terminal_pid, 1);

    // for (struct multiboot_tag * tag = (struct multiboot_tag *) (multiboot_info_str + 8); tag->type != MULTIBOOT_TAG_TYPE_END;
    //     tag = (struct multiboot_tag *) ((multiboot_uint8_t *) tag + ((tag->size + 7) & ~7))) {
    //         if (tag->type == MULTIBOOT_TAG_TYPE_MODULE) {
    //             if (str_starts_with(((struct multiboot_tag_module *)tag)->cmdline, "processd")) {
    //                 struct multiboot_tag_module * mod = (struct multiboot_tag_module *)tag;
    //                 load_multiboot_module(mod);
    //             }
    //         }
    //     }

    // for (struct multiboot_tag * tag = (struct multiboot_tag *) (multiboot_info_str + 8); tag->type != MULTIBOOT_TAG_TYPE_END;
    //     tag = (struct multiboot_tag *) ((multiboot_uint8_t *) tag + ((tag->size + 7) & ~7))) {
    //         if (tag->type == MULTIBOOT_TAG_TYPE_MODULE) {
    //             if (str_starts_with(((struct multiboot_tag_module *)tag)->cmdline, "devicesd")) {
    //                 struct multiboot_tag_module * mod = (struct multiboot_tag_module *)tag;
    //                 load_multiboot_module(mod);
    //             }
    //         }
    //     }

    
    // for (struct multiboot_tag * tag = (struct multiboot_tag *) (multiboot_info_str + 8); tag->type != MULTIBOOT_TAG_TYPE_END;
    //     tag = (struct multiboot_tag *) ((multiboot_uint8_t *) tag + ((tag->size + 7) & ~7))) {
    //         if (tag->type == MULTIBOOT_TAG_TYPE_MODULE) {
    //             if (str_starts_with(((struct multiboot_tag_module *)tag)->cmdline, "ps2d")) {
    //                 struct multiboot_tag_module * mod = (struct multiboot_tag_module *)tag;
    //                 load_multiboot_module(mod);
    //             }
    //         }
    //     }

    init_acpi(multiboot_info_str);

    while (1) {
    
        syscall_r r = syscall(SYSCALL_BLOCK, 0x01);
        if (r.result == SUCCESS && r.value == MESSAGE_S_NUM) {
            print_str("Loader: Recieved a message\n");

            Message_Descriptor desc;
            syscall_r s = syscall(SYSCALL_GET_MSG_INFO, &desc);
            if (s.result != SUCCESS) {

            }

            char buff[desc.size];
            s = syscall(SYSCALL_GET_MESSAGE, buff, 0);
            if (s.result != SUCCESS) {
            }

            if (desc.size < 4) {
                // Message too small
            }

            IPC_ACPI_Request_RSDT* ptr = (IPC_ACPI_Request_RSDT *)buff;

            if (ptr->type == 0x60) {
                IPC_ACPI_RSDT_Reply reply = {};

                reply.type = 0x61;
                reply.result = 0;
                reply.descriptor = 0;

                if (rsdp20_desc != 0) {
                    reply.result = 1;
                    reply.descriptor = (uint64_t *)rsdp20_desc;
                } else if (rsdp_desc != 0) {
                    reply.result = 1;
                    reply.descriptor = (uint64_t *)rsdp_desc;
                }

                syscall(SYSCALL_SEND_MSG_TASK, desc.sender, ptr->reply_channel, sizeof(reply), &reply);
            }
        }
 
    }

    //print_str("Everything seems ok. Nothing to do. Exiting...\n");

    syscall(SYSCALL_EXIT);
}