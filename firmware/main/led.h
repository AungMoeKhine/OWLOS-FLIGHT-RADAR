#pragma once
#include <stdint.h>

/* the RGB LED on the back (board.h). nothing on a board without one */
void led_init(int red_gpio);             // the red one's pin is the user's: core/light.h
void led_red(int gpio);                  // red moves to that pin, the old one is let go
void led_set(const uint8_t rgb[3]);      // 0..255 a colour
