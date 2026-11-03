// the QR encoder of OWLOS WiFi Radar's QrBake (byte mode, no library), with error correction M
// here: a code on a small screen wants big modules more than it wants to survive a picture in it
using System;
using System.Collections.Generic;

static class Qr
{
    // error correction M, versions 1..10: codewords per block, number of blocks (ISO 18004 table 9)
    static readonly int[] EccPerBlock = { -1, 10, 16, 26, 18, 24, 16, 18, 22, 22, 26 };
    static readonly int[] Blocks = { -1, 1, 1, 1, 2, 2, 4, 4, 4, 5, 5 };
    const int FormatBitsM = 0;                               // L 1, M 0, Q 3, H 2

    public static bool[,] Encode(byte[] data, out int version, out int mask)
    {
        // smallest version that holds it: mode (4 bits) + count (8 bits, versions 1-9) + the bytes
        version = 0;
        for (int v = 1; v <= 9; v++) {
            if (4 + 8 + data.Length * 8 <= DataCodewords(v) * 8) {
                version = v;
                break;
            }
        }
        if (version == 0) {
            throw new Exception("too long for versions 1-9");
        }

        List<bool> bits = new List<bool>();
        void Put(int val, int n)
        {
            for (int i = n - 1; i >= 0; i--) {
                bits.Add(((val >> i) & 1) != 0);
            }
        }
        Put(0b0100, 4);
        Put(data.Length, 8);
        foreach (byte b in data) {
            Put(b, 8);
        }
        int cap = DataCodewords(version) * 8;
        Put(0, Math.Min(4, cap - bits.Count));               // terminator
        Put(0, (8 - bits.Count % 8) % 8);
        for (int pad = 0xEC; bits.Count < cap; pad ^= 0xEC ^ 0x11) {
            Put(pad, 8);
        }
        byte[] cw = new byte[bits.Count / 8];
        for (int i = 0; i < bits.Count; i++) {
            if (bits[i]) {
                cw[i >> 3] |= (byte)(0x80 >> (i & 7));
            }
        }

        byte[] all = AddEcc(cw, version);
        int size = version * 4 + 17;
        bool[,] m = new bool[size, size];                     // [y, x]
        bool[,] function = new bool[size, size];
        DrawFunction(m, function, version);
        DrawCodewords(m, function, all);

        // try the 8 masks, keep the one with the lowest penalty
        int best = -1, bestPen = int.MaxValue;
        for (int k = 0; k < 8; k++) {
            ApplyMask(m, function, k);
            DrawFormat(m, function, k);
            int p = Penalty(m);
            if (p < bestPen) {
                bestPen = p;
                best = k;
            }
            ApplyMask(m, function, k);                         // xor again = undo
        }
        mask = best;
        ApplyMask(m, function, best);
        DrawFormat(m, function, best);
        return m;
    }

    static int RawModules(int v)
    {
        int r = (16 * v + 128) * v + 64;
        if (v >= 2) {
            int na = v / 7 + 2;
            r -= (25 * na - 10) * na - 55;
            if (v >= 7) {
                r -= 36;
            }
        }
        return r;
    }

    static int DataCodewords(int v) => RawModules(v) / 8 - EccPerBlock[v] * Blocks[v];

    static int Mul(int x, int y)                              // GF(2^8), x^8 + x^4 + x^3 + x^2 + 1
    {
        int z = 0;
        for (int i = 7; i >= 0; i--) {
            z = (z << 1) ^ ((z >> 7) * 0x11D);
            z ^= ((y >> i) & 1) * x;
        }
        return z;
    }

    static byte[] Divisor(int degree)
    {
        byte[] r = new byte[degree];
        r[degree - 1] = 1;
        int root = 1;
        for (int i = 0; i < degree; i++) {
            for (int j = 0; j < r.Length; j++) {
                r[j] = (byte)Mul(r[j], root);
                if (j + 1 < r.Length) {
                    r[j] ^= r[j + 1];
                }
            }
            root = Mul(root, 0x02);
        }
        return r;
    }

    static byte[] Remainder(byte[] data, byte[] div)
    {
        byte[] r = new byte[div.Length];
        foreach (byte b in data) {
            int f = b ^ r[0];
            Array.Copy(r, 1, r, 0, r.Length - 1);
            r[^1] = 0;
            for (int i = 0; i < r.Length; i++) {
                r[i] ^= (byte)Mul(div[i], f);
            }
        }
        return r;
    }

    // split into blocks, error correction for each, interleave
    static byte[] AddEcc(byte[] data, int v)
    {
        int nb = Blocks[v], ecc = EccPerBlock[v], raw = RawModules(v) / 8;
        int nShort = nb - raw % nb, shortLen = raw / nb;
        byte[] div = Divisor(ecc);
        List<byte[]> blocks = new List<byte[]>();
        for (int i = 0, k = 0; i < nb; i++) {
            int len = shortLen - ecc + (i < nShort ? 0 : 1);
            byte[] dat = new byte[len];
            Array.Copy(data, k, dat, 0, len);
            k += len;
            byte[] e = Remainder(dat, div);
            byte[] blk = new byte[shortLen + 1];
            Array.Copy(dat, 0, blk, 0, len);
            Array.Copy(e, 0, blk, shortLen + 1 - ecc, ecc);   // short blocks keep a hole before their ecc
            blocks.Add(blk);
        }
        List<byte> res = new List<byte>();
        for (int i = 0; i < shortLen + 1; i++) {
            for (int j = 0; j < nb; j++) {
                if (i != shortLen - ecc || j >= nShort) {
                    res.Add(blocks[j][i]);
                }
            }
        }
        return res.ToArray();
    }

    static void Set(bool[,] m, bool[,] f, int x, int y, bool dark)
    {
        m[y, x] = dark;
        f[y, x] = true;
    }

    static void DrawFunction(bool[,] m, bool[,] f, int v)
    {
        int size = m.GetLength(0);
        for (int i = 0; i < size; i++) {
            Set(m, f, 6, i, i % 2 == 0);
            Set(m, f, i, 6, i % 2 == 0);
        }
        foreach ((int cx, int cy) in new[] { (3, 3), (size - 4, 3), (3, size - 4) }) {
            for (int dy = -4; dy <= 4; dy++) {
                for (int dx = -4; dx <= 4; dx++) {
                    int x = cx + dx, y = cy + dy;
                    if (x < 0 || y < 0 || x >= size || y >= size) {
                        continue;
                    }
                    int d = Math.Max(Math.Abs(dx), Math.Abs(dy));
                    Set(m, f, x, y, d != 2 && d != 4);
                }
            }
        }
        int[] pos = Align(v);
        for (int i = 0; i < pos.Length; i++) {
            for (int j = 0; j < pos.Length; j++) {
                if ((i == 0 && j == 0) || (i == 0 && j == pos.Length - 1) || (i == pos.Length - 1 && j == 0)) {
                    continue;
                }
                for (int dy = -2; dy <= 2; dy++) {
                    for (int dx = -2; dx <= 2; dx++) {
                        Set(m, f, pos[i] + dx, pos[j] + dy, Math.Max(Math.Abs(dx), Math.Abs(dy)) != 1);
                    }
                }
            }
        }
        DrawFormat(m, f, 0);                                  // reserves the area, real bits come later
    }

    static int[] Align(int v)
    {
        if (v == 1) {
            return Array.Empty<int>();
        }
        int na = v / 7 + 2, size = v * 4 + 17;
        int step = (int)Math.Ceiling((v * 4 + 4) / (double)(na * 2 - 2)) * 2;
        int[] r = new int[na];
        r[0] = 6;
        for (int i = na - 1, p = size - 7; i >= 1; i--, p -= step) {
            r[i] = p;
        }
        return r;
    }

    static void DrawFormat(bool[,] m, bool[,] f, int mask)
    {
        int size = m.GetLength(0);
        int data = FormatBitsM << 3 | mask, rem = data;
        for (int i = 0; i < 10; i++) {
            rem = (rem << 1) ^ ((rem >> 9) * 0x537);
        }
        int bits = (data << 10 | rem) ^ 0x5412;
        bool B(int i) => ((bits >> i) & 1) != 0;
        for (int i = 0; i <= 5; i++) {
            Set(m, f, 8, i, B(i));
        }
        Set(m, f, 8, 7, B(6));
        Set(m, f, 8, 8, B(7));
        Set(m, f, 7, 8, B(8));
        for (int i = 9; i < 15; i++) {
            Set(m, f, 14 - i, 8, B(i));
        }
        for (int i = 0; i < 8; i++) {
            Set(m, f, size - 1 - i, 8, B(i));
        }
        for (int i = 8; i < 15; i++) {
            Set(m, f, 8, size - 15 + i, B(i));
        }
        Set(m, f, 8, size - 8, true);                         // the dark module
    }

    static void DrawCodewords(bool[,] m, bool[,] f, byte[] data)
    {
        int size = m.GetLength(0), i = 0;
        for (int right = size - 1; right >= 1; right -= 2) {
            if (right == 6) {
                right = 5;
            }
            for (int vert = 0; vert < size; vert++) {
                for (int j = 0; j < 2; j++) {
                    int x = right - j;
                    bool up = ((right + 1) & 2) == 0;
                    int y = up ? size - 1 - vert : vert;
                    if (!f[y, x] && i < data.Length * 8) {
                        m[y, x] = ((data[i >> 3] >> (7 - (i & 7))) & 1) != 0;
                        i++;
                    }
                }
            }
        }
    }

    static void ApplyMask(bool[,] m, bool[,] f, int k)
    {
        int size = m.GetLength(0);
        for (int y = 0; y < size; y++) {
            for (int x = 0; x < size; x++) {
                if (f[y, x]) {
                    continue;
                }
                bool inv = k switch {
                    0 => (x + y) % 2 == 0,
                    1 => y % 2 == 0,
                    2 => x % 3 == 0,
                    3 => (x + y) % 3 == 0,
                    4 => (x / 3 + y / 2) % 2 == 0,
                    5 => x * y % 2 + x * y % 3 == 0,
                    6 => (x * y % 2 + x * y % 3) % 2 == 0,
                    _ => ((x + y) % 2 + x * y % 3) % 2 == 0,
                };
                if (inv) {
                    m[y, x] = !m[y, x];
                }
            }
        }
    }

    // the four penalty rules of the standard
    static int Penalty(bool[,] m)
    {
        int size = m.GetLength(0), p = 0, dark = 0;
        for (int pass = 0; pass < 2; pass++) {
            for (int a = 0; a < size; a++) {
                int run = 1;
                for (int b = 1; b <= size; b++) {
                    bool same = b < size && Get(m, pass, a, b) == Get(m, pass, a, b - 1);
                    if (same) {
                        run++;
                        continue;
                    }
                    if (run >= 5) {
                        p += 3 + run - 5;
                    }
                    run = 1;
                }
                for (int b = 0; b + 11 <= size; b++) {         // 1:1:3:1:1 with 4 light on one side
                    string s = "";
                    for (int k = 0; k < 11; k++) {
                        s += Get(m, pass, a, b + k) ? '1' : '0';
                    }
                    if (s == "10111010000" || s == "00001011101") {
                        p += 40;
                    }
                }
            }
        }
        for (int y = 0; y + 1 < size; y++) {
            for (int x = 0; x + 1 < size; x++) {
                if (m[y, x] == m[y, x + 1] && m[y, x] == m[y + 1, x] && m[y, x] == m[y + 1, x + 1]) {
                    p += 3;
                }
            }
        }
        foreach (bool d in m) {
            if (d) {
                dark++;
            }
        }
        int total = size * size;
        int kdev = (Math.Abs(dark * 20 - total * 10) + total - 1) / total - 1;
        return p + Math.Max(0, kdev) * 10;
    }

    static bool Get(bool[,] m, int pass, int a, int b) => pass == 0 ? m[a, b] : m[b, a];
}
