#ifndef BEACON_TX_HANDLER_H
#define BEACON_TX_HANDLER_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "beacon.h"

/*
 * Beacon 송신 핸들러
 *
 * - clock 필드는 time_get_epoch()로 자동 세팅 (애플리케이션이 신경 쓸 필요 없음)
 * - interval_ms마다 주기적으로 beacon 패킷을 Serial1으로 송신
 * - 애플리케이션은 센서 데이터(state, power, thruster_state)만 채워서 넘기면 됨
 */

/* 기본 비콘 송신 주기 */
#define BEACON_INTERVAL_DEFAULT_MS  1000u

/*
 * INF_SETTING payload 레이아웃 (6바이트)
 *   payload[0] : 새 주기 (초)  — 0x01=1s  0x02=2s  0x0A=10s
 *   payload[1] : 설정 타입     — BEACON_SETTING_INTERVAL(0x01)이면 비콘 주기 변경
 *   payload[2..5] : 미사용 (0x00)
 */
#define BEACON_SETTING_INTERVAL     0x01

typedef struct {
    uint32_t interval_ms;
    uint32_t last_tx_ms;
    uint8_t  pkt_buf[BEACON_PACKET_SIZE];
} Beacon_TxContext;

#ifdef __cplusplus
extern "C" {
#endif

void beacon_tx_init(Beacon_TxContext* c, uint32_t interval_ms);

/* loop()마다 호출. interval 경과 시 beacon 송신 후 true 반환 */
bool beacon_tx_run(Beacon_TxContext* c, uint8_t status, Beacon_Argument* arg);

/* 송신 주기 변경 */
void beacon_tx_set_interval(Beacon_TxContext* c, uint32_t interval_ms);

/* INF_SETTING payload 파싱 후 주기 적용. 성공 시 true */
bool beacon_tx_apply_setting(Beacon_TxContext* c, const uint8_t* payload);

#ifdef __cplusplus
}
#endif

#endif  /* BEACON_TX_HANDLER_H */
