/* the RGB LED on the back of the CYD: three LEDC channels on a timer of their own, the backlight's
 * (lcd_spi.c, timer 0 and channel 0) is left alone. each colour is pulled to ground to light, so
 * the outputs are inverted: duty 0 is dark. what it shows is core/light.c's */
#include "led.h"
#include "board.h"

#if RADAR_HAS_LED
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
