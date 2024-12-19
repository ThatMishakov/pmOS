
#include <uacpi/resources.h>
#include <uacpi/types.h>
#include <uacpi/kernel_api.h>
#include <stdbool.h>
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <uacpi/utilities.h>
#include <pthread.h>
#include <pmos/ports.h>
#include <pmos/ipc.h>
#include <pmos/helpers.h>
#include <pmos/interrupts.h>
#include <interrupts.h>
#include <uacpi/uacpi.h>

#define AMD_GPIO_ID "AMDI0030"

struct amd_gpio_pin {
    unsigned DebounceTmrOut     : 4; // Read-write
    unsigned DebounceTmrOutUnit : 1; // Read-write
    unsigned DebounceCntrl      : 2; // Read-write
    unsigned DebounceTmrLarge   : 1; // Read-write
    unsigned LevelTrig          : 1; // Read-write
    unsigned ActiveLevel        : 2; // Read-write
    unsigned InterruptEnable    : 2; // Read-write
    unsigned WakeCntrl          : 3; // Read-write
    unsigned PinSts             : 1; // Read-only
    unsigned reserved           : 3; // reserved
    unsigned PullUpEnable       : 1; // Read-write
    unsigned PullDownEnable     : 1; // Read-write
    unsigned OutputValue        : 1; // Read-write
    unsigned OutputEnable       : 1; // Read-write
    unsigned reserved2          : 4; // Reserved
    unsigned InterruptSts       : 1; // Read; Write-1-to-clear
    unsigned WakeSts            : 1; // Read; Write-1-to-clear
    unsigned Less2secSts        : 1; // Read-only; Set-by-hardware
    unsigned Less10secSts       : 1; // Read-only; Set-by-hardware
};
#define PIN_INTERRUPT_STATUS 0x10000000
#define PIN_INTERRUPT_MASK 0x1800

#define GPIO_Wake_Inter_Master (0xFC/4)
#define EnWinBlueBtn (1 << 15)
#define EOI (1 << 29)

#define GPIO_Interrupt_Status_Index_0 (0x2f8/4)
#define GPIO_Interrupt_Status_Index_1 (0x2fc/4)

struct amd_gpio_device
{
    uacpi_namespace_node *node;
    uint32_t fixed_memory_base; // Physical address
    uint32_t fixed_memory_size;
    bool rw;
    int write_status;
    struct amd_gpio_device *next;

    uint32_t gsi;
    int polarity;
    bool level_trigger;
    bool shared;

    uint8_t interrupt_pin_mask[32];

    pthread_t thread;
    bool have_thread;

    volatile uint32_t *virt_addr;
    int refcount;
};
struct amd_gpio_device *amd_gpio_devices = NULL;
static void release_amd_gpio(struct amd_gpio_device *device);

static bool is_interrupt_pin(struct amd_gpio_device *device, int index)
{
    assert(index < 256);
    return device->interrupt_pin_mask[index / 8] & (1 << (index % 8));
}

static void service_interrupt_on_pin(struct amd_gpio_device *device, int index)
{
    // Try to execute _LXX/_EXX method first and fall back to _EVT
    if (index < 256) {
        char buff[6];
        sprintf(buff, "_L%c%02X", device->level_trigger ? 'L' : 'H', index);

        uacpi_status result = uacpi_execute_simple(device->node, buff);
        if (result == UACPI_STATUS_OK)
            return;
        else if (result != UACPI_STATUS_NOT_FOUND) {
            printf("Failed to execute %s: %i\n", buff, result);
            return;
        }
    }

    uacpi_object *arg = uacpi_object_create_integer(index);
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
        printf("Failed to execute _EVT(%i): %i\n", index, result);
    }
    uacpi_object_unref(arg);
}

static void amd_gpio_interrupt_handler(struct amd_gpio_device *device)
{
    bool had_interrupts = false;
    uint64_t status = 0;
    status = device->virt_addr[GPIO_Interrupt_Status_Index_1];
    status <<= 32;
    status |= device->virt_addr[GPIO_Interrupt_Status_Index_0];

    // 0:45 are interrupt status bits
    status &= ((uint64_t)1 << 46) - 1;

    for (int i = 0; i < 46; i++) {
        uint64_t mask = (uint64_t)1 << i;
        if (!(status & mask))
            continue;

        // Each bit is for 4 registers
        int offset = i * 4;
        for (int j = 0; j < 4; j++) {
            uint32_t reg = device->virt_addr[offset + j];

            if (!(reg & PIN_INTERRUPT_STATUS) || !(reg & PIN_INTERRUPT_MASK))
                continue;
            
            had_interrupts = true;

            service_interrupt_on_pin(device, offset + j);


            reg = device->virt_addr[offset + j];
            if (!is_interrupt_pin(device, offset + j))
                // Disable interrupt
                reg &= ~PIN_INTERRUPT_MASK;

            device->virt_addr[offset + j] = reg;
        }
    }

    if (had_interrupts) {
        // Send EOI
        uint32_t reg = device->virt_addr[GPIO_Wake_Inter_Master];
        reg |= EOI;
        device->virt_addr[GPIO_Wake_Inter_Master] = reg;
    }
}

void *interrupt_thread(void *d)
{
    // TL;DR: This function calls amd_gpio_interrupt_handler when an interrupt is received
    struct amd_gpio_device *device = d;

    ports_request_t p = create_port(TASK_ID_SELF, 0);
    if (p.result != SUCCESS) {
        printf("Failed to create port\n");
        return NULL;
    }

    uint32_t int_vector = 0;
    int r = set_up_gsi(device->gsi, device->polarity == UACPI_POLARITY_ACTIVE_LOW, device->level_trigger, get_task_id(), p.port, &int_vector);
    if (r < 0) {
        printf("Failed to set up GSI\n");
        return NULL;
    }

    // TODO: a helper function to do all this could be nice...
    while (true)
    {
        Message_Descriptor msg;
        unsigned char *message = NULL;
        result_t r = get_message(&msg, &message, p.port);
        if (r != SUCCESS)
        {
            printf("Failed to get message\n");
            return NULL;
        }

        assert(msg.size >= sizeof(IPC_Generic_Msg));
        IPC_Generic_Msg *ipc_msg = (IPC_Generic_Msg *)message;
        if (ipc_msg->type == IPC_Kernel_Interrupt_NUM) {
            amd_gpio_interrupt_handler(device);
            complete_interrupt(int_vector);
        }
        // else if (ipc_msg->type == IPC_ISR_Unregister_NUM)
        //     break;
        else
            printf("Unknown message type\n");
    }

    release_amd_gpio(device);
}

static void amd_gpio_device_push_back(struct amd_gpio_device *device)
{
    device->next = NULL;
    if (!amd_gpio_devices) {
        amd_gpio_devices = device;
        return;
    }

    struct amd_gpio_device *last = amd_gpio_devices;
    while (last->next) {
        last = last->next;
    }

    last->next = device;
}

static bool prepare_amd_gpio(struct amd_gpio_device *device)
{
    device->virt_addr = uacpi_kernel_map(device->fixed_memory_base, device->fixed_memory_size);
    if (!device->virt_addr) {
        return false;
    }

    device->refcount++;
    // TODO: this is potentially a race condition
    int result = pthread_create(&device->thread, NULL, interrupt_thread, device);
    if (result != 0) {
        printf("Failed to create interrupt thread: %i\n", result);
        uacpi_kernel_unmap((void *)device->virt_addr, device->fixed_memory_size);
        device->virt_addr = NULL;
        device->refcount--;
        return false;
    }
    pthread_detach(device->thread);
    device->have_thread = true;

    return true;
}

static void release_amd_gpio(struct amd_gpio_device *device)
{
    if (!device) {
        return;
    }

    if (__atomic_sub_fetch(&device->refcount, 1, __ATOMIC_SEQ_CST) != 0)
        return;

    if (device->virt_addr) {
        uacpi_kernel_unmap((void *)device->virt_addr, device->fixed_memory_size);
    }

    free(device);
}

static uacpi_iteration_decision find_amd_gpio_resources(void *ctx, uacpi_resource *resource)
{
    struct amd_gpio_device *dev = ctx;
    switch (resource->type)
    {
    case UACPI_RESOURCE_TYPE_IRQ: {
        uacpi_resource_irq *irq = &resource->irq;
        //assert(irq->num_irqs == 1); // TODO?
        if (irq->num_irqs != 1) {
            // ?
            return UACPI_ITERATION_DECISION_CONTINUE;
        }
        dev->gsi = irq->irqs[0];
        dev->polarity = irq->polarity;
        dev->level_trigger = irq->triggering == UACPI_TRIGGERING_LEVEL;
        dev->shared = irq->sharing == UACPI_SHARED;
        break;
    }
    case UACPI_RESOURCE_TYPE_EXTENDED_IRQ: {
        uacpi_resource_extended_irq *irq = &resource->extended_irq;
        //assert(irq->num_irqs == 1); // TODO?
        if (irq->num_irqs != 1) {
            // ?
            return UACPI_ITERATION_DECISION_CONTINUE;
        }
        dev->gsi = irq->irqs[0];
        dev->polarity = irq->polarity;
        dev->level_trigger = irq->triggering == UACPI_TRIGGERING_LEVEL;
        dev->shared = irq->sharing == UACPI_SHARED;
        break;
    }
    case UACPI_RESOURCE_TYPE_FIXED_MEMORY32: {
        uacpi_resource_fixed_memory32 *fixed = &resource->fixed_memory32;
        if (dev->fixed_memory_size != 0) {
            // ??? don't know what to do about this, sorry not sorry
            return UACPI_ITERATION_DECISION_CONTINUE;
        }
        if (fixed->length != 0x400)
            return UACPI_ITERATION_DECISION_CONTINUE;

        dev->fixed_memory_base = fixed->address;
        dev->fixed_memory_size = fixed->length;
        dev->rw = fixed->write_status;
        break;
    }
    }
    return UACPI_ITERATION_DECISION_CONTINUE;
}

static void set_debounce(struct amd_gpio_pin *pin, uint32_t debounce)
{
    if (debounce) {
        pin->DebounceCntrl = 0b11; // Remove glitch
        // Table 279: Debounce Timer Definition
        // DebounceTmrLarge | DebounceTmrOutUnit |       Timer Unit        | Max Debounce Time
        //        0         |         0          |    61 usec (2 RtcClk)   |       915us
        //        0         |         1          |   183 usec (6 RtcClk)   |     2.75 msec
        //        1         |         0          | 15.56 msec (510 RtcClk) |      233 msec
        //        1         |         1          | 62.44 msec (2046 RtcClk)|      936 msec
        if (debounce < 61) {
            pin->DebounceTmrOut = 1;
            pin->DebounceTmrOutUnit = 0;
            pin->DebounceTmrLarge = 0;
        } else if (debounce < 976) {
            pin->DebounceTmrOut = debounce / 61;
            pin->DebounceTmrOutUnit = 0;
            pin->DebounceTmrLarge = 0;
        } else if (debounce < 2928) {
            pin->DebounceTmrOut = debounce / 183;
            pin->DebounceTmrOutUnit = 1;
            pin->DebounceTmrLarge = 0;
        } else if (debounce < 248960) {
            pin->DebounceTmrOut = debounce / 15560;
            pin->DebounceTmrOutUnit = 0;
            pin->DebounceTmrLarge = 1;
        } else if (debounce < 999040) {
            pin->DebounceTmrOut = debounce / 62440;
            pin->DebounceTmrOutUnit = 1;
            pin->DebounceTmrLarge = 1;
        } else {
            pin->DebounceTmrOut = 0b1111;
            pin->DebounceTmrOutUnit = 1;
            pin->DebounceTmrLarge = 1;
        }
    } else {
        pin->DebounceCntrl = 0;
        pin->DebounceTmrOut = 0;
        pin->DebounceTmrOutUnit = 0;
        pin->DebounceTmrLarge = 0;
    }
}

static uacpi_iteration_decision amd_gpio_configure_pin(void *ctx, uacpi_resource *resource)
{
    struct amd_gpio_device *dev = ctx;
    if (resource->type != UACPI_RESOURCE_TYPE_GPIO_CONNECTION) {
        return UACPI_ITERATION_DECISION_CONTINUE;
    }
    uacpi_resource_gpio_connection *c = &resource->gpio_connection;
    if (c->pin_table_length < 1)
        return UACPI_ITERATION_DECISION_CONTINUE;

    bool lever_trigger = c->interrupt.triggering == UACPI_TRIGGERING_LEVEL;
    // The same meaning as in ACPI
    int active_level = c->interrupt.polarity;
    unsigned pin = c->pin_table[0];
    assert(pin < 256);

    // ACPI uses 100us, and we use 1us
    uint32_t debounce = c->debounce_timeout * 10;
    if (pin == 0) {
        volatile uint32_t *reg = dev->virt_addr+GPIO_Wake_Inter_Master;
        if (*reg & EnWinBlueBtn) {
            debounce = 0;
        }
    }

    union {
        uint32_t as_uint;
        struct amd_gpio_pin as_struct;
    } t;

    dev->interrupt_pin_mask[pin / 8] |= 1 << (pin % 8);

    volatile uint32_t *reg = &dev->virt_addr[pin];
    
    t.as_uint = *reg;
    
    // 1. The driver should program the DebounceTmrOut/DebounceTmrOutUnit/DebounceCntrl bits according to
    // the data passed by BIOS through ASL code.
    set_debounce(&t.as_struct, debounce);

    // 2. Configure Bit[8], Bit[9] and Bit[10] (See GPIOx[0F8:000:step4]) according to the data passed by BIOS
    // through ASL code.
    t.as_struct.LevelTrig = lever_trigger;
    t.as_struct.ActiveLevel = active_level;

    // 3. Set Bit[11] = 1 and Bit[12] = 1 (See GPIOx[0F8:000:step4]) to enable interrupt delivery and interrupt sta-
    // tus then its interrupt status is at Bit[28] when the GPIO input is asserted
    t.as_struct.InterruptEnable = 0b11;
    t.as_struct.OutputEnable = 0;
    
    *reg = t.as_uint;
}

static uacpi_iteration_decision match_amd_gpio(void *cc, uacpi_namespace_node *node, uint32_t depth)
{
    printf("Found AMD GPIO device\n");
    (void)depth;
    (void)cc;
    struct amd_gpio_device *device = NULL;

    device = calloc(sizeof(struct amd_gpio_device), 1);
    if (!device) {
        printf("Failed to allocate memory for AMD GPIO device\n");
        goto fail;
    }
    device->refcount = 1;
    device->node = node;

    uacpi_resources *resources;
    uacpi_status result = uacpi_get_current_resources(node, &resources);
    if (result != UACPI_STATUS_OK) {
        printf("Failed to get resources for AMD GPIO device\n");
        goto fail;
    }

    uacpi_for_each_resource(resources, find_amd_gpio_resources, device);
    uacpi_free_resources(resources);

    if (!device->fixed_memory_base || !device->gsi) {
        printf("AMD GPIO device didn't find needed resources\n");
        goto fail;
    }

    if (!prepare_amd_gpio(device)) {
        printf("Failed to prepare AMD GPIO device\n");
        goto fail;
    }

    uacpi_for_each_device_resource(node, "_AEI", amd_gpio_configure_pin, device);
    amd_gpio_device_push_back(device);
    return UACPI_ITERATION_DECISION_CONTINUE;

fail:
    release_amd_gpio(device);
    return UACPI_ITERATION_DECISION_CONTINUE;
}

void gpio_initialize()
{
    printf("trying to find GPIO devices\n");
    uacpi_find_devices(AMD_GPIO_ID, match_amd_gpio, NULL);
}