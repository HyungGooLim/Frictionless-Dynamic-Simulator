#include "tc_rx_handler.h"
#include "time_sync.h"
#include "platform.h"
#include "user.h"
#include <string.h>

static bool payload_all_ff(const uint8_t* payload)
{
    for (uint8_t i = 0; i < TELECMD_ARGU_PAYLOAD_LEN; i++) {
        if (payload[i] != 0xFF) return false;
    }
    return true;
}

static void send_res(uint16_t cmd_num, uint8_t cmd, uint8_t status)
{
    Telecmd_Res_Header   hdr;
    Telecmd_Res_Argument arg;
    uint8_t              buf[TELECMD_RES_PACKET_SIZE];
    size_t               n;

    hdr.SOF[0] = TELECMD_RES_SOF0;
    hdr.SOF[1] = TELECMD_RES_SOF1;
    hdr.SOF[2] = TELECMD_RES_SOF2;
    hdr.SOF[3] = TELECMD_RES_SOF3;
    hdr.status = status;
    telecommand_res_set_cmd_num(&hdr, cmd_num);
    arg.cmd = cmd;
    memset(arg.payload, 0, sizeof(arg.payload));

    n = create_telecommand_res_packet(&hdr, &arg, buf, sizeof(buf));
    platform_uart_write(buf, n);
}

void tc_rx_init(Telecmd_RxContext* c)
{
    if (!c) return;
    memset(c, 0, sizeof(*c));
}

void tc_rx_run(Telecmd_RxContext* c)
{
    if (!c || c->pkt_pending) return;

    int got = platform_uart_read(c->rx_buf + c->rx_len,
                                 TELECMD_PACKET_SIZE - c->rx_len);
    if (got > 0) c->rx_len += (size_t)got;
    if (c->rx_len < TELECMD_PACKET_SIZE) return;

    if (!parse_telecommand_packet(&c->pkt, c->rx_buf, c->rx_len)) {
        platform_log("[TC_RX] 패킷 검증 실패 (SOF/CRC 오류)\r\n");
        c->rx_len = 0;
        memset(c->rx_buf, 0, sizeof(c->rx_buf));
        return;
    }

    c->rx_len = 0;
    memset(c->rx_buf, 0, sizeof(c->rx_buf));

    uint16_t cmd_num = telecommand_get_cmd_num(&c->pkt);

    /* INF_CLOCK: 라이브러리 내부에서 자체 처리 */
    if (c->pkt.argument.cmd == INF_CLOCK) {
        uint32_t epoch = rd_u32_be(c->pkt.argument.payload);
        time_sync_apply(epoch);
        send_res(cmd_num, INF_CLOCK, STATUS_ACK);
        platform_log("[TC_RX] INF_CLOCK epoch=%lu synced\r\n", (unsigned long)epoch);
        return;
    }

    /* INF_RESET: payload 6바이트 모두 0xFF일 때만 리셋 */
    if (c->pkt.argument.cmd == INF_RESET) {
        if (payload_all_ff(c->pkt.argument.payload)) {
            send_res(cmd_num, INF_RESET, STATUS_ACK);
            platform_log("[TC_RX] INF_RESET → 리셋 실행\r\n");
            platform_delay_ms(1000);
            platform_reset();
        } else {
            send_res(cmd_num, INF_RESET, STATUS_NACK);
            platform_log("[TC_RX] INF_RESET payload 오류\r\n");
        }
        return;
    }

    /* 그 외 명령: 애플리케이션에서 처리 후 tc_rx_respond() 호출 */
    c->pkt_pending = true;
}

bool tc_rx_pkt_pending(const Telecmd_RxContext* c)
{
    return (c && c->pkt_pending);
}

const Telecmd_Packet_Frame* tc_rx_get_pkt(const Telecmd_RxContext* c)
{
    if (!c || !c->pkt_pending) return NULL;
    return &c->pkt;
}

void tc_rx_respond(Telecmd_RxContext* c, uint8_t status)
{
    if (!c || !c->pkt_pending) return;
    uint16_t cmd_num = telecommand_get_cmd_num(&c->pkt);
    send_res(cmd_num, c->pkt.argument.cmd, status);
    c->pkt_pending = false;
}
