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

pmos_bus_filter_conjunction *pmbus_filter_completion()
{
    pmos_bus_filter_conjunction *d = pmos_bus_filter_conjunction_create();
    if (!d)
        return NULL;

    pmos_bus_filter_equals *e = pmos_bus_filter_equals_create("subsystem", "devicesd");
    if (!e) {
        pmos_bus_filter_free(d);
        return NULL;
    }

    if (pmos_bus_filter_conjunction_add(d, e)) {
        pmos_bus_filter_free(e);
        pmos_bus_filter_free(d);
        return NULL;
    }

    e = pmos_bus_filter_equals_create("status", "ready");
    if (!e) {
        pmos_bus_filter_free(d);
        return NULL;
    }

    if (pmos_bus_filter_conjunction_add(d, e)) {
        pmos_bus_filter_free(e);
        pmos_bus_filter_free(d);
        return NULL;
    }

    return d;
}

const char *pmbus_name = "/pmos/pmbus";

pmos_port_t get_control_port();

pmos_right_t pmbus_right = INVALID_RIGHT;

pmos_right_t main_device_right = INVALID_RIGHT;
pmos_right_t aux_right = INVALID_RIGHT;

void request_ps2_rights()
{
    pmos_bus_filter_disjunction *keyboard = NULL, *aux = NULL;
    pmos_bus_filter_conjunction *completion = NULL;
    pmos_bus_filter_disjunction *filter = NULL;
    uint8_t *data = NULL; 

    if (!(keyboard = pmbus_filter_create(PRIMARY_PNP_IDS))) {
        fprintf(stderr, "[i8042] Error: Failed to create pmbus filter for keyboard\n");
        exit(1);
    }

    if (!(aux = pmbus_filter_create(AUX_PNP_IDS))) {
        fprintf(stderr, "[i8042] Error: Failed to create pmbus filter for aux\n");
        exit(1);
    }

    if (!(completion = pmbus_filter_completion())) {
        fprintf(stderr, "[i8042] Error: Failed to create pmbus filter for completion\n");
        exit(1);
    }

    filter = pmos_bus_filter_disjunction_create();
    if (!filter) {
        fprintf(stderr, "[i8042] Error: Failed to create pmbus filter\n");
        exit(1);
    }

    void *d = pmos_bus_filter_dup(keyboard);
    if (!d) {
        fprintf(stderr, "[i8042] Error: Failed to duplicate keyboard filter\n");
        exit(1);
    }
    if (pmos_bus_filter_disjunction_add(filter, d)) {
        fprintf(stderr, "[i8042] Error: Failed to add keyboard filter to main filter\n");
        exit(1);
    }
    d = pmos_bus_filter_dup(aux);
    if (!d) {
        fprintf(stderr, "[i8042] Error: Failed to duplicate aux filter\n");
        exit(1);
    }
    if (pmos_bus_filter_disjunction_add(filter, d)) {
        fprintf(stderr, "[i8042] Error: Failed to add aux filter to main filter\n");
        exit(1);
    }
    d = pmos_bus_filter_dup(completion);
    if (!d) {
        fprintf(stderr, "[i8042] Error: Failed to duplicate completion filter\n");
        exit(1);
    }
    if (pmos_bus_filter_conjunction_add(filter, d)) {
        fprintf(stderr, "[i8042] Error: Failed to add completion filter to main filter\n");
        exit(1);
    }


    size_t size = pmos_bus_filter_serialize_ipc(filter, NULL);
    data = malloc(size + sizeof(IPC_BUS_Request_Object));
    if (!data) {
        fprintf(stderr, "[i8042] Error: Failed to allocate memory for serialized filter\n");
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
        fprintf(stderr, "[i8042] Error: Failed to request pmbus right: %i\n", (int)req.result);
        exit(1);
    }
    pmbus_right = req.right;

    printf("[i8042] Info: Requested pmbus right: %" PRIu64 "\n", pmbus_right);

    pmos_port_t control_port = get_control_port();

    bool cont = true;
    while (cont) {
        req = send_message_right(pmbus_right, control_port, data, request_size, NULL, 0);
        if (req.result) {
            fprintf(stderr, "[i8042] Error: Failed to send message on pmbus right: %i\n", (int)req.result);
            exit(1);
        }

        uint8_t *reply_data;
        Message_Descriptor desc;
        pmos_right_t rights[4];

        result_t get_result = get_message(&desc, &reply_data, control_port, NULL, rights);
        if (get_result) {
            fprintf(stderr, "[i8042] Error: Failed to get message reply for pmbus right request: %i\n", (int)get_result);
            exit(1);
        }

        if (desc.size < sizeof(IPC_BUS_Request_Object_Reply)) {
            fprintf(stderr, "[i8042] Error: Received message with invalid size for pmbus right request: %i\n", (int)desc.size);
            exit(1);
        }

        IPC_BUS_Request_Object_Reply *reply = (IPC_BUS_Request_Object_Reply *)reply_data;
        if (reply->type != IPC_BUS_Request_Object_Reply_NUM) {
            fprintf(stderr, "[i8042] Error: Received message with invalid type for pmbus right request: %i\n", reply->type);
            exit(1);
        }

        if (reply->result != 0) {
            fprintf(stderr, "[i8042] Error: Received error result for pmbus right request: %i\n", (int)reply->result);
            exit(1);
        }

        msg->start_sequence_number = reply->next_sequence_number;

        pmos_bus_object_t *object = pmos_bus_object_deserialize_ipc(reply->object_data, desc.size - sizeof(IPC_BUS_Request_Object_Reply));
        if (!object) {
            fprintf(stderr, "[i8042] Error: Failed to deserialize pmbus object\n");
            exit(1);
        }

        if (pmos_bus_object_matches_filter(object, keyboard)) {
            if (main_device_right == INVALID_RIGHT) {
                main_device_right = rights[0];
                rights[0] = 0;
                printf("[i8042] Info: Received main device right: %" PRIu64 "\n", main_device_right);
            } else {
                printf("[i8042] Notice: Received duplicate main device right\n");
            }
        } else if (pmos_bus_object_matches_filter(object, aux)) {
            if (aux_right == INVALID_RIGHT) {
                aux_right = rights[0];
                rights[0] = 0;
                printf("[i8042] Info: Received aux device right: %" PRIu64 "\n", aux_right);
            } else {
                printf("[i8042] Notice: Received duplicate aux device right\n");
            }
        } else if (pmos_bus_object_matches_filter(object, completion)) {
            cont = false;
        }
        pmos_bus_object_free(object);
        free(reply_data);
        for (int i = 0; i < 4; i++) {
            if (rights[i] != 0)
                delete_right(rights[i]);
        }
    }

    delete_right(pmbus_right);
    free(data);
    pmos_bus_filter_free(filter);
    pmos_bus_filter_free(keyboard);
    pmos_bus_filter_free(aux);
    pmos_bus_filter_free(completion);
}