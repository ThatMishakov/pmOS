#ifndef KERNEL_TYPES_H
#define KERNEL_TYPES_H

typedef unsigned char u8;
typedef unsigned short u16;
typedef unsigned int u32;
typedef unsigned long u64;

typedef signed char i8;
typedef signed short i16;
typedef signed int i32;
typedef signed long i64;

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