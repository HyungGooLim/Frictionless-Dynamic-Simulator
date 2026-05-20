#include "proto_crc.h"

/* CRC-16-XMODEM (초기값 0x0000, 다항식 0x1021) */
uint16_t calculate_crc16(const void* data, size_t length)
{
    const uint8_t* bytes = (const uint8_t*)data;
    uint16_t crc = 0x0000;
    size_t i;
    uint8_t j;

    for (i = 0; i < length; i++) {
        crc ^= (uint16_t)bytes[i] << 8;
        for (j = 0; j < 8; j++) {
            if (crc & 0x8000)
                crc = (crc << 1) ^ CRC_POLY;
            else
                crc <<= 1;
        }
    }
    return crc;
}
