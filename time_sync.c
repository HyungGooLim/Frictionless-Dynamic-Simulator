#include "time_sync.h"
#include "platform.h"

static bool     s_synced  = false;
static uint32_t s_epoch_s = 0;  /* Unix timestamp at sync moment */
static uint32_t s_sync_ms = 0;  /* platform_now_ms() at sync moment */

void time_sync_apply(uint32_t epoch_s)
{
    s_epoch_s = epoch_s;
    s_sync_ms = platform_now_ms();
    s_synced  = true;
}

uint32_t time_get_epoch(void)
{
    if (!s_synced) return platform_now_ms() / 1000u;
    return s_epoch_s + (platform_now_ms() - s_sync_ms) / 1000u;
}

bool time_is_synced(void)
{
    return s_synced;
}
