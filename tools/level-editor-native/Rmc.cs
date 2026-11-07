using System.Globalization;

namespace RawMetalCampaignEditor;

// Export/import of .rmc world files. A C# twin of the web editor's
// build_runtime_campaign / parse_campaign_project: same records, same
// meanings, so files from either editor load in the game and each other.
public static class Rmc
{
    private static readonly CultureInfo Inv = CultureInfo.InvariantCulture;
    private static string F(float v) => v.ToString("0.###", Inv);
    private static float Num(string s, float fallback = 0)
        => float.TryParse(s, NumberStyles.Float, Inv, out var v) ? v : fallback;
    private static string Esc(string s) => Uri.EscapeDataString(s).Replace("%20", " ");

    public static readonly string[] PrimKinds =
    {
        "box","pillar","stairs","ramp","column","arch","vault","doorframe","window","fins",
        "canopy","railing","corbel","plinth","slit","clerestory","embrasure","roundel","niche",
        "hood","batter","waffle","pilotis","megastep","lightwell","bridge","core","dome",
        "strut","girder","quadpier","band","spout","chase","lattice",
    };

    public static readonly (int Id, string Name)[] SurfaceSets =
    {
        (-1,"Game default"),(0,"Concrete"),(1,"Pressure works"),(2,"Brick"),(3,"Clinic tile"),
        (4,"Lab panel"),(5,"Rock"),(6,"Corrugated"),(7,"Office plaster"),(8,"Mine timber"),
        (9,"Ancient stone"),(10,"Clinic paint teal"),(11,"Clinic paint green"),(12,"Clinic paint maroon"),
    };

    public static readonly string[] CreatureSpecies = { "Huntsman", "Wasp", "Brute", "Warden", "Mutant" };

    public static string Export(Project project, string campaignName, Mappings maps, List<string> warnings)
    {
        var chunks = project.Chunks.OrderBy(c => c.Number).ToList();
        var grid = chunks.ToDictionary(c => (c.Gx, c.Gy), c => chunks.IndexOf(c));
        int startMap = 0;
        for (int i = 0; i < chunks.Count; i++)
            if (chunks[i].Objects.Any(o => o.Type == "spawn" && o.SpawnKind == "Player")) { startMap = i; break; }

        var lines = new List<string> { $"CAMPAIGN|{Esc(campaignName)}|{startMap}" };
        var authored = new List<string>(project.RuntimeRecords);
        if (project.TerrainEnabled)
        {
            lines.Add($"TERRAIN|{project.TerrainSeed}|{F(project.TerrainBase)}|{F(project.TerrainAmplitude)}|{F(project.TerrainScale)}|{F(project.TerrainRoughness)}");
            lines.Add($"HORIZON|{F(project.HorizonDepth)}|{F(project.HorizonSpacing)}|{F(project.HorizonRise)}|{project.HorizonModels}");
            foreach (var g in project.Ground)
                lines.Add($"GROUND|{F(g.X1)}|{F(g.Y1)}|{F(g.X2)}|{F(g.Y2)}|{g.Kind}|{F(g.Edge)}");
            foreach (var fl in project.Flats)
                lines.Add($"TERRAIN_FLAT|{F(fl.X1)}|{F(fl.Y1)}|{F(fl.X2)}|{F(fl.Y2)}|{F(fl.Height)}|{F(fl.Blend)}");
        }

        for (int mi = 0; mi < chunks.Count; mi++)
        {
            var chunk = chunks[mi];
            var layers = chunk.Layers.OrderBy(l => l.Z).ToList();
            if (layers.Count == 0) continue;
            var baseLayer = layers[0];
            float baseZ = baseLayer.Z;
            var byId = layers.ToDictionary(l => l.Id);
            var objects = chunk.Objects;

            var player = objects.FirstOrDefault(o => o.Type == "spawn" && o.SpawnKind == "Player");
            float spawnX = player?.X ?? 3.5f, spawnY = player?.Y ?? 3.5f, spawnZ = player?.Z ?? baseZ;
            float top = layers.Max(l => l.Z + Math.Max(.5f, l.CeilingHeight));
            bool north = grid.ContainsKey((chunk.Gx, chunk.Gy - 1)), south = grid.ContainsKey((chunk.Gx, chunk.Gy + 1));
            bool west = grid.ContainsKey((chunk.Gx - 1, chunk.Gy)), east = grid.ContainsKey((chunk.Gx + 1, chunk.Gy));
            lines.Add($"MAP|{mi}|{Esc(chunk.Name)}|{chunk.Gx * 24}|{chunk.Gy * 24}|{F(spawnX)}|{F(spawnY)}|{F(spawnZ)}|0|{F(top)}|0.27|{Esc(project.Skybox)}|{(north ? 1 : 0)}|{(south ? 1 : 0)}|{(west ? 1 : 0)}|{(east ? 1 : 0)}|0");

            // Rows, with door/window openings carved.
            var exported = new Dictionary<string, char[][]>();
            foreach (var (layer, li) in layers.Select((l, i) => (l, i)))
            {
                var rows = new char[24][];
                for (int y = 0; y < 24; y++)
                {
                    string raw = li >= 0 && y < layer.Rows.Count ? layer.Rows[y] : "";
                    raw = (raw + new string('_', 24))[..24];
                    rows[y] = raw.ToCharArray();
                    if (li > 0)
                        for (int x = 0; x < 24; x++) rows[y][x] = rows[y][x] == '_' ? '_' : '=';
                }
                exported[layer.Id] = rows;
            }
            var openings = new Dictionary<(string, int, int), Obj>();
            foreach (var o in objects.Where(o => o.Type is "door" or "window"))
            {
                if (!byId.TryGetValue(o.LayerId, out var layer)) continue;
                int tx = Math.Clamp((int)o.X, 0, 23), ty = Math.Clamp((int)o.Y, 0, 23);
                string raw = ty < layer.Rows.Count ? layer.Rows[ty] : "";
                if (tx >= raw.Length || raw[tx] != '#') continue;
                var rows = exported[layer.Id];
                rows[ty][tx] = layer == baseLayer ? '.' : '=';
                openings[(layer.Id, tx, ty)] = o;
            }
            foreach (var (layer, li) in layers.Select((l, i) => (l, i)))
            {
                float thickness = li > 0 && layer.Thickness <= 0 ? .25f : layer.Thickness;
                lines.Add($"LAYER|{mi}|{li}|{Esc(layer.Name)}|{F(layer.Z)}|{F(thickness)}");
                var rows = exported[layer.Id];
                for (int y = 0; y < 24; y++) lines.Add($"ROW|{mi}|{li}|{y}|{new string(rows[y])}");
            }
            // Upper-layer walls become structures; openings get their pieces.
            foreach (var (layer, li) in layers.Select((l, i) => (l, i)))
            {
                if (li == 0) continue;
                float ceiling = layer.Z + Math.Max(.5f, layer.CeilingHeight);
                for (int y = 0; y < 24; y++)
                for (int x = 0; x < 24; x++)
                {
                    string raw = y < layer.Rows.Count ? layer.Rows[y] : "";
                    if (x < raw.Length && raw[x] == '#' && !openings.ContainsKey((layer.Id, x, y)))
                        lines.Add($"STRUCT|{mi}|{x}|{y}|{x + 1}|{y + 1}|{F(layer.Z)}|{F(ceiling)}|0|3");
                }
            }

            var actorLines = new List<string>();
            int actorCount = 0;
            foreach (var o in objects)
            {
                var layer = byId.TryGetValue(o.LayerId, out var found) ? found : baseLayer;
                switch (o.Type)
                {
                    case "door":
                    {
                        float width = Math.Clamp(o.W, .25f, 12f);
                        bool entry = o.Y <= 1.25f, transfer = o.Y >= 22.75f;
                        lines.Add($"DOOR|{mi}|{F(o.X - width / 2)}|{F(o.X + width / 2)}|{F(o.Y)}|{F(layer.Z - baseZ)}|{(entry ? 1 : 0)}|{(transfer ? 1 : 0)}|{(!entry && !transfer ? 1 : 0)}|0|-1");
                        break;
                    }
                    case "box":
                        lines.Add($"STRUCT|{mi}|{F(o.X - o.W / 2)}|{F(o.Y - o.D / 2)}|{F(o.X + o.W / 2)}|{F(o.Y + o.D / 2)}|{F(o.Z)}|{F(o.Z + Math.Max(.05f, o.H))}|0|2");
                        break;
                    case "stairs":
                    {
                        float topZ = o.Z + Math.Max(.25f, o.H);
                        lines.Add($"STAIR|{mi}|{F(o.X - o.W / 2)}|{F(o.Y - o.D / 2)}|{F(o.X + o.W / 2)}|{F(o.Y + o.D / 2)}|{F(o.Z)}|{F(topZ)}|17|1|1|0|0.12|0");
                        break;
                    }
                    case "light":
                    {
                        string line = $"LIGHT|{mi}|{F(o.X)}|{F(o.Y)}|{F(o.Z)}";
                        int mount = o.LampMount switch { "wall" => 1, "bare" => 2, _ => 0 };
                        bool extended = o.LampMount != null || o.Gel != null || o.Intensity != 1 || o.Kelvin != 4000;
                        if (extended)
                        {
                            float yaw = o.Rotation * MathF.PI / 180;
                            var tail = new List<string> { mount.ToString(), F(yaw), F(o.Intensity), F(o.Range), o.Kelvin.ToString() };
                            if (o.Gel != null && o.Gel.Length >= 7)
                            {
                                tail.AddRange(Enumerable.Repeat("", 12));
                                for (int c = 0; c < 3; c++)
                                    tail.Add(F(Convert.ToInt32(o.Gel.Substring(1 + c * 2, 2), 16) / 255f));
                            }
                            line += "|" + string.Join("|", tail);
                        }
                        lines.Add(line);
                        break;
                    }
                    case "sign":
                    case "label":
                    {
                        int accent = 0xd8a03c;
                        if (o.Accent != null && o.Accent.Length >= 7)
                            int.TryParse(o.Accent[1..], NumberStyles.HexNumber, Inv, out accent);
                        float yaw = o.Rotation * MathF.PI / 180;
                        lines.Add($"SIGN|{mi}|{F(o.X)}|{F(o.Y)}|{F(o.Z)}|{F(Math.Max(.1f, o.W))}|{F(Math.Max(.1f, o.H))}|{F(yaw)}|{Esc(o.Title ?? o.Name)}|{Esc(o.Subtitle ?? "")}|{accent}");
                        break;
                    }
                    case "prim":
                    {
                        int facing = ((int)Math.Round(o.Rotation / 90) % 4 + 4) % 4;
                        lines.Add($"PRIM|{mi}|{Esc(o.PrimKind ?? "box")}|{F(o.X)}|{F(o.Y)}|{F(o.Z)}|{F(o.W)}|{F(o.D)}|{F(o.H)}|{facing}|{o.Material}|1");
                        break;
                    }
                    case "terminal":
                        lines.Add($"TERMINAL|{mi}|{F(o.X)}|{F(o.Y)}|{F(o.Z - baseZ)}|0|{Esc(o.Title ?? o.Name)}|AUTHORED IN LEVEL EDITOR.|LOCAL TERMINAL.||0||{F(o.Rotation * MathF.PI / 180)}");
                        break;
                    case "hazard":
                    {
                        int kind = o.Kind switch { "Steam" => 1, "Crusher" => 2, "Toxic" => 3, "Fire" => 4, "FallingDebris" => 5, "Pressure" => 6, "Anomaly" => 7, _ => 0 };
                        lines.Add($"HAZARD|{mi}|{kind}|{F(o.X - o.W / 2)}|{F(o.Y - o.D / 2)}|{F(o.X + o.W / 2)}|{F(o.Y + o.D / 2)}|{F(o.Z)}|{F(o.Z + Math.Max(.1f, o.H))}|20");
                        break;
                    }
                    case "spawn":
                    {
                        string role = o.SpawnKind ?? "Worker";
                        if (role is "Creature" or "Hostile")
                        {
                            int species = Math.Max(0, Array.IndexOf(CreatureSpecies, o.Species ?? "Huntsman"));
                            lines.Add($"CREATURE|{mi}|{species}|{F(o.X)}|{F(o.Y)}|{F(o.Z)}");
                        }
                        else if (role != "Player")
                        {
                            var visuals = new Dictionary<string, int>
                            {
                                ["Worker"] = 0, ["Cargo"] = 3, ["Cleanup"] = 4, ["Security"] = 4,
                                ["Scientist"] = 10, ["SeniorScientist"] = 11, ["FemaleScientist"] = 12,
                                ["FemaleDoctor"] = 13, ["LabTech"] = 14, ["NPC"] = 15, ["Investigator"] = 15,
                            };
                            if (!visuals.TryGetValue(role, out int visual))
                            {
                                warnings.Add($"{chunk.Name}: {role} dummy remains blueprint-only.");
                                break;
                            }
                            int idx = actorCount++;
                            float yaw = o.Rotation * MathF.PI / 180;
                            actorLines.Add($"ACTOR|{mi}|editor_clock|{visual}|1|0|0|{idx}");
                            actorLines.Add($"ACTOR_KEY|{mi}|{idx}|0|{F(o.X)}|{F(o.Y)}|{F(o.Z)}|{F(yaw)}|0|0");
                            actorLines.Add($"ACTOR_KEY|{mi}|{idx}|60000|{F(o.X)}|{F(o.Y)}|{F(o.Z)}|{F(yaw)}|0|1");
                            if (o.Behaviour == "civilian") actorLines.Add($"ACTOR_AI|{mi}|{idx}|1||1.4|2|6");
                            else if (o.Behaviour == "guard")
                            {
                                actorLines.Add($"ACTOR_AI|{mi}|{idx}|3||1.4|2|6");
                                if (o.Armed is "handgun" or "shotgun")
                                    actorLines.Add($"ACTOR_WEAPON|{mi}|{idx}|{(o.Armed == "handgun" ? 2 : 1)}|650|1000|9|18");
                            }
                        }
                        break;
                    }
                    case "asset":
                    {
                        string model = NormalizeModel(o.Model);
                        float yaw = o.Rotation * MathF.PI / 180;
                        float baseOff = o.Z - baseZ;
                        if (maps.Facility.TryGetValue(model, out int fi))
                            lines.Add($"FIXTURE|{mi}|{fi}|{F(o.X)}|{F(o.Y)}|{F(baseOff)}|{F(o.W)}|{F(o.D)}|{F(o.H)}|{F(yaw)}|{(fi is 3 or 4 or 5 ? 0 : 1)}");
                        else if (maps.Prop.TryGetValue(model, out int pk))
                            lines.Add($"PROP|{mi}|{pk}|{F(o.X)}|{F(o.Y)}|{F(o.H)}|{F(Math.Max(o.W, o.D))}|{F(yaw)}|{F(o.W / 2)}|{F(o.D / 2)}|{F(baseOff)}");
                        else if (maps.Clutter.TryGetValue(model, out int ck))
                            lines.Add($"CLUTTER|{mi}|{ck}|{F(o.X)}|{F(o.Y)}|{F(Math.Abs(o.Z - baseZ) < .03f ? -999 : o.Z)}|{F(yaw)}");
                        else if (maps.Pickup.TryGetValue(model, out string kindName) && maps.PickupIndex.TryGetValue(kindName, out int pi))
                            lines.Add($"PICKUP|{mi}|{pi}|{F(o.X)}|{F(o.Y)}");
                        else if (maps.Creature.TryGetValue(model, out string creature))
                            lines.Add($"CREATURE|{mi}|{Math.Max(0, Array.IndexOf(CreatureSpecies, creature))}|{F(o.X)}|{F(o.Y)}|{F(o.Z)}");
                        else warnings.Add($"{chunk.Name}: asset '{o.Name}' has no engine placement mapping and was skipped.");
                        break;
                    }
                }
            }
            lines.AddRange(actorLines);
            if (chunk.SurfaceWall != -1 || chunk.SurfaceFloor != -1 || chunk.SurfaceCeiling != -1)
                lines.Add($"SURFACE|{mi}|{chunk.SurfaceWall}|{chunk.SurfaceFloor}|{chunk.SurfaceCeiling}");
        }
        lines.AddRange(authored);
        return "--- CUSTOM_CAMPAIGN_DATA_START ---\n" + string.Join("\n", lines) + "\n--- CUSTOM_CAMPAIGN_DATA_END ---\n";
    }

    private static string NormalizeModel(string? model)
    {
        string m = (model ?? "").Replace('\\', '/');
        return m;
    }

    // ---------------- Import ----------------
    private sealed class ActorAcc
    {
        public int Visual;
        public float X, Y, Z, Rotation;
        public bool HasKey;
        public string Behaviour = "stand", Armed = "none";
    }

    public static Project Import(string text, Mappings maps)
    {
        int start = text.IndexOf("--- CUSTOM_CAMPAIGN_DATA_START ---", StringComparison.Ordinal);
        int end = text.IndexOf("--- CUSTOM_CAMPAIGN_DATA_END ---", StringComparison.Ordinal);
        if (start >= 0 && end > start) text = text[start..end];

        var project = new Project();
        var chunks = new SortedDictionary<int, Chunk>();
        var actors = new Dictionary<int, Dictionary<int, ActorAcc>>();
        int counter = 0;
        string NextId() => "imp" + (++counter);

        var facilityByIndex = maps.Facility.ToDictionary(kv => kv.Value, kv => kv.Key);
        var propByKind = maps.Prop.ToDictionary(kv => kv.Value, kv => kv.Key);
        var pickupByIndex = new Dictionary<int, string>();
        foreach (var (path, kindName) in maps.Pickup)
            if (maps.PickupIndex.TryGetValue(kindName, out int pi)) pickupByIndex[pi] = path;
        string[] hazardNames = { "Electricity", "Steam", "Crusher", "Toxic", "Fire", "FallingDebris", "Pressure", "Anomaly" };

        Chunk? ChunkAt(int i) => chunks.TryGetValue(i, out var c) ? c : null;

        foreach (var raw in text.Split('\n'))
        {
            var line = raw.Trim();
            if (line.Length == 0 || line.StartsWith('#') || !line.Contains('|')) continue;
            var f = line.Split('|').Select(Uri.UnescapeDataString).ToArray();
            int MapIdx() => (int)Num(f[1]);
            switch (f[0])
            {
                case "CAMPAIGN" when f.Length > 1: project.Name = f[1]; break;
                case "TERRAIN" when f.Length >= 6:
                    project.TerrainEnabled = true;
                    project.TerrainSeed = (int)Num(f[1]); project.TerrainBase = Num(f[2]);
                    project.TerrainAmplitude = Num(f[3]); project.TerrainScale = Num(f[4]); project.TerrainRoughness = Num(f[5]);
                    break;
                case "HORIZON" when f.Length >= 5:
                    project.HorizonDepth = Num(f[1]); project.HorizonSpacing = Num(f[2]); project.HorizonRise = Num(f[3]);
                    project.HorizonModels = f[4];
                    break;
                case "GROUND" when f.Length >= 7:
                    project.TerrainEnabled = true;
                    project.Ground.Add(new GroundRect { X1 = Num(f[1]), Y1 = Num(f[2]), X2 = Num(f[3]), Y2 = Num(f[4]), Kind = (int)Num(f[5]), Edge = Num(f[6]) });
                    break;
                case "TERRAIN_FLAT" when f.Length >= 7:
                    project.TerrainEnabled = true;
                    project.Flats.Add(new FlatRect { X1 = Num(f[1]), Y1 = Num(f[2]), X2 = Num(f[3]), Y2 = Num(f[4]), Height = Num(f[5]), Blend = Num(f[6]) });
                    break;
                case "MAP" when f.Length >= 12:
                {
                    int idx = MapIdx();
                    var chunk = new Chunk
                    {
                        Id = "map" + idx, Number = idx + 1, Name = f[2],
                        Gx = (int)Num(f[3]) / 24, Gy = (int)Num(f[4]) / 24,
                    };
                    chunks[idx] = chunk;
                    project.Skybox = f[11];
                    chunk.Objects.Add(new Obj
                    {
                        Id = NextId(), Type = "spawn", SpawnKind = "Player", Name = "Player Start",
                        X = Num(f[5], 3.5f), Y = Num(f[6], 3.5f), Z = Num(f[7]), W = .65f, D = .65f, H = 1.75f,
                    });
                    break;
                }
                case "LAYER" when f.Length >= 6:
                {
                    var chunk = ChunkAt(MapIdx());
                    chunk?.Layers.Add(new Layer
                    {
                        Id = $"m{f[1]}l{f[2]}", Name = f[3], Z = Num(f[4]), Thickness = Num(f[5]),
                        Rows = Enumerable.Range(0, 24).Select(_ => new string('_', 24)).ToList(),
                    });
                    break;
                }
                case "ROW" when f.Length >= 5:
                {
                    var chunk = ChunkAt(MapIdx());
                    int li = (int)Num(f[2]), y = (int)Num(f[3]);
                    if (chunk != null && li < chunk.Layers.Count && y is >= 0 and < 24)
                    {
                        string row = (f[4] + new string('_', 24))[..24];
                        if (li > 0) row = row.Replace('=', '.');
                        chunk.Layers[li].Rows[y] = row;
                    }
                    break;
                }
                case "SURFACE" when f.Length >= 5:
                {
                    var chunk = ChunkAt(MapIdx());
                    if (chunk != null)
                    {
                        chunk.SurfaceWall = (int)Num(f[2], -1);
                        chunk.SurfaceFloor = (int)Num(f[3], -1);
                        chunk.SurfaceCeiling = (int)Num(f[4], -1);
                    }
                    break;
                }
                case "LIGHT" when f.Length >= 5:
                {
                    var chunk = ChunkAt(MapIdx()); if (chunk == null) break;
                    var o = new Obj { Id = NextId(), Type = "light", Name = "Light", X = Num(f[2]), Y = Num(f[3]), Z = Num(f[4]), W = .3f, D = .3f, H = .3f };
                    if (f.Length > 5 && f[5] != "")
                    {
                        o.LampMount = (int)Num(f[5]) switch { 1 => "wall", 2 => "bare", _ => "ceiling" };
                        o.Rotation = f.Length > 6 ? Num(f[6]) * 180 / MathF.PI : 0;
                    }
                    if (f.Length > 7 && f[7] != "") o.Intensity = Num(f[7], 1);
                    if (f.Length > 8 && f[8] != "") o.Range = Num(f[8], 7);
                    if (f.Length > 9 && f[9] != "") o.Kelvin = (int)Num(f[9], 4000);
                    if (f.Length >= 25 && f[22] != "" && f[23] != "" && f[24] != "")
                        o.Gel = $"#{Clamp255(Num(f[22])):X2}{Clamp255(Num(f[23])):X2}{Clamp255(Num(f[24])):X2}";
                    chunk.Objects.Add(o);
                    break;
                }
                case "SIGN" when f.Length >= 11:
                {
                    var chunk = ChunkAt(MapIdx()); if (chunk == null) break;
                    chunk.Objects.Add(new Obj
                    {
                        Id = NextId(), Type = "sign", Name = f[8], Title = f[8], Subtitle = f[9],
                        Accent = $"#{(int)Num(f[10]) & 0xFFFFFF:X6}",
                        X = Num(f[2]), Y = Num(f[3]), Z = Num(f[4]), W = Num(f[5], 1.4f), H = Num(f[6], .5f),
                        Rotation = Num(f[7]) * 180 / MathF.PI, Mount = "wall",
                    });
                    break;
                }
                case "PRIM" when f.Length >= 11:
                {
                    var chunk = ChunkAt(MapIdx()); if (chunk == null) break;
                    chunk.Objects.Add(new Obj
                    {
                        Id = NextId(), Type = "prim", Name = f[2], PrimKind = f[2],
                        X = Num(f[3]), Y = Num(f[4]), Z = Num(f[5]),
                        W = Num(f[6], 1), D = Num(f[7], 1), H = Num(f[8], 1),
                        Rotation = (int)Num(f[9]) * 90, Material = (int)Num(f[10], 40),
                    });
                    break;
                }
                case "DOOR" when f.Length >= 6:
                {
                    var chunk = ChunkAt(MapIdx()); if (chunk == null) break;
                    float left = Num(f[2]), right = Num(f[3]);
                    chunk.Objects.Add(new Obj
                    {
                        Id = NextId(), Type = "door", Name = "Door", WallAxis = "horizontal",
                        X = (left + right) / 2, Y = Num(f[4]), W = Math.Max(.25f, right - left), D = .18f, H = 2.2f, Mount = "wall",
                    });
                    break;
                }
                case "HAZARD" when f.Length >= 10:
                {
                    var chunk = ChunkAt(MapIdx()); if (chunk == null) break;
                    chunk.Objects.Add(new Obj
                    {
                        Id = NextId(), Type = "hazard", Name = "Hazard",
                        Kind = hazardNames[Math.Clamp((int)Num(f[2]), 0, 7)],
                        X = (Num(f[3]) + Num(f[5])) / 2, Y = (Num(f[4]) + Num(f[6])) / 2, Z = Num(f[7]),
                        W = Num(f[5]) - Num(f[3]), D = Num(f[6]) - Num(f[4]), H = Math.Max(.1f, Num(f[8]) - Num(f[7])),
                    });
                    break;
                }
                case "TERMINAL" when f.Length >= 5:
                {
                    var chunk = ChunkAt(MapIdx()); if (chunk == null) break;
                    chunk.Objects.Add(new Obj
                    {
                        Id = NextId(), Type = "terminal", Name = f.Length > 6 ? f[6] : "Terminal",
                        X = Num(f[2]), Y = Num(f[3]), Z = Num(f[4]), W = .55f, D = .35f, H = 1.1f,
                    });
                    break;
                }
                case "FIXTURE" when f.Length >= 11:
                {
                    var chunk = ChunkAt(MapIdx()); if (chunk == null) break;
                    if (facilityByIndex.TryGetValue((int)Num(f[2]), out string? model))
                        chunk.Objects.Add(new Obj
                        {
                            Id = NextId(), Type = "asset", Name = Path.GetFileNameWithoutExtension(model), Model = model,
                            X = Num(f[3]), Y = Num(f[4]), Z = Num(f[5]), W = Num(f[6], 1), D = Num(f[7], 1), H = Num(f[8], 1),
                            Rotation = Num(f[9]) * 180 / MathF.PI,
                        });
                    break;
                }
                case "PROP" when f.Length >= 8:
                {
                    var chunk = ChunkAt(MapIdx()); if (chunk == null) break;
                    if (propByKind.TryGetValue((int)Num(f[2]), out string? model))
                        chunk.Objects.Add(new Obj
                        {
                            Id = NextId(), Type = "asset", Name = Path.GetFileNameWithoutExtension(model), Model = model,
                            X = Num(f[3]), Y = Num(f[4]), H = Num(f[5], 1),
                            Rotation = Num(f[7]) * 180 / MathF.PI,
                        });
                    break;
                }
                case "PICKUP" when f.Length >= 5:
                {
                    var chunk = ChunkAt(MapIdx()); if (chunk == null) break;
                    if (pickupByIndex.TryGetValue((int)Num(f[2]), out string? model))
                        chunk.Objects.Add(new Obj
                        {
                            Id = NextId(), Type = "asset", Name = Path.GetFileNameWithoutExtension(model), Model = model,
                            X = Num(f[3]), Y = Num(f[4]), W = .5f, D = .5f, H = .5f,
                        });
                    break;
                }
                case "CREATURE" when f.Length >= 6:
                {
                    var chunk = ChunkAt(MapIdx()); if (chunk == null) break;
                    int species = Math.Clamp((int)Num(f[2]), 0, 4);
                    chunk.Objects.Add(new Obj
                    {
                        Id = NextId(), Type = "spawn", SpawnKind = "Creature", Species = CreatureSpecies[species],
                        Name = CreatureSpecies[species] + " Spawn",
                        X = Num(f[3]), Y = Num(f[4]), Z = Num(f[5]), W = .65f, D = .65f, H = 1.2f,
                    });
                    break;
                }
                case "ACTOR" when f.Length >= 8:
                {
                    int mi2 = MapIdx();
                    if (!actors.TryGetValue(mi2, out var dict)) actors[mi2] = dict = new();
                    dict[(int)Num(f[7])] = new ActorAcc { Visual = (int)Num(f[3]) };
                    break;
                }
                case "ACTOR_KEY" when f.Length >= 8:
                {
                    int mi2 = MapIdx();
                    if (actors.TryGetValue(mi2, out var dict) && dict.TryGetValue((int)Num(f[2]), out var acc) && !acc.HasKey)
                    {
                        acc.HasKey = true;
                        acc.X = Num(f[4]); acc.Y = Num(f[5]); acc.Z = Num(f[6]);
                        acc.Rotation = Num(f[7]) * 180 / MathF.PI;
                    }
                    break;
                }
                case "ACTOR_AI" when f.Length >= 4:
                {
                    int mi2 = MapIdx();
                    if (actors.TryGetValue(mi2, out var dict) && dict.TryGetValue((int)Num(f[2]), out var acc))
                        acc.Behaviour = (int)Num(f[3]) switch { 1 => "civilian", 3 => "guard", _ => "stand" };
                    break;
                }
                case "ACTOR_WEAPON" when f.Length >= 4:
                {
                    int mi2 = MapIdx();
                    if (actors.TryGetValue(mi2, out var dict) && dict.TryGetValue((int)Num(f[2]), out var acc))
                        acc.Armed = (int)Num(f[3]) switch { 1 => "shotgun", 2 => "handgun", _ => "none" };
                    break;
                }
                case "STRUCT" or "STAIR" or "CLUTTER" or "BREAKABLE":
                    break; // upper walls are rows; boxes/stairs return as generic blocks below
                default:
                {
                    string[] carried = { "TIMED_SEQUENCE", "ACTOR_TALK", "ACTOR_VOICE", "SEQUENCE_CUE", "SEQUENCE_SIGHT", "SURFACE" };
                    // SCRIPT_*/ACTOR_* handled or deliberately carried for re-export.
                    if (line.StartsWith("SCRIPT_") || carried.Contains(f[0]))
                        project.RuntimeRecords.Add(line);
                    break;
                }
            }
        }

        var roleNames = new Dictionary<int, string>
        {
            [0] = "Worker", [3] = "Cargo", [4] = "Security", [10] = "Scientist",
            [11] = "SeniorScientist", [12] = "FemaleScientist", [13] = "FemaleDoctor",
            [14] = "LabTech", [15] = "NPC",
        };
        foreach (var (mi, chunk) in chunks)
        {
            // Player start goes on the base layer; everything finds its layer by Z.
            foreach (var o in chunk.Objects)
            {
                var layer = chunk.Layers.Count == 0 ? null
                    : chunk.Layers.OrderBy(l => Math.Abs(l.Z - o.Z)).First();
                o.LayerId = layer?.Id ?? "";
            }
            if (actors.TryGetValue(mi, out var dict))
                foreach (var acc in dict.Values.Where(a => a.HasKey))
                {
                    string role = roleNames.TryGetValue(acc.Visual, out string? rn) ? rn : "Worker";
                    var o = new Obj
                    {
                        Id = NextId(), Type = "spawn", SpawnKind = role, Name = role + " Dummy",
                        X = acc.X, Y = acc.Y, Z = acc.Z, Rotation = acc.Rotation,
                        Behaviour = acc.Behaviour, Armed = acc.Armed, W = .65f, D = .65f, H = 1.75f,
                    };
                    var layer = chunk.Layers.Count == 0 ? null
                        : chunk.Layers.OrderBy(l => Math.Abs(l.Z - o.Z)).First();
                    o.LayerId = layer?.Id ?? "";
                    chunk.Objects.Add(o);
                }
            project.Chunks.Add(chunk);
        }
        if (project.Chunks.Count == 0)
            throw new InvalidDataException("No MAP records found. Is this a .rmc campaign file?");
        return project;
    }

    private static int Clamp255(float v) => Math.Clamp((int)Math.Round(v * 255), 0, 255);
}
