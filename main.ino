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
#include "beacon_tx_handler.h"
#include "beacon_rx_handler.h"
#include "tc_tx_handler.h"
#include "tc_rx_handler.h"
#include "thruster.h"
#include "time_sync.h"

/* ── 검증 모드 선택 ───────────────────────────────────── */
#define TEST_MODE  4  /* 0=Beacon송신  1=Beacon수신  2=TC송신  3=TC수신  4=추력기제어 */

/* ── INF_CLOCK 검증용 공통 epoch (MODE 2 송신 / MODE 3 수신 공유) ── */
#define TEST_EPOCH_VAL  1779779253UL  /* 2026-05-26 16:07:33 UTC . hex : 6A 15 46 B5 00 00*/

/* ════════════════════════════════════════════════════════
 *  MODE 0 : Beacon 주기적 송신 검증
 *
 *  동작 흐름:
 *    1) 1초마다 비콘 패킷을 생성하여 Serial1으로 송신
 *    2) Serial Monitor에서 전송된 hex 바이트 및 타임스탬프 확인
 *    3) RealTerm(또는 MODE 1 Arduino)에서 수신 여부 확인
 * ════════════════════════════════════════════════════════ */
#if TEST_MODE == 0

static Beacon_TxContext  g_beacon_ctx;
static Telecmd_RxContext g_rx_ctx;

void setup(void)
{
    platform_init();
    beacon_tx_init(&g_beacon_ctx, BEACON_INTERVAL_DEFAULT_MS);
    tc_rx_init(&g_rx_ctx);
    platform_log("=== MODE 0: Beacon TX ===\n");
    platform_log("INF_CLOCK 수신 시 자동 clock sync.\n");
}

void loop(void)
{
    /* INF_CLOCK / INF_RESET: 라이브러리 자동 처리 */
    tc_rx_run(&g_rx_ctx);
    if (tc_rx_pkt_pending(&g_rx_ctx)) {
        const Telecmd_Packet_Frame* pkt = tc_rx_get_pkt(&g_rx_ctx);
        if (pkt->argument.cmd == INF_SETTING) {
            bool ok = beacon_tx_apply_setting(&g_beacon_ctx, pkt->argument.payload);
            tc_rx_respond(&g_rx_ctx, ok ? STATUS_ACK : STATUS_NACK);
        } else {
            tc_rx_respond(&g_rx_ctx, STATUS_ACK);
        }
    }

    /* 센서 데이터 구성 (clock은 beacon_tx_run 내부에서 자동 세팅) */
    Beacon_Argument arg;
    memset(&arg, 0, sizeof(arg));
    arg.module_name[0] = MOD_TARG;
    arg.sw_version     = SOFTWARE_VERSION;
    arg.state[STATE_X]   = 200.0f;
    arg.state[STATE_Y]   = 500.0f;
    arg.state[STATE_YAW] = 900.0f;
    arg.power            = 100;
    arg.thruster_state   = 0xAB;

    if (beacon_tx_run(&g_beacon_ctx, MODULE_OK, &arg)) {
        platform_log("Beacon TX [%lums] clock=%lu synced=%d\n",
                     (unsigned long)platform_now_ms(),
                     (unsigned long)arg.clock,
                     (int)time_is_synced());
    }
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

static Beacon_RxContext g_bcn_rx;

void setup(void)
{
    platform_init();
    beacon_rx_init(&g_bcn_rx);
    platform_log("=== MODE 1: Beacon RX ===\n");
}

void loop(void)
{
    beacon_rx_run(&g_bcn_rx);
    if (!beacon_rx_pkt_pending(&g_bcn_rx)) return;

    const Beacon_Packet_Frame* f = beacon_rx_get_pkt(&g_bcn_rx);
    platform_log("[PARSE OK] status=%02X mod=%02X clock=%lu X=%.2f Y=%.2f YAW=%.2f pow=%u thr=%02X\n",
                 f->header.status[0], f->argument.module_name[0],
                 (unsigned long)f->argument.clock,
                 (double)f->argument.state[STATE_X],
                 (double)f->argument.state[STATE_Y],
                 (double)f->argument.state[STATE_YAW],
                 f->argument.power,
                 f->argument.thruster_state);
    beacon_rx_clear(&g_bcn_rx);
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

/* ── INF_CLOCK 검증 설정 ─────────────────────────────────── */
#define VERIFY_WAIT_MS  5000          /* INF_CLOCK ACK 후 비콘 대기 시간 (ms) */
#define CLOCK_TOLERANCE 3             /* 허용 오차 (초) */

/*
 * 검증 흐름:
 *   [M2_CLOCK_SEND] INF_CLOCK(epoch) 전송
 *        │ ACK 수신
 *        ▼
 *   [M2_CLOCK_WAIT] VERIFY_WAIT_MS 동안 모듈 비콘 수신 대기
 *        │ 비콘 수신 → clock 필드와 expected 비교 → PASS/FAIL 출력
 *        │ 타임아웃  → 모듈 Serial Monitor에서 직접 확인 안내
 *        ▼
 *   [M2_NORMAL] 일반 INF_REQUEST 루프
 *
 * 비콘 검증을 위해 모듈(상대방 Arduino)은 INF_CLOCK 수신 후
 * 비콘을 주기적으로 송신하는 상태여야 합니다.
 */
typedef enum {
    M2_CLOCK_SEND,
    M2_CLOCK_WAIT,
    M2_NORMAL
} Mode2Phase;

static Telecmd_TxContext g_ctx;
static Beacon_RxContext  g_bcn_rx;
static uint16_t          s_cmd_num      = 0x0001;
static Mode2Phase        g_phase        = M2_CLOCK_SEND;
static uint32_t          g_sent_epoch   = 0;
static uint32_t          g_sent_ms      = 0;
static uint32_t          g_verify_until = 0;

static void print_hex(const char* label, const uint8_t* buf, size_t len)
{
    platform_log("%s", label);
    for (size_t i = 0; i < len; i++) platform_log("%02X ", buf[i]);
    platform_log("\r\n");
}

static void send_clock_tc(void)
{
    Telecmd_Argument arg;
    uint8_t          preview[TELECMD_PACKET_SIZE];
    size_t           n;

    g_sent_epoch = TEST_EPOCH_VAL;
    g_sent_ms    = platform_now_ms();

    arg.cmd = INF_CLOCK;
    memset(arg.payload, 0, sizeof(arg.payload));
    arg.payload[0] = (uint8_t)(g_sent_epoch >> 24);
    arg.payload[1] = (uint8_t)(g_sent_epoch >> 16);
    arg.payload[2] = (uint8_t)(g_sent_epoch >>  8);
    arg.payload[3] = (uint8_t)(g_sent_epoch & 0xFF);

    n = create_telecommand_packet(s_cmd_num, &arg, preview, sizeof(preview));
    platform_log("--- [INF_CLOCK] cmd_num=0x%04X  epoch=%lu ---\r\n",
                 s_cmd_num, (unsigned long)g_sent_epoch);
    print_hex("TX TC  : ", preview, n);
    tc_tx_start(&g_ctx, s_cmd_num, &arg);
    platform_log("INF_CLOCK 전송. 모듈에서 TC_Res(ACK)를 전송하세요.\r\n");
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

static void try_verify_beacon(void)
{
    beacon_rx_run(&g_bcn_rx);
    if (!beacon_rx_pkt_pending(&g_bcn_rx)) return;

    const Beacon_Packet_Frame* f        = beacon_rx_get_pkt(&g_bcn_rx);
    uint32_t                   elapsed_s = (platform_now_ms() - g_sent_ms) / 1000u;
    uint32_t                   expected  = g_sent_epoch + elapsed_s;
    uint32_t                   got_clock = f->argument.clock;
    uint32_t                   diff      = (got_clock >= expected) ? (got_clock - expected)
                                                                   : (expected - got_clock);
    platform_log("--- [CLOCK VERIFY] ---\r\n");
    platform_log("  expected : %lu\r\n", (unsigned long)expected);
    platform_log("  received : %lu\r\n", (unsigned long)got_clock);
    platform_log("  diff     : %lus\r\n", (unsigned long)diff);
    platform_log(diff <= CLOCK_TOLERANCE
                 ? "  >> [PASS] 클럭 동기화 검증 성공\r\n"
                 : "  >> [FAIL] 클럭 오차 허용범위 초과\r\n");

    beacon_rx_clear(&g_bcn_rx);
    g_phase = M2_NORMAL;
    s_cmd_num++;
    start_next_tc();
}

void setup(void)
{
    platform_init();
    platform_log("=== MODE 2: TC Sender + INF_CLOCK 검증 ===\r\n");
    platform_log("흐름: INF_CLOCK 전송 -> ACK -> 비콘 수신 -> clock 비교\r\n");
    platform_log("(비콘 미수신 시 %ums 후 자동 스킵)\r\n", VERIFY_WAIT_MS);

    tc_tx_init(&g_ctx, 1000, 2);
    beacon_rx_init(&g_bcn_rx);
    send_clock_tc();
}

void loop(void)
{
    static uint8_t prev_retry = 0xFF;

    if (g_phase == M2_CLOCK_WAIT) {
        try_verify_beacon();
        if (platform_now_ms() >= g_verify_until) {
            platform_log(">> [TIMEOUT] 비콘 미수신 → 검증 스킵\r\n");
            platform_log("   모듈 Serial Monitor에서 '[CLOCK] UTC epoch=%lu synced' 확인\r\n",
                         (unsigned long)g_sent_epoch);
            g_phase = M2_NORMAL;
            s_cmd_num++;
            start_next_tc();
        }
        return;
    }

    tc_tx_run(&g_ctx);

    if (g_ctx.retry != prev_retry && g_ctx.retry > 0) {
        prev_retry = g_ctx.retry;
        platform_log(">> timeout. 재전송 #%u\r\n", g_ctx.retry);
    }

    if (tc_tx_is_done(&g_ctx)) {
        if (g_phase == M2_CLOCK_SEND) {
            g_verify_until = platform_now_ms() + VERIFY_WAIT_MS;
            g_phase        = M2_CLOCK_WAIT;
            platform_log(">> [ACK] INF_CLOCK 수신 완료. %ums 내 비콘 대기 중...\r\n",
                         VERIFY_WAIT_MS);
            prev_retry = 0xFF;
            return;
        }
        platform_log(">> [PASS] ACK 수신. cmd_num=0x%04X 성공\r\n", s_cmd_num);
        s_cmd_num++;
        prev_retry = 0xFF;
        start_next_tc();
    }
    if (tc_tx_is_fail(&g_ctx)) {
        if (g_phase == M2_CLOCK_SEND) {
            platform_log(">> [FAIL] INF_CLOCK 전송 실패 → 검증 스킵\r\n");
            g_phase = M2_NORMAL;
        } else {
            platform_log(">> [FAIL] cmd_num=0x%04X 실패. 다음으로 진행\r\n", s_cmd_num);
        }
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

static Telecmd_RxContext g_rx_ctx;
static Beacon_TxContext  g_beacon_ctx;

static void print_hex(const char* label, const uint8_t* buf, size_t len)
{
    platform_log("%s", label);
    for (size_t i = 0; i < len; i++) platform_log("%02X ", buf[i]);
    platform_log("\r\n");
}

void setup(void)
{
    Telecmd_Argument arg;
    uint8_t          preview[TELECMD_PACKET_SIZE];
    size_t           n;

    platform_init();
    thruster_init();
    tc_rx_init(&g_rx_ctx);
    beacon_tx_init(&g_beacon_ctx, BEACON_INTERVAL_DEFAULT_MS);
    platform_log("=== MODE 3: TC Receiver ===\r\n");
    platform_log("INF_CLOCK / INF_RESET: 라이브러리 자동 처리 (ACK/NACK 자동 송신)\r\n");

    /* INF_CLOCK 테스트 패킷 (cmd_num=0x0001) */
    arg.cmd = INF_CLOCK;
    memset(arg.payload, 0, sizeof(arg.payload));
    arg.payload[0] = (uint8_t)(TEST_EPOCH_VAL >> 24);
    arg.payload[1] = (uint8_t)(TEST_EPOCH_VAL >> 16);
    arg.payload[2] = (uint8_t)(TEST_EPOCH_VAL >>  8);
    arg.payload[3] = (uint8_t)(TEST_EPOCH_VAL & 0xFF);
    n = create_telecommand_packet(0x0001, &arg, preview, sizeof(preview));
    platform_log("--- [INF_CLOCK] cmd_num=0x0001 epoch=%lu ---\r\n",
                 (unsigned long)TEST_EPOCH_VAL);
    print_hex("RealTerm 전송 바이트: ", preview, n);

    /* INF_RESET 테스트 패킷 (cmd_num=0x0002, payload 모두 0xFF → 리셋) */
    arg.cmd = INF_RESET;
    memset(arg.payload, 0xFF, sizeof(arg.payload));
    n = create_telecommand_packet(0x0002, &arg, preview, sizeof(preview));
    platform_log("--- [INF_RESET] cmd_num=0x0002 (payload=FF*6) ---\r\n");
    print_hex("RealTerm 전송 바이트: ", preview, n);

    /* INF_SETTING 테스트 패킷 (cmd_num=0x0003, 비콘 주기 10s) */
    arg.cmd        = INF_SETTING;
    arg.payload[0] = 0x0A;                   /* 10s */
    arg.payload[1] = BEACON_SETTING_INTERVAL; /* 비콘 주기 변경 */
    memset(arg.payload + 2, 0, 4);
    n = create_telecommand_packet(0x0003, &arg, preview, sizeof(preview));
    platform_log("--- [INF_SETTING] cmd_num=0x0003 (비콘 주기 10s) ---\r\n");
    print_hex("RealTerm 전송 바이트: ", preview, n);

    /* INF_REQUEST 테스트 패킷 (cmd_num=0x0004) */
    arg.cmd = INF_REQUEST;
    memset(arg.payload, 0, sizeof(arg.payload));
    n = create_telecommand_packet(0x0004, &arg, preview, sizeof(preview));
    platform_log("--- [INF_REQUEST] cmd_num=0x0004 ---\r\n");
    print_hex("RealTerm 전송 바이트: ", preview, n);
    platform_log("TC 수신 대기 중...\r\n");
}

void loop(void)
{
    /* 클럭 동기화 후 1초마다 현재 epoch 출력 */
    if (time_is_synced()) {
        static uint32_t last_log_ms = 0;
        uint32_t now_ms = platform_now_ms();
        if (now_ms - last_log_ms >= 1000) {
            last_log_ms = now_ms;
            platform_log("[CLOCK TICK] epoch=%lu\r\n",
                         (unsigned long)time_get_epoch());
        }
    }

    /* INF_CLOCK, INF_RESET은 tc_rx_run() 내부 자동 처리 */
    tc_rx_run(&g_rx_ctx);

    if (!tc_rx_pkt_pending(&g_rx_ctx)) return;

    const Telecmd_Packet_Frame* pkt     = tc_rx_get_pkt(&g_rx_ctx);
    uint16_t                    cmd_num = telecommand_get_cmd_num(pkt);
    platform_log("[PARSE OK] cmd_num=0x%04X  cmd=0x%02X\r\n",
                 cmd_num, pkt->argument.cmd);

    if (pkt->argument.cmd == INF_SETTING) {
        bool ok = beacon_tx_apply_setting(&g_beacon_ctx, pkt->argument.payload);
        tc_rx_respond(&g_rx_ctx, ok ? STATUS_ACK : STATUS_NACK);
        platform_log(ok ? "[PASS] 비콘 주기 변경 완료\r\n" : "[FAIL] 비콘 주기 변경 실패\r\n");
    } else if (pkt->argument.cmd == INF_CONTROL) {
        bool ok = thruster_handle_control(pkt->argument.payload);
        tc_rx_respond(&g_rx_ctx, ok ? STATUS_ACK : STATUS_NACK);
        platform_log(ok ? "[PASS] 추력기 제어 완료\r\n" : "[FAIL] 추력기 제어 실패\r\n");
    } else {
        tc_rx_respond(&g_rx_ctx, STATUS_ACK);
        platform_log("[PASS] TC 수신 및 TC_Res(ACK) 송신 완료\r\n");
    }

    /* 다음 TC 바이트 미리 출력 */
    {
        Telecmd_Argument next_arg;
        uint8_t          next_preview[TELECMD_PACKET_SIZE];
        size_t           next_n;
        next_arg.cmd = INF_REQUEST;
        memset(next_arg.payload, 0, sizeof(next_arg.payload));
        next_n = create_telecommand_packet((uint16_t)(cmd_num + 1), &next_arg,
                                           next_preview, sizeof(next_preview));
        platform_log("--- 다음 cmd_num=0x%04X ---\r\n", (unsigned)(cmd_num + 1));
        print_hex("다음 TC 바이트: ", next_preview, next_n);
    }
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

static Telecmd_RxContext g_rx_ctx;

static void print_hex(const char* label, const uint8_t* buf, size_t len)
{
    platform_log("%s", label);
    for (size_t i = 0; i < len; i++) platform_log("%02X ", buf[i]);
    platform_log("\r\n");
}

void setup(void)
{
    Telecmd_Argument arg;
    uint8_t          preview[TELECMD_PACKET_SIZE];
    size_t           n;

    platform_init();
    thruster_init();
    tc_rx_init(&g_rx_ctx);
    platform_log("=== MODE 4: Thruster Control Verify ===\r\n");
    platform_log("INF_CLOCK / INF_RESET: 라이브러리 자동 처리\r\n");

    /* 예시 INF_CONTROL 패킷 출력 (T1~T6 = 5,10,15,5,10,15 mA) */
    arg.cmd        = INF_CONTROL;
    arg.payload[0] =  5;
    arg.payload[1] = 10;
    arg.payload[2] = 15;
    arg.payload[3] =  5;
    arg.payload[4] = 10;
    arg.payload[5] = 15;
    n = create_telecommand_packet(0x0001, &arg, preview, sizeof(preview));
    platform_log("--- INF_CONTROL 예시 패킷 (cmd_num=0x0001) ---\r\n");
    print_hex("RealTerm 전송 바이트: ", preview, n);
    platform_log("payload: T1=%umA T2=%umA T3=%umA T4=%umA T5=%umA T6=%umA\r\n",
                 arg.payload[0], arg.payload[1], arg.payload[2],
                 arg.payload[3], arg.payload[4], arg.payload[5]);
    platform_log("TC 수신 대기 중...\r\n");
}

void loop(void)
{
    /* INF_CLOCK, INF_RESET은 tc_rx_run() 내부 자동 처리 */
    tc_rx_run(&g_rx_ctx);

    if (!tc_rx_pkt_pending(&g_rx_ctx)) return;

    const Telecmd_Packet_Frame* pkt     = tc_rx_get_pkt(&g_rx_ctx);
    uint16_t                    cmd_num = telecommand_get_cmd_num(pkt);
    platform_log("[PARSE OK] cmd_num=0x%04X  cmd=0x%02X\r\n",
                 cmd_num, pkt->argument.cmd);

    if (pkt->argument.cmd == INF_CONTROL) {
        bool ok = thruster_handle_control(pkt->argument.payload);
        tc_rx_respond(&g_rx_ctx, ok ? STATUS_ACK : STATUS_NACK);
        platform_log(ok ? "[PASS] 추력기 제어 완료\r\n" : "[FAIL] 추력기 제어 실패\r\n");
    } else {
        tc_rx_respond(&g_rx_ctx, STATUS_ACK);
        platform_log("[INFO] cmd=0x%02X → ACK 송신\r\n", pkt->argument.cmd);
    }
}

#endif
