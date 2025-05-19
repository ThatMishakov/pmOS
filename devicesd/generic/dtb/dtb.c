#include "smoldtb.h"

#include <dtb/dtb.h>
#include <main.h>
#include <pmos/helpers.h>
#include <pmos/ipc.h>
#include <pmos/memory.h>
#include <stdlib.h>
#include <string.h>

static const char *loader_port_name = "/pmos/loader";

void *fdt_ptr = NULL;

uint64_t fdt_object = 0;
uint32_t fdt_offset = 0;
uint32_t fdt_size   = 0;

static void smoldtb_free(void *ptr, size_t size)
{
    (void)size;
    free(ptr);
}

static void smoldtb_panic(const char *msg)
{
    printf("smoldtb panic: %s\n", msg);
    fdt_ptr = NULL;
}

void init_dtb()
{
    unsigned char *message = NULL;

    printf("Info: Initializing DTB...\n");

    IPC_FDT_Request request = {IPC_FDT_Request_NUM, 0};

    auto loader_right = get_right_by_name(loader_port_name, strlen(loader_port_name), 0);
    if (loader_right.result != SUCCESS) {
        printf("Warning: Could not get loader right. Error %i\n", (int)loader_right.result);
        goto end;
    }

    result_t result = send_message_right(loader_right.right, configuration_port, (char *)&request, sizeof(request), NULL, 0).result;
    if (result != SUCCESS) {
        printf("Warning: Could not send message to get the RSDT\n");
        goto end;
    }

    Message_Descriptor desc = {};
    result                  = get_message(&desc, &message, configuration_port, NULL, NULL);

    if (result != SUCCESS) {
        printf("Warning: Could not get message\n");
        goto end;
    }

    if (desc.size < sizeof(IPC_FDT_Reply)) {
        printf("Warning: Recieved message which is too small\n");
        goto end;
    }

    IPC_FDT_Reply *reply = (IPC_FDT_Reply *)message;

    if (reply->type != IPC_FDT_Reply_NUM) {
        printf("Warning: Recieved unexepcted message type\n");
        goto end;
    }

    if (reply->result != 0) {
        printf("Warning: Did not get FDT memory object: %i\n", reply->result);
    } else {
        fdt_object = reply->fdt_mem_object_id;
        fdt_offset = reply->object_offset;
        fdt_size   = reply->object_size;

        mem_request_ret_t req =
            map_mem_object(0, NULL, fdt_size + fdt_offset, PROT_READ, fdt_object, 0);
        if (req.result != SUCCESS) {
            printf("Warning: Could not map FDT memory object: %li\n", req.result);
            goto end;
        }

        fdt_ptr = (void *)((char *)req.virt_addr + fdt_offset);

        dtb_ops ops = {.malloc = malloc, .free = smoldtb_free, .on_error = smoldtb_panic};
        dtb_init((uintptr_t)fdt_ptr, ops);
    }

end:
    free(message);
}