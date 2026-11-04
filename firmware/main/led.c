/* the RGB LED on the back of the CYD: three LEDC channels on a timer of their own, the backlight's
 * (lcd_spi.c, timer 0 and channel 0) is left alone. each colour is pulled to ground to light, so
 * the outputs are inverted: duty 0 is dark. a WS2812 instead on the boards that have one (board.h
 * PIN_LED_WS2812). what it shows is core/light.c's */
#include "led.h"
#include "board.h"

#if RADAR_HAS_LED && defined(PIN_LED_WS2812)
/* one WS2812 on one pin: 24 bits, green red blue, the highest first. a bit is a high and a low,
 * 0.3 + 0.9 us a 0 and 0.9 + 0.3 us a 1, sent by the RMT. 50 us low and it takes them: the screen
 * task changes it at most once a frame, far more apart than that */
#include <string.h>
#include "driver/rmt_tx.h"
#include "esp_log.h"
#include "soc/soc_caps.h"

static const char *TAG = "led";
static rmt_channel_handle_t s_ch;
static rmt_encoder_handle_t s_bits;
static uint8_t s_now[3];
static uint8_t s_grb[3];                               // the RMT reads it after rmt_transmit is back
static int s_ready;

void led_init(int red_gpio)
{
    (void)red_gpio;                                    // one pin for all three colours here
    rmt_tx_channel_config_t ch = {
        .gpio_num = PIN_LED_WS2812,
        .clk_src = RMT_CLK_SRC_DEFAULT,
        .resolution_hz = 10 * 1000 * 1000,             // a tick is 0.1 us
        .mem_block_symbols = SOC_RMT_MEM_WORDS_PER_CHANNEL,   // 24 bits fit
        .trans_queue_depth = 1,
    };
    rmt_bytes_encoder_config_t bits = {
        .bit0 = { .level0 = 1, .duration0 = 3, .level1 = 0, .duration1 = 9 },
        .bit1 = { .level0 = 1, .duration0 = 9, .level1 = 0, .duration1 = 3 },
        .flags.msb_first = 1,
    };
    esp_err_t e = rmt_new_tx_channel(&ch, &s_ch);
    if (e == ESP_OK) e = rmt_new_bytes_encoder(&bits, &s_bits);
    if (e == ESP_OK) e = rmt_enable(s_ch);
    if (e != ESP_OK) { ESP_LOGE(TAG, "WS2812 on GPIO %d: %s", PIN_LED_WS2812, esp_err_to_name(e)); return; }
    s_ready = 1;
    s_now[0] = 1;                                      // not what led_set gets first: dark goes out too
    const uint8_t dark[3] = { 0, 0, 0 };
    led_set(dark);
    ESP_LOGI(TAG, "WS2812 LED on GPIO %d", PIN_LED_WS2812);
}

void led_red(int gpio) { (void)gpio; }                 // a CYD setting, nothing to move here

void led_set(const uint8_t rgb[3])
{
    if (!s_ready || !memcmp(rgb, s_now, 3)) return;
    memcpy(s_now, rgb, 3);
    rmt_tx_wait_all_done(s_ch, 20);                    // the last one out first: s_grb is read while it goes
    s_grb[0] = rgb[1];
    s_grb[1] = rgb[0];
    s_grb[2] = rgb[2];
    const rmt_transmit_config_t once = { .loop_count = 0 };
    rmt_transmit(s_ch, s_bits, s_grb, sizeof s_grb, &once);
}

#elif RADAR_HAS_LED
#include "driver/gpio.h"
#include "driver/ledc.h"
#include "esp_log.h"

static const char *TAG = "led";
static int s_pin[3] = { -1, PIN_LED_G, PIN_LED_B };           // red's comes from the settings
static const ledc_channel_t CH[3] = { LEDC_CHANNEL_1, LEDC_CHANNEL_2, LEDC_CHANNEL_3 };
static uint8_t s_now[3];
static int s_ready;

static esp_err_t channel(int i)
{
    ledc_channel_config_t ch = {
        .gpio_num = s_pin[i],
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .channel = CH[i],
        .timer_sel = LEDC_TIMER_1,
        .duty = 0,
        .hpoint = 0,
        .flags.output_invert = 1,
    };
    s_now[i] = 0;
    return ledc_channel_config(&ch);
}

void led_init(int red_gpio)
{
    s_pin[0] = red_gpio;
    ledc_timer_config_t t = { .speed_mode = LEDC_LOW_SPEED_MODE, .duty_resolution = LEDC_TIMER_10_BIT,
                              .timer_num = LEDC_TIMER_1, .freq_hz = 5000, .clk_cfg = LEDC_AUTO_CLK };
    esp_err_t e = ledc_timer_config(&t);
    if (e != ESP_OK) { ESP_LOGE(TAG, "timer: %s", esp_err_to_name(e)); return; }
    for (int i = 0; i < 3; i++) {
        e = channel(i);
        if (e != ESP_OK) { ESP_LOGE(TAG, "GPIO %d: %s", s_pin[i], esp_err_to_name(e)); return; }
    }
    s_ready = 1;
    ESP_LOGI(TAG, "RGB LED on GPIO %d %d %d, lit by pulling them low", s_pin[0], s_pin[1], s_pin[2]);
}

void led_red(int gpio)
{
    if (!s_ready || gpio == s_pin[0]) return;
    gpio_reset_pin(s_pin[0]);                          // off the LEDC: an input with its pull-up, nothing driven
    s_pin[0] = gpio;
    esp_err_t e = channel(0);
    if (e != ESP_OK) ESP_LOGE(TAG, "GPIO %d: %s", gpio, esp_err_to_name(e));
    else ESP_LOGI(TAG, "red moved to GPIO %d", gpio);
}

void led_set(const uint8_t rgb[3])
{
    if (!s_ready) return;
    for (int i = 0; i < 3; i++) {
        if (rgb[i] == s_now[i]) continue;              // the same as now: nothing to write
        s_now[i] = rgb[i];
        ledc_set_duty(LEDC_LOW_SPEED_MODE, CH[i], (uint32_t)rgb[i] * 1023u / 255u);
        ledc_update_duty(LEDC_LOW_SPEED_MODE, CH[i]);
    }
}

#else
void led_init(int red_gpio) { (void)red_gpio; }
void led_red(int gpio) { (void)gpio; }
void led_set(const uint8_t rgb[3]) { (void)rgb; }
#endif
