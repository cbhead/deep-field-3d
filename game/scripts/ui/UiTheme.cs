using Godot;
using System.Collections.Generic;
using DeepField.Sim.Content;

namespace DeepField.Game.Ui;

/// <summary>Shared visual language for every UI surface.
///
/// DESIGN OWNS THE PALETTE. The colors here are neutral placeholders that keep
/// the game readable until Claude Design delivers its palette spec (see
/// docs/DESIGN-BRIEF.md §1). When that lands, replace the values in this file
/// and in GameRoot.TintEnemy — nothing else hardcodes color.
///
/// Icons resolve from res://assets/ui/icon_&lt;id&gt;.png the moment design ships
/// them; until then Placeholder() draws a labeled chip so every surface is
/// usable and correctly laid out in the meantime.</summary>
public static class UiTheme
{
    // ---- Placeholder palette (design replaces) ---------------------------
    public static readonly Color Ink = new(0.93f, 0.95f, 0.98f);
    public static readonly Color InkDim = new(0.62f, 0.66f, 0.72f);
    public static readonly Color Panel = new(0.07f, 0.08f, 0.10f, 0.88f);
    public static readonly Color PanelRaised = new(0.13f, 0.15f, 0.18f, 0.95f);
    public static readonly Color Accent = new(0.45f, 0.72f, 0.95f);
    public static readonly Color Good = new(0.42f, 0.82f, 0.48f);
    public static readonly Color Warn = new(0.95f, 0.75f, 0.30f);
    public static readonly Color Danger = new(0.92f, 0.36f, 0.32f);
    public static readonly Color Disabled = new(0.38f, 0.40f, 0.44f);

    public static Color Faction(string id) => id switch
    {
        "forge" => new Color(0.95f, 0.60f, 0.25f),
        "ember" => new Color(0.93f, 0.35f, 0.22f),
        "tempest" => new Color(0.60f, 0.50f, 0.95f),
        _ => Accent,
    };

    public static Color Scrap(ScrapType type) => type switch
    {
        ScrapType.Alloy => new Color(0.70f, 0.72f, 0.76f),
        ScrapType.Flux => new Color(0.35f, 0.85f, 0.90f),
        ScrapType.Plating => new Color(0.80f, 0.55f, 0.30f),
        ScrapType.Gravium => new Color(0.66f, 0.42f, 0.90f),
        _ => Ink,
    };

    public static Color Status(string statusId) => statusId switch
    {
        "burn" => new Color(1.00f, 0.45f, 0.10f),
        "chill" => new Color(0.50f, 0.75f, 1.00f),
        "mark" => new Color(1.00f, 0.90f, 0.20f),
        "shock" => new Color(0.70f, 0.45f, 1.00f),
        "freeze" => new Color(0.75f, 0.92f, 1.00f),
        "shred" => new Color(1.00f, 0.62f, 0.35f),
        _ => Ink,
    };

    // ---- Icons ------------------------------------------------------------

    private static readonly Dictionary<string, Texture2D> IconCache = new();

    /// <summary>Design's icon if present, else a generated labeled chip.
    /// Ids match docs/DESIGN-BRIEF.md §3.8 (e.g. "tower_lance", "scrap_flux").</summary>
    public static Texture2D Icon(string id, Color? tint = null)
    {
        if (IconCache.TryGetValue(id, out var cached)) return cached;

        string path = $"res://assets/ui/icon_{id}.png";
        Texture2D texture = ResourceLoader.Exists(path)
            ? GD.Load<Texture2D>(path)
            : Placeholder(id, tint ?? Accent);

        IconCache[id] = texture;
        return texture;
    }

    /// <summary>A 64px chip carrying the asset's initials — legible stand-in
    /// that makes missing art obvious without breaking layout.</summary>
    private static Texture2D Placeholder(string id, Color tint)
    {
        const int size = 64;
        var image = Image.CreateEmpty(size, size, false, Image.Format.Rgba8);
        image.Fill(new Color(tint.R, tint.G, tint.B, 0.28f));

        // Border so the chip reads as a deliberate placeholder, not a blank.
        for (int i = 0; i < size; i++)
        {
            image.SetPixel(i, 0, tint);
            image.SetPixel(i, size - 1, tint);
            image.SetPixel(0, i, tint);
            image.SetPixel(size - 1, i, tint);
        }

        // Diagonal slash marks "art pending".
        for (int i = 4; i < size - 4; i++)
            image.SetPixel(i, size - 1 - i, new Color(tint.R, tint.G, tint.B, 0.55f));

        return ImageTexture.CreateFromImage(image);
    }

    /// <summary>Short label used beside placeholder icons so a surface is
    /// readable before art lands (e.g. "tower_lance" → "LANCE").</summary>
    public static string ShortLabel(string id)
    {
        int underscore = id.LastIndexOf('_');
        string tail = underscore >= 0 ? id[(underscore + 1)..] : id;
        return tail.ToUpperInvariant();
    }

    // ---- Widget helpers ---------------------------------------------------

    public static StyleBoxFlat PanelBox(Color? fill = null, Color? border = null)
    {
        var box = new StyleBoxFlat
        {
            BgColor = fill ?? Panel,
            CornerRadiusTopLeft = 6, CornerRadiusTopRight = 6,
            CornerRadiusBottomLeft = 6, CornerRadiusBottomRight = 6,
            ContentMarginLeft = 10, ContentMarginRight = 10,
            ContentMarginTop = 8, ContentMarginBottom = 8,
        };
        if (border is { } b)
        {
            box.BorderColor = b;
            box.BorderWidthLeft = box.BorderWidthRight = box.BorderWidthTop = box.BorderWidthBottom = 1;
        }
        return box;
    }

    public static PanelContainer Card(Color? fill = null, Color? border = null)
    {
        var panel = new PanelContainer();
        panel.AddThemeStyleboxOverride("panel", PanelBox(fill, border));
        return panel;
    }

    public static Label Text(string text, int size = 14, Color? color = null)
    {
        var label = new Label { Text = text };
        label.AddThemeFontSizeOverride("font_size", size);
        label.AddThemeColorOverride("font_color", color ?? Ink);
        return label;
    }

    /// <summary>Horizontal meter used for hp, shields, progress, stat deltas.</summary>
    public static ProgressBar Meter(float value, float max, Color fill, Vector2 size)
    {
        var bar = new ProgressBar
        {
            MinValue = 0, MaxValue = max, Value = value,
            ShowPercentage = false,
            CustomMinimumSize = size,
        };
        bar.AddThemeStyleboxOverride("background", new StyleBoxFlat { BgColor = new Color(0, 0, 0, 0.55f) });
        bar.AddThemeStyleboxOverride("fill", new StyleBoxFlat { BgColor = fill });
        return bar;
    }

    /// <summary>Icon + amount chip, used for scrap and ammo readouts.</summary>
    public static HBoxContainer CountChip(string iconId, int amount, Color color, int need = -1)
    {
        var row = new HBoxContainer();
        row.AddChild(new TextureRect
        {
            Texture = Icon(iconId, color),
            CustomMinimumSize = new Vector2(16, 16),
            StretchMode = TextureRect.StretchModeEnum.KeepAspectCentered,
            TooltipText = iconId,
        });
        bool short_ = need >= 0 && amount < need;
        row.AddChild(Text(need >= 0 ? $"{amount}/{need}" : amount.ToString(), 13,
            short_ ? Danger : color));
        return row;
    }
}
