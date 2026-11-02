#include "console.h"
#include <stdlib.h>
#include <string.h>
#include "driver/uart.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "console";
static console_line_fn s_fn;

static void console_task(void *arg)
{
    (void)arg;
    static char line[1024];
    int n = 0;
    for (;;) {
        uint8_t ch;
        if (uart_read_bytes(UART_NUM_0, &ch, 1, pdMS_TO_TICKS(1000)) != 1) continue;
        if (ch == '\r') continue;
        if (ch == '\n') {
            line[n] = 0;
            if (n && s_fn) s_fn(line);
            n = 0;
            continue;
        }
        if (n < (int)sizeof line - 1) line[n++] = (char)ch;
        else n = 0;                                    // too long for anything we know: drop it
    }
}

void console_start(console_line_fn fn)
{
    s_fn = fn;
    // the log keeps writing to UART0 as it always did, this only adds the receiving side
    if (uart_driver_install(UART_NUM_0, 2048, 0, 0, NULL, 0) != ESP_OK) {
        ESP_LOGE(TAG, "no UART driver: settings can't come from the site");
        return;
    }
    xTaskCreate(console_task, "console", 4096, NULL, 3, NULL);
}

// finds "key": in a flat object and leaves p after the colon
static const char *field(const char *json, const char *key)
{
    size_t kl = strlen(key);
    const char *p = json;
    while ((p = strchr(p, '"')) != NULL) {
        p++;
        if (!strncmp(p, key, kl) && p[kl] == '"') {
            p += kl + 1;
            while (*p == ' ') p++;
            if (*p != ':') continue;
            p++;
            while (*p == ' ') p++;
            return p;
        }
        while (*p && *p != '"') { if (*p == '\\' && p[1]) p++; p++; }   // skip the rest of this string
        if (*p) p++;
    }
    return NULL;
}

int json_str(const char *json, const char *key, char *out, int cap)
{
    const char *p = field(json, key);
    if (!p || *p != '"') return 0;
    p++;
    int n = 0;
    while (*p && *p != '"') {
        char ch = *p++;
        if (ch == '\\' && *p == 'u') {                 // č: a letter written as its number, back to UTF-8
            unsigned cp = (unsigned)strtoul((char[5]){ p[1], p[2], p[3], p[4], 0 }, NULL, 16);
            int k = 0;
            while (k < 5 && p[k]) k++;
            p += k;
            char u8[3];
            int len = cp < 0x80 ? 1 : cp < 0x800 ? 2 : 3;
            if (len == 1) u8[0] = (char)cp;
            else if (len == 2) { u8[0] = (char)(0xC0 | cp >> 6); u8[1] = (char)(0x80 | (cp & 63)); }
            else { u8[0] = (char)(0xE0 | cp >> 12); u8[1] = (char)(0x80 | (cp >> 6 & 63)); u8[2] = (char)(0x80 | (cp & 63)); }
            if (n + len < cap) { memcpy(out + n, u8, (size_t)len); n += len; }
            continue;
        }
        if (ch == '\\' && *p) {
            ch = *p++;
            if (ch == 'n') ch = '\n';
            else if (ch == 't') ch = '\t';
        }
        if (n < cap - 1) out[n++] = ch;
    }
    out[n] = 0;
    return 1;
}

int json_num(const char *json, const char *key, double *out)
{
    const char *p = field(json, key);
    if (!p || (*p != '-' && (*p < '0' || *p > '9'))) return 0;
    *out = strtod(p, NULL);
    return 1;
}
