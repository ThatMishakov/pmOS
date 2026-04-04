#ifndef _ENDIAN_H
#define _ENDIAN_H
#include <stdint.h>


#define LITTLE_ENDIAN 1234
#define BIG_ENDIAN 4321

#define BYTE_ORDER LITTLE_ENDIAN

uint16_t  be16toh(uint16_t);
uint32_t  be32toh(uint32_t);
uint64_t  be64toh(uint64_t);
uint16_t  htobe16(uint16_t);
uint32_t  htobe32(uint32_t);
uint64_t  htobe64(uint64_t);
uint16_t  htole16(uint16_t);
uint32_t  htole32(uint32_t);
uint64_t  htole64(uint64_t);
uint16_t  le16toh(uint16_t);
uint32_t  le32toh(uint32_t);
uint64_t  le64toh(uint64_t);

#endif // _ENDIAN_H