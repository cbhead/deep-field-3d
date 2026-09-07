using Godot;
using System.Collections.Generic;
using System.Linq;

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

    /// <summary>See UiTheme.ReleaseCaches: static Godot resources must be let
    /// go before the SceneTree tears down, or mono aborts at shutdown.</summary>
    public static void ReleaseCaches() => Tracked.Clear();


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

    /// <summary>Wrapping body copy. A Label sizes to its longest line by
    /// default, so an unwrapped paragraph silently forces its whole column
    /// wider than the grid it sits in.</summary>
    public static Label Paragraph(string text, int size = Tokens.SizeCaption, Color? color = null)
    {
        var label = Body(text, size, color);
        label.AutowrapMode = TextServer.AutowrapMode.WordSmart;
        label.CustomMinimumSize = new Vector2(80, 0);
        label.SizeFlagsHorizontal = Control.SizeFlags.ExpandFill;
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
        toast.AddChild(Paragraph(text, Tokens.SizeCaption, Tokens.TextSecondary));
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

    /// <summary>Label on the left, value on the right — the row shape that
    /// carries almost every stat readout in the kit.</summary>
    public static HBoxContainer Between(Control left, Control right)
    {
        var row = Row();
        left.SizeFlagsHorizontal = Control.SizeFlags.ExpandFill;
        row.AddChild(left);
        row.AddChild(right);
        return row;
    }

    /// <summary>Tinted tag chips: the kit's `.tag` tones.</summary>
    public static PanelContainer TagBrass(string t) => Tag(t, Tokens.Brass500);
    public static PanelContainer TagArcane(string t) => Tag(t, Tokens.Arcane500);
    public static PanelContainer TagDanger(string t) => Tag(t, Tokens.Threat500, Tokens.Steel050);
    public static PanelContainer TagOk(string t) => Tag(t, Tokens.Venom500);
    public static PanelContainer TagSoul(string t) => Tag(t, Tokens.Soul500, Tokens.Steel050);

    /// <summary>The 24px icon used inline throughout the screens.</summary>
    public static TextureRect Icon(string id, Color tint, int size = 24)
        => SlotIcon(UiTheme.Icon(id, tint), tint, size);

    /// <summary>Full-screen surfaces all sit on the same obsidian lattice.</summary>
    public static Control Backdrop(Control root)
    {
        var grid = new KitGrid();
        root.AddChild(grid);
        return grid;
    }

    /// <summary>The 64px title bar with a brass underline that every
    /// full-screen surface opens with.</summary>
    public static (PanelContainer Bar, HBoxContainer Row) ScreenHeader(string title)
    {
        var bar = new PanelContainer();
        bar.AddThemeStyleboxOverride("panel", new StyleBoxFlat
        {
            BgColor = Tokens.SurfacePanel,
            BorderColor = Tokens.Brass500,
            BorderWidthBottom = 2,
            ContentMarginLeft = Tokens.Space9,
            ContentMarginRight = Tokens.Space9,
            ContentMarginTop = Tokens.Space5,
            ContentMarginBottom = Tokens.Space5,
        });
        bar.SetAnchorsPreset(Control.LayoutPreset.TopWide);
        bar.CustomMinimumSize = new Vector2(0, 64);

        var row = Row(Tokens.Space8);
        bar.AddChild(row);
        row.AddChild(Title(title, Tokens.SizeDisplayMd));
        return (bar, row);
    }

    public static Control Spacer()
        => new Control { SizeFlagsHorizontal = Control.SizeFlags.ExpandFill };
}

/// <summary>The kit's titled panel: a 38px header carrying a brass diamond and
/// tracked caps, a padded body, and an optional inset footer. The header's
/// bottom edge takes the panel's tone, which is how a surface says whether it
/// is informational, a purchase, or a warning.</summary>
public partial class KitPanel : PanelContainer
{
    public VBoxContainer Body = null!;
    private VBoxContainer _stack = null!;
    private HBoxContainer _header = null!;

    public KitPanel(string title, Color? tone = null, bool hazard = false)
    {
        AddThemeStyleboxOverride("panel", new ChamferBox
        {
            Fill = Tokens.SurfacePanel,
            Stroke = Tokens.BorderPanel,
            Chamfer = Tokens.ChamferMd,
            DropShadow = true,
        });

        _stack = new VBoxContainer();
        _stack.AddThemeConstantOverride("separation", 0);
        AddChild(_stack);

        var headerBox = new PanelContainer();
        headerBox.AddThemeStyleboxOverride("panel", new StyleBoxFlat
        {
            BgColor = hazard ? Tokens.Obsidian500 : Tokens.SurfaceRaised,
            BorderColor = tone ?? Tokens.BorderPanel,
            BorderWidthBottom = tone is null ? 1 : 2,
            ContentMarginLeft = Tokens.PadPanelTight,
            ContentMarginRight = Tokens.PadPanelTight,
            ContentMarginTop = 9,
            ContentMarginBottom = 9,
        });
        _stack.AddChild(headerBox);

        _header = Kit.Row(Tokens.GapInline);
        headerBox.AddChild(_header);
        _header.AddChild(new KitDiamond(10f, tone ?? Tokens.Brass400));
        var label = Kit.Label(title, Tokens.TextSecondary);
        label.SizeFlagsHorizontal = Control.SizeFlags.ExpandFill;
        _header.AddChild(label);

        var bodyBox = new PanelContainer();
        bodyBox.AddThemeStyleboxOverride("panel", new StyleBoxFlat
        {
            BgColor = new Color(0, 0, 0, 0),
            ContentMarginLeft = Tokens.PadPanel,
            ContentMarginRight = Tokens.PadPanel,
            ContentMarginTop = Tokens.PadPanel,
            ContentMarginBottom = Tokens.PadPanel,
        });
        bodyBox.SizeFlagsVertical = Control.SizeFlags.ExpandFill;
        _stack.AddChild(bodyBox);

        Body = Kit.Col(Tokens.GapStack);
        bodyBox.AddChild(Body);
    }

    /// <summary>Right-hand chip in the header — costs, counts, states.</summary>
    public void HeaderTrailing(Control node) => _header.AddChild(node);

    public void SetFooter(Control content)
    {
        var footer = new PanelContainer();
        footer.AddThemeStyleboxOverride("panel", new StyleBoxFlat
        {
            BgColor = Tokens.SurfaceInset,
            BorderColor = Tokens.BorderPanel,
            BorderWidthTop = 1,
            ContentMarginLeft = Tokens.PadPanel,
            ContentMarginRight = Tokens.PadPanel,
            ContentMarginTop = Tokens.PadPanelTight,
            ContentMarginBottom = Tokens.PadPanelTight,
        });
        footer.AddChild(content);
        _stack.AddChild(footer);
    }
}

/// <summary>Buttons are chamfered on two corners and never rounded. Primary is
/// a brass gradient with dark ink — it is the only element allowed to be that
/// loud, which is what makes "the action" obvious on a busy screen.</summary>
public partial class KitButton : Button
{
    public enum Tone { Primary, Secondary, Danger, Arcane, Ghost }

    public KitButton(string text, Tone tone = Tone.Primary, int height = Tokens.ControlMd)
    {
        Text = text.ToUpperInvariant();
        CustomMinimumSize = new Vector2(0, height);

        if (Kit.TrackedDisplay(Tokens.TrackingLabel) is { } font)
            AddThemeFontOverride("font", font);
        AddThemeFontSizeOverride("font_size", height >= Tokens.ControlLg ? Tokens.SizeBody : Tokens.SizeCaption);

        var (fill, ink, stroke) = tone switch
        {
            Tone.Primary => (Tokens.Brass500, Tokens.TextInverse, Tokens.Brass300),
            Tone.Danger => (Tokens.Threat500, Tokens.Steel050, Tokens.Threat400),
            Tone.Arcane => (Tokens.Arcane500, Tokens.TextInverse, Tokens.Arcane300),
            Tone.Ghost => (new Color(0, 0, 0, 0), Tokens.TextSecondary, Tokens.BorderPanel),
            _ => (Tokens.Obsidian500, Tokens.TextPrimary, Tokens.BorderStrong),
        };

        foreach (string state in new[] { "normal", "hover", "pressed", "disabled", "focus" })
        {
            bool hot = state is "hover" or "pressed";
            bool off = state == "disabled";
            var box = new ChamferBox
            {
                Fill = off ? fill with { A = fill.A * 0.4f } : hot ? fill.Lightened(0.12f) : fill,
                Stroke = stroke,
                Chamfer = 7f,
                DropShadow = tone != Tone.Ghost,
            };
            // Room for the letter-spacing. Labels are drawn through a
            // FontVariation with tracking applied, and Godot sizes the button
            // from the untracked string — so the text renders wider than the
            // box it was measured for and the last glyph gets cut. RECRAFT read
            // as "IECRAFT" and every FIT button lost its F. The chamfer eats
            // the corners too, so the padding covers both.
            box.ContentMarginLeft = Tokens.Space5;
            box.ContentMarginRight = Tokens.Space5;
            AddThemeStyleboxOverride(state, box);
        }

        AddThemeColorOverride("font_color", ink);
        AddThemeColorOverride("font_hover_color", ink);
        AddThemeColorOverride("font_pressed_color", ink);
        AddThemeColorOverride("font_disabled_color", Tokens.TextDisabled);
    }
}

/// <summary>Settings switch. Hard edges, brass when on — no pill, no slide.</summary>
public partial class KitToggle : Control
{
    public bool On;

    public KitToggle(bool on = false)
    {
        On = on;
        CustomMinimumSize = new Vector2(44, 22);
    }

    public override void _Draw()
    {
        var track = new Rect2(Vector2.Zero, Size);
        DrawRect(track, On ? Tokens.Brass700 : Tokens.SurfaceSlot);
        DrawRect(track, On ? Tokens.Brass500 : Tokens.BorderStrong, filled: false, width: 1f);
        var knob = new Rect2(new Vector2(On ? Size.X - 20 : 2, 2), new Vector2(18, Size.Y - 4));
        DrawRect(knob, On ? Tokens.Brass400 : Tokens.Steel400);
    }
}

/// <summary>Per-wave clear times. Bars, not a curve — this is a game HUD.</summary>
public partial class KitSpark : Control
{
    public float[] Values = System.Array.Empty<float>();
    public int Highlight = -1;

    public KitSpark() => CustomMinimumSize = new Vector2(0, 40);

    public void Set(float[] values, int highlight = -1)
    {
        Values = values;
        Highlight = highlight;
        QueueRedraw();
    }

    public override void _Draw()
    {
        if (Values.Length == 0) return;
        float peak = 0.001f;
        foreach (float v in Values) peak = Mathf.Max(peak, v);
        float slot = Size.X / Values.Length;
        for (int i = 0; i < Values.Length; i++)
        {
            float h = Size.Y * (Values[i] / peak);
            DrawRect(new Rect2(new Vector2(i * slot, Size.Y - h), new Vector2(slot - 2, h)),
                i == Highlight ? Tokens.Arcane400 : Tokens.Arcane600);
        }
    }
}

/// <summary>The bleed-out and revive dial. A ring rather than a bar because it
/// sits at screen centre where a bar would read as damage.</summary>
public partial class KitRing : Control
{
    public float Value;                       // 0..1
    public Color Fill = Tokens.Venom500;
    public string Caption = "";

    public KitRing(float size = 120f) => CustomMinimumSize = new Vector2(size, size);

    public void Set(float value, Color fill, string caption)
    {
        Value = Mathf.Clamp(value, 0f, 1f);
        Fill = fill;
        Caption = caption;
        QueueRedraw();
    }

    public override void _Draw()
    {
        var centre = Size * 0.5f;
        float radius = Mathf.Min(Size.X, Size.Y) * 0.5f - 6f;
        DrawArc(centre, radius, 0, Mathf.Tau, 64, Tokens.Obsidian500, 8f);
        if (Value > 0f)
            DrawArc(centre, radius, -Mathf.Pi / 2f, -Mathf.Pi / 2f + Mathf.Tau * Value, 64, Fill, 8f);

        if (Caption.Length == 0 || Tokens.Mono is not { } font) return;
        var size = font.GetStringSize(Caption, HorizontalAlignment.Center, -1, Tokens.SizeStat);
        DrawString(font, centre + new Vector2(-size.X * 0.5f, size.Y * 0.3f), Caption,
            HorizontalAlignment.Center, -1, Tokens.SizeStat, Fill);
    }
}

/// <summary>A map at a glance: its lanes, its sockets, and — where one exists
/// — the shortcut and the barricade that closes it.
///
/// Drawn from MapDef rather than authored per map, so a route change or a new
/// socket shows up here without anyone remembering to redraw a thumbnail. That
/// matters because this is the picture a player chooses a sector from.</summary>
public partial class KitRouteSketch : Control
{
    public DeepField.Sim.Content.MapDef? Map;

    public KitRouteSketch() => CustomMinimumSize = new Vector2(0, 210);

    public override void _Draw()
    {
        if (Map is not { } map || Size.X < 20) return;

        DrawRect(new Rect2(Vector2.Zero, Size), Tokens.SurfaceInset);
        var grid = new Color(Tokens.Steel500.R, Tokens.Steel500.G, Tokens.Steel500.B, 0.16f);
        for (float x = 0; x < Size.X; x += 20) DrawLine(new Vector2(x, 0), new Vector2(x, Size.Y), grid, 1f);
        for (float y = 0; y < Size.Y; y += 20) DrawLine(new Vector2(0, y), new Vector2(Size.X, y), grid, 1f);

        // Fit every waypoint and socket, preserving aspect so the lane's shape
        // is honest rather than stretched to the card.
        float minX = float.MaxValue, maxX = float.MinValue, minZ = float.MaxValue, maxZ = float.MinValue;
        void Include(float x, float z)
        {
            minX = Mathf.Min(minX, x); maxX = Mathf.Max(maxX, x);
            minZ = Mathf.Min(minZ, z); maxZ = Mathf.Max(maxZ, z);
        }
        foreach (var route in map.Routes)
            foreach (var w in route.Waypoints) Include(w.X, w.Z);
        foreach (var socket in map.Sockets) Include(socket.Pos.X, socket.Pos.Z);

        const float pad = 14f;
        float spanX = Mathf.Max(1f, maxX - minX), spanZ = Mathf.Max(1f, maxZ - minZ);
        float scale = Mathf.Min((Size.X - pad * 2) / spanX, (Size.Y - pad * 2) / spanZ);
        var origin = new Vector2(
            (Size.X - spanX * scale) * 0.5f - minX * scale,
            (Size.Y - spanZ * scale) * 0.5f - minZ * scale);
        Vector2 At(float x, float z) => origin + new Vector2(x * scale, z * scale);

        // Sockets first, so lanes read on top of them.
        foreach (var socket in map.Sockets)
        {
            var tint = socket.Tag switch
            {
                DeepField.Sim.Content.SocketTag.Wall => Tokens.Arcane500,
                DeepField.Sim.Content.SocketTag.Trap => Tokens.Ember500,
                DeepField.Sim.Content.SocketTag.Barricade => Tokens.Threat400,
                _ => Tokens.Venom500,
            };
            DrawCircle(At(socket.Pos.X, socket.Pos.Z), 2.2f, tint with { A = 0.75f });
        }

        foreach (var route in map.Routes)
        {
            bool air = route.Layer == DeepField.Sim.Content.EnemyLayer.Air;
            bool shortcut = route.BarricadeGate is not null;
            var colour = shortcut ? Tokens.Threat400 : air ? Tokens.Arcane400 : Tokens.Brass400;

            for (int i = 0; i < route.Waypoints.Count - 1; i++)
            {
                var a = At(route.Waypoints[i].X, route.Waypoints[i].Z);
                var b = At(route.Waypoints[i + 1].X, route.Waypoints[i + 1].Z);
                if (air || shortcut) DashedLine(a, b, colour, shortcut ? 2f : 1.5f);
                else DrawLine(a, b, colour, 2.5f);
            }

            // The gate that closes the shortcut is the whole lesson of a map
            // that has one, so it gets a marker rather than a line style.
            if (route.BarricadeGate is { } gateId)
            {
                var gate = map.Sockets.FirstOrDefault(s => s.Id == gateId);
                if (gate is not null)
                {
                    var at = At(gate.Pos.X, gate.Pos.Z);
                    DrawRect(new Rect2(at - new Vector2(4, 4), new Vector2(8, 8)), Tokens.Threat500);
                }
            }
        }

        // Mouths: where they come in, where they are going.
        foreach (var route in map.Routes)
        {
            if (route.Waypoints.Count < 2) continue;
            var start = At(route.Waypoints[0].X, route.Waypoints[0].Z);
            var end = At(route.Waypoints[^1].X, route.Waypoints[^1].Z);
            DrawCircle(start, 4f, Tokens.Ember500);
            DrawCircle(end, 4f, Tokens.Arcane300);
        }
    }

    private void DashedLine(Vector2 a, Vector2 b, Color colour, float width)
    {
        float length = a.DistanceTo(b);
        var step = (b - a).Normalized() * 5f;
        for (float travelled = 0; travelled < length; travelled += 10f)
        {
            var from = a + step * (travelled / 5f);
            var to = from + step;
            if (from.DistanceTo(a) > length) break;
            DrawLine(from, to, colour, width);
        }
    }
}

/// <summary>The faint 24px lattice behind every full-screen surface.</summary>
public partial class KitGrid : Control
{
    public KitGrid()
    {
        MouseFilter = MouseFilterEnum.Ignore;
        SetAnchorsAndOffsetsPreset(LayoutPreset.FullRect);
    }

    public override void _Draw()
    {
        DrawRect(new Rect2(Vector2.Zero, Size), Tokens.Obsidian900);
        var line = new Color(Tokens.Steel500.R, Tokens.Steel500.G, Tokens.Steel500.B, 0.16f);
        for (float x = 0; x < Size.X; x += 24) DrawLine(new Vector2(x, 0), new Vector2(x, Size.Y), line, 1f);
        for (float y = 0; y < Size.Y; y += 24) DrawLine(new Vector2(0, y), new Vector2(Size.X, y), line, 1f);
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
