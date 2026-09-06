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

    // ---- Semantics --------------------------------------------------------
    // Aliases onto Tokens, which is the transcription of ds/tokens/colors.css.
    // Nothing is defined here any more; this is the vocabulary the game code
    // already speaks, pointed at design's system.
    public static readonly Color Danger = Tokens.StateDanger;
    public static readonly Color Warn = Tokens.StateWarning;
    public static readonly Color Good = Tokens.StateSuccess;
    public static readonly Color Accent = Tokens.StateInfo;
    public static readonly Color Hp = Tokens.BarHp;
    public static readonly Color HpLow = Tokens.BarHpLow;
    public static readonly Color Shield = Tokens.BarShield;
    public static readonly Color Armor = Tokens.BarArmor;
    public static readonly Color Currency = Tokens.ResGold;
    public static readonly Color Elite = Tokens.Soul500;

    public static readonly Color Ink = Tokens.TextPrimary;
    public static readonly Color InkDim = Tokens.TextMuted;
    public static readonly Color Panel = Tokens.SurfaceGlass;
    public static readonly Color PanelRaised = Tokens.SurfaceRaised;
    public static readonly Color Disabled = Tokens.TextDisabled;

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

    /// <summary>Every icon id the running game has asked for. Icon ids are
    /// built by interpolation ($"enemy_{defId}"), so scanning source for string
    /// literals under-reports badly — this is the honest record.</summary>
    public static readonly SortedSet<string> RequestedIcons = new();

    /// <summary>Design's icon if present, else a generated labeled chip.
    /// Ids match docs/DESIGN-BRIEF.md §3.8 (e.g. "tower_lance", "scrap_flux").</summary>
    public static Texture2D Icon(string id, Color? tint = null)
    {
        // Same lower-casing rule as AssetLibrary: design's filenames are all
        // lower case, sim content ids are not.
        id = id.ToLowerInvariant();
        RequestedIcons.Add(id);
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

    /// <summary>Chamfered, not rounded — design's corners are cut at 45° on
    /// the top-left and bottom-right and square elsewhere. Every surface in
    /// the game routes through here, so this one change restyles all of them.</summary>
    public static StyleBox PanelBox(Color? fill = null, Color? border = null)
        => new ChamferBox
        {
            Fill = fill ?? Tokens.SurfacePanel,
            Stroke = border ?? Tokens.BorderPanel,
            StrokeWidth = border is null ? 0f : Tokens.StrokePanel,
            Chamfer = Tokens.ChamferMd,
            DropShadow = true,
            ContentMarginLeft = Tokens.PadPanel,
            ContentMarginRight = Tokens.PadPanel,
            ContentMarginTop = Tokens.PadPanelTight,
            ContentMarginBottom = Tokens.PadPanelTight,
        };

    public static PanelContainer Card(Color? fill = null, Color? border = null)
    {
        var panel = new PanelContainer();
        panel.AddThemeStyleboxOverride("panel", PanelBox(fill, border));
        return panel;
    }

    /// <summary>Body copy in Barlow. Numbers should go through
    /// <see cref="Kit.Numeral"/> instead — design wants every numeral mono and
    /// tabular so counters don't jitter.</summary>
    public static Label Text(string text, int size = 14, Color? color = null)
    {
        var label = new Label { Text = text };
        if (Tokens.Body is { } font) label.AddThemeFontOverride("font", font);
        label.AddThemeFontSizeOverride("font_size", size);
        label.AddThemeColorOverride("font_color", color ?? Ink);
        return label;
    }

    /// <summary>Horizontal meter used for hp, shields, progress, stat deltas.
    /// Track, fill and the inner top highlight per the kit's `.bar`.</summary>
    public static KitBar Meter(float value, float max, Color fill, Vector2 size)
    {
        var bar = new KitBar
        {
            Value = max <= 0f ? 0f : Mathf.Clamp(value / max, 0f, 1f),
            FillColor = fill,
            CustomMinimumSize = size,
        };
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
            ExpandMode = TextureRect.ExpandModeEnum.IgnoreSize,
            TooltipText = iconId,
        });
        bool short_ = need >= 0 && amount < need;
        row.AddChild(Text(need >= 0 ? $"{amount}/{need}" : amount.ToString(), 13,
            short_ ? Danger : color));
        return row;
    }
}
