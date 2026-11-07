namespace RawMetalCampaignEditor;

public sealed partial class EditorForm
{
    // ---------------- painting & placing (the realtime loop) ----------------
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
                    _plan.Invalidate();
                }
                break;
        }
    }

    // Drag-a-room: one gesture, a walled room with a floor. The single
    // most Minecraft/Tiny-Glade-feeling action in the whole editor.
    private void RectFillRoom(int x0, int y0, int x1, int y1)
    {
        if (Chunk == null || Layer == null) return;
        x0 = Math.Clamp(x0, 0, 23); x1 = Math.Clamp(x1, 0, 23);
        y0 = Math.Clamp(y0, 0, 23); y1 = Math.Clamp(y1, 0, 23);
        for (int y = y0; y <= y1; y++)
            for (int x = x0; x <= x1; x++)
                SetTile(Layer, x, y, x == x0 || x == x1 || y == y0 || y == y1 ? '#' : '.');
        AfterEdit();
        SetStatus("Room! Knock a door into a wall with the 🚪 Door tool, drop a 💡 Lamp in, done.");
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
        if (button == MouseButtons.Right)
        {
            if (_tool == "erase") return;
            var hit = LastHit(wx, wy);
            if (hit != null && _tool is not ("room" or "wall" or "deck" or "void"))
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
                if (GetTile(layer, tx, ty) != '#') { SetStatus("Doors and windows go on wall tiles - paint a wall there first."); return; }
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
                o = new Obj { Type = "prim", Name = "Architecture", X = wx, Y = wy, Z = layer.Z, PrimKind = _primKind.SelectedItem?.ToString() ?? "arch", Material = 42, W = 2, D = 1, H = 3 };
                break;
            case "terminal":
                o = new Obj { Type = "terminal", Name = "Terminal", X = wx, Y = wy, Z = layer.Z, W = 1, D = .8f, H = 1.3f };
                break;
            case "hazard":
                o = new Obj { Type = "hazard", Name = "Hazard", X = wx, Y = wy, Z = layer.Z, Kind = "Electrical", W = 2, D = 2, H = 1 };
                break;
            case "people":
            {
                string role = _spawnRole.SelectedItem?.ToString() ?? "Player";
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
                o = new Obj { Type = "box", Name = "Block", X = wx, Y = wy, Z = layer.Z, W = 2, D = 2, H = 2 };
                break;
            case "asset":
                if (_pickedPrefab == null) { SetStatus("Pick a model in the Prefabs tab first."); return; }
                o = new Obj
                {
                    Type = "asset", Name = _pickedPrefab.Name, Model = _pickedPrefab.Model,
                    Texture = _pickedPrefab.TextureUrl ?? _pickedPrefab.Texture,
                    X = wx, Y = wy, Z = layer.Z, W = _pickedPrefab.W, D = _pickedPrefab.D, H = _pickedPrefab.H,
                };
                break;
            default:
                return;
        }
        o.LayerId = layer.Id;
        Chunk.Objects.Add(o);
        _plan.Selected = o;
        AfterEdit();
        BuildInspector(o);
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

    private void AfterEdit()
    {
        _plan.Invalidate();
        _gl.Invalidate();
        UpdateChecklist();
    }

    // ---------------- browsers ----------------
    private readonly Dictionary<string, Image> _thumbCache = new();

    private Image? Thumb(string? url)
    {
        if (url == null || _blob == null) return null;
        if (_thumbCache.TryGetValue(url, out var img)) return img;
        var src = _blob.TextureImage(url);
        if (src == null) return null;
        var bmp = new Bitmap(48, 48);
        using (var g = Graphics.FromImage(bmp)) g.DrawImage(src, 0, 0, 48, 48);
        src.Dispose();
        _thumbCache[url] = bmp;
        return bmp;
    }

    private void ListPrefabs()
    {
        _prefabs.Items.Clear();
        if (_blob == null) return;
        string q = _prefabSearch.Text.Trim().ToLowerInvariant();
        int shown = 0;
        foreach (var pf in _blob.Prefabs)
        {
            if (q.Length > 0 && !(pf.Name + " " + pf.Category + " " + pf.Model).ToLowerInvariant().Contains(q)) continue;
            // Cap the list: decoding hundreds of thumbnails in one go is
            // exactly the kind of freeze that makes an editor feel dead.
            // Type to narrow it down instead.
            if (++shown > 150) break;
            var item = new ListViewItem(pf.Name) { Tag = pf };
            var thumb = Thumb(pf.TextureUrl ?? pf.Texture);
            if (thumb != null)
            {
                // Tile view draws the image list image; share one growing list.
                SharedImages.Images.Add(pf.Id, thumb);
                item.ImageKey = pf.Id;
                _prefabs.LargeImageList = SharedImages;
                _prefabs.SmallImageList = SharedImages;
            }
            _prefabs.Items.Add(item);
            if (_prefabs.Items.Count > 900) break;
        }
    }

    private static readonly ImageList SharedImages = new() { ImageSize = new Size(48, 48) };

    private void ListMaterials()
    {
        _materials.Items.Clear();
        if (_blob == null) return;
        foreach (var m in _blob.Materials)
        {
            var item = new ListViewItem($"{m.Name}") { Tag = m };
            var thumb = Thumb(m.Url);
            if (thumb != null)
            {
                if (!SharedImages.Images.ContainsKey(m.Id)) SharedImages.Images.Add(m.Id, thumb);
                item.ImageKey = m.Id;
                _materials.LargeImageList = SharedImages;
                _materials.SmallImageList = SharedImages;
            }
            _materials.Items.Add(item);
        }
    }

    // ---------------- checklist ----------------
    private void UpdateChecklist()
    {
        if (Chunk == null) { _checklist.Text = "No map yet."; return; }
        var objs = _project.Chunks.SelectMany(c => c.Objects).ToList();
        bool hasFloor = Chunk.Layers.Any(l => l.Rows.Any(r => r.Contains('.')));
        bool Check(bool ok) => ok;
        string M(bool ok) => ok ? "☑" : "☐";
        _checklist.Text =
            "CAMPAIGN CHECKLIST\r\n\r\n" +
            $"{M(hasFloor)} Rooms painted\r\n" +
            $"{M(objs.Any(o => o.Type == "door"))} A door on a wall\r\n" +
            $"{M(objs.Any(o => o.Type == "light"))} Lights placed\r\n" +
            $"{M(objs.Any(o => o.Type is "sign" or "terminal" or "asset"))} Signs / equipment\r\n" +
            $"{M(objs.Any(o => o.Type == "spawn" && o.SpawnKind != "Player"))} People or creatures\r\n" +
            $"{M(objs.Any(o => o.Type == "spawn" && o.SpawnKind == "Player"))} Player Start placed\r\n" +
            $"{M(_playtested)} Playtested with WALK (or engine)\r\n" +
            $"{M(_exported)} Exported .rmc\r\n\r\n" +
            "Then: 📸 ENGINE SHOT shows the real game render.\r\n▶ TEST IN ENGINE plays it for real.";
        _ = Check(true);
    }
}
