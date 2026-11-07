using OpenTK.GLControl;
using OpenTK.Graphics.OpenGL;
using OpenTK.Mathematics;

namespace RawMetalCampaignEditor;

// 3D preview + WALK playtest. Boxes and baked OBJ meshes from the blob,
// textures straight out of editor-assets.rmblob. The game stays
// authoritative; this is for eyeballing and basic behaviour.
public sealed class GlView : GLControl
{
    public Project? Project;
    public Blob? Blob;
    public bool WalkMode;
    // Only draw the focused map and its neighbours: a 150-map campaign
    // must not cost 150 maps of geometry per frame.
    public int FocusGx, FocusGy;
    public bool FocusSet;
    public bool Wireframe;   // the Hammer view: pure structure, no textures

    // 3D build mode (the Creating Constructs way): a ghost follows the
    // mouse on a build plane, click places for real. PageUp/PageDown
    // raise the plane, R turns the piece. Plan and 3D edit one model.
    public bool BuildMode;
    public float BuildPlaneZ;
    public float BuildRotation;
    public (float W, float D, float H) GhostSize = (1, 1, 1);
    public Prefab? GhostPrefab;
    public Action<float, float, float>? BuildClick;
    private Vector3 _ghostWorld;
    private bool _ghostValid;
    private Vector3 _rayEye, _rayFwd = -Vector3.UnitZ, _rayRight = Vector3.UnitX, _rayUp = Vector3.UnitY;
    private float _tanHalfFov = .637f, _rayAspect = 1f;

    private void UpdateGhost(Point mouse)
    {
        if (Width < 4 || Height < 4) { _ghostValid = false; return; }
        float ndcX = 2f * mouse.X / Width - 1f, ndcY = 1f - 2f * mouse.Y / Height;
        var dir = Vector3.Normalize(_rayFwd + _rayRight * (ndcX * _tanHalfFov * _rayAspect) + _rayUp * (ndcY * _tanHalfFov));
        if (Math.Abs(dir.Y) < .0001f) { _ghostValid = false; return; }
        float t = (BuildPlaneZ - _rayEye.Y) / dir.Y;
        if (t < 0) { _ghostValid = false; return; }
        var hit = _rayEye + dir * t;
        _ghostWorld = new Vector3(MathF.Round(hit.X * 2) / 2, BuildPlaneZ, MathF.Round(hit.Z * 2) / 2);
        _ghostValid = true;
    }

    private float _yaw = -2.4f, _pitch = -.7f;
    private float _distMul = 1f;
    private Vector3 _cam = new(30, 26, 34);
    private bool _dragging;
    private Point _lastMouse;

    // playtest state
    private float _px, _py, _pz, _wyaw, _wpitch, _health = 100;
    private bool _lookCapture, _warping;
    private readonly HashSet<Keys> _keys = new();
    private readonly Dictionary<Obj, float> _doorOpen = new();
    public event Action<string>? Status;

    private readonly Dictionary<string, int> _textures = new();
    private sealed class MeshTri { public float[] Vertices = Array.Empty<float>(); public string Material = ""; }
    private sealed class MeshData { public List<MeshTri> Tris = new(); public Vector3 Min, Size = Vector3.One; }
    private readonly Dictionary<string, MeshData> _meshes = new();
    private Dictionary<string, MaterialItem>? _matByName;

    private MaterialItem? FindMat(string name)
    {
        if (Blob == null) return null;
        _matByName ??= Blob.Materials.GroupBy(m => m.Name).ToDictionary(g => g.Key, g => g.First());
        return _matByName.TryGetValue(name, out var m) ? m : null;
    }

    // The game's finish sets, shown with the actual texture files the
    // engine's own library ships. Painted sets 10-12 are tints of the
    // painted-wall texture, exactly the cheap trick the game pulls.
    private int SurfaceTex(Chunk chunk, string role)
    {
        int set = role switch { "wall" => chunk.SurfaceWall, "floor" => chunk.SurfaceFloor, _ => chunk.SurfaceCeiling };
        (string Name, Color Tint) pick = (set, role) switch
        {
            (0, "floor") => ("Concrete Floor", Color.White),
            (0, _) => ("Wall Grey", Color.White),
            (1, "floor") => ("Pressure Floor", Color.White),
            (1, "wall") => ("Pressure Wall", Color.White),
            (2, "floor") => ("Foundry Floor", Color.White),
            (2, _) => ("Foundry Wall", Color.White),
            (3, "floor") => ("Floor 1", Color.White),
            (3, _) => ("Wall Painted", Color.White),
            (4, "floor") => ("Metal Floor Mx 1", Color.White),
            (4, _) => ("Panel Metal", Color.White),
            (5, _) => ("Ashfall Rock", Color.White),
            (6, "floor") => ("Grating", Color.White),
            (6, _) => ("Metal Hr 1", Color.White),
            (7, "floor") => ("Office Carpet", Color.White),
            (7, _) => ("Wall Painted", Color.White),
            (8, "floor") => ("Wooden Pallet", Color.White),
            (8, _) => ("Foundry Wall", Color.White),
            (9, "floor") => ("Concrete Floor", Color.White),
            (9, _) => ("Ashfall Rock", Color.White),
            (10, "wall") => ("Wall Painted", Color.FromArgb(120, 200, 195)),
            (11, "wall") => ("Wall Painted", Color.FromArgb(140, 205, 140)),
            (12, "wall") => ("Wall Painted", Color.FromArgb(205, 130, 140)),
            (_, "floor") => ("Floor 1", Color.White),
            (_, "ceiling") => ("Ceiling 1", Color.White),
            _ => ("Wall Grey", Color.White),
        };
        var mat = FindMat(pick.Name);
        return mat != null ? TextureFor(mat.Url, pick.Tint) : TextureFor(null, Color.Gray);
    }

    // Compatibility profile is load-bearing: this view paints with the
    // fixed-function pipeline (glBegin / matrix stack), which core-profile
    // contexts silently ignore - clear works, geometry never appears.
    public GlView() : base(new GLControlSettings { Profile = OpenTK.Windowing.Common.ContextProfile.Compatability })
    {
        Dock = DockStyle.Fill;
        BackColor = Color.FromArgb(9, 12, 15);
        Resize += (_, _) => Invalidate();
        LostFocus += (_, _) => ReleaseLook();
        HandleCreated += (_, _) =>
        {
            // Vsync on: without a swap interval the preview tears on
            // wide/fast monitors, which reads as "glitchy" instantly.
            try { MakeCurrent(); if (Context != null) Context.SwapInterval = 1; } catch { }
        };
        MouseDown += OnMouseDown;
        MouseUp += (_, _) => _dragging = false;
        MouseMove += OnMouseMove;
        MouseWheel += (_, e) =>
        {
            if (!WalkMode) _distMul = Math.Clamp(_distMul * (e.Delta > 0 ? .88f : 1.14f), .25f, 2.5f);
        };
        KeyDown += (_, e) =>
        {
            _keys.Add(e.KeyCode);
            if (BuildMode && e.KeyCode == Keys.PageUp) { BuildPlaneZ = MathF.Round(BuildPlaneZ + .5f, 1); Status?.Invoke($"Build plane: {BuildPlaneZ:0.#}m - click to place at this height."); }
            if (BuildMode && e.KeyCode == Keys.PageDown) { BuildPlaneZ = MathF.Round(Math.Max(0, BuildPlaneZ - .5f), 1); Status?.Invoke($"Build plane: {BuildPlaneZ:0.#}m."); }
            if (BuildMode && e.KeyCode == Keys.R) { BuildRotation = (BuildRotation + 45) % 360; Status?.Invoke($"Piece turned to {BuildRotation:0}°."); }
        };
        KeyUp += (_, e) => _keys.Remove(e.KeyCode);
        var timer = new System.Windows.Forms.Timer { Interval = 16 };
        timer.Tick += (_, _) => { if (WalkMode) StepPlaytest(1f / 60); if (Visible) Invalidate(); };
        timer.Start();
    }

    public void EnterWalk()
    {
        if (Project == null) return;
        foreach (var chunk in Project.Chunks)
        {
            var player = chunk.Objects.FirstOrDefault(o => o.Type == "spawn" && o.SpawnKind == "Player");
            if (player == null) continue;
            _px = chunk.Gx * 24 + player.X; _py = chunk.Gy * 24 + player.Y; _pz = player.Z;
            _wyaw = -player.Rotation * MathF.PI / 180;
            _wpitch = 0; _health = 100;
            WalkMode = true;
            Focus();
            EngageLook();
            Status?.Invoke("Playtest: WASD moves, mouse looks (click the view if it stops), Shift runs. Hazards and hostiles are live.");
            return;
        }
        Status?.Invoke("Playtest needs a Player Start (PEOPLE tool).");
    }

    public void ExitWalk()
    {
        WalkMode = false;
        ReleaseLook();
    }

    // Real FPS mouse-look: cursor captured at the view's centre, every
    // mouse move steers, cursor warped back. Drag-look is the single
    // most "busted controls" feeling a playtest can have.
    private void EngageLook()
    {
        if (!WalkMode || _lookCapture) return;
        _lookCapture = true;
        Cursor.Hide();
        WarpToCentre();
    }

    private void ReleaseLook()
    {
        if (!_lookCapture) return;
        _lookCapture = false;
        Cursor.Show();
    }

    private void WarpToCentre()
    {
        if (Width < 10 || Height < 10) return;
        _warping = true;
        Cursor.Position = PointToScreen(new Point(Width / 2, Height / 2));
        _warping = false;
    }

    private void OnMouseDown(object? sender, MouseEventArgs e)
    {
        Focus();
        if (BuildMode && e.Button == MouseButtons.Left)
        {
            UpdateGhost(e.Location);
            if (_ghostValid) BuildClick?.Invoke(_ghostWorld.X, _ghostWorld.Z, BuildPlaneZ);
            return;
        }
        _dragging = true; _lastMouse = e.Location;
        if (WalkMode && !_lookCapture) EngageLook();
    }

    private void OnMouseMove(object? sender, MouseEventArgs e)
    {
        if (BuildMode) UpdateGhost(e.Location);
        if (WalkMode && _lookCapture && !_warping)
        {
            int mdx = e.X - Width / 2, mdy = e.Y - Height / 2;
            if (mdx == 0 && mdy == 0) return;
            _wyaw -= mdx * .0042f;
            _wpitch = Math.Clamp(_wpitch - mdy * .0042f, -1.4f, 1.4f);
            WarpToCentre();
            return;
        }
        if (!_dragging) return;
        float dx = e.X - _lastMouse.X, dy = e.Y - _lastMouse.Y;
        _lastMouse = e.Location;
        if (WalkMode) { _wyaw -= dx * .005f; _wpitch = Math.Clamp(_wpitch - dy * .005f, -1.4f, 1.4f); }
        else { _yaw -= dx * .01f; _pitch = Math.Clamp(_pitch - dy * .01f, -1.5f, 1.5f); }
    }

    // ---------- playtest ----------
    private char TileAt(float wx, float wy, Layer layer, Chunk chunk)
    {
        int x = (int)Math.Floor(wx - chunk.Gx * 24), y = (int)Math.Floor(wy - chunk.Gy * 24);
        if (x is < 0 or > 23 || y is < 0 or > 23 || y >= layer.Rows.Count) return '_';
        string row = layer.Rows[y];
        return x < row.Length ? row[x] : '_';
    }

    private bool Blocked(float wx, float wy, float feet)
    {
        if (Project == null) return true;
        foreach (var chunk in Project.Chunks)
        {
            if (wx < chunk.Gx * 24 - 1 || wx > chunk.Gx * 24 + 25 || wy < chunk.Gy * 24 - 1 || wy > chunk.Gy * 24 + 25) continue;
            bool hasFloor = false;
            foreach (var layer in chunk.Layers)
            {
                char q = TileAt(wx, wy, layer, chunk);
                if (q is '.' or '=') hasFloor = true;
                if (q == '#' && layer.Z < feet + 1.5f && layer.Z + layer.CeilingHeight > feet + .3f)
                {
                    bool doorThere = chunk.Objects.Any(o => o.Type == "door" &&
                        Math.Abs(chunk.Gx * 24 + o.X - wx) < 1 && Math.Abs(chunk.Gy * 24 + o.Y - wy) < 1);
                    if (!doorThere) return true;
                }
            }
            foreach (var o in chunk.Objects)
            {
                if (o.Type is not ("box" or "asset" or "terminal" or "prim")) continue;
                float ox = chunk.Gx * 24 + o.X, oy = chunk.Gy * 24 + o.Y;
                if (Math.Abs(wx - ox) < o.W / 2 + .3f && Math.Abs(wy - oy) < o.D / 2 + .3f &&
                    feet + .3f < o.Z + o.H && feet + 1.5f > o.Z) return true;
            }
            if (!hasFloor && chunk.Layers.Count > 0) return true;
        }
        return false;
    }

    private void StepPlaytest(float dt)
    {
        if (Project == null) return;
        float fx = -MathF.Sin(_wyaw), fy = -MathF.Cos(_wyaw);
        float vx = 0, vy = 0;
        if (_keys.Contains(Keys.W)) { vx += fx; vy += fy; }
        if (_keys.Contains(Keys.S)) { vx -= fx; vy -= fy; }
        if (_keys.Contains(Keys.D)) { vx += -fy; vy += fx; }
        if (_keys.Contains(Keys.A)) { vx -= -fy; vy -= fx; }
        float len = MathF.Sqrt(vx * vx + vy * vy);
        if (len > 0)
        {
            float speed = (_keys.Contains(Keys.ShiftKey) ? 8f : 4.5f) * dt / len;
            if (!Blocked(_px + vx * speed, _py, _pz)) _px += vx * speed;
            if (!Blocked(_px, _py + vy * speed, _pz)) _py += vy * speed;
        }
        foreach (var chunk in Project.Chunks)
        foreach (var o in chunk.Objects)
        {
            float ox = chunk.Gx * 24 + o.X, oy = chunk.Gy * 24 + o.Y;
            if (o.Type == "door")
                _doorOpen[o] = Math.Clamp((_doorOpen.TryGetValue(o, out float v) ? v : 0) +
                    (MathF.Sqrt((_px - ox) * (_px - ox) + (_py - oy) * (_py - oy)) < 1.7f ? dt * 3 : -dt * 2), 0, 1);
            if (o.Type == "hazard" && Math.Abs(_px - ox) < o.W / 2 && Math.Abs(_py - oy) < o.D / 2)
                _health -= 22 * dt;
            if (o.Type == "spawn" && o.SpawnKind is "Hostile" or "Creature")
            {
                float dx = _px - ox, dy = _py - oy, dist = MathF.Sqrt(dx * dx + dy * dy);
                if (dist < .8f) _health -= 12 * dt;
            }
        }
        _health = MathF.Min(100, _health + dt * 2);
        if (_health <= 0)
        {
            _health = 100;
            foreach (var chunk in Project.Chunks)
            {
                var player = chunk.Objects.FirstOrDefault(o => o.Type == "spawn" && o.SpawnKind == "Player");
                if (player != null) { _px = chunk.Gx * 24 + player.X; _py = chunk.Gy * 24 + player.Y; _pz = player.Z; }
            }
            Status?.Invoke("Playtest: you died. Back at the Player Start.");
        }
    }

    // Renders one frame and samples the front buffer. The self-test uses
    // this to prove the 3D view is drawing real content, not a black box.
    public (int Distinct, float Brightness) RenderStats()
    {
        MakeCurrent();
        using (var g = CreateGraphics()) OnPaint(new PaintEventArgs(g, ClientRectangle));
        GL.ReadBuffer(ReadBufferMode.Front);
        int w = Math.Max(1, Width), h = Math.Max(1, Height);
        var px = new byte[w * h * 4];
        GL.ReadPixels(0, 0, w, h, PixelFormat.Rgba, PixelType.UnsignedByte, px);
        var seen = new HashSet<int>();
        long sum = 0; int samples = 0;
        for (int i = 0; i + 3 < px.Length; i += 4 * 613)
        {
            seen.Add(px[i] << 16 | px[i + 1] << 8 | px[i + 2]);
            sum += px[i] + px[i + 1] + px[i + 2]; samples++;
        }
        return (seen.Count, samples == 0 ? 0 : sum / (3f * samples * 255f));
    }

    // Saves the front buffer as PNG - for debugging the preview itself.
    public void DebugSaveFrame(string path)
    {
        MakeCurrent();
        using (var g = CreateGraphics()) OnPaint(new PaintEventArgs(g, ClientRectangle));
        GL.ReadBuffer(ReadBufferMode.Front);
        int w = Math.Max(1, Width), h = Math.Max(1, Height);
        var px = new byte[w * h * 4];
        GL.ReadPixels(0, 0, w, h, PixelFormat.Rgba, PixelType.UnsignedByte, px);
        using var bmp = new Bitmap(w, h);
        for (int y = 0; y < h; y++)
            for (int x = 0; x < w; x++)
            {
                int src = ((h - 1 - y) * w + x) * 4;
                bmp.SetPixel(x, y, Color.FromArgb(px[src + 3], px[src], px[src + 1], px[src + 2]));
            }
        bmp.Save(path, System.Drawing.Imaging.ImageFormat.Png);
    }

    public float Health => _health;
    public (float X, float Y, float Z) PlayerPos => (_px, _py, _pz);

    // ---------- rendering ----------
    protected override void OnPaint(PaintEventArgs e)
    {
        base.OnPaint(e);
        if (Project == null) { GL.ClearColor(0.04f, 0.05f, 0.06f, 1); GL.Clear(ClearBufferMask.ColorBufferBit); SwapBuffers(); return; }
        MakeCurrent();
        GL.Viewport(0, 0, Width, Height);
        var focused = Project.Chunks.FirstOrDefault(chunk => chunk.Gx == FocusGx && chunk.Gy == FocusGy)
            ?? Project.Chunks.FirstOrDefault();
        var sky = SkyColour(focused);
        GL.ClearColor(sky.R / 255f, sky.G / 255f, sky.B / 255f, 1);
        GL.Enable(EnableCap.DepthTest);
        GL.Clear(ClearBufferMask.ColorBufferBit | ClearBufferMask.DepthBufferBit);

        var proj = Matrix4.CreatePerspectiveFieldOfView(MathHelper.DegreesToRadians(65),
            Math.Max(.1f, (float)Width / Math.Max(1, Height)), .05f, 600);
        GL.MatrixMode(MatrixMode.Projection); GL.LoadMatrix(ref proj);

        Matrix4 view;
        if (WalkMode)
        {
            var eye = new Vector3(_px, _pz + 1.62f, _py);
            var dir = new Vector3(-MathF.Sin(_wyaw) * MathF.Cos(_wpitch), MathF.Sin(_wpitch), -MathF.Cos(_wyaw) * MathF.Cos(_wpitch));
            view = Matrix4.LookAt(eye, eye + dir, Vector3.UnitY);
            _rayEye = eye; _rayFwd = dir;
            _rayRight = Vector3.Normalize(Vector3.Cross(dir, Vector3.UnitY));
            _rayUp = Vector3.Cross(_rayRight, dir);
        }
        else
        {
            // Frame the map to the panel, any aspect: distance comes from
            // the tighter of the two fields of view, so an ultrawide or
            // a tall skinny pane both get a map that fills the view.
            float aspect = Math.Max(.1f, (float)Width / Math.Max(1, Height));
            float fovY = MathHelper.DegreesToRadians(65);
            float fovX = 2 * MathF.Atan(MathF.Tan(fovY / 2) * aspect);
            float dist = Math.Clamp(18f / MathF.Sin(Math.Min(fovY, fovX) / 2), 22f, 120f) * _distMul;
            var c0 = Project.Chunks.Count > 0 ? Project.Chunks[0] : null;
            var target = new Vector3((c0?.Gx ?? 0) * 24 + 12, 0, (c0?.Gy ?? 0) * 24 + 12);
            var eye = new Vector3(
                target.X + MathF.Sin(_yaw) * MathF.Cos(_pitch) * dist,
                MathF.Sin(-_pitch) * dist + 4,
                target.Z + MathF.Cos(_yaw) * MathF.Cos(_pitch) * dist);
            _cam = eye;
            view = Matrix4.LookAt(eye, target, Vector3.UnitY);
            _rayEye = eye; _rayFwd = Vector3.Normalize(target - eye);
            _rayRight = Vector3.Normalize(Vector3.Cross(_rayFwd, Vector3.UnitY));
            _rayUp = Vector3.Cross(_rayRight, _rayFwd);
            _rayAspect = Math.Max(.1f, (float)Width / Math.Max(1, Height));
        }
        GL.MatrixMode(MatrixMode.Modelview); GL.LoadMatrix(ref view);
        GL.PolygonMode(TriangleFace.FrontAndBack, Wireframe ? PolygonMode.Line : PolygonMode.Fill);
        if (Wireframe) GL.Color3(0.55f, 0.9f, 0.85f);
        DrawScene();
        if (BuildMode && _ghostValid)
        {
            if (GhostPrefab != null)
                DrawPrefab(GhostPrefab, _ghostWorld.X, BuildPlaneZ, _ghostWorld.Z, BuildRotation, GhostSize.W, GhostSize.H, GhostSize.D);
            GL.PolygonMode(TriangleFace.FrontAndBack, PolygonMode.Line);
            Box(_ghostWorld.X, BuildPlaneZ + GhostSize.H / 2, _ghostWorld.Z,
                GhostSize.W, GhostSize.H, GhostSize.D, TextureFor(null, Color.FromArgb(240, 200, 110)), BuildRotation);
        }
        GL.PolygonMode(TriangleFace.FrontAndBack, PolygonMode.Fill);
        GL.Color3(1f, 1f, 1f);
        SwapBuffers();
    }

    // The edit view is intentionally lightweight, but it must still respect
    // the map's authored world mood.  Engine Shot remains the exact renderer;
    // this immediate backdrop makes night, wasteland and underground maps
    // legible while editing rather than showing one misleading black void.
    private static Color SkyColour(Chunk? chunk)
    {
        string name = chunk?.MapTail.Count > 3 ? Uri.UnescapeDataString(chunk.MapTail[3]) : "industrial_night";
        return name.ToLowerInvariant() switch
        {
            "brutal_wasteland" or "ashfall" => Color.FromArgb(101, 79, 57),
            "industrial_day" => Color.FromArgb(105, 132, 151),
            "mine" or "underground" => Color.FromArgb(30, 25, 20),
            _ => Color.FromArgb(9, 16, 29),
        };
    }

    private static Color EngineFixtureTint(Obj o)
    {
        string name = o.EngineFixtureName ?? "";
        if (name.Contains("tree", StringComparison.OrdinalIgnoreCase) || name.Contains("fern", StringComparison.OrdinalIgnoreCase) ||
            name.Contains("grass", StringComparison.OrdinalIgnoreCase) || name.Contains("bush", StringComparison.OrdinalIgnoreCase) ||
            name.Contains("plant", StringComparison.OrdinalIgnoreCase) || name.Contains("clover", StringComparison.OrdinalIgnoreCase))
            return Color.FromArgb(96, 132, 82);
        if (name.Contains("rock", StringComparison.OrdinalIgnoreCase) || name.Contains("ore", StringComparison.OrdinalIgnoreCase))
            return Color.FromArgb(111, 109, 101);
        if (name.Contains("wood", StringComparison.OrdinalIgnoreCase) || name.Contains("log", StringComparison.OrdinalIgnoreCase) || name.Contains("crate", StringComparison.OrdinalIgnoreCase))
            return Color.FromArgb(127, 91, 57);
        return Color.FromArgb(123, 137, 143);
    }

    private int TextureFor(string? url, Color tint)
    {
        string key = (url ?? "c") + "@" + tint.ToArgb();
        if (_textures.TryGetValue(key, out int id)) return id;
        id = GL.GenTexture();
        GL.BindTexture(TextureTarget.Texture2D, id);
        GL.TexParameter(TextureTarget.Texture2D, TextureParameterName.TextureMinFilter, (int)TextureMinFilter.Linear);
        GL.TexParameter(TextureTarget.Texture2D, TextureParameterName.TextureMagFilter, (int)TextureMagFilter.Linear);
        GL.TexParameter(TextureTarget.Texture2D, TextureParameterName.TextureWrapS, (int)TextureWrapMode.Repeat);
        GL.TexParameter(TextureTarget.Texture2D, TextureParameterName.TextureWrapT, (int)TextureWrapMode.Repeat);
        // The compatibility context is shared with WPF/WinForms.  Set the
        // fixed-function texture combine explicitly rather than inheriting
        // whichever state the host control last used.
        GL.TexEnv(TextureEnvTarget.TextureEnv, TextureEnvParameter.TextureEnvMode, (int)TextureEnvMode.Modulate);
        var image = url != null ? Blob?.TextureImage(url) : null;
        if (image != null && tint != Color.White)
        {
            // Painted finishes: same texture, multiplied colour.
            var tinted = new Bitmap(image.Width, image.Height);
            using (var g = Graphics.FromImage(tinted))
            {
                var cm = new System.Drawing.Imaging.ColorMatrix(new[]
                {
                    new[] { tint.R / 255f, 0, 0, 0, 0 },
                    new[] { 0, tint.G / 255f, 0, 0, 0 },
                    new[] { 0, 0, tint.B / 255f, 0, 0 },
                    new[] { 0, 0, 0, 1f, 0 },
                    new[] { 0, 0, 0, 0, 1f },
                });
                using var attrs = new System.Drawing.Imaging.ImageAttributes();
                attrs.SetColorMatrix(cm);
                g.DrawImage(image, new Rectangle(0, 0, tinted.Width, tinted.Height), 0, 0, image.Width, image.Height, GraphicsUnit.Pixel, attrs);
            }
            image.Dispose();
            image = tinted;
        }
        if (image is Bitmap bmp)
        {
            var data = bmp.LockBits(new Rectangle(0, 0, bmp.Width, bmp.Height),
                System.Drawing.Imaging.ImageLockMode.ReadOnly, System.Drawing.Imaging.PixelFormat.Format32bppArgb);
            GL.TexImage2D(TextureTarget.Texture2D, 0, PixelInternalFormat.Rgba, bmp.Width, bmp.Height, 0,
                PixelFormat.Bgra, PixelType.UnsignedByte, data.Scan0);
            bmp.UnlockBits(data);
        }
        else
        {
            var px = new byte[] { (byte)tint.B, (byte)tint.G, (byte)tint.R, 255 };
            GL.TexImage2D(TextureTarget.Texture2D, 0, PixelInternalFormat.Rgba, 1, 1, 0,
                PixelFormat.Bgra, PixelType.UnsignedByte, px);
        }
        _textures[key] = id;
        return id;
    }

    private void Box(float x, float z, float y, float w, float h, float d, int tex, float rotDeg = 0)
    {
        GL.PushMatrix();
        GL.Translate(x, z, y);
        if (rotDeg != 0) GL.Rotate(-rotDeg, 0, 1, 0);
        GL.Scale(w, h, d);
        if (!Wireframe) { GL.BindTexture(TextureTarget.Texture2D, tex); GL.Enable(EnableCap.Texture2D); }
        GL.Begin(PrimitiveType.Quads);
        // unit cube centred at origin, base at -0.5.. top +0.5 -> shifted by caller via z+h/2
        float[][] faces =
        {
            new[] { -.5f, -.5f, .5f, .5f, -.5f, .5f, .5f, .5f, .5f, -.5f, .5f, .5f },   // front (+z in GL = world +y)
            new[] { .5f, -.5f, -.5f, -.5f, -.5f, -.5f, -.5f, .5f, -.5f, .5f, .5f, -.5f },
            new[] { .5f, -.5f, .5f, .5f, -.5f, -.5f, .5f, .5f, -.5f, .5f, .5f, .5f },
            new[] { -.5f, -.5f, -.5f, -.5f, -.5f, .5f, -.5f, .5f, .5f, -.5f, .5f, -.5f },
            new[] { -.5f, .5f, .5f, .5f, .5f, .5f, .5f, .5f, -.5f, -.5f, .5f, -.5f },
            new[] { -.5f, -.5f, -.5f, .5f, -.5f, -.5f, .5f, -.5f, .5f, -.5f, -.5f, .5f },
        };
        // Minecraft shading: fixed brightness per face direction. No
        // lighting rig, no cost - and blocks suddenly read as 3D solids
        // instead of flat blobs.
        float[] shade = { .92f, .78f, .85f, .68f, 1.12f, .5f };
        for (int fi = 0; fi < faces.Length; fi++)
        {
            var f = faces[fi];
            GL.Color3(shade[fi], shade[fi], shade[fi]);
            GL.TexCoord2(0, 0); GL.Vertex3(f[0], f[1], f[2]);
            GL.TexCoord2(1, 0); GL.Vertex3(f[3], f[4], f[5]);
            GL.TexCoord2(1, 1); GL.Vertex3(f[6], f[7], f[8]);
            GL.TexCoord2(0, 1); GL.Vertex3(f[9], f[10], f[11]);
        }
        GL.Color3(1f, 1f, 1f);
        GL.End();
        if (!Wireframe) GL.Disable(EnableCap.Texture2D);
        GL.PopMatrix();
    }

    private static readonly float[][] CubeFaces =
    {
        new[] { -.5f, -.5f, .5f, .5f, -.5f, .5f, .5f, .5f, .5f, -.5f, .5f, .5f },
        new[] { .5f, -.5f, -.5f, -.5f, -.5f, -.5f, -.5f, .5f, -.5f, .5f, .5f, -.5f },
        new[] { .5f, -.5f, .5f, .5f, -.5f, -.5f, .5f, .5f, -.5f, .5f, .5f, .5f },
        new[] { -.5f, -.5f, -.5f, -.5f, -.5f, .5f, -.5f, .5f, .5f, -.5f, .5f, -.5f },
        new[] { -.5f, .5f, .5f, .5f, .5f, .5f, .5f, .5f, -.5f, -.5f, .5f, -.5f },
        new[] { -.5f, -.5f, -.5f, .5f, -.5f, -.5f, .5f, -.5f, .5f, -.5f, -.5f, .5f },
    };

    // A box whose FRONT face (+z local) carries one texture and every
    // other face another. Signs read forwards from the front and are
    // blank-backed from behind - never mirror writing.
    private void BoxFaced(float x, float z, float y, float w, float h, float d, int frontTex, int sideTex, float rotDeg = 0)
    {
        GL.PushMatrix();
        GL.Translate(x, z, y);
        if (rotDeg != 0) GL.Rotate(-rotDeg, 0, 1, 0);
        GL.Scale(w, h, d);
        float[] shade = { .98f, .6f, .85f, .68f, 1.05f, .5f };
        for (int fi = 0; fi < CubeFaces.Length; fi++)
        {
            var f = CubeFaces[fi];
            if (!Wireframe) { GL.BindTexture(TextureTarget.Texture2D, fi == 0 ? frontTex : sideTex); GL.Enable(EnableCap.Texture2D); }
            GL.Begin(PrimitiveType.Quads);
            GL.Color3(shade[fi], shade[fi], shade[fi]);
            GL.TexCoord2(0, 0); GL.Vertex3(f[0], f[1], f[2]);
            GL.TexCoord2(1, 0); GL.Vertex3(f[3], f[4], f[5]);
            GL.TexCoord2(1, 1); GL.Vertex3(f[6], f[7], f[8]);
            GL.TexCoord2(0, 1); GL.Vertex3(f[9], f[10], f[11]);
            GL.End();
            if (!Wireframe) GL.Disable(EnableCap.Texture2D);
        }
        GL.Color3(1f, 1f, 1f);
        GL.PopMatrix();
    }

    private MeshData? MeshFor(Prefab prefab)
    {
        if (_meshes.TryGetValue(prefab.Model, out var cached)) return cached;
        var bytes = Blob?.ForUrl(prefab.ModelUrl);
        var mesh = new MeshData();
        if (bytes != null && (prefab.Ext == ".obj" || (prefab.ModelUrl?.EndsWith(".obj") ?? false)))
        {
            var inv = System.Globalization.CultureInfo.InvariantCulture;
            var verts = new List<Vector3>();
            var uvs = new List<Vector2>();
            string material = "";
            foreach (var line in System.Text.Encoding.UTF8.GetString(bytes).Split('\n'))
            {
                var p = line.Split(' ', StringSplitOptions.RemoveEmptyEntries);
                if (p.Length == 0) continue;
                if (p[0] == "v" && p.Length >= 4)
                    verts.Add(new(float.Parse(p[1], inv), float.Parse(p[2], inv), float.Parse(p[3], inv)));
                else if (p[0] == "vt" && p.Length >= 3)
                    uvs.Add(new(float.Parse(p[1], inv), float.Parse(p[2], inv)));
                else if (p[0] == "usemtl" && p.Length >= 2)
                    material = string.Join(' ', p.Skip(1));
                else if (p[0] == "f" && p.Length >= 4)
                {
                    // OBJ faces can be polygons: fan-triangulate, and
                    // honour negative (relative) indices.
                    var face = new List<(Vector3 V, Vector2 T)>();
                    for (int i = 1; i < p.Length; i++)
                    {
                        var parts = p[i].Split('/');
                        if (!int.TryParse(parts[0], out int raw)) continue;
                        int vi = raw > 0 ? raw - 1 : verts.Count + raw;
                        if (vi < 0 || vi >= verts.Count) continue;
                        var t = Vector2.Zero;
                        if (parts.Length > 1 && int.TryParse(parts[1], out int ti) && ti > 0 && ti - 1 < uvs.Count)
                            t = uvs[ti - 1];
                        face.Add((verts[vi], t));
                    }
                    for (int k = 1; k + 1 < face.Count; k++)
                    {
                        var tri = new float[15];
                        (Vector3 V, Vector2 T)[] corners = { face[0], face[k], face[k + 1] };
                        for (int c = 0; c < 3; c++)
                        {
                            tri[c * 5] = corners[c].V.X; tri[c * 5 + 1] = corners[c].V.Y; tri[c * 5 + 2] = corners[c].V.Z;
                            tri[c * 5 + 3] = corners[c].T.X; tri[c * 5 + 4] = corners[c].T.Y;
                        }
                        mesh.Tris.Add(new MeshTri { Vertices = tri, Material = material });
                    }
                }
            }
            if (verts.Count > 0)
            {
                var min = verts[0]; var max = verts[0];
                foreach (var v in verts) { min = Vector3.ComponentMin(min, v); max = Vector3.ComponentMax(max, v); }
                mesh.Min = min;
                mesh.Size = Vector3.ComponentMax(max - min, new Vector3(.001f));
            }
        }
        _meshes[prefab.Model] = mesh;
        return mesh;
    }

    // Draws a real blob model fitted into a footprint, whatever units it
    // was baked in. Returns false when there is no usable mesh.
    private bool DrawPrefab(Prefab prefab, float wx, float baseZ, float wy, float rot, float w, float h, float d)
    {
        var mesh = MeshFor(prefab);
        if (mesh is not { Tris.Count: > 0 }) return false;
        // Authored model textures must be shown unmodified.  The old lavender
        // multiply was intended as a missing-material fallback, but it also
        // tinted every textured engine fixture and made the preview unlike the
        // game.  Keep the neutral fallback only when no diffuse map exists.
        bool hasTexture = !string.IsNullOrWhiteSpace(prefab.TextureUrl ?? prefab.Texture);
        int fallbackTex = TextureFor(prefab.TextureUrl ?? prefab.Texture,
            hasTexture ? Color.White : Color.FromArgb(170, 160, 175));
        GL.PushMatrix();
        GL.Translate(wx, baseZ, wy);
        GL.Rotate(-rot, 0, 1, 0);
        GL.Scale(w / mesh.Size.X, h / mesh.Size.Y, d / mesh.Size.Z);
        GL.Translate(-mesh.Min.X, -mesh.Min.Y, -mesh.Min.Z);
        if (!Wireframe) { GL.BindTexture(TextureTarget.Texture2D, fallbackTex); GL.Enable(EnableCap.Texture2D); }
        GL.Begin(PrimitiveType.Triangles);
        // Flat shading from real face normals: without this, models are
        // paper cut-outs - every triangle identical, no form at all.
        var lightDir = Vector3.Normalize(new Vector3(-.45f, .85f, .4f));
        string? activeMaterial = null;
        foreach (var triData in mesh.Tris)
        {
            var tri = triData.Vertices;
            if (!Wireframe && !string.Equals(activeMaterial, triData.Material, StringComparison.Ordinal))
            {
                activeMaterial = triData.Material;
                string? textureUrl = null;
                if (!string.IsNullOrWhiteSpace(activeMaterial))
                    prefab.MaterialUrls.TryGetValue(activeMaterial, out textureUrl);
                GL.BindTexture(TextureTarget.Texture2D,
                    TextureFor(textureUrl ?? prefab.TextureUrl ?? prefab.Texture,
                        textureUrl != null || hasTexture ? Color.White : Color.FromArgb(170, 160, 175)));
            }
            var a = new Vector3(tri[0], tri[1], tri[2]);
            var b = new Vector3(tri[5], tri[6], tri[7]);
            var c = new Vector3(tri[10], tri[11], tri[12]);
            var n = Vector3.Cross(b - a, c - a);
            float shade = n.LengthSquared > 0
                ? .34f + .66f * Math.Clamp(Vector3.Dot(Vector3.Normalize(n), lightDir), 0, 1)
                : .7f;
            GL.Color3(shade, shade, shade * 1.02f);
            for (int k = 0; k < 3; k++)
            { GL.TexCoord2(tri[k * 5 + 3], tri[k * 5 + 4]); GL.Vertex3(tri[k * 5], tri[k * 5 + 1], tri[k * 5 + 2]); }
        }
        GL.Color3(1f, 1f, 1f);
        GL.End();
        if (!Wireframe) GL.Disable(EnableCap.Texture2D);
        GL.PopMatrix();
        return true;
    }

    private int UploadBitmap(Bitmap bmp)
    {
        int id = GL.GenTexture();
        GL.BindTexture(TextureTarget.Texture2D, id);
        GL.TexParameter(TextureTarget.Texture2D, TextureParameterName.TextureMinFilter, (int)TextureMinFilter.Linear);
        GL.TexParameter(TextureTarget.Texture2D, TextureParameterName.TextureMagFilter, (int)TextureMagFilter.Linear);
        var data = bmp.LockBits(new Rectangle(0, 0, bmp.Width, bmp.Height),
            System.Drawing.Imaging.ImageLockMode.ReadOnly, System.Drawing.Imaging.PixelFormat.Format32bppArgb);
        GL.TexImage2D(TextureTarget.Texture2D, 0, PixelInternalFormat.Rgba, bmp.Width, bmp.Height, 0,
            PixelFormat.Bgra, PixelType.UnsignedByte, data.Scan0);
        bmp.UnlockBits(data);
        return id;
    }

    private readonly Dictionary<string, int> _signTex = new();

    // Signs you can READ: the title/subtitle rendered into a texture,
    // accent stripe and all - same words the game will show.
    private int SignTexture(Obj o)
    {
        string key = $"{o.Title}|{o.Subtitle}|{o.Accent}";
        if (_signTex.TryGetValue(key, out int id)) return id;
        using var bmp = new Bitmap(512, 192);
        using (var g = Graphics.FromImage(bmp))
        {
            g.Clear(Color.FromArgb(16, 21, 29));
            Color accent;
            try { accent = ColorTranslator.FromHtml(string.IsNullOrEmpty(o.Accent) ? "#d9a441" : o.Accent); }
            catch { accent = Color.FromArgb(217, 164, 65); }
            using var bar = new SolidBrush(accent);
            g.FillRectangle(bar, 0, 0, 14, 192);
            using var title = new Font("Segoe UI", 40f, FontStyle.Bold);
            using var sub = new Font("Segoe UI", 22f);
            g.DrawString(string.IsNullOrEmpty(o.Title) ? "SIGN" : o.Title, title, Brushes.White, 30, 26);
            if (!string.IsNullOrEmpty(o.Subtitle)) g.DrawString(o.Subtitle, sub, Brushes.LightSteelBlue, 32, 104);
        }
        id = UploadBitmap(bmp);
        _signTex[key] = id;
        return id;
    }

    private void CylinderY(float cx, float baseZ, float cy, float r, float h, int tex)
    {
        const int seg = 14;
        if (!Wireframe) { GL.BindTexture(TextureTarget.Texture2D, tex); GL.Enable(EnableCap.Texture2D); }
        GL.Begin(PrimitiveType.Quads);
        for (int i = 0; i < seg; i++)
        {
            float a0 = i * MathF.Tau / seg, a1 = (i + 1) * MathF.Tau / seg;
            float shade = .55f + .45f * MathF.Cos(a0 + .6f);
            GL.Color3(shade, shade, shade);
            GL.TexCoord2(i / (float)seg, 0); GL.Vertex3(cx + MathF.Cos(a0) * r, baseZ, cy + MathF.Sin(a0) * r);
            GL.TexCoord2((i + 1) / (float)seg, 0); GL.Vertex3(cx + MathF.Cos(a1) * r, baseZ, cy + MathF.Sin(a1) * r);
            GL.TexCoord2((i + 1) / (float)seg, 1); GL.Vertex3(cx + MathF.Cos(a1) * r, baseZ + h, cy + MathF.Sin(a1) * r);
            GL.TexCoord2(i / (float)seg, 1); GL.Vertex3(cx + MathF.Cos(a0) * r, baseZ + h, cy + MathF.Sin(a0) * r);
        }
        GL.End();
        GL.Begin(PrimitiveType.TriangleFan);
        GL.Color3(1.05f, 1.05f, 1.05f);
        GL.TexCoord2(.5, .5); GL.Vertex3(cx, baseZ + h, cy);
        for (int i = 0; i <= seg; i++)
        {
            float a = i * MathF.Tau / seg;
            GL.TexCoord2(.5 + MathF.Cos(a) * .5, .5 + MathF.Sin(a) * .5);
            GL.Vertex3(cx + MathF.Cos(a) * r, baseZ + h, cy + MathF.Sin(a) * r);
        }
        GL.End();
        GL.Color3(1f, 1f, 1f);
        if (!Wireframe) GL.Disable(EnableCap.Texture2D);
    }

    private void WedgeY(float cx, float baseZ, float cy, float w, float h, float d, int tex, float rotDeg)
    {
        GL.PushMatrix();
        GL.Translate(cx, baseZ, cy);
        if (rotDeg != 0) GL.Rotate(-rotDeg, 0, 1, 0);
        if (!Wireframe) { GL.BindTexture(TextureTarget.Texture2D, tex); GL.Enable(EnableCap.Texture2D); }
        float x = w / 2, z = d / 2;
        GL.Begin(PrimitiveType.Quads);
        // slope
        GL.Color3(.95f, .95f, .95f);
        GL.TexCoord2(0, 0); GL.Vertex3(-x, 0, -z);
        GL.TexCoord2(1, 0); GL.Vertex3(x, 0, -z);
        GL.TexCoord2(1, 1); GL.Vertex3(x, h, z);
        GL.TexCoord2(0, 1); GL.Vertex3(-x, h, z);
        // tall back
        GL.Color3(.7f, .7f, .7f);
        GL.TexCoord2(0, 0); GL.Vertex3(x, 0, z);
        GL.TexCoord2(1, 0); GL.Vertex3(-x, 0, z);
        GL.TexCoord2(1, 1); GL.Vertex3(-x, h, z);
        GL.TexCoord2(0, 1); GL.Vertex3(x, h, z);
        GL.End();
        GL.Begin(PrimitiveType.Triangles);
        GL.Color3(.82f, .82f, .82f);
        GL.Vertex3(-x, 0, -z); GL.Vertex3(-x, 0, z); GL.Vertex3(-x, h, z);
        GL.Vertex3(x, 0, z); GL.Vertex3(x, 0, -z); GL.Vertex3(x, h, z);
        GL.End();
        GL.Color3(1f, 1f, 1f);
        if (!Wireframe) GL.Disable(EnableCap.Texture2D);
        GL.PopMatrix();
    }

    // Architecture with actual shapes: an arch is posts and a lintel you
    // can walk under, stairs step, a column is round. Boxes are the last
    // resort, not the whole vocabulary.
    private void DrawPrim(Obj o, float wx, float wy)
    {
        int tex = TextureFor(null, Color.FromArgb(133, 126, 113));
        float b = o.Z, W = Math.Max(.3f, o.W), D = Math.Max(.3f, o.D), H = Math.Max(.3f, o.H);
        double rad = -o.Rotation * Math.PI / 180;
        (float X, float Y) Off(float dx, float dy) =>
            (wx + (float)(dx * Math.Cos(rad) - dy * Math.Sin(rad)), wy + (float)(dx * Math.Sin(rad) + dy * Math.Cos(rad)));
        void Piece(float dx, float dy, float baseOff, float w, float h, float d)
        {
            var (px, py) = Off(dx, dy);
            Box(px, b + baseOff + h / 2, py, w, h, d, tex, o.Rotation);
        }
        switch ((o.PrimKind ?? "box").ToLowerInvariant())
        {
            case "column": CylinderY(wx, b, wy, Math.Min(W, D) / 2, H, tex); break;
            case "roundel": CylinderY(wx, b, wy, W / 2, Math.Min(H, .3f), tex); break;
            case "dome":
                CylinderY(wx, b, wy, W / 2, H * .45f, tex);
                CylinderY(wx, b + H * .45f, wy, W * .36f, H * .32f, tex);
                CylinderY(wx, b + H * .77f, wy, W * .2f, H * .23f, tex);
                break;
            case "stairs": case "megastep":
            {
                int n = (o.PrimKind == "megastep") ? 3 : 7;
                for (int i = 0; i < n; i++)
                    Piece(0, -D / 2 + (i + .5f) * D / n, 0, W, H * (i + 1) / n, D / n);
                break;
            }
            case "ramp": case "hood": case "batter": case "strut": case "spout":
                WedgeY(wx, b, wy, W, H, D, tex, o.Rotation); break;
            case "arch": case "vault": case "doorframe":
            {
                float t = Math.Min(.22f, D);
                Piece(-W / 2 + t / 2, 0, 0, t, H, D);
                Piece(W / 2 - t / 2, 0, 0, t, H, D);
                Piece(0, 0, H - .38f, W, .38f, D);
                break;
            }
            case "window":
                Piece(-W / 2 + .09f, 0, 0, .18f, H, D);
                Piece(W / 2 - .09f, 0, 0, .18f, H, D);
                Piece(0, 0, H - .18f, W, .18f, D);
                Piece(0, 0, 0, W, .18f, D);
                break;
            case "canopy":
                Piece(0, 0, H - .16f, W, .16f, D);
                foreach (var (sx, sy) in new[] { (-1, -1), (1, -1), (-1, 1), (1, 1) })
                    Piece(sx * (W / 2 - .12f), sy * (D / 2 - .12f), 0, .16f, H - .16f, .16f);
                break;
            case "pilotis": case "quadpier":
                foreach (var (sx, sy) in new[] { (-1, -1), (1, -1), (-1, 1), (1, 1) })
                    Piece(sx * (W / 2 - .2f), sy * (D / 2 - .2f), 0, .24f, H - .2f, .24f);
                Piece(0, 0, H - .2f, W, .2f, D);
                break;
            case "railing":
                Piece(0, 0, H - .07f, W, .07f, .07f);
                Piece(0, 0, H * .55f, W, .05f, .05f);
                for (float fx = -W / 2; fx <= W / 2; fx += .8f) Piece(fx, 0, 0, .06f, H, .06f);
                break;
            case "fins":
            {
                int n = Math.Max(2, (int)(W / .7f));
                for (int i = 0; i < n; i++) Piece(-W / 2 + (i + .5f) * W / n, 0, 0, .13f, H, D);
                break;
            }
            case "corbel":
                Piece(0, 0, 0, W, H * .34f, D);
                Piece(0, 0, H * .34f, W * .72f, H * .33f, D * .72f);
                Piece(0, 0, H * .67f, W * .45f, H * .33f, D * .45f);
                break;
            case "bridge":
                Piece(0, 0, H - .25f, W, .25f, D);
                Piece(-W / 2 + .2f, 0, 0, .25f, H - .25f, .25f);
                Piece(W / 2 - .2f, 0, 0, .25f, H - .25f, .25f);
                break;
            case "lightwell":
                Piece(0, -D / 2 + .1f, 0, W, H, .2f);
                Piece(0, D / 2 - .1f, 0, W, H, .2f);
                Piece(-W / 2 + .1f, 0, 0, .2f, H, D);
                Piece(W / 2 - .1f, 0, 0, .2f, H, D);
                break;
            case "lattice":
            {
                var (px1, py1) = Off(0, 0);
                Box(px1, b + H / 2, py1, .14f, H, D, tex, o.Rotation + 32);
                Box(px1, b + H / 2, py1, .14f, H, D, tex, o.Rotation - 32);
                break;
            }
            case "slit": case "chase":
                Box(wx, b + H / 2, wy, Math.Min(W, .32f), H, D, tex, o.Rotation); break;
            default:
                Box(wx, b + H / 2, wy, W, H, D, tex, o.Rotation); break;
        }
    }

    public static Color GroundColor(int kind) => kind switch
    {
        0 => Color.FromArgb(93, 125, 60), 1 => Color.FromArgb(78, 112, 52), 2 => Color.FromArgb(109, 143, 69),
        3 => Color.FromArgb(125, 143, 74), 4 => Color.FromArgb(154, 125, 85), 5 => Color.FromArgb(91, 74, 51),
        6 => Color.FromArgb(141, 141, 132), 7 => Color.FromArgb(107, 86, 54), 8 => Color.FromArgb(134, 168, 96),
        _ => Color.FromArgb(100, 120, 70),
    };

    private void DrawScene()
    {
        if (Project == null) return;
        if (Project.TerrainEnabled)
        {
            foreach (var g in Project.Ground)
                Box((g.X1 + g.X2) / 2, Project.TerrainBase - .06f, (g.Y1 + g.Y2) / 2,
                    Math.Max(.5f, g.X2 - g.X1), .12f, Math.Max(.5f, g.Y2 - g.Y1), TextureFor(null, GroundColor(g.Kind)));
            foreach (var fl in Project.Flats)
                Box((fl.X1 + fl.X2) / 2, fl.Height - .07f, (fl.Y1 + fl.Y2) / 2,
                    Math.Max(.5f, fl.X2 - fl.X1), .14f, Math.Max(.5f, fl.Y2 - fl.Y1), TextureFor(null, Color.FromArgb(141, 141, 132)));
        }
        foreach (var chunk in Project.Chunks)
        {
            if (FocusSet && (Math.Abs(chunk.Gx - FocusGx) > 1 || Math.Abs(chunk.Gy - FocusGy) > 1)) continue;
            float ox = chunk.Gx * 24, oy = chunk.Gy * 24;
            int wallTex = SurfaceTex(chunk, "wall");
            int floorTexS = SurfaceTex(chunk, "floor");
            int ceilTex = SurfaceTex(chunk, "ceiling");
            foreach (var layer in chunk.Layers)
            {
                for (int y = 0; y < 24 && y < layer.Rows.Count; y++)
                {
                    string row = layer.Rows[y];
                    for (int x = 0; x < 24 && x < row.Length; x++)
                    {
                        char q = row[x];
                        if (q == '_') continue;
                        string? matPath = layer.Materials.TryGetValue($"{x},{y}", out string? mp) ? mp : null;
                        int tex = matPath != null ? TextureFor(matPath, Color.White) : q == '#' ? wallTex : floorTexS;
                        if (q == '#')
                            Box(ox + x + .5f, layer.Z + layer.CeilingHeight / 2, oy + y + .5f, 1, layer.CeilingHeight, 1, tex);
                        else
                        {
                            Box(ox + x + .5f, layer.Z - .04f, oy + y + .5f, 1, .08f, 1, tex);
                            // Ceilings only when you're inside walking -
                            // from orbit they'd hide the rooms you built.
                            if (WalkMode && q == '.')
                                Box(ox + x + .5f, layer.Z + layer.CeilingHeight + .04f, oy + y + .5f, 1, .08f, 1, ceilTex);
                        }
                    }
                }
            }
            foreach (var o in chunk.Objects)
            {
                float wx = ox + o.X, wy = oy + o.Y;
                switch (o.Type)
                {
                    case "asset":
                    {
                        var prefab = Blob?.Prefabs.FirstOrDefault(p => p.Model == o.Model)
                            ?? (string.IsNullOrWhiteSpace(o.EngineFixtureName) ? null
                                : Blob?.Prefabs.FirstOrDefault(p =>
                                    string.Equals(Path.GetFileNameWithoutExtension(p.Model), o.EngineFixtureName, StringComparison.OrdinalIgnoreCase)));
                        if (prefab == null || !DrawPrefab(prefab, wx, o.Z, wy, o.Rotation, o.W, o.H, o.D))
                            Box(wx, o.Z + o.H / 2, wy, o.W, o.H, o.D,
                                TextureFor(o.Texture, o.EngineFixture >= 0 ? EngineFixtureTint(o) : Color.FromArgb(143, 113, 158)), o.Rotation);
                        break;
                    }
                    case "light":
                    {
                        // A little glowing fitting in the lamp's own colour.
                        Box(wx, o.Z, wy, .34f, .12f, .34f, TextureFor(null, LightColor(o)), o.Rotation);
                        break;
                    }
                    case "sign":
                        BoxFaced(wx, o.Z, wy, Math.Max(.3f, o.W), Math.Max(.2f, o.H), .08f,
                            SignTexture(o), TextureFor(null, Color.FromArgb(24, 29, 38)), o.Rotation);
                        break;
                    case "prim":
                        DrawPrim(o, wx, wy);
                        break;
                    case "door":
                    {
                        float open = _doorOpen.TryGetValue(o, out float v) ? v : 0;
                        Box(wx + open * o.W * .9f, o.Z + 1.1f, wy, o.W, 2.2f, .15f, TextureFor(null, Color.FromArgb(70, 119, 137)), o.Rotation);
                        break;
                    }
                    case "spawn":
                    {
                        // People and creatures are the real actor models,
                        // not coloured blocks - same meshes the game spawns.
                        string[] candidates = (o.SpawnKind ?? "") switch
                        {
                            "Player" => new[] { "Investigator" },
                            "Scientist" => new[] { "Scientist" },
                            "FemaleScientist" => new[] { "Female Scientist" },
                            "FemaleDoctor" => new[] { "Female Doctor" },
                            "SeniorScientist" => new[] { "Senior Scientist" },
                            "LabTech" => new[] { "Lab Tech" },
                            "Security" => new[] { "Assault", "Heavy" },
                            "Worker" or "Cargo" => new[] { "Freight Worker" },
                            "Investigator" => new[] { "Investigator" },
                            "Creature" or "Hostile" => (o.Species ?? "Huntsman") switch
                            {
                                "Wasp" => new[] { "Flying Creature" },
                                "Brute" => new[] { "Scissor Fiend" },
                                "Warden" or "Mutant" => new[] { "Mutant (Infected)" },
                                _ => new[] { "Huntsman" },
                            },
                            _ => new[] { "Freight Worker" },
                        };
                        var actor = Blob?.Prefabs.FirstOrDefault(p => candidates.Contains(p.Name));
                        if (actor == null || !DrawPrefab(actor, wx, o.Z, wy, o.Rotation, .7f, Math.Max(1.2f, o.H), .7f))
                        {
                            var col = o.SpawnKind switch
                            {
                                "Player" => Color.FromArgb(118, 200, 159),
                                "Hostile" or "Creature" => Color.FromArgb(208, 121, 104),
                                "Security" => Color.FromArgb(125, 167, 217),
                                _ => Color.FromArgb(183, 183, 173),
                            };
                            Box(wx, o.Z + o.H / 2, wy, .5f, o.H, .5f, TextureFor(null, col), o.Rotation);
                        }
                        break;
                    }
                    case "hazard":
                        Box(wx, o.Z + o.H / 2, wy, o.W, o.H, o.D, TextureFor(null, Color.FromArgb(160, 70, 50)), o.Rotation);
                        break;
                    case "terminal":
                        Box(wx, o.Z + o.H / 2, wy, o.W, o.H, o.D, TextureFor(null, Color.FromArgb(70, 110, 90)), o.Rotation);
                        break;
                    case "stairs":
                        Box(wx, o.Z + o.H / 2, wy, o.W, o.H, o.D, TextureFor(null, Color.FromArgb(105, 112, 119)), o.Rotation);
                        break;
                    default:
                        Box(wx, o.Z + o.H / 2, wy, o.W, o.H, o.D, TextureFor(null, Color.FromArgb(101, 117, 105)), o.Rotation);
                        break;
                }
            }
        }
    }

    public static Color LightColor(Obj o)
    {
        double k = Math.Clamp(o.Kelvin, 1000, 12000) / 100.0;
        double r = k <= 66 ? 255 : 329.698727446 * Math.Pow(k - 60, -.1332047592);
        double g = k <= 66 ? 99.4708025861 * Math.Log(k) - 161.1195681661 : 288.1221695283 * Math.Pow(k - 60, -.0755148492);
        double b = k >= 66 ? 255 : k <= 19 ? 0 : 138.5177312231 * Math.Log(k - 10) - 305.0447927307;
        int cr = Math.Clamp((int)r, 0, 255), cg = Math.Clamp((int)g, 0, 255), cb = Math.Clamp((int)b, 0, 255);
        if (o.Gel is { Length: >= 7 })
        {
            cr = cr * Convert.ToInt32(o.Gel.Substring(1, 2), 16) / 255;
            cg = cg * Convert.ToInt32(o.Gel.Substring(3, 2), 16) / 255;
            cb = cb * Convert.ToInt32(o.Gel.Substring(5, 2), 16) / 255;
        }
        return Color.FromArgb(cr, cg, cb);
    }
}
