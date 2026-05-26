#ifndef TC_RX_HANDLER_H
#define TC_RX_HANDLER_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "telecommand.h"
#include "telecommand_res.h"

/*
 * TC 수신 처리기
 *
 * INF_CLOCK : 수신 즉시 time_sync_apply() 호출 후 ACK 자동 송신 (내부 처리)
 * 그 외 CMD : pkt_pending 플래그 설정 → 애플리케이션이 tc_rx_get_pkt()로
 *             패킷을 읽은 뒤 tc_rx_respond()로 ACK/NACK 송신
 */
typedef struct {
    uint8_t              rx_buf[TELECMD_PACKET_SIZE];
    size_t               rx_len;
    bool                 pkt_pending;
    Telecmd_Packet_Frame pkt;
} Telecmd_RxContext;

#ifdef __cplusplus
extern "C" {
#endif

void                        tc_rx_init(Telecmd_RxContext* c);
void                        tc_rx_run(Telecmd_RxContext* c);
bool                        tc_rx_pkt_pending(const Telecmd_RxContext* c);
const Telecmd_Packet_Frame* tc_rx_get_pkt(const Telecmd_RxContext* c);
void                        tc_rx_respond(Telecmd_RxContext* c, uint8_t status);

#ifdef __cplusplus
}
#endif

#endif  /* TC_RX_HANDLER_H */
