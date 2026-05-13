#include <stddef.h>
#include <pmos/pmbus_object.h>
#include <pmos/ipc.h>
#include <pmos/helpers.h>
#include <inttypes.h>
#include <string.h>

const char *PRIMARY_PNP_IDS[] = {
    "PNP0300",
    "PNP0301",
    "PNP0302",
    "PNP0303",
    "PNP0304",
    "PNP0305",
    "PNP0306",
    "PNP0309",
    "PNP030A",
    "PNP030B",
    "PNP0320",
    "PNP0343",
    "PNP0344",
    "PNP0345",
    "PNP0347",
    "CPQA0D7",
    NULL,
};

const char *AUX_PNP_IDS[] = {
    "AUI0200",
    "FJC6000",
    "FJC6001",
    "PNP0F03",
    "PNP0F0B",
    "PNP0F0E",
    "PNP0F12",
    "PNP0F13",
    "PNP0F19",
    "PNP0F1C",
    "SYN0801",
    NULL,
};

pmos_bus_filter_conjunction *pmbus_filter_create(const char **ids)
{
    pmos_bus_filter_disjunction *d = pmos_bus_filter_disjunction_create();
    if (!d)
        return NULL;

    for (const char **id = ids; *id; ++id) {
        pmos_bus_filter_equals *e = pmos_bus_filter_equals_create("acpi_hid", *id);
        if (!e) {
            pmos_bus_filter_free(d);
            return NULL;
        }
        if (pmos_bus_filter_disjunction_add(d, e)) {
            pmos_bus_filter_free(e);
            pmos_bus_filter_free(d);
            return NULL;
        }

        e = pmos_bus_filter_equals_create("acpi_cid", *id);
        if (!e) {
            pmos_bus_filter_free(d);
            return NULL;
        }

        if (pmos_bus_filter_disjunction_add(d, e)) {
            pmos_bus_filter_free(e);
            pmos_bus_filter_free(d);
            return NULL;
        }
    }

    pmos_bus_filter_conjunction *c = pmos_bus_filter_conjunction_create();
    if (!c) {
        pmos_bus_filter_free(d);
        return NULL;
    }

    if (pmos_bus_filter_conjunction_add(c, d)) {
        pmos_bus_filter_free(c);
        pmos_bus_filter_free(d);
        return NULL;
    }

    pmos_bus_filter_equals *e = pmos_bus_filter_equals_create("type", "acpi_device");
    if (!e) {
        pmos_bus_filter_free(c);
        return NULL;
    }

    if (pmos_bus_filter_conjunction_add(c, e)) {
        pmos_bus_filter_free(c);
        pmos_bus_filter_free(e);
        return NULL;
    }

    return c;
}

pmos_bus_filter_disjunction *create_filter()
{
    pmos_bus_filter_disjunction *d = pmos_bus_filter_disjunction_create();
    if (!d)
        return NULL;

    pmos_bus_filter_conjunction *primary = pmbus_filter_create(PRIMARY_PNP_IDS);
    if (!primary) {
        pmos_bus_filter_free(d);
        return NULL;
    }

    if (pmos_bus_filter_disjunction_add(d, primary)) {
        pmos_bus_filter_free(d);
        pmos_bus_filter_free(primary);
        return NULL;
    }

    pmos_bus_filter_conjunction *aux = pmbus_filter_create(AUX_PNP_IDS);
    if (!aux) {
        pmos_bus_filter_free(d);
        return NULL;
    }

    if (pmos_bus_filter_disjunction_add(d, aux)) {
        pmos_bus_filter_free(d);
        pmos_bus_filter_free(aux);
        return NULL;
    }

    return d;
}

const char *pmbus_name = "/pmos/pmbus";

pmos_port_t get_control_port();

pmos_right_t pmbus_right = INVALID_RIGHT;

void request_ps2_rights()
{
    uint8_t *data = NULL; 
    pmos_bus_filter_disjunction *filter = create_filter();
    if (!filter) {
        fprintf(stderr, "Failed to create pmbus filter\n");
        exit(1);
    }

    size_t size = pmos_bus_filter_serialize_ipc(filter, NULL);
    data = malloc(size + sizeof(IPC_BUS_Request_Object));
    if (!data) {
        fprintf(stderr, "Failed to allocate memory for serialized filter\n");
        exit(1);
    }

    size_t request_size = sizeof(IPC_BUS_Request_Object) + size;

    IPC_BUS_Request_Object *msg = (IPC_BUS_Request_Object *)data;
    msg->type = IPC_BUS_Request_Object_NUM;
    msg->flags = 0;
    msg->start_sequence_number = 0;

    pmos_bus_filter_serialize_ipc(filter, msg->data);

    right_request_t req = get_right_by_name(pmbus_name, strlen(pmbus_name), 0);
    if (req.result) {
        fprintf(stderr, "Failed to request pmbus right: %i\n", (int)req.result);
        exit(1);
    }
    pmbus_right = req.right;

    printf("Requested pmbus right: %" PRIu64 "\n", pmbus_right);

    pmos_port_t control_port = get_control_port();

    bool cont = true;
    while (cont) {
        req = send_message_right(pmbus_right, control_port, data, request_size, NULL, 0);
        if (req.result) {
            fprintf(stderr, "Failed to send message on pmbus right: %i\n", (int)req.result);
            exit(1);
        }

        uint8_t *reply_data;
        Message_Descriptor desc;
        pmos_right_t rights[4];

        result_t get_result = get_message(&desc, &reply_data, control_port, NULL, rights);
        if (get_result) {
            fprintf(stderr, "Failed to get message reply for pmbus right request: %i\n", (int)get_result);
            exit(1);
        }

        if (desc.size < sizeof(IPC_BUS_Request_Object_Reply)) {
            fprintf(stderr, "Received message with invalid size for pmbus right request: %i\n", (int)desc.size);
            exit(1);
        }

        IPC_BUS_Request_Object_Reply *reply = (IPC_BUS_Request_Object_Reply *)reply_data;
        if (reply->type != IPC_BUS_Request_Object_Reply_NUM) {
            fprintf(stderr, "Received message with invalid type for pmbus right request: %i\n", reply->type);
            exit(1);
        }

        if (reply->result != 0) {
            fprintf(stderr, "Received error result for pmbus right request: %i\n", (int)reply->result);
            exit(1);
        }

        printf("Got pmbus object\n");
    }

    free(data);
    pmos_bus_filter_free(filter);
}