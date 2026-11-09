#include <assert.h>
#include <errno.h>
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
#include <semaphore.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
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

static pthread_mutex_t work_lock = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t work_done  = PTHREAD_COND_INITIALIZER;
static size_t outstanding_work;

static void synchronize_interrupts(void);

uacpi_status uacpi_kernel_wait_for_work_completion(void)
{
    /* An ISR may still be about to publish deferred work. Drain it first. */
    synchronize_interrupts();

    pthread_mutex_lock(&work_lock);
    while (outstanding_work != 0)
        pthread_cond_wait(&work_done, &work_lock);
    pthread_mutex_unlock(&work_lock);
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
    pthread_t thread;
    uacpi_interrupt_handler handler;
    uacpi_handle ctx;
    uacpi_u32 irq;
    struct isr_data *next;
    size_t refcount;

    sem_t ready;
    uacpi_status init_status;
    pmos_right_t control_right;
};

/* Never hold isr_lock while waiting for an ISR: callbacks may enter uACPI. */
static pthread_mutex_t isr_lock = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t isr_done  = PTHREAD_COND_INITIALIZER;
static struct isr_data *installed_isrs;
static size_t retiring_isrs;
static size_t starting_isrs;
static size_t pending_isr_syncs;

#define IPC_ISR_Synchronize_NUM 0xff000003
#define IPC_ISR_Unregister_NUM  0xff000002

/* Serialize barrier batches so their acknowledgements cannot overlap. */
static pthread_mutex_t isr_sync_lock = PTHREAD_MUTEX_INITIALIZER;

/* uACPI teardown callers do not check the completion-wait status. Continuing
 * after a failed barrier would let them free state still used by callbacks. */
static void check_async_result(result_t result, const char *operation)
{
    if (result != SUCCESS) {
        fprintf(stderr, "[devicesd] uACPI %s failed: %lld\n", operation, (long long)result);
        abort();
    }
}

static void wait_semaphore(sem_t *sem)
{
    int result;
    do {
        result = sem_wait(sem);
    } while (result < 0 && errno == EINTR);
    if (result < 0)
        check_async_result(-errno, "semaphore wait");
}

static void release_isr(struct isr_data *isr)
{
    if (__atomic_sub_fetch(&isr->refcount, 1, __ATOMIC_ACQ_REL) == 0) {
        sem_destroy(&isr->ready);
        free(isr);
    }
}

static void synchronize_interrupts(void)
{
    pthread_mutex_lock(&isr_sync_lock);
    pthread_mutex_lock(&isr_lock);
    /* Installation enables the IRQ before publishing the handle. Include any
     * installation already in progress without blocking callbacks that install. */
    while (starting_isrs != 0)
        pthread_cond_wait(&isr_done, &isr_lock);
    IPC_Generic_Msg message = {.type = IPC_ISR_Synchronize_NUM};
    for (struct isr_data *isr = installed_isrs; isr; isr = isr->next) {
        ++pending_isr_syncs;
        check_async_result(
            send_message_right(isr->control_right, INVALID_PORT, &message, sizeof(message), NULL, 0)
                .result,
            "ISR barrier");
    }

    /* Include handlers removed from the registry but still finishing an IRQ. */
    while (pending_isr_syncs != 0 || retiring_isrs != 0)
        pthread_cond_wait(&isr_done, &isr_lock);
    pthread_mutex_unlock(&isr_lock);
    pthread_mutex_unlock(&isr_sync_lock);
}

static void *isr_thread(void *arg)
{
    struct isr_data *isr = arg;
    ports_request_t port = create_port(TASK_ID_SELF, 0);
    if (port.result != SUCCESS)
        goto init_failed;

    right_request_t control = create_right(port.port, NULL, 0);
    if (control.result != SUCCESS)
        goto init_failed;
    isr->control_right = control.right;

    right_request_t interrupt = install_isa_interrupt(isr->irq, port.port);
    if (interrupt.result != SUCCESS)
        goto init_failed;

    isr->init_status = UACPI_STATUS_OK;
    sem_post(&isr->ready);

    for (;;) {
        Message_Descriptor desc;
        /* Receive IRQ and control messages without allocating a buffer. */
        IPC_Generic_Msg message;
        check_async_result(syscall_get_message_info(&desc, port.port, 0, 0), "ISR receive");
        if (desc.size < sizeof(IPC_Generic_Msg) || desc.size > sizeof(message))
            abort();
        check_async_result(read_message((char *)&message, sizeof(message), MSG_ARG_REJECT_RIGHT,
                                        port.port, desc.message_id)
                               .result,
                           "ISR message read");

        if (message.type == IPC_Kernel_Interrupt_NUM) {
            isr->handler(isr->ctx);
            check_async_result(complete_interrupt(port.port, interrupt.right), "IRQ completion");
        } else if (message.type == IPC_ISR_Synchronize_NUM) {
            /* FIFO receive: earlier IRQ callbacks have published their work. */
            pthread_mutex_lock(&isr_lock);
            assert(pending_isr_syncs != 0);
            --pending_isr_syncs;
            pthread_cond_broadcast(&isr_done);
            pthread_mutex_unlock(&isr_lock);
        } else if (message.type == IPC_ISR_Unregister_NUM) {
            break;
        } else {
            fprintf(stderr, "[devicesd] Unexpected uACPI ISR message %u\n", message.type);
        }
    }

    /* No more callbacks can run. The kernel destroys this thread's port and
     * interrupt receive right when the thread exits. */
    pthread_mutex_lock(&isr_lock);
    assert(retiring_isrs != 0);
    --retiring_isrs;
    pthread_cond_broadcast(&isr_done);
    pthread_mutex_unlock(&isr_lock);
    release_isr(isr);
    return NULL;

init_failed:
    isr->init_status = UACPI_STATUS_OUT_OF_MEMORY;
    sem_post(&isr->ready);
    release_isr(isr);
    return NULL;
}

uacpi_status uacpi_kernel_install_interrupt_handler(uacpi_u32 irq, uacpi_interrupt_handler handler,
                                                    uacpi_handle ctx, uacpi_handle *out_irq_handle)
{
    struct isr_data *isr = calloc(1, sizeof(*isr));
    if (!isr)
        return UACPI_STATUS_OUT_OF_MEMORY;
    if (sem_init(&isr->ready, 0, 0) < 0) {
        free(isr);
        return UACPI_STATUS_OUT_OF_MEMORY;
    }

    isr->handler  = handler;
    isr->ctx      = ctx;
    isr->irq      = irq;
    isr->refcount = 2; /* One reference each for the caller and ISR thread. */
    pthread_mutex_lock(&isr_lock);
    ++starting_isrs;
    pthread_mutex_unlock(&isr_lock);
    if (pthread_create(&isr->thread, NULL, isr_thread, isr) != 0) {
        pthread_mutex_lock(&isr_lock);
        --starting_isrs;
        pthread_cond_broadcast(&isr_done);
        pthread_mutex_unlock(&isr_lock);
        sem_destroy(&isr->ready);
        free(isr);
        return UACPI_STATUS_OUT_OF_MEMORY;
    }

    wait_semaphore(&isr->ready);
    uacpi_status status = isr->init_status;
    if (status != UACPI_STATUS_OK) {
        check_async_result(pthread_join(isr->thread, NULL), "ISR join");
        release_isr(isr);
        pthread_mutex_lock(&isr_lock);
        --starting_isrs;
        pthread_cond_broadcast(&isr_done);
        pthread_mutex_unlock(&isr_lock);
        return status;
    }

    pthread_mutex_lock(&isr_lock);
    isr->next      = installed_isrs;
    installed_isrs = isr;
    --starting_isrs;
    pthread_cond_broadcast(&isr_done);
    pthread_mutex_unlock(&isr_lock);
    *out_irq_handle = isr;
    return UACPI_STATUS_OK;
}

uacpi_status uacpi_kernel_uninstall_interrupt_handler(uacpi_interrupt_handler handler,
                                                      uacpi_handle irq_handle)
{
    (void)handler;
    struct isr_data *isr = irq_handle;
    pthread_mutex_lock(&isr_lock);
    IPC_Generic_Msg message = {.type = IPC_ISR_Unregister_NUM};
    if (send_message_right(isr->control_right, INVALID_PORT, &message, sizeof(message), NULL, 0)
            .result != SUCCESS) {
        pthread_mutex_unlock(&isr_lock);
        return UACPI_STATUS_INTERNAL_ERROR;
    }

    struct isr_data **link = &installed_isrs;
    while (*link && *link != isr)
        link = &(*link)->next;
    assert(*link == isr);
    *link = isr->next;
    ++retiring_isrs;
    pthread_mutex_unlock(&isr_lock);

    check_async_result(pthread_detach(isr->thread), "ISR detach");
    release_isr(isr);
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
        int result;
        do {
            result = sem_wait(handle);
        } while (result < 0 && errno == EINTR);
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
        if (clock_gettime(CLOCK_REALTIME, &ts) < 0)
            return UACPI_STATUS_INTERNAL_ERROR;
        ts.tv_sec += timeout_ms / 1000;
        ts.tv_nsec += (timeout_ms % 1000) * 1000000;

        if (ts.tv_nsec >= 1000000000L) {
            ts.tv_sec++;
            ts.tv_nsec -= 1000000000L;
        }

        int result;
        do {
            result = sem_timedwait(handle, &ts);
        } while (result < 0 && errno == EINTR);
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

struct uacpi_work_item {
    uacpi_work_handler handler;
    uacpi_handle ctx;
    uacpi_work_type work_type;
    pthread_t thread;
    pthread_mutex_t lock;
    sem_t semaphore;
    bool stop;
    bool cleanup_on_exit;
};

static void destroy_work_item(struct uacpi_work_item *item)
{
    sem_destroy(&item->semaphore);
    pthread_mutex_destroy(&item->lock);
    free(item);
}

static void *work_thread(void *arg)
{
    struct uacpi_work_item *item = arg;
    bool currently_scheduled_any = true;

    for (;;) {
        wait_semaphore(&item->semaphore);
        pthread_mutex_lock(&item->lock);
        if (item->stop) {
            bool cleanup = item->cleanup_on_exit;
            pthread_mutex_unlock(&item->lock);
            if (cleanup)
                destroy_work_item(item);
            return NULL;
        }
        /* A callback may reschedule itself with different arguments. */
        uacpi_work_handler handler = item->handler;
        uacpi_handle ctx           = item->ctx;
        uacpi_work_type type       = item->work_type;
        pthread_mutex_unlock(&item->lock);

        bool schedule_any = type == UACPI_WORK_NOTIFICATION;
        if (currently_scheduled_any != schedule_any)
            set_affinity(TASK_ID_SELF, schedule_any ? 0 : 1, 0);
        currently_scheduled_any = schedule_any;
        handler(ctx);

        /* The allocation stays alive until this thread exits, even on self-free.
         * Completion is global so it also includes callbacks that reschedule. */
        pthread_mutex_lock(&work_lock);
        assert(outstanding_work != 0);
        if (--outstanding_work == 0)
            pthread_cond_broadcast(&work_done);
        pthread_mutex_unlock(&work_lock);
    }
}

uacpi_handle uacpi_kernel_create_work_item(void)
{
    struct uacpi_work_item *item = calloc(1, sizeof(*item));
    if (!item)
        return NULL;
    if (pthread_mutex_init(&item->lock, NULL) != 0) {
        free(item);
        return NULL;
    }
    if (sem_init(&item->semaphore, 0, 0) < 0) {
        pthread_mutex_destroy(&item->lock);
        free(item);
        return NULL;
    }
    if (pthread_create(&item->thread, NULL, work_thread, item) != 0) {
        destroy_work_item(item);
        return NULL;
    }
    return item;
}

void uacpi_kernel_free_work_item(uacpi_handle handle)
{
    struct uacpi_work_item *item = handle;
    pthread_t thread             = item->thread;
    bool self                    = pthread_equal(pthread_self(), thread);
    if (self)
        check_async_result(pthread_detach(thread), "worker detach");

    pthread_mutex_lock(&item->lock);
    item->stop            = true;
    item->cleanup_on_exit = self;
    sem_post(&item->semaphore);
    pthread_mutex_unlock(&item->lock);

    if (!self) {
        check_async_result(pthread_join(thread, NULL), "worker join");
        destroy_work_item(item);
    }
}

void uacpi_kernel_schedule_work(uacpi_work_type type, uacpi_handle work_item,
                                uacpi_work_handler handler, uacpi_handle ctx)
{
    struct uacpi_work_item *item = work_item;
    /* ISR callbacks run in ordinary userspace threads, so mutexes are safe. */
    pthread_mutex_lock(&item->lock);
    item->handler   = handler;
    item->ctx       = ctx;
    item->work_type = type;
    pthread_mutex_lock(&work_lock);
    ++outstanding_work;
    pthread_mutex_unlock(&work_lock);
    sem_post(&item->semaphore);
    pthread_mutex_unlock(&item->lock);
}
