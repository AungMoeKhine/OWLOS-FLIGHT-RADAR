/*
 * tour - the how-to film for the site: the flight radar's own screen code (core/, the same as in
 * the firmware) plays a script of touches over the demo sky and a finger is drawn where it touches.
 * frames go to ffmpeg as raw RGB, out comes an MP4 of the screen alone (the site puts it into a
 * photo of the board); what each part shows goes to a WebVTT file the page reads under it.
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
#define Z        2                                  /* screen pixels are 2x2 in the film */
#define OUT_W    (GFX_W * Z)
#define OUT_H    (GFX_H * Z)

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
static uint32_t strip_px[GFX_W * STRIP];
static uint16_t screen565[GFX_W * GFX_H];

static void put565(int y0, int h)
{
    for (int y = y0; y < y0 + h; y++)
        for (int x = 0; x < GFX_W; x++) {
            uint16_t v = screen565[y * GFX_W + x];
            v = (uint16_t)((v >> 8) | (v << 8));
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
    const float cx = fx * Z + Z / 2.0f, cy = fy * Z + Z / 2.0f, R = 15.0f;
    float rip = (float)(t_ms - f_lift) / 350.0f;
    for (int y = (int)(cy - 40); y <= (int)(cy + 40); y++) {
        if (y < 0 || y >= GFX_H * Z) continue;
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

static void frame(void)
{
    if (!ui.t_down) make_ground(&ui);             // like the board: made again once the finger is off, stretched till then
    ui.snap = sky;
    ui_frame(&ui, t_ms);
    for (int y = 0; y < GFX_H; y += STRIP) {
        canvas_t cv = { strip_px, y, y + STRIP <= GFX_H ? STRIP : GFX_H - y, 0 };
        ui_strip(&ui, &cv);
        gfx_pack565(&cv, screen565 + y * GFX_W);
    }
    put565(0, GFX_H);
    finger_overlay();
    fwrite(rgb, 1, sizeof rgb, film);
    if (frames++ == FPS * 2 && poster) write_png(poster, rgb, OUT_W, OUT_H);
    tick();
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

static void tap(void)
{
    f_down = 1;
    for (int k = 0; k < 4; k++) { touch(1); frame(); }   // 160 ms: a tap, not a hold
    f_down = 0;
    touch(0);
    f_lift = t_ms;
    frame();
}

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

int main(int argc, char **argv)
{
    if (argc < 3) { printf("usage: tour <ffmpeg.exe> <out.mp4> [poster.png]\n"); return 1; }
    poster = argv[3];
    crc_init();
    char line[1024];
    snprintf(line, sizeof line, "%s -hide_banner -loglevel error -y -f rawvideo -pix_fmt rgb24 -s %dx%d -r %d -i - "
             "-c:v libx264 -preset slow -crf 24 -tune animation -pix_fmt yuv420p -movflags +faststart \"%s\"",
             argv[1], OUT_W, OUT_H, FPS, argv[2]);
    film = _popen(line, "wb");
    if (!film) { printf("could not start %s\n", argv[1]); return 1; }

    ui_init(&ui, 0);
    sky_init(&sky, HOME_LAT, HOME_LON);
    demo_init(&demo, range_km(&ui.cfg), 82);
    ui.net.configured = ui.net.wifi = 1;               // a board with its settings, on the WiFi
    snprintf(ui.net.ssid, sizeof ui.net.ssid, "HOME");
    snprintf(ui.net.server, sizeof ui.net.server, "ADSB.LOL");
    for (int s = 0; s < 90 * FPS; s++) tick();       // ninety seconds of sky first: trails grow
    t_start = t_ms;
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

    write_vtt(argv[4], t_ms - t_start);
    int rc = _pclose(film);
    printf("tour: %d frames, %.1f s, ffmpeg %d -> %s\n", frames, frames / (double)FPS, rc, argv[2]);
    return rc;
}
