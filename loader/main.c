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
#include <pmos/ipc.h>
#include <stddef.h>
#include <pmos/system.h>
#include <pmos/memory.h>
#include <string.h>

int* __get_errno()
{
    static int errno_s = 0;
    return &errno_s;
}

uint64_t multiboot_magic;
uint64_t multiboot_info_str;

uint64_t loader_port = 0;

struct task_list_node {
    struct task_list_node *next;
    struct multiboot_tag_module * mod_ptr;
    ELF_64bit* elf_virt_addr;
    uint64_t page_table;
    void * tls_virt;
};

struct task_list_node *modules_list = NULL;

void push_modules_list(struct task_list_node *n)
{
    n->next = modules_list;
    modules_list = n;
}

struct task_list_node *get_by_page_table(uint64_t page_table)
{
    struct task_list_node *p = modules_list;
    while (p && p->page_table != page_table)
        p = p->next;

    return p;
}

void load_multiboot_module(struct multiboot_tag_module * mod)
{
    print_str(" --> loading ");
    print_str(mod->cmdline);
    print_str("\n");

    struct task_list_node *n = malloc(sizeof(struct task_list_node));
    push_modules_list(n);

    n->mod_ptr = mod;
    n->page_table = 0;
    n->tls_virt = NULL;

    uint64_t phys_start = (uint64_t)mod->mod_start & ~(uint64_t)0xfff;
    uint64_t phys_end = (uint64_t)mod->mod_end;
    uint64_t nb_pages = (phys_end - phys_start) >> 12;
    if (phys_end & 0xfff) nb_pages += 1;

    mem_request_ret_t result = create_phys_map_region(0, NULL, nb_pages*4096, PROT_READ, (void *)phys_start);
    if (result.result != SUCCESS) {
        asm ("xchgw %bx, %bx");
    }
    
    n->elf_virt_addr = result.virt_addr;

    uint64_t pid = load_elf(n, 3);

    syscall(SYSCALL_SET_TASK_NAME, pid, mod->cmdline, strlen(mod->cmdline));

    start_process(pid, n->elf_virt_addr->program_entry, 0, 0, n->tls_virt);
}

void react_alloc_page(IPC_Kernel_Alloc_Page *p);

void init_std_lib(void);

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

    init_std_lib();

    init_acpi(multiboot_info_str);

    syscall_r r = syscall(SYSCALL_CREATE_PORT, getpid());
    loader_port = r.value; 
    if (r.result != SUCCESS) {
        print_str("Loader: could not create a port. Error: ");
        print_hex(r.result);
        print_str("\n");
        goto exit;
    }

    start_cpus();

    print_str("Loading modules...\n");
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

    init_acpi(multiboot_info_str);

    char *log_port_name = "/pmos/terminald";
    syscall(SYSCALL_REQUEST_NAMED_PORT, log_port_name, strlen(log_port_name), loader_port, 0);

    char *loader_port_name = "/pmos/loader";
    syscall(SYSCALL_NAME_PORT, loader_port, loader_port_name, strlen(loader_port_name));


    while (1) {
        Message_Descriptor desc;
        syscall_r s = syscall(SYSCALL_GET_MSG_INFO, &desc, loader_port, 0);

        if (s.result != SUCCESS) {

        }

        char buff[desc.size];
        s = syscall(SYSCALL_GET_MESSAGE, buff, 0, loader_port);
        if (s.result != SUCCESS) {
        }

        if (desc.size < 4) {
            // Message too small
        }


        IPC_ACPI_Request_RSDT* ptr = (IPC_ACPI_Request_RSDT *)buff;

        switch (ptr->type) {
        case 0x60: {
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

            syscall(SYSCALL_SEND_MSG_PORT, ptr->reply_channel, sizeof(reply), &reply);
        }
            break;
        case 0x21: {
            IPC_Kernel_Named_Port_Notification* notif = (IPC_Kernel_Named_Port_Notification *)ptr;
            if (desc.size < sizeof(IPC_Kernel_Named_Port_Notification)) {
                print_str("Loader: Recieved IPC_Kernel_Named_Port_Notification with unexpected size 0x");
                print_hex(desc.size);
                print_str("\n");
                break;
            }

            size_t msg_size = desc.size - sizeof(IPC_Kernel_Named_Port_Notification);

            if (strncmp(notif->port_name, log_port_name, msg_size) == 0) {
                set_print_syscalls(notif->port_num);
            }
        }
            break;
        
        case 0x22: {
            IPC_Kernel_Alloc_Page *a = (IPC_Kernel_Alloc_Page *)ptr;
            if (desc.size < sizeof (IPC_Kernel_Alloc_Page)) {
                print_str("Loader: Recieved IPC_Kernel_Alloc_Page of unexpected size 0x");
                print_hex(desc.size);
                print_str("\n");
                break;
            }

            react_alloc_page(a);


            break;
        }

        default:
            print_str("Loader: Recievend unknown message with type 0x");
            print_hex(ptr->type);
            print_str("\n");
        }
 
    }

    //print_str("Everything seems ok. Nothing to do. Exiting...\n");
exit:
    syscall(SYSCALL_EXIT);
}