#include <stdint.h>
#include <pmos/ports.h>
#include <pmos/helpers.h>
#include <pmos/ipc.h>
#include <inttypes.h>
#include <stdlib.h>
#include <pmos/pmbus_object.h>
#include <assert.h>
#include <string.h>
#include <errno.h>

extern pmos_port_t main_port;

enum DeviceType {
    ACPI,
    PCI,
};

struct DeviceObject {
    pmos_msgloop_tree_node_t device_node;
    pmos_right_t right;
    uint64_t pmbus_id;
    char *acpi_name;
};

struct RegisterRequest {
    struct RegisterRequest *next;
    uint64_t id;

    union {
        char *acpi_name;
        struct PCIDevice *pci_device;
    };
    
    uint8_t *message_data;
    size_t message_data_size;
    uint64_t pmbus_sequence_id;

    pmos_msgloop_tree_node_t reply_node;

    struct DeviceObject *device_object;
    enum DeviceType device_type;
};

struct RegisterRequest *pending_registering = NULL;
uint64_t next_counter                       = 1;

static pmos_right_t pmbus_right   = 0;
static bool pmbus_right_requested = false;
const char *pmbus_right_name      = "/pmos/pmbus";
int request_pmbus_right(pmos_right_t *right_out)
{
    assert(right_out);
    if (pmbus_right) {
        *right_out = pmbus_right;
        return 0;
    }

    *right_out = 0;
    if (pmbus_right_requested)
        return 0;

    int result =
        (int)request_named_port(pmbus_right_name, strlen(pmbus_right_name), main_port, 0).result;
    if (result < 0) {
        fprintf(stderr, "devicesd: Fauled to request pmbus port: %i (%s)\n", result,
                strerror(-result));
        return -1;
    }

    pmbus_right_requested = true;
    return 0;
}

int publish_object_reply(Message_Descriptor *desc, void *buff, pmos_right_t *, pmos_right_t *,
                         void *ctx, struct pmos_msgloop_data *msgloop)
{
    struct RegisterRequest *rr = ctx;
    if (desc->size < sizeof(IPC_Generic_Msg)) {
        fprintf(stderr,
                "Received message from task %" PRIu64
                " in reply to publish object that is too small; size: 0x%x\n",
                desc->sender, (int)desc->size);
        goto error;
    }

    IPC_Generic_Msg *m = buff;
    switch (m->type) {
    case IPC_BUS_Publish_Object_Reply_NUM: {
        IPC_BUS_Publish_Object_Reply *r = buff;

        if (r->result == 0) {
            printf("[devicesd] Published %s, sequence number %" PRIi64 "!\n", rr->acpi_name,
                   r->sequence_number);
            rr->device_object->pmbus_id = r->sequence_number;
        } else {
            printf("[devicesd] Failed to publish %s! Result %i\n", rr->acpi_name, r->result);
        }
    } break;
    default:
        fprintf(stderr,
                "Recieved unknown message %" PRIu32 " from task %" PRIu64
                " waiting for object register reply...\n",
                m->type, desc->sender);
    }

error:
    // This is a send once right, so erase this...
    pmos_msgloop_erase(msgloop, &rr->reply_node);

    // And free ourselves....
    free(rr);

    // Continue with msgloop
    return 0;
}

int acpi_device_message_callback(Message_Descriptor *, void *, pmos_right_t *,
                                 pmos_right_t *, void *ctx, struct pmos_msgloop_data *)
{
    struct DeviceObject *obj = ctx;

    printf("Devicesd: received message for %s! (it's not implemented yet!!)\n", obj->acpi_name);
    return 0;
}

extern struct pmos_msgloop_data main_msgloop_data;

static void send_request(struct RegisterRequest *r)
{
    struct DeviceObject *obj   = malloc(sizeof(*obj));
    right_request_t send_right = {};
    pmos_right_t recieve_right;

    if (!obj) {
        fprintf(stderr, "devicesd: Couldn't allocate memory for DeviceObject...\n");
        goto error;
    }

    send_right = create_right(main_port, &recieve_right, 0);
    if (send_right.result) {
        fprintf(stderr, "devicesd: Couldn't create right for the object %s: %i (%s)\n",
                r->acpi_name, (int)send_right.result, strerror(-(int)send_right.result));
        goto error;
    }

    message_extra_t aux_data = {};
    aux_data.extra_rights[0] = send_right.right;

    auto result =
        send_message_right(pmbus_right, main_port, r->message_data, r->message_data_size, &aux_data, 0);
    if (result.result < 0) {
        fprintf(stderr, "devicesd: Couldn't send message to pmbus: %i (%s)\n", (int)result.result,
                strerror(-result.result));
    }
    assert(result.right);

    pmos_msgloop_node_set(&obj->device_node, recieve_right, acpi_device_message_callback, obj);
    pmos_msgloop_insert(&main_msgloop_data, &obj->device_node);

    obj->right       = send_right.right;
    r->device_object = obj;

    pmos_msgloop_node_set(&r->reply_node, result.right, publish_object_reply, r);
    pmos_msgloop_insert(&main_msgloop_data, &r->reply_node);

    obj              = NULL;
    send_right.right = 0;

error:
    delete_right(send_right.right);
    free(obj);
    free(r->message_data);
    r->message_data      = NULL;
    r->message_data_size = 0;
}

static void publish_send()
{
    while (pending_registering) {
        struct RegisterRequest *r = pending_registering;
        pending_registering       = r->next;

        send_request(r);
    }
}

int send_register_object(struct RegisterRequest *r)
{
    pmos_right_t pmbus_right;
    int result = request_pmbus_right(&pmbus_right);
    if (result < 0) {
        fprintf(stderr, "Failed to request pmbus right: %i\n", result);
        return -1;
    }

    if (pmbus_right != 0)
        send_request(r);
    else {
        r->next = pending_registering;
        pending_registering = r;
    }

    return 0;
}

#define BRED   "\e[1;31m"
#define BGRN   "\e[1;32m"
#define CRESET "\e[0m"

void named_port_notification(Message_Descriptor *desc, IPC_Kernel_Named_Port_Notification *n,
                             pmos_right_t first_right)
{
    // if (desc->sender != 0) {
    //     fprintf(stderr,
    //             "[devicesd] Warning: recieved IPC_Kernel_Named_Port_Notification from task %"
    //             PRIi64 " , expected kernel (0)\n", desc->sender);
    //     return;
    // }

    if (!first_right) {
        fprintf(stderr,
                BRED "devicesd: Recieved named right notification with no right..." CRESET "\n");
        return;
    }

    printf(BGRN "named port right %" PRIi64 CRESET "\n", first_right);

    size_t len = NAMED_PORT_NOTIFICATION_STR_LEN(desc->size);
    if (len == strlen(pmbus_right_name) && !memcmp(pmbus_right_name, n->port_name, len)) {
        pmbus_right = first_right;

        publish_send();
    }
}

int register_pci_object(pmos_bus_object_t *object_owning, struct PCIDevice *device)
{
    int result = -1;
    struct RegisterRequest *rr = calloc(sizeof(*rr), 1);
    if (!rr) {
        result = -ENOMEM;
        fprintf(stderr, "devicesd: Couldn't allocate memory for RegisterRequest\n");
        goto end;
    }

    rr->id = next_counter++;
    rr->device_type = PCI;
    rr->pci_device = device;

    uint8_t *msg    = NULL;
    size_t msg_size = 0;

    if (!pmos_bus_object_serialize_ipc(object_owning, &msg, &msg_size)) {
        fprintf(stderr, "Couldn't serialize pmbus object\n");
        goto end;
    }

    rr->message_data      = msg;
    rr->message_data_size = msg_size;

    result = send_register_object(rr);    

end:
    if (result && rr) {
        free(rr->message_data);
        free(rr);
    }

    pmos_bus_object_free(object_owning);
    return result;
}

int register_acpi_object(pmos_bus_object_t *object_owning)
{
    int result                 = -1;
    struct RegisterRequest *rr = calloc(sizeof(*rr), 1);
    if (!rr) {
        fprintf(stderr, "devicesd: Couldn't allocate memory for RegisterRequest\n");
        goto end;
    }

    rr->acpi_name = strdup(pmos_bus_object_get_name(object_owning));
    if (!rr->acpi_name) {
        fprintf(stderr, "devicesd: Couldn't allocate memory for acpi_name\n");
        goto end;
    }

    rr->id = next_counter++;
    rr->device_type = ACPI;

    uint8_t *msg    = NULL;
    size_t msg_size = 0;

    if (!pmos_bus_object_serialize_ipc(object_owning, &msg, &msg_size)) {
        fprintf(stderr, "Couldn't serialize pmbus object\n");
        goto end;
    }

    rr->message_data      = msg;
    rr->message_data_size = msg_size;

    result = send_register_object(rr);

end:
    if (result && rr) {
        free(rr->message_data);
        free(rr->acpi_name);
        free(rr);
    }

    pmos_bus_object_free(object_owning);
    return result;
}