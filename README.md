# OWLOS Flight Radar

![The 3.5 inch board: the planes around home, a tap on one, the map turned and zoomed, the three looks](shots/st7796/tour.gif)

**Flash it from the browser: [kirindenis.github.io/OWLOS-FLIGHT-RADAR](https://kirindenis.github.io/OWLOS-FLIGHT-RADAR/)**

The planes around your home on a small ESP32 board with a screen. Each plane is a silhouette turned the
way it flies, coloured by its height, with a trail behind it. Tap one for its callsign, type, height and
speed. The planes come from the free community ADS-B networks, or from Flightradar24 if you have a key.
The map under them comes from OpenStreetMap.

## What it does

- A radar scope with a sweeping beam over the map, or a plain square map, or the same map in
  OpenStreetMap's own colours. One button switches between the three.
- Real distances and bearings round your home. Drag the rim to turn the map the way the screen stands,
  drag inside to zoom from 2 to 250 km (or nautical miles, or miles).
- The nearest planes in a panel, every flight in a list, the picked one with all it is known about it.
- Five colour themes, the colour of a plane always says its height.
- Next to an airport the planes parked and taxiing there can be hidden: SET, ON GROUND.
- A watch list of up to 8 planes: the board picks one when it comes in range, and says when one takes
  off or lands anywhere in the world.
- The RGB LED on the back of the 2.8" and 3.5" CYD lights for what happens: an emergency squawk round
  home, a watched plane coming, taking off or landing, no data. Each one on or off.
- Without WiFi it flies a made-up demo sky and says DEMO in big letters. The WiFi and the home are set
  on the touch screen itself, or from the site over the USB cable.
- It knows its version and finds out by itself when the site has a newer one: a small note on the
  screen opens ABOUT, with a QR code to the site. What changed in each version: [CHANGES.md](CHANGES.md).
- The map pictures are downloaded once and kept in the board's flash: a range it has shown comes back
  at once, even without WiFi.

## The boards

- **2.8" CYD** ("cheap yellow display", ESP32-2432S028R): ESP32, 320x240, resistive touch. Sold with three
  different screens, one build fits them all (see below).
- **3.5" CYD** (ESP32-3248S035, R or C version): ESP32, ST7796 480x320, resistive or capacitive touch. The
  second core draws half the radar.
- LCDWIKI's ESP32-32E boards, also sold by Elegoo: the 2.8" E32R28T takes the 2.8" CYD build and the
  3.5" E32R35T the 3.5" one, the screen and the touch are on the same pins. Only the LED's red is on
  another pin, see The LED.
- **1.28" round** (Elecrow CrowPanel): ESP32-C3, GC9A01 240x240, capacitive touch.
- **3.5" RPi LCD** (Waveshare RPi LCD (A) on the OWLOSAirQuality board): ESP32, ILI9486 480x320, no touch.

## Flash it

1. Open [the site](https://kirindenis.github.io/OWLOS-FLIGHT-RADAR/) in Chrome or Edge on a computer,
   a Chromebook too. Flashing goes over Web Serial: phones and Firefox can't do it.
2. Plug the board in with a USB cable that carries data.
3. Press CONNECT & FLASH under your board, pick its port and say yes to erase.
4. No port in the list? On Windows the CH340 driver may be missing. Won't connect? Hold BOOT while you
   plug it in.
5. Boards with touch ask once for 5 crosses to be pressed, so the touch knows where the screen is.
6. An update is the same button, with no to erase: the board keeps its WiFi, home, watch list and the
   rest.

## WiFi and home

- On the 2.8" and 3.5" CYD: tap **SET UP WIFI** on the demo's card (or SET, the WIFI row), tap your
  network, type its password, JOIN. The home is typed there too: SET, the HOME row.
- From the site, step 2: the WiFi, your home point, where the planes come from and how the screen shows,
  sent over the same USB cable. The round board and the RPi LCD take their WiFi this way.
- Till it gets a home the board sits in Trenčín, Slovakia. The plane network only hears the home rounded
  to about a kilometre: enough to say where to look, not where you live. A town or a postcode typed on
  the board goes to OpenStreetMap's Nominatim to be found.
- The password and the Flightradar24 key go into the board and stay there. The board never shows them
  and never sends them back.

## Where to tap

The pictures are the 3.5" board, the 2.8" one has the same screens a little smaller.

### The radar

![The radar look with arrows to where to tap](shots/guide/radar.png)

- **Tap a plane** for its details in the panel. A row of the panel's list picks the same plane.
- **Drag the rim** to turn the map: match it to the way the screen stands. Back near north it snaps to
  north up.
- **Drag inside** to zoom: away from home in, toward home out. BOOT steps through 5, 10, 25, 50, 100 and 200.
- **The look**, top right, goes round RADAR, MAP and CLASSIC.
- **HUE** changes the colours, **LIST** shows every flight, **SET** has the settings.
- **Colour is height**: grey on the ground, warm under 10 000 ft, the theme's colour up to 25 000, white
  above. Emergency squawks blink.

### A plane picked

![A plane picked, with arrows to its details, NEXT and X](shots/guide/plane.png)

- The panel says its callsign, type and registration, height, speed and track, how far it is and which
  way, its squawk. Flightradar24 adds where it comes from and where it goes.
- The line ahead of it is where it will be in a minute.
- **NEXT** picks the next nearest one, **X** lets it go.
- **WATCH** puts it on the watch list by its registration (lit while it is on it), again takes it off.

### The map

![The map look with arrows to the grid, the look, the three dots and the scale](shots/guide/map.png)

- MAP is a map over the whole screen, north up, home in the middle. CLASSIC is the same map in
  OpenStreetMap's own colours.
- The grid's lines are whole degrees or minutes, named at the edges. The scale is in the corner.
- **The three dots** slide the panel in and out. A picked plane brings it by itself.
- **Drag** anywhere on the map to zoom.

### LIST

![The list of flights with arrows to a row, ZOOM and BACK](shots/guide/list.png)

- Every flight in range, the nearest first. Drag to scroll.
- **Tap a row** and that plane is picked on the scope. **ZOOM** steps the range, **BACK** goes back.

### SET

![The settings page with arrows to its buttons](shots/guide/setup.png)

- Three tabs on the bar: **VIEW** the range, units, map up, the screen upside down, labels, colours, the
  look, the map and the touch alignment; **DATA** planes on the ground shown or hidden, the watch list,
  where the planes come from, home, the WiFi and ABOUT; **LED** the light on the back (2.8" and 3.5" CYD).
- **Tap a button** to change it, **minus and plus** for numbers. Drag to see the rest.

### No WiFi yet: the demo

![The demo sky with DEMO in big letters and the SET UP WIFI button](shots/guide/demo.png)

- Without WiFi the board flies a made-up sky and says **DEMO** in big letters. Under it why: no WiFi
  set up yet, the one set can't be seen, or its password looks wrong.
- **SET UP WIFI** opens the networks.

### The networks

![The WiFi networks the board hears, with arrows](shots/guide/wifi.png)

- The networks the board hears, the strongest first. A lock asks for a password, OPEN does not, SET is
  the one in use.
- **Tap your network**. **LOOK AGAIN** looks round once more. The ESP32 hears 2.4 GHz networks only.

### The password

![The keyboard for the WiFi password, with arrows](shots/guide/keys.png)

- Dots for what you typed, the last letter shows for a moment. **SHOW** shows all of it.
- **ABC** gives one capital, **#+=** the signs, **DEL** rubs out, held it goes on.
- **JOIN**: the board joins the network and the real planes come in a few seconds.

### Home

![The keyboard for a new home, with arrows](shots/guide/home.png)

- SET, the HOME row, **CHANGE**: type a town, a postcode, or the point as `48.9 18.1`. **FIND** asks
  OpenStreetMap where it is, a point needs no WiFi. The planes come round the new home at once.
- A postcode is looked for first in the country of the home the board has, then anywhere. One from
  another country goes with its name: `10115, Germany`. OpenStreetMap does not know every full postcode,
  the town's name always works.
- **#+=** has the space. The round board and the RPi LCD take their home from the site.

### Watch

![The keyboard for the planes to follow, with arrows](shots/guide/watch.png)

- SET, DATA, the WATCH row (or WATCH on a picked plane's panel): up to 8 planes, by commas. A
  callsign as the plane sends it (EZY12AB, BAW123, not the flight number on a ticket), a registration
  with its dash (G-EUPT), or the hex address (4CA7B4).
  Letters come out as capitals by themselves, **#+=** has the comma and the dash. **SAVE**, empty: nothing
  watched.
- One comes in range: **EZY12AB IN RANGE**, and the board picks it on the scope (unless you picked
  another one). The others it asks the free networks about, one every 15 seconds: **EZY12AB IS FLYING**
  when one takes off anywhere, **LANDED** when it is down. The WATCH row says how many fly and how many
  are in range.
- The round board and the RPi LCD take their list from the site, step 2.

### The LED

- The 2.8" and 3.5" CYD have an RGB LED on the back. SET's LED tab has a row for each thing that lights
  it, ON or OFF: **SOS** red and blinking fast, an emergency squawk (7500, 7600, 7700) round home; **IN
  RANGE** green, a watched plane comes; **TAKE-OFF** blue; **LANDING** cyan; **ANY NEAR** white, any plane
  in the air within **HOW NEAR** of home (1, 2, 5 or 10 km, or nautical miles, or miles); **NO DATA**
  amber and steady, the WiFi set is not there or no answer. When two are lit, the first in that order
  shows.
- **BRIGHTNESS**: low, mid or high. **HOW LONG**: till the screen is touched (a minute at most),
  10 seconds, or while it lasts. A touch on the screen puts it out. **TEST**: red, green, blue in turn.
- **RED ON**: GPIO 4 or GPIO 22. The red one is on GPIO 4 on the CYD and on GPIO 22 on LCDWIKI's (and
  Elegoo's) ESP32-32E boards, and nothing on the board tells which. No red in the test: tap it, the test
  runs again on the other pin.
- The site's step 2 has a few ready sets of them, the brightness, how long and the red one's pin.

### ABOUT and a new version

![ABOUT with arrows: this version, what the site has, how to update, the code to scan, the address, BACK](shots/guide/about.png)

- SET, DATA, **ABOUT**: the version the board runs, the one the site has, the board, whether an FR24
  key is stored, and a QR code to the site with its address under it.
- A board on WiFi asks the site which version it has once it is up, then twice a day (`version.json`
  next to the page). When the site's is newer, a small **NEW VERSION** note shows: in the panel's title
  over the radar, under the town's card over the map, under home on the round glass. A tap on it opens
  ABOUT. Nothing else changes.
- To update: the site on a computer, CONNECT & FLASH, no to erase.

![The radar with the NEW VERSION note in the panel's title](shots/guide/news.png)

### The 2.8" CYD: which screen

![The 2.8 inch board in its first seconds, the card that says to press BOOT](shots/guide/screen.png)

- It is sold with three different screens. It starts as the USB-C one (v2), the one most boards have.
  Wrong colours or a mirrored picture: press **BOOT** in the first seconds after power on. It goes v2,
  v1 (micro USB), v3 (ST7789) and keeps the one you stop at.

### The round glass

![The round glass with arrows to the menu, the rim and a plane](shots/guide/round.png)

- The whole glass is the scope. **Tap an empty spot** for HUE, the look, LIST and SET.
- **Tap a plane** for its card under home, tap the card for the next one.
- The rim turns the map and inside zooms, like on the square boards.

### And

- **Hold BOOT** to go back to the scope from any page.
- **Hold a finger 4 s** on an empty spot to align the touch again.
- **The RPi LCD** has no touch: BOOT steps through the planes, held it changes the range.

## If it ever stands still

Plug it into a computer and press READ WHAT IT HAS on the site, step 2. The board says how long it has
been running, what it started after (power on, the reset pin, a watchdog, a crash) and, to a minute, how
long the run before lasted. It keeps that through a reset and a power cut. If opening the port restarted
it, the run before is the one that stood still.

## Build it yourself

The firmware is ESP-IDF 5.5. The screen code in `core/` is plain C, the same files build the firmware
and a PC preview.

- `firmware/BUILD.BAT <screen> [COMx]` builds one screen's firmware (and flashes it to COMx). Screens:
  `ili9341` (2.8" CYD, the default), `st7796` (3.5" CYD), `gc9a01` (round), `ili9486` (RPi LCD).
  Each screen has its own build folder and sdkconfig.
- `.github/workflows/pages.yml` builds all four in Espressif's ESP-IDF image and publishes the site
  with them on GitHub Pages, on every push to main.
- The version is the first `## ` line of `CHANGES.md`: the firmware is built with it, and the site
  publishes it in `version.json` for the boards and shows the file as its history. A new version is a new
  section at the top of that file.
- `tools/QrBake` (.NET 8) writes the site's address and its QR code into `core/qr_site.h` and `.c`.
- `tools/preview/BUILD.BAT <screen>` (Visual Studio 2022) draws every page on the PC into `shots/`.
  `TOUR.BAT` films the how-to tour, `GIF.BAT` makes the GIF above from it (both with ffmpeg).
- `tools/Guide/GUIDE.BAT` (.NET 8) puts the arrows on the shots: `shots/guide/`, from `notes.txt`.
- `tools/FontBake` bakes the fonts in `core/font_*.c`.

What is where:

- `core/`: the sky (the networks' answers read piece by piece), the demo sky, the map from
  OpenStreetMap's tiles, the drawing, the fonts, the pages and the touch.
- `firmware/`: the boards, the screens and the touch chips, WiFi and HTTPS, the tiles in the flash,
  the settings in NVS, the USB console.
- `site/`: the page that flashes the boards and sends them their settings.

## The USB console

The site talks to the board over the USB cable at 115200 baud, a line each way. Scripts can do the same.

- `OWLOS HELLO` is answered with `OWLOS READY flight-radar <version>`.
- `OWLOS GET` is answered with `OWLOS CFG {...}`: the network's name, home (`lat`, `lon`, `place`,
  `country`), `src`, whether a `key` is stored, `up`, `flip`, `theme`, `look`, `map`, `ground`, `watch`,
  `led`, `led_level`, `led_hold`, `led_near`, `led_red`, `version`, `newest` (the site's, `""` not heard
  yet), `uptime` and `ran` in seconds, `reset`, and `screen` on the
  2.8" CYD. Never the password, never the key.
- `OWLOS SET {...}` with any of `ssid`, `pass`, `key`, `lat`, `lon`, `place`, `country`, `src` (`free` or
  `fr24`), `up` (degrees), `flip`, `theme` (0 to 4), `look` (0 radar, 1 map, 2 classic), `map`, `ground`
  (1 planes on the ground shown, 0 hidden), `watch` (`"EZY12AB, G-EUPT"`, `""` empties it), `led` (what
  lights it, added up: 1 SOS, 2 in range, 4 take-off, 8 landing, 16 any near, 32 no data), `led_near`
  (1, 2, 5 or 10 of the distance unit), `led_level` (0 low, 1 mid,
  2 high), `led_hold` (0 till touched, 1 10 s, 2 while it lasts), `led_red` (4 or 22, the red one's GPIO),
  `led_test` (1: red, green, blue once),
  `screen` (0 v1, 1 v2, 2 v3). What is left out stays as it is. Answered with `OWLOS OK` or
  `OWLOS ERR <why>`.

## Data and licences

- The code is MIT licensed, see `LICENSE`.
- Planes: [adsb.lol](https://adsb.lol/) (ODbL) and [adsb.fi](https://adsb.fi/), or
  [Flightradar24](https://fr24api.flightradar24.com/) with your own key.
- The map and the town names: © [OpenStreetMap](https://www.openstreetmap.org/copyright) contributors (ODbL).
- Flashing from the browser: [ESP Web Tools](https://esphome.github.io/esp-web-tools/).
