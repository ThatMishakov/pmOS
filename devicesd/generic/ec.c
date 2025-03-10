#include <pthread.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <uacpi/acpi.h>
#include <uacpi/event.h>
#include <uacpi/io.h>
#include <uacpi/kernel_api.h>
#include <uacpi/namespace.h>
#include <uacpi/opregion.h>
#include <uacpi/resources.h>
#include <uacpi/tables.h>
#include <uacpi/uacpi.h>
#include <uacpi/utilities.h>
#include <acpi.h>

struct ECDevice {
    uacpi_namespace_node *node, *gpe_node;
    struct acpi_gas control;
    struct acpi_gas data;

    struct ECDevice *next;
    uint16_t gpe_idx;
    bool initialized;
    bool requires_lock;
    pthread_spinlock_t lock;
};

#define EC_OBF     0x01
#define EC_IBF     0x02
#define EC_CMD     0x08
#define EC_BURST   0x10
#define EC_SCI_EVT 0x20
#define EC_SMI_EVT 0x40

#define RD_EC 0x80
#define WR_EC 0x81
#define BE_EC 0x82
#define BD_EC 0x83
#define QR_EC 0x84

#define BURST_ACK 0x90

struct ECDevice *ec_devices = NULL;
bool ec_initialized         = false;

static uacpi_interrupt_ret handle_ec_event(uacpi_handle ctx, uacpi_namespace_node *n, uacpi_u16 l);
static uacpi_status handle_ec_region(uacpi_region_op op, uacpi_handle op_data);

static void free_ec_devices()
{
    struct ECDevice *device = ec_devices;
    while (device) {
        struct ECDevice *next = device->next;
        free(device);
        device = next;
    }
}

static void ec_device_push_back(struct ECDevice *device)
{
    device->next = NULL;
    if (!ec_devices) {
        ec_devices = device;
        return;
    }

    struct ECDevice *last = ec_devices;
    while (last->next) {
        last = last->next;
    }

    last->next = device;
}

static int install_ec_handler(struct ECDevice *device)
{
    bool first_device = ec_devices == device;

    uacpi_namespace_node *address_space_node = first_device ? uacpi_namespace_root() : device->node;
    first_device                             = false;

    uacpi_status result = uacpi_install_address_space_handler(
        address_space_node, UACPI_ADDRESS_SPACE_EMBEDDED_CONTROLLER, handle_ec_region, device);
    if (result != UACPI_STATUS_OK) {
        printf("Failed to install EC handler\n");
        return -1;
    }

    uint64_t value = 0;
    uacpi_eval_simple_integer(device->node, "_GLK", &value);
    if (value) {
        device->requires_lock = true;
        printf("EC device has GLK\n");
    }

    result = uacpi_eval_simple_integer(device->node, "_GPE", &value);
    if (result != UACPI_STATUS_OK) {
        printf("Failed to read GPE: %i\n", result);
        uacpi_uninstall_address_space_handler(address_space_node,
                                            UACPI_ADDRESS_SPACE_EMBEDDED_CONTROLLER);
        return -1;
    }

    device->gpe_idx = value;
    result          = uacpi_install_gpe_handler(NULL, device->gpe_idx, UACPI_GPE_TRIGGERING_EDGE,
                                                handle_ec_event, device);
    if (result != UACPI_STATUS_OK) {
        printf("Failed to install GPE handler\n");
        uacpi_uninstall_address_space_handler(address_space_node,
                                            UACPI_ADDRESS_SPACE_EMBEDDED_CONTROLLER);
        return -1;
    }
    device->initialized = true;
    return 0;
}

static void init_from_ecdt()
{
    uacpi_table edcd;
    for (uacpi_status result               = uacpi_table_find_by_signature("ECDT", &edcd);
         result == UACPI_STATUS_OK; result = uacpi_table_find_next_with_same_signature(&edcd)) {

        struct acpi_ecdt *ecdt = edcd.ptr;
        printf("Found ECDT table with ID %s\n", ecdt->ec_id);

        uacpi_namespace_node *ec_node = NULL;
        result = uacpi_namespace_node_find(UACPI_NULL, ecdt->ec_id, &ec_node);
        if (result != UACPI_STATUS_OK) {
            printf("Failed to find EC device in namespace\n");
            free_ec_devices();
            uacpi_table_unref(&edcd);
            return;
        }

        struct ECDevice *device = malloc(sizeof(struct ECDevice));
        if (!device) {
            printf("Failed to allocate memory for EC device\n");
            free_ec_devices();
            uacpi_table_unref(&edcd);
            return;
        }

        device->node          = ec_node;
        device->gpe_node      = NULL;
        device->control       = ecdt->ec_control;
        device->data          = ecdt->ec_data;
        device->next          = NULL;
        device->initialized   = false;
        device->requires_lock = false;
        device->gpe_idx       = 0;
        pthread_spin_init(&device->lock, PTHREAD_PROCESS_PRIVATE);

        ec_device_push_back(device);

        if (!install_ec_handler(device)) {
            fprintf(stderr, "Failed to install EC handler...\n");
        }
    }
}

struct ec_init_ctx {
    struct acpi_gas control, data;
    size_t index;
};

static uacpi_iteration_decision find_ec_resources(void *ctx, uacpi_resource *resource)
{
    struct ec_init_ctx *init_ctx = ctx;
    struct acpi_gas *reg         = init_ctx->index == 0 ? &init_ctx->data : &init_ctx->control;

    switch (resource->type) {
    case UACPI_RESOURCE_TYPE_IO:
        reg->address            = resource->io.minimum;
        reg->register_bit_width = resource->io.length * 8;
        break;
    case UACPI_RESOURCE_TYPE_FIXED_IO:
        reg->address            = resource->fixed_io.address;
        reg->register_bit_width = resource->fixed_io.length * 8;
        break;
    default:
        return UACPI_ITERATION_DECISION_CONTINUE;
    }

    reg->address_space_id = UACPI_ADDRESS_SPACE_SYSTEM_IO;

    if (++init_ctx->index == 2)
        return UACPI_ITERATION_DECISION_BREAK;
    return UACPI_ITERATION_DECISION_CONTINUE;
}

static uint8_t read_reg(struct acpi_gas *reg)
{
    uint64_t val;
    uacpi_status result = uacpi_gas_read(reg, &val);
    if (result != UACPI_STATUS_OK) {
        printf("Failed to read EC register\n");
        return 0;
    }

    return val;
}

static void write_reg(struct acpi_gas *reg, uint8_t value)
{
    uacpi_status result = uacpi_gas_write(reg, value);
    if (result != UACPI_STATUS_OK) {
        printf("Failed to write EC register\n");
    }
}

static void ec_wait_for_bit(struct acpi_gas *reg, uint8_t bit, bool set)
{
    uint8_t status;
    do {
        status = read_reg(reg);
    } while ((status & bit) != (set ? bit : 0));
}

static void poll_ibf(struct ECDevice *device) { ec_wait_for_bit(&device->control, EC_IBF, false); }

static void poll_obf(struct ECDevice *device) { ec_wait_for_bit(&device->control, EC_OBF, true); }

static void ec_burst_enable(struct ECDevice *device)
{
    poll_ibf(device);
    write_reg(&device->control, BE_EC);
    poll_obf(device);
    uint8_t status = read_reg(&device->data);
    if (status != BURST_ACK)
        printf("Failed to enable EC burst mode. Out: %hhx\n", status);
}

static void ec_burst_disable(struct ECDevice *device)
{
    poll_ibf(device);
    write_reg(&device->control, BD_EC);
    ec_wait_for_bit(&device->control, EC_BURST, false);
}

static uint8_t ec_read(struct ECDevice *device, uint8_t offset)
{
    poll_ibf(device);
    write_reg(&device->control, RD_EC);
    poll_ibf(device);
    write_reg(&device->data, offset);
    poll_obf(device);
    return read_reg(&device->data);
}

static void ec_write(struct ECDevice *device, uint8_t offset, uint8_t value)
{
    poll_ibf(device);
    write_reg(&device->control, WR_EC);
    poll_ibf(device);
    write_reg(&device->data, offset);
    poll_ibf(device);
    write_reg(&device->data, value);
}

static uacpi_status ec_do_rw(uacpi_region_op op, uacpi_region_rw_data *data)
{
    struct ECDevice *device = data->handler_context;

    if (data->byte_width != 1) {
        printf("ec_do_rw: Invalid byte width %i\n", data->byte_width);
        return UACPI_STATUS_INVALID_ARGUMENT;
    }

    pthread_spin_lock(&device->lock);
    uint32_t out_seq;
    if (device->requires_lock) {
        uacpi_status result = uacpi_acquire_global_lock(0xFFFF, &out_seq);
        if (result != UACPI_STATUS_OK) {
            pthread_spin_unlock(&device->lock);
            return result;
        }
    }

    ec_burst_enable(device);

    uacpi_status result = UACPI_STATUS_OK;
    switch (op) {
    case UACPI_REGION_OP_READ:
        data->value = ec_read(device, data->offset);
        break;
    case UACPI_REGION_OP_WRITE:
        ec_write(device, data->offset, data->value);
        break;
    default:
        printf("ec_do_rw: Invalid operation %i\n", op);
        result = UACPI_STATUS_INVALID_ARGUMENT;
    }

    ec_burst_disable(device);
    if (device->requires_lock)
        uacpi_release_global_lock(out_seq);
    pthread_spin_unlock(&device->lock);

    return result;
}

static uacpi_status handle_ec_region(uacpi_region_op op, uacpi_handle op_data)
{
    switch (op) {
    case UACPI_REGION_OP_ATTACH:
    case UACPI_REGION_OP_DETACH:
        return UACPI_STATUS_OK;
    default:
        return ec_do_rw(op, op_data);
    }
}

static bool ec_check_event(struct ECDevice *device, uint8_t *event)
{
    uint8_t status = read_reg(&device->control);

    if (!(status & EC_SCI_EVT))
        return false;

    ec_burst_enable(device);
    poll_ibf(device);
    write_reg(&device->control, QR_EC);
    poll_obf(device);
    *event = read_reg(&device->data);
    ec_burst_disable(device);
    return true;
}

struct ec_event {
    struct ECDevice *device;
    uint8_t event;
};

static void to_method_name(char *name, uint8_t event)
{
    char *hex_chars = "0123456789ABCDEF";
    name[0]         = '_';
    name[1]         = 'Q';
    name[2]         = hex_chars[(event >> 4) & 0xF];
    name[3]         = hex_chars[event & 0xF];
    name[4]         = '\0';
}

static void ec_handle_query(uacpi_handle opaque)
{
    struct ec_event *event  = opaque;
    struct ECDevice *device = event->device;
    uint8_t ec_event        = event->event;
    free(event);

    char buff[5];
    to_method_name(buff, ec_event);

    uacpi_status result = uacpi_eval(device->node, buff, NULL, NULL);
    if (result != UACPI_STATUS_OK) {
        printf("[devicesd] Failed to handle EC event, result %i\n", result);
    }

    result = uacpi_finish_handling_gpe(device->gpe_node, device->gpe_idx);
    if (result != UACPI_STATUS_OK) {
        printf("Failed to finish handling GPE\n");
    }
}

static uacpi_interrupt_ret handle_ec_event(uacpi_handle ctx, uacpi_namespace_node *n, uacpi_u16 l)
{
    (void)n;
    (void)l;

    struct ECDevice *device = ctx;
    uint32_t global_lock_out_seq;
    uacpi_interrupt_ret ret = UACPI_GPE_REENABLE | UACPI_INTERRUPT_HANDLED;

    pthread_spin_lock(&device->lock);
    if (device->requires_lock) {
        uacpi_status result = uacpi_acquire_global_lock(0xFFFF, &global_lock_out_seq);
        if (result != UACPI_STATUS_OK) {
            pthread_spin_unlock(&device->lock);
            return UACPI_INTERRUPT_NOT_HANDLED;
        }
    }

    uint8_t event;
    if (!ec_check_event(device, &event))
        goto ret;

    struct ec_event *ec_event = malloc(sizeof(struct ec_event));
    if (!ec_event) {
        printf("Failed to allocate memory for EC event\n");
        ret = UACPI_INTERRUPT_NOT_HANDLED;
        goto ret;
    }

    if (device->requires_lock)
        uacpi_release_global_lock(global_lock_out_seq);
    pthread_spin_unlock(&device->lock);

    ec_event->device = device;
    ec_event->event  = event;

    uacpi_status result =
        uacpi_kernel_schedule_work(UACPI_WORK_GPE_EXECUTION, ec_handle_query, ec_event);
    if (result != UACPI_STATUS_OK) {
        printf("Failed to schedule work\n");
        free(ec_event);
        ret = UACPI_INTERRUPT_NOT_HANDLED;
    }

    ret = UACPI_INTERRUPT_HANDLED;
    return ret;

ret:
    if (device->requires_lock)
        uacpi_release_global_lock(global_lock_out_seq);
    pthread_spin_unlock(&device->lock);
    return ret;
}

void init_ec()
{
    init_from_ecdt();
}

static int match_ec_pnp(uacpi_namespace_node *node,
                                             uacpi_namespace_node_info *)
{
    struct ECDevice *device = ec_devices;
    while (device) {
        if (device->node == node)
            break;

        device = device->next;
    }

    if (!device) {
        uacpi_resources *resources;

        uacpi_status result = uacpi_get_current_resources(node, &resources);
        if (result != UACPI_STATUS_OK) {
            return UACPI_ITERATION_DECISION_CONTINUE;
        }

        struct ec_init_ctx ctx = {};
        uacpi_for_each_resource(resources, find_ec_resources, &ctx);
        uacpi_free_resources(resources);

        if (ctx.index != 2) {
            printf("EC device didn't find needed resources\n");
            return UACPI_ITERATION_DECISION_CONTINUE;
        }

        device = malloc(sizeof(struct ECDevice));
        if (!device) {
            printf("Failed to allocate memory for EC device\n");
            return UACPI_ITERATION_DECISION_CONTINUE;
        }

        device->node          = node;
        device->gpe_node      = NULL;
        device->control       = ctx.control;
        device->data          = ctx.data;
        device->next          = NULL;
        device->initialized   = false;
        device->requires_lock = false;
        pthread_spin_init(&device->lock, PTHREAD_PROCESS_PRIVATE);
        device->gpe_idx = 0;
        ec_device_push_back(device);

        const uacpi_char *path = uacpi_namespace_node_generate_absolute_path(node);
        printf("Found EC device at %s\n", path);
        uacpi_kernel_free((void *)path);

        if (!install_ec_handler(device)) {
            fprintf(stderr, "Failed to install handler for the EC device");
        }
    }

    uacpi_status result = uacpi_enable_gpe(device->gpe_node, device->gpe_idx);
    if (result != UACPI_STATUS_OK) {
        printf("Failed to enable GPE\n");
    }

    return UACPI_ITERATION_DECISION_CONTINUE;
}

static const char *const ec_ids[] = {
    "PNP0C09",
    NULL,
};

static acpi_driver ec = {
    .device_name = "ACPI Embedded Controller",
    .pnp_ids = ec_ids,
    .device_probe = match_ec_pnp,
};

__attribute__((constructor)) static void add_ec()
{
    acpi_register_driver(&ec);
}