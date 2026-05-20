#ifndef PLATFORM_H
#define PLATFORM_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

bool     platform_init(void);               // opens Serial + Serial1 at 9600
uint32_t platform_now_ms(void);             // tracking and TC TX timeout deadlines
void     platform_delay_ms(uint32_t ms);

bool     platform_uart_init(uint32_t baudrate);
int      platform_uart_available(void);
int      platform_uart_read(uint8_t* buf, size_t maxlen);
int      platform_uart_write(const uint8_t* buf, size_t len);

void     platform_log(const char* fmt, ...);

#ifdef __cplusplus
}
#endif

#endif  /* PLATFORM_H */
