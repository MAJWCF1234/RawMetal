using System.IO.Compression;
using System.Text.Json;

namespace RawMetalCampaignEditor;

// ---------- Project model (mirrors the web editor's project JSON) ----------
public sealed class Project
{
    public string Name { get; set; } = "Depthworks Blockout";
    public string Skybox { get; set; } = "industrial_night";
    public List<Chunk> Chunks { get; set; } = new();
    public List<string> RuntimeRecords { get; set; } = new();
    // Outdoor terrain (campaign-wide, world metres - the Starter Map's
    // TERRAIN / HORIZON / GROUND / TERRAIN_FLAT records).
    public bool TerrainEnabled { get; set; }
    public int TerrainSeed { get; set; } = 2026;
    public float TerrainBase { get; set; } = 1;
    public float TerrainAmplitude { get; set; } = 2.5f;
    public float TerrainScale { get; set; } = 30;
    public float TerrainRoughness { get; set; } = .45f;
    public float HorizonDepth { get; set; } = 110;
    public float HorizonSpacing { get; set; } = 5;
    public float HorizonRise { get; set; } = 9;
    public string HorizonModels { get; set; } = "121,122,121,118,150,120";
    public List<GroundRect> Ground { get; set; } = new();
    public List<FlatRect> Flats { get; set; } = new();
}

public sealed class GroundRect
{
    public float X1 { get; set; } public float Y1 { get; set; }
    public float X2 { get; set; } public float Y2 { get; set; }
    public int Kind { get; set; } public float Edge { get; set; } = 2;
}

public sealed class FlatRect
{
    public float X1 { get; set; } public float Y1 { get; set; }
    public float X2 { get; set; } public float Y2 { get; set; }
    public float Height { get; set; } = 1; public float Blend { get; set; } = 8;
}

public sealed class Chunk
{
    public string Id { get; set; } = Guid.NewGuid().ToString("N")[..8];
    public int Number { get; set; } = 1;
    public string Name { get; set; } = "Pump Annex";
    public int Gx { get; set; }
    public int Gy { get; set; }
    public int SurfaceWall { get; set; } = -1;
    public int SurfaceFloor { get; set; } = -1;
    public int SurfaceCeiling { get; set; } = -1;
    // MAP records carry environment tuning per map.  Keeping the raw tail
    // avoids silently flattening an imported campaign to one skybox.
    public List<string> MapTail { get; set; } = new();
    public List<Layer> Layers { get; set; } = new();
    public List<Obj> Objects { get; set; } = new();
}

public sealed class Layer
{
    public string Id { get; set; } = Guid.NewGuid().ToString("N")[..8];
    public string Name { get; set; } = "Ground Floor";
    public float Z { get; set; }
    public float CeilingHeight { get; set; } = 3;
    public float Thickness { get; set; }
    public List<string> Rows { get; set; } = new();
    public Dictionary<string, string> Materials { get; set; } = new();
}

public sealed class Obj
{
    public string Id { get; set; } = Guid.NewGuid().ToString("N")[..8];
    public string LayerId { get; set; } = "";
    public string Type { get; set; } = "box";
    public string Name { get; set; } = "";
    public float X { get; set; }
    public float Y { get; set; }
    public float Z { get; set; }
    public float Rotation { get; set; }
    public float W { get; set; } = 1;
    public float D { get; set; } = 1;
    public float H { get; set; } = 1;
    public string Mount { get; set; } = "floor";
    // asset
    public string? Model { get; set; }
    public string? Texture { get; set; }
    // Engine fixture slots are the complete in-game catalogue.  A project can
    // contain models that have no editor-asset thumbnail yet; retain their
    // slot and dimensions so they remain visible/editable and round-trip.
    public int EngineFixture { get; set; } = -1;
    public string? EngineFixtureName { get; set; }
    public List<string> EngineFixtureTail { get; set; } = new();
    // light
    public string? LampMount { get; set; }
    public float Intensity { get; set; } = 1;
    public float Range { get; set; } = 7;
    public int Kelvin { get; set; } = 4000;
    public string? Gel { get; set; }
    // sign
    public string? Title { get; set; }
    public string? Subtitle { get; set; }
    public string? Accent { get; set; }
    // prim
    public string? PrimKind { get; set; }
    public int Material { get; set; } = 42;
    // hazard / spawn
    public string? Kind { get; set; }
    public string? SpawnKind { get; set; }
    public string? Species { get; set; }
    public string? Behaviour { get; set; }
    public string? Armed { get; set; }
    // door
    public string? WallAxis { get; set; }
    public int TileX { get; set; }
    public int TileY { get; set; }
    // Physics and stair settings carried by the native .rmc format.
    // Defaults retain the editor's simple placement behaviour.
    public int BreakMaterial { get; set; } = -1;
    public float BreakHealth { get; set; }
    public bool Holds { get; set; }
    public string? BreakState { get; set; }
    public string? BreakTrigger { get; set; }
    public int StairSteps { get; set; } = 17;
    public bool StairAlongY { get; set; } = true;
    public bool StairAscending { get; set; } = true;
    public bool StairOpenUnderside { get; set; }
    public float StairTread { get; set; } = .12f;
    public bool StairSideRails { get; set; }
}

// ---------- Asset blob (editor-assets.rmblob) ----------
public sealed class Prefab
{
    public string Id = "", Name = "", Category = "", Model = "", Ext = ".obj";
    public string? ModelUrl, Texture, TextureUrl;
    public Dictionary<string, string> MaterialUrls = new(StringComparer.OrdinalIgnoreCase);
    public float W = 1, D = 1, H = 1;
}

public sealed class MaterialItem
{
    public string Id = "", Name = "", Category = "", Path = "", Url = "";
}

public sealed class Blob : IDisposable
{
    private readonly ZipArchive _zip;
    public List<Prefab> Prefabs { get; } = new();
    public List<MaterialItem> Materials { get; } = new();
    public Dictionary<string, string> Registry { get; } = new();

    private Blob(ZipArchive zip) { _zip = zip; }

    public static Blob? Open(string path)
    {
        if (!File.Exists(path)) return null;
        var zip = ZipFile.OpenRead(path);
        var manifestEntry = zip.GetEntry("manifest.json");
        if (manifestEntry == null) return null;
        using var reader = new StreamReader(manifestEntry.Open());
        using var doc = JsonDocument.Parse(reader.ReadToEnd());
        var root = doc.RootElement;
        var blob = new Blob(zip);
        foreach (var p in root.GetProperty("prefabs").EnumerateArray())
        {
            blob.Prefabs.Add(new Prefab
            {
                Id = Str(p, "id"), Name = Str(p, "name"), Category = Str(p, "category"),
                Model = Str(p, "model"), ModelUrl = Str(p, "modelUrl", null),
                Texture = Str(p, "texture", null), TextureUrl = Str(p, "textureUrl", null),
                Ext = Str(p, "ext"), W = Num(p, "w", 1), D = Num(p, "d", 1), H = Num(p, "h", 1),
            });
            var prefab = blob.Prefabs[^1];
            if (p.TryGetProperty("materialUrls", out var materialUrls) && materialUrls.ValueKind == JsonValueKind.Object)
                foreach (var material in materialUrls.EnumerateObject())
                    if (material.Value.ValueKind == JsonValueKind.String)
                        prefab.MaterialUrls[material.Name] = material.Value.GetString() ?? "";
        }
        foreach (var m in root.GetProperty("materials").EnumerateArray())
            blob.Materials.Add(new MaterialItem
            {
                Id = Str(m, "id"), Name = Str(m, "name"), Category = Str(m, "category"),
                Path = Str(m, "path"), Url = Str(m, "url"),
            });
        if (root.TryGetProperty("registry", out var reg))
            foreach (var kv in reg.EnumerateObject())
                blob.Registry[kv.Name] = kv.Value.GetString() ?? "";
        return blob;
    }

    private static string Str(JsonElement e, string key, string? fallback = "")
        => e.TryGetProperty(key, out var v) && v.ValueKind == JsonValueKind.String ? v.GetString() ?? fallback ?? "" : fallback ?? "";
    private static float Num(JsonElement e, string key, float fallback)
        => e.TryGetProperty(key, out var v) && v.ValueKind == JsonValueKind.Number ? (float)v.GetDouble() : fallback;

    public byte[]? Entry(string name)
    {
        var entry = _zip.GetEntry(name);
        if (entry == null) return null;
        using var stream = entry.Open();
        using var ms = new MemoryStream();
        stream.CopyTo(ms);
        return ms.ToArray();
    }

    public byte[]? ForUrl(string? url)
    {
        if (string.IsNullOrEmpty(url)) return null;
        if (url.StartsWith("/__blob/")) return Entry(url["/__blob/".Length..]);
        if (Registry.TryGetValue(url.TrimStart('/'), out var entry)) return Entry(entry);
        return null;
    }

    public Image? TextureImage(string? urlOrPath)
    {
        var bytes = ForUrl(urlOrPath);
        if (bytes == null) return null;
        // Image.FromStream keeps the source stream alive lazily.  The old
        // implementation disposed that stream before OpenGL locked the
        // pixels, which made blob textures silently degrade to flat fixture
        // colours on some GDI+ drivers.  Clone while the stream is alive.
        try
        {
            using var ms = new MemoryStream(bytes);
            using var source = Image.FromStream(ms);
            return new Bitmap(source);
        }
        catch { return null; }
    }

    public void Dispose() => _zip.Dispose();
}

// ---------- Engine placement mappings (generated from the exporter) ----------
public sealed class Mappings
{
    public Dictionary<string, int> Facility = new();
    public Dictionary<string, int> Prop = new();
    public Dictionary<string, int> Clutter = new();
    public Dictionary<string, string> Pickup = new();
    public Dictionary<string, int> PickupIndex = new();
    public Dictionary<string, string> Creature = new();
    public Dictionary<int, string> FixtureNames = new();

    public static Mappings Load()
    {
        var m = new Mappings();
        foreach (var dir in new[] { AppContext.BaseDirectory,
                                    Path.Combine(AppContext.BaseDirectory, "..", "..", "..", "..", "level-editor"),
                                    Path.Combine(Directory.GetCurrentDirectory(), "tools", "level-editor") })
        {
            var path = Path.Combine(dir, "mappings.json");
            if (!File.Exists(path)) continue;
            using var doc = JsonDocument.Parse(File.ReadAllText(path));
            var r = doc.RootElement;
            Fill(r, "facility", m.Facility); Fill(r, "prop", m.Prop);
            Fill(r, "clutter", m.Clutter); Fill(r, "pickupIndex", m.PickupIndex);
            if (r.TryGetProperty("pickup", out var pk))
                foreach (var kv in pk.EnumerateObject()) m.Pickup[kv.Name] = kv.Value.GetString() ?? "";
            if (r.TryGetProperty("creature", out var cr))
                foreach (var kv in cr.EnumerateObject()) m.Creature[kv.Name] = kv.Value.GetString() ?? "";
            LoadFixtureNames(m);
            return m;
        }
        LoadFixtureNames(m);
        return m;
    }

    private static void LoadFixtureNames(Mappings mappings)
    {
        // ModelIds.h is generated from the exact engine catalogue.  Using it
        // here keeps the editor's visual lookup aligned with the game instead
        // of a tiny hand-maintained subset of mappings.json.
        foreach (var path in new[]
        {
            Path.Combine(Directory.GetCurrentDirectory(), "src", "world", "ModelIds.h"),
            Path.Combine(AppContext.BaseDirectory, "ModelIds.h"),
        })
        {
            if (!File.Exists(path)) continue;
            foreach (var line in File.ReadLines(path))
            {
                var match = System.Text.RegularExpressions.Regex.Match(line, @"inline constexpr int ([A-Za-z0-9_]+)=(\d+);");
                if (match.Success) mappings.FixtureNames[int.Parse(match.Groups[2].Value)] = match.Groups[1].Value;
            }
            if (mappings.FixtureNames.Count > 0) return;
        }
    }

    private static void Fill(JsonElement root, string key, Dictionary<string, int> dst)
    {
        if (root.TryGetProperty(key, out var e))
            foreach (var kv in e.EnumerateObject()) dst[kv.Name] = kv.Value.GetInt32();
    }
}
