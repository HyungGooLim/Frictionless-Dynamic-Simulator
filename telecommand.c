#include "telecommand.h"
#include "proto_crc.h"
#include <string.h>

/* ---------- 내부 헬퍼 ---------- */
static uint16_t rd_u16_be(const uint8_t* p)
{
    if (!p) return 0;
    return (uint16_t)(((uint16_t)p[0] << 8) | (uint16_t)p[1]);
}

static void wr_u16_be(uint8_t* p, uint16_t v)
{
    if (!p) return;
    p[0] = (uint8_t)((v >> 8) & 0xFF);
    p[1] = (uint8_t)(v & 0xFF);
}

/* ---------- Public API ---------- */
bool telecommand_check_length(size_t len)
{
    return (len == TELECMD_PACKET_SIZE);
}

bool telecommand_check_sof(const uint8_t* buf, size_t len)
{
    if (!buf || len < TELECMD_HEADER_SOF_LEN) return false;
    return (buf[0] == TELECMD_SOF0 &&
            buf[1] == TELECMD_SOF1 &&
            buf[2] == TELECMD_SOF2 &&
            buf[3] == TELECMD_SOF3);
}

bool telecommand_validate_packet(const uint8_t* buf, size_t len)
{
    uint16_t got_crc, cal_crc;

    if (!buf) return false;
    if (!telecommand_check_length(len)) return false;
    if (!telecommand_check_sof(buf, len)) return false;

    got_crc = rd_u16_be(&buf[len - CRC_SIZE]);
    cal_crc = calculate_crc16(buf, len - CRC_SIZE);
    return (got_crc == cal_crc);
}

void telecommand_set_cmd_num(Telecmd_Header* hdr, uint16_t cmd_num)
{
    if (!hdr) return;
    wr_u16_be(hdr->cmd_num, cmd_num);
}

uint16_t telecommand_get_cmd_num(const Telecmd_Packet_Frame* pkt)
{
    if (!pkt) return 0;
    return rd_u16_be(pkt->header.cmd_num);
}

size_t create_telecommand_packet(uint16_t cmd_num,
                                 const Telecmd_Argument* argument,
                                 uint8_t* out_buf,
                                 size_t out_buf_size)
{
    size_t   i = 0;
    uint16_t crc = 0;

    if (!argument || !out_buf) return 0;
    if (out_buf_size < TELECMD_PACKET_SIZE) return 0;

    /* 1) SOF (4 B) */
    out_buf[i++] = TELECMD_SOF0;
    out_buf[i++] = TELECMD_SOF1;
    out_buf[i++] = TELECMD_SOF2;
    out_buf[i++] = TELECMD_SOF3;

    /* 2) cmd_num (2 B, big-endian) */
    wr_u16_be(&out_buf[i], cmd_num);
    i += TELECMD_HEADER_CMD_NUM_LEN;

    out_buf[i++] = argument->cmd;

    memcpy(&out_buf[i], argument->payload, TELECMD_ARGU_PAYLOAD_LEN);
    i += TELECMD_ARGU_PAYLOAD_LEN;

    crc = calculate_crc16(out_buf, i);
    wr_u16_be(&out_buf[i], crc);
    i += CRC_SIZE;

    return i;
}

bool parse_telecommand_packet(Telecmd_Packet_Frame* out_rx,
                              const uint8_t* buf,
                              size_t len)
{
    size_t i = 0;

    if (!out_rx || !buf) return false;
    if (!telecommand_validate_packet(buf, len)) return false;

    out_rx->pk_len = len;

    memcpy(out_rx->header.SOF, &buf[i], TELECMD_HEADER_SOF_LEN);
    i += TELECMD_HEADER_SOF_LEN;

    memcpy(out_rx->header.cmd_num, &buf[i], TELECMD_HEADER_CMD_NUM_LEN);
    i += TELECMD_HEADER_CMD_NUM_LEN;

    out_rx->argument.cmd = buf[i++];

    memcpy(out_rx->argument.payload, &buf[i], TELECMD_ARGU_PAYLOAD_LEN);
    i += TELECMD_ARGU_PAYLOAD_LEN;

    out_rx->crc = rd_u16_be(&buf[i]);

    return true;
}
