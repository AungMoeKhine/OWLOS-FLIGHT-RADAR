/* the other pages: the list of flights, setup, the touch alignment */
#include "ui_int.h"
#include <stdlib.h>

/* --- the list: every plane, nearest first --- */

/* the columns were laid out on 320 wide, wider screens spread them. the round glass has room
 * for less: no column names, the name, the height and how far */
#define LX(x) ((x) * GFX_W / 320)
#define LIST_HEAD (RADAR_ROUND ? 0 : 14)

static int list_y(const ui_t *u, int k) { return LIST_Y0 + LIST_HEAD + k * LIST_ROW - u->list_scroll; }

int list_hit(const ui_t *u, int x, int y)
{
    (void)x;
    int b = bar_hit(x, y, 2);
    if (b >= 0) return HIT_BAR + b;
    if (y < LIST_Y0 + LIST_HEAD || y >= LIST_Y1) return HIT_NONE;
    int k = (y - LIST_Y0 - LIST_HEAD + u->list_scroll) / LIST_ROW;
    return k >= 0 && k < u->n_order ? HIT_ROW + k : HIT_NONE;
}

void list_draw(ui_t *u, canvas_t *cv)
{
    char buf[24];
    int ya = cv->y0 > LIST_Y0 + LIST_HEAD ? cv->y0 : LIST_Y0 + LIST_HEAD, yb = cv->y0 + cv->h - 1 < LIST_Y1 - 1 ? cv->y0 + cv->h - 1 : LIST_Y1 - 1;
    if (ya <= yb) {
        canvas_t lc = { cv->px + (ya - cv->y0) * GFX_W, ya, yb - ya + 1, cv->helper };
        for (int k = 0; k < u->n_order; k++) {
            int y = list_y(u, k);
            if (!gfx_touches(&lc, y, y + LIST_ROW)) continue;
            const plane_t *p = &u->snap.p[u->order[k]];
            int pressed = u->t_down && !u->t_moved && u->t_hit == HIT_ROW + k;
            if (pressed || p->icao == u->sel) gfx_fill(&lc, PG_L + 4, y, PG_R - PG_L - 8, LIST_ROW - 2, C_PRIM, pressed ? 70 : 35);
            gfx_hline(&lc, PG_L + 6, PG_R - 7, y + LIST_ROW - 1, C_QUIET, 70);
            uint32_t col = alt_colour(u, p);
            silhouette(&lc, p->kind, PG_L + 14.0f, y + 10.0f, p->track, 0.9f, col, 255);
            char d[16];
#if RADAR_ROUND
            font_text_clip(&lc, &FONT_SMALL, PG_L + 26, y + 6, p->flight[0] ? p->flight : p->reg, C_FOCUS, 250, 58);
            fmt_alt(u, p, buf, sizeof buf, 1);
            font_text(&lc, &FONT_SMALL, PG_L + 88, y + 6, buf, col, 245);
            fmt_dist(u, p->dist, d, sizeof d);
            char *sp = strchr(d, ' ');
            if (sp) *sp = 0;                            // the number, the unit is in the title
            font_text(&lc, &FONT_SMALL, PG_R - 6 - font_width(&FONT_SMALL, d), y + 6, d, C_DIM, 235);
            continue;
#endif
            font_text_clip(&lc, &FONT_MEDIUM, 28, y + 4, p->flight[0] ? p->flight : p->reg, C_FOCUS, 250, LX(102) - 32);
            font_text_clip(&lc, &FONT_SMALL, LX(102), y + 6, p->type[0] ? p->type : "----", C_DIM, 235, 34);
            fmt_alt(u, p, buf, sizeof buf, 1);
            font_text(&lc, &FONT_SMALL, LX(140), y + 6, buf, col, 245);
            snprintf(buf, sizeof buf, p->has_spd ? "%d KT" : "--", (int)lroundf(p->gs));
            font_text(&lc, &FONT_SMALL, LX(190), y + 6, buf, C_PRIM, 235);
            fmt_dist(u, p->dist, d, sizeof d);
            snprintf(buf, sizeof buf, "%s %s", d, compass8(p->brg));
            font_text(&lc, &FONT_SMALL, GFX_W - 8 - font_width(&FONT_SMALL, buf), y + 6, buf, C_DIM, 235);
        }
        if (!u->n_order) micro_text(&lc, PG_L + 12, LIST_Y0 + 24, "NOTHING IN RANGE", C_DIM, 230, RADAR_ROUND ? 1 : 2);
    }
    char d[16], right[24];
    fmt_dist(u, range_km(&u->cfg), d, sizeof d);
    snprintf(right, sizeof right, "%d IN %s", u->n_order, d);
    page_header(cv, "FLIGHTS", right, C_FOCUS);
    if (!RADAR_ROUND && gfx_touches(cv, LIST_Y0, LIST_Y0 + 12)) {
        static const char *const H[5] = { "FLIGHT", "TYPE", "HEIGHT", "SPEED", "DISTANCE" };
        const int X[5] = { 28, LX(102), LX(140), LX(190), 0 };
        for (int i = 0; i < 4; i++) micro_text(cv, X[i], LIST_Y0 + 4, H[i], C_QUIET, 230, 1);
        micro_text(cv, GFX_W - 8 - micro_width(H[4], 1), LIST_Y0 + 4, H[4], C_QUIET, 230, 1);
    }
    if (RADAR_ROUND) snprintf(right, sizeof right, "ZOOM");            // the round bar is short
    else snprintf(right, sizeof right, "ZOOM %s", d);
    const char *L[2] = { right, "BACK" };
    bar(u, cv, L, 2, -1);
}

/* --- setup: one row a setting, what it does under its name --- */

enum { S_RANGE, S_DIST, S_ALT, S_UP, S_FLIP, S_LABELS, S_THEME, S_LOOK, S_MAP, S_SOURCE, S_SERVER, S_HOME, S_WIFI, S_TOUCH,
       S_ABOUT, S_ROWS };
enum { K_NUM, K_BTN, K_INFO };

static const struct { const char *label, *desc; int kind; } ROWS[S_ROWS] = {
    { "RANGE",       "or drag inside the scope",     K_NUM },
    { "DISTANCE",    "km, nautical miles or miles",  K_BTN },
    { "ALTITUDE",    "feet or metres",               K_BTN },
    { "MAP UP",      "or drag the scope's rim",      K_NUM },
    { "SCREEN",      "mounted upside down?",         K_BTN },
    { "LABELS",      "names beside the planes",      K_BTN },
    { "COLOURS",     "five colour themes",           K_BTN },
    { "LOOK",        "radar, flat map, map's colours", K_BTN },
    { "MAP",         "OpenStreetMap under the scope", K_BTN },
    { "DATA",        "free, or FR24 with a key", K_BTN },
    { "FREE SERVER", "tried first, others back up", K_BTN },
    { "HOME",        "",                             K_INFO },
    { "WIFI",        "",                             UI_WIFI_PAGE ? K_BTN : K_INFO },
    { "TOUCH",       "align the touch screen",       K_BTN },
    { RADAR_NAME,    "",                             K_INFO },
};

#define SB_X  (PG_R - 84)                     /* the keys of a row */
#define SB_W  38
#define SK_H  (RADAR_ROUND ? 26 : 30)
#define SK_Y  ((ROW_H - SK_H) / 2)

int setup_rows(void) { return S_ROWS; }

static int row_y(const ui_t *u, int i) { return LIST_Y0 + i * ROW_H - u->setup_scroll; }

int setup_hit(const ui_t *u, int x, int y)
{
    if (bar_hit(x, y, 1) == 0) return HIT_BAR;
    if (y < LIST_Y0 || y > BAR_Y - 3) return HIT_NONE;
    int i = (y - LIST_Y0 + u->setup_scroll) / ROW_H;
    if (i < 0 || i >= S_ROWS || x < SB_X - 4) return HIT_NONE;
    if (ROWS[i].kind == K_NUM) return x < SB_X + SB_W + 2 ? HIT_SDEC + i : HIT_SINC + i;
    if (ROWS[i].kind == K_BTN) return HIT_SBTN + i;
    return HIT_NONE;
}

int setup_repeats(int hit) { return hit >= HIT_SDEC && hit < HIT_SBTN; }

static void value_text(const ui_t *u, int i, char *b, int sz)
{
    const fr_cfg_t *c = &u->cfg;
    static const char *const DU[3] = { "KM", "NM", "MI" };
    b[0] = 0;
    switch (i) {
    case S_RANGE:  fmt_dist(u, range_km(c), b, sz); break;
    case S_DIST:   snprintf(b, (size_t)sz, "%s", DU[c->dist_unit < 3 ? c->dist_unit : 0]); break;
    case S_ALT:    snprintf(b, (size_t)sz, c->alt_unit ? "M" : "FT"); break;
    case S_UP:     if (c->up) snprintf(b, (size_t)sz, "%03d\xC2\xB0", c->up); else snprintf(b, (size_t)sz, "NORTH"); break;
    case S_FLIP:   snprintf(b, (size_t)sz, c->flip ? "FLIPPED" : "NORMAL"); break;
    case S_LABELS: snprintf(b, (size_t)sz, c->labels ? "PICKED" : "ALL"); break;
    case S_THEME:  snprintf(b, (size_t)sz, "%s", UI_THEME[u->theme].name); break;
    case S_LOOK:   snprintf(b, (size_t)sz, "%s", LOOK_NAME[c->look < LOOKS ? c->look : 0]); break;
    case S_MAP:    snprintf(b, (size_t)sz, c->map ? "ON" : "OFF"); break;
    case S_SOURCE: snprintf(b, (size_t)sz, c->source == SRC_FR24 ? "FR24" : "FREE"); break;
    case S_SERVER: snprintf(b, (size_t)sz, "%s", SERVER_NAME[c->server < FREE_SERVERS ? c->server : 0]); break;
    case S_TOUCH:  snprintf(b, (size_t)sz, "ALIGN"); break;
    case S_WIFI:   snprintf(b, (size_t)sz, u->net.configured ? "CHANGE" : "SET UP"); break;
    default: break;
    }
}

static void small_key(ui_t *u, canvas_t *cv, int x, int y, int hit, int plus)
{
    int pressed = u->t_down && !u->t_moved && u->t_hit == hit;
    chamfer_box(cv, x, y, SB_W, SK_H, pressed ? C_PRIM : C_PANEL, pressed ? 150 : 170, pressed ? C_FOCUS : C_PRIM, 210);
    float cx = x + SB_W / 2.0f, cy = y + SK_H / 2.0f;
    uint32_t c = pressed ? C_FOCUS : C_PRIM;
    gfx_line(cv, cx - 7, cy, cx + 7, cy, 2.4f, c, 255);
    if (plus) gfx_line(cv, cx, cy - 7, cx, cy + 7, 2.4f, c, 255);
}

void setup_draw(ui_t *u, canvas_t *cv)
{
    int y1 = BAR_Y - 3;
    int ya = cv->y0 > LIST_Y0 ? cv->y0 : LIST_Y0, yb = cv->y0 + cv->h - 1 < y1 ? cv->y0 + cv->h - 1 : y1;
    if (ya <= yb) {
        canvas_t lc = { cv->px + (ya - cv->y0) * GFX_W, ya, yb - ya + 1, cv->helper };
        char v[32], d[112];
        for (int i = 0; i < S_ROWS; i++) {
            int y = row_y(u, i);
            if (!gfx_touches(&lc, y, y + ROW_H)) continue;
            gfx_hline(&lc, PG_L + 6, PG_R - 7, y + ROW_H - 1, C_QUIET, 90);
            font_text(&lc, RADAR_ROUND ? &FONT_SMALL : &FONT_MEDIUM, PG_L + 10, y + 4, ROWS[i].label,
                      ROWS[i].kind == K_INFO ? C_DIM : C_PRIM, 255);
            const char *desc = ROWS[i].desc;
            if (i == S_HOME) {
                if (u->cfg.place[0]) snprintf(d, sizeof d, "%s%s%s, %.3f %.3f", u->cfg.place, u->cfg.country[0] ? ", " : "",
                                              u->cfg.country, (double)u->cfg.lat, (double)u->cfg.lon);
                else snprintf(d, sizeof d, "%.4f %.4f, from the site", (double)u->cfg.lat, (double)u->cfg.lon);
                desc = d;
            } else if (i == S_WIFI) {
                if (!u->net.configured) snprintf(d, sizeof d, UI_WIFI_PAGE ? "not set: SET UP, or from the site" : "not set: send it from the site");
                else snprintf(d, sizeof d, "%s, %s", u->net.ssid, u->net.wifi ? u->net.ip : "not connected");
                desc = d;
            } else if (i == S_ABOUT) {
                snprintf(d, sizeof d, "FR24 key: %s", u->net.has_key ? "stored" : "none");
                desc = d;
            }
            value_text(u, i, v, sizeof v);
#if RADAR_ROUND
            /* the round glass: no room for what a row does. under the name: a number's value,
             * or what an information row says */
            const char *under = ROWS[i].kind == K_NUM ? v : ROWS[i].kind == K_INFO ? desc : NULL;
            if (under) font_text_clip(&lc, &FONT_SMALL, PG_L + 10, y + 17, under, ROWS[i].kind == K_NUM ? C_FOCUS : C_DIM, 240,
                                      ROWS[i].kind == K_INFO ? PG_R - PG_L - 20 : SB_X - PG_L - 14);
#else
            font_text_clip(&lc, &FONT_SMALL, 10, y + 22, desc, C_DIM, 235, (ROWS[i].kind == K_INFO ? GFX_W - 20 : SB_X - 16));
            if (ROWS[i].kind == K_NUM) font_text(&lc, &FONT_MEDIUM, SB_X - 8 - font_width(&FONT_MEDIUM, v), y + 6, v, C_FOCUS, 250);
#endif
            if (ROWS[i].kind == K_NUM) {
                small_key(u, &lc, SB_X, y + SK_Y, HIT_SDEC + i, 0);
                small_key(u, &lc, SB_X + SB_W + 4, y + SK_Y, HIT_SINC + i, 1);
            } else if (ROWS[i].kind == K_BTN) {
                int pressed = u->t_down && !u->t_moved && u->t_hit == HIT_SBTN + i;
                button_font(&lc, SB_X, y + SK_Y, 2 * SB_W + 4, SK_H, v, pressed ? 2 : 0, &FONT_SMALL);
            }
        }
        int total = S_ROWS * ROW_H, view = y1 - LIST_Y0;
        int th = view * view / total, ty = LIST_Y0 + (view - th) * u->setup_scroll / (total - view);
        gfx_fill(&lc, PG_R - 3, ty, 2, th, C_QUIET, 200);
    }
    page_header(cv, "SETUP", RADAR_ROUND ? NULL : "DRAG TO SCROLL", C_FOCUS);
    static const char *const L[1] = { "BACK" };
    bar(u, cv, L, 1, -1);
}

void setup_act(ui_t *u, int hit, int longp, ui_cmd_t *cmd)
{
    if (hit == HIT_BAR) { if (!longp) u->view = VIEW_MAIN; return; }
    int i, dir = 0;
    if (hit >= HIT_SBTN) i = hit - HIT_SBTN;
    else if (hit >= HIT_SINC) { i = hit - HIT_SINC; dir = 1; }
    else if (hit >= HIT_SDEC) { i = hit - HIT_SDEC; dir = -1; }
    else return;
    if (i < 0 || i >= S_ROWS || longp) return;
    fr_cfg_t *c = &u->cfg;
    switch (i) {
    case S_RANGE:
        c->range = range_step(c->range, dir);
        cmd->refetch = 1;
        break;
    case S_DIST: {                                     // the scope keeps about its size in the new unit
        float km = range_km(c);
        c->dist_unit = (uint8_t)((c->dist_unit + 1) % 3);
        c->range = range_snap(km / unit_km(c->dist_unit));
        cmd->refetch = 1;
        break;
    }
    case S_ALT:    c->alt_unit ^= 1; break;
    case S_UP:     c->up = (int16_t)(((c->up + dir * 15) % 360 + 360) % 360); break;
    case S_FLIP:
        c->flip ^= 1;
        toast(u, c->flip ? "SCREEN FLIPPED" : "SCREEN NORMAL");
        break;
    case S_LABELS: c->labels ^= 1; break;
    case S_THEME:
        c->theme = (uint8_t)((u->theme + 1) % UI_THEMES);
        ui_set_theme(u, c->theme);
        break;
    case S_LOOK:   c->look = (uint8_t)((c->look + 1) % LOOKS); cmd->refetch = 1; break;
    case S_MAP:    c->map ^= 1; break;
    case S_SOURCE:
        if (c->source == SRC_FREE && !u->net.has_key) { toast(u, "NO FR24 KEY: SEND IT FROM THE SITE"); return; }
        c->source = c->source == SRC_FREE ? SRC_FR24 : SRC_FREE;
        cmd->refetch = 1;
        break;
    case S_SERVER: c->server = (uint8_t)((c->server + 1) % FREE_SERVERS); cmd->refetch = 1; break;
    case S_TOUCH:  ui_start_calibration(u); return;
    case S_WIFI:   wifi_open(u, cmd); return;
    default: return;
    }
    cmd->cfg = 1;
}

/* --- touch alignment --- */

void cal_draw(ui_t *u, canvas_t *cv)
{
    char buf[32];
    int s = u->cal_step < CAL_POINTS ? u->cal_step : CAL_POINTS - 1;
    float x = (float)CAL_PT[s][0], y = (float)CAL_PT[s][1];
    float f = (float)(u->now % 1000u) / 1000.0f;
    gfx_ring(cv, x, y, 6.0f + 14.0f * f, 1.2f, C_FOCUS, (int)(200 * (1.0f - f)));
    gfx_line(cv, x - 14, y, x + 14, y, 1.2f, C_FOCUS, 240);
    gfx_line(cv, x, y - 14, x, y + 14, 1.2f, C_FOCUS, 240);
    gfx_ring(cv, x, y, 6.0f, 1.2f, C_PRIM, 240);
    if (u->t_down) gfx_disc(cv, x, y, 5.0f, C_FOCUS, 200);
    int ty = CAL_PT[s][1] <= GFX_H / 2 ? GFX_H / 2 + 26 : GFX_H / 2 - 66;
    const char *t1 = "TOUCH ALIGNMENT";
    micro_text(cv, GFX_W / 2 - micro_width(t1, 2) / 2, ty, t1, C_FOCUS, 250, 2);
    const char *t2 = "PRESS AND HOLD THE CENTRE OF THE CROSS";
    micro_text(cv, GFX_W / 2 - micro_width(t2, 1) / 2, ty + 20, t2, C_DIM, 235, 1);
    snprintf(buf, sizeof buf, "POINT %d OF %d", s + 1, CAL_POINTS);
    micro_text(cv, GFX_W / 2 - micro_width(buf, 1) / 2, ty + 34, buf, C_PRIM, 240, 1);
    if (ui_screen_name) {
        /* a board sold with different screens: this page is the first thing after flashing,
         * the place to say how to pick the other ones */
        const char *t4 = "SCREEN WRONG? PRESS BOOT";
        micro_text(cv, GFX_W / 2 - micro_width(t4, 1) / 2, ty + 48, t4, C_PRIM, 240, 1);
        char t5[48];
        snprintf(t5, sizeof t5, "NOW %s   HOLD BOOT: SKIP", ui_screen_name);
        micro_text(cv, GFX_W / 2 - micro_width(t5, 1) / 2, ty + 57, t5, C_QUIET, 230, 1);
    } else {
        const char *t4 = "BOOT BUTTON: SKIP";
        micro_text(cv, GFX_W / 2 - micro_width(t4, 1) / 2, ty + 48, t4, C_QUIET, 230, 1);
    }
}
