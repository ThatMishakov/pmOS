#include <acpi.h>
#include <interrupts.h>
#include <stdint.h>
#include <stdlib.h>
#include <pmos/ports.h>
#include <pmos/ipc.h>
#include <pmos/helpers.h>
#include <pmos/interrupts.h>
#include <assert.h>
#include <pthread.h>

struct ged_device;
struct ged_interrupt_h {
    struct ged_interrupt_h *next;
    struct ged_device *parent;

    pthread_t thread;

    uint32_t gsi;
    bool active_low;
    bool level_trigger;
    bool shared;
};

struct ged_device {
    struct ged_device *next;
    uacpi_namespace_node *node;
    struct ged_interrupt_h *interrupts;
};

static struct ged_device *ged_devices = nullptr;
static void ged_push_back(struct ged_device *d)
{
    assert(d);
    d->next = ged_devices;
    ged_devices = d;
}

static void release_irq(struct ged_interrupt_h *i)
{
    // TODO: Cancel thread
    // but just leak memory for now...
    printf("Warning: leaking memory in release_irq\n");
}

static void release_ged(struct ged_device *d)
{
    if (!d)
        return;

    struct ged_interrupt_h *i;
    while ((i = d->interrupts)) {
        d->interrupts = i->next;

        release_irq(i);
    }

    free(d);
}

static void ged_interrupt_handle(struct ged_interrupt_h *irq)
{
    struct ged_device *device = irq->parent;

    switch (irq->gsi) {
    case 0 ... 255: {
        char buff[6];
        sprintf(buff, "_L%c%02X", irq->level_trigger ? 'L' : 'H', irq->gsi);

        uacpi_status result = uacpi_execute_simple(device->node, buff);
        if (result == UACPI_STATUS_OK)
            return;
        else if (result != UACPI_STATUS_NOT_FOUND) {
            printf("Failed to execute %s: %i\n", buff, result);
            return;
        }
    }
    [[fallthrough]];
    default: {
        uacpi_object *arg = uacpi_object_create_integer(irq->gsi);
        if (!arg) {
            printf("Failed to create integer object\n");
            return;
        }
        uacpi_object_array args = {
            .objects = &arg,
            .count = 1,
        };
    
        uacpi_status result = uacpi_execute(device->node, "_EVT", &args);
        if (result != UACPI_STATUS_OK) {
            printf("Failed to execute _EVT(%i): %i\n", irq->gsi, result);
        }
        uacpi_object_unref(arg);
    }
    }
}

static void *interrupt_thread(void *d)
{
    // TL;DR: This function calls amd_gpio_interrupt_handler when an interrupt is received
    struct ged_interrupt_h *irq = d;

    ports_request_t p = create_port(TASK_ID_SELF, 0);
    if (p.result != SUCCESS) {
        printf("Failed to create port\n");
        return NULL;
    }

    uint32_t int_vector = 0;
    int r = set_up_gsi(irq->gsi, irq->active_low, irq->level_trigger, get_task_id(), p.port,
                       &int_vector);
    if (r < 0) {
        printf("Failed to set up GSI\n");
        return NULL;
    }

    // TODO: a helper function to do all this could be nice...
    while (true) {
        Message_Descriptor msg;
        unsigned char *message = NULL;
        result_t r             = get_message(&msg, &message, p.port);
        if (r != SUCCESS) {
            printf("Failed to get message\n");
            return NULL;
        }

        assert(msg.size >= sizeof(IPC_Generic_Msg));
        IPC_Generic_Msg *ipc_msg = (IPC_Generic_Msg *)message;
        if (ipc_msg->type == IPC_Kernel_Interrupt_NUM) {
            ged_interrupt_handle(irq);
            complete_interrupt(int_vector);
        }
        // else if (ipc_msg->type == IPC_ISR_Unregister_NUM)
        //     break;
        else
            printf("Unknown message type\n");
    }
}

static uacpi_iteration_decision find_ged_resources(void *ctx, uacpi_resource *resource)
{
    struct ged_device *dev = ctx;
    struct ged_interrupt_h *irq = nullptr;

    switch (resource->type) {
    case UACPI_RESOURCE_TYPE_IRQ: {
        uacpi_resource_irq *acpi_irq     = &resource->irq;
        irq = calloc(sizeof(irq), 1);
        if (!irq)
            return UACPI_ITERATION_DECISION_CONTINUE;

        // assert(irq->num_irqs == 1); // TODO?
        if (acpi_irq->num_irqs != 1) {
            // ?
            return UACPI_ITERATION_DECISION_CONTINUE;
        }
        irq->gsi           = acpi_irq->irqs[0];
        irq->active_low    = acpi_irq->polarity == UACPI_POLARITY_ACTIVE_LOW;
        irq->level_trigger = acpi_irq->triggering == UACPI_TRIGGERING_LEVEL;
        irq->shared        = acpi_irq->sharing == UACPI_SHARED;
        break;
    }
    case UACPI_RESOURCE_TYPE_EXTENDED_IRQ: {
        uacpi_resource_extended_irq *acpi_irq = &resource->extended_irq;
        
        irq = calloc(sizeof(irq), 1);
        if (!irq)
            return UACPI_ITERATION_DECISION_CONTINUE;

        
        // assert(irq->num_irqs == 1); // TODO?
        if (acpi_irq->num_irqs != 1) {
            // ?
            return UACPI_ITERATION_DECISION_CONTINUE;
        }
        irq->gsi           = acpi_irq->irqs[0];
        irq->active_low    = acpi_irq->polarity == UACPI_POLARITY_ACTIVE_LOW;
        irq->level_trigger = acpi_irq->triggering == UACPI_TRIGGERING_LEVEL;
        irq->shared        = acpi_irq->sharing == UACPI_SHARED;
        break;
    }
    default:
        return UACPI_ITERATION_DECISION_CONTINUE;
    }

    irq->parent = dev;

    int result = pthread_create(&irq->thread, NULL, interrupt_thread, irq);
    if (result != 0) {
        printf("Failed to create interrupt thread: %i\n", result);
        free(irq);
        return UACPI_ITERATION_DECISION_CONTINUE;
    }
    pthread_detach(irq->thread);

    irq->next       = dev->interrupts;
    dev->interrupts = irq;

    printf("GED interrupt on GSI %u installed.\n", irq->gsi);

    return UACPI_ITERATION_DECISION_CONTINUE;
}

static int match_ged(uacpi_namespace_node *node, uacpi_namespace_node_info *)
{
    printf("Found GED\n");
    struct ged_device *device = NULL;

    device = calloc(sizeof(struct ged_device), 1);
    if (!device) {
        printf("Failed to allocate memory for GED\n");
        goto fail;
    }
    device->node = node;

    uacpi_status result = uacpi_for_each_device_resource(node, "_CRS", find_ged_resources, device);
    if (result != UACPI_STATUS_OK) {
        printf("Failed to get resources for GED\n");
        goto fail;
    }

    ged_push_back(device);
    return UACPI_ITERATION_DECISION_CONTINUE;

fail:
    release_ged(device);
    return UACPI_ITERATION_DECISION_CONTINUE;
}

static const char *const ged_ids[] = {
    "ACPI0013",
    NULL,
};

static acpi_driver ged = {
    .device_name  = "AMD GPIO Controller",
    .pnp_ids      = ged_ids,
    .device_probe = match_ged,
};

__attribute__((constructor)) static void add_ged_driver() { acpi_register_driver(&ged); }