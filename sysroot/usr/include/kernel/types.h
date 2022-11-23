#ifndef KERNEL_TYPES_H
#define KERNEL_TYPES_H

#define u8 unsigned char
#define u16 unsigned short
#define u32 unsigned int
#define u64 unsigned long

#define i8 signed char
#define i16 signed short
#define i32 signed int
#define i64 signed long long

#define KB(x) ((u64)(x) << 10)
#define MB(x) ((u64)(x) << 20)
#define GB(x) ((u64)(x) << 30)
#define TB(x) ((u64)(x) << 40)

#define PACKED __attribute__((packed))
#define ALIGNED(x) __attribute__((aligned(x)))
#define UNUSED __attribute__((unused))
#define GSRELATIVE __attribute__((address_space(256)))
#define FALLTHROUGH __attribute__((fallthrough)) 

#endif