namespace RawMetalCampaignEditor;

// Double-buffered 2D plan. Painting mutates the layer rows directly and
// only ever redraws this control - the artist's drag loop stays at
// frame rate and nothing else in the app is touched per mouse-move.
public sealed class PlanCanvas : Control
{
    public Project? Project;
    public Chunk? Chunk;
    public int LayerIndex;
    public Obj? Selected;
    public float Zoom = 1f;

    public Action<int, int, MouseButtons>? TileAction;   // tile x, y
    public Action<float, float, MouseButtons>? PlaceAction;
    public Action<Obj?>? PickAction;
    public Action<Obj, float, float>? DragAction;
    public Action<int, int, int, int>? RectAction;       // drag-a-room
    public Action? StrokeStarted;                        // undo snapshots
    public bool ToolUsesRect;

    private PointF _pan = new(18, 18);
    private Point _last;
    private bool _panning;
    private Obj? _dragObj;
    private (int X, int Y)? _rectStart;
    private (int X, int Y) _hoverTile;
    private (int X, int Y)? _lastPaintTile;

    private static readonly Brush Bg = new SolidBrush(Color.FromArgb(13, 18, 24));
    private static readonly Brush FloorB = new SolidBrush(Color.FromArgb(40, 50, 62));
    private static readonly Brush WallB = new SolidBrush(Color.FromArgb(103, 118, 136));
    private static readonly Brush DeckB = new SolidBrush(Color.FromArgb(112, 92, 56));
    private static readonly Pen GridP = new(Color.FromArgb(32, 42, 54));
    private static readonly Font LabelF = new("Segoe UI", 7.5f);

    public PlanCanvas()
    {
        DoubleBuffered = true;
        ResizeRedraw = true;
        BackColor = Color.FromArgb(13, 17, 22);
        MouseDown += OnDown;
        MouseMove += OnMove;
        MouseUp += (_, e) =>
        {
            _panning = false; _dragObj = null; _lastPaintTile = null;
            if (_rectStart is { } s && e.Button == MouseButtons.Left)
            {
                RectAction?.Invoke(Math.Min(s.X, _hoverTile.X), Math.Min(s.Y, _hoverTile.Y),
                                   Math.Max(s.X, _hoverTile.X), Math.Max(s.Y, _hoverTile.Y));
            }
            _rectStart = null;
            Invalidate();
        };
        MouseWheel += (_, e) =>
        {
            Zoom = Math.Clamp(Zoom * (e.Delta > 0 ? 1.12f : .89f), .35f, 4f);
            Invalidate();
        };
    }

    public float Cell => 19 * Zoom;

    // Frame the whole 24x24 map in the panel, centred, whatever the
    // monitor shape. Called on load and resize - a map floating in a
    // black corner is the fastest way to look broken.
    public void FitView()
    {
        if (Width < 60 || Height < 60) return;
        float fit = Math.Min(Width, Height) - 28;
        Zoom = Math.Clamp(fit / (24 * 19f), .3f, 4f);
        float span = 24 * Cell;
        _pan = new PointF((Width - span) / 2, (Height - span) / 2);
        Invalidate();
    }

    private (float X, float Y) ToWorld(Point p) => ((p.X - _pan.X) / Cell, (p.Y - _pan.Y) / Cell);

    private Obj? HitTest(float wx, float wy)
    {
        if (Chunk == null) return null;
        for (int i = Chunk.Objects.Count - 1; i >= 0; i--)
        {
            var o = Chunk.Objects[i];
            if (Math.Abs(wx - o.X) <= Math.Max(.5f, o.W / 2) &&
                Math.Abs(wy - o.Y) <= Math.Max(.5f, o.D / 2)) return o;
        }
        return null;
    }

    private void OnDown(object? sender, MouseEventArgs e)
    {
        Focus();
        StrokeStarted?.Invoke();
        _last = e.Location;
        if (e.Button == MouseButtons.Middle) { _panning = true; return; }
        var (wx, wy) = ToWorld(e.Location);
        _hoverTile = ((int)wx, (int)wy);
        if (ToolUsesRect && e.Button == MouseButtons.Left)
        { _rectStart = _hoverTile; Invalidate(); return; }
        if (ToolNeedsObjectPick)
        {
            var hit = HitTest(wx, wy);
            PickAction?.Invoke(hit);
            if (hit != null && e.Button == MouseButtons.Left) _dragObj = hit;
            return;
        }
        if (wx is >= 0 and < 24 && wy is >= 0 and < 24)
        {
            TileAction?.Invoke((int)wx, (int)wy, e.Button);
            PlaceAction?.Invoke(wx, wy, e.Button);
            _lastPaintTile = ((int)wx, (int)wy);
        }
    }

    private void OnMove(object? sender, MouseEventArgs e)
    {
        if (_panning)
        {
            _pan.X += e.X - _last.X; _pan.Y += e.Y - _last.Y;
            _last = e.Location; Invalidate(); return;
        }
        _last = e.Location;
        var (hwx, hwy) = ToWorld(e.Location);
        _hoverTile = ((int)hwx, (int)hwy);
        if (_rectStart != null) { Invalidate(); return; }
        if (e.Button == MouseButtons.None) return;
        var (wx, wy) = ToWorld(e.Location);
        if (_dragObj != null && DragAction != null)
        { DragAction(_dragObj, wx, wy); Invalidate(); return; }
        if (wx is >= 0 and < 24 && wy is >= 0 and < 24)
        {
            // MS-Paint rule: a fast drag draws a continuous stroke, not
            // dots. Walk the line from the last tile to this one and
            // paint every tile between. Objects are placed once, on the
            // click - only tiles stream.
            int tx = (int)wx, ty = (int)wy;
            if (_lastPaintTile is { } from)
            {
                int dx = Math.Abs(tx - from.X), dy = Math.Abs(ty - from.Y);
                int sx = from.X < tx ? 1 : -1, sy = from.Y < ty ? 1 : -1;
                int err = dx - dy, cx = from.X, cy = from.Y;
                while (true)
                {
                    TileAction?.Invoke(cx, cy, e.Button);
                    if (cx == tx && cy == ty) break;
                    int e2 = 2 * err;
                    if (e2 > -dy) { err -= dy; cx += sx; }
                    if (e2 < dx) { err += dx; cy += sy; }
                }
            }
            else TileAction?.Invoke(tx, ty, e.Button);
            _lastPaintTile = (tx, ty);
            Invalidate();
        }
    }

    public bool ToolNeedsObjectPick;

    protected override void OnPaint(PaintEventArgs e)
    {
        var g = e.Graphics;
        g.FillRectangle(Bg, ClientRectangle);
        if (Project == null || Chunk == null || LayerIndex >= Chunk.Layers.Count)
        {
            g.DrawString("No map yet - LOAD SAMPLE BUILDING or paint a room.",
                LabelF, Brushes.Gray, 24, 24);
            return;
        }
        var layer = Chunk.Layers[LayerIndex];
        float c = Cell;
        // Building upstairs: the floor below shows through as a ghost,
        // so walls land on walls and stairs line up. Non-negotiable for
        // stacked maps.
        if (LayerIndex > 0)
        {
            var below = Chunk.Layers[LayerIndex - 1];
            using var ghostWall = new SolidBrush(Color.FromArgb(46, 130, 145, 165));
            using var ghostFloor = new SolidBrush(Color.FromArgb(30, 90, 105, 125));
            for (int y = 0; y < 24 && y < below.Rows.Count; y++)
            {
                string row = below.Rows[y];
                for (int x = 0; x < 24 && x < row.Length; x++)
                {
                    if (row[x] == '_') continue;
                    var r = new RectangleF(_pan.X + x * c, _pan.Y + y * c, c, c);
                    g.FillRectangle(row[x] == '#' ? ghostWall : ghostFloor, r);
                }
            }
        }
        // Each finish gets its own stable colour on the plan, so material
        // painting is something you can SEE, not a leap of faith.
        Color MatColor(string path)
        {
            Color[] palette =
            {
                Color.FromArgb(196, 120, 74), Color.FromArgb(96, 160, 200), Color.FromArgb(120, 190, 120),
                Color.FromArgb(200, 180, 90), Color.FromArgb(170, 120, 200), Color.FromArgb(90, 190, 180),
                Color.FromArgb(210, 140, 160), Color.FromArgb(140, 140, 210), Color.FromArgb(180, 160, 120),
                Color.FromArgb(110, 200, 150), Color.FromArgb(220, 110, 90), Color.FromArgb(150, 170, 90),
            };
            int h = 0; foreach (char ch in path) h = h * 31 + ch;
            return palette[Math.Abs(h) % palette.Length];
        }
        for (int y = 0; y < 24; y++)
        {
            string row = y < layer.Rows.Count ? layer.Rows[y] : "";
            for (int x = 0; x < 24; x++)
            {
                char q = x < row.Length ? row[x] : '_';
                var rect = new RectangleF(_pan.X + x * c, _pan.Y + y * c, c, c);
                if (q == '#') g.FillRectangle(WallB, rect);
                else if (q == '.') g.FillRectangle(FloorB, rect);
                else if (q == '=') g.FillRectangle(DeckB, rect);
                if (q != '_' && layer.Materials.TryGetValue($"{x},{y}", out var mp))
                {
                    using var mb = new SolidBrush(Color.FromArgb(165, MatColor(mp)));
                    g.FillRectangle(mb, rect);
                }
                g.DrawRectangle(GridP, rect.X, rect.Y, rect.Width, rect.Height);
            }
        }
        if (Project is { TerrainEnabled: true })
        {
            float ox = Chunk.Gx * 24, oy = Chunk.Gy * 24;
            // Ground paint is campaign-wide; clip it to this map and keep
            // it faint, or one big rect washes the whole plan into soup.
            var clip = new RectangleF(_pan.X, _pan.Y, 24 * c, 24 * c);
            foreach (var gr in Project.Ground)
            {
                var r = RectangleF.Intersect(clip, new RectangleF(_pan.X + (gr.X1 - ox) * c, _pan.Y + (gr.Y1 - oy) * c,
                                       (gr.X2 - gr.X1) * c, (gr.Y2 - gr.Y1) * c));
                if (r.IsEmpty) continue;
                using var gb = new SolidBrush(Color.FromArgb(55, GlView.GroundColor(gr.Kind)));
                g.FillRectangle(gb, r);
            }
            foreach (var fl in Project.Flats)
            {
                var r = RectangleF.Intersect(clip, new RectangleF(_pan.X + (fl.X1 - ox) * c, _pan.Y + (fl.Y1 - oy) * c,
                                       (fl.X2 - fl.X1) * c, (fl.Y2 - fl.Y1) * c));
                if (r.IsEmpty) continue;
                using var pen = new Pen(Color.FromArgb(232, 184, 96), 2f);
                g.DrawRectangle(pen, r.X, r.Y, r.Width, r.Height);
                if (c > 9) g.DrawString($"pad {fl.Height:0.#}m", LabelF, Brushes.Gold, r.X + 3, r.Y + 2);
            }
        }
        foreach (var o in Chunk.Objects)
        {
            float w = Math.Max(.45f, o.W), d = Math.Max(.45f, o.D);
            var rect = new RectangleF(_pan.X + (o.X - w / 2) * c, _pan.Y + (o.Y - d / 2) * c, w * c, d * c);
            var brush = new SolidBrush(ColorFor(o));
            g.FillRectangle(brush, rect);
            g.DrawRectangle(o == Selected ? Pens.Gold : Pens.Black, rect.X, rect.Y, rect.Width, rect.Height);
            brush.Dispose();
            string label = o.EngineFixture >= 0 && Selected != o ? "" : o.Type switch
            {
                "spawn" => o.SpawnKind ?? "spawn",
                "asset" => o.Name,
                "prim" => o.PrimKind ?? "prim",
                "sign" => string.IsNullOrEmpty(o.Title) ? "sign" : o.Title!,
                _ => o.Type,
            };
            // Imported showcase maps carry hundreds of fixtures per map;
            // labelling every one is noise, not information. Labels earn
            // their place when zoomed in or on sparse maps.
            bool showLabel = c > 15 || Chunk.Objects.Count <= 120 || o == Selected;
            if (showLabel && label.Length > 0)
                g.DrawString(label.Length > 14 ? label[..14] : label, LabelF, Brushes.White, rect.X + 2, rect.Y + 1);
        }
        if (_rectStart is { } s)
        {
            int x0 = Math.Min(s.X, _hoverTile.X), x1 = Math.Max(s.X, _hoverTile.X);
            int y0 = Math.Min(s.Y, _hoverTile.Y), y1 = Math.Max(s.Y, _hoverTile.Y);
            var r = new RectangleF(_pan.X + x0 * c, _pan.Y + y0 * c, (x1 - x0 + 1) * c, (y1 - y0 + 1) * c);
            using var fill = new SolidBrush(Color.FromArgb(60, 232, 184, 96));
            g.FillRectangle(fill, r);
            using var pen = new Pen(Color.FromArgb(232, 184, 96), 2f);
            g.DrawRectangle(pen, r.X, r.Y, r.Width, r.Height);
            g.DrawString("let go = a whole room appears", LabelF, Brushes.Gold, r.X + 4, Math.Max(0, r.Y - 16));
        }
        // Say where you are, on top of everything else. A loaded campaign
        // opening on an unnamed grid of rectangles is how
        // "what am I even looking at??" happens.
        {
            int idx = Project.Chunks.IndexOf(Chunk);
            string banner = $"Map {idx + 1} of {Project.Chunks.Count} — {Chunk.Name}";
            using var bannerFont = new Font("Segoe UI", 11f, FontStyle.Bold);
            var size = g.MeasureString(banner, bannerFont);
            using var back = new SolidBrush(Color.FromArgb(205, 10, 15, 22));
            g.FillRectangle(back, 8, 8, size.Width + 16, size.Height + 24);
            g.DrawString(banner, bannerFont, Brushes.White, 16, 12);
            g.DrawString("plan — top-down view of this map", LabelF, Brushes.LightSteelBlue, 16, 14 + size.Height);
        }
    }

    private static Color ColorFor(Obj o) => o.Type switch
    {
        "light" => GlView.LightColor(o),
        "door" or "window" => Color.FromArgb(70, 119, 137),
        "sign" => Color.FromArgb(196, 160, 74),
        "prim" => Color.FromArgb(125, 118, 105),
        "hazard" => Color.FromArgb(160, 70, 50),
        "terminal" => Color.FromArgb(70, 130, 95),
        "stairs" => Color.FromArgb(105, 112, 119),
        "spawn" => (o.SpawnKind ?? "") switch
        {
            "Player" => Color.FromArgb(80, 190, 130),
            "Hostile" or "Creature" => Color.FromArgb(200, 100, 85),
            _ => Color.FromArgb(140, 160, 200),
        },
        "asset" => AssetColor(o),
        _ => Color.FromArgb(101, 117, 105),
    };

    // One lavender for every model made a forest and a forklift the same
    // purple blob. Colour by what the thing IS; lavender keeps the rest.
    private static Color AssetColor(Obj o)
    {
        string name = (o.EngineFixtureName ?? o.Name ?? o.Model ?? "").ToLowerInvariant();
        if (name.Contains("tree") || name.Contains("pine") || name.Contains("oak") || name.Contains("aspen")
            || name.Contains("fern") || name.Contains("grass") || name.Contains("bush") || name.Contains("plant")
            || name.Contains("ivy") || name.Contains("mushroom") || name.Contains("clover"))
            return Color.FromArgb(96, 132, 82);
        if (name.Contains("rock") || name.Contains("stone") || name.Contains("stump") || name.Contains("ore"))
            return Color.FromArgb(121, 116, 106);
        if (name.Contains("crate") || name.Contains("wood") || name.Contains("log") || name.Contains("barrel"))
            return Color.FromArgb(150, 108, 64);
        if (name.Contains("car") || name.Contains("van") || name.Contains("vehicle") || name.Contains("truck"))
            return Color.FromArgb(96, 130, 168);
        return Color.FromArgb(143, 113, 158);
    }
}
