#include <assert.h>
#include <interrupts.h>
#include <pci/pci.h>
#include <pmos/helpers.h>
#include <pmos/interrupts.h>
#include <pmos/io.h>
#include <pmos/ipc.h>
#include <pmos/memory.h>
#include <pmos/special.h>
#include <pmos/system.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <uacpi/kernel_api.h>
#include <unistd.h>

void *uacpi_kernel_alloc(uacpi_size size) { return malloc(size); }

void *uacpi_kernel_calloc(uacpi_size count, uacpi_size size) { return calloc(count, size); }

void uacpi_kernel_free(void *mem) { free(mem); }

void uacpi_kernel_vlog(enum uacpi_log_level ll, const char *fmt, uacpi_va_list l)
{
    fprintf(stderr, "[uACPI Log %i] ", ll);
    vfprintf(stderr, fmt, l);
}

void uacpi_kernel_log(enum uacpi_log_level ll, const char *log)
{
    fprintf(stderr, "[uACPI Log %i] %s", ll, log);
}

void *uacpi_kernel_map(uacpi_phys_addr addr, uacpi_size len)
{
    uint64_t page_alligned_addr = addr & ~0xFFFUL;
    uint64_t page_offset        = addr & 0xFFFUL;
    uint64_t size_alligned      = (len + page_offset + 0xFFF) & ~0xFFFUL;

    mem_request_ret_t r = create_phys_map_region(TASK_ID_SELF, NULL, size_alligned,
                                                 PROT_READ | PROT_WRITE, page_alligned_addr);
    if (r.result != SUCCESS)
        return NULL;

    return (char *)r.virt_addr + page_offset;
}

void uacpi_kernel_unmap(void *addr, uacpi_size len)
{
    (void)len;

    void *region_start_alligned = (void *)((uint64_t)addr & ~0xFFFUL);

    release_region(TASK_ID_SELF, region_start_alligned);
}

uacpi_handle uacpi_kernel_create_mutex(void)
{
    pthread_mutex_t *mutex = malloc(sizeof(pthread_mutex_t));
    pthread_mutex_init(mutex, NULL);
    return (uacpi_handle)mutex;
}

void uacpi_kernel_free_mutex(uacpi_handle handle)
{
    pthread_mutex_destroy((pthread_mutex_t *)handle);
    free(handle);
}

uacpi_status uacpi_kernel_acquire_mutex(uacpi_handle mutex, uacpi_u16 timeout)
{
    // if (timeout == 0xFFFF)
    //     return pthread_mutex_lock((pthread_mutex_t *)mutex) == 0 ? UACPI_TRUE : UACPI_FALSE;

    // struct timespec ts;
    // ts.tv_sec = timeout / 1000;
    // ts.tv_nsec = (timeout % 1000) * 1000000;

    // return pthread_mutex_timedlock((pthread_mutex_t *)mutex, &ts) == 0 ? UACPI_TRUE :
    // UACPI_FALSE;

    // Timed mutex lock is not yet implemented
    int result = pthread_mutex_lock((pthread_mutex_t *)mutex);
    if (!result)
        return UACPI_STATUS_OK;

    return UACPI_STATUS_INTERNAL_ERROR;
}

void uacpi_kernel_release_mutex(uacpi_handle mutex)
{
    pthread_mutex_unlock((pthread_mutex_t *)mutex);
}

uacpi_handle uacpi_kernel_create_spinlock(void)
{
    pthread_spinlock_t *spinlock = malloc(sizeof(pthread_spinlock_t));
    pthread_spin_init(spinlock, PTHREAD_PROCESS_PRIVATE);
    return (uacpi_handle)spinlock;
}

void uacpi_kernel_free_spinlock(uacpi_handle handle)
{
    pthread_spin_destroy((pthread_spinlock_t *)handle);
    free(handle);
}

uacpi_thread_id uacpi_kernel_get_thread_id(void) { return pthread_self(); }

uacpi_cpu_flags uacpi_kernel_lock_spinlock(uacpi_handle spinlock)
{
    pthread_spin_lock((pthread_spinlock_t *)spinlock);
    return 0;
}

void uacpi_kernel_unlock_spinlock(uacpi_handle spinlock, uacpi_cpu_flags flags)
{
    (void)flags;
    pthread_spin_unlock((pthread_spinlock_t *)spinlock);
}

struct uacpi_work {
    struct uacpi_work *next;
    pthread_t thread;

    uacpi_work_type type;
    uacpi_work_handler handler;
    uacpi_handle ctx;
};

static void *uacpi_work(void *arg)
{
    struct uacpi_work *work = arg;
    if (work->type == UACPI_WORK_GPE_EXECUTION)
        set_affinity(TASK_ID_SELF, 1, 0);

#if defined(__x86_64__) || defined(__i386__)
    pmos_request_io_permission();
#endif

    work->handler(work->ctx);

    return NULL;
}

struct uacpi_work *work_queue_start = NULL, *work_queue_end = NULL;
pthread_spinlock_t work_queue_lock;
__attribute__((constructor)) void uacpi_work_queue_init(void)
{
    pthread_spin_init(&work_queue_lock, PTHREAD_PROCESS_PRIVATE);
}

uacpi_status uacpi_kernel_schedule_work(uacpi_work_type type, uacpi_work_handler handler,
                                        uacpi_handle ctx)
{
    struct uacpi_work *work = malloc(sizeof(struct uacpi_work));
    if (work == NULL)
        return UACPI_STATUS_OUT_OF_MEMORY;

    work->type    = type;
    work->handler = handler;
    work->ctx     = ctx;
    work->next    = NULL;

    int t = pthread_create(&work->thread, NULL, uacpi_work, work);
    if (t != 0) {
        free(work);
        return UACPI_STATUS_INTERNAL_ERROR;
    }

    pthread_spin_lock(&work_queue_lock);
    if (work_queue_start == NULL) {
        work_queue_start = work_queue_end = work;
    } else {
        work_queue_end->next = work;
        work_queue_end       = work;
    }
    pthread_spin_unlock(&work_queue_lock);

    return UACPI_STATUS_OK;
}

uacpi_status uacpi_kernel_wait_for_work_completion(void)
{
    while (true) {
        pthread_spin_lock(&work_queue_lock);
        struct uacpi_work *work = work_queue_start;
        if (work == NULL) {
            pthread_spin_unlock(&work_queue_lock);
            break;
        }

        work_queue_start = work->next;
        if (work_queue_start == NULL)
            work_queue_end = NULL;
        pthread_spin_unlock(&work_queue_lock);

        pthread_join(work->thread, NULL);
        free(work);
    }

    return UACPI_STATUS_OK;
}

struct event_handle {
    pthread_cond_t cond;
    pthread_mutex_t mutex;
    size_t counter;
};

uacpi_handle uacpi_kernel_create_event(void)
{
    struct event_handle *event = malloc(sizeof(struct event_handle));
    if (event == NULL)
        return NULL;

    int r = pthread_cond_init(&event->cond, NULL);
    if (r != 0) {
        free(event);
        return NULL;
    }

    r = pthread_mutex_init(&event->mutex, NULL);
    if (r != 0) {
        pthread_cond_destroy(&event->cond);
        free(event);
        return NULL;
    }

    event->counter = 0;
    return event;
}

void uacpi_kernel_free_event(uacpi_handle handle)
{
    struct event_handle *event = handle;
    pthread_cond_destroy(&event->cond);
    pthread_mutex_destroy(&event->mutex);
    free(event);
}

uacpi_bool uacpi_kernel_wait_for_event(uacpi_handle handle, uacpi_u16 timeout)
{
    // Timeout is not yet implemented
    (void)timeout;
    int r;

    struct event_handle *event = handle;
    r                          = pthread_mutex_lock(&event->mutex);
    if (r != 0)
        return UACPI_FALSE;
    while (event->counter == 0) {
        r = pthread_cond_wait(&event->cond, &event->mutex);
        if (r != 0) {
            pthread_mutex_unlock(&event->mutex);
            return UACPI_FALSE;
        }
    }
    event->counter--;
    pthread_mutex_unlock(&event->mutex);

    return UACPI_TRUE;
}

void uacpi_kernel_signal_event(uacpi_handle handle)
{
    struct event_handle *event = handle;
    pthread_mutex_lock(&event->mutex);
    event->counter++;
    pthread_cond_signal(&event->cond);
    pthread_mutex_unlock(&event->mutex);
}

void uacpi_kernel_reset_event(uacpi_handle handle)
{
    struct event_handle *event = handle;
    pthread_mutex_lock(&event->mutex);
    event->counter = 0;
    pthread_mutex_unlock(&event->mutex);
}

uacpi_status uacpi_kernel_pci_device_open(uacpi_pci_address address, uacpi_handle *out_handle)
{
    if (!pci_fully_working) {
        if (address.segment != 0)
            return UACPI_STATUS_NOT_FOUND;

        struct PCIDevicePtr *p = malloc(sizeof(*p));
        if (!p)
            return UACPI_STATUS_OUT_OF_MEMORY;

        int result = fill_device_early(p, address.bus, address.device, address.function);
        if (result) {
            free(p);
            return UACPI_STATUS_NOT_FOUND;
        }

        *out_handle = p;
        return UACPI_STATUS_OK;
    }

    struct PCIHostBridge *g = pci_host_bridge_find(address.segment);
    if (g == NULL) {
        return UACPI_STATUS_NOT_FOUND;
    }

    struct PCIDevicePtr *p = malloc(sizeof(*p));
    if (!p)
        return UACPI_STATUS_OUT_OF_MEMORY;

    int result = fill_device(p, g, address.bus, address.device, address.function);
    if (result) {
        free(p);
        return UACPI_STATUS_NOT_FOUND;
    }

    *out_handle = p;

    return UACPI_STATUS_OK;
}

uacpi_status uacpi_kernel_pci_read8(uacpi_handle device, uacpi_size offset, uacpi_u8 *value)
{
    uint32_t v = pci_read_register(device, offset / 4);
    *value     = (v >> ((offset % 4) * 8)) & 0xFF;
    return UACPI_STATUS_OK;
}

uacpi_status uacpi_kernel_pci_read16(uacpi_handle device, uacpi_size offset, uacpi_u16 *value)
{
    uint32_t v = pci_read_register(device, offset / 4);
    *value     = (v >> ((offset % 4) * 8)) & 0xFFFF;
    return UACPI_STATUS_OK;
}

uacpi_status uacpi_kernel_pci_read32(uacpi_handle device, uacpi_size offset, uacpi_u32 *value)
{
    *value = pci_read_register(device, offset / 4);
    return UACPI_STATUS_OK;
}

uacpi_status uacpi_kernel_pci_write8(uacpi_handle device, uacpi_size offset, uacpi_u8 value)
{
    uint32_t v = pci_read_register(device, offset / 4);
    v &= ~(0xFF << ((offset % 4) * 8));
    v |= value << ((offset % 4) * 8);
    pci_write_register(device, offset / 4, v);
    return UACPI_STATUS_OK;
}

uacpi_status uacpi_kernel_pci_write16(uacpi_handle device, uacpi_size offset, uacpi_u16 value)
{
    uint32_t v = pci_read_register(device, offset / 4);
    v &= ~(0xFFFF << ((offset % 4) * 8));
    v |= value << ((offset % 4) * 8);
    pci_write_register(device, offset / 4, v);
    return UACPI_STATUS_OK;
}

uacpi_status uacpi_kernel_pci_write32(uacpi_handle device, uacpi_size offset, uacpi_u32 value)
{
    pci_write_register(device, offset / 4, value);
    return UACPI_STATUS_OK;
}

void uacpi_kernel_pci_device_close(uacpi_handle h)
{
    printf("!!!!! pci device close !!! %lx\n", h);
    free(h);
}

uacpi_status uacpi_kernel_raw_io_read(uacpi_io_addr address, uacpi_u8 byte_width,
                                      uacpi_u64 *out_value)
{
#if defined(__x86_64__) || defined(__i386__)
    switch (byte_width) {
    case 1:
        *out_value = io_in8(address);
        break;
    case 2:
        *out_value = io_in16(address);
        break;
    case 4:
        *out_value = io_in32(address);
        break;
    default:
        return UACPI_STATUS_INVALID_ARGUMENT;
    }
    return UACPI_STATUS_OK;
#else
    (void)address, (void)byte_width, (void)out_value;
    return UACPI_STATUS_UNIMPLEMENTED;
#endif
}

uacpi_status uacpi_kernel_raw_io_write(uacpi_io_addr address, uacpi_u8 byte_width, uacpi_u64 value)
{
#if defined(__x86_64__) || defined(__i386__)
    switch (byte_width) {
    case 1:
        io_out8(address, value);
        break;
    case 2:
        io_out16(address, value);
        break;
    case 4:
        io_out32(address, value);
        break;
    default:
        return UACPI_STATUS_INVALID_ARGUMENT;
    }
    return UACPI_STATUS_OK;
#else
    (void)address, (void)byte_width, (void)value;
    return UACPI_STATUS_UNIMPLEMENTED;
#endif
}

uacpi_status uacpi_kernel_pci_read(uacpi_handle c, uacpi_size offset, uacpi_u8 byte_width,
                                   uacpi_u64 *value)
{
    uint32_t v = pci_read_register(c, offset / 4);
    switch (byte_width) {
    case 1:
        *value = (v >> ((offset % 4) * 8)) & 0xFF;
        break;
    case 2:
        *value = (v >> ((offset % 4) * 8)) & 0xFFFF;
        break;
    case 4:
        *value = v;
        break;
    default:
        return UACPI_STATUS_INVALID_ARGUMENT;
    }

    return UACPI_STATUS_OK;
}

uacpi_status uacpi_kernel_pci_write(uacpi_handle c, uacpi_size offset, uacpi_u8 byte_width,
                                    uacpi_u64 value)
{
    uint32_t v = pci_read_register(c, offset / 4);
    switch (byte_width) {
    case 1:
        v &= ~(0xFF << ((offset % 4) * 8));
        v |= (value & 0xFF) << ((offset % 4) * 8);
        break;
    case 2:
        v &= ~(0xFFFF << ((offset % 4) * 8));
        v |= (value & 0xFFFF) << ((offset % 4) * 8);
        break;
    case 4:
        v = value;
        break;
    default:
        return UACPI_STATUS_INVALID_ARGUMENT;
    }

    pci_write_register(c, offset / 4, v);
    return UACPI_STATUS_OK;
}

uacpi_status uacpi_kernel_io_map(uacpi_io_addr base, uacpi_size len, uacpi_handle *out_handle)
{
    (void)len;
    *out_handle = (void *)base;
    return UACPI_STATUS_OK;
}

void uacpi_kernel_io_unmap(uacpi_handle handle) { (void)handle; }

uacpi_status uacpi_kernel_io_read(uacpi_handle h, uacpi_size offset, uacpi_u8 byte_width,
                                  uacpi_u64 *value)
{
    return uacpi_kernel_raw_io_read((uacpi_io_addr)h + offset, byte_width, value);
}

uacpi_status uacpi_kernel_io_read8(uacpi_handle h, uacpi_size offset, uacpi_u8 *value)
{
    return uacpi_kernel_raw_io_read((uacpi_io_addr)h + offset, 1, (uacpi_u64 *)value);
}

uacpi_status uacpi_kernel_io_read16(uacpi_handle h, uacpi_size offset, uacpi_u16 *value)
{
    return uacpi_kernel_raw_io_read((uacpi_io_addr)h + offset, 2, (uacpi_u64 *)value);
}

uacpi_status uacpi_kernel_io_read32(uacpi_handle h, uacpi_size offset, uacpi_u32 *value)
{
    return uacpi_kernel_raw_io_read((uacpi_io_addr)h + offset, 4, (uacpi_u64 *)value);
}

uacpi_status uacpi_kernel_io_write(uacpi_handle h, uacpi_size offset, uacpi_u8 byte_width,
                                   uacpi_u64 value)
{
    return uacpi_kernel_raw_io_write((uacpi_io_addr)h + offset, byte_width, value);
}

uacpi_status uacpi_kernel_io_write8(uacpi_handle h, uacpi_size offset, uacpi_u8 value)
{
    return uacpi_kernel_raw_io_write((uacpi_io_addr)h + offset, 1, value);
}

uacpi_status uacpi_kernel_io_write16(uacpi_handle h, uacpi_size offset, uacpi_u16 value)
{
    return uacpi_kernel_raw_io_write((uacpi_io_addr)h + offset, 2, value);
}

uacpi_status uacpi_kernel_io_write32(uacpi_handle h, uacpi_size offset, uacpi_u32 value)
{
    return uacpi_kernel_raw_io_write((uacpi_io_addr)h + offset, 4, value);
}

uacpi_status uacpi_kernel_raw_memory_read(uacpi_phys_addr address, uacpi_u8 byte_width,
                                          uacpi_u64 *out_value)
{
    void *ptr = uacpi_kernel_map(address, byte_width);
    if (ptr == NULL)
        return UACPI_STATUS_OUT_OF_MEMORY;
    uacpi_status status = UACPI_STATUS_OK;

    switch (byte_width) {
    case 1:
        *out_value = *(volatile uint8_t *)ptr;
        break;
    case 2:
        *out_value = *(volatile uint16_t *)ptr;
        break;
    case 4:
        *out_value = *(volatile uint32_t *)ptr;
        break;
    case 8:
        *out_value = *(volatile uint64_t *)ptr;
        break;
    default:
        status = UACPI_STATUS_INVALID_ARGUMENT;
    }

    uacpi_kernel_unmap(ptr, byte_width);
    return status;
}

uacpi_status uacpi_kernel_raw_memory_write(uacpi_phys_addr address, uacpi_u8 byte_width,
                                           uacpi_u64 in_value)
{
    void *ptr = uacpi_kernel_map(address, byte_width);
    if (ptr == NULL)
        return UACPI_STATUS_OUT_OF_MEMORY;
    uacpi_status status = UACPI_STATUS_OK;

    switch (byte_width) {
    case 1:
        *(volatile uint8_t *)ptr = in_value;
        break;
    case 2:
        *(volatile uint16_t *)ptr = in_value;
        break;
    case 4:
        *(volatile uint32_t *)ptr = in_value;
        break;
    case 8:
        *(volatile uint64_t *)ptr = in_value;
        break;
    default:
        status = UACPI_STATUS_INVALID_ARGUMENT;
    }

    uacpi_kernel_unmap(ptr, byte_width);
    return status;
}

void uacpi_kernel_sleep(uacpi_u64 msec) { usleep(msec * 1000); }

void uacpi_kernel_stall(uacpi_u8 usec) { usleep(usec); }

uacpi_u64 uacpi_kernel_get_nanoseconds_since_boot(void)
{
    return pmos_get_time(GET_TIME_NANOSECONDS_SINCE_BOOTUP).value;
}

struct isr_data {
    pthread_t isr_thread;
    pmos_port_t right;
    uacpi_interrupt_handler handler;
    uacpi_handle ctx;
    uacpi_u32 irq;
    size_t refcount;
};

#define IPC_ISR_Reply_NUM 0xff000001
struct IPC_ISR_Reply {
    uint32_t type;
    uacpi_status status;
};

#define IPC_ISR_Unregister_NUM 0xff000002
struct IPC_ISR_Unregister {
    uint32_t type;
};

void send_isr_reply(pmos_right_t right, uacpi_status status)
{
    struct IPC_ISR_Reply reply = {.type = IPC_ISR_Reply_NUM, .status = status};

    send_message_right(right, INVALID_PORT, &reply, sizeof(reply), NULL, 0);
}

void *isr_func(void *arg)
{
    struct isr_data *data   = arg;
    pmos_port_t reply_right = data->right;

    ports_request_t p = create_port(TASK_ID_SELF, 0);
    if (p.result != SUCCESS) {
        send_isr_reply(reply_right, UACPI_STATUS_INTERNAL_ERROR);
        return NULL;
    }

    uint64_t right_reciever;
    auto rr = create_right(p.port, &right_reciever, 0);
    if (rr.result != SUCCESS) {
        send_isr_reply(reply_right, UACPI_STATUS_INTERNAL_ERROR);
        return NULL;
    }
    data->right = rr.right;

    uint32_t int_vector = 0;
    int r               = install_isa_interrupt(data->irq, 0, p.port, &int_vector);
    if (r < 0) {
        send_isr_reply(reply_right, UACPI_STATUS_INTERNAL_ERROR);
        return NULL;
    }

#if defined(__x86_64__) || defined(__i386__)
    int result = pmos_request_io_permission();
    if (result != SUCCESS) {
        send_isr_reply(reply_right, UACPI_STATUS_INTERNAL_ERROR);
        return NULL;
    }
#endif

    send_isr_reply(reply_right, UACPI_STATUS_OK);

    while (true) {
        Message_Descriptor msg;
        unsigned char *message;
        r = get_message(&msg, &message, p.port, NULL, NULL);
        if (r != SUCCESS) {
            fprintf(stderr, "Failed to get message\n");
            return NULL;
        }

        assert(msg.size >= sizeof(IPC_Generic_Msg));
        IPC_Generic_Msg *ipc_msg = (IPC_Generic_Msg *)message;
        if (ipc_msg->type == IPC_Kernel_Interrupt_NUM) {
            data->handler(data->ctx);
            complete_interrupt(int_vector);
        } else if (ipc_msg->type == IPC_ISR_Unregister_NUM) {
            break;
        } else {
            fprintf(stderr, "Unknown message type\n");
        }
    }

    if (__atomic_sub_fetch(&data->refcount, 1, __ATOMIC_SEQ_CST) == 0)
        free(data);
    // unregister_interrupt(rr.value);
    return NULL;
}

__thread pmos_port_t isr_register_reply_port = INVALID_PORT;
pmos_port_t get_reply_port(void)
{
    if (isr_register_reply_port == INVALID_PORT) {
        ports_request_t port_request = create_port(TASK_ID_SELF, 0);
        if (port_request.result != SUCCESS) {
            return INVALID_PORT;
        }
        isr_register_reply_port = port_request.port;
    }
    return isr_register_reply_port;
}

uacpi_status uacpi_kernel_install_interrupt_handler(uacpi_u32 irq, uacpi_interrupt_handler handler,
                                                    uacpi_handle ctx, uacpi_handle *out_irq_handle)
{
    pmos_port_t reply_port = get_reply_port();
    if (reply_port == INVALID_PORT)
        return UACPI_STATUS_INTERNAL_ERROR;

    struct isr_data *data = malloc(sizeof(struct isr_data));
    if (data == NULL)
        return UACPI_STATUS_OUT_OF_MEMORY;

    uint64_t id = 0;
    auto right  = create_right(reply_port, &id, CREATE_RIGHT_SEND_ONCE);
    if (right.result != SUCCESS) {
        free(data);
        return UACPI_STATUS_INTERNAL_ERROR;
    }

    data->handler  = handler;
    data->ctx      = ctx;
    data->irq      = irq;
    data->right    = right.right;
    data->refcount = 2;

    int result = pthread_create(&data->isr_thread, NULL, isr_func, data);
    if (result != 0) {
        delete_right(right.result);
        free(data);
        return UACPI_STATUS_INTERNAL_ERROR;
    }

    Message_Descriptor msg;
    unsigned char *message;
    result_t r = get_message(&msg, &message, reply_port, NULL, NULL);
    if (r != SUCCESS) {
        assert(!"Failed to get message");
    }

    // assert(msg.sender == data->port);
    assert(msg.size == sizeof(struct IPC_ISR_Reply));
    struct IPC_ISR_Reply *reply = (struct IPC_ISR_Reply *)message;
    assert(reply->type == IPC_ISR_Reply_NUM);
    if (reply->status != UACPI_STATUS_OK) {
        pthread_detach(data->isr_thread);
        free(data);
        return reply->status;
    }

    *out_irq_handle = data;
    return UACPI_STATUS_OK;
}

uacpi_status uacpi_kernel_uninstall_interrupt_handler(uacpi_interrupt_handler,
                                                      uacpi_handle irq_handle)
{
    struct isr_data *data         = irq_handle;
    struct IPC_ISR_Unregister msg = {.type = IPC_ISR_Unregister_NUM};

    send_message_right(data->right, INVALID_PORT, &msg, sizeof(msg), NULL, 0);
    pthread_detach(data->isr_thread);
    if (__atomic_sub_fetch(&data->refcount, 1, __ATOMIC_SEQ_CST) == 0)
        free(data);
    return UACPI_STATUS_OK;
}

uacpi_status uacpi_kernel_handle_firmware_request(uacpi_firmware_request *r)
{
    fprintf(stderr, "!!! Firmware request: %i !!!\n", r->type);
    return UACPI_STATUS_OK;
}
