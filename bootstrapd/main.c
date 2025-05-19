/* Copyright (c) 2024, Mikhail Kovalev
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice, this
 *    list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 *
 * 3. Neither the name of the copyright holder nor the names of its
 *    contributors may be used to endorse or promote products derived from
 *    this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "fs.h"
#include "io.h"
#include "named_ports.h"

#include <errno.h>
#include <kernel/messaging.h>
#include <kernel/syscalls.h>
#include <pmos/helpers.h>
#include <pmos/ipc.h>
#include <pmos/load_data.h>
#include <pmos/ports.h>
#include <pmos/system.h>
#include <stdlib.h>
#include <string.h>
#include <sys/syscall.h>

uint64_t loader_port = 0;

extern void *__load_data_kernel;
extern size_t __load_data_size_kernel;

struct module_descriptor_list {
    struct module_descriptor_list *next;
    uint64_t object_id;
    size_t size;
    char *cmdline;
    char *path;
};

struct module_descriptor_list *module_list = NULL;

struct framebuffer {
    uint64_t framebuffer_addr;
    uint32_t framebuffer_width;
    uint32_t framebuffer_height;
    uint32_t framebuffer_pitch;
    uint32_t framebuffer_bpp;
    uint16_t framebuffer_memory_model;
    uint8_t framebuffer_red_mask_size;
    uint8_t framebuffer_red_mask_shift;
    uint8_t framebuffer_green_mask_size;
    uint8_t framebuffer_green_mask_shift;
    uint8_t framebuffer_blue_mask_size;
    uint8_t framebuffer_blue_mask_shift;
};

// TODO: Several framebuffers can be present. Only store one for now
struct framebuffer *fb = NULL;

void init_modules()
{
    struct load_tag_generic *t =
        get_load_tag(LOAD_TAG_LOAD_MODULES, __load_data_kernel, __load_data_size_kernel);
    if (t == NULL)
        return;

    struct load_tag_load_modules_descriptor *m = (struct load_tag_load_modules_descriptor *)t;

    for (size_t i = 0; i < m->modules_count; i++) {
        struct module_descriptor_list *d = malloc(sizeof(struct module_descriptor_list));
        if (d == NULL) {
            print_str("Loader: Could not allocate memory for module descriptor\n");
            return;
        }

        d->object_id = m->modules[i].memory_object_id;
        d->size      = m->modules[i].size;
        d->cmdline   = strdup((char *)m + m->modules[i].cmdline_offset);
        d->path      = strdup((char *)m + m->modules[i].path_offset);
        d->next      = module_list;
        module_list  = d;
    }
}

uint64_t rsdp_desc = 0;

struct fdt_descriptor {
    uint64_t memory_object;
    uint32_t start_offset;
    uint32_t mem_object_size;
} fdt_desc = {};

void init_misc()
{
    do {
        struct load_tag_generic *t =
            get_load_tag(LOAD_TAG_FRAMEBUFFER, __load_data_kernel, __load_data_size_kernel);
        if (t == NULL)
            continue;

        fb = malloc(sizeof(struct framebuffer));
        if (fb == NULL)
            continue;

        struct load_tag_framebuffer *tt = (struct load_tag_framebuffer *)t;

        fb->framebuffer_addr             = tt->framebuffer_addr;
        fb->framebuffer_width            = tt->framebuffer_width;
        fb->framebuffer_height           = tt->framebuffer_height;
        fb->framebuffer_pitch            = tt->framebuffer_pitch;
        fb->framebuffer_bpp              = tt->framebuffer_bpp;
        fb->framebuffer_memory_model     = tt->memory_model;
        fb->framebuffer_red_mask_size    = tt->red_mask_size;
        fb->framebuffer_red_mask_shift   = tt->red_mask_shift;
        fb->framebuffer_green_mask_size  = tt->green_mask_size;
        fb->framebuffer_green_mask_shift = tt->green_mask_shift;
        fb->framebuffer_blue_mask_size   = tt->blue_mask_size;
        fb->framebuffer_blue_mask_shift  = tt->blue_mask_shift;
    } while (0);

    do {
        struct load_tag_rsdp *tr = (struct load_tag_rsdp *)get_load_tag(
            LOAD_TAG_RSDP, __load_data_kernel, __load_data_size_kernel);
        if (tr == NULL)
            continue;

        rsdp_desc = tr->rsdp;
    } while (0);

    do {
        struct load_tag_fdt *tr = (struct load_tag_fdt *)get_load_tag(
            LOAD_TAG_FDT, __load_data_kernel, __load_data_size_kernel);
        if (tr == NULL)
            continue;

        fdt_desc = (struct fdt_descriptor) {.memory_object   = tr->fdt_memory_object,
                                            .start_offset    = tr->start_offset,
                                            .mem_object_size = tr->mem_object_size};
    } while (0);
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
        r.result_code                  = 0;
        r.flags                        = 0;
        r.framebuffer_addr             = fb->framebuffer_addr;
        r.framebuffer_bpp              = fb->framebuffer_bpp;
        r.framebuffer_height           = fb->framebuffer_height;
        r.framebuffer_width            = fb->framebuffer_width;
        r.framebuffer_pitch            = fb->framebuffer_pitch;
        r.framebuffer_memory_model     = fb->framebuffer_memory_model;
        r.framebuffer_red_mask_size    = fb->framebuffer_red_mask_size;
        r.framebuffer_red_mask_shift   = fb->framebuffer_red_mask_shift;
        r.framebuffer_green_mask_size  = fb->framebuffer_green_mask_size;
        r.framebuffer_green_mask_shift = fb->framebuffer_green_mask_shift;
        r.framebuffer_blue_mask_size   = fb->framebuffer_blue_mask_size;
        r.framebuffer_blue_mask_shift  = fb->framebuffer_blue_mask_shift;
    }

    send_message_port(port, sizeof(r), &r);
}

void start_executables()
{
    struct module_descriptor_list *d = module_list;
    while (d != NULL) {
        struct module_descriptor_list *c = d;
        d                                = d->next;
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

void set_print_callback(int result, const char * /* port_name */, pmos_port_t port)
{
    if (!result)
        set_print_syscalls(port);
}

static const char *log_port_name = "/pmos/terminald";
static char *vfsd_port_name      = "/pmos/vfsd";

int default_callback(Message_Descriptor *desc, void *buff, pmos_right_t *reply_right,
                     pmos_right_t *extra_rights, void *, struct pmos_msgloop_data *)
{
    if (desc->size < 4) {
        // Message too small
    }

    IPC_ACPI_Request_RSDT *ptr = (IPC_ACPI_Request_RSDT *)buff;

    switch (ptr->type) {
    case 0x60: {
        IPC_ACPI_RSDT_Reply reply = {};

        reply.type       = 0x61;
        reply.result     = -ENOSYS;
        reply.descriptor = 0;

        if (rsdp_desc != 0) {
            reply.result     = 0;
            reply.descriptor = rsdp_desc;
            print_str("********************************************\n");
        }

        auto r = send_message_right(*reply_right, 0, &reply, sizeof(reply), NULL,
                                    SEND_MESSAGE_DELETE_RIGHT);
        if (!r.result)
            *reply_right = 0;
    } break;
    case IPC_FDT_Request_NUM: {
        IPC_FDT_Reply reply = {.type              = IPC_FDT_Reply_NUM,
                               .result            = -ENOSYS,
                               .fdt_mem_object_id = 0,
                               .object_offset     = 0,
                               .object_size       = 0};

        if (fdt_desc.memory_object != 0) {
            reply.result            = 0;
            reply.fdt_mem_object_id = fdt_desc.memory_object;
            reply.object_offset     = fdt_desc.start_offset;
            reply.object_size       = fdt_desc.mem_object_size;
        }

        auto r = send_message_right(*reply_right, 0, &reply, sizeof(reply), NULL, SEND_MESSAGE_DELETE_RIGHT);
        if (!r.result)
            *reply_right = 0;
    } break;
    // case 0x21: {
    //     IPC_Kernel_Named_Port_Notification *notif = (IPC_Kernel_Named_Port_Notification *)ptr;
    //     if (desc->size < sizeof(IPC_Kernel_Named_Port_Notification)) {
    //         print_str(
    //             "Loader: Recieved IPC_Kernel_Named_Port_Notification with unexpected size 0x");
    //         print_hex(desc->size);
    //         print_str("\n");
    //         break;
    //     }

    //     size_t msg_size = desc->size - sizeof(IPC_Kernel_Named_Port_Notification);

    //     if (strncmp(notif->port_name, log_port_name, msg_size) == 0) {
    //         set_print_syscalls(notif->port_num);
    //     } else if (strncmp(notif->port_name, vfsd_port_name, msg_size) == 0) {
    //         initialize_filesystem(notif->port_num);
    //     }
    // } break;
    case IPC_Register_FS_Reply_NUM: {
        IPC_Register_FS_Reply *a = (IPC_Register_FS_Reply *)ptr;
        if (desc->size < sizeof(IPC_Register_FS_Reply)) {
            print_str("Loader: Recieved IPC_Register_FS_Reply of unexpected size 0x");
            print_hex(desc->size);
            print_str("\n");
            break;
        }

        if (fs_react_register_reply(a, desc->size, desc->sender) != 0) {
            print_str("Loader: Error registering filesystem\n");
        }
        break;
    }
    case IPC_Mount_FS_Reply_NUM: {
        IPC_Mount_FS_Reply *a = (IPC_Mount_FS_Reply *)ptr;
        if (desc->size < sizeof(IPC_Mount_FS_Reply)) {
            print_str("Loader: Recieved IPC_Mount_FS_Reply of unexpected size 0x");
            print_hex(desc->size);
            print_str("\n");
            break;
        }

        if (fs_react_mount_reply(a, desc->size, desc->sender) != 0) {
            print_str("Loader: Error mounting filesystem\n");
        }

        break;
    }
    case IPC_FS_Open_NUM: {
        IPC_FS_Open *a          = (IPC_FS_Open *)ptr;
        IPC_FS_Open_Reply reply = {.type = IPC_FS_Open_Reply_NUM};
        if (desc->size >= sizeof(IPC_FS_Open)) {
            int result = register_open_request(a, &reply);

            if (result != 0) {
                reply.result_code = result;
            }
            auto r = send_message_right(*reply_right, 0, &reply, sizeof(reply), NULL,
                                        SEND_MESSAGE_DELETE_RIGHT);
            if (!r.result)
                *reply_right = 0;
        } else {
            print_str("Loader: Recieved IPC_FS_Open of unexpected size 0x");
            print_hex(desc->size);
            print_str("\n");
        }

        break;
    }
    case IPC_FS_Resolve_Path_NUM: {
        IPC_FS_Resolve_Path *a = (IPC_FS_Resolve_Path *)ptr;
        if (desc->size < sizeof(IPC_FS_Resolve_Path)) {
            print_str("Loader: Recieved IPC_FS_Resolve_Path of unexpected size 0x");
            print_hex(desc->size);
            print_str("\n");
            break;
        }

        fs_react_resolve_path(a, desc->size, desc->sender);

        break;
    }
    case IPC_Read_NUM: {
        IPC_Read *a = (IPC_Read *)ptr;
        if (desc->size < sizeof(IPC_Read)) {
            print_str("Loader: Recieved IPC_Read of unexpected size 0x");
            print_hex(desc->size);
            print_str("\n");
            break;
        }

        fs_react_read(a, desc->size, desc->sender);

        break;
    }
    case IPC_FS_Dup_NUM: {
        IPC_FS_Dup *a = (IPC_FS_Dup *)ptr;
        if (desc->size < sizeof(IPC_FS_Dup)) {
            print_str("Loader: Recieved IPC_Dup of unexpected size 0x");
            print_hex(desc->size);
            print_str("\n");
            break;
        }

        fs_react_dup(a, desc->size, desc->sender);

        break;
    }
    case IPC_Framebuffer_Request_NUM: {
        // Provide framebuffer
        IPC_Framebuffer_Request *a = (IPC_Framebuffer_Request *)ptr;
        if (desc->size < sizeof(IPC_Framebuffer_Request)) {
            print_str("Loader: Recieved IPC_Framebuffer_Request of unexpected size 0x");
            print_hex(desc->size);
            print_str("\n");
            break;
        }

        provide_framebuffer(a->reply_port, a->flags);

        break;
    }

    case IPC_Get_Named_Port_NUM: {
        IPC_Get_Named_Port *r = (IPC_Get_Named_Port *)ptr;
        if (desc->size < sizeof(IPC_Get_Named_Port)) {
            print_str("Loader: Recieved IPC_Get_Named_Port of unexpected size 0x");
            print_hex(desc->size);
            print_str("\n");
            break;
        }

        size_t length = desc->size - sizeof(IPC_Get_Named_Port);
        request_port_message(r->name, length, r->flags, *reply_right);
        break;
    }

    case IPC_Name_Port_NUM: {
        IPC_Name_Port *r = (IPC_Name_Port *)ptr;
        if (desc->size < sizeof(IPC_Name_Port)) {
            print_str("Loader: Recieved IPC_Name_Port of unexpected size 0x");
            print_hex(desc->size);
            print_str("\n");
            break;
        }

        size_t length = desc->size - sizeof(IPC_Name_Port);
        auto result   = register_right(r->name, length, extra_rights[0]);

        IPC_Name_Port_Reply reply = {
            .type   = IPC_Name_Port_Reply_NUM,
            .flags  = 0,
            .result = result,
        };

        auto send_result = send_message_right(*reply_right, 0, &reply, sizeof(reply), NULL,
                                              SEND_MESSAGE_DELETE_RIGHT);
        if (send_result.result)
            delete_right(*reply_right);

        break;
    }

    default:
        print_str("Loader: Recievend unknown message with type ");
        print_hex(ptr->type);
        print_str(" from task ");
        print_hex(desc->sender);
        print_str("\n");
    }

    return 0;
}

pmos_right_t loader_right    = 0;
pmos_right_t namespace_right = 0;

void service_ports()
{
    auto result = request_port_callback(log_port_name, strlen(log_port_name), set_print_callback);
    if (result != SUCCESS) {
        print_str("Loader: could not request log port. Error: ");
        print_hex(result);
        print_str("\n");
        goto exit;
    }

    // res = request_named_port(vfsd_port_name, strlen(vfsd_port_name), loader_port, 0);
    // if (res != SUCCESS) {
    //     print_str("Loader: could not request vfsd port. Error: ");
    //     print_hex(res);
    //     print_str("\n");
    //     goto exit;
    // }

    struct pmos_msgloop_data data;
    pmos_msgloop_initialize(&data, loader_port);

    pmos_msgloop_tree_node_t n;
    pmos_msgloop_node_set(&n, 0, default_callback, NULL);
    pmos_msgloop_insert(&data, &n);
    pmos_msgloop_loop(&data);

exit:
}

int main()
{
    ports_request_t r = create_port(0, 0);
    loader_port       = r.port;
    if (r.result != SUCCESS) {
        print_str("Loader: could not create a port. Error: ");
        print_hex(r.result);
        print_str("\n");
        exit(1);
    }

    auto create_result = create_right(loader_port, &loader_right, 0);
    if (create_result.result != SUCCESS) {
        print_str("Loader: failed to create loader right: ");
        print_hex(create_result.result);
        print_str("\n");
        exit(1);
    }

    char *loader_port_name = "/pmos/loader";
    int result = register_right(loader_port_name, strlen(loader_port_name), loader_right);
    // res = loader_port, loader_port_name, strlen(loader_port_name), 0);
    if (result != SUCCESS) {
        print_str("Loader: could not name loader port. Error: ");
        print_hex(result);
        print_str("\n");
        exit(1);
    }

    create_result = create_right(loader_port, &namespace_right, 0);
    if (create_result.result != SUCCESS) {
        print_str("Loader: failed to create namespace right: ");
        print_hex(create_result.result);
        print_str("\n");
        exit(1);
    }

    auto kresult = set_right0(create_result.right);
    if (kresult != SUCCESS) {
        print_str("Loader: failed to set port 0: ");
        print_hex(kresult);
        print_str("\n");
        exit(1);
    }

    init_modules();
    init_misc();
    start_executables();

    service_ports();
}