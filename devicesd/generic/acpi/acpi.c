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

#include <acpi/acpi.h>
#include <kernel/block.h>
#include <main.h>
#include <phys_map/phys_map.h>
#include <pmos/helpers.h>
#include <pmos/ipc.h>
#include <pmos/ports.h>
#include <pmos/system.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

int acpi_revision = -1;

RSDP_descriptor20 *rsdp_desc = NULL;

typedef struct acpi_mem_map_node {
    void *phys;
    size_t size_bytes;
    void *virt;
    struct acpi_mem_map_node *next;
} acpi_mem_map_node;

acpi_mem_map_node acpi_mem_map_list_dummy = {0, 0, 0, NULL};

void acpi_mem_map_list_push_front(acpi_mem_map_node *n)
{
    n->next                      = acpi_mem_map_list_dummy.next;
    acpi_mem_map_list_dummy.next = n;
}

void *acpi_map_and_get_virt(void *phys, size_t size)
{
    for (acpi_mem_map_node *p = acpi_mem_map_list_dummy.next; p != NULL; p = p->next) {

        if (p->phys <= phys && (p->size_bytes >= size + (((size_t)phys - (size_t)p->phys))))
            return (void *)((uint64_t)phys - (uint64_t)p->phys + (uint64_t)p->virt);
    }

    void *virt = map_phys(phys, size);
    // if (virt == NULL) panic("Panic: Could not map meory\n");

    acpi_mem_map_node *n = malloc(sizeof(acpi_mem_map_node));
    // if (n == NULL) panic("Panic: Could not allocate memory\n");

    n->phys       = phys;
    n->size_bytes = size;
    n->virt       = virt;
    n->next       = NULL;
    acpi_mem_map_list_push_front(n);
    return virt;
}

void acpi_map_release_exact(void *virt, size_t size)
{
    acpi_mem_map_node *p = &acpi_mem_map_list_dummy;
    for (; p->next != NULL; p = p->next) {
        if (p->next->virt == virt && p->next->size_bytes == size)
            break;
    }

    acpi_mem_map_node *temp = p->next->next;
    unmap_phys(p->next->virt, p->next->size_bytes);
    free(p->next);
    p->next = temp;
}

RSDT *rsdt_phys = NULL;

XSDT *xsdt_phys = NULL;

static const char *loader_port_name = "/pmos/loader";

void request_acpi_tables()
{
    IPC_ACPI_Request_RSDT request = {IPC_ACPI_Request_RSDT_NUM, configuration_port};

    ports_request_t loader_port = get_port_by_name(loader_port_name, strlen(loader_port_name), 0);
    if (loader_port.result != SUCCESS) {
        printf("Warning: Could not get loader port. Error %li\n", loader_port.result);
        return;
    }

    result_t result = send_message_port(loader_port.port, sizeof(request), (char *)&request);
    if (result != SUCCESS) {
        printf("Warning: Could not send message to get the RSDT\n");
        return;
    }

    Message_Descriptor desc = {};
    unsigned char *message  = NULL;
    result                  = get_message(&desc, &message, configuration_port);

    if (result != SUCCESS) {
        printf("Warning: Could not get message\n");
        return;
    }

    if (desc.size < sizeof(IPC_ACPI_RSDT_Reply)) {
        printf("Warning: Recieved message which is too small\n");
        free(message);
        return;
    }

    IPC_ACPI_RSDT_Reply *reply = (IPC_ACPI_RSDT_Reply *)message;

    if (reply->type != IPC_ACPI_RSDT_Reply_NUM) {
        printf("Warning: Recieved unexepcted message type\n");
        free(message);
        return;
    }

    if (reply->result != 0) {
        printf("Warning: Did not get RSDT table: %i\n", reply->result);
    } else {
        rsdp_desc = (RSDP_descriptor20 *)reply->descriptor;
    }
    free(message);
}

#include <uacpi/uacpi.h>
#include <uacpi/event.h>
int acpi_init(uacpi_phys_addr rsdp_phys_addr) {
    uacpi_init_params init_params = {
        .rsdp = rsdp_phys_addr,
        .rt_params = {
            .flags = 0,
            .log_level = UACPI_LOG_DEBUG,
        },
    };

    uacpi_status ret = uacpi_initialize(&init_params);
    if (uacpi_unlikely_error(ret)) {
        fprintf(stderr, "uacpi_initialize error: %s", uacpi_status_to_string(ret));
        return -ENODEV;
    }

    ret = uacpi_namespace_load();
    if (uacpi_unlikely_error(ret)) {
        fprintf(stderr, "uacpi_namespace_load error: %s", uacpi_status_to_string(ret));
        return -ENODEV;
    }

    ret = uacpi_namespace_initialize();
    if (uacpi_unlikely_error(ret)) {
        fprintf(stderr, "uacpi_namespace_initialize error: %s", uacpi_status_to_string(ret));
        return -ENODEV;
    }

    ret = uacpi_finalize_gpe_initialization();
    if (uacpi_unlikely_error(ret)) {
        fprintf(stderr, "uACPI GPE initialization error: %s", uacpi_status_to_string(ret));
        return -ENODEV;
    }

    return 0;
}

void init_acpi()
{
    printf("Info: Initializing ACPI...\n");

    request_acpi_tables();

    acpi_revision = walk_acpi_tables();

    if (acpi_revision == -1) {
        printf("Warning: Did not initialize ACPI\n");
        return;
    }

    acpi_init((uacpi_phys_addr)rsdp_desc);

    printf("Walked ACPI tables! ACPI revision: %i\n", acpi_revision);
}