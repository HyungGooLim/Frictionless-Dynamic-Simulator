#ifndef TC_HANDLER_H
#define TC_HANDLER_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "telecommand.h"
#include "telecommand_res.h"

typedef enum TcTxState {
    TC_TX_IDLE     = 0,
    TC_TX_SEND     = 1,
    TC_TX_WAIT_RES = 2,
    TC_TX_DONE     = 3,
    TC_TX_FAIL     = 4
} TcTxState;

typedef struct Telecmd_TxContext {
    TcTxState                state;

    Telecmd_Packet_Frame     req_pkt;
    uint8_t                  tx_pkt[TELECMD_PACKET_SIZE];
    size_t                   tx_len;

    uint32_t                 deadline_ms;
    uint32_t                 timeout_ms;

    uint8_t                  retry;
    uint8_t                  max_retry;

    uint8_t                  last_status;
    Telecmd_Res_Packet_Frame last_res_pkt;
} Telecmd_TxContext;

#ifdef __cplusplus
extern "C" {
#endif

// 
void tc_tx_init(Telecmd_TxContext* c, uint32_t timeout_ms, uint8_t max_retry);
void tc_tx_reset(Telecmd_TxContext* c);

bool tc_tx_start(Telecmd_TxContext* c,
                 uint16_t cmd_num,
                 const Telecmd_Argument* argument);

bool           tc_tx_need_send(const Telecmd_TxContext* c);
const uint8_t* tc_tx_data(const Telecmd_TxContext* c);
size_t         tc_tx_size(const Telecmd_TxContext* c);

bool tc_tx_mark_sent(Telecmd_TxContext* c, uint32_t now_ms);
bool tc_tx_on_response(Telecmd_TxContext* c,
                       const uint8_t* buf,
                       size_t len,
                       uint32_t now_ms);
void tc_tx_step(Telecmd_TxContext* c, uint32_t now_ms);

bool tc_tx_is_idle(const Telecmd_TxContext* c);
bool tc_tx_is_done(const Telecmd_TxContext* c);
bool tc_tx_is_fail(const Telecmd_TxContext* c);
bool tc_tx_is_waiting(const Telecmd_TxContext* c);

void tc_tx_run(Telecmd_TxContext* c);

#ifdef __cplusplus
}
#endif

#endif  /* TC_HANDLER_H */
