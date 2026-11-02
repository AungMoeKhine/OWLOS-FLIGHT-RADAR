
/* ---- tiny PNG writer with its own deflate, so no zlib needed ---- */
static uint32_t crc_table[256];
static void crc_init(void)
{
    for (uint32_t n = 0; n < 256; n++) {
        uint32_t c = n;
        for (int k = 0; k < 8; k++) c = c & 1 ? 0xEDB88320u ^ (c >> 1) : c >> 1;
        crc_table[n] = c;
    }
}
static uint32_t crc(uint32_t c, const uint8_t *p, size_t n)
{
    c ^= 0xFFFFFFFFu;
    while (n--) c = crc_table[(c ^ *p++) & 255] ^ (c >> 8);
    return c ^ 0xFFFFFFFFu;
}
static void be32(uint8_t *p, uint32_t v) { p[0] = (uint8_t)(v >> 24); p[1] = (uint8_t)(v >> 16); p[2] = (uint8_t)(v >> 8); p[3] = (uint8_t)v; }
static void chunk(FILE *f, const char *type, const uint8_t *data, uint32_t len)
{
    uint8_t h[8];
    be32(h, len);
    memcpy(h + 4, type, 4);
    fwrite(h, 1, 8, f);
    if (len) fwrite(data, 1, len, f);
    uint32_t c = crc(0, (const uint8_t *)type, 4);
    c = crc(c, data, len);
    be32(h, c);
    fwrite(h, 1, 4, f);
}
/* deflate with the fixed huffman table (RFC 1951 3.2.6) and plain LZ77, hash chains.
 * not zlib grade, but screenshots shrink 5-20x and it's 80 lines instead of a library */
typedef struct { uint8_t *p; uint32_t bits; int nb; } bitw_t;

static void put_bits(bitw_t *w, uint32_t v, int n)
{
    w->bits |= v << w->nb;
    w->nb += n;
    while (w->nb >= 8) { *w->p++ = (uint8_t)w->bits; w->bits >>= 8; w->nb -= 8; }
}

// huffman codes go out MSB first, the rest of the stream LSB first
static void put_code(bitw_t *w, uint32_t code, int len)
{
    uint32_t r = 0;
    for (int i = 0; i < len; i++) r |= ((code >> i) & 1u) << (len - 1 - i);
    put_bits(w, r, len);
}

static void put_sym(bitw_t *w, int v)
{
    if (v < 144) put_code(w, 0x30u + v, 8);
    else if (v < 256) put_code(w, 0x190u + (v - 144), 9);
    else if (v < 280) put_code(w, (uint32_t)(v - 256), 7);
    else put_code(w, 0xC0u + (v - 280), 8);
}

static const uint16_t LEN_BASE[29] = { 3, 4, 5, 6, 7, 8, 9, 10, 11, 13, 15, 17, 19, 23, 27, 31, 35, 43, 51, 59,
                                       67, 83, 99, 115, 131, 163, 195, 227, 258 };
static const uint8_t  LEN_EXTRA[29] = { 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 2, 2, 2, 2, 3, 3, 3, 3, 4, 4, 4, 4, 5, 5, 5, 5, 0 };
static const uint16_t DIST_BASE[30] = { 1, 2, 3, 4, 5, 7, 9, 13, 17, 25, 33, 49, 65, 97, 129, 193, 257, 385, 513, 769,
                                        1025, 1537, 2049, 3073, 4097, 6145, 8193, 12289, 16385, 24577 };
static const uint8_t  DIST_EXTRA[30] = { 0, 0, 0, 0, 1, 1, 2, 2, 3, 3, 4, 4, 5, 5, 6, 6, 7, 7, 8, 8, 9, 9, 10, 10, 11, 11, 12, 12, 13, 13 };

static size_t deflate_fixed(const uint8_t *in, size_t n, uint8_t *out)
{
    enum { WIN = 32768, HBITS = 15, CHAIN = 48 };
    int32_t *head = malloc(sizeof(int32_t) << HBITS), *prev = malloc(sizeof(int32_t) * WIN);
    for (int i = 0; i < 1 << HBITS; i++) head[i] = -1;
    bitw_t w = { out, 0, 0 };
    put_bits(&w, 1, 1);                           // last block
    put_bits(&w, 1, 2);                           // fixed huffman
    size_t i = 0;
    while (i < n) {
        int best = 0, dist = 0;
        if (i + 2 < n) {
            uint32_t h = ((uint32_t)in[i] << 16 | (uint32_t)in[i + 1] << 8 | in[i + 2]) * 2654435761u >> (32 - HBITS);
            int32_t c = head[h];
            for (int k = 0; k < CHAIN && c >= 0 && i - (size_t)c <= WIN - 1; k++, c = prev[c % WIN]) {
                int l = 0, lim = n - i < 258 ? (int)(n - i) : 258;
                while (l < lim && in[c + l] == in[i + l]) l++;
                if (l > best) { best = l; dist = (int)(i - (size_t)c); if (l == lim) break; }
            }
            prev[i % WIN] = head[h];
            head[h] = (int32_t)i;
        }
        if (best >= 3) {
            int li = 28;
            while (LEN_BASE[li] > best) li--;
            put_sym(&w, 257 + li);
            put_bits(&w, (uint32_t)(best - LEN_BASE[li]), LEN_EXTRA[li]);
            int di = 29;
            while (DIST_BASE[di] > dist) di--;
            put_code(&w, (uint32_t)di, 5);
            put_bits(&w, (uint32_t)(dist - DIST_BASE[di]), DIST_EXTRA[di]);
            // the skipped bytes go into the hash too, or later matches are missed
            for (size_t j = i + 1; j < i + (size_t)best && j + 2 < n; j++) {
                uint32_t h = ((uint32_t)in[j] << 16 | (uint32_t)in[j + 1] << 8 | in[j + 2]) * 2654435761u >> (32 - HBITS);
                prev[j % WIN] = head[h];
                head[h] = (int32_t)j;
            }
            i += (size_t)best;
        } else {
            put_sym(&w, in[i++]);
        }
    }
    put_sym(&w, 256);
    if (w.nb) put_bits(&w, 0, 8 - w.nb);
    free(head); free(prev);
    return (size_t)(w.p - out);
}

static int paeth(int a, int b, int c)
{
    int p = a + b - c, pa = abs(p - a), pb = abs(p - b), pc = abs(p - c);
    return pa <= pb && pa <= pc ? a : pb <= pc ? b : c;
}

static int write_png(const char *path, const uint8_t *rgb, int w, int h)
{
    // every row gets the filter that leaves the smallest numbers (the usual trick)
    const size_t stride = (size_t)w * 3;
    size_t raw_len = (size_t)h * (1 + stride);
    uint8_t *raw = malloc(raw_len), *tmp = malloc(stride);
    for (int y = 0; y < h; y++) {
        const uint8_t *cur = rgb + y * stride, *up = y ? cur - stride : 0;
        uint8_t *dst = raw + y * (1 + stride);
        long best_sum = -1;
        for (int f = 0; f < 5; f++) {
            if (f >= 2 && !up) break;
            long sum = 0;
            for (size_t x = 0; x < stride; x++) {
                int a = x >= 3 ? cur[x - 3] : 0, b = up ? up[x] : 0, c = (x >= 3 && up) ? up[x - 3] : 0;
                int pred = f == 0 ? 0 : f == 1 ? a : f == 2 ? b : f == 3 ? (a + b) / 2 : paeth(a, b, c);
                tmp[x] = (uint8_t)(cur[x] - pred);
                sum += tmp[x] < 128 ? tmp[x] : 256 - tmp[x];
            }
            if (best_sum < 0 || sum < best_sum) { best_sum = sum; dst[0] = (uint8_t)f; memcpy(dst + 1, tmp, stride); }
        }
    }
    free(tmp);
    uint8_t *z = malloc(raw_len + raw_len / 4 + 64), *p = z;
    *p++ = 0x78; *p++ = 0x01;
    p += deflate_fixed(raw, raw_len, p);
    uint32_t a = 1, b = 0;
    for (size_t i = 0; i < raw_len; i++) { a = (a + raw[i]) % 65521u; b = (b + a) % 65521u; }
    be32(p, (b << 16) | a);
    p += 4;
    FILE *f = fopen(path, "wb");
    if (!f) { free(raw); free(z); return 0; }
    static const uint8_t sig[8] = { 137, 80, 78, 71, 13, 10, 26, 10 };
    fwrite(sig, 1, 8, f);
    uint8_t ihdr[13];
    be32(ihdr, (uint32_t)w); be32(ihdr + 4, (uint32_t)h);
    ihdr[8] = 8; ihdr[9] = 2; ihdr[10] = 0; ihdr[11] = 0; ihdr[12] = 0;
    chunk(f, "IHDR", ihdr, 13);
    chunk(f, "IDAT", z, (uint32_t)(p - z));
    chunk(f, "IEND", 0, 0);
    fclose(f);
    free(raw); free(z);
    return 1;
}
