/*
 * main.ino
 *
 * TEST_MODE 값으로 검증 대상을 선택합니다.
 *
 *   0 : Beacon 주기적 송신 검증  — Arduino가 비콘 패킷을 주기적으로 송신
 *   1 : Beacon 수신 및 파싱 검증 — Arduino가 수신한 비콘 패킷을 파싱
 *   2 : TC 송신 검증             — Arduino = Host PC,  RealTerm = Module
 *   3 : TC 수신 검증             — Arduino = Module,   RealTerm = Host PC
 *   4 : 추력기 제어 검증         — INF_CONTROL 수신 시 mA 출력 + DAC 제어
 *
 * 배선:
 *   Arduino Serial  <--USB-->  PC Serial Monitor (디버그 로그)
 *   Arduino Serial1 <--UART--> Zigbee / USB-UART 어댑터 (RealTerm)
 */

#include <stdint.h>
#include <string.h>
#include "platform.h"
#include "user.h"
#include "beacon.h"
#include "proto_crc.h"
#include "tc_handler.h"
#include "telecommand_res.h"
#include "thruster.h"

/* ── 검증 모드 선택 ───────────────────────────────────── */
#define TEST_MODE  0  /* 0=Beacon송신  1=Beacon수신  2=TC송신  3=TC수신  4=추력기제어 */

/* ════════════════════════════════════════════════════════
 *  MODE 0 : Beacon 주기적 송신 검증
 *
 *  동작 흐름:
 *    1) 1초마다 비콘 패킷을 생성하여 Serial1으로 송신
 *    2) Serial Monitor에서 전송된 hex 바이트 및 타임스탬프 확인
 *    3) RealTerm(또는 MODE 1 Arduino)에서 수신 여부 확인
 * ════════════════════════════════════════════════════════ */
#if TEST_MODE == 0

#define BEACON_INTERVAL_MS 1000
static uint8_t g_beacon_pkt[BEACON_PACKET_SIZE];

static size_t make_beacon_pkt(uint8_t* out)
{
    Beacon_Argument arg;
    memset(&arg, 0, sizeof(arg));
    arg.module_name[0]   = MOD_TARG;
    arg.clock            = 0xDEADBEEFUL;
    arg.sw_version       = SOFTWARE_VERSION;
    arg.state[STATE_X]   = 200.0f;
    arg.state[STATE_Y]   = 500.0f;
    arg.state[STATE_YAW] = 900.0f;
    arg.power            = 100;
    arg.thruster_state   = 0xAB;
    return create_beacon_packet(MODULE_OK, &arg, out);
}

void setup(void)
{
    platform_init();
    platform_log("=== MODE 0: Beacon TX ===\n");
}

void loop(void)
{
    static unsigned long last_ms = 0;
    unsigned long now = millis();
    if (now - last_ms < BEACON_INTERVAL_MS) return;
    last_ms = now;

    size_t n = make_beacon_pkt(g_beacon_pkt);
    platform_uart_write(g_beacon_pkt, n);

    platform_log("Beacon TX [%lums] ", now);
    platform_log("Beacon Packet Num [%d]", n);
    for (size_t i = 0; i < n; i++) platform_log("%02X ", g_beacon_pkt[i]);
    platform_log("\n");
}


/* ════════════════════════════════════════════════════════
 *  MODE 1 : Beacon 수신 및 파싱 검증
 *
 *  동작 흐름:
 *    1) Serial1으로 수신된 바이트를 버퍼에 누적
 *    2) BEACON_PACKET_SIZE만큼 쌓이면 SOF 확인
 *    3) parse_beacon_packet()으로 파싱 후 결과 출력
 *    4) SOF 불일치 시 버퍼를 1바이트씩 당겨 재탐색
 * ════════════════════════════════════════════════════════ */
#elif TEST_MODE == 1

static void beacon_parse_test(void)
{
    static uint8_t rx_buf[BEACON_PACKET_SIZE];
    static size_t  rx_len = 0;

    int got = platform_uart_read(rx_buf + rx_len, BEACON_PACKET_SIZE - rx_len);
    if (got > 0) rx_len += (size_t)got;
    if (rx_len < BEACON_PACKET_SIZE) return;

    if (!beacon_check_sof(rx_buf, rx_len)) {
        memmove(rx_buf, rx_buf + 1, rx_len - 1);
        rx_len--;
        return;
    }

    Beacon_Packet_Frame frame;
    if (parse_beacon_packet(&frame, rx_buf, BEACON_PACKET_SIZE)) {
        platform_log("[PARSE OK] status=%02X mod=%02X clock=%lu X=%.2f Y=%.2f YAW=%.2f pow=%u thr=%02X\n",
                     frame.header.status[0], frame.argument.module_name[0],
                     (unsigned long)frame.argument.clock,
                     (double)frame.argument.state[STATE_X],
                     (double)frame.argument.state[STATE_Y],
                     (double)frame.argument.state[STATE_YAW],
                     frame.argument.power,
                     frame.argument.thruster_state);
    } else {
        platform_log("[PARSE FAIL] CRC or format error\n");
    }

    memmove(rx_buf, rx_buf + BEACON_PACKET_SIZE, rx_len - BEACON_PACKET_SIZE);
    rx_len -= BEACON_PACKET_SIZE;
}

void setup(void)
{
    platform_init();
    platform_log("=== MODE 1: Beacon RX ===\n");
}

void loop(void)
{
    beacon_parse_test();
}


/* ════════════════════════════════════════════════════════
 *  MODE 2 : TC 송신 검증
 *
 *  동작 흐름:
 *    1) Arduino가 TC 패킷을 Serial1으로 송신
 *    2) Serial Monitor에서 전송된 바이트 확인
 *    3) RealTerm에서 TC_Res(ACK) 바이트를 직접 입력
 *    4) Arduino가 ACK 수신 후 결과 출력
 *    5) timeout 내 응답 없으면 재전송 후 최종 FAIL 출력
 *
 *  RealTerm 설정:
 *    - Port    : USB-UART 어댑터 COM 포트
 *    - Baud    : 9600
 *    - Display : Hex
 *    - Send    : 아래 setup() 로그에 출력되는 ACK 바이트 입력
 * ════════════════════════════════════════════════════════ */
#elif TEST_MODE == 2

static Telecmd_TxContext g_ctx;
static uint16_t          s_cmd_num = 0x0001;

static void print_hex(const char* label, const uint8_t* buf, size_t len)
{
    platform_log("%s", label);
    for (size_t i = 0; i < len; i++) platform_log("%02X ", buf[i]);
    platform_log("\r\n");
}

static void start_next_tc(void)
{
    Telecmd_Argument arg;
    uint8_t          preview[TELECMD_PACKET_SIZE];
    size_t           n;

    arg.cmd = INF_REQUEST;
    memset(arg.payload, 0, sizeof(arg.payload));

    n = create_telecommand_packet(s_cmd_num, &arg, preview, sizeof(preview));
    platform_log("--- cmd_num=0x%04X ---\r\n", s_cmd_num);
    print_hex("TX TC  : ", preview, n);

    tc_tx_start(&g_ctx, s_cmd_num, &arg);
    platform_log("TC 전송. RealTerm에서 TC_Res(ACK)를 전송하세요.\r\n");
}

void setup(void)
{
    platform_init();
    platform_log("=== MODE 2: TC Sender ===\r\n");

    tc_tx_init(&g_ctx, 1000, 2);
    start_next_tc();
}

void loop(void)
{
    static uint8_t prev_retry = 0xFF;

    tc_tx_run(&g_ctx);

    if (g_ctx.retry != prev_retry && g_ctx.retry > 0) {
        prev_retry = g_ctx.retry;
        platform_log(">> timeout. 재전송 #%u\r\n", g_ctx.retry);
    }

    if (tc_tx_is_done(&g_ctx)) {
        platform_log(">> [PASS] ACK 수신. cmd_num=0x%04X 성공\r\n", s_cmd_num);
        s_cmd_num++;
        prev_retry = 0xFF;
        start_next_tc();
    }
    if (tc_tx_is_fail(&g_ctx)) {
        platform_log(">> [FAIL] cmd_num=0x%04X 실패. 다음으로 진행\r\n", s_cmd_num);
        s_cmd_num++;
        prev_retry = 0xFF;
        start_next_tc();
    }
}


/* ════════════════════════════════════════════════════════
 *  MODE 3 : TC 수신 + TC_Res 송신 검증
 *
 *  동작 흐름:
 *    1) Serial Monitor에서 전송해야 할 TC 바이트 확인
 *    2) RealTerm에서 해당 바이트를 Serial1으로 전송
 *    3) Arduino가 수신 후 parse_telecommand_packet으로 파싱
 *    4) Serial Monitor에서 파싱 결과 확인
 *    5) Arduino가 TC_Res(ACK) 자동 송신 → RealTerm에서 수신 확인
 *
 *  RealTerm 설정:
 *    - Port    : USB-UART 어댑터 COM 포트
 *    - Baud    : 9600
 *    - Display : Hex
 *    - Send    : 아래 setup() 로그에 출력되는 TC 바이트 입력
 * ════════════════════════════════════════════════════════ */
#elif TEST_MODE == 3

static uint8_t  s_rx_buf[TELECMD_PACKET_SIZE];
static size_t   s_rx_len  = 0;
static uint16_t s_cmd_num = 0x0001;

static void print_hex(const char* label, const uint8_t* buf, size_t len)
{
    platform_log("%s", label);
    for (size_t i = 0; i < len; i++) platform_log("%02X ", buf[i]);
    platform_log("\r\n");
}

static void send_tc_res(uint16_t cmd_num, uint8_t cmd, uint8_t status)
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
    print_hex("TX TC_Res: ", buf, n);
}

void setup(void)
{
    Telecmd_Argument arg;
    uint8_t          preview[TELECMD_PACKET_SIZE];
    size_t           n;

    platform_init();
    thruster_init();
    platform_log("=== MODE 3: TC Receiver ===\r\n");

    arg.cmd = INF_REQUEST;
    memset(arg.payload, 0, sizeof(arg.payload));
    n = create_telecommand_packet(s_cmd_num, &arg, preview, sizeof(preview));
    platform_log("--- cmd_num=0x%04X ---\r\n", s_cmd_num);
    print_hex("RealTerm에서 전송할 TC 바이트: ", preview, n);
    platform_log("TC 수신 대기 중...\r\n");
}

void loop(void)
{
    int avail = platform_uart_available();
    if (avail > 0) {
        int n = platform_uart_read(s_rx_buf + s_rx_len,
                                   TELECMD_PACKET_SIZE - s_rx_len);
        if (n > 0) s_rx_len += (size_t)n;
    }

    if (s_rx_len < TELECMD_PACKET_SIZE) return;

    Telecmd_Packet_Frame pkt;
    if (parse_telecommand_packet(&pkt, s_rx_buf, s_rx_len)) {
        uint16_t cmd_num = telecommand_get_cmd_num(&pkt);
        platform_log("[PARSE OK] cmd_num=0x%04X  cmd=0x%02X\r\n",
                     cmd_num, pkt.argument.cmd);

        if (pkt.argument.cmd == INF_CONTROL) {
            bool ok = thruster_handle_control(pkt.argument.payload);
            send_tc_res(cmd_num, pkt.argument.cmd, ok ? STATUS_ACK : STATUS_NACK);
            platform_log(ok ? "[PASS] 추력기 제어 완료\r\n" : "[FAIL] 추력기 제어 실패\r\n");
        } else {
            send_tc_res(cmd_num, pkt.argument.cmd, STATUS_ACK);
            platform_log("[PASS] TC 수신 및 TC_Res(ACK) 송신 완료\r\n");
        }

        s_cmd_num++;
        uint8_t preview[TELECMD_PACKET_SIZE];
        Telecmd_Argument next_arg;
        next_arg.cmd = INF_REQUEST;
        memset(next_arg.payload, 0, sizeof(next_arg.payload));
        size_t n = create_telecommand_packet(s_cmd_num, &next_arg, preview, sizeof(preview));
        platform_log("--- 다음 cmd_num=0x%04X ---\r\n", s_cmd_num);
        print_hex("다음 TC 바이트: ", preview, n);
    } else {
        platform_log("[FAIL] 패킷 검증 실패 (SOF/CRC 오류)\r\n");
    }

    s_rx_len = 0;
    memset(s_rx_buf, 0, sizeof(s_rx_buf));
}

/* ════════════════════════════════════════════════════════
 *  MODE 4 : 추력기 제어 검증 (INF_CONTROL 수신 + DAC 출력)
 *
 *  동작 흐름:
 *    1) setup()에서 예시 INF_CONTROL 패킷 바이트를 Serial Monitor에 출력
 *    2) RealTerm에서 해당 바이트를 Serial1으로 전송
 *    3) INF_CONTROL(0x02) 수신 시 T1~T6 mA를 Serial Monitor에 출력
 *    4) thruster_set_ma()로 각 추력기 DAC 제어
 *    5) 전체 성공 시 ACK, 실패 시 NACK 응답
 *
 *  RealTerm 설정:
 *    - Port    : USB-UART 어댑터 COM 포트
 *    - Baud    : 9600
 *    - Display : Hex
 *    - Send    : setup() 로그에 출력되는 예시 패킷 바이트 입력
 * ════════════════════════════════════════════════════════ */
#elif TEST_MODE == 4

static uint8_t  s_rx_buf[TELECMD_PACKET_SIZE];
static size_t   s_rx_len  = 0;
static uint16_t s_cmd_num = 0x0001;

static void print_hex(const char* label, const uint8_t* buf, size_t len)
{
    platform_log("%s", label);
    for (size_t i = 0; i < len; i++) platform_log("%02X ", buf[i]);
    platform_log("\r\n");
}

static void send_tc_res(uint16_t cmd_num, uint8_t cmd, uint8_t status)
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
    print_hex("TX TC_Res: ", buf, n);
}

void setup(void)
{
    Telecmd_Argument arg;
    uint8_t          preview[TELECMD_PACKET_SIZE];
    size_t           n;

    platform_init();
    thruster_init();
    platform_log("=== MODE 4: Thruster Control Verify ===\r\n");

    /* 예시 INF_CONTROL 패킷 생성 후 출력 (T1~T6 = 5,10,15,5,10,15 mA) */
    arg.cmd        = INF_CONTROL;
    arg.payload[0] =  5;   /* T1 */
    arg.payload[1] = 10;   /* T2 */
    arg.payload[2] = 15;   /* T3 */
    arg.payload[3] =  5;   /* T4 */
    arg.payload[4] = 10;   /* T5 */
    arg.payload[5] = 15;   /* T6 */
    n = create_telecommand_packet(s_cmd_num, &arg, preview, sizeof(preview));

    platform_log("--- INF_CONTROL 예시 패킷 (cmd_num=0x%04X) ---\r\n", s_cmd_num);
    print_hex("RealTerm 전송 바이트: ", preview, n);
    platform_log("payload: T1=%umA T2=%umA T3=%umA T4=%umA T5=%umA T6=%umA\r\n",
                 arg.payload[0], arg.payload[1], arg.payload[2],
                 arg.payload[3], arg.payload[4], arg.payload[5]);
    platform_log("TC 수신 대기 중...\r\n");
}

void loop(void)
{
    int avail = platform_uart_available();
    if (avail > 0) {
        int got = platform_uart_read(s_rx_buf + s_rx_len,
                                     TELECMD_PACKET_SIZE - s_rx_len);
        if (got > 0) s_rx_len += (size_t)got;
    }
    if (s_rx_len < TELECMD_PACKET_SIZE) return;

    Telecmd_Packet_Frame pkt;
    if (parse_telecommand_packet(&pkt, s_rx_buf, s_rx_len)) {
        uint16_t cmd_num = telecommand_get_cmd_num(&pkt);
        platform_log("[PARSE OK] cmd_num=0x%04X  cmd=0x%02X\r\n",
                     cmd_num, pkt.argument.cmd);

        if (pkt.argument.cmd == INF_CONTROL) {
            ThrusterCmd cmd;
            if (thruster_parse_payload(pkt.argument.payload, &cmd)) {
                /* Serial Monitor에 파싱된 mA 출력 */
                platform_log("[THRUSTER] 수신 mA >> ");
                for (uint8_t i = 0; i < THRUSTER_COUNT; i++)
                    platform_log("T%u=%umA ", (unsigned)(i + 1), (unsigned)cmd.ma[i]);
                platform_log("\r\n");

                /* DAC 제어 — 추력기별 결과 thruster_set_ma() 내부에서 로그 출력 */
                bool all_ok = true;
                for (uint8_t i = 0; i < THRUSTER_COUNT; i++) {
                    if (!thruster_set_ma(i, (float)cmd.ma[i]))
                        all_ok = false;
                }
                send_tc_res(cmd_num, pkt.argument.cmd,
                            all_ok ? STATUS_ACK : STATUS_NACK);
                platform_log(all_ok ? "[PASS] 전체 추력기 제어 완료\r\n"
                                    : "[FAIL] 일부 추력기 제어 실패\r\n");
            } else {
                send_tc_res(cmd_num, pkt.argument.cmd, STATUS_NACK);
                platform_log("[FAIL] payload 파싱 실패\r\n");
            }
        } else {
            send_tc_res(cmd_num, pkt.argument.cmd, STATUS_ACK);
            platform_log("[INFO] cmd=0x%02X → ACK 송신\r\n", pkt.argument.cmd);
        }

        s_cmd_num++;
    } else {
        platform_log("[FAIL] 패킷 검증 실패 (SOF/CRC 오류)\r\n");
    }

    s_rx_len = 0;
    memset(s_rx_buf, 0, sizeof(s_rx_buf));
}

#endif
