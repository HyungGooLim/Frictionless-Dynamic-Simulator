#include "platform.h"
#include "user.h"

/* ================================================================ */
#if defined(PLATFORM_ARDUINO)
/* ================================================================ */

#include <Arduino.h>
#include <stdarg.h>

static uint32_t g_baudrate = 9600;

bool platform_init(void)
{
    Serial.begin(9600);
    return platform_uart_init(g_baudrate);
}

uint32_t platform_now_ms(void)
{
    return (uint32_t)millis();
}

void platform_delay_ms(uint32_t ms)
{
    delay(ms);
}

bool platform_uart_init(uint32_t baudrate)
{
    g_baudrate = baudrate;
    Serial1.begin(baudrate);
    return true;
}

int platform_uart_available(void)
{
    return Serial1.available();
}

int platform_uart_read(uint8_t* buf, size_t maxlen)
{
    if (!buf || maxlen == 0) return 0;
    size_t n = 0;
    while (Serial1.available() > 0 && n < maxlen) {
        buf[n++] = (uint8_t)Serial1.read();
    }
    return (int)n;
}

int platform_uart_write(const uint8_t* buf, size_t len)
{
    if (!buf || len == 0) return 0;
    return (int)Serial1.write(buf, len);
}

void platform_log(const char* fmt, ...)
{
    char    tmp[128];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(tmp, sizeof(tmp), fmt, ap);
    va_end(ap);
    Serial.print(tmp);
}

/* ================================================================ */
#elif defined(PLATFORM_WSL)
/* ================================================================ */

#include <stdio.h>
#include <stdarg.h>
#include <time.h>
#include <unistd.h>

static uint32_t g_baudrate = 9600;

bool platform_init(void)           { return true; }

uint32_t platform_now_ms(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint32_t)(ts.tv_sec * 1000u + ts.tv_nsec / 1000000u);
}

void platform_delay_ms(uint32_t ms) { usleep(ms * 1000u); }

bool platform_uart_init(uint32_t baudrate)
{
    g_baudrate = baudrate;
    (void)g_baudrate;
    return true;
}

int platform_uart_available(void)                        { return 0; }
int platform_uart_read(uint8_t* buf, size_t maxlen)      { (void)buf; (void)maxlen; return 0; }

int platform_uart_write(const uint8_t* buf, size_t len)
{
    if (!buf || len == 0) return 0;
    printf("[UART TX] ");
    for (size_t i = 0; i < len; i++) printf("%02X ", buf[i]);
    printf("\n");
    return (int)len;
}

void platform_log(const char* fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    vprintf(fmt, ap);
    va_end(ap);
}

/* ================================================================ */
#else
#error "플랫폼 미선택: user.h에서 PLATFORM_ARDUINO 또는 PLATFORM_WSL을 정의하세요."
#endif
