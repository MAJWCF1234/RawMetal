using System.Text.Json;

namespace RawMetalCampaignEditor;

public sealed partial class EditorForm : Form
{
    private Project _project = NewBlankProject();
    private readonly Mappings _maps = Mappings.Load();
    private Blob? _blob;
    private string _tool = "room";
    private int _layerIndex;
    private Prefab? _pickedPrefab;
    private MaterialItem? _pickedMaterial;
    private bool _playtested, _exported;

    private readonly PlanCanvas _plan = new();
    private readonly GlView _gl = new();
    private readonly SplitContainer _split = new();
    private string _viewMode = "dual";
    private readonly Label _status = new();
    private readonly Label _brand = new();
    private readonly ListBox _layers = new();
    private readonly ListView _prefabs = new();
    private readonly ListView _materials = new();
    private readonly TextBox _prefabSearch = new();
    private readonly Panel _inspector = new();
    private readonly TextBox _checklist = new();
    private readonly ComboBox _spawnRole = new();
    private readonly ComboBox _primKind = new();
    private readonly ComboBox _surfWall = new(), _surfFloor = new(), _surfCeil = new();
    private readonly Dictionary<string, Button> _toolButtons = new();
    private bool _assetsListed;

    private static readonly Color Ink = Color.FromArgb(13, 17, 22);
    private static readonly Color PanelC = Color.FromArgb(20, 26, 34);
    private static readonly Color Gold = Color.FromArgb(212, 168, 83);
    private static readonly Color Teal = Color.FromArgb(84, 190, 178);

    public EditorForm()
    {
        Text = "RAW METAL // CAMPAIGN EDITOR";
        Width = 1560; Height = 940;
        WindowState = FormWindowState.Maximized;
        BackColor = Ink; ForeColor = Color.FromArgb(225, 230, 236);
        Font = new Font("Segoe UI", 10f);
        KeyPreview = true;

        BuildLayout();
        FindBlob();
        LoadIntoViews();
        SetStatus(_blob != null
            ? $"Asset library loaded: {_blob.Prefabs.Count} prefabs, {_blob.Materials.Count} materials. Paint a room, or load the sample."
            : "No editor-assets.rmblob found - geometry still works; build the blob for model previews.");
    }

    private Chunk? Chunk => _project.Chunks.Count == 0 ? null
        : _project.Chunks[Math.Min(_project.Chunks.Count - 1, Math.Max(0, _chunkIndex))];
    private int _chunkIndex;

    private Layer? Layer => Chunk is { } c && _layerIndex < c.Layers.Count ? c.Layers[_layerIndex] : null;

    // ---------------- layout ----------------
    private void BuildLayout()
    {
        var top = new Panel { Dock = DockStyle.Top, Height = 96, BackColor = PanelC };
        _brand.Text = "RAW METAL  //  CAMPAIGN EDITOR";
        _brand.Font = new Font("Segoe UI", 17f, FontStyle.Bold);
        _brand.ForeColor = Gold; _brand.AutoSize = true; _brand.Location = new Point(14, 8);
        var sub = new Label
        {
            Text = "Paint rooms. See them in 3D instantly. Walk them. Ship .rmc world files the game runs as-is.",
            AutoSize = true, Location = new Point(16, 44), ForeColor = Color.FromArgb(170, 180, 192),
        };
        top.Controls.Add(_brand); top.Controls.Add(sub);

        var buttons = new (string, Action)[]
        {
            ("New", () => { _project = NewBlankProject(); _chunkIndex = 0; _layerIndex = 0; LoadIntoViews(); SetStatus("Blank campaign. Paint a room."); }),
            ("Open Project", OpenProject),
            ("Save Project", SaveProject),
            ("Open .RMC", OpenRmc),
            ("EXPORT .RMC", ExportRmc),
            ("LOAD SAMPLE BUILDING", LoadSample),
            ("⬒ DUAL VIEW", () => SetViewMode("dual")),
            ("PLAN ONLY", () => SetViewMode("plan")),
            ("3D ONLY", () => SetViewMode("3d")),
            ("▶ WALK PLAYTEST", StartWalk),
            ("◼ STOP", () => { _gl.ExitWalk(); SetViewMode("dual"); }),
            ("▦ WIREFRAME", () => { _gl.Wireframe = !_gl.Wireframe; _gl.Invalidate(); SetStatus(_gl.Wireframe ? "Wireframe: pure structure, like Hammer. Toggle again for textures." : "Textured view."); }),
            ("📸 ENGINE SHOT", EngineShot),
            ("▶ TEST IN ENGINE", TestInEngine),
        };
        int bx = 14;
        foreach (var (label, act) in buttons)
        {
            var b = new PillButton
            {
                Text = label, AutoSize = true, Location = new Point(bx, 62),
                Accent = label.StartsWith("EXPORT") ? Theme.Gold : Theme.Teal,
                Active = label.StartsWith("EXPORT"),
            };
            b.Click += (_, _) => act();
            top.Controls.Add(b);
            bx += b.Width + 8;
        }
        Controls.Add(top);

        _status.Dock = DockStyle.Bottom; _status.Height = 26;
        _status.BackColor = PanelC; _status.ForeColor = Teal;
        _status.Padding = new Padding(10, 4, 0, 0);
        Controls.Add(_status);

        var body = new TableLayoutPanel { Dock = DockStyle.Fill, ColumnCount = 3, RowCount = 1 };
        body.ColumnStyles.Add(new ColumnStyle(SizeType.Absolute, 205));
        body.ColumnStyles.Add(new ColumnStyle(SizeType.Percent, 100));
        body.ColumnStyles.Add(new ColumnStyle(SizeType.Absolute, 330));
        Controls.Add(body);
        body.BringToFront();

        body.Controls.Add(BuildToolsPanel(), 0, 0);

        // Dual view is the default way of working: plan on the left, the
        // live 3D on the right, both updating in the same frame. The
        // splitter is draggable, which is what makes ultrawide pleasant.
        _split.Dock = DockStyle.Fill;
        _split.SplitterWidth = 7;
        _split.BackColor = Theme.Edge;
        _plan.Dock = DockStyle.Fill;
        _gl.Dock = DockStyle.Fill;
        _split.Panel1.Controls.Add(_plan);
        _split.Panel2.Controls.Add(_gl);
        body.Controls.Add(_split, 1, 0);
        Shown += (_, _) =>
        {
            try { _split.SplitterDistance = Math.Max(320, (int)(_split.Width * .52)); } catch { }
        };

        body.Controls.Add(BuildRightPanel(), 2, 0);

        _plan.Project = _project;
        _plan.TileAction = PaintTile;
        _plan.RectAction = RectFillRoom;
        _plan.PlaceAction = PlaceAt;
        _plan.PickAction = o => { _plan.Selected = o; BuildInspector(o); _plan.Invalidate(); };
        _plan.DragAction = (o, wx, wy) =>
        {
            if (_tool != "select") return;
            o.X = Math.Clamp(wx, 0, 24); o.Y = Math.Clamp(wy, 0, 24);
            if (o.Type is "door" or "window") { o.TileX = (int)o.X; o.TileY = (int)o.Y; }
            _gl.Invalidate();
        };
        _gl.Status += SetStatus;
    }

    private Panel BuildToolsPanel()
    {
        var p = new Panel { Dock = DockStyle.Fill, BackColor = PanelC, AutoScroll = true };
        var title = new Label { Text = "TOOLS", ForeColor = Gold, Font = new Font("Segoe UI", 11f, FontStyle.Bold), Location = new Point(10, 8), AutoSize = true };
        p.Controls.Add(title);
        // MS-Paint rule: every tool is a picture-word a first-timer
        // understands without a manual. No engine vocabulary here.
        var tools = new (string Id, string Label)[]
        {
            ("select","✋ Move things"), ("roombox","🏠 Drag-a-room"), ("room","🖌 Paint floor"), ("wall","🧱 Wall block"),
            ("deck","🪜 Upper floor"), ("void","🧽 Rub out"), ("finish","🎨 Paint colour"),
            ("door","🚪 Door"), ("window","🪟 Window"), ("light","💡 Lamp"), ("sign","🪧 Sign"),
            ("prim","🏛 Shapes"), ("terminal","💻 Computer"), ("hazard","⚠ Danger zone"),
            ("people","🧍 People + monsters"), ("stairs","Stairs"), ("box","📦 Big block"), ("erase","💥 Remove thing"),
        };
        int y = 38;
        foreach (var (id, label) in tools)
        {
            var b = new PillButton
            {
                Text = label, Location = new Point(10, y), Width = 175, Height = 32,
                Accent = Theme.Gold,
            };
            b.Click += (_, _) => SetTool(id);
            p.Controls.Add(b); _toolButtons[id] = b; y += 38;
        }
        SetToolVisual();

        y += 6;
        p.Controls.Add(new Label { Text = "Person role", Location = new Point(10, y), AutoSize = true, ForeColor = Color.Silver });
        _spawnRole.Location = new Point(10, y + 22); _spawnRole.Width = 175; _spawnRole.DropDownStyle = ComboBoxStyle.DropDownList;
        _spawnRole.Items.AddRange(new object[] { "Player", "Worker", "Scientist", "FemaleScientist", "FemaleDoctor", "LabTech", "Security", "Cargo", "Cleanup", "Investigator", "Hostile", "Creature" });
        _spawnRole.SelectedIndex = 0;
        p.Controls.Add(_spawnRole); y += 58;

        p.Controls.Add(new Label { Text = "Architecture kind", Location = new Point(10, y), AutoSize = true, ForeColor = Color.Silver });
        _primKind.Location = new Point(10, y + 22); _primKind.Width = 175; _primKind.DropDownStyle = ComboBoxStyle.DropDownList;
        _primKind.Items.AddRange(Rmc.PrimKinds); _primKind.SelectedIndex = 5; // arch
        p.Controls.Add(_primKind); y += 58;

        p.Controls.Add(new Label { Text = "Floors (layers)", Location = new Point(10, y), AutoSize = true, ForeColor = Color.Silver });
        _layers.Location = new Point(10, y + 22); _layers.Size = new Size(175, 84);
        _layers.SelectedIndexChanged += (_, _) => { if (_layers.SelectedIndex >= 0) { _layerIndex = _layers.SelectedIndex; _plan.LayerIndex = _layerIndex; _plan.Invalidate(); _gl.Invalidate(); } };
        p.Controls.Add(_layers);
        var addFloor = new Button { Text = "+ Add floor above", Location = new Point(10, y + 110), Width = 175, Height = 28, FlatStyle = FlatStyle.Flat, BackColor = Color.FromArgb(34, 44, 56), ForeColor = Color.White };
        addFloor.Click += (_, _) => AddFloor();
        p.Controls.Add(addFloor); y += 150;

        foreach (var (label, combo) in new[] { ("Wall finish", _surfWall), ("Floor finish", _surfFloor), ("Ceiling finish", _surfCeil) })
        {
            p.Controls.Add(new Label { Text = label, Location = new Point(10, y), AutoSize = true, ForeColor = Color.Silver });
            combo.Location = new Point(10, y + 22); combo.Width = 175; combo.DropDownStyle = ComboBoxStyle.DropDownList;
            foreach (var (id, name) in Rmc.SurfaceSets) combo.Items.Add($"{id}: {name}");
            combo.SelectedIndex = 0;
            combo.SelectedIndexChanged += (_, _) => ApplySurfaces();
            p.Controls.Add(combo); y += 56;
        }
        return p;
    }

    private Control BuildRightPanel()
    {
        var tabs = new TabControl { Dock = DockStyle.Fill };
        tabs.Appearance = TabAppearance.FlatButtons;

        var prefTab = new TabPage("Prefabs") { BackColor = PanelC };
        _prefabSearch.Dock = DockStyle.Top; _prefabSearch.PlaceholderText = "Search models (chair, door, generator...)";
        _prefabSearch.TextChanged += (_, _) => ListPrefabs();
        _prefabs.Dock = DockStyle.Fill; _prefabs.View = View.Tile; _prefabs.BackColor = Ink; _prefabs.ForeColor = Color.White;
        _prefabs.TileSize = new Size(150, 58);
        _prefabs.SelectedIndexChanged += (_, _) =>
        {
            if (_prefabs.SelectedItems.Count == 1 && _prefabs.SelectedItems[0].Tag is Prefab pf)
            { _pickedPrefab = pf; SetTool("asset"); SetStatus($"Placing {pf.Name} - click the plan to drop it."); }
        };
        prefTab.Controls.Add(_prefabs); prefTab.Controls.Add(_prefabSearch);
        tabs.TabPages.Add(prefTab);

        var matTab = new TabPage("Materials") { BackColor = PanelC };
        _materials.Dock = DockStyle.Fill; _materials.View = View.Tile; _materials.BackColor = Ink; _materials.ForeColor = Color.White;
        _materials.TileSize = new Size(150, 58);
        _materials.SelectedIndexChanged += (_, _) =>
        {
            if (_materials.SelectedItems.Count == 1 && _materials.SelectedItems[0].Tag is MaterialItem m)
            { _pickedMaterial = m; SetTool("finish"); SetStatus($"Painting finish {m.Name} - drag across tiles."); }
        };
        matTab.Controls.Add(_materials);
        tabs.TabPages.Add(matTab);

        var inspTab = new TabPage("Inspector") { BackColor = PanelC };
        _inspector.Dock = DockStyle.Fill; _inspector.AutoScroll = true; _inspector.BackColor = PanelC;
        inspTab.Controls.Add(_inspector);
        tabs.TabPages.Add(inspTab);

        var checkTab = new TabPage("Checklist") { BackColor = PanelC };
        _checklist.Dock = DockStyle.Fill; _checklist.Multiline = true; _checklist.ReadOnly = true;
        _checklist.BackColor = Ink; _checklist.ForeColor = Color.FromArgb(200, 210, 220);
        _checklist.Font = new Font("Segoe UI", 10.5f);
        checkTab.Controls.Add(_checklist);
        tabs.TabPages.Add(checkTab);
        return tabs;
    }

    // ---------------- state ----------------
    private void SetStatus(string s) => _status.Text = s;

    private void SetTool(string tool)
    {
        _tool = tool == "asset" ? "asset" : tool;
        _plan.ToolNeedsObjectPick = _tool is "select" or "erase";
        _plan.ToolUsesRect = _tool == "roombox";
        if (_tool == "roombox") SetStatus("Drag a rectangle, let go - a whole walled room appears.");
        SetToolVisual();
    }

    private void SetToolVisual()
    {
        foreach (var (id, b) in _toolButtons)
        {
            if (b is PillButton pb) { pb.Active = id == _tool; pb.Invalidate(); }
            else b.BackColor = id == _tool ? Gold : Color.FromArgb(34, 44, 56);
        }
    }

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
        RefreshLayers();
        if (!_assetsListed) { ListPrefabs(); ListMaterials(); _assetsListed = true; }
        UpdateChecklist();
        _plan.Selected = null;
        BuildInspector(null);
        _plan.Invalidate(); _gl.Invalidate();
    }

    private void RefreshLayers()
    {
        _layers.Items.Clear();
        if (Chunk == null) return;
        foreach (var l in Chunk.Layers) _layers.Items.Add($"{l.Name}  (z {l.Z:0.#})");
        _layerIndex = Math.Min(_layerIndex, Math.Max(0, Chunk.Layers.Count - 1));
        if (_layers.Items.Count > 0) _layers.SelectedIndex = _layerIndex;
        _plan.Chunk = Chunk; _plan.LayerIndex = _layerIndex;
        if (Chunk != null)
        {
            _surfWall.SelectedIndex = SurfaceIndex(Chunk.SurfaceWall);
            _surfFloor.SelectedIndex = SurfaceIndex(Chunk.SurfaceFloor);
            _surfCeil.SelectedIndex = SurfaceIndex(Chunk.SurfaceCeiling);
        }
    }

    private static int SurfaceIndex(int id)
    {
        for (int i = 0; i < Rmc.SurfaceSets.Length; i++) if (Rmc.SurfaceSets[i].Id == id) return i;
        return 0;
    }

    private void ApplySurfaces()
    {
        if (Chunk == null) return;
        Chunk.SurfaceWall = Rmc.SurfaceSets[Math.Max(0, _surfWall.SelectedIndex)].Id;
        Chunk.SurfaceFloor = Rmc.SurfaceSets[Math.Max(0, _surfFloor.SelectedIndex)].Id;
        Chunk.SurfaceCeiling = Rmc.SurfaceSets[Math.Max(0, _surfCeil.SelectedIndex)].Id;
    }

    private void AddFloor()
    {
        if (Chunk == null) return;
        float z = Chunk.Layers.Count == 0 ? 0 : Chunk.Layers.Max(l => l.Z + Math.Max(2.5f, l.CeilingHeight));
        Chunk.Layers.Add(BlankLayer($"Floor {Chunk.Layers.Count + 1}", z));
        _layerIndex = Chunk.Layers.Count - 1;
        RefreshLayers(); LoadIntoViews();
        SetStatus("Floor added. Deck tiles (=) become its floor in the game.");
    }

    private void SetViewMode(string mode)
    {
        _viewMode = mode;
        _split.Panel1Collapsed = mode == "3d";
        _split.Panel2Collapsed = mode == "plan";
        if (mode != "plan") { _gl.Project = _project; _gl.Blob = _blob; _gl.Invalidate(); }
        if (mode == "3d") _gl.Focus();
        if (mode != "3d") _plan.Invalidate();
    }

    private void ShowView(bool threeD) => SetViewMode(threeD ? "3d" : "dual");

    private void StartWalk()
    {
        ShowView(true);
        _gl.EnterWalk();
        _playtested = true;
        UpdateChecklist();
    }
}
