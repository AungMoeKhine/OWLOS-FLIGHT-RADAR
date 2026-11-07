/* nets - the WiFi networks a board remembers, the one set last first: home and a second home, or
 * a phone's hotspot. it joins whichever it finds (net.c). nothing of the ESP-IDF in here */
#pragma once

#define NETS_KEPT 5

typedef struct {
    char ssid[33];
    char pass[65];
} net_kept_t;

typedef struct {
    net_kept_t n[NETS_KEPT];
    int        count;
} nets_t;

/* first in the list. a name it has moves up with this password, past five the oldest one goes */
void nets_put(nets_t *l, const char *ssid, const char *pass);
int  nets_forget(nets_t *l, const char *ssid);                    // 1: it was there
const net_kept_t *nets_find(const nets_t *l, const char *ssid);   // NULL: not one of them
