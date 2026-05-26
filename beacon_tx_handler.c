#include "beacon_tx_handler.h"
#include "time_sync.h"
#include "platform.h"
#include <string.h>

void beacon_tx_init(Beacon_TxContext* c, uint32_t interval_ms)
{
    if (!c) return;
    memset(c, 0, sizeof(*c));
    c->interval_ms = interval_ms;
}

bool beacon_tx_run(Beacon_TxContext* c, uint8_t status, Beacon_Argument* arg)
{
    uint32_t now;
    size_t   n;

    if (!c || !arg) return false;

    now = platform_now_ms();
    if (now - c->last_tx_ms < c->interval_ms) return false;
    c->last_tx_ms = now;

    /* clock은 항상 time_get_epoch()로 자동 세팅 */
    arg->clock = time_get_epoch();

    n = create_beacon_packet(status, arg, c->pkt_buf);
    if (n == 0) return false;

    platform_uart_write(c->pkt_buf, n);
    return true;
}

void beacon_tx_set_interval(Beacon_TxContext* c, uint32_t interval_ms)
{
    if (!c || interval_ms == 0) return;
    c->interval_ms = interval_ms;
}

bool beacon_tx_apply_setting(Beacon_TxContext* c, const uint8_t* payload)
{
    if (!c || !payload) return false;
    if (payload[1] != BEACON_SETTING_INTERVAL) return false;

    uint8_t sec = payload[0];
    if (sec != 1 && sec != 2 && sec != 10) return false;

    beacon_tx_set_interval(c, (uint32_t)sec * 1000u);
    platform_log("[BCN_TX] interval → %us\r\n", (unsigned)sec);
    return true;
}
