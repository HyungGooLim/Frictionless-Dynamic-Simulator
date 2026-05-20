#ifndef THRUSTER_H
#define THRUSTER_H

#include <stdint.h>
#include <stdbool.h>

/*
 * INF_CONTROL (0x02) Payload Layout (6 bytes)
 * +-------+-------+-------+-------+-------+-------+
 * | T1 mA | T2 mA | T3 mA | T4 mA | T5 mA | T6 mA |
 * | (1 B) | (1 B) | (1 B) | (1 B) | (1 B) | (1 B) |
 * +-------+-------+-------+-------+-------+-------+
 *
 * T1~T6 mA : 0~20 (정수, 1 mA 해상도) — 6개 추력기 동시 제어
 */

/* RS-485 / Modbus 설정 */
#define THRUSTER_RS485_DE_PIN    3       /* MAX485 DE/RE 방향 제어 핀 */
#define THRUSTER_MODBUS_SLAVE_ID 1       /* Modbus 슬레이브 ID */
#define THRUSTER_BAUD            9600

/* 추력기 → DAC 채널 매핑 (8ch DAC, 추력기 0~5 → AO1~AO6) */
#define THRUSTER_COUNT           6
#define THRUSTER_DAC_BASE_ADDR   0x0000  /* AO1 시작 레지스터 주소 */

/* DAC 변환 상수 (DAC_control.ino 기반: 1 mA = 1258 unit) */
#define THRUSTER_DAC_PER_MA      1258.0f
#define THRUSTER_MA_MAX          20.0f

typedef struct {
    uint8_t ma[THRUSTER_COUNT];  /* ma[0]~ma[5]: 각 추력기 mA (0~20) */
} ThrusterCmd;

#ifdef __cplusplus
extern "C" {
#endif

/* Serial2(UART2)와 MAX485 핀 초기화 */
void thruster_init(void);

/* INF_CONTROL payload 6바이트를 ThrusterCmd로 파싱. 범위 초과 시 false */
bool thruster_parse_payload(const uint8_t* payload, ThrusterCmd* out);

/* 지정 추력기에 mA 출력 (Modbus writeSingleRegister). 성공 시 true */
bool thruster_set_ma(uint8_t thruster_id, float ma);

/* parse + set_ma 를 한 번에. INF_CONTROL 수신 핸들러에서 직접 호출 */
bool thruster_handle_control(const uint8_t* payload);

#ifdef __cplusplus
}
#endif

#endif /* THRUSTER_H */
