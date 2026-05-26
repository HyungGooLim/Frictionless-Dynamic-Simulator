#include "beacon_rx_handler.h"
#include "platform.h"
#include <string.h>

void beacon_rx_init(Beacon_RxContext* c)
{
    if (!c) return;
    memset(c, 0, sizeof(*c));
}

void beacon_rx_run(Beacon_RxContext* c)
{
    if (!c || c->pkt_pending) return;

    int got = platform_uart_read(c->rx_buf + c->rx_len,
                                 BEACON_PACKET_SIZE - c->rx_len);
    if (got > 0) c->rx_len += (size_t)got;
    if (c->rx_len < BEACON_PACKET_SIZE) return;

    /* SOF 불일치: 1바이트 슬라이드 후 재탐색 */
    if (!beacon_check_sof(c->rx_buf, c->rx_len)) {
        memmove(c->rx_buf, c->rx_buf + 1, c->rx_len - 1);
        c->rx_len--;
        return;
    }

    /* SOF 일치 → CRC 포함 전체 파싱 */
    if (!parse_beacon_packet(&c->pkt, c->rx_buf, c->rx_len)) {
        platform_log("[BCN_RX] CRC 오류 → 슬라이드\r\n");
        memmove(c->rx_buf, c->rx_buf + 1, c->rx_len - 1);
        c->rx_len--;
        return;
    }

    /* 파싱 성공: 소비한 바이트 제거 */
    c->rx_len -= BEACON_PACKET_SIZE;
    if (c->rx_len > 0)
        memmove(c->rx_buf, c->rx_buf + BEACON_PACKET_SIZE, c->rx_len);

    c->pkt_pending = true;
}

bool beacon_rx_pkt_pending(const Beacon_RxContext* c)
{
    return (c && c->pkt_pending);
}

const Beacon_Packet_Frame* beacon_rx_get_pkt(const Beacon_RxContext* c)
{
    if (!c || !c->pkt_pending) return NULL;
    return &c->pkt;
}

void beacon_rx_clear(Beacon_RxContext* c)
{
    if (!c) return;
    c->pkt_pending = false;
}
