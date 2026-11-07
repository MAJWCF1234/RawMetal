using MouseButtons = System.Windows.Forms.MouseButtons;

namespace RawMetalCampaignEditor;

public partial class MainWindow
{
    // Painting & placing - same proven logic as before, new shell.
    private void PaintTile(int x, int y, MouseButtons button)
    {
        if (Chunk == null || Layer == null) return;
        var layer = Layer;
        if (button == MouseButtons.Right && _tool is "room" or "wall" or "deck" or "void")
        { SetTile(layer, x, y, '_'); return; }
        if (button != MouseButtons.Left) return;
        switch (_tool)
        {
            case "room": SetTile(layer, x, y, '.'); break;
            case "wall": SetTile(layer, x, y, '#'); break;
            case "deck": SetTile(layer, x, y, '='); break;
            case "void": SetTile(layer, x, y, '_'); break;
            case "finish":
                if (_pickedMaterial != null)
                {
                    if (GetTile(layer, x, y) == '_') SetTile(layer, x, y, '.');
                    layer.Materials[$"{x},{y}"] = _pickedMaterial.Path;
                    _plan.Invalidate(); _gl.Invalidate();
                }
                break;
        }
    }

    private void RectFillRoom(int x0, int y0, int x1, int y1)
    {
        if (Chunk == null || Layer == null) return;
        x0 = Math.Clamp(x0, 0, 23); x1 = Math.Clamp(x1, 0, 23);
        y0 = Math.Clamp(y0, 0, 23); y1 = Math.Clamp(y1, 0, 23);
        for (int y = y0; y <= y1; y++)
            for (int x = x0; x <= x1; x++)
                SetTile(Layer, x, y, x == x0 || x == x1 || y == y0 || y == y1 ? '#' : '.');
        AfterEdit();
        SetStatus("Room! Knock a 🚪 door into a wall, drop a 💡 lamp in, done.");
    }

    private static char GetTile(Layer l, int x, int y)
        => y < l.Rows.Count && x < l.Rows[y].Length ? l.Rows[y][x] : '_';

    private static void SetTile(Layer l, int x, int y, char q)
    {
        while (l.Rows.Count < 24) l.Rows.Add(new string('_', 24));
        var row = (l.Rows[y] + new string('_', 24))[..24].ToCharArray();
        if (row[x] == q) return;
        row[x] = q;
        l.Rows[y] = new string(row);
    }

    private void PlaceAt(float wx, float wy, MouseButtons button)
    {
        if (Chunk == null || Layer == null) return;
        PushUndo();
        if (button == MouseButtons.Right)
        {
            var hit = LastHit(wx, wy);
            if (hit != null && _tool is not ("room" or "wall" or "deck" or "void" or "roombox"))
            { Chunk.Objects.Remove(hit); AfterEdit(); }
            return;
        }
        if (button != MouseButtons.Left) return;
        var layer = Layer;
        Obj? o = null;
        switch (_tool)
        {
            case "erase":
                var hit = LastHit(wx, wy);
                if (hit != null) { Chunk.Objects.Remove(hit); AfterEdit(); }
                return;
            case "door" or "window":
            {
                int tx = (int)wx, ty = (int)wy;
                if (GetTile(layer, tx, ty) != '#') { SetStatus("Doors and windows sit in walls - paint a 🧱 wall block there first."); return; }
                o = new Obj { Type = _tool, Name = _tool, X = tx, Y = ty, TileX = tx, TileY = ty, W = .95f, D = .25f, H = 2.2f };
                break;
            }
            case "light":
                o = new Obj { Type = "light", Name = "Ceiling light", X = wx, Y = wy, Z = layer.Z + layer.CeilingHeight - .2f, LampMount = "Ceiling", Kelvin = 4000, Intensity = 1, Range = 7 };
                break;
            case "sign":
                o = new Obj { Type = "sign", Name = "Sign", X = wx, Y = wy, Z = layer.Z + 1.8f, W = 1.8f, H = .7f, Title = "NEW SIGN", Subtitle = "", Accent = "#d9a441" };
                break;
            case "prim":
                o = new Obj { Type = "prim", Name = "Architecture", X = wx, Y = wy, Z = EffectiveZ(layer), PrimKind = PrimKindCombo.SelectedItem?.ToString() ?? "arch", Material = 42, W = 2, D = 1, H = 3 };
                break;
            case "terminal":
                o = new Obj { Type = "terminal", Name = "Terminal", X = wx, Y = wy, Z = EffectiveZ(layer), W = 1, D = .8f, H = 1.3f };
                break;
            case "hazard":
                o = new Obj { Type = "hazard", Name = "Hazard", X = wx, Y = wy, Z = layer.Z, Kind = "Electrical", W = 2, D = 2, H = 1 };
                break;
            case "people":
            {
                string role = SpawnRoleCombo.SelectedItem?.ToString() ?? "Player";
                o = new Obj
                {
                    Type = "spawn", Name = role, SpawnKind = role, X = wx, Y = wy, Z = layer.Z, H = 1.8f, W = .6f, D = .6f,
                    Species = "Brute", Behaviour = role == "Security" ? "guard" : "civilian", Armed = role == "Security" ? "handgun" : "none",
                };
                break;
            }
            case "stairs":
                o = new Obj { Type = "stairs", Name = "Stairs", X = wx, Y = wy, Z = layer.Z, W = 2, D = 4, H = 3 };
                break;
            case "box":
                o = new Obj { Type = "box", Name = "Block", X = wx, Y = wy, Z = EffectiveZ(layer), W = 2, D = 2, H = 2 };
                break;
            case "asset":
                if (_pickedPrefab == null) { SetStatus("Pick a model in the 🧩 Parts tab first."); return; }
                o = new Obj
                {
                    Type = "asset", Name = _pickedPrefab.Name, Model = _pickedPrefab.Model,
                    Texture = _pickedPrefab.TextureUrl ?? _pickedPrefab.Texture,
                    X = wx, Y = wy, Z = EffectiveZ(layer), W = _pickedPrefab.W, D = _pickedPrefab.D, H = _pickedPrefab.H,
                };
                break;
            default:
                return;
        }
        if (_placeZ.HasValue) o.Rotation = _gl.BuildRotation;
        o.LayerId = layer.Id;
        Chunk.Objects.Add(o);
        _plan.Selected = o;
        AfterEdit();
        BuildInspector(o);
    }

    // Clutter on shelves, props on tables: placement height is a first-
    // class choice in the rail, not a number buried in an inspector.
    private float PlaceHeight => HeightCombo.SelectedIndex switch { 1 => .9f, 2 => 1.5f, 3 => 2.1f, _ => 0f };
    private float? _placeZ;   // set while placing from the 3D build view
    private float EffectiveZ(Layer layer) => _placeZ ?? (layer.Z + PlaceHeight);

    // A click in the 3D build view: same placement code as the plan,
    // aimed by raycast. One model, three editors (plan / 3D / walk).
    private void Place3D(float wx, float wy, float z)
    {
        var chunk = _project.Chunks.FirstOrDefault(c =>
            wx >= c.Gx * 24 && wx < c.Gx * 24 + 24 && wy >= c.Gy * 24 && wy < c.Gy * 24 + 24) ?? Chunk;
        if (chunk == null) return;
        PushUndo();
        _chunkIndex = _project.Chunks.IndexOf(chunk);
        _layerIndex = 0;
        for (int i = 0; i < chunk.Layers.Count; i++) if (chunk.Layers[i].Z <= z + .01f) _layerIndex = i;
        _plan.Chunk = chunk; _plan.LayerIndex = _layerIndex;
        _placeZ = z;
        PlaceAt(wx - chunk.Gx * 24, wy - chunk.Gy * 24, MouseButtons.Left);
        _placeZ = null;
        SyncGhost();
    }

    private void SyncGhost()
    {
        _gl.GhostPrefab = _tool == "asset" ? _pickedPrefab : null;
        _gl.GhostSize = _tool switch
        {
            "asset" when _pickedPrefab != null => (_pickedPrefab.W, _pickedPrefab.D, _pickedPrefab.H),
            "prim" => (2, 1, 3),
            "box" => (2, 2, 2),
            "terminal" => (1, .8f, 1.3f),
            "sign" => (1.8f, .3f, .7f),
            "people" => (.7f, .7f, 1.8f),
            "hazard" => (2, 2, 1),
            "stairs" => (2, 4, 3),
            "light" => (.4f, .4f, .15f),
            _ => (1, 1, 1),
        };
    }

    // ✨ FURNISH: the editor does the tedious-correct thing itself.
    // Lamps gridded across every room, a sign over every door - a bare
    // boxout becomes a place in one click, then she arranges by hand.
    private void FurnishRoom()
    {
        if (Chunk == null || Chunk.Layers.Count == 0) { SetStatus("Paint a room first, then ✨ FURNISH."); return; }
        var chunk = Chunk;
        var layer = chunk.Layers[0];
        PushUndo();
        int lamps = 0, signs = 0;
        var existing = chunk.Objects.Where(o => o.Type == "light").ToList();
        void AddLamp(float x, float y)
        {
            var lamp = new Obj
            {
                Type = "light", Name = "Ceiling light", LayerId = layer.Id,
                X = x, Y = y, Z = layer.Z + layer.CeilingHeight - .2f,
                LampMount = "Ceiling", Kelvin = 3200, Intensity = 1, Range = 8,
            };
            chunk.Objects.Add(lamp); existing.Add(lamp); lamps++;
        }
        for (int y = 1; y < 23; y++)
            for (int x = 1; x < 23; x++)
            {
                if (GetTile(layer, x, y) != '.' || x % 4 != 2 || y % 4 != 2) continue;
                if (existing.Any(l => Math.Abs(l.X - x) < 2.6f && Math.Abs(l.Y - y) < 2.6f)) continue;
                AddLamp(x + .5f, y + .5f);
            }
        if (lamps == 0)
        {
            // A room must never stay dark: light the floor tile that is
            // furthest from any lamp already there.
            float best = -1; (int X, int Y) spot = (0, 0);
            for (int y = 1; y < 23; y++)
                for (int x = 1; x < 23; x++)
                {
                    if (GetTile(layer, x, y) != '.') continue;
                    float d = existing.Count == 0 ? 99
                        : existing.Min(l => MathF.Sqrt((l.X - x) * (l.X - x) + (l.Y - y) * (l.Y - y)));
                    if (d > best) { best = d; spot = (x, y); }
                }
            if (best >= 0) AddLamp(spot.X + .5f, spot.Y + .5f);
        }
        foreach (var door in chunk.Objects.Where(o => o.Type == "door").ToList())
        {
            if (chunk.Objects.Any(o => o.Type == "sign" && Math.Abs(o.X - door.X) < 1.6f && Math.Abs(o.Y - door.Y) < 1.6f)) continue;
            chunk.Objects.Add(new Obj
            {
                Type = "sign", Name = "Door sign", LayerId = layer.Id,
                X = door.X, Y = door.Y, Z = layer.Z + 2.15f, W = 1.6f, H = .55f,
                Title = (chunk.Name ?? "ROOM").ToUpperInvariant(), Subtitle = "", Accent = "#d9a441",
            });
            signs++;
        }
        AfterEdit();
        SetStatus($"✨ Furnished: {lamps} lamp(s), {signs} door sign(s). Drag anything that's in the way - it's your room now.");
    }

    private Obj? LastHit(float wx, float wy)
    {
        if (Chunk == null) return null;
        for (int i = Chunk.Objects.Count - 1; i >= 0; i--)
        {
            var o = Chunk.Objects[i];
            if (Math.Abs(wx - o.X) <= Math.Max(.5f, o.W / 2) && Math.Abs(wy - o.Y) <= Math.Max(.5f, o.D / 2)) return o;
        }
        return null;
    }
}

