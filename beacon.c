#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include "beacon.h"

size_t create_beacon_packet(uint8_t status,
                            const Beacon_Argument* a,
                            uint8_t* out)
{
    size_t   i = 0;
    uint16_t crc;

    if (!a || !out) return 0;

    /* 1) SOF (4 B) */
    out[i++] = BEACON_SOF0;
    out[i++] = BEACON_SOF1;
    out[i++] = BEACON_SOF2;
    out[i++] = BEACON_SOF3;

    /* 2) Status (1 B) */
    out[i++] = (uint8_t)status;

    /* 3) module_name (1 B) */
    out[i++] = a->module_name[0];

    /* 4) clock (4 B, big-endian) */
    wr_u32_be(&out[i], a->clock);
    i += 4;

    /* 5) sw_version (1 B) */
    out[i++] = a->sw_version;

    /* 6) state[3] (12 B, big-endian) */
    wr_f32_be(&out[i], a->state[STATE_X]);   i += 4;
    wr_f32_be(&out[i], a->state[STATE_Y]);   i += 4;
    wr_f32_be(&out[i], a->state[STATE_YAW]); i += 4;

    /* 7) power (1 B) */
    out[i++] = a->power;

    /* 8) thruster_state (1 B) */
    out[i++] = a->thruster_state;

    /* 여기까지: 4(SOF) + 1(status) + 20(argument) = 25 bytes */

    /* 9) CRC-16 (2 B, big-endian) — SOF 포함 25바이트 기준 */
    crc = calculate_crc16(out, BEACON_HEADER_SIZE + BEACON_ARGUMENT_SIZE);
    out[i++] = (uint8_t)(crc >> 8);
    out[i++] = (uint8_t)(crc & 0xFF);

    return i;  /* == BEACON_PACKET_SIZE (27) */
}

bool beacon_check_length(size_t len)
{
    return (len == BEACON_PACKET_SIZE);
}

bool beacon_check_sof(const uint8_t* buf, size_t len)
{
    if (!buf || len < BEACON_SOF_LEN) return false;
    return (buf[0] == BEACON_SOF0 &&
            buf[1] == BEACON_SOF1 &&
            buf[2] == BEACON_SOF2 &&
            buf[3] == BEACON_SOF3);
}

bool beacon_validate_packet(const uint8_t* buf, size_t len)
{
    if (!buf) return false;
    if (!beacon_check_length(len))   return false;
    if (!beacon_check_sof(buf, len)) return false;
    return verify_crc16_tail(buf, len);
}

bool parse_beacon_packet(Beacon_Packet_Frame* out,
                         const uint8_t* buf,
                         size_t len)
{
    size_t i = 0;

    if (!out || !buf) return false;
    if (!beacon_validate_packet(buf, len)) return false;

    /* SOF (4 B) */
    memcpy(out->header.SOF, &buf[i], BEACON_SOF_LEN);
    i += BEACON_SOF_LEN;

    /* status (1 B) */
    out->header.status[0] = buf[i++];

    /* module_name (1 B) */
    out->argument.module_name[0] = buf[i++];

    /* clock (4 B, big-endian) */
    out->argument.clock = rd_u32_be(&buf[i]);
    i += 4;

    /* sw_version (1 B) */
    out->argument.sw_version = buf[i++];

    /* state[3] (12 B, big-endian) */
    out->argument.state[STATE_X]   = rd_f32_be(&buf[i]); i += 4;
    out->argument.state[STATE_Y]   = rd_f32_be(&buf[i]); i += 4;
    out->argument.state[STATE_YAW] = rd_f32_be(&buf[i]); i += 4;

    /* power (1 B) */
    out->argument.power = buf[i++];

    /* thruster_state (1 B) */
    out->argument.thruster_state = buf[i++];

    /* CRC (2 B, big-endian) */
    out->crc = rd_u16_be(&buf[i]);

    return true;
}
