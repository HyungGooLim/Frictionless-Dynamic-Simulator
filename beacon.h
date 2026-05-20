#ifndef BEACON_H
#define BEACON_H

#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include "proto_crc.h"
#include "user.h"

/*
 * Beacon Packet (총 27 Bytes)
 * +---------+-------------+-------------+-------+--------+-----------+---------+-----------------+--------+
 * |   SOF   || Status     | Module Name | Clock | SW.Ver |  Pos/Att  |  Power  | Thruster Status | CRC-16 |
 * |  (4 B)  ||  (1 B)     |    (1 B)    | (4 B) |  (1 B) |   (12 B)   |  (1 B)  |     (1 B)       |  (2 B) |
 * +---------+-------------+-------+--------+-----------+---------+-----------------+--------+
 */

/* ------------------------------- Header (5 B) ------------------------------- */
#define BEACON_SOF_LEN       4
#define BEACON_SOF0          0xA1
#define BEACON_SOF1          0xA2
#define BEACON_SOF2          0xA3
#define BEACON_SOF3          0xA4

#define BEACON_STATUS_LEN 1

#define MODULE_OK             0x00
#define MODULE_NOT_READY      0x01
#define MODULE_POWER_SHORTAGE 0x02

#define BEACON_HEADER_SIZE \
    (BEACON_SOF_LEN + BEACON_STATUS_LEN)


/* ------------------------------- Module Name (1B) ------------------------------- */
#define BEACON_MODULE_NAME_LEN 1
#define MOD_TARG 0x30
#define MOD_CHAS 0x40

/* ------------------------------- State Index ------------------------------- */
typedef enum {
    STATE_X   = 0,  /* state[0] -> x 위치 (cm),      float: 0 ~ 65535 cm */
    STATE_Y   = 1,  /* state[1] -> y 위치 (cm),      float: 0 ~ 65535 cm */
    STATE_YAW = 2,  /* state[2] -> Yaw 각 (0.1°),    float: 0 ~ 3600 (= 0.0° ~ 360.0°) */
    STATE_DIM = 3
} BeaconStateIndex;

/* ------------------------------- Argument (20B) ------------------------------- */
/*  1(module_name) + 4(clock) + 1(sw_version) + 12(state[3]) + 1(power) + 1(thruster) = 20 */
#define BEACON_ARGUMENT_SIZE 20

/* ------------------------------- Packet ------------------------------- */
#define BEACON_PACKET_SIZE (BEACON_HEADER_SIZE + BEACON_ARGUMENT_SIZE + CRC_SIZE)

/* ------------------------------- Structures  (21 B)------------------------------- */
typedef struct Beacon_Header {
    uint8_t SOF[BEACON_SOF_LEN];
    uint8_t status[BEACON_STATUS_LEN];
} Beacon_Header;

typedef struct Beacon_Argument {
    uint8_t  module_name[BEACON_MODULE_NAME_LEN]; /* 1 B */
    uint32_t clock;                               /* 4 B */
    uint8_t  sw_version;                          /* 1 B */
    float state[STATE_DIM];                       /* 12 B */
    uint8_t  power;                               /* 1 B */
    uint8_t  thruster_state;                      /* 1 B */
} Beacon_Argument;

typedef struct Beacon_Packet_Frame {
    Beacon_Header   header;
    Beacon_Argument argument;
    uint16_t        crc;
} Beacon_Packet_Frame;

/* ------------------------------- API ------------------------------- */
#ifdef __cplusplus
extern "C" {
#endif

size_t create_beacon_packet(uint8_t status,
                            const Beacon_Argument* argument,
                            uint8_t* out_buf);

bool   parse_beacon_packet(Beacon_Packet_Frame* out,
                           const uint8_t* buf,
                           size_t len);

bool   beacon_check_length(size_t len);
bool   beacon_check_sof(const uint8_t* buf, size_t len);
bool   beacon_validate_packet(const uint8_t* buf, size_t len);

#ifdef __cplusplus
}
#endif

#endif  /* BEACON_H */
