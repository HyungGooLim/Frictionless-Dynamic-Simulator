#ifndef TELECOMMAND_H
#define TELECOMMAND_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "proto_crc.h"

/*
 * Telecommand Packet (총 15 Bytes)
 * +--------+-----------+-------+------------+--------+
 * | SOF(4) | CMD_NUM(2)| CMD(1)| PAYLOAD(6) | CRC(2) |
 * +--------+-----------+-------+------------+--------+
 */

/* ------------------------------- Header ------------------------------- */
#define TELECMD_HEADER_SOF_LEN      4
#define TELECMD_SOF0  0xAA
#define TELECMD_SOF1  0xBB
#define TELECMD_SOF2  0xCC
#define TELECMD_SOF3  0xDD

#define TELECMD_HEADER_CMD_NUM_LEN  2
#define TELECMD_HEADER_LEN  (TELECMD_HEADER_SOF_LEN + TELECMD_HEADER_CMD_NUM_LEN)

/* ------------------------------- Argument ------------------------------- */
#define TELECMD_ARGU_CMD_LEN      1

#define INF_CLOCK                 0x01
#define INF_CONTROL               0x02
#define INF_SETTING               0x03
#define INF_REQUEST               0x04
#define INF_RESET                 0x05

#define TELECMD_ARGU_PAYLOAD_LEN  6
#define TELECMD_ARGU_LEN  (TELECMD_ARGU_CMD_LEN + TELECMD_ARGU_PAYLOAD_LEN)

/* ------------------------------- Packet ------------------------------- */
#define TELECMD_PACKET_SIZE  (TELECMD_HEADER_LEN + TELECMD_ARGU_LEN + CRC_SIZE)

/* ------------------------------- Structures ------------------------------- */
typedef struct Telecmd_Header {
    uint8_t SOF[TELECMD_HEADER_SOF_LEN];
    uint8_t cmd_num[TELECMD_HEADER_CMD_NUM_LEN];
} Telecmd_Header;

typedef struct Telecmd_Argument {
    uint8_t cmd;
    uint8_t payload[TELECMD_ARGU_PAYLOAD_LEN];
} Telecmd_Argument;

typedef struct Telecmd_Packet_Frame {
    size_t           pk_len;
    Telecmd_Header   header;
    Telecmd_Argument argument;
    uint16_t         crc;
} Telecmd_Packet_Frame;

/* ------------------------------- API ------------------------------- */
#ifdef __cplusplus
extern "C" {
#endif

size_t   create_telecommand_packet(uint16_t cmd_num,
                                   const Telecmd_Argument* argument,
                                   uint8_t* out_buf,
                                   size_t out_buf_size);

bool     parse_telecommand_packet(Telecmd_Packet_Frame* out_rx,
                                  const uint8_t* buf,
                                  size_t len);

bool     telecommand_check_length(size_t len);
bool     telecommand_check_sof(const uint8_t* buf, size_t len);
bool     telecommand_validate_packet(const uint8_t* buf, size_t len);

void     telecommand_set_cmd_num(Telecmd_Header* hdr, uint16_t cmd_num);
uint16_t telecommand_get_cmd_num(const Telecmd_Packet_Frame* pkt);

#ifdef __cplusplus
}
#endif

#endif  /* TELECOMMAND_H */
