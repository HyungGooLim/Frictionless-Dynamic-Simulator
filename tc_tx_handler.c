#include "tc_tx_handler.h"
#include "platform.h"
#include <string.h>

// Telecmd_TxContext 구조체 전체 초기화 //
static void tc_zero_context(Telecmd_TxContext* c)
{
    if (!c) return;
    memset(c, 0, sizeof(*c));
}


void tc_tx_init(Telecmd_TxContext* c, uint32_t timeout_ms, uint8_t max_retry)
{
    if (!c) return;
    tc_zero_context(c);
    c->state       = TC_TX_IDLE;
    c->timeout_ms  = timeout_ms;
    c->max_retry   = max_retry;
    c->last_status = 0xFF;
}

void tc_tx_reset(Telecmd_TxContext* c)
{
    uint32_t timeout_ms;
    uint8_t  max_retry;

    if (!c) return;
    timeout_ms = c->timeout_ms;
    max_retry  = c->max_retry;
    tc_zero_context(c);
    c->state       = TC_TX_IDLE;
    c->timeout_ms  = timeout_ms;
    c->max_retry   = max_retry;
    c->last_status = 0xFF;
}

bool tc_tx_start(Telecmd_TxContext* c,
                 uint16_t cmd_num,
                 const Telecmd_Argument* argument)
{
    size_t n;

    if (!c || !argument) return false;
    if (!(c->state == TC_TX_IDLE ||
          c->state == TC_TX_DONE ||
          c->state == TC_TX_FAIL)) {
        return false;
    }

    memset(&c->req_pkt,      0, sizeof(c->req_pkt));
    memset(&c->last_res_pkt, 0, sizeof(c->last_res_pkt));
    memset(c->tx_pkt,        0, sizeof(c->tx_pkt));

    n = create_telecommand_packet(cmd_num, argument, c->tx_pkt, sizeof(c->tx_pkt));
    if (n != TELECMD_PACKET_SIZE) return false;

    c->tx_len      = n;
    c->last_status = 0xFF;
    c->retry       = 0;
    c->deadline_ms = 0;

    c->req_pkt.pk_len         = TELECMD_PACKET_SIZE;
    c->req_pkt.header.SOF[0]  = TELECMD_SOF0;
    c->req_pkt.header.SOF[1]  = TELECMD_SOF1;
    c->req_pkt.header.SOF[2]  = TELECMD_SOF2;
    c->req_pkt.header.SOF[3]  = TELECMD_SOF3;
    telecommand_set_cmd_num(&c->req_pkt.header, cmd_num);
    c->req_pkt.argument.cmd = argument->cmd;
    memcpy(c->req_pkt.argument.payload, argument->payload, TELECMD_ARGU_PAYLOAD_LEN);

    c->state = TC_TX_SEND;
    return true;
}

// TC_TX_SEND로 state 변경 //
bool tc_tx_need_send(const Telecmd_TxContext* c)
{
    return (c && c->state == TC_TX_SEND);
}

// TC_패킷 return //
const uint8_t* tc_tx_data(const Telecmd_TxContext* c)
{
    if (!c) return NULL;
    return c->tx_pkt;
}

// TC_패킷 크기값 return //
size_t tc_tx_size(const Telecmd_TxContext* c)
{
    if (!c) return 0;
    return c->tx_len;
}

// TC를 전송후 호출하는 함수. 응답 대기(TC_TX_WAIT_RES) 상태로 status 전환 //
bool tc_tx_mark_sent(Telecmd_TxContext* c, uint32_t now_ms)
{
    if (!c) return false;
    if (c->state != TC_TX_SEND) return false;
    c->deadline_ms = now_ms + c->timeout_ms;
    c->state = TC_TX_WAIT_RES;
    return true;
}

// Module : TC를 수신했을때 호출되는 함수 //
// 응답패킷 유효성 판단 후, ACK/NACK 호출 //
bool tc_tx_on_response(Telecmd_TxContext* c,
                       const uint8_t* buf,
                       size_t len,
                       uint32_t now_ms)
{
    Telecmd_Res_Packet_Frame res;
    uint16_t req_cmd_num, res_cmd_num;

    (void)now_ms;

    if (!c || !buf) return false;
    if (c->state != TC_TX_WAIT_RES) return false;

    // 1단계) SOF 및 CRC-16 검증
    memset(&res, 0, sizeof(res));
    if (!telecommand_res_check_length(len)) {
        platform_log("[RES FAIL] 길이 오류: got %u, expected %u\r\n",
                     (unsigned)len, (unsigned)TELECMD_RES_PACKET_SIZE);
        return false;
    }
    if (!telecommand_res_check_sof(buf, len)) {
        platform_log("[RES FAIL] SOF 불일치: %02X %02X %02X %02X\r\n",
                     buf[0], buf[1], buf[2], buf[3]);
        return false;
    }
    if (!parse_telecommand_res_packet(&res, buf, len)) {
        platform_log("[RES FAIL] CRC-16 오류\r\n");
        return false;
    }

    req_cmd_num = telecommand_get_cmd_num(&c->req_pkt);
    res_cmd_num = telecommand_res_get_cmd_num(&res);

    // 2단계) 내가보낸 cmd_num에 대한 응답인지 판단
    if (req_cmd_num != res_cmd_num) {
        platform_log("[RES FAIL] cmd_num 불일치: req=0x%04X res=0x%04X\r\n",
                     req_cmd_num, res_cmd_num);
        return false;
    }
    if (c->req_pkt.argument.cmd != res.argument.cmd) {
        platform_log("[RES FAIL] cmd 불일치: req=0x%02X res=0x%02X\r\n",
                     c->req_pkt.argument.cmd, res.argument.cmd);
        return false;
    }

    // 3단계) 응답패킷의 ACK/NACK를 판단 후, STATUS 설정
    c->last_res_pkt = res;
    c->last_status  = res.header.status;
    if (res.header.status == STATUS_ACK) {
        platform_log("[RES OK] ACK 수신 (cmd_num=0x%04X)\r\n", res_cmd_num);
        c->state = TC_TX_DONE;
    } else {
        platform_log("[RES OK] NACK 수신 (cmd_num=0x%04X)\r\n", res_cmd_num);
        c->state = TC_TX_FAIL;
    }
    return true;
}

// 타임아웃을 계산, 재전송 or 실패 처리 함수
void tc_tx_step(Telecmd_TxContext* c, uint32_t now_ms)
{
    if (!c || c->state != TC_TX_WAIT_RES) return;
    if (now_ms < c->deadline_ms) return;

    if (c->retry < c->max_retry) {
        c->retry++;
        c->state = TC_TX_SEND;
    } else {
        c->state = TC_TX_FAIL;
    }
}

// 상태처리 함수
bool tc_tx_is_idle(const Telecmd_TxContext* c)    { return (c && c->state == TC_TX_IDLE); }
bool tc_tx_is_done(const Telecmd_TxContext* c)    { return (c && c->state == TC_TX_DONE); }
bool tc_tx_is_fail(const Telecmd_TxContext* c)    { return (c && c->state == TC_TX_FAIL); }
bool tc_tx_is_waiting(const Telecmd_TxContext* c) { return (c && c->state == TC_TX_WAIT_RES); }

// TC_handler 상태머신의 메인루프 //
// 상태에 따라서, 동작하도록 설계 //
void tc_tx_run(Telecmd_TxContext* c)
{
    uint8_t  rx_buf[TELECMD_RES_PACKET_SIZE];
    int      n;
    uint32_t now;

    if (!c) return;

    now = platform_now_ms();

    switch (c->state) {
    case TC_TX_SEND:
        platform_uart_write(c->tx_pkt, c->tx_len);
        tc_tx_mark_sent(c, now);
        break;

    case TC_TX_WAIT_RES:
        if (platform_uart_available() >= TELECMD_RES_PACKET_SIZE) {
            n = platform_uart_read(rx_buf, sizeof(rx_buf));
            if (n > 0) {
                tc_tx_on_response(c, rx_buf, (size_t)n, now);
            }
        }
        tc_tx_step(c, now);
        break;

    default:
        break;
    }
}
