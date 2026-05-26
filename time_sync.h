#ifndef TIME_SYNC_H
#define TIME_SYNC_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

void     time_sync_apply(uint32_t epoch_s);
uint32_t time_get_epoch(void);
bool     time_is_synced(void);

#ifdef __cplusplus
}
#endif

#endif  /* TIME_SYNC_H */
