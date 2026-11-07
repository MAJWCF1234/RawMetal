using System.Diagnostics;
using System.Text.Json;

namespace RawMetalCampaignEditor;

public sealed partial class EditorForm
{
    // ---------------- inspector ----------------
    private void BuildInspector(Obj? o)
    {
        _inspector.Controls.Clear();
        if (o == null || Chunk == null)
        {
            _inspector.Controls.Add(new Label
            {
                Text = "Select something on the plan to edit it here.\n\nLights get colour and warmth sliders, signs get text, people get roles - everything updates the 3D view live.",
                AutoSize = true, ForeColor = Color.Silver, Location = new Point(10, 10), MaximumSize = new Size(290, 0),
            });
            return;
        }
        int y = 8;
        Label Head(string t) { var l = new Label { Text = t, ForeColor = Color.FromArgb(212, 168, 83), Font = new Font("Segoe UI", 11f, FontStyle.Bold), Location = new Point(10, y), AutoSize = true }; _inspector.Controls.Add(l); y += 26; return l; }
        void Row(string label, Control c)
        {
            _inspector.Controls.Add(new Label { Text = label, Location = new Point(10, y + 3), AutoSize = true, ForeColor = Color.Silver });
            c.Location = new Point(120, y); c.Width = 185;
            _inspector.Controls.Add(c); y += 32;
        }
        NumericUpDown Num(float v, float min, float max, int dec = 2)
        {
            var n = new NumericUpDown { Minimum = (decimal)min, Maximum = (decimal)max, Value = (decimal)Math.Clamp(v, min, max), DecimalPlaces = dec, Increment = 0.25m };
            return n;
        }

        Head(o.Name.Length > 0 ? o.Name : o.Type);
        var name = new TextBox { Text = o.Name };
        name.TextChanged += (_, _) => { o.Name = name.Text; _plan.Invalidate(); };
        Row("Name", name);

        var nx = Num(o.X, -10, 40); nx.ValueChanged += (_, _) => { o.X = (float)nx.Value; AfterEdit(); }; Row("X", nx);
        var ny = Num(o.Y, -10, 40); ny.ValueChanged += (_, _) => { o.Y = (float)ny.Value; AfterEdit(); }; Row("Y", ny);
        var nz = Num(o.Z, -5, 60); nz.ValueChanged += (_, _) => { o.Z = (float)nz.Value; AfterEdit(); }; Row("Height (Z)", nz);
        var nw = Num(o.W, .1f, 24); nw.ValueChanged += (_, _) => { o.W = (float)nw.Value; AfterEdit(); }; Row("Width", nw);
        var nd = Num(o.D, .1f, 24); nd.ValueChanged += (_, _) => { o.D = (float)nd.Value; AfterEdit(); }; Row("Depth", nd);
        var nh = Num(o.H, .1f, 20); nh.ValueChanged += (_, _) => { o.H = (float)nh.Value; AfterEdit(); }; Row("Tall (H)", nh);
        var nr = Num(o.Rotation, 0, 360, 0); nr.ValueChanged += (_, _) => { o.Rotation = (float)nr.Value; AfterEdit(); }; Row("Rotation°", nr);

        if (o.Type == "light")
        {
            Head("Light");
            var mount = new ComboBox { DropDownStyle = ComboBoxStyle.DropDownList };
            mount.Items.AddRange(new object[] { "Ceiling", "Wall", "Bare" });
            mount.SelectedItem = o.LampMount ?? "Ceiling";
            mount.SelectedIndexChanged += (_, _) => { o.LampMount = mount.SelectedItem?.ToString(); AfterEdit(); };
            Row("Mounted", mount);
            var kelvin = new ComboBox { DropDownStyle = ComboBoxStyle.DropDownList };
            kelvin.Items.AddRange(new object[] { "🕯 Candlelight warm", "🏠 Cosy house warm", "🏢 Office white", "🌙 Cold moonlight" });
            kelvin.SelectedIndex = o.Kelvin switch { <= 2850 => 0, <= 3600 => 1, <= 4800 => 2, _ => 3 };
            kelvin.SelectedIndexChanged += (_, _) => { o.Kelvin = new[] { 2700, 3200, 4000, 5600 }[kelvin.SelectedIndex]; AfterEdit(); };
            Row("Warmth", kelvin);
            var inten = new TrackBar { Minimum = 0, Maximum = 30, Value = Math.Clamp((int)(o.Intensity * 10), 0, 30), Width = 185 };
            inten.Scroll += (_, _) => { o.Intensity = inten.Value / 10f; };
            Row("Brightness", inten);
            var range = new TrackBar { Minimum = 1, Maximum = 24, Value = Math.Clamp((int)o.Range, 1, 24), Width = 185 };
            range.Scroll += (_, _) => { o.Range = range.Value; };
            Row("Reach (m)", range);
            var gel = new Button { Text = string.IsNullOrEmpty(o.Gel) ? "Gel colour: none" : $"Gel colour: {o.Gel}", FlatStyle = FlatStyle.Flat, BackColor = Color.FromArgb(34, 44, 56), ForeColor = Color.White };
            gel.Click += (_, _) =>
            {
                using var dlg = new ColorDialog();
                if (dlg.ShowDialog(this) == DialogResult.OK)
                { o.Gel = $"#{dlg.Color.R:X2}{dlg.Color.G:X2}{dlg.Color.B:X2}"; BuildInspector(o); AfterEdit(); }
            };
            Row("Colour gel", gel);
        }
        if (o.Type == "sign")
        {
            Head("Sign text");
            var t = new TextBox { Text = o.Title ?? "" }; t.TextChanged += (_, _) => { o.Title = t.Text; AfterEdit(); }; Row("Title", t);
            var s = new TextBox { Text = o.Subtitle ?? "" }; s.TextChanged += (_, _) => { o.Subtitle = s.Text; AfterEdit(); }; Row("Subtitle", s);
            var a = new TextBox { Text = o.Accent ?? "" }; a.TextChanged += (_, _) => { o.Accent = a.Text; AfterEdit(); }; Row("Accent #hex", a);
        }
        if (o.Type == "prim")
        {
            Head("Architecture");
            var kind = new ComboBox { DropDownStyle = ComboBoxStyle.DropDownList };
            kind.Items.AddRange(Rmc.PrimKinds); kind.SelectedItem = o.PrimKind ?? "arch";
            kind.SelectedIndexChanged += (_, _) => { o.PrimKind = kind.SelectedItem?.ToString(); AfterEdit(); };
            Row("Kind", kind);
            var mat = Num(o.Material, 0, 99, 0); mat.ValueChanged += (_, _) => { o.Material = (int)mat.Value; AfterEdit(); }; Row("Material #", mat);
        }
        if (o.Type == "hazard")
        {
            Head("Hazard");
            var kind = new ComboBox { DropDownStyle = ComboBoxStyle.DropDownList };
            kind.Items.AddRange(new object[] { "Electrical", "Steam", "Crusher", "Toxic", "Fire", "FallingDebris", "Pressure", "Anomaly" });
            kind.SelectedItem = o.Kind ?? "Electrical";
            kind.SelectedIndexChanged += (_, _) => { o.Kind = kind.SelectedItem?.ToString(); AfterEdit(); };
            Row("Kind", kind);
        }
        if (o.Type == "spawn")
        {
            Head("Person / creature");
            var role = new ComboBox { DropDownStyle = ComboBoxStyle.DropDownList };
            role.Items.AddRange(new object[] { "Player", "Worker", "Scientist", "FemaleScientist", "FemaleDoctor", "LabTech", "Security", "Cargo", "Cleanup", "Investigator", "Hostile", "Creature" });
            role.SelectedItem = o.SpawnKind ?? "Worker";
            role.SelectedIndexChanged += (_, _) => { o.SpawnKind = role.SelectedItem?.ToString(); o.Name = o.SpawnKind ?? o.Name; AfterEdit(); };
            Row("Role", role);
            var species = new ComboBox { DropDownStyle = ComboBoxStyle.DropDownList };
            species.Items.AddRange(Rmc.CreatureSpecies); species.SelectedItem = o.Species ?? "Brute";
            species.SelectedIndexChanged += (_, _) => { o.Species = species.SelectedItem?.ToString(); AfterEdit(); };
            Row("Creature", species);
            var beh = new ComboBox { DropDownStyle = ComboBoxStyle.DropDownList };
            beh.Items.AddRange(new object[] { "civilian", "guard", "stand" }); beh.SelectedItem = o.Behaviour ?? "civilian";
            beh.SelectedIndexChanged += (_, _) => { o.Behaviour = beh.SelectedItem?.ToString(); AfterEdit(); };
            Row("Behaviour", beh);
            var armed = new ComboBox { DropDownStyle = ComboBoxStyle.DropDownList };
            armed.Items.AddRange(new object[] { "none", "handgun", "shotgun" }); armed.SelectedItem = o.Armed ?? "none";
            armed.SelectedIndexChanged += (_, _) => { o.Armed = armed.SelectedItem?.ToString(); AfterEdit(); };
            Row("Armed", armed);
        }

        y += 8;
        var del = new Button { Text = "Delete", Location = new Point(10, y), Width = 140, Height = 32, FlatStyle = FlatStyle.Flat, BackColor = Color.FromArgb(120, 50, 40), ForeColor = Color.White };
        del.Click += (_, _) => { Chunk.Objects.Remove(o); _plan.Selected = null; BuildInspector(null); AfterEdit(); };
        var dup = new Button { Text = "Duplicate", Location = new Point(160, y), Width = 140, Height = 32, FlatStyle = FlatStyle.Flat, BackColor = Color.FromArgb(34, 44, 56), ForeColor = Color.White };
        dup.Click += (_, _) =>
        {
            var copy = JsonSerializer.Deserialize<Obj>(JsonSerializer.Serialize(o))!;
            copy.Id = Guid.NewGuid().ToString("N")[..8]; copy.X += 1; copy.Y += 1;
            Chunk.Objects.Add(copy); _plan.Selected = copy; BuildInspector(copy); AfterEdit();
        };
        _inspector.Controls.Add(del); _inspector.Controls.Add(dup);
    }

    // ---------------- project / world files ----------------
    private void OpenProject()
    {
        using var dlg = new OpenFileDialog { Filter = "Editor project (*.json)|*.json|All files|*.*" };
        if (dlg.ShowDialog(this) != DialogResult.OK) return;
        _project = JsonSerializer.Deserialize<Project>(File.ReadAllText(dlg.FileName)) ?? NewBlankProject();
        _chunkIndex = 0; _layerIndex = 0;
        LoadIntoViews();
        SetStatus("Project opened.");
    }

    private void SaveProject()
    {
        using var dlg = new SaveFileDialog { Filter = "Editor project (*.json)|*.json", FileName = "campaign-project.json" };
        if (dlg.ShowDialog(this) != DialogResult.OK) return;
        File.WriteAllText(dlg.FileName, JsonSerializer.Serialize(_project, new JsonSerializerOptions { WriteIndented = true }));
        SetStatus("Project saved - reopen it any time to keep editing.");
    }

    private void OpenRmc()
    {
        using var dlg = new OpenFileDialog { Filter = "RawMetal campaign (*.rmc;*.txt)|*.rmc;*.txt|All files|*.*" };
        if (dlg.ShowDialog(this) != DialogResult.OK) return;
        _project = Rmc.Import(File.ReadAllText(dlg.FileName), _maps);
        if (_project.Chunks.Count == 0) _project = NewBlankProject();
        _chunkIndex = 0; _layerIndex = 0;
        LoadIntoViews();
        SetStatus("World file imported. Note: some engine-only pieces (scripts, stairs detail) stay in the file as carried records.");
    }

    private void ExportRmc()
    {
        using var dlg = new SaveFileDialog { Filter = "RawMetal campaign (*.rmc)|*.rmc", FileName = $"{_project.Name}.rmc" };
        if (dlg.ShowDialog(this) != DialogResult.OK) return;
        var warnings = new List<string>();
        File.WriteAllText(dlg.FileName, Rmc.Export(_project, _project.Name, _maps, warnings));
        _exported = true; UpdateChecklist();
        SetStatus(warnings.Count == 0 ? "Exported .rmc - drop it in the game's 'custom maps' folder and play."
            : $"Exported with {warnings.Count} note(s): {warnings[0]}");
    }

    private string ExportTempRmc()
    {
        var dir = Path.Combine(Path.GetTempPath(), "RawMetalEditor");
        Directory.CreateDirectory(dir);
        var path = Path.Combine(dir, "editor-preview.rmc");
        var warnings = new List<string>();
        File.WriteAllText(path, Rmc.Export(_project, _project.Name, _maps, warnings));
        return path;
    }

    // ---------------- the real engine, on demand ----------------
    private string? FindEngine()
    {
        var candidates = new List<string>
        {
            Path.Combine(AppContext.BaseDirectory, "DepthWorks.exe"),
            Path.Combine(AppContext.BaseDirectory, "RawMetal.exe"),
            Path.Combine(Directory.GetCurrentDirectory(), "RawMetal.exe"),
            Path.Combine(Directory.GetCurrentDirectory(), "DepthWorks.exe"),
            @"n:\rawmetal\RawMetal.exe",
        };
        return candidates.FirstOrDefault(File.Exists);
    }

    private async void EngineShot()
    {
        var engine = FindEngine();
        if (engine == null) { SetStatus("Could not find DepthWorks.exe / RawMetal.exe next to the editor or in the repo."); return; }
        var player = _project.Chunks.SelectMany(c => c.Objects.Where(o => o.Type == "spawn" && o.SpawnKind == "Player")
            .Select(o => (Chunk: c, Obj: o))).FirstOrDefault();
        if (player.Obj == null) { SetStatus("Engine shot needs a Player Start to stand the camera on."); return; }
        SetStatus("Asking the real engine for a render... (the editor stays live)");
        var rmc = ExportTempRmc();
        var listPath = Path.Combine(Path.GetTempPath(), "RawMetalEditor", "views.txt");
        int mapIndex = Math.Max(0, _project.Chunks.OrderBy(c => c.Number).ToList().IndexOf(player.Chunk));
        float wx = player.Chunk.Gx * 24 + player.Obj.X, wy = player.Chunk.Gy * 24 + player.Obj.Y;
        File.WriteAllText(listPath, $"editorview {mapIndex} {wx:0.##} {wy:0.##} 0 0 1.7\n");
        var engineDir = Path.GetDirectoryName(engine)!;
        try
        {
            await Task.Run(() =>
            {
                using var proc = Process.Start(new ProcessStartInfo(engine)
                {
                    Arguments = $"--zone-pack=\"{rmc}\" --zone-shots=\"{listPath}\"",
                    WorkingDirectory = engineDir, UseShellExecute = false, CreateNoWindow = true,
                })!;
                proc.WaitForExit(60000);
            });
            var ppm = Path.Combine(engineDir, ".build", "shot", "editorview.ppm");
            var img = File.Exists(ppm) ? LoadPpm(ppm) : null;
            if (img == null) { SetStatus("Engine ran but produced no shot - check the map has a floor under the Player Start."); return; }
            _playtested = true; UpdateChecklist();
            var form = new Form { Text = "Real engine render - this is exactly what players see", Width = 1000, Height = 620, BackColor = Color.Black };
            form.Controls.Add(new PictureBox { Dock = DockStyle.Fill, Image = img, SizeMode = PictureBoxSizeMode.Zoom });
            form.Show(this);
            SetStatus("That's the real game rendering your map - lighting, bloom and all.");
        }
        catch (Exception ex) { SetStatus($"Engine shot failed: {ex.Message}"); }
    }

    private void TestInEngine()
    {
        var engine = FindEngine();
        if (engine == null) { SetStatus("Could not find the game exe."); return; }
        var engineDir = Path.GetDirectoryName(engine)!;
        var mapsDir = Path.Combine(engineDir, "custom maps");
        Directory.CreateDirectory(mapsDir);
        var target = Path.Combine(mapsDir, $"{_project.Name}.rmc");
        var warnings = new List<string>();
        File.WriteAllText(target, Rmc.Export(_project, _project.Name, _maps, warnings));
        Process.Start(new ProcessStartInfo(engine) { WorkingDirectory = engineDir, UseShellExecute = true });
        _playtested = true; UpdateChecklist();
        SetStatus($"Game launched. In the game: Custom Maps -> {_project.Name}. (Installed to {target})");
    }

    private static Image? LoadPpm(string path)
    {
        var bytes = File.ReadAllBytes(path);
        int pos = 0;
        string Next()
        {
            while (pos < bytes.Length && (bytes[pos] is (byte)' ' or (byte)'\n' or (byte)'\r' or (byte)'\t')) pos++;
            int start = pos;
            while (pos < bytes.Length && bytes[pos] is not ((byte)' ' or (byte)'\n' or (byte)'\r' or (byte)'\t')) pos++;
            return System.Text.Encoding.ASCII.GetString(bytes, start, pos - start);
        }
        if (Next() != "P6") return null;
        int w = int.Parse(Next()), h = int.Parse(Next());
        _ = Next(); pos++; // maxval + single whitespace
        var bmp = new Bitmap(w, h);
        var data = bmp.LockBits(new Rectangle(0, 0, w, h), System.Drawing.Imaging.ImageLockMode.WriteOnly, System.Drawing.Imaging.PixelFormat.Format32bppArgb);
        var row = new byte[w * 4];
        for (int y = 0; y < h; y++)
        {
            for (int x = 0; x < w; x++)
            {
                int src = pos + (y * w + x) * 3;
                row[x * 4] = bytes[src + 2]; row[x * 4 + 1] = bytes[src + 1]; row[x * 4 + 2] = bytes[src]; row[x * 4 + 3] = 255;
            }
            System.Runtime.InteropServices.Marshal.Copy(row, 0, data.Scan0 + y * data.Stride, row.Length);
        }
        bmp.UnlockBits(data);
        return bmp;
    }

    // Headless smoke test: sample -> 3D paint -> walk tick -> engine-format
    // export. Writes selftest-ok.txt and exits; used so GL regressions fail
    // loudly in scripts instead of in front of the artist.
    public void RunSelfTest()
    {
        Shown += async (_, _) =>
        {
            try
            {
                LoadSample();
                SetViewMode("dual");
                _gl.Refresh();
                try { Directory.CreateDirectory(@"n:\rawmetal\.build"); _gl.DebugSaveFrame(@"n:\rawmetal\.build\editor-frame.png"); } catch { }
                var (distinct, brightness) = _gl.RenderStats();
                if (distinct < 8 || brightness <= 0.001f)
                    throw new InvalidOperationException($"3D view looks blank (distinct={distinct}, brightness={brightness:0.###})");
                SetViewMode("3d");
                _gl.EnterWalk();
                await Task.Delay(400);
                var warnings = new List<string>();
                var text = Rmc.Export(_project, _project.Name, _maps, warnings);
                if (!text.Contains("CAMPAIGN|") || !text.Contains("LIGHT|"))
                    throw new InvalidOperationException("export missing records");
                File.WriteAllText("selftest-ok.txt",
                    $"3D + walk + export OK, {text.Split('\n').Length} lines, {warnings.Count} warnings");
            }
            catch (Exception ex)
            {
                File.WriteAllText("selftest-fail.txt", ex.ToString());
            }
            finally { Application.Exit(); }
        };
    }

    // ---------------- sample building ----------------
    private void LoadSample()
    {
        var p = new Project { Name = "Sample Facility" };
        var chunk = new Chunk { Number = 1, Name = "Pump Annex", SurfaceWall = 0, SurfaceFloor = 0, SurfaceCeiling = 0 };
        var layer = BlankLayer("Ground Floor", 0);
        for (int y = 0; y < 24; y++)
        {
            var row = new char[24];
            for (int x = 0; x < 24; x++)
                row[x] = x is 0 or 23 || y is 0 or 23 || x == 12 ? '#' : '.';
            layer.Rows[y] = new string(row);
        }
        chunk.Layers.Add(layer);
        string lid = layer.Id;
        Obj O(Obj o) { o.LayerId = lid; chunk.Objects.Add(o); return o; }

        O(new Obj { Type = "spawn", SpawnKind = "Player", Name = "Player Start", X = 3, Y = 3, Z = 0, H = 1.8f, W = .6f, D = .6f });
        O(new Obj { Type = "door", Name = "door", X = 12, Y = 10, TileX = 12, TileY = 10, W = .95f, D = .25f, H = 2.2f });
        O(new Obj { Type = "light", Name = "Warm work light", X = 6, Y = 6, Z = 2.8f, LampMount = "Ceiling", Kelvin = 3200, Intensity = 1, Range = 8 });
        O(new Obj { Type = "light", Name = "Cold lab light", X = 18, Y = 6, Z = 2.8f, LampMount = "Ceiling", Kelvin = 5600, Intensity = 1, Range = 8 });
        O(new Obj { Type = "light", Name = "Alarm light", X = 18, Y = 18, Z = 2.8f, LampMount = "Wall", Kelvin = 4000, Intensity = .9f, Range = 7, Gel = "#FF2A1A" });
        O(new Obj { Type = "sign", Name = "Sign", X = 12, Y = 8.6f, Z = 1.8f, W = 1.8f, H = .7f, Title = "PUMP ANNEX", Subtitle = "mind the steam", Accent = "#d9a441" });
        O(new Obj { Type = "prim", Name = "Arch", X = 6, Y = 14, Z = 0, PrimKind = "arch", Material = 42, W = 3, D = 1, H = 3 });
        O(new Obj { Type = "prim", Name = "Column", X = 18, Y = 13, Z = 0, PrimKind = "column", Material = 40, W = 1, D = 1, H = 3 });
        O(new Obj { Type = "terminal", Name = "Terminal", X = 3, Y = 20, Z = 0, W = 1, D = .8f, H = 1.3f });
        O(new Obj { Type = "hazard", Name = "Live cables", X = 20, Y = 20, Z = 0, Kind = "Electrical", W = 2, D = 2, H = 1 });
        O(new Obj { Type = "spawn", SpawnKind = "Scientist", Name = "Scientist", X = 5, Y = 5, Z = 0, H = 1.8f, W = .6f, D = .6f, Behaviour = "civilian", Armed = "none" });
        O(new Obj { Type = "spawn", SpawnKind = "Security", Name = "Guard", X = 18, Y = 4, Z = 0, H = 1.8f, W = .6f, D = .6f, Behaviour = "guard", Armed = "handgun" });
        O(new Obj { Type = "spawn", SpawnKind = "Creature", Name = "Something in the dark", X = 19, Y = 17, Z = 0, H = 2.2f, W = .8f, D = .8f, Species = "Brute" });

        var gen = _blob?.Prefabs.FirstOrDefault(pf => pf.Name.Contains("generator", StringComparison.OrdinalIgnoreCase));
        if (gen != null)
            O(new Obj { Type = "asset", Name = gen.Name, Model = gen.Model, Texture = gen.TextureUrl ?? gen.Texture, X = 6, Y = 19, Z = 0, W = gen.W, D = gen.D, H = gen.H });

        p.Chunks.Add(chunk);
        _project = p; _chunkIndex = 0; _layerIndex = 0;
        LoadIntoViews();
        SetStatus("Sample loaded. Paint on it, walk it (▶ WALK), then 📸 ENGINE SHOT to see the real render.");
    }
}
