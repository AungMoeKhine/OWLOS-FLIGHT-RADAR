// QrBake - the site's address as a QR code for the board's ABOUT page
//   QrBake <url> <core folder>
// writes qr_site.h (the address, the size) and qr_site.c (the modules, a bit each). the board draws
// it with nothing to work out: the address does not change. run again when it does
using System;
using System.IO;
using System.Text;

static class Program
{
    static int Main(string[] args)
    {
        if (args.Length < 2) {
            Console.WriteLine("QrBake <url> <core folder>");
            return 1;
        }
        string url = args[0], dir = args[1];
        bool[,] m = Qr.Encode(Encoding.UTF8.GetBytes(url), out int ver, out int mask);
        int n = m.GetLength(0), row = (n + 7) / 8;
        Console.WriteLine($"{url}: version {ver} ({n}x{n}), error correction M, mask {mask}");

        string made = $"written by tools/QrBake, don't edit: QrBake {url} core";
        StringBuilder h = new StringBuilder();
        h.Append("/* the site's address and its QR code, for ABOUT (ui_about.c) and for the board to ask the\n");
        h.Append($" * site for its version. {made} */\n");
        h.Append("#pragma once\n#include <stdint.h>\n\n");
        h.Append($"#define SITE_URL    \"{url}\"\n");
        h.Append($"#define QR_SITE_N   {n}                 /* modules a side: version {ver}, error correction M */\n");
        h.Append($"#define QR_SITE_ROW {row}                  /* bytes a row, the leftmost module in the top bit */\n");
        h.Append("extern const uint8_t QR_SITE[QR_SITE_N * QR_SITE_ROW];   /* 1: dark */\n");

        StringBuilder c = new StringBuilder();
        c.Append($"// {made}\n#include \"qr_site.h\"\n\n");
        c.Append("const uint8_t QR_SITE[QR_SITE_N * QR_SITE_ROW] = {\n");
        for (int y = 0; y < n; y++) {
            c.Append("   ");
            for (int b = 0; b < row; b++) {
                int v = 0;
                for (int k = 0; k < 8; k++) {
                    int x = b * 8 + k;
                    if (x < n && m[y, x]) {
                        v |= 0x80 >> k;
                    }
                }
                c.Append($" 0x{v:X2},");
            }
            c.Append("   // ");
            for (int x = 0; x < n; x++) {
                c.Append(m[y, x] ? '#' : '.');
            }
            c.Append('\n');
        }
        c.Append("};\n");

        File.WriteAllText(Path.Combine(dir, "qr_site.h"), h.ToString());
        File.WriteAllText(Path.Combine(dir, "qr_site.c"), c.ToString());
        Console.WriteLine($"wrote {Path.Combine(dir, "qr_site.h")} and qr_site.c");
        return 0;
    }
}
