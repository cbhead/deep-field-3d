using Godot;
using System.Collections.Generic;
using DeepField.Sim.Content;

namespace DeepField.Game.Ui;

/// <summary>Shared visual language for every UI surface.
///
/// DESIGN OWNS THE PALETTE. Every value below is transcribed from Claude
/// Design's spec (docs/PALETTE.md, machine-readable twin docs/palette.json) —
/// this file adopts design's decisions, it does not make them. If a colour
/// needs to change, it changes in the spec first.
///
/// Icons resolve from res://assets/ui/icon_&lt;id&gt;.svg (or .png); Placeholder()
/// draws a labeled chip for anything not yet delivered.</summary>
public static class UiTheme
{
    private static Color Hex(string rgb) => new(rgb);

    // ---- Semantics (PALETTE.md §Semantics) --------------------------------
    public static readonly Color Danger = Hex("C93B28");
    public static readonly Color Warn = Hex("E8862B");
    public static readonly Color Good = Hex("7BC043");
    public static readonly Color Accent = Hex("2FB4BE");      // info
    public static readonly Color Hp = Hex("7BC043");
    public static readonly Color HpLow = Hex("C93B28");
    public static readonly Color Shield = Hex("2FB4BE");
    public static readonly Color Armor = Hex("7D8BA3");
    public static readonly Color Currency = Hex("C89B3C");
    public static readonly Color Elite = Hex("9B5BE8");

    // Chrome the spec doesn't legislate — kept neutral so design's accents
    // carry the identity rather than competing with a coloured panel.
    public static readonly Color Ink = new(0.93f, 0.95f, 0.98f);
    public static readonly Color InkDim = new(0.62f, 0.66f, 0.72f);
    public static readonly Color Panel = new(0.07f, 0.08f, 0.10f, 0.88f);
    public static readonly Color PanelRaised = new(0.13f, 0.15f, 0.18f, 0.95f);
    public static readonly Color Disabled = new(0.49f, 0.55f, 0.64f);

    public static Color Faction(string id) => id switch
    {
        "forge" => Hex("C89B3C"),
        "ember" => Hex("E8622B"),
        "tempest" => Hex("5B76E8"),
        _ => Accent,
    };

    public static Color Scrap(ScrapType type) => type switch
    {
        ScrapType.Alloy => Hex("A6B2C6"),
        ScrapType.Flux => Hex("2FB4BE"),
        ScrapType.Plating => Hex("C89B3C"),
        ScrapType.Gravium => Hex("9B5BE8"),
        _ => Hex("F4DCA4"),   // prime core
    };

    public static Color Status(string statusId) => statusId switch
    {
        "burn" => Hex("E8622B"),
        "chill" => Hex("4FC0E8"),
        "freeze" => Hex("A8F0F4"),
        "shock" => Hex("F05AE6"),
        "shred" => Hex("E9614C"),
        "mark" => Hex("E3BC66"),
        "reveal" => Hex("7FE65A"),
        "tar" => Hex("1B2233"),
        _ => Ink,
    };

    /// <summary>Statuses the spec renders as emissive only — the silhouette
    /// keeps its own colour and gains a glow instead of being repainted.</summary>
    public static bool StatusIsEmissiveOnly(string statusId)
        => statusId is "mark" or "reveal";

    /// <summary>One energy hue per tower; its projectile, muzzle flash and
    /// impact all inherit it, which is what makes fire legible at range.</summary>
    public static Color TowerEnergy(string towerId) => towerId switch
    {
        "lance" => Hex("2B5CFF"),
        "skywatch" => Hex("22D3EE"),
        "detector" => Hex("7FE65A"),
        "nova" => Hex("F0C83A"),
        "overclock" => Hex("FF6F1A"),
        "filament" => Hex("FF2E4A"),
        "arc" => Hex("F05AE6"),
        "singularity" => Hex("9B5BE8"),
        "barricade" => Hex("F0C83A"),
        _ => Accent,
    };

    // ---- Icons ------------------------------------------------------------

    private static readonly Dictionary<string, Texture2D> IconCache = new();

    /// <summary>Design's icon if present, else a generated labeled chip.
    /// Ids match docs/DESIGN-BRIEF.md §3.8 (e.g. "tower_lance", "scrap_flux").</summary>
    public static Texture2D Icon(string id, Color? tint = null)
    {
        if (IconCache.TryGetValue(id, out var cached)) return cached;

        // Design ships SVG (Godot rasterises it to a CompressedTexture2D on
        // import); PNG is accepted too so either delivery format just works.
        string svg = $"res://assets/ui/icon_{id}.svg";
        string png = $"res://assets/ui/icon_{id}.png";
        Texture2D texture = ResourceLoader.Exists(svg) ? GD.Load<Texture2D>(svg)
            : ResourceLoader.Exists(png) ? GD.Load<Texture2D>(png)
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
            Modulate = color,
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
