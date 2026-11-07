using System.Windows;

namespace RawMetalCampaignEditor;

internal static class Program
{
    [STAThread]
    private static void Main(string[] args)
    {
        System.Windows.Forms.Application.EnableVisualStyles();
        var app = new System.Windows.Application { ShutdownMode = ShutdownMode.OnMainWindowClose };
        var window = new MainWindow();
        if (args.Contains("--selftest-3d")) window.RunSelfTest();
        if (args.Contains("--selftest-tools")) window.RunToolTest();
        app.Run(window);
    }
}
