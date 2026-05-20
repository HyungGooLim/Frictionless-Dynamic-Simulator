#ifndef PROTO_CRC_H
#define PROTO_CRC_H

#include <stdint.h>
#include <stddef.h>

#define CRC_SIZE 2
#define CRC_POLY 0x1021  /* CRC-16-CCITT / XMODEM */

#ifdef __cplusplus
extern "C" {
#endif

uint16_t calculate_crc16(const void* data, size_t length);

#ifdef __cplusplus
}
#endif

#endif  /* PROTO_CRC_H */
