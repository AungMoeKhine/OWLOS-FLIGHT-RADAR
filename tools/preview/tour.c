/*
 * tour - the how-to film for the site: the flight radar's own screen code (core/, the same as in
 * the firmware) plays a script of touches over the demo sky and a finger is drawn where it touches.
 * frames go to ffmpeg as raw RGB, out comes an MP4 of the screen alone (the site puts it into a
 * photo of the board); what each part shows goes to a WebVTT file the page reads under it.
 * built with PROMO (TOUR.BAT promo) it is the short film for the posts instead: the 800x480 glass, a
 * script of what 0.6 added, its captions in a band under the screen, a last card.
 * usage: tour <ffmpeg.exe> <out.mp4> <poster.png> <captions.vtt>
 */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../../core/sky.h"
#include "../../core/demo.h"
#include "../../core/ui.h"
#include "../../core/ui_int.h"

#include "png.inc"
#include "ground.inc"

#define HOME_LAT 48.8945f              // Trencin, the board's home till the site sends one
#define HOME_LON 18.0444f
#define FPS      25
#define FRAME_MS (1000 / FPS)
#ifndef PROMO
#define PROMO    0
#endif
#define Z        (GFX_SCALED ? 1 : 2)               /* screen pixels are 2x2 in the film, the 5" has enough */
#define BAND     (PROMO ? 80 : 0)                   /* rows under the screen for the captions */
#define SIDE     (PROMO ? 196 : 0)                  /* columns beside it for the LED: 16:9 with that */
#define OUT_W    (GFX_PW * Z + SIDE)
#define OUT_H    (GFX_PH * Z + BAND)

static sky_t    sky;
static demo_t   demo;
static ui_t     ui;
static uint32_t t_ms;
static char     json[32768];
static FILE    *film;
static uint8_t  rgb[OUT_W * OUT_H * 3];
static int      frames;
static const char *poster;

/* the finger: where it is, whether it presses, and the last lift for a ripple */
static float    fx = -100, fy = -100;
static int      f_down, f_shown;
static uint32_t f_lift = 0;
/* the captions: what each part shows, from when. written out as WebVTT at the end */
typedef struct { uint32_t at; const char *a, *b; } cue_t;
static cue_t    cues[32];
static int      ncues;
static uint32_t t_start;

static void feed_sky(void)
{
    int n = demo_json(&demo, HOME_LAT, HOME_LON, range_km(&ui.cfg), t_ms, json, sizeof json);
    static sky_reader_t rd;
    sky_read_begin(&rd, &sky, 0, t_ms);
    sky_read(&rd, json, n);
    sky_read_end(&rd, &sky);
    sky_forget(&sky, t_ms);
}

static void tick(void)
{
    uint32_t was = t_ms;
    t_ms += FRAME_MS;
    if (t_ms / 5000u != was / 5000u) feed_sky();     // an answer every 5 s, like the board asks
}

/* --- drawing a frame --- */

#define STRIP 24
static uint32_t strip_px[GFX_PW * STRIP];
static uint16_t screen565[GFX_PW * GFX_PH];

static void put565(int y0, int h)
{
    for (int y = y0; y < y0 + h; y++)
        for (int x = 0; x < GFX_PW; x++) {
            uint16_t v = screen565[y * GFX_PW + x];
            if (!RADAR_RGB_BUS) v = (uint16_t)((v >> 8) | (v << 8));
            uint8_t r = (uint8_t)(((v >> 11) & 31) * 255 / 31), g = (uint8_t)(((v >> 5) & 63) * 255 / 63), b = (uint8_t)((v & 31) * 255 / 31);
            for (int dy = 0; dy < Z; dy++)
                for (int dx = 0; dx < Z; dx++) {
                    uint8_t *o = rgb + ((size_t)(y * Z + dy) * OUT_W + x * Z + dx) * 3;
                    o[0] = r; o[1] = g; o[2] = b;
                }
        }
}

// a fingertip seen from above: a soft light disc, firmer when it presses, a ripple when it lets go
static void finger_overlay(void)
{
    if (!f_shown) return;
    const float cx = GFX_PX(fx) * Z + Z / 2.0f, cy = GFX_PX(fy) * Z + Z / 2.0f, R = 15.0f;
    float rip = (float)(t_ms - f_lift) / 350.0f;
    for (int y = (int)(cy - 40); y <= (int)(cy + 40); y++) {
        if (y < 0 || y >= GFX_PH * Z) continue;
        for (int x = (int)(cx - 40); x <= (int)(cx + 40); x++) {
            if (x < 0 || x >= OUT_W) continue;
            float d = sqrtf((x - cx) * (x - cx) + (y - cy) * (y - cy)), a = 0;
            a += (f_down ? 0.34f : 0.16f) * clampf(R - d, 0, 1);                  // the tip
            a += (f_down ? 0.85f : 0.55f) * clampf(1.2f - fabsf(d - R), 0, 1);    // its edge
            if (f_down) a += 0.5f * clampf(4.5f - d, 0, 1);                       // where it presses
            if (!f_down && rip < 1.0f) a += 0.7f * (1.0f - rip) * clampf(1.5f - fabsf(d - (R + 22.0f * rip)), 0, 1);
            if (a <= 0) continue;
            a = clampf(a, 0, 1);
            uint8_t *o = rgb + ((size_t)y * OUT_W + x) * 3;
            for (int k = 0; k < 3; k++) o[k] = (uint8_t)(o[k] + (255 - o[k]) * a);
        }
    }
}

#if PROMO
/* words under the screen and on the last card, drawn with the same text code on a canvas of their
 * own. all pixels of the glass here: the film's fonts are for the film only */
extern const font_t FONT_TITLE, FONT_BIG;
static const char *band_a = "", *band_b = "";
static uint32_t    band_at;
static uint32_t    whole[GFX_PW * (GFX_PH + BAND)];
/* the LED beside the screen: on the 5" there is none, this film draws the one the CYDs have on
 * their back. what it shows comes from core/light.c */
static light_t     led;
static light_cfg_t led_cfg;
static int         led_ev = -1;                        /* its line in the legend */

// a canvas is as wide as the glass: the part of the film it stands for starts at column x0
static void to_whole(int y0, int rows, int x0)
{
    for (int y = y0; y < y0 + rows; y++)
        for (int x = 0; x < GFX_PW; x++) {
            const uint8_t *p = rgb + ((size_t)y * OUT_W + x0 + x) * 3;
            whole[y * GFX_PW + x] = RGB(p[0], p[1], p[2]);
        }
}

static void from_whole(int y0, int rows, int x0)
{
    for (int y = y0; y < y0 + rows; y++)
        for (int x = 0; x < GFX_PW; x++) {
            const uint32_t c = whole[y * GFX_PW + x];
            uint8_t *p = rgb + ((size_t)y * OUT_W + x0 + x) * 3;
            p[0] = (uint8_t)(c >> 16);
            p[1] = (uint8_t)(c >> 8);
            p[2] = (uint8_t)c;
        }
}

// the layout's y for a row of the glass, the text calls take that
static int ly(int row) { return (row * 2 + 2) / 3; }

static void centred(canvas_t *cv, const font_t *f, int row, const char *s, uint32_t c, int a)
{
    font_text(cv, f, (GFX_W - font_width(f, s)) / 2, ly(row), s, c, a);
}

static void band(void)
{
    canvas_t cv = { whole + GFX_PH * GFX_PW, 0, BAND, 0 };
    for (int i = 0; i < GFX_PW * BAND; i++) cv.px[i] = RGB(7, 13, 24);
    gfx_fill(&cv, 0, 0, GFX_W, 1, C_QUIET, 200);
    const int a = t_ms - band_at < 300u ? (int)((t_ms - band_at) * 256u / 300u) : 256;   // words fade in
    font_text(&cv, &FONT_TITLE, 12, ly(10), band_a, C_FOCUS, a);
    font_text(&cv, &FONT_MEDIUM, 12, ly(46), band_b, C_PRIM, a);
    const char *note = "demo sky, PC simulator";
    font_text(&cv, &FONT_TINY, GFX_W - 12 - font_width(&FONT_TINY, note), ly(14), note, C_DIM, 230);
    from_whole(GFX_PH, BAND, 0);
}

static void side_text(canvas_t *cv, const font_t *f, int row, const char *s, uint32_t c, int a)
{
    font_text(cv, f, (SIDE * 2 / 3 - font_width(f, s)) / 2, ly(row), s, c, a);
}

static void side(void)
{
    static const struct { const char *name; uint8_t r, g, b; } ROW[LIGHT_EVENTS] = {
        [LIGHT_SOS] = { "SOS", 255, 40, 40 },        [LIGHT_RANGE] = { "IN RANGE", 40, 255, 80 },
        [LIGHT_UP] = { "TAKE-OFF", 70, 110, 255 },   [LIGHT_DOWN] = { "LANDING", 40, 240, 240 },
        [LIGHT_NEAR] = { "ANY NEAR", 255, 255, 255 }, [LIGHT_NODATA] = { "NO DATA", 255, 150, 40 },
    };
    static uint32_t px[GFX_PW * (GFX_PH + BAND)];
    const int rows = GFX_PH + BAND, cx = SIDE / 3, cy = ly(128);
    canvas_t cv = { px, 0, rows, 0 };
    for (int y = 0; y < rows; y++)
        for (int x = 0; x < SIDE; x++) px[y * GFX_PW + x] = RGB(7, 13, 24);
    gfx_fill(&cv, 0, 0, 1, ly(rows), C_QUIET, 200);
    side_text(&cv, &FONT_MEDIUM, 16, "RGB LED", C_FOCUS, 255);
    side_text(&cv, &FONT_TINY, 42, "on the CYD boards", C_DIM, 235);
    uint8_t c[3];
    light_colour(&led, &led_cfg, t_ms, c);
    const uint32_t lit = RGB(c[0], c[1], c[2]);
    const int on = c[0] || c[1] || c[2];
    round_box(&cv, cx - 25, cy - 25, 50, 50, 5, RGB(196, 200, 206), 255);    // the package
    gfx_disc(&cv, (float)cx, (float)cy, 19.0f, RGB(38, 42, 50), 255);
    gfx_disc(&cv, (float)cx, (float)cy, 16.0f, on ? gfx_mix(lit, RGB(255, 255, 255), 70) : RGB(96, 102, 112), 255);
    if (on) {
        gfx_disc(&cv, (float)cx, (float)cy, 7.0f, gfx_mix(lit, RGB(255, 255, 255), 190), 255);
        gfx_glow(&cv, (float)cx, (float)cy, 60.0f, lit, 235);
    }
    for (int i = 0; i < LIGHT_EVENTS; i++) {
        const int row = 236 + i * 36, here = i == led_ev;
        const uint32_t col = RGB(ROW[i].r, ROW[i].g, ROW[i].b);
        if (here) gfx_fill(&cv, 8, ly(row) - 4, SIDE * 2 / 3 - 16, 20, col, 42);
        gfx_disc(&cv, 21.0f, (float)ly(row) + 5.0f, 4.5f, col, here ? 255 : 130);
        font_text(&cv, &FONT_SMALL, 34, ly(row), ROW[i].name, here ? C_FOCUS : C_DIM, here ? 255 : 210);
    }
    side_text(&cv, &FONT_TINY, 464, "drawn here, the real one", C_DIM, 210);
    side_text(&cv, &FONT_TINY, 478, "is on the board's back", C_DIM, 210);
    for (int y = 0; y < rows; y++)
        for (int x = 0; x < SIDE; x++) {
            const uint32_t v = px[y * GFX_PW + x];
            uint8_t *p = rgb + ((size_t)y * OUT_W + GFX_PW + x) * 3;
            p[0] = (uint8_t)(v >> 16);
            p[1] = (uint8_t)(v >> 8);
            p[2] = (uint8_t)v;
        }
}
#endif

static void compose(void)
{
    if (!ui.t_down) make_ground(&ui);             // like the board: made again once the finger is off, stretched till then
    ui.snap = sky;
    ui_frame(&ui, t_ms);
    for (int y = 0; y < GFX_PH; y += STRIP) {
        canvas_t cv = { strip_px, y, y + STRIP <= GFX_PH ? STRIP : GFX_PH - y, 0 };
        ui_strip(&ui, &cv);
        gfx_pack565(&cv, screen565 + y * GFX_PW);
    }
    put565(0, GFX_PH);
    finger_overlay();
#if PROMO
    band();
    side();
#endif
}

static void emit(void)
{
    fwrite(rgb, 1, sizeof rgb, film);
    if (frames++ == FPS * 2 && poster) write_png(poster, rgb, OUT_W, OUT_H);
    tick();
}

static void frame(void)
{
    compose();
    emit();
}

/* --- the script's verbs --- */

static ui_cmd_t cmd;

static void touch(int down)
{
    ui.snap = sky;
    ui_frame(&ui, t_ms);
    ui_touch(&ui, down, (int)lroundf(fx), (int)lroundf(fy), 0, 0, t_ms, &cmd);
}

static void caption(const char *a, const char *b)
{
    if (ncues < (int)(sizeof cues / sizeof cues[0])) cues[ncues++] = (cue_t){ t_ms - t_start, a, b };
#if PROMO
    band_a = a;
    band_b = b;
    band_at = t_ms;
#endif
}

static void vtt_time(FILE *f, uint32_t ms)
{
    fprintf(f, "%02u:%02u:%02u.%03u", ms / 3600000u, ms / 60000u % 60u, ms / 1000u % 60u, ms % 1000u);
}

static void write_vtt(const char *path, uint32_t end)
{
    FILE *f = fopen(path, "wb");
    if (!f) { printf("could not write %s\n", path); return; }
    fprintf(f, "WEBVTT\n\n");
    for (int i = 0; i < ncues; i++) {
        fprintf(f, "%d\n", i + 1);
        vtt_time(f, cues[i].at);
        fprintf(f, " --> ");
        vtt_time(f, i + 1 < ncues ? cues[i + 1].at : end);
        fprintf(f, "\n%s\n%s\n\n", cues[i].a, cues[i].b);
    }
    fclose(f);
}

static void wait(int ms) { for (int t = 0; t < ms; t += FRAME_MS) frame(); }

static float ease(float f) { return f * f * (3.0f - 2.0f * f); }

// the finger, up, glides to x,y. a plane moves meanwhile: a target can be followed by giving -1 and a flight
static int plane_spot(const char *flight, float *x, float *y)
{
    for (int i = 0; i < sky.n; i++)
        if (!strcmp(sky.p[i].flight, flight) && ui.spot[i].vis == 1) { *x = ui.spot[i].x; *y = ui.spot[i].y; return 1; }
    for (int k = 0; k < ui.n_order; k++) {             // not on the scope now: the nearest one that is
        const spot_t *sp = &ui.spot[ui.order[k]];
        if (sp->vis == 1) { *x = sp->x; *y = sp->y; return 1; }
    }
    return 0;
}

static void glide(float x, float y, int ms, const char *follow)
{
    float x0 = fx, y0 = fy;
    if (!f_shown) { x0 = x + 40; y0 = GFX_H + 20; f_shown = 1; }
    for (int t = 0; t < ms; t += FRAME_MS) {
        float tx = x, ty = y;
        if (follow) { ui.snap = sky; ui_frame(&ui, t_ms); plane_spot(follow, &tx, &ty); }
        float f = ease((float)(t + FRAME_MS) / (float)ms);
        fx = x0 + (tx - x0) * f;
        fy = y0 + (ty - y0) * f;
        frame();
    }
}

static void press(int frames_down)
{
    f_down = 1;
    for (int k = 0; k < frames_down; k++) { touch(1); frame(); }
    f_down = 0;
    touch(0);
    f_lift = t_ms;
    frame();
}

static void tap(void) { press(4); }                    // 160 ms: a tap, not a hold

// pressed, moved along the scope from (a0, r0) to (a1, r1) in polar steps round home, lifted
static void drag_polar(float a0, float a1, float r0, float r1, int ms)
{
    f_down = 1;
    int n = ms / FRAME_MS;
    for (int k = 0; k <= n; k++) {
        float f = ease((float)k / (float)n), a = (a0 + (a1 - a0) * f) * D2R, r = r0 + (r1 - r0) * f;
        fx = SCX + r * sinf(a);
        fy = SCY - r * cosf(a);
        touch(1);
        frame();
    }
    for (int k = 0; k < 6; k++) { touch(1); frame(); }   // a moment still, then let go
    f_down = 0;
    touch(0);
    f_lift = t_ms;
    frame();
}

static void drag_line(float x1, float y1, int ms)
{
    float x0 = fx, y0 = fy;
    f_down = 1;
    int n = ms / FRAME_MS;
    for (int k = 0; k <= n; k++) {
        float f = ease((float)k / (float)n);
        fx = x0 + (x1 - x0) * f;
        fy = y0 + (y1 - y0) * f;
        touch(1);
        frame();
    }
    f_down = 0;
    touch(0);
    f_lift = t_ms;
    frame();
}

static float polar_x(float a, float r) { return SCX + r * sinf(a * D2R); }
static float polar_y(float a, float r) { return SCY - r * cosf(a * D2R); }
static float btn_cx(int i) { return (float)(TX - 3 + i * (BTN_W + 3) + BTN_W / 2); }
static float btn_cy(void) { return (float)(BTN_Y + BTN_H / 2); }

#if PROMO
#define GEAR_CX ((float)(PX - 9 - 13))                 /* the gear's middle, ui_scope.c has where it is */
#define GEAR_CY ((float)(GFX_H - 28 - 13))

/* the gear, and SET at the radar's tab */
static void open_options(void)
{
    glide(GEAR_CX, GEAR_CY, 600, NULL);
    tap();
    wait(350);
}

// a switch in that tab, by its row. one that is under the bar is dragged up first
static void set_row(int k)
{
    int y = LIST_Y0 + k * ROW_H - ui.setup_scroll + ROW_H / 2;
    if (y > BAR_Y - 8) {
        const float dy = (float)(y - (BAR_Y - ROW_H));
        glide(150.0f, 240.0f, 350, NULL);
        drag_line(150.0f, 240.0f - dy, 500);
        y = LIST_Y0 + k * ROW_H - ui.setup_scroll + ROW_H / 2;
    }
    /* to the left of the word, so the finger does not cover what changes. pressed long enough to
     * be seen, and it stays a moment with the switch lit */
    glide(GFX_W - 62.0f, (float)y, 340, NULL);
    press(7);
    wait(450);
}

// SET's last button, BACK: VIEW DATA RADAR BACK
static void close_options(void)
{
    glide(GFX_W - 70.0f, (float)(BAR_Y + BAR_H / 2), 380, NULL);
    tap();
}

// the screen dims, and the words stay, in the middle of the film
static void last_card(int ms)
{
    const int off = (OUT_W - GFX_PW) / 2, rows = GFX_PH + BAND;
    band_a = band_b = "";
    for (int t = 0; t < ms; t += FRAME_MS) {
        compose();
        const int f = t < 500 ? t * 256 / 500 : 256, dim = 256 - f * 215 / 256;
        for (size_t i = 0; i < sizeof rgb; i++) rgb[i] = (uint8_t)(rgb[i] * dim >> 8);
        to_whole(0, rows, off);
        canvas_t cv = { whole, 0, rows, 0 };
        const uint32_t focus = C_FOCUS, prim = C_PRIM;
        centred(&cv, &FONT_BIG, 110, "OWLOS FLIGHT RADAR", focus, f);
        centred(&cv, &FONT_TITLE, 176, "live planes round your home", prim, f);
        centred(&cv, &FONT_MEDIUM, 232, "CYD 2.8\" and 3.5\", CrowPanel 1.28\", 2.8\" and 5\", and more", prim, f);
        centred(&cv, &FONT_MEDIUM, 262, "free and open source, flash it from the browser", prim, f);
        centred(&cv, &FONT_TITLE, 334, "kirindenis.github.io/OWLOS-FLIGHT-RADAR", focus, f);
        centred(&cv, &FONT_TINY, 400, "The planes in this film are a demo sky, drawn by the firmware's own code on a PC.", C_DIM, f);
        from_whole(0, rows, off);
        emit();
    }
}

static void finger_away(void)
{
    glide(GFX_W + 40.0f, GFX_H + 30.0f, 450, NULL);
    f_shown = 0;
}

// what it does for someone who has never held one, then what 0.6 added, then the LED
static void promo(void)
{
    const float RIM = SR - 7.0f, R_IN = PR * 0.45f, R_OUT = PR * 0.9f;   // where the finger turns and zooms
    light_defaults(&led_cfg);
    led_cfg.level = LIGHT_HIGH;

    caption("OWLOS FLIGHT RADAR 0.6", "live planes round your home on an ESP32 touch screen");
    wait(2600);

    caption("TAP A PLANE", "its callsign, type, height, speed, and more if it sends it");
    glide(0, 0, 800, "OWL205");
    tap();
    wait(3000);
    glide(btn_cx(2), btn_cy(), 500, NULL);
    tap();
    wait(250);

    caption("DRAG THE RIM: THE MAP TURNS", "match it to the way the screen stands");
    glide(polar_x(0, RIM), polar_y(0, RIM), 650, NULL);
    drag_polar(0, 80, RIM, RIM, 1500);
    wait(700);
    caption("BACK NEAR NORTH IT SNAPS", "north up again");
    glide(polar_x(80, RIM), polar_y(80, RIM), 350, NULL);
    drag_polar(80, 3, RIM, RIM, 1200);
    wait(500);

    caption("DRAG INSIDE: ZOOM", "away from home: closer");
    glide(polar_x(225, R_IN), polar_y(225, R_IN), 650, NULL);
    drag_polar(225, 225, R_IN, R_OUT, 1400);
    wait(1000);
    caption("DRAG INSIDE: ZOOM", "toward home: farther");
    drag_polar(225, 225, R_OUT, R_IN, 1400);
    wait(700);

    caption("RADAR, MAP, CLASSIC", "a tap on the look, the map comes from OpenStreetMap");
    for (int k = 0; k < LOOKS; k++) {                  // the button moves with the look: the finger follows it
        glide(look_x(&ui) + look_w(&ui) / 2.0f, LOOK_Y + LOOK_H / 2.0f, k ? 420 : 650, NULL);
        tap();
        wait(k == LOOKS - 1 ? 300 : 1900);             // round to the radar again
    }

    caption("A GEAR OPENS THE RADAR'S OPTIONS", "new in 0.6, all off until you want them");
    open_options();
    set_row(2);                                        // DATA BLOCK
    set_row(3);                                        // AUTOPILOT
    set_row(4);                                        // VECTORS
    close_options();
    caption("VECTORS AND DATA BLOCKS", "where it is going, climbing, speed, the autopilot's height");
    finger_away();
    wait(3000);

    caption("BEAM STEPS", "a plane moves only when the beam passes over it");
    open_options();
    set_row(5);                                        // BEAM STEPS
    close_options();
    finger_away();
    wait(4400);

    caption("CONFLICT ALERT", "two planes closing on each other turn red");
    open_options();
    set_row(8);                                        // CONFLICT
    close_options();
    finger_away();
    wait(3200);

    /* the LED itself is the firmware's code: what lights it, its colours and its blinks. the
     * emergency comes through the sky, the watch list's news is said to it here */
    caption("AN RGB LED FOR WHAT YOU PICK", "an emergency round home: it blinks red, fast");
    demo.emergency = 1;
    feed_sky();
    light_sky(&led, &led_cfg, &sky, unit_km(ui.cfg.dist_unit), t_ms);
    led_ev = LIGHT_SOS;
    wait(2800);
    caption("A TOUCH PUTS IT OUT", "it stays till you have seen it");
    glide(60.0f, (float)(GFX_H - 50), 500, NULL);
    tap();
    light_touch(&led);
    led_ev = -1;
    demo.emergency = 0;
    feed_sky();
    wait(800);
    caption("A WATCHED PLANE IN RANGE: GREEN", "up to 8 planes on your watch list");
    light_event(&led, LIGHT_RANGE, t_ms);
    led_ev = LIGHT_RANGE;
    finger_away();
    wait(1900);
    light_touch(&led);
    caption("A WATCHED PLANE TAKES OFF: BLUE", "anywhere in the world");
    light_event(&led, LIGHT_UP, t_ms);
    led_ev = LIGHT_UP;
    wait(1800);
    light_touch(&led);
    caption("A WATCHED PLANE LANDS: CYAN", "anywhere in the world");
    light_event(&led, LIGHT_DOWN, t_ms);
    led_ev = LIGHT_DOWN;
    wait(1800);
    light_touch(&led);
    led_ev = -1;
    last_card(4600);
}
#endif

int main(int argc, char **argv)
{
    if (argc < 3) { printf("usage: tour <ffmpeg.exe> <out.mp4> [poster.png]\n"); return 1; }
    poster = argv[3];
    crc_init();
    char line[1024];
    snprintf(line, sizeof line, "%s -hide_banner -loglevel error -y -f rawvideo -pix_fmt rgb24 -s %dx%d -r %d -i - "
             "-c:v libx264 -preset slow -crf %d -tune animation -pix_fmt yuv420p -movflags +faststart \"%s\"",
             argv[1], OUT_W, OUT_H, FPS, PROMO ? 20 : 24, argv[2]);
    film = _popen(line, "wb");
    if (!film) { printf("could not start %s\n", argv[1]); return 1; }

    ui_init(&ui, 0);
    sky_init(&sky, HOME_LAT, HOME_LON);
    demo_init(&demo, range_km(&ui.cfg), 82);
    ui.net.configured = ui.net.wifi = 1;               // a board with its settings, on the WiFi
    snprintf(ui.net.ssid, sizeof ui.net.ssid, "HOME");
    snprintf(ui.net.server, sizeof ui.net.server, "ADSB.LOL");
#if PROMO
    /* two of them head on at one height, for the conflict: it fires inside two minutes of meeting,
     * so they are in it by the time the film switches it on, and still apart on the scope */
    demo.p[0].x = -42.0f; demo.p[0].y = 12.0f; demo.p[0].track = 90.0f; demo.p[0].alt = 34000.0f;
    demo.p[10].x = 42.0f; demo.p[10].y = 12.0f; demo.p[10].track = 270.0f; demo.p[10].alt = 34000.0f;
#endif
    for (int s = 0; s < 90 * FPS; s++) tick();       // ninety seconds of sky first: trails grow
    t_start = t_ms;
#if PROMO
    promo();
#else
    const float RIM = SR - 7.0f, R_IN = PR * 0.45f, R_OUT = PR * 0.9f;   // where the finger turns and zooms

    caption("THE PLANES AROUND YOUR HOME", "home in the middle, each one turned the way it flies");
    wait(3500);

    caption("TAP A PLANE", "its callsign, type, height, speed and how far it is");
    glide(0, 0, 900, "OWL77H");
    tap();
    wait(3000);
    caption("NEXT: THE NEXT NEAREST ONE", "X lets it go");
    glide(btn_cx(0), btn_cy(), 700, NULL);
    tap();
    wait(1800);
    glide(btn_cx(2), btn_cy(), 600, NULL);
    tap();
    wait(800);

    caption("DRAG THE RIM: THE MAP TURNS", "match it to the way the screen stands");
    glide(polar_x(0, RIM), polar_y(0, RIM), 900, NULL);
    drag_polar(0, 80, RIM, RIM, 1800);
    wait(1500);
    caption("BACK NEAR NORTH IT SNAPS", "north up again");
    glide(polar_x(80, RIM), polar_y(80, RIM), 500, NULL);
    drag_polar(80, 3, RIM, RIM, 1500);
    wait(1400);

    caption("DRAG INSIDE: ZOOM", "away from home: closer");
    glide(polar_x(225, R_IN), polar_y(225, R_IN), 900, NULL);
    drag_polar(225, 225, R_IN, R_OUT, 1600);
    wait(1800);
    caption("DRAG INSIDE: ZOOM", "toward home: farther");
    drag_polar(225, 225, R_OUT, R_IN, 1600);
    wait(1500);

    caption("HUE: FIVE COLOUR THEMES", "the same meaning in all of them: colour is height");
    glide(btn_cx(0), btn_cy(), 900, NULL);
    for (int k = 0; k < UI_THEMES; k++) { tap(); wait(k == UI_THEMES - 1 ? 2300 : 1300); }   // the last name fades first

    caption("THE LOOK: RADAR, MAP, CLASSIC", "the beam over the map, a square map north up, and in the map's own colours");
    for (int k = 0; k < LOOKS; k++) {                // the button moves with the look: the finger follows it
        glide(look_x(&ui) + look_w(&ui) / 2.0f, LOOK_Y + LOOK_H / 2.0f, k ? 600 : 900, NULL);
        tap();
        wait(k == LOOKS - 1 ? 1200 : 2400);          // round to the radar again
        if (k == 0) {
            caption("THE THREE DOTS: THE PANEL IN AND OUT", "the map has the whole screen, a picked plane brings the panel");
            glide(GFX_W - 18.0f, LOOK_Y + LOOK_H / 2.0f, 600, NULL);
            tap();
            wait(2000);
            tap();
            wait(1000);
            caption("THE LOOK: RADAR, MAP, CLASSIC", "the beam over the map, a square map north up, and in the map's own colours");
        }
    }

    caption("LIST: EVERY FLIGHT, NEAREST FIRST", "drag to scroll");
    glide(btn_cx(1), btn_cy(), 700, NULL);
    tap();
    glide(GFX_W / 2.0f, GFX_H * 0.66f, 600, NULL);  // off the BACK that is under it now
    wait(900);
    drag_line(GFX_W / 2.0f, GFX_H * 0.33f, 900);
    wait(700);
    caption("TAP A ROW", "and it is picked on the scope");
    glide(60, GFX_H * 0.46f, 600, NULL);
    tap();
    wait(2500);
    glide(btn_cx(2), btn_cy(), 700, NULL);
    tap();
    wait(600);

    caption("SET: HOW IT SHOWS", "units, labels, map up, the screen upside down");
    glide(btn_cx(2), btn_cy(), 500, NULL);
    tap();
    wait(1500);
    glide(GFX_W / 2.0f, GFX_H * 0.7f, 600, NULL);
    drag_line(GFX_W / 2.0f, GFX_H * 0.25f, 1100);
    wait(1500);
    glide(GFX_W / 2.0f, (float)(BAR_Y + BAR_H / 2), 700, NULL);
    tap();
    caption("THE PLANES AROUND YOUR HOME", "home in the middle, each one turned the way it flies");
    glide(GFX_W + 40.0f, GFX_H + 30.0f, 900, NULL);
    f_shown = 0;
    wait(1500);
#endif

    write_vtt(argv[4], t_ms - t_start);
    int rc = _pclose(film);
    printf("tour: %d frames, %.1f s, ffmpeg %d -> %s\n", frames, frames / (double)FPS, rc, argv[2]);
    return rc;
}
