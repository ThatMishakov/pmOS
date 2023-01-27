#ifndef GDT_HH
#define GDT_HH

struct TSS {
    u32 reserved0 = 0;
    u64 rsp0 = 0;
    u64 rsp1 = 0;
    u64 rsp2 = 0;
    u64 reserved = 0;
    u64 ist1 = 0;
    u64 ist2 = 0;
    u64 ist3 = 0;
    u64 ist4 = 0;
    u64 ist5 = 0;
    u64 ist6 = 0;
    u64 ist7 = 0;
    u64 reserved1 = 0;
    u16 reserved2 = 0;
    u16 iopb = 0;
} PACKED;

struct System_Segment_Descriptor {
    u16 limit0;
    u16 base0;
    u8  base1;
    u8  access;
    u8  limit1: 4;
    u8  flags:  4;
    u8  base2;
    u32 base3;
    u32 reserved;

    constexpr System_Segment_Descriptor()
        : System_Segment_Descriptor(0, 0, 0, 0) {}
    constexpr System_Segment_Descriptor(u64 base, u32 limit, u8 access, u8 flags)
        : limit0(limit & 0xffff), base0(base & 0xffff), base1((base >> 16) & 0xff),
          access(access), limit1((limit >> 16) & 0xff), flags(flags),
          base2((base >> 24) & 0xff), base3((base >> 32) & 0xffffffff),
          reserved(0) {}

    TSS *tss();
} PACKED;

struct GDT_entry {
    u16 limit0;
    u16 base0;
    u8 base1;
    u8 access;
    u8 limit1_flags;
    u8 base2;
} PACKED;

struct PACKED GDT_descriptor {
    u16 size;
    u64 offset;
};

struct GDT {
    GDT_entry Null {}; // always null
    GDT_entry _64bit_kernel_code {0, 0, 0, 0x9a, 0xa2, 0};
    GDT_entry _64bit_kernel_data {0, 0, 0, 0x92, 0xa0, 0};
    GDT_entry _32bit_user_code   {0, 0, 0, 0xfa, 0xc0, 0};
    GDT_entry _64bit_user_data   {0, 0, 0, 0xf2, 0xa0, 0};
    GDT_entry _64bit_user_code   {0, 0, 0, 0xfa, 0xa0, 0};
    System_Segment_Descriptor tss_descriptor;

    constexpr u16 get_size()
    {
        return sizeof(GDT) - 1;
    }
} PACKED ALIGNED(8);

#define R0_CODE_SEGMENT        (0x08)
#define R0_DATA_SEGMENT        (0x10)
#define R3_LEGACY_CODE_SEGMENT (0x18 | 0x03)
#define R3_DATA_SEGMENT        (0x20 | 0x03)
#define R3_CODE_SEGMENT        (0x28 | 0x03)
#define TSS_OFFSET             (0x30)

extern "C" void loadGDT(GDT* gdt, u16 size);
void loadGDT(GDT* gdt);

void init_gdt();

void load_temp_gdt();

extern GDT temp_gdt;

#endif