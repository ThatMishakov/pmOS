#ifndef KERNEL_TYPES_H
#define KERNEL_TYPES_H
#include <stdint.h>

#define u8 uint8_t 
#define u16 uint16_t 
#define u32 uint32_t 
#define u64 uint64_t 

#define i8 int8_t 
#define i16 int16_t 
#define i32 int32_t 
#define i64 int64_t 

#define KB(x) ((u64)(x) << 10)
#define MB(x) ((u64)(x) << 20)
#define GB(x) ((u64)(x) << 30)
#define TB(x) ((u64)(x) << 40)

#define PACKED __attribute__((packed))
#define ALIGNED(x) __attribute__((aligned(x)))

#endif