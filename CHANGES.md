# Versions

What changed in each version of OWLOS Flight Radar, the newest first. The first `## ` line below is the
version: the firmware is built with it, the site publishes it, and a board compares its own with it.
The site keeps the firmware of each version: a new number here keeps the one before it as it was.

## 0.4

- Three more boards, each asked for by people who have one, none run here yet: the LCDWIKI ES3C28P
  (ESP32-S3, ILI9341V, capacitive touch), the NM-CYD-C5 (ESP32-C5, ST7789, resistive touch) and
  Elecrow's CrowPanel Advance 2.8" (ESP32-S3, ST7789 IPS, capacitive touch). Tell us how it goes.
- The site talks to a board over the chip's own USB too, not only through a CH340: the ES3C28P has no
  other.
- The WS2812 LED on the ES3C28P and the NM-CYD-C5 lights for the same things as the CYDs' RGB one.
- The site keeps every version from 0.2 on: step 1 can flash an older one, for a board the newest went
  wrong on.

## 0.3

- Planes on the ground can be left out: SET, DATA, ON GROUND, or the site. Next to an airport the parked
  ones no longer fill the list.
- Home is set on the board itself: SET, DATA, HOME, then a town, a postcode or a point typed on the
  screen, and the board looks it up.
- A watch list of up to 8 planes, by callsign, registration or hex address. The board says when one
  comes in range and picks it, and when one takes off or lands anywhere. WATCH on a picked plane's panel
  puts it on the list or takes it off.
- The RGB LED on the back of the 2.8" and 3.5" CYD lights for what you pick: an emergency squawk round
  home, a watched plane in range, taking off or landing, any plane near, no data. How bright, for how
  long, and which pin the red one is on (GPIO 4, or 22 on LCDWIKI's and Elegoo's ESP32-32E boards).
- SET is in tabs: VIEW, DATA, LED.
- ABOUT, in SET's DATA tab: the version, whether the site has a newer one, and a QR code to the site.
  When the site has a newer one, a small note on the main screen says so and opens ABOUT.
- The site shows this history, the newest version, and the version the board runs.
- LCDWIKI's ESP32-32E boards, also sold by Elegoo, take the 2.8" and 3.5" CYD builds.
- Fixed: a setting the site sent the moment the board said it was ready could be lost.

## 0.2

- The first public version. A radar scope over the map, a plain square map, or the map in
  OpenStreetMap's own colours. Planes from the free community ADS-B networks (adsb.lol, adsb.fi) or
  from Flightradar24 with a key. Turn and zoom with a finger, the nearest planes in a panel, every flight
  in a list, five colour themes, a demo sky without WiFi, the WiFi set on the screen or from the site.
- Boards: the 2.8" CYD (all three of its screens), the 3.5" CYD, the 1.28" round Elecrow CrowPanel and
  the 3.5" RPi LCD on OWLOSAirQuality. Flashed and set up from the site.
