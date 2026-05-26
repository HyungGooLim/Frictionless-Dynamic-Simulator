#ifndef BEACON_RX_HANDLER_H
#define BEACON_RX_HANDLER_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "beacon.h"

/*
 * Beacon 수신 처리기
 *
 * - SOF 슬라이딩 탐색: SOF 불일치 시 1바이트씩 앞으로 당겨 재탐색
 * - CRC 검증 포함 파싱 성공 시 pkt_pending 설정
 * - 애플리케이션은 beacon_rx_get_pkt()로 패킷을 읽은 뒤 beacon_rx_clear() 호출
 */
typedef struct {
    uint8_t             rx_buf[BEACON_PACKET_SIZE];
    size_t              rx_len;
    bool                pkt_pending;
    Beacon_Packet_Frame pkt;
} Beacon_RxContext;

#ifdef __cplusplus
extern "C" {
#endif

void                       beacon_rx_init(Beacon_RxContext* c);
void                       beacon_rx_run(Beacon_RxContext* c);
bool                       beacon_rx_pkt_pending(const Beacon_RxContext* c);
const Beacon_Packet_Frame* beacon_rx_get_pkt(const Beacon_RxContext* c);
void                       beacon_rx_clear(Beacon_RxContext* c);

#ifdef __cplusplus
}
#endif

#endif  /* BEACON_RX_HANDLER_H */
