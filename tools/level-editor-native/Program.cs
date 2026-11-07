using System.Windows;

namespace RawMetalCampaignEditor;

internal static class Program
{
    [STAThread]
    private static void Main(string[] args)
    {
        if (args.Contains("--selftest-starter"))
        {
            RunStarterMapRoundTrip();
            return;
        }
        System.Windows.Forms.Application.EnableVisualStyles();
        var app = new System.Windows.Application { ShutdownMode = ShutdownMode.OnMainWindowClose };
        var window = new MainWindow();
        if (args.Contains("--selftest-3d")) window.RunSelfTest();
        if (args.Contains("--selftest-view")) window.RunStarterViewTest();
        if (args.Contains("--selftest-tools")) window.RunToolTest();
        if (args.Contains("--selftest-fixture-windows")) window.RunFixtureWindowTest();
        app.Run(window);
    }

    // The shipped Starter Map is the compatibility contract for this editor.
    // This stays headless so it can run in CI and catches import/export loss
    // before someone discovers it through a crashed editor window.
    private static void RunStarterMapRoundTrip()
    {
        string root = Directory.GetCurrentDirectory();
        string Check(string fileName, string label, int expectedMaps, params string[] requiredKinds)
        {
            string path = Path.Combine(root, "custom maps", fileName);
            if (!File.Exists(path)) throw new FileNotFoundException($"{label} was not found.", path);
            var project = Rmc.Import(File.ReadAllText(path), Mappings.Load());
            if (project.Chunks.Count != expectedMaps) throw new InvalidDataException($"{label}: expected {expectedMaps} maps, got {project.Chunks.Count}.");
            if (project.Chunks.Sum(c => c.Layers.Count) < expectedMaps) throw new InvalidDataException($"{label}: layers were lost during import.");
            if (project.Chunks.Any(c => c.MapTail.Count == 0)) throw new InvalidDataException($"{label}: per-map environment settings were lost during import.");
            var warnings = new List<string>();
            string roundTrip = Rmc.Export(project, project.Name, Mappings.Load(), warnings);
            foreach (string kind in requiredKinds)
                if (!roundTrip.Contains(kind, StringComparison.Ordinal)) throw new InvalidDataException($"{label}: round-trip lost {kind} records.");
            return $"{label}: {project.Chunks.Count} maps, {project.Chunks.Sum(c => c.Layers.Count)} layers, {project.Chunks.Sum(c => c.Objects.Count)} editable objects, {project.RuntimeRecords.Count} preserved raw records, {warnings.Count} export warnings.";
        }

        string starter = Check("Starter Map.rmc", "Starter Map", 150,
            "MAP|", "LAYER|", "STRUCT|", "STAIR|", "BREAKABLE|", "CLUTTER|", "FIXTURE|", "SIGN|", "LIGHT|");
        string ashfall = Check("RawMetal-Ashfall-Exclusion-Zone.txt", "Ashfall Exclusion Zone", 20,
            "MAP|", "LAYER|", "STRUCT|", "CLUTTER|", "FIXTURE|", "PICKUP|", "ACTOR|", "SIGN|", "LIGHT|");
        File.WriteAllText(Path.Combine(root, "selftest-starter.txt"), starter + Environment.NewLine + ashfall);
    }
}
