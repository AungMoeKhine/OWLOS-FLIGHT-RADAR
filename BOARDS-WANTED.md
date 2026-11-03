# Boards people asked for

Asked for in the groups after the first posts. Most of them are not built yet: each needs its screen
and touch chips and their pins, and a board here to try or someone with one who tests a build.

## An ESP32-C5 board, model not named

- Asked: will it work on the C5 board?
- The chip is no problem: ESP-IDF 5.5, which the firmware is built with, supports the ESP32-C5. It is a
  single core RISC-V like the C3, faster, with WiFi on 2.4 and 5 GHz.
- Missing: which board, which screen and touch.

## NM-CYD-C5

- A 2.8" board in the CYD's form on the ESP32-C5, dual band WiFi 6.
- From its owner: ESP32-C5-WROOM-1 with 16 MB flash and 8 MB PSRAM, a 320x240 LCD, a touch screen, two
  USB-C ports, one the C5's own USB, one through a USB to UART chip.
- Others' words, not checked: a shop listing names the screen ST7789 and the touch resistive; a CNX
  Software note says resistive too. If so the touch is likely an XPT2046 as on the CYD, and the ST7789 is
  the 2.8" CYD v3's screen, already one of its variants.
- Its maker's repository is named RockBase-iot/NM-CYD-C5 on GitHub; it did not open when looked at, and
  no pin list was found anywhere.
- Missing: the pins of the screen, the touch, the backlight, and the LED if there is one.
- Needs: the esp32c5 target, these pins, its own card on the site. The 8 MB PSRAM gives room the
  classic ESP32 does not have: taller strips, the map's picture off the heap.
- The one who asked has the board and offered to test a build.

## LCDWIKI ES3C28P

- In the asker's words: same form factor as the CYD, with more RAM. They flashed a basic ESP32-2432S
  from a Chromebook with no trouble, then this one could not be flashed.
- 2.8", ESP32-S3 with 16 MB flash and 8 MB PSRAM, ILI9341V 240x320 IPS, FT6336G capacitive touch.
- Pins, from https://www.lcdwiki.com/2.8inch_ESP32-S3_Display: LCD CS 10, DC 46, SCLK 12, MOSI 11,
  MISO 13, backlight 45, reset on the chip's own; touch on I2C SDA 16, SCL 15, INT 17, RST 18.
- Today the site turns it down: the 2.8" build is for the classic ESP32 only.
- Needs: the S3 target, these pins, an FT6336 driver (a simple I2C one), its own card on the site.
  Colours and orientation unknown without the board: BOOT in the first seconds could go through the
  variants, as on the 2.8" CYD.
- The one who asked was asked whether they would try a test build.

## Elegoo ESP32-32E 2.8" (LCDWIKI E32R28T)

- Asked: sold on Amazon as "ESP32 ILI9341", can it take the 2.8" ESP32-2432S028R build?
- It should, nothing to build: from https://www.lcdwiki.com/2.8inch_ESP32-32E_Display the screen
  (ILI9341V, CS 15, DC 2, SCLK 14, MOSI 13, backlight 21, reset on EN) and the touch (XPT2046 on its own
  pins, CLK 25, MOSI 32, MISO 39, CS 33, IRQ 36) are on exactly the 2.8" CYD's pins. Colours and
  mirroring: BOOT in the first seconds goes through the screen variants. Not run on one here.
- One difference: the RGB LED's red is on IO22, not IO4 (green 16, blue 17 the same). IO4 on this board
  enables the speaker's amplifier, low is on. Now a setting: RED ON in SET's LED tab, `led_red` on the
  site, GPIO 4 by default.
- LCDWIKI's 3.5" twin, E32R35T, is the same story: every pin as the 3.5" build, red on IO22. The 3.5"
  here turned out to be one: red lit with GPIO 22 alone held low (2026-11-03).
