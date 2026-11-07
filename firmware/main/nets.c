#include "nets.h"
#include <stdio.h>
#include <string.h>

static int index_of(const nets_t *l, const char *ssid)
{
    for (int i = 0; i < l->count; i++) {
        if (!strcmp(l->n[i].ssid, ssid)) return i;
    }
    return -1;
}

void nets_put(nets_t *l, const char *ssid, const char *pass)
{
    net_kept_t t;                                      // first: ssid and pass may lie in the list
    snprintf(t.ssid, sizeof t.ssid, "%s", ssid);
    snprintf(t.pass, sizeof t.pass, "%s", pass);
    int at = index_of(l, ssid);
    if (at < 0) at = l->count < NETS_KEPT ? l->count++ : NETS_KEPT - 1;
    memmove(&l->n[1], &l->n[0], (size_t)at * sizeof l->n[0]);
    l->n[0] = t;
}

int nets_forget(nets_t *l, const char *ssid)
{
    int at = index_of(l, ssid);
    if (at < 0) return 0;
    l->count--;
    memmove(&l->n[at], &l->n[at + 1], (size_t)(l->count - at) * sizeof l->n[0]);
    memset(&l->n[l->count], 0, sizeof l->n[0]);        // the store saves the empty places too
    return 1;
}

const net_kept_t *nets_find(const nets_t *l, const char *ssid)
{
    int at = index_of(l, ssid);
    return at < 0 ? NULL : &l->n[at];
}
