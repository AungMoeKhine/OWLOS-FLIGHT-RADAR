/* ground.inc - the map under the scope for the PC preview and the film: the same core/map.c
 * and core/png_decode.c as the firmware, the tiles from OpenStreetMap kept in tiles\ (curl
 * fetches the ones missing, named like the board names itself). included by preview.c, tour.c */
#include "../../core/map.h"

static map_t      g_map;
static map_plan_t g_plan;

static int file_read(void *ctx, uint8_t *buf, int cap) { return (int)fread(buf, 1, (size_t)cap, (FILE *)ctx); }

// the ground for the settings now, made again when home or the range changed
static void make_ground(ui_t *u)
{
    float kpx = ui_ground_kpx(&u->cfg);
    int w, h, disc;
    ui_ground_size(&u->cfg, &w, &h, &disc);
    u->map = &g_map;
    if (g_map.ready && g_map.lat == u->cfg.lat && g_map.lon == u->cfg.lon && g_map.kpx == kpx && g_map.w == w && g_map.h == h) return;
    if (!map_alloc(&g_map, w, h, disc)) return;
    map_plan(&g_plan, u->cfg.lat, u->cfg.lon, kpx, w, h, disc);
    map_begin(&g_map);
    for (int k = 0; k < g_plan.ntiles; k++) {
        const tile_id_t *t = &g_plan.tiles[k];
        char path[128], url[128], cmd[512];
        snprintf(path, sizeof path, "tiles/%d_%ld_%ld.png", t->z, (long)t->x, (long)t->y);
        FILE *f = fopen(path, "rb");
        if (!f && map_url(t, url, sizeof url)) {
            snprintf(cmd, sizeof cmd, "curl -s -f -A \"OWLOS-Flight-Radar-preview/0.2 (+https://github.com/KirinDenis/OWLOS-FLIGHT-RADAR)\" -o %s %s", path, url);
            if (system(cmd) == 0) printf("  fetched %s\n", url);
            f = fopen(path, "rb");
        }
        if (!f) { printf("  no tile %s\n", path); continue; }
        if (map_tile(&g_map, &g_plan, k, file_read, f) != PNG_OK) printf("  tile %s did not read\n", path);
        fclose(f);
    }
    map_end(&g_map, &g_plan);
}
