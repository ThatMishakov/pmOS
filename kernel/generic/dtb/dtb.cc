#include "dtb.hh"

#include <kern_logger/kern_logger.hh>
#include <smoldtb.h>

klib::shared_ptr<Mem_Object> dtb_object = nullptr;

// Reads big-endian 32 bit uint into native endianness
static u32 be32(u32 input)
{
#if __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
    return input;
#else
    u32 temp = 0;
    temp |= (input & 0xFF) << 24;
    temp |= (input & 0xFF00) << 8;
    temp |= (input & 0xFF0000) >> 8;
    temp |= (input & 0xFF000000) >> 24;
    return temp;
#endif
}

void *dtb_virt_base = nullptr;

void init_dtb(u64 phys_addr)
{
    fdt_header h;
    copy_from_phys(phys_addr, &h, sizeof(h));

    if (be32(h.magic) != FDT_MAGIC) {
        serial_logger.printf("Error: invalid FDT magic, got %x\n", h.magic);
        return;
    }

    u32 total_size = be32(h.totalsize);
    // Align up to the nearest page
    total_size     = (total_size + 0xfff) & ~0xfff;

    serial_logger.printf("FDT at %p, size %x\n", phys_addr, total_size);

    // FDT should be reclaimable
    dtb_object =
        Mem_Object::create_from_phys(phys_addr, total_size, true, Mem_Object::Protection::Readable);
    if (not dtb_object) {
        serial_logger.printf("Error: could not create FDT object\n");
        return;
    }

    // Map it into the kernel space
    const Page_Table_Argumments args = {
        .readable           = true,
        .writeable          = false,
        .user_access        = false,
        .global             = false,
        .execution_disabled = true,
        .extra              = 0,
        .cache_policy       = Memory_Type::Normal,
    };
    auto t = dtb_object->map_to_kernel(0, total_size, args);
    assert(t.success());
    void *const virt_base = t.val;
    if (virt_base == nullptr) {
        serial_logger.printf("Error: could not map FDT object\n");
        return;
    }
    ::dtb_virt_base = virt_base;

    serial_logger.printf("FDT mapped to %p\n", virt_base);

    dtb_ops ops = {.malloc = malloc,
                   .free =
                       [](void *ptr, size_t size) {
                           (void)size;
                           free(ptr);
                       },
                   .on_error = [](const char *why) { serial_logger.printf("Error: %s\n", why); }};

    dtb_init(reinterpret_cast<uintptr_t>(virt_base), ops);

    serial_logger.printf("FDT initialized!\n");
}

bool have_dtb() { return dtb_virt_base != nullptr; }