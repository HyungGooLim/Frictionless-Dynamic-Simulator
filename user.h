#ifndef _USER_H_
#define _USER_H_

/* ---- 플랫폼 선택: 사용할 플랫폼 하나만 주석 해제 ---- */
#define PLATFORM_ARDUINO    /* Arduino 보드 */
//#define PLATFORM_WSL      /* WSL/Linux 테스트 */

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <string.h>

#include "proto_crc.h"

#define SOFTWARE_VERSION 1  /* 2026.02.24 */

/* Big-endian 쓰기 */
static inline void wr_u16_be(uint8_t* p, uint16_t v)
{
    p[0] = (uint8_t)(v >> 8);
    p[1] = (uint8_t)(v & 0xFF);
}

static inline void wr_u32_be(uint8_t* p, uint32_t v)
{
    p[0] = (uint8_t)(v >> 24);
    p[1] = (uint8_t)(v >> 16);
    p[2] = (uint8_t)(v >> 8);
    p[3] = (uint8_t)(v & 0xFF);
}

static inline void wr_f32_be(uint8_t* p, float v)
{
    uint32_t u;
    memcpy(&u, &v, 4);
    wr_u32_be(p, u);
}

/* Big-endian 읽기 */
static inline uint16_t rd_u16_be(const uint8_t* p)
{
    return (uint16_t)(((uint16_t)p[0] << 8) | (uint16_t)p[1]);
}

static inline uint32_t rd_u32_be(const uint8_t* p)
{
    return ((uint32_t)p[0] << 24) |
           ((uint32_t)p[1] << 16) |
           ((uint32_t)p[2] <<  8) |
            (uint32_t)p[3];
}

static inline float rd_f32_be(const uint8_t* p)
{
    float v;
    uint32_t u = rd_u32_be(p);
    memcpy(&v, &u, 4);
    return v;
}

/* 패킷 꼬리 CRC 검증 */
static inline bool verify_crc16_tail(const uint8_t* pkt, size_t pkt_len)
{
    size_t   body_len;
    uint16_t crc_calc, crc_recv;

    if (!pkt || pkt_len < (size_t)CRC_SIZE) return false;

    body_len = pkt_len - CRC_SIZE;
    crc_calc = calculate_crc16(pkt, body_len);
    crc_recv = rd_u16_be(&pkt[body_len]);
    return (crc_calc == crc_recv);
}

static inline bool memeq(const uint8_t* a, const uint8_t* b, size_t n)
{
    return (memcmp(a, b, n) == 0);
}

#endif  /* _USER_H_ */
