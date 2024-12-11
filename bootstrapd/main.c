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

#include <errno.h>
#include <kernel/messaging.h>
#include <kernel/syscalls.h>
#include <pmos/ipc.h>
#include <pmos/load_data.h>
#include <pmos/system.h>
#include <stdlib.h>
#include <string.h>
#include <sys/syscall.h>
#include <pmos/ports.h>

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

void service_ports()
{
    ports_request_t r = create_port(0, 0);
    loader_port = r.port;
    if (r.result != SUCCESS) {
        print_str("Loader: could not create a port. Error: ");
        print_hex(r.result);
        print_str("\n");
        goto exit;
    }

    char *log_port_name = "/pmos/terminald";
    result_t res = request_named_port(log_port_name, strlen(log_port_name), loader_port, 0);
    if (res != SUCCESS) {
        print_str("Loader: could not request log port. Error: ");
        print_hex(res);
        print_str("\n");
        goto exit;
    }

    char *vfsd_port_name = "/pmos/vfsd";
    res = request_named_port(vfsd_port_name, strlen(vfsd_port_name), loader_port, 0);
    if (res != SUCCESS) {
        print_str("Loader: could not request vfsd port. Error: ");
        print_hex(res);
        print_str("\n");
        goto exit;
    }

    char *loader_port_name = "/pmos/loader";
    res = name_port(loader_port, loader_port_name, strlen(loader_port_name), 0);
    if (res != SUCCESS) {
        print_str("Loader: could not name loader port. Error: ");
        print_hex(res);
        print_str("\n");
        goto exit;
    }

    while (1) {
        Message_Descriptor desc;
        result_t r = syscall_get_message_info(&desc, loader_port, 0);

        if (r != SUCCESS) {
            print_str("Loader: Could not get message info. Error: ");
            print_hex(r);
            print_str("\n");
            break;
        }

        char buff[desc.size];
        r = get_first_message(buff, 0, loader_port);
        if (r != SUCCESS) {
            print_str("Loader: Could not get message. Error: ");
            print_hex(r);
            print_str("\n");
            break;
        }

        if (desc.size < 4) {
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
                reply.descriptor = (void *)rsdp_desc;
                print_str("********************************************\n");
            }

            send_message_port(ptr->reply_channel, sizeof(reply), &reply);
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

            send_message_port(ptr->reply_channel, sizeof(reply), &reply);
        } break;
        case 0x21: {
            IPC_Kernel_Named_Port_Notification *notif = (IPC_Kernel_Named_Port_Notification *)ptr;
            if (desc.size < sizeof(IPC_Kernel_Named_Port_Notification)) {
                print_str(
                    "Loader: Recieved IPC_Kernel_Named_Port_Notification with unexpected size 0x");
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
        } break;
        case IPC_Register_FS_Reply_NUM: {
            IPC_Register_FS_Reply *a = (IPC_Register_FS_Reply *)ptr;
            if (desc.size < sizeof(IPC_Register_FS_Reply)) {
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
            if (desc.size < sizeof(IPC_Mount_FS_Reply)) {
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
            IPC_FS_Open *a          = (IPC_FS_Open *)ptr;
            IPC_FS_Open_Reply reply = {.type = IPC_FS_Open_Reply_NUM};
            if (desc.size >= sizeof(IPC_FS_Open)) {
                int result = register_open_request(a, &reply);

                if (result != 0) {
                    reply.result_code = result;
                }
                send_message_port(ptr->reply_channel, sizeof(reply), &reply);
            } else {
                print_str("Loader: Recieved IPC_FS_Open of unexpected size 0x");
                print_hex(desc.size);
                print_str("\n");
            }

            break;
        }
        case IPC_FS_Resolve_Path_NUM: {
            IPC_FS_Resolve_Path *a = (IPC_FS_Resolve_Path *)ptr;
            if (desc.size < sizeof(IPC_FS_Resolve_Path)) {
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
            if (desc.size < sizeof(IPC_Read)) {
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
            if (desc.size < sizeof(IPC_FS_Dup)) {
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
            IPC_Framebuffer_Request *a = (IPC_Framebuffer_Request *)ptr;
            if (desc.size < sizeof(IPC_Framebuffer_Request)) {
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