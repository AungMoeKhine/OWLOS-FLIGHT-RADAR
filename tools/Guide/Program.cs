// Guide - the pictures of where to tap, for the site and the README: a screenshot from
// tools/preview with short words at its sides and an arrow from each to its place on the screen
//   Guide <notes.txt> <shots folder>
// notes.txt: "image <shot> <out>" starts a picture (the shot as the preview writes it, twice the
// screen's size; out goes to <shots>/guide), then a line a note: "<L|R> <x> <y> <words>" rings a
// spot, "<L|R> <x> <y> <w>x<h> <words>" frames a button, its top left corner at x y. all in the
// screen's own pixels. the words go to the left or the right side, '|' breaks a line
using System;
using System.Collections.Generic;
using System.Drawing;
using System.Drawing.Drawing2D;
using System.Drawing.Imaging;
using System.Drawing.Text;
using System.Globalization;
using System.IO;
using System.Linq;

static class Program
{
    const int Zoom = 2;          // the preview's shots are twice the screen
    const int Side = 300;        // room for the words on each side of the shot
    const int Pad = 28;          // above and below it
    const int Gap = 10;          // between two notes on one side
    const int Ring = 15;         // the circle round the place to tap

    static readonly Color Back = Color.FromArgb(3, 8, 26);
    static readonly Color Frame = Color.FromArgb(29, 74, 120);
    static readonly Color Card = Color.FromArgb(240, 11, 33, 66);
    static readonly Color Ink = Color.FromArgb(234, 247, 255);
    static readonly Color Mark = Color.FromArgb(255, 184, 72);
    static readonly Color Shade = Color.FromArgb(170, 3, 8, 26);

    sealed class Note
    {
        public bool Left;
        public int X, Y, W, H;           // W 0: a spot, ringed. else a button, framed
        public string[] Lines = Array.Empty<string>();
        public RectangleF Box;
        public float MidY => W > 0 ? Y + H / 2f : Y;
    }

    static int Main(string[] args)
    {
        if (args.Length < 2)
        {
            Console.Error.WriteLine("usage: Guide <notes.txt> <shots folder>");
            return 2;
        }
        string shots = Path.GetFullPath(args[1]);
        string outDir = Path.Combine(shots, "guide");
        Directory.CreateDirectory(outDir);
        string? shot = null, outName = null;
        List<Note> notes = new List<Note>();
        int made = 0;
        foreach (string raw in File.ReadAllLines(args[0]))
        {
            string line = raw.Trim();
            if (line.Length == 0 || line.StartsWith("#", StringComparison.Ordinal))
            {
                continue;
            }
            string[] w = line.Split(' ', 4, StringSplitOptions.RemoveEmptyEntries);
            if (w[0] == "image")
            {
                if (shot != null && outName != null)
                {
                    Render(Path.Combine(shots, shot), Path.Combine(outDir, outName), notes);
                    made++;
                }
                shot = w[1];
                outName = w[2];
                notes = new List<Note>();
                continue;
            }
            if (w.Length < 4 || (w[0] != "L" && w[0] != "R"))
            {
                Console.Error.WriteLine("not a note: " + line);
                return 1;
            }
            Note note = new Note
            {
                Left = w[0] == "L",
                X = int.Parse(w[1], CultureInfo.InvariantCulture),
                Y = int.Parse(w[2], CultureInfo.InvariantCulture),
            };
            string words = w[3];
            string[] size = words.Split(' ', 2)[0].Split('x');
            if (size.Length == 2 && int.TryParse(size[0], out int bw) && int.TryParse(size[1], out int bh))
            {
                note.W = bw;
                note.H = bh;
                words = words.Split(' ', 2)[1];
            }
            note.Lines = words.Split('|');
            notes.Add(note);
        }
        if (shot != null && outName != null)
        {
            Render(Path.Combine(shots, shot), Path.Combine(outDir, outName), notes);
            made++;
        }
        Console.WriteLine(made + " pictures in " + outDir);
        return 0;
    }

    static void Render(string shotPath, string outPath, List<Note> notes)
    {
        using Bitmap shot = new Bitmap(shotPath);
        int w = Side + shot.Width + Side, h = shot.Height + 2 * Pad;
        using Bitmap pic = new Bitmap(w, h, PixelFormat.Format24bppRgb);
        using Graphics g = Graphics.FromImage(pic);
        g.Clear(Back);
        g.InterpolationMode = InterpolationMode.NearestNeighbor;
        g.DrawImage(shot, new Rectangle(Side, Pad, shot.Width, shot.Height));
        g.SmoothingMode = SmoothingMode.AntiAlias;
        g.TextRenderingHint = TextRenderingHint.AntiAliasGridFit;
        using (Pen frame = new Pen(Frame, 2))
        {
            g.DrawRectangle(frame, Side - 1, Pad - 1, shot.Width + 1, shot.Height + 1);
        }

        using Font font = new Font("Consolas", 21, FontStyle.Bold, GraphicsUnit.Pixel);
        StringFormat fmt = StringFormat.GenericTypographic;
        float lineH = font.GetHeight(g) + 2;
        foreach (Note n in notes)
        {
            float tw = n.Lines.Max(s => g.MeasureString(s, font, 1000, fmt).Width);
            float bw = tw + 28, bh = n.Lines.Length * lineH + 16;
            float bx = n.Left ? Side - 22 - bw : Side + shot.Width + 22;
            n.Box = new RectangleF(bx, 0, bw, bh);
        }
        Place(notes.Where(n => n.Left).ToList(), h);
        Place(notes.Where(n => !n.Left).ToList(), h);

        // the arrows first, the words over them
        foreach (Note n in notes)
        {
            PointF from = new PointF(n.Left ? n.Box.Right : n.Box.Left, n.Box.Top + n.Box.Height / 2);
            if (n.W == 0)
            {
                Arrow(g, from, new PointF(Side + n.X * Zoom, Pad + n.Y * Zoom), Ring + 3);
                Circle(g, Side + n.X * Zoom, Pad + n.Y * Zoom);
                continue;
            }
            // a button: a frame round it, the arrow to the nearest point of the frame
            RectangleF f = new RectangleF(Side + n.X * Zoom - 5, Pad + n.Y * Zoom - 5, n.W * Zoom + 10, n.H * Zoom + 10);
            PointF to = new PointF(Math.Clamp(from.X, f.Left, f.Right), Math.Clamp(from.Y, f.Top, f.Bottom));
            Arrow(g, from, to, 3);
            Outline(g, f);
        }
        using (SolidBrush card = new SolidBrush(Card))
        using (SolidBrush ink = new SolidBrush(Ink))
        using (Pen edge = new Pen(Mark, 2))
        {
            foreach (Note n in notes)
            {
                using (GraphicsPath box = Rounded(n.Box, 8))
                {
                    g.FillPath(card, box);
                    g.DrawPath(edge, box);
                }
                for (int i = 0; i < n.Lines.Length; i++)
                {
                    g.DrawString(n.Lines[i], font, ink, n.Box.Left + 14, n.Box.Top + 8 + i * lineH, fmt);
                }
            }
        }
        pic.Save(outPath, ImageFormat.Png);
        Console.WriteLine("wrote " + outPath);
    }

    // a side's notes top to bottom, each as near its place's height as the ones above it let it be
    static void Place(List<Note> side, int h)
    {
        side.Sort((a, b) => a.MidY.CompareTo(b.MidY));
        float below = 4;
        foreach (Note n in side)
        {
            float want = Pad + n.MidY * Zoom - n.Box.Height / 2;
            float top = Math.Max(want, below);
            n.Box = new RectangleF(n.Box.X, top, n.Box.Width, n.Box.Height);
            below = top + n.Box.Height + Gap;
        }
        float limit = h - 4;                            // pushed off the bottom: back up, the ones above too
        for (int i = side.Count - 1; i >= 0; i--)
        {
            Note n = side[i];
            if (n.Box.Bottom > limit)
            {
                n.Box = new RectangleF(n.Box.X, limit - n.Box.Height, n.Box.Width, n.Box.Height);
            }
            limit = n.Box.Top - Gap;
        }
    }

    // a line from the words to the place, its head short of it by stop. a dark edge under the
    // line, the circle and the frame keeps them seen over the light map
    static void Arrow(Graphics g, PointF from, PointF to, float stop)
    {
        float dx = to.X - from.X, dy = to.Y - from.Y, len = (float)Math.Sqrt(dx * dx + dy * dy);
        if (len < stop + 20)
        {
            return;
        }
        float ux = dx / len, uy = dy / len;
        PointF end = new PointF(to.X - ux * stop, to.Y - uy * stop);
        PointF[] head =
        {
            end,
            new PointF(end.X - ux * 16 - uy * 8, end.Y - uy * 16 + ux * 8),
            new PointF(end.X - ux * 16 + uy * 8, end.Y - uy * 16 - ux * 8),
        };
        PointF neck = new PointF(end.X - ux * 12, end.Y - uy * 12);
        using (Pen shade = new Pen(Shade, 8))
        {
            shade.LineJoin = LineJoin.Round;
            g.DrawLine(shade, from, neck);
            g.DrawPolygon(shade, head);
        }
        using (Pen mark = new Pen(Mark, 3))
        using (SolidBrush fill = new SolidBrush(Mark))
        {
            g.DrawLine(mark, from, neck);
            g.FillPolygon(fill, head);
        }
    }

    static void Circle(Graphics g, float x, float y)
    {
        using (Pen shade = new Pen(Shade, 8))
        {
            g.DrawEllipse(shade, x - Ring, y - Ring, 2 * Ring, 2 * Ring);
        }
        using (Pen mark = new Pen(Mark, 3))
        {
            g.DrawEllipse(mark, x - Ring, y - Ring, 2 * Ring, 2 * Ring);
        }
    }

    static void Outline(Graphics g, RectangleF f)
    {
        using (GraphicsPath p = Rounded(f, 8))
        {
            using (Pen shade = new Pen(Shade, 8))
            {
                g.DrawPath(shade, p);
            }
            using (Pen mark = new Pen(Mark, 3))
            {
                g.DrawPath(mark, p);
            }
        }
    }

    static GraphicsPath Rounded(RectangleF r, float rad)
    {
        GraphicsPath p = new GraphicsPath();
        float d = 2 * rad;
        p.AddArc(r.Left, r.Top, d, d, 180, 90);
        p.AddArc(r.Right - d, r.Top, d, d, 270, 90);
        p.AddArc(r.Right - d, r.Bottom - d, d, d, 0, 90);
        p.AddArc(r.Left, r.Bottom - d, d, d, 90, 90);
        p.CloseFigure();
        return p;
    }
}
