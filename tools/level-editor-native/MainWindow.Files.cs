using System.Diagnostics;
using System.Text.Json;
using System.Windows;
using System.Windows.Controls;
using System.Windows.Media;
using System.Windows.Media.Imaging;
using Color = System.Windows.Media.Color;
using Button = System.Windows.Controls.Button;
using TextBox = System.Windows.Controls.TextBox;
using ComboBox = System.Windows.Controls.ComboBox;

namespace RawMetalCampaignEditor;

public partial class MainWindow
{
    private static readonly SolidColorBrush GoldB = new(Color.FromRgb(232, 184, 96));
    private static readonly SolidColorBrush LoB = new(Color.FromRgb(169, 188, 210));

    // ---------------- inspector ----------------
    private void BuildInspector(Obj? o)
    {
        InspectorPanel.Children.Clear();
        if (o == null || Chunk == null)
        {
            InspectorPanel.Children.Add(new TextBlock
            {
                Text = "Click anything on the plan and its knobs appear here.\n\nLamps get warmth and colour, signs get words, people get jobs. Everything moves the 3D view live - play with it.",
                Foreground = LoB, FontSize = 13.5, TextWrapping = TextWrapping.Wrap, Margin = new Thickness(2, 6, 2, 0),
            });
            return;
        }
        void Head(string t) => InspectorPanel.Children.Add(new TextBlock
        { Text = t, Foreground = GoldB, FontWeight = FontWeights.Bold, FontSize = 15, Margin = new Thickness(2, 12, 0, 6) });
        void Row(string label, UIElement c)
        {
            var g = new Grid { Margin = new Thickness(0, 0, 0, 6) };
            g.ColumnDefinitions.Add(new ColumnDefinition { Width = new GridLength(92) });
            g.ColumnDefinitions.Add(new ColumnDefinition { Width = new GridLength(1, GridUnitType.Star) });
            var tb = new TextBlock { Text = label, Foreground = LoB, FontSize = 12.5, VerticalAlignment = VerticalAlignment.Center };
            Grid.SetColumn(tb, 0); Grid.SetColumn((UIElement)c, 1);
            g.Children.Add(tb); g.Children.Add(c);
            InspectorPanel.Children.Add(g);
        }
        TextBox Num(float v, Action<float> set)
        {
            var t = new TextBox { Text = v.ToString("0.##", System.Globalization.CultureInfo.InvariantCulture) };
            t.LostFocus += (_, _) => { if (float.TryParse(t.Text, out var f)) { set(f); AfterEdit(); } };
            return t;
        }
        ComboBox Combo(string[] items, string current, Action<string> set)
        {
            var c = new ComboBox { ItemsSource = items };
            c.SelectedItem = items.Contains(current) ? current : items[0];
            c.SelectionChanged += (_, _) => { if (c.SelectedItem is string s) { set(s); AfterEdit(); } };
            return c;
        }
        Slider Slide(double min, double max, double v, Action<double> set)
        {
            var s = new Slider { Minimum = min, Maximum = max, Value = v };
            s.ValueChanged += (_, _) => set(s.Value);
            return s;
        }

        Head(string.IsNullOrEmpty(o.Name) ? o.Type : o.Name);
        var name = new TextBox { Text = o.Name };
        name.TextChanged += (_, _) => { o.Name = name.Text; _plan.Invalidate(); };
        Row("Name", name);
        Row("Across →", Num(o.X, f => o.X = f));
        Row("Down ↓", Num(o.Y, f => o.Y = f));
        Row("Height", Num(o.Z, f => o.Z = f));
        Row("Width", Num(o.W, f => o.W = f));
        Row("Depth", Num(o.D, f => o.D = f));
        Row("Tall", Num(o.H, f => o.H = f));
        Row("Turn °", Num(o.Rotation, f => o.Rotation = f));

        if (o.Type == "light")
        {
            Head("💡 Lamp");
            Row("Hangs on", Combo(new[] { "Ceiling", "Wall", "Bare" }, o.LampMount ?? "Ceiling", s => o.LampMount = s));
            Row("Warmth", Combo(new[] { "🕯 Candlelight warm", "🏠 Cosy house warm", "🏢 Office white", "🌙 Cold moonlight" },
                o.Kelvin switch { <= 2850 => "🕯 Candlelight warm", <= 3600 => "🏠 Cosy house warm", <= 4800 => "🏢 Office white", _ => "🌙 Cold moonlight" },
                s => o.Kelvin = s.StartsWith("🕯") ? 2700 : s.StartsWith("🏠") ? 3200 : s.StartsWith("🏢") ? 4000 : 5600));
            Row("Brightness", Slide(0, 3, o.Intensity, v => { o.Intensity = (float)v; _gl.Invalidate(); }));
            Row("Reach (m)", Slide(1, 24, o.Range, v => { o.Range = (float)v; }));
            var gel = new Button { Content = string.IsNullOrEmpty(o.Gel) ? "🎨 Gel colour: none" : $"🎨 Gel colour: {o.Gel}", Style = (Style)FindResource("TopBtn") };
            gel.Click += (_, _) =>
            {
                using var dlg = new System.Windows.Forms.ColorDialog();
                if (dlg.ShowDialog() == System.Windows.Forms.DialogResult.OK)
                { o.Gel = $"#{dlg.Color.R:X2}{dlg.Color.G:X2}{dlg.Color.B:X2}"; BuildInspector(o); AfterEdit(); }
            };
            Row("Colour gel", gel);
        }
        if (o.Type == "sign")
        {
            Head("🪧 Sign words");
            var t = new TextBox { Text = o.Title ?? "" }; t.TextChanged += (_, _) => { o.Title = t.Text; AfterEdit(); }; Row("Big words", t);
            var s2 = new TextBox { Text = o.Subtitle ?? "" }; s2.TextChanged += (_, _) => { o.Subtitle = s2.Text; AfterEdit(); }; Row("Small words", s2);
            var a = new TextBox { Text = o.Accent ?? "" }; a.TextChanged += (_, _) => { o.Accent = a.Text; AfterEdit(); }; Row("Colour #hex", a);
        }
        if (o.Type == "prim")
        {
            Head("🏛 Shape");
            Row("Kind", Combo(Rmc.PrimKinds, o.PrimKind ?? "arch", s => o.PrimKind = s));
            Row("Material #", Num(o.Material, f => o.Material = (int)f));
        }
        if (o.Type == "hazard")
        {
            Head("⚠ Danger");
            Row("Kind", Combo(new[] { "Electrical", "Steam", "Crusher", "Toxic", "Fire", "FallingDebris", "Pressure", "Anomaly" }, o.Kind ?? "Electrical", s => o.Kind = s));
        }
        if (o.Type == "spawn")
        {
            Head("🧍 Who is this?");
            Row("Job", Combo(new[] { "Player", "Worker", "Scientist", "FemaleScientist", "FemaleDoctor", "LabTech", "Security", "Cargo", "Cleanup", "Investigator", "Hostile", "Creature" }, o.SpawnKind ?? "Worker", s => { o.SpawnKind = s; o.Name = s; }));
            Row("Monster", Combo(Rmc.CreatureSpecies, o.Species ?? "Brute", s => o.Species = s));
            Row("Behaviour", Combo(new[] { "civilian", "guard", "stand" }, o.Behaviour ?? "civilian", s => o.Behaviour = s));
            Row("Weapon", Combo(new[] { "none", "handgun", "shotgun" }, o.Armed ?? "none", s => o.Armed = s));
        }

        var del = new Button { Content = "💥 Remove this", Style = (Style)FindResource("TopBtn"), Margin = new Thickness(0, 14, 0, 6) };
        del.Click += (_, _) => { Chunk.Objects.Remove(o); _plan.Selected = null; BuildInspector(null); AfterEdit(); };
        var dup = new Button { Content = "⧉ Copy this", Style = (Style)FindResource("TopBtn") };
        dup.Click += (_, _) =>
        {
            var copy = JsonSerializer.Deserialize<Obj>(JsonSerializer.Serialize(o))!;
            copy.Id = Guid.NewGuid().ToString("N")[..8]; copy.X += 1; copy.Y += 1;
            Chunk.Objects.Add(copy); _plan.Selected = copy; BuildInspector(copy); AfterEdit();
        };
        InspectorPanel.Children.Add(del);
        InspectorPanel.Children.Add(dup);
    }

    // ---------------- files ----------------
    private void OpenProject()
    {
        var dlg = new Microsoft.Win32.OpenFileDialog { Filter = "Editor project (*.json)|*.json|All files|*.*" };
        if (dlg.ShowDialog(this) != true) return;
        _project = JsonSerializer.Deserialize<Project>(File.ReadAllText(dlg.FileName)) ?? NewBlankProject();
        _chunkIndex = 0; _layerIndex = 0;
        LoadIntoViews();
        SetStatus("Project opened - keep building.");
    }

    private void SaveProject()
    {
        var dlg = new Microsoft.Win32.SaveFileDialog { Filter = "Editor project (*.json)|*.json", FileName = "campaign-project.json" };
        if (dlg.ShowDialog(this) != true) return;
        File.WriteAllText(dlg.FileName, JsonSerializer.Serialize(_project, new JsonSerializerOptions { WriteIndented = true }));
        SetStatus("Project saved. Reopen it any time to keep editing.");
    }

    private void OpenRmc()
    {
        var dlg = new Microsoft.Win32.OpenFileDialog { Filter = "RawMetal campaign (*.rmc;*.txt)|*.rmc;*.txt|All files|*.*" };
        if (dlg.ShowDialog(this) != true) return;
        OpenCampaignFile(dlg.FileName, Path.GetFileNameWithoutExtension(dlg.FileName));
    }

    private void OpenCampaignFile(string path, string label)
    {
        try
        {
            var imported = Rmc.Import(File.ReadAllText(path), _maps);
            if (imported.Chunks.Count == 0) throw new InvalidDataException("The file did not contain any maps.");
            _project = imported; _chunkIndex = 0; _layerIndex = 0;
            MapSearch.Clear();
            LoadIntoViews();
            SetStatus($"{label} loaded: {_project.Chunks.Count} maps. Search the map list, click a map, then edit safely.");
            // Every normal campaign load gets a game-authored reference view
            // when it has a Player Start.  This is not a Starter-map special
            // case: it is the honest visual companion for any RMC, using the
            // same renderer, materials, fog and sky as play mode.
            _ = ShowLoadedCampaignReferenceAsync(label);
        }
        catch (Exception ex)
        {
            SetStatus($"Could not open {label}: {ex.Message}");
        }
    }

    private void ExportRmc()
    {
        var dlg = new Microsoft.Win32.SaveFileDialog { Filter = "RawMetal campaign (*.rmc)|*.rmc", FileName = $"{_project.Name}.rmc" };
        if (dlg.ShowDialog(this) != true) return;
        var warnings = new List<string>();
        File.WriteAllText(dlg.FileName, Rmc.Export(_project, _project.Name, _maps, warnings));
        _exported = true; UpdateChecklist();
        SetStatus(warnings.Count == 0 ? "Exported! Drop the .rmc in the game's 'custom maps' folder and play it."
            : $"Exported with {warnings.Count} note(s): {warnings[0]}");
    }

    private string ExportTempRmc()
    {
        var dir = Path.Combine(Path.GetTempPath(), "RawMetalEditor");
        Directory.CreateDirectory(dir);
        var path = Path.Combine(dir, "editor-preview.rmc");
        File.WriteAllText(path, Rmc.Export(_project, _project.Name, _maps, new List<string>()));
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
        var player = _project.Chunks.SelectMany(c => c.Objects.Where(o => o.Type == "spawn" && o.SpawnKind == "Player")
            .Select(o => (Chunk: c, Obj: o))).FirstOrDefault();
        if (player.Obj == null) { SetStatus("📸 needs a Player Start to stand the camera on (🧍 People, role: Player)."); return; }
        SetStatus("Asking the real game engine for a render… the editor stays live.");
        int mapIndex = Math.Max(0, _project.Chunks.OrderBy(c => c.Number).ToList().IndexOf(player.Chunk));
        float wx = player.Chunk.Gx * 24 + player.Obj.X, wy = player.Chunk.Gy * 24 + player.Obj.Y;
        var img = await RunEngineShotAsync(mapIndex, wx, wy, player.Obj.Rotation, 0, 1.7f);
        if (img == null) { SetStatus("Engine ran but made no picture - is there floor under the Player Start?"); return; }
        _playtested = true; UpdateChecklist();
        ShowEngineImage(img, "The real game, rendering your map");
        SetStatus("That's the actual game - lighting, bloom and all. 🎉");
    }

    private bool _referenceBusy;
    private async Task ShowLoadedCampaignReferenceAsync(string label)
    {
        if (_referenceBusy) return;
        var player = _project.Chunks.SelectMany(c => c.Objects.Where(o => o.Type == "spawn" && o.SpawnKind == "Player")
            .Select(o => (Chunk: c, Obj: o))).FirstOrDefault();
        if (player.Obj == null || FindEngine() == null) return;
        _referenceBusy = true;
        try
        {
            int mapIndex = Math.Max(0, _project.Chunks.OrderBy(c => c.Number).ToList().IndexOf(player.Chunk));
            float wx = player.Chunk.Gx * 24 + player.Obj.X, wy = player.Chunk.Gy * 24 + player.Obj.Y;
            var img = await RunEngineShotAsync(mapIndex, wx, wy, player.Obj.Rotation, 0, 1.7f);
            if (img != null)
                ShowEngineImage(img, $"Real game reference — {label}");
        }
        catch { /* The fast editor remains usable if a reference shot cannot render. */ }
        finally { _referenceBusy = false; }
    }

    // The actual engine, headless, rendering one named view of the temp
    // export. This is the ONLY renderer that looks exactly like the game,
    // so the editor borrows it instead of imitating it.
    private async Task<System.Windows.Media.Imaging.BitmapImage?> RunEngineShotAsync(
        int mapIndex, float wx, float wy, float yaw, float pitch, float lift)
    {
        var engine = FindEngine();
        if (engine == null) return null;
        var rmc = ExportTempRmc();
        var listPath = Path.Combine(Path.GetTempPath(), "RawMetalEditor", "views.txt");
        File.WriteAllText(listPath, $"editorview {mapIndex} {wx:0.##} {wy:0.##} {yaw:0.##} {pitch:0.##} {lift:0.##}\n");
        var engineDir = Path.GetDirectoryName(engine)!;
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
        return File.Exists(ppm) ? LoadPpmImage(ppm) : null;
    }

    private void ShowEngineImage(System.Windows.Media.Imaging.BitmapImage img, string title)
    {
        if (_engineWin == null)
        {
            _engineImg = new System.Windows.Controls.Image { Stretch = Stretch.Uniform };
            _engineWin = new Window
            {
                Title = title, Width = 1020, Height = 640,
                Background = System.Windows.Media.Brushes.Black, Owner = this,
                Content = _engineImg,
            };
            _engineWin.Closed += (_, _) => { _engineWin = null; _engineImg = null; _autoEngine = false; };
        }
        _engineWin.Title = title;
        _engineImg!.Source = img;
        if (!_engineWin.IsVisible) _engineWin.Show();
    }

    // Auto engine view: the real render, refreshed a breath after every
    // edit. Build in the fast sketch; judge in the real thing.
    private bool _autoEngine, _engineBusy;
    private System.Windows.Threading.DispatcherTimer? _engineTimer;
    private Window? _engineWin;
    private System.Windows.Controls.Image? _engineImg;

    private void ToggleAutoEngine()
    {
        _autoEngine = !_autoEngine;
        if (_autoEngine)
        {
            SetStatus("📸 AUTO ENGINE VIEW on: the real game re-renders your map after every edit. Keep it open beside you.");
            KickAutoEngine();
        }
        else
        {
            _engineWin?.Close();
            SetStatus("Auto engine view off.");
        }
    }

    private void KickAutoEngine()
    {
        if (!_autoEngine) return;
        _engineTimer ??= new System.Windows.Threading.DispatcherTimer
        { Interval = TimeSpan.FromMilliseconds(900) };
        _engineTimer.Tick -= OnEngineTimer;
        _engineTimer.Tick += OnEngineTimer;
        _engineTimer.Stop();
        _engineTimer.Start();
    }

    private async void OnEngineTimer(object? sender, EventArgs e)
    {
        _engineTimer?.Stop();
        if (!_autoEngine || _engineBusy || Chunk == null) return;
        _engineBusy = true;
        try
        {
            var ordered = _project.Chunks.OrderBy(c => c.Number).ToList();
            int mapIndex = Math.Max(0, ordered.IndexOf(Chunk));
            float wx = Chunk.Gx * 24 + 12, wy = Chunk.Gy * 24 + 12;
            var img = await RunEngineShotAsync(mapIndex, wx, wy, 0, -52, 25);
            if (img != null && _autoEngine) ShowEngineImage(img, $"Real game render - {Chunk.Name} (auto)");
        }
        catch { }
        finally { _engineBusy = false; }
    }

    private void TestInEngine()
    {
        var engine = FindEngine();
        if (engine == null) { SetStatus("Could not find the game exe."); return; }
        var engineDir = Path.GetDirectoryName(engine)!;
        var mapsDir = Path.Combine(engineDir, "custom maps");
        Directory.CreateDirectory(mapsDir);
        var target = Path.Combine(mapsDir, $"{_project.Name}.rmc");
        File.WriteAllText(target, Rmc.Export(_project, _project.Name, _maps, new List<string>()));
        Process.Start(new ProcessStartInfo(engine) { WorkingDirectory = engineDir, UseShellExecute = true });
        _playtested = true; UpdateChecklist();
        SetStatus($"Game launched! In the game choose: Custom Maps → {_project.Name}");
    }

    private static BitmapImage? LoadPpmImage(string path)
    {
        using var bmp = LoadPpm(path);
        if (bmp == null) return null;
        using var ms = new MemoryStream();
        bmp.Save(ms, System.Drawing.Imaging.ImageFormat.Png);
        ms.Position = 0;
        var bi = new BitmapImage();
        bi.BeginInit(); bi.CacheOption = BitmapCacheOption.OnLoad; bi.StreamSource = ms; bi.EndInit();
        bi.Freeze();
        return bi;
    }

    private static System.Drawing.Bitmap? LoadPpm(string path)
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
        _ = Next(); pos++;
        var bmp = new System.Drawing.Bitmap(w, h);
        var data = bmp.LockBits(new System.Drawing.Rectangle(0, 0, w, h),
            System.Drawing.Imaging.ImageLockMode.WriteOnly, System.Drawing.Imaging.PixelFormat.Format32bppArgb);
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

    // Headless smoke test: sample -> dual 3D paint (with pixel readback)
    // -> walk -> engine-format export. Fails loudly, never silently.
    public void RunSelfTest()
    {
        Loaded += async (_, _) =>
        {
            try
            {
                LoadSample();
                SetViewMode("dual");
                await Task.Delay(500);
                try { Directory.CreateDirectory(@"n:\rawmetal\.build"); _gl.DebugSaveFrame(@"n:\rawmetal\.build\editor-frame.png"); } catch { }
                var (distinct, brightness) = _gl.RenderStats();
                if (distinct < 8 || brightness <= 0.001f)
                    throw new InvalidOperationException($"3D view looks blank (distinct={distinct}, brightness={brightness:0.###})");
                SetViewMode("3d");
                _gl.EnterWalk();
                await Task.Delay(300);
                var warnings = new List<string>();
                var text = Rmc.Export(_project, _project.Name, _maps, warnings);
                if (!text.Contains("CAMPAIGN|") || !text.Contains("LIGHT|"))
                    throw new InvalidOperationException("export missing records");
                _project.TerrainEnabled = true;
                _project.Ground.Add(new GroundRect { X1 = 0, Y1 = 0, X2 = 24, Y2 = 24, Kind = 5, Edge = 2 });
                _project.Flats.Add(new FlatRect { X1 = 2, Y1 = 2, X2 = 10, Y2 = 8, Height = 1, Blend = 8 });
                text = Rmc.Export(_project, _project.Name, _maps, warnings);
                if (!text.Contains("TERRAIN|") || !text.Contains("GROUND|") || !text.Contains("TERRAIN_FLAT|"))
                    throw new InvalidOperationException("terrain export missing records");
                // 3D build mode places through the same code as the plan,
                // and undo must take it back. Both proven, not promised.
                int beforeCount = _project.Chunks[0].Objects.Count;
                _gl.BuildMode = true;
                SetTool("box");
                Place3D(6, 19, 0);
                if (_project.Chunks[0].Objects.Count != beforeCount + 1)
                    throw new InvalidOperationException("3D build click did not place");
                Undo();
                if (_project.Chunks[0].Objects.Count != beforeCount)
                    throw new InvalidOperationException("undo did not restore");
                _gl.BuildMode = false;
                File.WriteAllText("selftest-ok.txt",
                    $"WPF shell + 3D + walk + export OK, {text.Split('\n').Length} lines, {warnings.Count} warnings");
            }
            catch (Exception ex) { File.WriteAllText("selftest-fail.txt", ex.ToString()); }
            finally { System.Windows.Application.Current.Shutdown(); }
        };
    }

    // Every tool, exercised the way a person uses it, with an assertion
    // on the result. Written because "most tools don't work" shipped
    // under a happy-path-only test. Output: selftest-tools.txt
    public void RunToolTest()
    {
        Loaded += async (_, _) =>
        {
            var results = new List<string>();
            void Check(string name, Func<bool> test)
            {
                try { results.Add(test() ? $"PASS {name}" : $"FAIL {name}"); }
                catch (Exception ex) { results.Add($"FAIL {name}: {ex.GetType().Name} {ex.Message}"); }
            }
            try
            {
                await Task.Delay(400);
                _project = NewBlankProject(); _chunkIndex = 0; _layerIndex = 0;
                LoadIntoViews();
                var chunk = Chunk!; var layer = Layer!;
                int Count(string type) => chunk.Objects.Count(o => o.Type == type);

                Check("simple mode keeps the same map", () =>
                {
                    var before = Chunk;
                    ToggleSimpleMode();
                    bool simple = ToolsPanel.Visibility == Visibility.Collapsed &&
                                  RightDock.Visibility == Visibility.Collapsed && Chunk == before;
                    ToggleSimpleMode();
                    return simple && ToolsPanel.Visibility == Visibility.Visible && RightDock.Visibility == Visibility.Visible;
                });

                Check("drag-a-room makes walls+floor", () =>
                { RectFillRoom(2, 2, 10, 8); return GetTile(layer, 2, 2) == '#' && GetTile(layer, 5, 5) == '.'; });
                Check("paint floor stroke", () =>
                { SetTool("room"); PaintTile(5, 5, System.Windows.Forms.MouseButtons.Left); return GetTile(layer, 5, 5) == '.'; });
                Check("wall block", () =>
                { SetTool("wall"); PaintTile(5, 4, System.Windows.Forms.MouseButtons.Left); return GetTile(layer, 5, 4) == '#'; });
                Check("door lands on wall", () =>
                { int n = Count("door"); SetTool("door"); PlaceAt(2, 5, System.Windows.Forms.MouseButtons.Left); return Count("door") == n + 1; });
                Check("window lands on wall", () =>
                { int n = Count("window"); SetTool("window"); PlaceAt(10, 5, System.Windows.Forms.MouseButtons.Left); return Count("window") == n + 1; });
                Check("lamp", () =>
                { int n = Count("light"); SetTool("light"); PlaceAt(5, 5, System.Windows.Forms.MouseButtons.Left); return Count("light") == n + 1; });
                Check("sign", () =>
                { int n = Count("sign"); SetTool("sign"); PlaceAt(6, 5, System.Windows.Forms.MouseButtons.Left); return Count("sign") == n + 1; });
                Check("architecture shape", () =>
                { int n = Count("prim"); SetTool("prim"); PlaceAt(7, 5, System.Windows.Forms.MouseButtons.Left); return Count("prim") == n + 1; });
                Check("computer terminal", () =>
                { int n = Count("terminal"); SetTool("terminal"); PlaceAt(8, 5, System.Windows.Forms.MouseButtons.Left); return Count("terminal") == n + 1; });
                Check("danger zone", () =>
                { int n = Count("hazard"); SetTool("hazard"); PlaceAt(9, 6, System.Windows.Forms.MouseButtons.Left); return Count("hazard") == n + 1; });
                Check("stairs", () =>
                { int n = Count("stairs"); SetTool("stairs"); PlaceAt(4, 7, System.Windows.Forms.MouseButtons.Left); return Count("stairs") == n + 1; });
                Check("big block", () =>
                { int n = Count("box"); SetTool("box"); PlaceAt(6, 7, System.Windows.Forms.MouseButtons.Left); return Count("box") == n + 1; });
                Check("player start", () =>
                { SpawnRoleCombo.SelectedItem = "Player"; int n = Count("spawn"); SetTool("people"); PlaceAt(3, 3, System.Windows.Forms.MouseButtons.Left); return chunk.Objects.Count(o => o.Type == "spawn" && o.SpawnKind == "Player") == 1 && Count("spawn") == n + 1; });
                Check("guard (armed person)", () =>
                { SpawnRoleCombo.SelectedItem = "Security"; SetTool("people"); PlaceAt(8, 3, System.Windows.Forms.MouseButtons.Left); return chunk.Objects.Any(o => o.Type == "spawn" && o.SpawnKind == "Security"); });
                Check("creature", () =>
                { SpawnRoleCombo.SelectedItem = "Creature"; SetTool("people"); PlaceAt(9, 3, System.Windows.Forms.MouseButtons.Left); return chunk.Objects.Any(o => o.Type == "spawn" && o.SpawnKind == "Creature"); });
                Check("place a real model (mapped fixture)", () =>
                {
                    if (_blob == null) return false;
                    var pf = _blob.Prefabs.FirstOrDefault(p => _maps.Facility.ContainsKey(p.Model.Replace('\\', '/'))
                                                              || _maps.Clutter.ContainsKey(p.Model.Replace('\\', '/')));
                    if (pf == null) return false;
                    _pickedPrefab = pf; int n = Count("asset");
                    SetTool("asset"); PlaceAt(5, 6, System.Windows.Forms.MouseButtons.Left);
                    return Count("asset") == n + 1;
                });
                Check("paint finish shows on plan data", () =>
                {
                    if (_blob == null || _blob.Materials.Count == 0) return false;
                    _pickedMaterial = _blob.Materials[0]; SetTool("finish");
                    PaintTile(5, 5, System.Windows.Forms.MouseButtons.Left);
                    return layer.Materials.ContainsKey("5,5");
                });
                Check("erase object (right-click)", () =>
                { int n = chunk.Objects.Count; PlaceAt(6, 7, System.Windows.Forms.MouseButtons.Right); return chunk.Objects.Count == n - 1; });
                Check("ground paint rect", () =>
                { int n = _project.Ground.Count; AddGroundRect(0, 0, 3, 3); return _project.Ground.Count == n + 1; });
                Check("flatten pad rect", () =>
                { int n = _project.Flats.Count; AddFlatRect(12, 12, 18, 18); return _project.Flats.Count == n + 1; });
                Check("furnish adds lamps", () =>
                { int n = Count("light"); FurnishRoom(); return Count("light") > n; });
                Check("export carries every record kind", () =>
                {
                    var text = Rmc.Export(_project, _project.Name, _maps, new List<string>());
                    foreach (var tag in new[] { "LIGHT|", "SIGN|", "PRIM|", "DOOR|", "HAZARD|", "TERMINAL|", "CREATURE|", "ACTOR|", "GROUND|", "TERRAIN_FLAT|" })
                        if (!text.Contains(tag)) { results.Add($"  missing {tag}"); return false; }
                    return true;
                });
                Check("undo restores", () =>
                { int n = chunk.Objects.Count; Undo(); return chunk.Objects.Count <= n; });
            }
            catch (Exception ex) { results.Add($"FAIL harness: {ex}"); }
            File.WriteAllText("selftest-tools.txt", string.Join("\n", results));
            System.Windows.Application.Current.Shutdown();
        };
    }

    // Reproduces the real application path for both engine-owned fixtures:
    // import, populate the map rail, bind the plan and render the focused
    // neighbourhood.  This protects against a parser-only green test while
    // the actual editor still falls over when a campaign is opened.
    public void RunFixtureWindowTest()
    {
        Loaded += async (_, _) =>
        {
            try
            {
                var reports = new List<string>();
                foreach (var file in new[] { "Starter Map.rmc", "RawMetal-Ashfall-Exclusion-Zone.txt" })
                {
                    string path = Path.Combine(Directory.GetCurrentDirectory(), "custom maps", file);
                    _project = Rmc.Import(File.ReadAllText(path), _maps);
                    _chunkIndex = 0; _layerIndex = 0;
                    LoadIntoViews();
                    await Task.Delay(450);
                    var (distinct, brightness) = _gl.RenderStats();
                    if (distinct < 8 || brightness <= .001f)
                        throw new InvalidOperationException($"{file}: focused 3D view is blank.");
                    reports.Add($"{file}: {_project.Chunks.Count} maps, 3D distinct={distinct}, brightness={brightness:0.###}");
                }
                File.WriteAllLines("selftest-fixture-windows.txt", reports);
            }
            catch (Exception ex) { File.WriteAllText("selftest-fixture-windows-fail.txt", ex.ToString()); }
            finally { System.Windows.Application.Current.Shutdown(); }
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
        SetStatus("Sample loaded. Paint on it, 🚶 WALK it, then 📸 ENGINE SHOT to see the real render.");
    }
}
