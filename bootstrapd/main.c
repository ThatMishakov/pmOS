#include <string.h>
#include <pmos/ipc.h>
#include <kernel/syscalls.h>
#include <kernel/messaging.h>
#include <pmos/system.h>
#include <sys/syscall.h>
#include "io.h"
#include "fs.h"
#include <pmos/load_data.h>
#include <stdlib.h>
#include <errno.h>

uint64_t loader_port = 0;

extern void * __load_data_kernel;
extern size_t __load_data_size_kernel;

struct module_descriptor_list {
    struct module_descriptor_list * next;
    uint64_t object_id;
    size_t size;
    char * cmdline;
    char * path;
};

struct module_descriptor_list * module_list = NULL;

struct framebuffer {
    uint64_t framebuffer_addr;
    uint32_t framebuffer_width;
    uint32_t framebuffer_height;
    uint32_t framebuffer_pitch;
    uint32_t framebuffer_bpp;
};

// TODO: Several framebuffers can be present. Only store one for now
struct framebuffer *fb = NULL;

void init_modules()
{
    struct load_tag_generic * t = get_load_tag(LOAD_TAG_LOAD_MODULES, __load_data_kernel, __load_data_size_kernel);
    if (t == NULL)
        return;

    struct load_tag_load_modules_descriptor * m = (struct load_tag_load_modules_descriptor *)t;

    for (size_t i = 0; i < m->modules_count; i++) {
        struct module_descriptor_list * d = malloc(sizeof(struct module_descriptor_list));
        if (d == NULL) {
            print_str("Loader: Could not allocate memory for module descriptor\n");
            return;
        }

        d->object_id = m->modules[i].memory_object_id;
        d->size = m->modules[i].size;
        d->cmdline = strdup((char *)m + m->modules[i].cmdline_offset);
        d->path = strdup((char *)m + m->modules[i].path_offset);
        d->next = module_list;
        module_list = d;
    }
}

void init_misc()
{
    struct load_tag_generic * t = get_load_tag(LOAD_TAG_FRAMEBUFFER, __load_data_kernel, __load_data_size_kernel);
    if (t == NULL)
        return;

    fb = malloc(sizeof(struct framebuffer));
    if (fb == NULL)
        return;

    struct load_tag_framebuffer *tt = (struct load_tag_framebuffer*)t;

    fb->framebuffer_addr = tt->framebuffer_addr;
    fb->framebuffer_width = tt->framebuffer_width;
    fb->framebuffer_height = tt->framebuffer_height;
    fb->framebuffer_pitch = tt->framebuffer_pitch;
    fb->framebuffer_bpp = tt->framebuffer_bpp;
}

void provide_framebuffer(pmos_port_t port, uint32_t flags)
{
    (void)flags;

    IPC_Framebuffer_Reply r = {
        .type = IPC_Framebuffer_Reply_NUM,
    };

    if (fb == NULL)
        r.result_code = -ENOSYS;
    else {
        r.result_code = 0;
        r.flags = 0;
        r.framebuffer_addr = fb->framebuffer_addr;
        r.framebuffer_bpp = fb->framebuffer_bpp;
        r.framebuffer_height = fb->framebuffer_height;
        r.framebuffer_width = fb->framebuffer_width;
        r.framebuffer_pitch = fb->framebuffer_pitch;
    }


    send_message_port(port, sizeof(r), &r);
}

void start_executables()
{
    struct module_descriptor_list * d = module_list;
    while (d != NULL) {
        struct module_descriptor_list * c = d;
        d = d->next;
        if (strcmp(c->cmdline, "init") == 0) {
            syscall_r r = syscall_new_process();
            if (r.result != SUCCESS) {
                print_str("Loader: Could not create process for ");
                print_str(c->path);
                print_str(". Error: ");
                print_hex(r.result);
                print_str("\n");
            }

            syscall_set_task_name(r.value, c->path, strlen(c->path));

            result_t res = syscall_load_executable(r.value, c->object_id, 0);
            if (res != SUCCESS) {
                print_str("Loader: Could not load executable ");
                print_str(c->path);
                print_str(". Error: ");
                print_hex(res);
                print_str("\n");

                // TODO: Terminate the task on error
            }
        }
    }
}
        

void service_ports()
{
    syscall_r r = pmos_syscall(SYSCALL_CREATE_PORT, getpid());
    loader_port = r.value; 
    if (r.result != SUCCESS) {
        print_str("Loader: could not create a port. Error: ");
        print_hex(r.result);
        print_str("\n");
        goto exit;
    }

    char *log_port_name = "/pmos/terminald";
    pmos_syscall(SYSCALL_REQUEST_NAMED_PORT, log_port_name, strlen(log_port_name), loader_port, 0);

    char *vfsd_port_name = "/pmos/vfsd";
    pmos_syscall(SYSCALL_REQUEST_NAMED_PORT, vfsd_port_name, strlen(vfsd_port_name), loader_port, 0);

    char *loader_port_name = "/pmos/loader";
    pmos_syscall(SYSCALL_NAME_PORT, loader_port, loader_port_name, strlen(loader_port_name));


    while (1) {
        Message_Descriptor desc;
        syscall_r s = pmos_syscall(SYSCALL_GET_MSG_INFO, &desc, loader_port, 0);

        if (s.result != SUCCESS) {

        }

        char buff[desc.size];
        s = pmos_syscall(SYSCALL_GET_MESSAGE, buff, 0, loader_port);
        if (s.result != SUCCESS) {
        }

        if (desc.size < 4) {
            // Message too small
        }


        IPC_ACPI_Request_RSDT* ptr = (IPC_ACPI_Request_RSDT *)buff;

        switch (ptr->type) {
        // case 0x60: {
        //     IPC_ACPI_RSDT_Reply reply = {};

        //     reply.type = 0x61;
        //     reply.result = 0;
        //     reply.descriptor = 0;

        //     if (rsdp20_desc != 0) {
        //         reply.result = 1;
        //         reply.descriptor = (uint64_t *)rsdp20_desc;
        //     } else if (rsdp_desc != 0) {
        //         reply.result = 1;
        //         reply.descriptor = (uint64_t *)rsdp_desc;
        //     }

        //     pmos_syscall(SYSCALL_SEND_MSG_PORT, ptr->reply_channel, sizeof(reply), &reply);
        // }
        //     break;
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
            } else if (strncmp(notif->port_name, vfsd_port_name, msg_size) == 0) {
                initialize_filesystem(notif->port_num);
            }
        }
            break;
        case IPC_Register_FS_Reply_NUM: {
            IPC_Register_FS_Reply *a = (IPC_Register_FS_Reply *)ptr;
            if (desc.size < sizeof (IPC_Register_FS_Reply)) {
                print_str("Loader: Recieved IPC_Register_FS_Reply of unexpected size 0x");
                print_hex(desc.size);
                print_str("\n");
                break;
            }

            if (fs_react_register_reply(a, desc.size, desc.sender) != 0) {
                print_str("Loader: Error registering filesystem\n");
            }
            break;
        }
        case IPC_Mount_FS_Reply_NUM: {
            IPC_Mount_FS_Reply *a = (IPC_Mount_FS_Reply *)ptr;
            if (desc.size < sizeof (IPC_Mount_FS_Reply)) {
                print_str("Loader: Recieved IPC_Mount_FS_Reply of unexpected size 0x");
                print_hex(desc.size);
                print_str("\n");
                break;
            }

            if (fs_react_mount_reply(a, desc.size, desc.sender) != 0) {
                print_str("Loader: Error mounting filesystem\n");
            }

            break;
        }
        case IPC_FS_Open_NUM: {
            IPC_FS_Open *a = (IPC_FS_Open *)ptr;
            IPC_FS_Open_Reply reply = { .type = IPC_FS_Open_Reply_NUM };
            if (desc.size >= sizeof (IPC_FS_Open)) {
                int result = register_open_request(a, &reply);
                
                if (result != 0) {
                    reply.result_code = result;
                }
                pmos_syscall(SYSCALL_SEND_MSG_PORT, ptr->reply_channel, sizeof(reply), &reply);
            } else {
                print_str("Loader: Recieved IPC_FS_Open of unexpected size 0x");
                print_hex(desc.size);
                print_str("\n");
            }

            break;
        }
        case IPC_FS_Resolve_Path_NUM: {
            IPC_FS_Resolve_Path *a = (IPC_FS_Resolve_Path *)ptr;
            if (desc.size < sizeof (IPC_FS_Resolve_Path)) {
                print_str("Loader: Recieved IPC_FS_Resolve_Path of unexpected size 0x");
                print_hex(desc.size);
                print_str("\n");
                break;
            }

            fs_react_resolve_path(a, desc.size, desc.sender);

            break;
        }
        case IPC_Read_NUM: {
            IPC_Read *a = (IPC_Read *)ptr;
            if (desc.size < sizeof (IPC_Read)) {
                print_str("Loader: Recieved IPC_Read of unexpected size 0x");
                print_hex(desc.size);
                print_str("\n");
                break;
            }

            fs_react_read(a, desc.size, desc.sender);

            break;
        }
        case IPC_FS_Dup_NUM: {
            IPC_FS_Dup *a = (IPC_FS_Dup *)ptr;
            if (desc.size < sizeof (IPC_FS_Dup)) {
                print_str("Loader: Recieved IPC_Dup of unexpected size 0x");
                print_hex(desc.size);
                print_str("\n");
                break;
            }

            fs_react_dup(a, desc.size, desc.sender);

            break;
        }
        case IPC_Framebuffer_Request_NUM: {
            // Provide framebuffer
            IPC_Framebuffer_Request *a = (IPC_Framebuffer_Request*)ptr;
            if (desc.size < sizeof (IPC_Framebuffer_Request)) {
                print_str("Loader: Recieved IPC_Framebuffer_Request of unexpected size 0x");
                print_hex(desc.size);
                print_str("\n");
                break;
            }

            provide_framebuffer(a->reply_port, a->flags);

            break;
        }

        default:
            print_str("Loader: Recievend unknown message with type ");
            print_hex(ptr->type);
            print_str(" from task ");
            print_hex(desc.sender);
            print_str("\n");
        }
 
    }
exit:
}

int main()
{
    init_modules();
    init_misc();
    start_executables();
    service_ports();
}