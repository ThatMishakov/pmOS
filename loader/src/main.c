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
#include <fs.h>
#include <stdbool.h>
#include <string.h>
#include <stdlib.h>
#include <pmos/load_data.h>

#define alignup(size, alignment) (size%alignment ? size + (alignment - size%alignment) : size)

int* __get_errno()
{
    static int errno_s = 0;
    return &errno_s;
}

uint64_t multiboot_magic;
uint64_t multiboot_info_str;

uint64_t loader_port = 0;

struct task_list_node *modules_list = NULL;

void push_modules_list(struct task_list_node *n)
{
    n->next = modules_list;
    modules_list = n;
}

struct task_list_node *get_by_page_table(uint64_t page_table)
{
    struct task_list_node *p = modules_list;
    while (p && (!p->executable || p->page_table != page_table))
        p = p->next;

    return p;
}

struct load_data_array {
    char * data;
    uint64_t size;
    uint64_t capacity;
};

int init_load_data_array(struct load_data_array *a)
{
    a->data = NULL;
    a->size = 0;
    a->capacity = 0;

    return 0;
}

int push_load_data_array(struct load_data_array *a, char * data, uint64_t size)
{
    if (size % 16 != 0) {
        return -1;
    }

    if (a->size + size > a->capacity) {
        uint64_t new_capacity = a->capacity + size;
        char * new_data = realloc(a->data, new_capacity);
        if (new_data == NULL) {
            return -1;
        }

        a->data = new_data;
        a->capacity = new_capacity;
    }

    memcpy(a->data + a->size, data, size);
    a->size += size;

    return 0;
}

int send_and_close_load_data_array(struct load_data_array *a, struct task_list_node *n)
{
    size_t needed_size = a->size + sizeof(struct load_tag_close);
    needed_size = alignup(needed_size, 4096); // PAGE_SIZE

    mem_request_ret_t req = create_normal_region(0, 0, needed_size, 1 | 2);
    if (req.result != SUCCESS) {
        free(a->data);
        return -1;
    }

    memcpy(req.virt_addr, a->data, a->size);
    // Don't copy closing tag, since it's all zeroes and that's what kernel sets memory to anyways

    req = transfer_region(n->page_table, req.virt_addr, NULL, 1);
    if (req.result != SUCCESS) {
        free(a->data);
        return -1;
    }

    free(a->data);

    n->load_data_virt_addr = req.virt_addr;
    n->load_data_size = needed_size;

    return 0;
}

void load_multiboot_module(struct multiboot_tag_module * mod)
{
    print_str(" --> loading ");
    print_str(mod->cmdline);
    print_str("\n");

    struct task_list_node *n = malloc(sizeof(struct task_list_node));
    n->mod_ptr = mod;
    n->name = NULL;
    n->path = NULL;
    n->cmdline = NULL;
    n->page_table = 0;
    n->tls_virt = NULL;
    n->executable = false;
    n->stack_top = 0;
    n->stack_size = 0;
    n->stack_guard_size = 0;
    n->load_data_virt_addr = 0;
    n->load_data_size = 0;

    push_modules_list(n);

    char * cmdline = strdup(mod->cmdline);
    char * ptrptr;
    char * token = strtok_r(cmdline, " ", &ptrptr);
    while (token != NULL) {
        if (strncmp(token, "--execute", 9) == 0) {
            n->executable = true;
        } else if (strncmp(token, "--path", 6) == 0) {
            if (n->path == NULL)
                strncpy(n->path, ptrptr, strlen(ptrptr) + 1);
        } else {
            if (n->name == NULL)
                n->name = strdup(token);
        }

        token = strtok_r(NULL, " ", &ptrptr);
    }
    free(cmdline);


    uint64_t phys_start = (uint64_t)mod->mod_start & ~(uint64_t)0xfff;
    uint64_t phys_end = (uint64_t)mod->mod_end;
    uint64_t nb_pages = (phys_end - phys_start) >> 12;
    if (phys_end & 0xfff) nb_pages += 1;

    mem_request_ret_t result = create_phys_map_region(0, NULL, nb_pages*4096, PROT_READ, phys_start);
    if (result.result != SUCCESS) {
        asm ("xchgw %bx, %bx");
    }

    n->file_virt_addr = result.virt_addr;

    
    if (n->executable) {
        uint64_t pid = load_elf(n, 3);

        syscall(SYSCALL_SET_TASK_NAME, pid, n->name, strlen(n->name));

        ELF_64bit * elf = n->file_virt_addr;

        struct load_data_array load_data;
        init_load_data_array(&load_data);

        struct load_tag_stack_descriptor stack_desc = {
            .header = {
                .tag = LOAD_TAG_STACK_DESCRIPTOR,
                .flags = 0,
                .offset_to_next = sizeof(struct load_tag_stack_descriptor),
            },
            .stack_top = n->stack_top,
            .stack_size = n->stack_size,
            .guard_size = n->stack_guard_size,
            .unused0 = 0,
        };

        push_load_data_array(&load_data, (char *)&stack_desc, sizeof(stack_desc));
        send_and_close_load_data_array(&load_data, n);

        start_process(pid, elf->program_entry, n->load_data_virt_addr, n->load_data_size, n->tls_virt);
    }
}

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

    // init_std_lib();

    init_acpi(multiboot_info_str);

    syscall_r r = syscall(SYSCALL_CREATE_PORT, get_task_id());
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

    service_ports();

    //print_str("Everything seems ok. Nothing to do. Exiting...\n");
exit:
    syscall(SYSCALL_EXIT);
}