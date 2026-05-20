#include "telecommand_res.h"
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
bool telecommand_res_check_length(size_t len)
{
    return (len == TELECMD_RES_PACKET_SIZE);
}

bool telecommand_res_check_sof(const uint8_t* buf, size_t len)
{
    if (!buf || len < TELECMD_RES_SOF_LEN) return false;
    return (buf[0] == TELECMD_RES_SOF0 &&
            buf[1] == TELECMD_RES_SOF1 &&
            buf[2] == TELECMD_RES_SOF2 &&
            buf[3] == TELECMD_RES_SOF3);
}

bool telecommand_res_validate_packet(const uint8_t* buf, size_t len)
{
    uint16_t got_crc, cal_crc;

    if (!buf) return false;
    if (!telecommand_res_check_length(len)) return false;
    if (!telecommand_res_check_sof(buf, len)) return false;

    got_crc = rd_u16_be(&buf[len - CRC_SIZE]);
    cal_crc = calculate_crc16(buf, len - CRC_SIZE);
    return (got_crc == cal_crc);
}

void telecommand_res_set_cmd_num(Telecmd_Res_Header* hdr, uint16_t cmd_num)
{
    if (!hdr) return;
    wr_u16_be(hdr->cmd_num, cmd_num);
}

uint16_t telecommand_res_get_cmd_num(const Telecmd_Res_Packet_Frame* pkt)
{
    if (!pkt) return 0;
    return rd_u16_be(pkt->header.cmd_num);
}

size_t create_telecommand_res_packet(const Telecmd_Res_Header* header,
                                     const Telecmd_Res_Argument* argument,
                                     uint8_t* out_buf,
                                     size_t out_buf_size)
{
    size_t   i = 0;
    uint16_t crc = 0;

    if (!header || !argument || !out_buf) return 0;
    if (out_buf_size < TELECMD_RES_PACKET_SIZE) return 0;

    memcpy(&out_buf[i], header->SOF, TELECMD_RES_SOF_LEN);
    i += TELECMD_RES_SOF_LEN;

    out_buf[i++] = header->status;

    memcpy(&out_buf[i], header->cmd_num, TELECMD_RES_CMD_NUM_LEN);
    i += TELECMD_RES_CMD_NUM_LEN;

    out_buf[i++] = argument->cmd;

    memcpy(&out_buf[i], argument->payload, TELECMD_RES_ARGU_PAYLOAD_LEN);
    i += TELECMD_RES_ARGU_PAYLOAD_LEN;

    crc = calculate_crc16(out_buf, i);
    wr_u16_be(&out_buf[i], crc);
    i += CRC_SIZE;

    return i;
}

bool parse_telecommand_res_packet(Telecmd_Res_Packet_Frame* out_rx,
                                  const uint8_t* buf,
                                  size_t len)
{
    size_t i = 0;

    if (!out_rx || !buf) return false;
    if (!telecommand_res_validate_packet(buf, len)) return false;

    out_rx->pk_len = len;

    memcpy(out_rx->header.SOF, &buf[i], TELECMD_RES_SOF_LEN);
    i += TELECMD_RES_SOF_LEN;

    out_rx->header.status = buf[i++];

    memcpy(out_rx->header.cmd_num, &buf[i], TELECMD_RES_CMD_NUM_LEN);
    i += TELECMD_RES_CMD_NUM_LEN;

    out_rx->argument.cmd = buf[i++];

    memcpy(out_rx->argument.payload, &buf[i], TELECMD_RES_ARGU_PAYLOAD_LEN);
    i += TELECMD_RES_ARGU_PAYLOAD_LEN;

    out_rx->crc = rd_u16_be(&buf[i]);

    return true;
}
