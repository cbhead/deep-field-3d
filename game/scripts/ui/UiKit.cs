using Godot;
using System.Collections.Generic;

namespace DeepField.Game.Ui;

/// <summary>The chamfered surface every panel in the game is cut from.
///
/// Design's rule is "corners are chamfered, never pill" — the top-left and
/// bottom-right corners are sliced at 45°, the other two stay square. Godot's
/// StyleBoxFlat only does rounded corners, so this draws the hexagon itself,
/// plus the 1px inset stroke and the hard low-spread drop shadow that the kit
/// uses instead of soft blur.</summary>
public partial class ChamferBox : StyleBox
{
    public Color Fill = Tokens.SurfacePanel;
    public Color Stroke = Tokens.BorderPanel;
    public Color? Accent;          // emphasised edge under a panel header
    public float Chamfer = Tokens.ChamferMd;
    public float StrokeWidth = Tokens.StrokePanel;
    public bool DropShadow;
    public Color Glow = new(0, 0, 0, 0);

    private static Vector2[] Points(Rect2 r, float c)
    {
        float x = r.Position.X, y = r.Position.Y, w = r.Size.X, h = r.Size.Y;
        c = Mathf.Min(c, Mathf.Min(w, h) * 0.5f);
        return new[]
        {
            new Vector2(x + c, y),
            new Vector2(x + w, y),
            new Vector2(x + w, y + h - c),
            new Vector2(x + w - c, y + h),
            new Vector2(x, y + h),
            new Vector2(x, y + c),
        };
    }

    public override void _Draw(Rid toCanvasItem, Rect2 rect)
    {
        // Hard drop: a solid offset copy, never a blur. Design is explicit that
        // shadows are "hard, low-spread drops... no soft blobs".
        if (DropShadow)
        {
            var below = Points(new Rect2(rect.Position + new Vector2(0, 2), rect.Size), Chamfer);
            RenderingServer.CanvasItemAddPolygon(toCanvasItem, below, Fill3(Tokens.ShadowDrop, below.Length));
        }

        var points = Points(rect, Chamfer);
        RenderingServer.CanvasItemAddPolygon(toCanvasItem, points, Fill3(Fill, points.Length));

        if (Glow.A > 0.001f)
        {
            var halo = Points(rect.Grow(2f), Chamfer + 2f);
            RenderingServer.CanvasItemAddPolyline(toCanvasItem, Closed(halo),
                Fill3(Glow, halo.Length + 1), 3f);
        }

        if (StrokeWidth > 0f)
            RenderingServer.CanvasItemAddPolyline(toCanvasItem, Closed(points),
                Fill3(Accent ?? Stroke, points.Length + 1), StrokeWidth);
    }

    private static Vector2[] Closed(Vector2[] p)
    {
        var loop = new Vector2[p.Length + 1];
        p.CopyTo(loop, 0);
        loop[^1] = p[0];
        return loop;
    }

    private static Color[] Fill3(Color c, int n)
    {
        var colors = new Color[n];
        for (int i = 0; i < n; i++) colors[i] = c;
        return colors;
    }
}

/// <summary>A meter. Track, fill, an inner top highlight, and optional segment
/// dividers for magazine counts — the `.bar` component from the kit.</summary>
public partial class KitBar : Control
{
    public float Value;                 // 0..1
    public Color FillColor = Tokens.BarHp;
    public int Segments;                // 0 = continuous

    public void Set(float value, Color fill)
    {
        Value = Mathf.Clamp(value, 0f, 1f);
        FillColor = fill;
        QueueRedraw();
    }

    public override void _Draw()
    {
        var full = new Rect2(Vector2.Zero, Size);
        DrawRect(full, Tokens.BarTrack);
        DrawRect(full, Tokens.BorderPanel, filled: false, width: 1f);

        if (Value > 0f)
        {
            var fill = new Rect2(Vector2.Zero, new Vector2(Size.X * Value, Size.Y));
            DrawRect(fill, FillColor);
            // The inner top highlight the kit puts on every filled bar.
            DrawRect(new Rect2(Vector2.Zero, new Vector2(fill.Size.X, 1)),
                new Color(1, 1, 1, 0.28f));
        }

        for (int i = 1; i < Segments; i++)
        {
            float x = Size.X * i / Segments;
            DrawRect(new Rect2(new Vector2(x - 1, 0), new Vector2(1, Size.Y)), Tokens.SurfaceBase);
        }
    }
}

/// <summary>Ten upgrade pips with the L4/L7/L10 breakpoints called out in
/// brass — the player reads a tower's build state off these.</summary>
public partial class KitPips : Control
{
    public int Level;
    public int Max = 10;
    public int[] Breakpoints = { 4, 7, 10 };

    public void Set(int level, int max)
    {
        Level = level;
        Max = max;
        CustomMinimumSize = new Vector2(max * 13 - 3, 6);
        QueueRedraw();
    }

    public override void _Draw()
    {
        for (int i = 0; i < Max; i++)
        {
            var at = new Rect2(new Vector2(i * 13, 0), new Vector2(10, 6));
            bool on = i < Level;
            bool breakpoint = System.Array.IndexOf(Breakpoints, i + 1) >= 0;
            DrawRect(at, on ? (breakpoint ? Tokens.Brass400 : Tokens.Arcane400) : Tokens.Obsidian500);
            if (!on) DrawRect(at, Tokens.BorderStrong, filled: false, width: 1f);
        }
    }
}

/// <summary>Shared builders so every surface is made of the same parts.</summary>
public static class Kit
{
    private static readonly Dictionary<string, FontVariation> Tracked = new();

    /// <summary>Design's labels are widely tracked all-caps micro type, which
    /// Godot expresses as glyph spacing on a font variation.</summary>
    public static FontVariation? TrackedDisplay(float spacing)
    {
        string key = spacing.ToString("0.0");
        if (Tracked.TryGetValue(key, out var cached)) return cached;
        if (Tokens.Display is not { } bas) return null;

        var variation = new FontVariation { BaseFont = bas };
        variation.SetSpacing(TextServer.SpacingType.Glyph, Mathf.RoundToInt(spacing));
        Tracked[key] = variation;
        return variation;
    }

    // ---- Surfaces ---------------------------------------------------------

    public static PanelContainer Surface(Color fill, Color? stroke = null, float chamfer = Tokens.ChamferMd,
        bool shadow = true, Color? glow = null)
    {
        var panel = new PanelContainer();
        panel.AddThemeStyleboxOverride("panel", new ChamferBox
        {
            Fill = fill,
            Stroke = stroke ?? Tokens.BorderPanel,
            Chamfer = chamfer,
            DropShadow = shadow,
            Glow = glow ?? new Color(0, 0, 0, 0),
            ContentMarginLeft = Tokens.PadPanel,
            ContentMarginRight = Tokens.PadPanel,
            ContentMarginTop = Tokens.PadPanelTight,
            ContentMarginBottom = Tokens.PadPanelTight,
        });
        return panel;
    }

    /// <summary>The in-world HUD surface: translucent obsidian over the game.</summary>
    public static PanelContainer Glass(float chamfer = Tokens.ChamferMd)
        => Surface(Tokens.SurfaceGlass, Tokens.BorderPanel, chamfer);

    public static PanelContainer Card(bool selected = false)
        => Surface(Tokens.SurfaceRaised, selected ? Tokens.Brass500 : Tokens.BorderPanel,
            Tokens.ChamferSm, glow: selected ? Tokens.GlowBrass : null);

    /// <summary>An inventory well — attachment slots, weapon slots, ability.</summary>
    public static PanelContainer SlotBox(bool selected = false, int size = Tokens.Slot)
    {
        var slot = Surface(Tokens.SurfaceSlot, selected ? Tokens.Arcane400 : Tokens.BorderPanel,
            Tokens.ChamferSm, shadow: false, glow: selected ? Tokens.GlowArcane : null);
        slot.CustomMinimumSize = new Vector2(size, size);
        // Slots are square wells with padding, not stretched frames.
        if (slot.GetThemeStylebox("panel") is ChamferBox box)
        {
            box.ContentMarginLeft = box.ContentMarginRight = 6;
            box.ContentMarginTop = box.ContentMarginBottom = 6;
        }
        return slot;
    }

    /// <summary>Artwork inside a slot: fixed size, centred, never stretched to
    /// the well. A PanelContainer expands its children by default, which turns
    /// a 24px icon into a 72px one.</summary>
    public static TextureRect SlotIcon(Texture2D? texture, Color tint, int size = 32)
        => new()
        {
            Texture = texture,
            Modulate = tint,
            CustomMinimumSize = new Vector2(size, size),
            StretchMode = TextureRect.StretchModeEnum.KeepAspectCentered,
            ExpandMode = TextureRect.ExpandModeEnum.IgnoreSize,
            SizeFlagsHorizontal = Control.SizeFlags.ShrinkCenter,
            SizeFlagsVertical = Control.SizeFlags.ShrinkCenter,
            MouseFilter = Control.MouseFilterEnum.Ignore,
        };

    // ---- Type -------------------------------------------------------------

    /// <summary>Uppercase tracked micro type. Most of the interface's texture
    /// comes from these.</summary>
    public static Label Label(string text, Color? color = null)
    {
        var label = new Label { Text = text.ToUpperInvariant() };
        if (TrackedDisplay(Tokens.TrackingLabel) is { } font)
            label.AddThemeFontOverride("font", font);
        label.AddThemeFontSizeOverride("font_size", Tokens.SizeMicro);
        label.AddThemeColorOverride("font_color", color ?? Tokens.TextMuted);
        return label;
    }

    public static Label Body(string text, int size = Tokens.SizeBody, Color? color = null)
    {
        var label = new Label { Text = text };
        if (Tokens.Body is { } font) label.AddThemeFontOverride("font", font);
        label.AddThemeFontSizeOverride("font_size", size);
        label.AddThemeColorOverride("font_color", color ?? Tokens.TextSecondary);
        return label;
    }

    public static Label Title(string text, int size = Tokens.SizeTitle, Color? color = null)
    {
        var label = new Label { Text = text };
        if (Tokens.Display is { } font) label.AddThemeFontOverride("font", font);
        label.AddThemeFontSizeOverride("font_size", size);
        label.AddThemeColorOverride("font_color", color ?? Tokens.TextPrimary);
        return label;
    }

    /// <summary>Every number in the game. Mono and tabular, so a counter
    /// ticking down doesn't shuffle the layout sideways.</summary>
    public static Label Numeral(string text, int size = Tokens.SizeStat, Color? color = null)
    {
        var label = new Label { Text = text };
        if (Tokens.Mono is { } font) label.AddThemeFontOverride("font", font);
        label.AddThemeFontSizeOverride("font_size", size);
        label.AddThemeColorOverride("font_color", color ?? Tokens.TextPrimary);
        return label;
    }

    // ---- Parts ------------------------------------------------------------

    public static KitBar Bar(float value, Color fill, float height = 8f, int segments = 0)
    {
        var bar = new KitBar { Value = Mathf.Clamp(value, 0f, 1f), FillColor = fill, Segments = segments };
        bar.CustomMinimumSize = new Vector2(0, height);
        bar.SizeFlagsHorizontal = Control.SizeFlags.ExpandFill;
        return bar;
    }

    /// <summary>hp reads green until 30%, then flips to threat red. The
    /// threshold is design's, and it is the only colour change on the bar.</summary>
    public static Color HpColor(float fraction) => fraction <= 0.30f ? Tokens.BarHpLow : Tokens.BarHp;

    public static PanelContainer Tag(string text, Color? background = null, Color? ink = null)
    {
        var tag = Surface(background ?? Tokens.Obsidian500,
            background is null ? Tokens.BorderStrong : background, 4f, shadow: false);
        tag.AddChild(Label(text, ink ?? (background is null ? Tokens.TextSecondary : Tokens.TextInverse)));
        return tag;
    }

    public static PanelContainer Toast(string text, Color? edge = null)
    {
        var toast = Surface(Tokens.SurfaceGlass, edge ?? Tokens.BorderPanel, Tokens.ChamferSm, shadow: false);
        toast.AddChild(Body(text, Tokens.SizeCaption, Tokens.TextSecondary));
        return toast;
    }

    /// <summary>A keycap. Prompts name the key rather than describing it.</summary>
    public static PanelContainer Key(string key)
    {
        var cap = Surface(Tokens.Obsidian600, Tokens.BorderStrong, 3f, shadow: false);
        cap.AddChild(Numeral(key, Tokens.SizeMicro, Tokens.TextPrimary));
        return cap;
    }

    public static HSeparator Rule()
    {
        var rule = new HSeparator();
        var box = new StyleBoxFlat { BgColor = Tokens.BorderPanel, ContentMarginTop = 0, ContentMarginBottom = 0 };
        rule.AddThemeStyleboxOverride("separator", box);
        rule.AddThemeConstantOverride("separation", 1);
        return rule;
    }

    public static HBoxContainer Row(int gap = Tokens.GapInline)
    {
        var row = new HBoxContainer();
        row.AddThemeConstantOverride("separation", gap);
        return row;
    }

    public static VBoxContainer Col(int gap = Tokens.GapStack)
    {
        var col = new VBoxContainer();
        col.AddThemeConstantOverride("separation", gap);
        return col;
    }
}

/// <summary>A rotated square. Lives, faction chips and panel header marks are
/// all this shape — it is the kit's punctuation.</summary>
public partial class KitDiamond : Control
{
    public Color Color = Tokens.Brass400;
    public bool Lit = true;

    public KitDiamond(float size = 12f, Color? color = null)
    {
        CustomMinimumSize = new Vector2(size, size);
        if (color is { } c) Color = c;
    }

    public KitDiamond() : this(12f) { }

    public override void _Draw()
    {
        var half = Size * 0.5f;
        var points = new[]
        {
            new Vector2(half.X, 0), new Vector2(Size.X, half.Y),
            new Vector2(half.X, Size.Y), new Vector2(0, half.Y),
        };
        DrawColoredPolygon(points, Lit ? Color : Tokens.Obsidian500);
    }
}
