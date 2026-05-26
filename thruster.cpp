#include "thruster.h"
#include "user.h"
#include "platform.h"
#include <ModbusMaster.h>


static ModbusMaster g_node;

/* MAX485 half-duplex 방향 전환 콜백 */
static void pre_tx(void)  { digitalWrite(THRUSTER_RS485_DE_PIN, HIGH); }
static void post_tx(void) { digitalWrite(THRUSTER_RS485_DE_PIN, LOW);  }

// Serial3 + MAX485 초기화
void thruster_init(void)
{
    pinMode(THRUSTER_RS485_DE_PIN, OUTPUT);
    digitalWrite(THRUSTER_RS485_DE_PIN, LOW);  /* 초기 수신 모드 */

    /* UART3 핀(RX3/TX3)으로 RS-485 모듈 연결 */
    Serial3.begin(THRUSTER_BAUD);
    g_node.begin(THRUSTER_MODBUS_SLAVE_ID, Serial3);
    g_node.preTransmission(pre_tx);
    g_node.postTransmission(post_tx);

    platform_log("[THRUSTER] init OK (UART3, slave=%u)\n", THRUSTER_MODBUS_SLAVE_ID);
}

//  6바이트 payload → ThrusterCmd 파싱 + 범위 검증 (추력기 6개 동시)
bool thruster_parse_payload(const uint8_t* payload, ThrusterCmd* out)
{
    if (!payload || !out) return false;

    for (uint8_t i = 0; i < THRUSTER_COUNT; i++) {
        if (payload[i] > (uint8_t)THRUSTER_MA_MAX) {
            platform_log("[THRUSTER] id=%u mA out of range: %u (max 20)\n", i, payload[i]);
            return false;
        }
        out->ma[i] = payload[i];
    }
    return true;
}

//  Modbus writeSingleRegister로 DAC 출력
bool thruster_set_ma(uint8_t thruster_id, float ma)
{
    if (thruster_id >= THRUSTER_COUNT) return false;

    /* 안전 클램핑 */
    if (ma < 0.0f)            ma = 0.0f;
    if (ma > THRUSTER_MA_MAX) ma = THRUSTER_MA_MAX;

    uint16_t dac_val  = (uint16_t)(ma * THRUSTER_DAC_PER_MA);
    uint16_t reg_addr = (uint16_t)(THRUSTER_DAC_BASE_ADDR + thruster_id);

    uint8_t result = g_node.writeSingleRegister(reg_addr, dac_val);

    if (result == ModbusMaster::ku8MBSuccess) {
        platform_log("[THRUSTER] id=%u %.1f mA -> DAC %u (reg 0x%04X)\n",
                     thruster_id, (double)ma, dac_val, reg_addr);
        return true;
    }

    platform_log("[THRUSTER ERR] id=%u Modbus error=0x%02X\n", thruster_id, result);
    return false;
}

// 위 두 함수(thruster_parse_payload,thruster_set_ma)를 한 번에 호출하는 편의 함수
bool thruster_handle_control(const uint8_t* payload)
{
    ThrusterCmd cmd;
    if (!thruster_parse_payload(payload, &cmd)) return false;

    bool all_ok = true;
    for (uint8_t i = 0; i < THRUSTER_COUNT; i++) {
        if (!thruster_set_ma(i, (float)cmd.ma[i]))
            all_ok = false;
    }
    return all_ok;
}
