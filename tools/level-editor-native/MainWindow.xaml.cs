using System.Windows;
using System.Windows.Controls;
using System.Windows.Media;
using System.Windows.Media.Imaging;
using MouseButtons = System.Windows.Forms.MouseButtons;
using Button = System.Windows.Controls.Button;

namespace RawMetalCampaignEditor;

public sealed class PartVm
{
    // WPF binds to properties, never fields - these must stay properties.
    public string Name { get; set; } = "";
    public BitmapImage? Image { get; set; }
    public Prefab? Prefab { get; set; }
    public MaterialItem? Material { get; set; }
    // Shown big when a model has no baked texture photo yet.
    public string Emoji { get; set; } = "🧊";
}

public partial class MainWindow : Window
{
    private Project _project = NewBlankProject();
    private readonly Mappings _maps = Mappings.Load();
    private Blob? _blob;
    private string _tool = "roombox";
    private int _chunkIndex;
    private int _layerIndex;
    private Prefab? _pickedPrefab;
    private MaterialItem? _pickedMaterial;
    private bool _playtested, _exported, _catsFilled, _simpleMode;

    private readonly PlanCanvas _plan = new();
    private readonly GlView _gl = new();
    private readonly Dictionary<string, Button> _toolButtons = new();
    private sealed class MapItem
    {
        public int Index { get; init; }
        public string Text { get; init; } = "";
        public override string ToString() => Text;
    }

    private Chunk? Chunk => _project.Chunks.Count == 0 ? null
        : _project.Chunks[Math.Min(_project.Chunks.Count - 1, Math.Max(0, _chunkIndex))];
    private Layer? Layer => Chunk is { } c && _layerIndex < c.Layers.Count ? c.Layers[_layerIndex] : null;

    public MainWindow()
    {
        InitializeComponent();
        PlanHost.Child = _plan;
        GlHost.Child = _gl;

        BuildToolbar();
        BuildTools();
        WireControls();
        FindBlob();
        LoadIntoViews();

        _plan.TileAction = PaintTile;
        _plan.PlaceAction = PlaceAt;
        _plan.StrokeStarted += PushUndo;
        _gl.BuildClick = (wx, wy, z) => Place3D(wx, wy, z);
        PreviewKeyDown += (_, e) =>
        {
            if (e.Key == System.Windows.Input.Key.Z &&
                System.Windows.Input.Keyboard.Modifiers.HasFlag(System.Windows.Input.ModifierKeys.Control))
            { Undo(); e.Handled = true; }
        };
        _plan.RectAction = (x0, y0, x1, y1) =>
        {
            switch (_tool)
            {
                case "ground": AddGroundRect(x0, y0, x1, y1); break;
                case "flat": AddFlatRect(x0, y0, x1, y1); break;
                default: RectFillRoom(x0, y0, x1, y1); break;
            }
        };
        _plan.PickAction = o => { _plan.Selected = o; BuildInspector(o); _plan.Invalidate(); };
        _plan.DragAction = (o, wx, wy) =>
        {
            if (_tool != "select") return;
            o.X = Math.Clamp(wx, 0, 24); o.Y = Math.Clamp(wy, 0, 24);
            if (o.Type is "door" or "window") { o.TileX = (int)o.X; o.TileY = (int)o.Y; }
            _gl.Invalidate();
        };
        _gl.Status += SetStatus;

        Loaded += (_, _) =>
        {
            _plan.FitView();
            SetTool("roombox");
            // Open alive, like Tiny Glade: a built sample you can poke,
            // not a black void and a manual. New wipes it in one click.
            if (_project.Chunks.All(c => c.Objects.Count == 0)) LoadSample();
        };
        ViewGrid.SizeChanged += (_, _) => { if (_viewMode != "3d") _plan.FitView(); };
        SetStatus(_blob != null
            ? $"Asset library loaded: {_blob.Prefabs.Count} models, {_blob.Materials.Count} finishes. Drag a room, or load the sample building."
            : "No editor-assets.rmblob found - build the asset blob for model previews.");
    }

    private string _viewMode = "dual";

    // ---------------- chrome ----------------
    private void BuildToolbar()
    {
        ToolbarPanel.Children.Clear();
        void Add(string label, Action act, bool gold = false)
        {
            var b = new Button { Content = label, Style = (Style)FindResource(gold ? "GoldBtn" : "TopBtn") };
            b.Click += (_, _) => act();
            ToolbarPanel.Children.Add(b);
        }
        if (_simpleMode)
        {
            // This is deliberately project-agnostic.  It edits whatever map
            // the person opened through the normal file flow.
            Add("🌼 Advanced tools", ToggleSimpleMode);
            Add("🏠 Draw a room", () => { SetTool("roombox"); SetViewMode("dual"); SetStatus("Drag on the plan to make a room. The game walls and floor are made for you."); });
            Add("✨ Light & furnish", FurnishRoom);
            Add("🧍 Add a person", () => { SpawnRoleCombo.SelectedItem = "Scientist"; SetTool("people"); SetStatus("Click the plan to put down a friendly scientist. Use Advanced tools for guards and monsters."); });
            Add("✋ Move things", () => { SetTool("select"); SetStatus("Drag anything in the plan to rearrange it. Right-click an item to remove it."); });
            Add("▶ Walk inside", StartWalk);
            Add("📸 See it in game", EngineShot);
            Add("EXPORT .RMC", ExportRmc, gold: true);
            return;
        }
        Add("🌼 SIMPLE MODE", ToggleSimpleMode, gold: true);
        Add("New", () => { _project = NewBlankProject(); _chunkIndex = 0; _layerIndex = 0; LoadIntoViews(); SetStatus("Blank campaign. 🏠 Drag-a-room to begin."); });
        Add("Open Project", OpenProject);
        Add("Save Project", SaveProject);
        Add("Open .RMC", OpenRmc);
        Add("EXPORT .RMC", ExportRmc, gold: true);
        Add("🏠 SAMPLE", LoadSample);
        Add("↩ UNDO", Undo);
        Add("🧱 3D BUILD", ToggleBuild);
        Add("⬒ DUAL", () => SetViewMode("dual"));
        Add("PLAN", () => SetViewMode("plan"));
        Add("3D", () => SetViewMode("3d"));
        Add("▶ WALK", StartWalk);
        Add("◼ STOP", () => { _gl.ExitWalk(); _gl.BuildMode = false; SetViewMode("dual"); });
        Add("▦ WIREFRAME", () => { _gl.Wireframe = !_gl.Wireframe; _gl.Invalidate(); SetStatus(_gl.Wireframe ? "Wireframe: pure structure, like Hammer." : "Textured view."); });
        Add("📸 ENGINE SHOT", EngineShot);
        Add("📸 AUTO ENGINE", ToggleAutoEngine);
        Add("✨ FURNISH", FurnishRoom);
        Add("▶ TEST IN GAME", TestInEngine);
    }

    private void ToggleSimpleMode()
    {
        _simpleMode = !_simpleMode;
        ToolsPanel.Visibility = _simpleMode ? Visibility.Collapsed : Visibility.Visible;
        RightDock.Visibility = _simpleMode ? Visibility.Collapsed : Visibility.Visible;
        RightCol.Width = _simpleMode ? new GridLength(0) : new GridLength(348);
        BuildToolbar();
        if (_simpleMode)
        {
            SetViewMode("dual");
            SetStatus("Simple Mode: choose one friendly action above. Your current map and all engine data stay exactly where they are.");
        }
        else SetStatus("Advanced tools are back. Nothing in your map changed.");
    }

    private void BuildTools()
    {
        var tools = new (string Id, string Label)[]
        {
            ("select","✋  Move things"), ("roombox","🏠  Drag-a-room"), ("room","🖌  Paint floor"), ("wall","🧱  Wall block"),
            ("deck","🪜  Upper floor"), ("void","🧽  Rub out"), ("finish","🎨  Paint colour"),
            ("ground","🌱  Ground paint"), ("flat","🛣  Flatten pad"),
            ("door","🚪  Door"), ("window","🪟  Window"), ("light","💡  Lamp"), ("sign","🪧  Sign"),
            ("prim","🏛  Shapes"), ("terminal","💻  Computer"), ("hazard","⚠  Danger zone"),
            ("people","🧍  People + monsters"), ("stairs","↗  Stairs"), ("box","📦  Big block"), ("erase","💥  Remove thing"),
        };
        foreach (var (id, label) in tools)
        {
            var b = new Button { Content = label, Style = (Style)FindResource("ToolBtn"), Tag = id };
            b.Click += (_, _) => SetTool(id);
            ToolsPanel.Children.Add(b);
            _toolButtons[id] = b;
        }
    }

    private void WireControls()
    {
        SpawnRoleCombo.ItemsSource = new[] { "Player", "Worker", "Scientist", "FemaleScientist", "FemaleDoctor", "LabTech", "Security", "Cargo", "Cleanup", "Investigator", "Hostile", "Creature" };
        SpawnRoleCombo.SelectedIndex = 0;
        PrimKindCombo.ItemsSource = Rmc.PrimKinds;
        PrimKindCombo.SelectedIndex = 5;
        HeightCombo.ItemsSource = new[] { "🟫 On the floor", "🍽 Table height", "🗄 On a shelf", "🏢 Up high" };
        HeightCombo.SelectedIndex = 0;
        foreach (var combo in new[] { SurfWallCombo, SurfFloorCombo, SurfCeilCombo })
        {
            combo.ItemsSource = Rmc.SurfaceSets.Select(s => s.Name).ToArray();
            combo.SelectedIndex = 0;
            combo.SelectionChanged += (_, _) => ApplySurfaces();
        }
        LayerList.SelectionChanged += (_, _) =>
        {
            if (LayerList.SelectedIndex >= 0) { _layerIndex = LayerList.SelectedIndex; _plan.LayerIndex = _layerIndex; _plan.Invalidate(); _gl.Invalidate(); }
        };
        AddFloorBtn.Click += (_, _) => AddFloor();
        MapList.SelectionChanged += (_, _) =>
        {
            if (MapList.SelectedItem is MapItem item && item.Index != _chunkIndex)
            {
                _chunkIndex = item.Index; _layerIndex = 0;
                _plan.Selected = null; BuildInspector(null);
                RefreshLayers(); _plan.Invalidate(); _gl.Invalidate();
                SetStatus($"Now editing map: {Chunk?.Name}. Each map is a place the player travels between.");
            }
        };
        MapSearch.TextChanged += (_, _) => RefreshMaps();
        AddMapBtn.Click += (_, _) => AddMap();
        WireTerrain();
        PartSearch.TextChanged += (_, _) => ListParts();
        PartCatCombo.SelectionChanged += (_, _) => ListParts();
        PartsList.SelectionChanged += (_, _) =>
        {
            if (PartsList.SelectedItem is PartVm { Prefab: not null } vm)
            { _pickedPrefab = vm.Prefab; SetTool("asset"); SetStatus($"Placing {vm.Name} - click the plan, or place it in 🧱 3D BUILD."); }
        };
        MatsList.SelectionChanged += (_, _) =>
        {
            if (MatsList.SelectedItem is PartVm { Material: not null } vm)
            { _pickedMaterial = vm.Material; SetTool("finish"); SetStatus($"🎨 Painting {vm.Name} - drag across floors and walls."); }
        };
    }

    // ---------------- outdoors ----------------
    public sealed class GroundItem
    {
        public object Ref = null!;
        public string Text = "";
        public override string ToString() => Text;
    }

    public static readonly string[] GroundKindNames =
        { "🌿 Short grass", "🍃 Leafy grass", "☘ Clover", "🌾 Wild grass", "🏜 Dry dirt", "🍂 Forest litter", "🪨 Gravel", "🟤 Mud", "🌼 Meadow" };

    private void WireTerrain()
    {
        GroundKindCombo.ItemsSource = GroundKindNames;
        GroundKindCombo.SelectedIndex = 5;
        TerrainOn.Checked += (_, _) => { _project.TerrainEnabled = true; AfterEdit(); };
        TerrainOn.Unchecked += (_, _) => { _project.TerrainEnabled = false; AfterEdit(); };
        void Num(System.Windows.Controls.TextBox box, Action<float> set)
            => box.LostFocus += (_, _) => { if (float.TryParse(box.Text, out var v)) { set(v); AfterEdit(); } };
        Num(TSeed, v => _project.TerrainSeed = (int)v);
        Num(TBase, v => _project.TerrainBase = v);
        Num(TAmp, v => _project.TerrainAmplitude = v);
        Num(TScale, v => _project.TerrainScale = v);
        Num(TRough, v => _project.TerrainRoughness = v);
        Num(HDepth, v => _project.HorizonDepth = v);
        Num(HSpacing, v => _project.HorizonSpacing = v);
        Num(HRise, v => _project.HorizonRise = v);
        DelGroundBtn.Click += (_, _) =>
        {
            if (GroundList.SelectedItem is GroundItem item)
            {
                if (item.Ref is GroundRect g) _project.Ground.Remove(g);
                if (item.Ref is FlatRect f) _project.Flats.Remove(f);
                RefreshGroundList(); AfterEdit();
            }
        };
        SyncTerrainUi();
    }

    private void SyncTerrainUi()
    {
        TerrainOn.IsChecked = _project.TerrainEnabled;
        TSeed.Text = _project.TerrainSeed.ToString();
        TBase.Text = _project.TerrainBase.ToString("0.##");
        TAmp.Text = _project.TerrainAmplitude.ToString("0.##");
        TScale.Text = _project.TerrainScale.ToString("0.##");
        TRough.Text = _project.TerrainRoughness.ToString("0.##");
        HDepth.Text = _project.HorizonDepth.ToString("0.##");
        HSpacing.Text = _project.HorizonSpacing.ToString("0.##");
        HRise.Text = _project.HorizonRise.ToString("0.##");
        RefreshGroundList();
    }

    private void RefreshGroundList()
    {
        GroundList.Items.Clear();
        foreach (var g in _project.Ground)
            GroundList.Items.Add(new GroundItem { Ref = g, Text = $"{GroundKindNames[Math.Clamp(g.Kind, 0, 8)]}  ({g.X1:0},{g.Y1:0})-({g.X2:0},{g.Y2:0})" });
        foreach (var f in _project.Flats)
            GroundList.Items.Add(new GroundItem { Ref = f, Text = $"🛣 Pad at {f.Height:0.#}m  ({f.X1:0},{f.Y1:0})-({f.X2:0},{f.Y2:0})" });
    }

    private void AddGroundRect(int x0, int y0, int x1, int y1)
    {
        if (Chunk == null) return;
        _project.TerrainEnabled = true; TerrainOn.IsChecked = true;
        _project.Ground.Add(new GroundRect
        {
            X1 = Chunk.Gx * 24 + x0, Y1 = Chunk.Gy * 24 + y0,
            X2 = Chunk.Gx * 24 + x1 + 1, Y2 = Chunk.Gy * 24 + y1 + 1,
            Kind = Math.Max(0, GroundKindCombo.SelectedIndex), Edge = 2,
        });
        RefreshGroundList(); AfterEdit();
        SetStatus("Ground painted. It exports as real GROUND records - the engine grows grass there.");
    }

    private void AddFlatRect(int x0, int y0, int x1, int y1)
    {
        if (Chunk == null) return;
        _project.TerrainEnabled = true; TerrainOn.IsChecked = true;
        float h = float.TryParse(FlatHeight.Text, out var v) ? v : 1f;
        _project.Flats.Add(new FlatRect
        {
            X1 = Chunk.Gx * 24 + x0, Y1 = Chunk.Gy * 24 + y0,
            X2 = Chunk.Gx * 24 + x1 + 1, Y2 = Chunk.Gy * 24 + y1 + 1,
            Height = h, Blend = 8,
        });
        RefreshGroundList(); AfterEdit();
        SetStatus($"Pad flattened at {h:0.#}m - build on it like the Starter Map's forest clearings.");
    }

    private void SetStatus(string s) => StatusText.Text = s;

    private void SetTool(string tool)
    {
        _tool = tool;
        _plan.ToolNeedsObjectPick = tool is "select" or "erase";
        _plan.ToolUsesRect = tool is "roombox" or "ground" or "flat";
        if (tool == "roombox") SetStatus("Drag a rectangle on the plan, let go - a whole walled room appears.");
        if (tool == "ground") SetStatus("🌱 Drag a rectangle to paint ground. Pick the grass/dirt kind in the 🌍 Outdoors tab.");
        if (tool == "flat") SetStatus("🛣 Drag a rectangle to flatten a building pad at the height set in 🌍 Outdoors.");
        SyncGhost();
        foreach (var (id, b) in _toolButtons)
        {
            bool active = id == tool;
            b.Foreground = new SolidColorBrush(active ? System.Windows.Media.Color.FromRgb(232, 184, 96) : System.Windows.Media.Color.FromRgb(220, 229, 240));
            b.FontWeight = active ? FontWeights.Bold : FontWeights.Normal;
        }
    }

    private void SetViewMode(string mode)
    {
        _viewMode = mode;
        PlanCol.Width = mode == "3d" ? new GridLength(0) : new GridLength(1, GridUnitType.Star);
        GlCol.Width = mode == "plan" ? new GridLength(0) : new GridLength(1, GridUnitType.Star);
        if (mode != "plan") { _gl.Project = _project; _gl.Blob = _blob; _gl.Invalidate(); }
        Dispatcher.BeginInvoke(new Action(() => { if (mode != "3d") _plan.FitView(); }));
    }

    // ---------------- undo ----------------
    // ChunkIndex -1 means "whole project". Big campaigns (the 150-map
    // Starter) must never pay a whole-project JSON snapshot per click -
    // that was a visible freeze on every stroke. Everyday edits are
    // chunk-local, so everyday undo is chunk-scoped and instant.
    private sealed class UndoEntry { public string Json = ""; public int ChunkIndex = -1; }
    private readonly Stack<UndoEntry> _undo = new();

    private void PushUndo()
    {
        try
        {
            UndoEntry entry;
            if (Chunk != null && _project.Chunks.Count > 4)
                entry = new UndoEntry { Json = System.Text.Json.JsonSerializer.Serialize(Chunk), ChunkIndex = _chunkIndex };
            else
                entry = new UndoEntry { Json = System.Text.Json.JsonSerializer.Serialize(_project), ChunkIndex = -1 };
            if (_undo.Count > 0 && _undo.Peek().Json == entry.Json && _undo.Peek().ChunkIndex == entry.ChunkIndex) return;
            _undo.Push(entry);
            if (_undo.Count > 40)
            {
                var keep = _undo.Take(20).Reverse().ToArray();
                _undo.Clear();
                foreach (var s in keep) _undo.Push(s);
            }
        }
        catch { }
    }

    private void Undo()
    {
        if (_undo.Count == 0) { SetStatus("Nothing to undo yet."); return; }
        var entry = _undo.Pop();
        if (entry.ChunkIndex >= 0 && entry.ChunkIndex < _project.Chunks.Count)
        {
            var chunk = System.Text.Json.JsonSerializer.Deserialize<Chunk>(entry.Json);
            if (chunk == null) return;
            _project.Chunks[entry.ChunkIndex] = chunk;
            _chunkIndex = entry.ChunkIndex;
        }
        else
        {
            var restored = System.Text.Json.JsonSerializer.Deserialize<Project>(entry.Json);
            if (restored == null) return;
            _project = restored;
            _chunkIndex = Math.Min(_chunkIndex, Math.Max(0, _project.Chunks.Count - 1));
        }
        _layerIndex = 0;
        LoadIntoViews();
        SetStatus("Undone. (Ctrl+Z is your friend - experimenting is free now.)");
    }

    // ---------------- 3D build ----------------
    private void ToggleBuild()
    {
        _gl.BuildMode = !_gl.BuildMode;
        if (_gl.BuildMode)
        {
            _gl.ExitWalk();
            SetViewMode("3d");
            SyncGhost();
            _gl.Focus();
            SetStatus("🧱 3D BUILD: mouse aims the gold ghost, click places it, PageUp/PageDown lifts the shelf, R turns it, right-drag orbits. Same map as the plan - cross freely.");
        }
        else SetStatus("3D build off.");
        _gl.Invalidate();
    }

    private void StartWalk()
    {
        SetViewMode("3d");
        _gl.EnterWalk();
        _playtested = true;
        UpdateChecklist();
    }

    // ---------------- project state ----------------
    private static Project NewBlankProject()
    {
        var p = new Project();
        var chunk = new Chunk { Number = 1, Name = "First Map" };
        chunk.Layers.Add(BlankLayer("Ground Floor", 0));
        p.Chunks.Add(chunk);
        return p;
    }

    private static Layer BlankLayer(string name, float z)
    {
        var l = new Layer { Name = name, Z = z };
        for (int i = 0; i < 24; i++) l.Rows.Add(new string('_', 24));
        return l;
    }

    private void FindBlob()
    {
        var candidates = new[]
        {
            Path.Combine(AppContext.BaseDirectory, "editor-assets.rmblob"),
            Path.Combine(AppContext.BaseDirectory, "..", "..", "..", "..", "level-editor", "editor-assets.rmblob"),
            Path.Combine(Directory.GetCurrentDirectory(), "tools", "level-editor", "editor-assets.rmblob"),
            Path.Combine(Directory.GetCurrentDirectory(), "editor-assets.rmblob"),
        };
        foreach (var c in candidates)
            if (File.Exists(c)) { _blob = Blob.Open(Path.GetFullPath(c)); if (_blob != null) return; }
    }

    private void LoadIntoViews()
    {
        _plan.Project = _project; _gl.Project = _project; _gl.Blob = _blob;
        RefreshMaps();
        RefreshLayers();
        SyncTerrainUi();
        ListParts(); ListMaterials();
        UpdateChecklist();
        _plan.Selected = null;
        BuildInspector(null);
        _plan.Invalidate(); _gl.Invalidate();
        Dispatcher.BeginInvoke(new Action(() => _plan.FitView()));
    }

    private void RefreshMaps()
    {
        MapList.Items.Clear();
        string query = MapSearch?.Text.Trim() ?? "";
        foreach (var item in _project.Chunks
            .Select((c, index) => new MapItem { Index = index, Text = $"Map {c.Number}: {c.Name}" })
            .OrderBy(item => _project.Chunks[item.Index].Number)
            .Where(item => query.Length == 0 || item.Text.Contains(query, StringComparison.OrdinalIgnoreCase)))
            MapList.Items.Add(item);
        MapList.SelectedItem = MapList.Items.OfType<MapItem>().FirstOrDefault(item => item.Index == _chunkIndex);
    }

    private void AddMap()
    {
        int n = _project.Chunks.Count == 0 ? 1 : _project.Chunks.Max(c => c.Number) + 1;
        var chunk = new Chunk { Number = n, Name = $"Map {n}", Gx = _project.Chunks.Count, Gy = 0 };
        chunk.Layers.Add(BlankLayer("Ground Floor", 0));
        _project.Chunks.Add(chunk);
        _chunkIndex = _project.Chunks.Count - 1; _layerIndex = 0;
        RefreshMaps(); RefreshLayers(); LoadIntoViews();
        SetStatus("New map added next door. Doors on the map edge take the player between maps in game.");
    }

    private void RefreshLayers()
    {
        LayerList.Items.Clear();
        if (Chunk == null) return;
        foreach (var l in Chunk.Layers) LayerList.Items.Add($"{l.Name}  (z {l.Z:0.#})");
        _layerIndex = Math.Min(_layerIndex, Math.Max(0, Chunk.Layers.Count - 1));
        if (LayerList.Items.Count > 0) LayerList.SelectedIndex = _layerIndex;
        _plan.Chunk = Chunk; _plan.LayerIndex = _layerIndex;
        _gl.FocusGx = Chunk.Gx; _gl.FocusGy = Chunk.Gy; _gl.FocusSet = true;
        SurfWallCombo.SelectedIndex = SurfaceIndex(Chunk.SurfaceWall);
        SurfFloorCombo.SelectedIndex = SurfaceIndex(Chunk.SurfaceFloor);
        SurfCeilCombo.SelectedIndex = SurfaceIndex(Chunk.SurfaceCeiling);
    }

    private static int SurfaceIndex(int id)
    {
        for (int i = 0; i < Rmc.SurfaceSets.Length; i++) if (Rmc.SurfaceSets[i].Id == id) return i;
        return 0;
    }

    private void ApplySurfaces()
    {
        if (Chunk == null) return;
        if (SurfWallCombo.SelectedIndex >= 0) Chunk.SurfaceWall = Rmc.SurfaceSets[SurfWallCombo.SelectedIndex].Id;
        if (SurfFloorCombo.SelectedIndex >= 0) Chunk.SurfaceFloor = Rmc.SurfaceSets[SurfFloorCombo.SelectedIndex].Id;
        if (SurfCeilCombo.SelectedIndex >= 0) Chunk.SurfaceCeiling = Rmc.SurfaceSets[SurfCeilCombo.SelectedIndex].Id;
        _gl.Invalidate();
    }

    private void AddFloor()
    {
        if (Chunk == null) return;
        float z = Chunk.Layers.Count == 0 ? 0 : Chunk.Layers.Max(l => l.Z + Math.Max(2.5f, l.CeilingHeight));
        Chunk.Layers.Add(BlankLayer($"Floor {Chunk.Layers.Count + 1}", z));
        _layerIndex = Chunk.Layers.Count - 1;
        RefreshLayers(); LoadIntoViews();
        SetStatus("Floor added above. 🪜 Upper floor tiles become its ground in the game.");
    }

    private void AfterEdit()
    {
        _plan.Invalidate();
        _gl.Invalidate();
        UpdateChecklist();
        KickAutoEngine();
    }

    // ---------------- browsers ----------------
    private BitmapImage? Img(string? url)
    {
        var bytes = _blob?.ForUrl(url);
        if (bytes == null) return null;
        try
        {
            var bi = new BitmapImage();
            using var ms = new MemoryStream(bytes);
            bi.BeginInit(); bi.CacheOption = BitmapCacheOption.OnLoad; bi.StreamSource = ms; bi.EndInit();
            bi.Freeze();
            return bi;
        }
        catch { return null; }
    }

    private void ListParts()
    {
        PartsList.Items.Clear();
        if (_blob == null) return;
        if (!_catsFilled)
        {
            PartCatCombo.Items.Add("✦ Everything");
            foreach (var cname in _blob.Prefabs.Select(p => p.Category).Distinct().OrderBy(c => c)) PartCatCombo.Items.Add(cname);
            PartCatCombo.SelectedIndex = 0;
            _catsFilled = true;
        }
        string q = PartSearch.Text.Trim().ToLowerInvariant();
        string cat = PartCatCombo.SelectedItem?.ToString() ?? "✦ Everything";
        int shown = 0;
        foreach (var pf in _blob.Prefabs)
        {
            if (cat != "✦ Everything" && pf.Category != cat) continue;
            if (q.Length > 0 && !(pf.Name + " " + pf.Category + " " + pf.Model).ToLowerInvariant().Contains(q)) continue;
            if (++shown > 150) break;   // never decode hundreds of thumbs at once
            PartsList.Items.Add(new PartVm
            {
                Name = pf.Name, Prefab = pf, Image = Img(pf.TextureUrl ?? pf.Texture),
                Emoji = pf.Category switch
                {
                    "Actors" => "🧍", "Creatures" => "👾", "Machinery" => "⚙️", "Architecture" => "🏛",
                    "Cargo" => "📦", "Clutter" => "🧺", "Pickups" => "🎁", "Environment" => "🌲",
                    "Reactor" => "☢️", "Weapons" => "🔫", _ => "🧊",
                },
            });
        }
    }

    private void ListMaterials()
    {
        MatsList.Items.Clear();
        if (_blob == null) return;
        foreach (var m in _blob.Materials)
            MatsList.Items.Add(new PartVm { Name = m.Name, Material = m, Image = Img(m.Url) });
    }

    private void UpdateChecklist()
    {
        if (Chunk == null) { ChecklistText.Text = "No map yet."; return; }
        var objs = _project.Chunks.SelectMany(c => c.Objects).ToList();
        bool hasFloor = Chunk.Layers.Any(l => l.Rows.Any(r => r.Contains('.')));
        string M(bool ok) => ok ? "☑  " : "☐  ";
        ChecklistText.Text =
            "YOUR CAMPAIGN, STEP BY STEP\n\n" +
            M(hasFloor) + "Paint a room (🏠 Drag-a-room)\n" +
            M(objs.Any(o => o.Type == "door")) + "Knock a 🚪 door into a wall\n" +
            M(objs.Any(o => o.Type == "light")) + "Drop in a 💡 lamp\n" +
            M(objs.Any(o => o.Type is "sign" or "terminal" or "asset")) + "Add 🧩 parts, signs, computers\n" +
            M(objs.Any(o => o.Type == "spawn" && o.SpawnKind != "Player")) + "People or monsters 🧍\n" +
            M(objs.Any(o => o.Type == "spawn" && o.SpawnKind == "Player")) + "Place the Player Start\n" +
            M(_playtested) + "▶ WALK inside it\n" +
            M(_exported) + "EXPORT .RMC\n\n" +
            "Then 📸 ENGINE SHOT shows the real game's render of your map, and ▶ TEST IN GAME plays it for real. No wrong answers - move things until it feels right.";
    }
}
