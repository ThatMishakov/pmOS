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
#include <sys/mman.h>
#include <semaphore.h>
#include <errno.h>

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
    uint64_t page_aligned_addr = addr & ~0xFFFUL;
    uint64_t page_offset        = addr & 0xFFFUL;
    uint64_t size_aligned      = (len + page_offset + 0xFFF) & ~0xFFFUL;

    mem_request_ret_t r = create_phys_map_region(TASK_ID_SELF, NULL, size_aligned,
                                                 PROT_READ | PROT_WRITE, page_aligned_addr);
    if (r.result != SUCCESS)
        return UACPI_MAP_FAILED;

    return (char *)r.virt_addr + page_offset;
}

void uacpi_kernel_unmap(void *addr, uacpi_size len)
{
    void *region_start_aligned = (void *)((uint64_t)addr & ~0xFFFUL);
    void *region_end_aligned   = (void *)(((uint64_t)addr + len + 0xFFFUL) & ~0xFFFUL);
    size_t region_size_aligned  = (size_t)((uintptr_t)region_end_aligned - (uintptr_t)region_start_aligned);

    release_memory_range(TASK_ID_SELF, region_start_aligned, region_size_aligned);
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

    work->handler(work->ctx);

    return NULL;
}

struct uacpi_work *work_queue_start = NULL, *work_queue_end = NULL;
pthread_spinlock_t work_queue_lock;
__attribute__((constructor)) void uacpi_work_queue_init(void)
{
    pthread_spin_init(&work_queue_lock, PTHREAD_PROCESS_PRIVATE);
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

uacpi_status uacpi_kernel_pci_device_open(uacpi_pci_address address, uacpi_handle *out_handle)
{
    if (!pci_fully_working) {
        struct PCIDevicePtr *p = malloc(sizeof(*p));
        if (!p)
            return UACPI_STATUS_OUT_OF_MEMORY;

        uacpi_status result = fill_device_early(p, address.segment, address.bus, address.device, address.function);
        if (result) {
            free(p);
            return result;
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
    if (offset & 1)
        return UACPI_STATUS_INVALID_ARGUMENT;

    uint32_t v = pci_read_register(device, offset / 4);
    *value     = (v >> ((offset % 4) * 8)) & 0xFFFF;
    return UACPI_STATUS_OK;
}

uacpi_status uacpi_kernel_pci_read32(uacpi_handle device, uacpi_size offset, uacpi_u32 *value)
{
    if (offset & 3)
        return UACPI_STATUS_INVALID_ARGUMENT;

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
    close_maybe_early(h);
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

uacpi_u8 uacpi_kernel_mmio_read8(void* ptr)
{
    return mmio_readb(ptr);
}

uacpi_u16 uacpi_kernel_mmio_read16(void* ptr)
{
    return mmio_readh(ptr);
}

uacpi_u32 uacpi_kernel_mmio_read32(void* ptr)
{
    return mmio_readl(ptr);
}

uacpi_u64 uacpi_kernel_mmio_read64(void* ptr)
{
    return mmio_readd(ptr);
}

void uacpi_kernel_mmio_write8(void* ptr, uacpi_u8 value)
{
    mmio_writeb(ptr, value);
}

void uacpi_kernel_mmio_write16(void* ptr, uacpi_u16 value)
{
    mmio_writeh(ptr, value);
}

void uacpi_kernel_mmio_write32(void* ptr, uacpi_u32 value)
{
    mmio_writel(ptr, value);
}

void uacpi_kernel_mmio_write64(void* ptr, uacpi_u64 value)
{
    mmio_writed(ptr, value);
}

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
    return pmos_get_time(CLOCK_MONOTONIC).value;
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
        send_isr_reply(reply_right, UACPI_STATUS_OUT_OF_MEMORY);
        return NULL;
    }

    uint64_t right_receiver;
    auto rr = create_right(p.port, &right_receiver, 0);
    if (rr.result != SUCCESS) {
        send_isr_reply(reply_right, UACPI_STATUS_OUT_OF_MEMORY);
        return NULL;
    }
    data->right = rr.right;

    uint32_t int_vector = 0;
    right_request_t interrupt = install_isa_interrupt(data->irq, p.port);
    if (interrupt.result < 0) {
        send_isr_reply(reply_right, UACPI_STATUS_OUT_OF_MEMORY);
        return NULL;
    }

    send_isr_reply(reply_right, UACPI_STATUS_OK);

    while (true) {
        Message_Descriptor msg;
        unsigned char *message;
        int r = get_message(&msg, &message, p.port, NULL, NULL);
        if (r != SUCCESS) {
            fprintf(stderr, "Failed to get message\n");
            return NULL;
        }

        assert(msg.size >= sizeof(IPC_Generic_Msg));
        IPC_Generic_Msg *ipc_msg = (IPC_Generic_Msg *)message;
        if (ipc_msg->type == IPC_Kernel_Interrupt_NUM) {
            data->handler(data->ctx);
            complete_interrupt(p.port, interrupt.right);
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
        return UACPI_STATUS_OUT_OF_MEMORY;

    struct isr_data *data = malloc(sizeof(struct isr_data));
    if (data == NULL)
        return UACPI_STATUS_OUT_OF_MEMORY;

    uint64_t id = 0;
    auto right  = create_right(reply_port, &id, CREATE_RIGHT_SEND_ONCE);
    if (right.result != SUCCESS) {
        free(data);
        return UACPI_STATUS_OUT_OF_MEMORY;
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
        return UACPI_STATUS_OUT_OF_MEMORY;
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

syscall_r __pmos_syscall_set_attr(uint64_t pid, uint32_t attr, unsigned long value);

uacpi_interrupt_state uacpi_kernel_disable_interrupts(void)
{
    return __pmos_syscall_set_attr(0, 8, 0).value;
}

void uacpi_kernel_restore_interrupts(uacpi_interrupt_state state)
{
    __pmos_syscall_set_attr(0, 9, state);
}

uacpi_handle uacpi_kernel_create_semaphore(uacpi_u32 initial_units)
{
    sem_t *sem = malloc(sizeof(sem_t));
    if (!sem)
        return sem;

    int result = sem_init(sem, 0, initial_units);
    if (result < 0) {
        free(sem);
        return nullptr;
    }

    return sem;
}

void uacpi_kernel_free_semaphore(uacpi_handle handle)
{
    int result = sem_destroy(handle);
    (void)result;
    assert(result == 0);

    free(handle);
}

uacpi_status uacpi_kernel_wait_for_semaphore(uacpi_handle handle, uacpi_u16 timeout_ms)
{
    if (timeout_ms == 0xFFFF) {
        int result = sem_wait(handle);
        if (result < 0)
            return UACPI_STATUS_INTERNAL_ERROR;
        return UACPI_STATUS_OK;
    } else if (timeout_ms == 0) {
        int result = sem_trywait(handle);
        if (result < 0) {
            if (errno == EAGAIN)
                return UACPI_STATUS_TIMEOUT;
            else
                return UACPI_STATUS_INTERNAL_ERROR;
        }
        return UACPI_STATUS_OK;
    } else {
        struct timespec ts;
        clock_gettime(CLOCK_REALTIME, &ts);
        ts.tv_sec += timeout_ms / 1000;
        ts.tv_nsec += (timeout_ms % 1000) * 1000000;

        if (ts.tv_nsec >= 1000000000L) {
            ts.tv_sec++;
            ts.tv_nsec -= 1000000000L;
        }

        int result = sem_timedwait(handle, &ts);
        if (result < 0) {
            if (errno == ETIMEDOUT)
                return UACPI_STATUS_TIMEOUT;
            else
                return UACPI_STATUS_INTERNAL_ERROR;
        }
        return UACPI_STATUS_OK;
    }
}

void uacpi_kernel_signal_semaphore(uacpi_handle handle)
{
    int result = sem_post(handle);
    (void)result;
    assert(result == 0);
}

struct WorkItem {
    uacpi_work_handler handler;
    uacpi_handle ctx;
    uacpi_work_type work_type;
    pthread_t thread;
    sem_t semaphore;
    bool cancel_self;
};

void *work_thread(void *arg)
{
    struct WorkItem *item = arg;
    bool currently_scheduled_any = true;

    while (!item->cancel_self) {
        sem_wait(&item->semaphore);
        if (item->handler == NULL)
            break;

        bool schedule_any = item->work_type == UACPI_WORK_NOTIFICATION;
        if (currently_scheduled_any != schedule_any)
            set_affinity(TASK_ID_SELF, schedule_any ? 0 : 1, 0);
        currently_scheduled_any = schedule_any;

        item->handler(item->ctx);
    }

    if (item->cancel_self) {
        sem_destroy(&item->semaphore);
        free(item);
    }

    return nullptr;
}

uacpi_handle uacpi_kernel_create_work_item(void)
{
    struct WorkItem *item = malloc(sizeof(struct WorkItem));
    if (!item)
        return NULL;

    sem_init(&item->semaphore, 0, 0);

    int result = pthread_create(&item->thread, NULL, (void *(*)(void *))work_thread, item);
    if (result != 0) {
        sem_destroy(&item->semaphore);
        free(item);
        return NULL;
    }

    item->handler = NULL;
    item->ctx     = NULL;
    item->cancel_self = false;
    return item;
}

void uacpi_kernel_free_event(uacpi_handle handle)
{
    struct WorkItem *item = handle;
    item->handler = NULL;

    if (pthread_equal(pthread_self(), item->thread)) {
        pthread_detach(item->thread);
        item->cancel_self = true;
    } else {
        sem_post(&item->semaphore);
        pthread_join(item->thread, NULL);
        sem_destroy(&item->semaphore);
        free(item);
    }
}

void uacpi_kernel_schedule_work(uacpi_work_type type, uacpi_handle work_item, uacpi_work_handler handler, uacpi_handle ctx)
{
    struct WorkItem *item = handle;
    item->handler = handler;
    item->ctx = ctx;
    item->work_type = type;
    sem_post(&item->semaphore);
}
