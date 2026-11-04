/* config.h - which display this build is for (the same set of screens as OWLOS WiFi Radar).
 * pick one here, or pass it from the build: BUILD.BAT <display> does -DRADAR_DISPLAY=...
 * pins and drivers for each one are in firmware/main/board.h */
#pragma once

#define DISPLAY_ILI9341_320x240  1   // ESP32-2432S028R "cheap yellow display", every revision (v1 v2 v3: board.h)
#define DISPLAY_ST7796_480x320   2   // 3.5" ST7796 on classic ESP32 (ViewOwl 480x320)
#define DISPLAY_ILI9486_480x320  3   // Waveshare RPi LCD (A) 3.5" on OWLOSAirQuality board
#define DISPLAY_GC9A01_240x240   4   // round 1.28", ESP32-C3
#define DISPLAY_EPD_400x300      5   // e-paper 4.2" SSD1683, ESP32-S3
#define DISPLAY_EPD_792x272      6   // wide e-paper 5.79", two SSD1683, ESP32-S3
#define DISPLAY_ES3C28P_320x240  7   // LCDWIKI ES3C28P 2.8": ESP32-S3, ILI9341V, FT6336 touch
#define DISPLAY_NMCYDC5_320x240  8   // NM-CYD-C5 2.8": ESP32-C5, ST7789, XPT2046 touch
#define DISPLAY_CROWADV28_320x240 9  // Elecrow CrowPanel Advance 2.8": ESP32-S3, ST7789 IPS, FT6336 touch

#ifndef RADAR_DISPLAY
#define RADAR_DISPLAY DISPLAY_ILI9341_320x240
#endif

#if RADAR_DISPLAY == DISPLAY_ILI9341_320x240 || RADAR_DISPLAY == DISPLAY_ES3C28P_320x240 || \
    RADAR_DISPLAY == DISPLAY_NMCYDC5_320x240 || RADAR_DISPLAY == DISPLAY_CROWADV28_320x240
#define GFX_W 320
#define GFX_H 240
#elif RADAR_DISPLAY == DISPLAY_ST7796_480x320 || RADAR_DISPLAY == DISPLAY_ILI9486_480x320
#define GFX_W 480
#define GFX_H 320
#elif RADAR_DISPLAY == DISPLAY_GC9A01_240x240
#define GFX_W 240
#define GFX_H 240
#define RADAR_ROUND 1                // glass is a circle, nothing may sit in the corners
#elif RADAR_DISPLAY == DISPLAY_EPD_400x300
#define GFX_W 400
#define GFX_H 300
#define RADAR_EPAPER 1               // slow refresh, 4 greys, no animation
#elif RADAR_DISPLAY == DISPLAY_EPD_792x272
#define GFX_W 792
#define GFX_H 272
#define RADAR_EPAPER 1
#else
#error "unknown RADAR_DISPLAY, see config.h"
#endif

#ifndef RADAR_ROUND
#define RADAR_ROUND 0
#endif
#ifndef RADAR_EPAPER
#define RADAR_EPAPER 0
#endif

/* boards with touch. all of them go thru the 5 cross alignment once: the resistive ones
 * need it anyway, and on the round one it finds out how the touch chip's axes sit */
#define RADAR_TOUCH_CAL (RADAR_DISPLAY == DISPLAY_ILI9341_320x240 || RADAR_DISPLAY == DISPLAY_ST7796_480x320 || \
                         RADAR_DISPLAY == DISPLAY_GC9A01_240x240 || RADAR_DISPLAY == DISPLAY_ES3C28P_320x240 || \
                         RADAR_DISPLAY == DISPLAY_NMCYDC5_320x240 || RADAR_DISPLAY == DISPLAY_CROWADV28_320x240)

/* the RPi LCD has no touch: BOOT does what it can there, the site sends the rest */
#define RADAR_HAS_TOUCH RADAR_TOUCH_CAL

/* the RGB LED: on three pins on the back of the 2.8" and the 3.5" CYD, a WS2812 on one pin on the
 * ES3C28P and the NM-CYD-C5 (firmware/main/board.h has the pins) */
#define RADAR_HAS_LED (RADAR_DISPLAY == DISPLAY_ILI9341_320x240 || RADAR_DISPLAY == DISPLAY_ST7796_480x320 || \
                       RADAR_DISPLAY == DISPLAY_ES3C28P_320x240 || RADAR_DISPLAY == DISPLAY_NMCYDC5_320x240)
/* the CYDs' red is on one of two pins and nothing tells which: the user picks (core/light.h) */
#define RADAR_LED_RED_PIN (RADAR_DISPLAY == DISPLAY_ILI9341_320x240 || RADAR_DISPLAY == DISPLAY_ST7796_480x320)

#define RADAR_NAME "OWLOS FLIGHTS"

/* the board, as ABOUT says it */
#if RADAR_DISPLAY == DISPLAY_ILI9341_320x240
#define RADAR_BOARD "2.8\" CYD, 320x240"
#elif RADAR_DISPLAY == DISPLAY_ST7796_480x320
#define RADAR_BOARD "3.5\" CYD, ST7796 480x320"
#elif RADAR_DISPLAY == DISPLAY_ILI9486_480x320
#define RADAR_BOARD "3.5\" RPI LCD, ILI9486 480x320"
#elif RADAR_DISPLAY == DISPLAY_GC9A01_240x240
#define RADAR_BOARD "1.28\" ROUND, GC9A01"
#elif RADAR_DISPLAY == DISPLAY_ES3C28P_320x240
#define RADAR_BOARD "2.8\" ES3C28P, ESP32-S3"
#elif RADAR_DISPLAY == DISPLAY_NMCYDC5_320x240
#define RADAR_BOARD "2.8\" NM-CYD-C5, ESP32-C5"
#elif RADAR_DISPLAY == DISPLAY_CROWADV28_320x240
#define RADAR_BOARD "2.8\" CrowPanel Advance, S3"
#else
#define RADAR_BOARD "E-PAPER"
#endif
