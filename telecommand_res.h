#ifndef TELECOMMAND_RES_H
#define TELECOMMAND_RES_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "proto_crc.h"
#include "telecommand.h"

/*
 * Telecommand Response Packet (총 14 Bytes)
 * +--------+-----------+-----------+-------+------------+--------+
 * | SOF(4) | STATUS(1) | CMD_NUM(2)| CMD(1)| PAYLOAD(4) | CRC(2) |
 * +--------+-----------+-----------+-------+------------+--------+
 */

/* ------------------------------- Header ------------------------------- */
#define TELECMD_RES_SOF_LEN       4
#define TELECMD_RES_SOF0          0xAA
#define TELECMD_RES_SOF1          0xBB
#define TELECMD_RES_SOF2          0xCC
#define TELECMD_RES_SOF3          0xDD

#define TELECMD_RES_STATUS_LEN    1
#define STATUS_ACK                0x00
#define STATUS_NACK               0x01

#define TELECMD_RES_CMD_NUM_LEN   2

#define TELECMD_RES_HEADER_LEN \
    (TELECMD_RES_SOF_LEN + TELECMD_RES_STATUS_LEN + TELECMD_RES_CMD_NUM_LEN)

/* ------------------------------- Argument ------------------------------- */
#define TELECMD_RES_ARGU_CMD_LEN      1
#define TELECMD_RES_ARGU_PAYLOAD_LEN  4
#define TELECMD_RES_ARGU_LEN \
    (TELECMD_RES_ARGU_CMD_LEN + TELECMD_RES_ARGU_PAYLOAD_LEN)

/* ------------------------------- Packet ------------------------------- */
#define TELECMD_RES_PACKET_SIZE \
    (TELECMD_RES_HEADER_LEN + TELECMD_RES_ARGU_LEN + CRC_SIZE)

/* ------------------------------- Structures ------------------------------- */
typedef struct Telecmd_Res_Header {
    uint8_t SOF[TELECMD_RES_SOF_LEN];
    uint8_t status;
    uint8_t cmd_num[TELECMD_RES_CMD_NUM_LEN];
} Telecmd_Res_Header;

typedef struct Telecmd_Res_Argument {
    uint8_t cmd;
    uint8_t payload[TELECMD_RES_ARGU_PAYLOAD_LEN];
} Telecmd_Res_Argument;

typedef struct Telecmd_Res_Packet_Frame {
    size_t               pk_len;
    Telecmd_Res_Header   header;
    Telecmd_Res_Argument argument;
    uint16_t             crc;
} Telecmd_Res_Packet_Frame;

/* ------------------------------- API ------------------------------- */
#ifdef __cplusplus
extern "C" {
#endif

size_t   create_telecommand_res_packet(const Telecmd_Res_Header* header,
                                       const Telecmd_Res_Argument* argument,
                                       uint8_t* out_buf,
                                       size_t out_buf_size);

bool     parse_telecommand_res_packet(Telecmd_Res_Packet_Frame* out_rx,
                                      const uint8_t* buf,
                                      size_t len);

bool     telecommand_res_check_length(size_t len);
bool     telecommand_res_check_sof(const uint8_t* buf, size_t len);
bool     telecommand_res_validate_packet(const uint8_t* buf, size_t len);
void     telecommand_res_set_cmd_num(Telecmd_Res_Header* hdr, uint16_t cmd_num);
uint16_t telecommand_res_get_cmd_num(const Telecmd_Res_Packet_Frame* pkt);

#ifdef __cplusplus
}
#endif

#endif  /* TELECOMMAND_RES_H */
