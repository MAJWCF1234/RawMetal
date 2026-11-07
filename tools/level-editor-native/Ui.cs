using System.Drawing.Drawing2D;

namespace RawMetalCampaignEditor;

// The glass kit. WinForms has no glass out of the box, so we paint it:
// vertical gradients, a 1px light edge, rounded corners, soft accent
// glows. Everything the artist touches is one of these controls, which
// is why the whole app reads as one designed object instead of a dialog.
public static class Theme
{
    public static readonly Color Ink = Color.FromArgb(10, 13, 18);
    public static readonly Color GlassTop = Color.FromArgb(38, 48, 62);
    public static readonly Color GlassBottom = Color.FromArgb(20, 26, 36);
    public static readonly Color Edge = Color.FromArgb(90, 110, 135);
    public static readonly Color Gold = Color.FromArgb(232, 184, 96);
    public static readonly Color Teal = Color.FromArgb(92, 210, 196);
    public static readonly Color Rose = Color.FromArgb(232, 120, 110);
    public static readonly Color TextHi = Color.FromArgb(238, 242, 247);
    public static readonly Color TextLo = Color.FromArgb(165, 178, 195);

    public static GraphicsPath Rounded(Rectangle r, int radius)
    {
        var path = new GraphicsPath();
        int d = radius * 2;
        path.AddArc(r.X, r.Y, d, d, 180, 90);
        path.AddArc(r.Right - d, r.Y, d, d, 270, 90);
        path.AddArc(r.Right - d, r.Bottom - d, d, d, 0, 90);
        path.AddArc(r.X, r.Bottom - d, d, d, 90, 90);
        path.CloseFigure();
        return path;
    }
}

public sealed class GlassPanel : Panel
{
    public int Radius = 14;
    public GlassPanel()
    {
        DoubleBuffered = true;
        BackColor = Theme.GlassBottom;
    }
    protected override void OnPaint(PaintEventArgs e)
    {
        var g = e.Graphics;
        g.SmoothingMode = SmoothingMode.AntiAlias;
        var rect = new Rectangle(0, 0, Width - 1, Height - 1);
        using var path = Theme.Rounded(rect, Radius);
        using var brush = new LinearGradientBrush(rect, Theme.GlassTop, Theme.GlassBottom, 90f);
        g.FillPath(brush, path);
        using var pen = new Pen(Color.FromArgb(110, Theme.Edge), 1f);
        g.DrawPath(pen, path);
    }
}

// A pill button with hover glow and an "active" accent state. The whole
// toolbar and tool list are these, so modes are visible at a glance.
public sealed class PillButton : Button
{
    public bool Active;
    public Color Accent = Theme.Teal;

    public PillButton()
    {
        FlatStyle = FlatStyle.Flat;
        FlatAppearance.BorderSize = 0;
        BackColor = Color.Transparent;
        ForeColor = Theme.TextHi;
        Font = new Font("Segoe UI", 10f, FontStyle.Bold);
        Height = 34;
        Cursor = Cursors.Hand;
    }

    protected override void OnPaint(PaintEventArgs e)
    {
        var g = e.Graphics;
        g.SmoothingMode = SmoothingMode.AntiAlias;
        var rect = new Rectangle(1, 1, Width - 3, Height - 3);
        using var path = Theme.Rounded(rect, 10);
        Color top, bottom;
        if (Active) { top = Accent; bottom = Color.FromArgb(200, Accent); }
        else if (ClientRectangle.Contains(PointToClient(MousePosition)) && MouseButtons == MouseButtons.None)
        { top = Color.FromArgb(64, 78, 96); bottom = Color.FromArgb(40, 50, 66); }
        else { top = Color.FromArgb(44, 55, 72); bottom = Color.FromArgb(28, 36, 50); }
        using var brush = new LinearGradientBrush(rect, top, bottom, 90f);
        g.FillPath(brush, path);
        using var pen = new Pen(Active ? Color.White : Color.FromArgb(90, Theme.Edge), Active ? 1.4f : 1f);
        g.DrawPath(pen, path);
        var textColor = Active && Accent == Theme.Gold ? Color.Black : Theme.TextHi;
        TextRenderer.DrawText(g, Text, Font, rect, textColor,
            TextFormatFlags.HorizontalCenter | TextFormatFlags.VerticalCenter | TextFormatFlags.EndEllipsis);
    }
}

public sealed class SectionLabel : Label
{
    public SectionLabel(string text)
    {
        Text = text;
        ForeColor = Theme.Gold;
        Font = new Font("Segoe UI", 10.5f, FontStyle.Bold);
        AutoSize = true;
    }
}
