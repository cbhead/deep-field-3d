using Godot;
using System.Collections.Generic;

namespace DeepField.Game.Ui;

/// <summary>Claude Design's system, transcribed.
///
/// One-to-one with `ds/tokens/*.css` from the design project: colour ramps and
/// their semantic aliases, the type scale and its composite roles, the spacing
/// step scale, and the effect constants. Nothing here is invented — when a
/// value needs to change it changes in the design source first and gets copied
/// down, the same contract PALETTE.md set for the 3D art.
///
/// The look has a few load-bearing rules worth stating in code, because they
/// are easy to erode: corners are <b>chamfered, never rounded</b>; shadows are
/// hard low-spread drops with an inner top bevel, never soft blobs; and every
/// numeral is mono and tabular so digits do not jitter as they count.</summary>
public static class Tokens
{
    private static Color Hex(string rgb) => new(rgb);

    // ---- Base ramps -------------------------------------------------------
    // Obsidian: the battlefield night. Every surface is cut from this ramp.
    public static readonly Color Obsidian900 = Hex("080B11");
    public static readonly Color Obsidian800 = Hex("0D1119");
    public static readonly Color Obsidian700 = Hex("131926");
    public static readonly Color Obsidian600 = Hex("1B2233");
    public static readonly Color Obsidian500 = Hex("242E43");
    public static readonly Color Obsidian400 = Hex("313C55");

    // Steel: strokes, bevels, secondary type.
    public static readonly Color Steel500 = Hex("3E4A63");
    public static readonly Color Steel400 = Hex("5A6880");
    public static readonly Color Steel300 = Hex("7D8BA3");
    public static readonly Color Steel200 = Hex("A6B2C6");
    public static readonly Color Steel100 = Hex("CFD7E4");
    public static readonly Color Steel050 = Hex("ECF0F7");

    // Brass: the primary brand accent — currency, primary actions, victory.
    public static readonly Color Brass700 = Hex("6F4C15");
    public static readonly Color Brass600 = Hex("9A6D1E");
    public static readonly Color Brass500 = Hex("C89B3C");
    public static readonly Color Brass400 = Hex("E3BC66");
    public static readonly Color Brass300 = Hex("F4DCA4");

    // Arcane: the secondary accent — upgrades, selection, info.
    public static readonly Color Arcane700 = Hex("12525A");
    public static readonly Color Arcane600 = Hex("1B7C86");
    public static readonly Color Arcane500 = Hex("2FB4BE");
    public static readonly Color Arcane400 = Hex("65DCE4");
    public static readonly Color Arcane300 = Hex("A8F0F4");

    // Signal hues.
    public static readonly Color Threat600 = Hex("8E2318");
    public static readonly Color Threat500 = Hex("C93B28");
    public static readonly Color Threat400 = Hex("E9614C");
    public static readonly Color Ember500 = Hex("E8862B");
    public static readonly Color Ember400 = Hex("F3A85B");
    public static readonly Color Venom500 = Hex("7BC043");
    public static readonly Color Venom400 = Hex("9FDC6C");
    public static readonly Color Mana500 = Hex("5B76E8");
    public static readonly Color Mana400 = Hex("8A9DF2");
    public static readonly Color Soul500 = Hex("9B5BE8");
    public static readonly Color Soul400 = Hex("BE8DF5");

    // ---- Semantic surfaces ------------------------------------------------
    public static readonly Color SurfaceBase = Obsidian900;
    public static readonly Color SurfacePanel = Obsidian700;
    public static readonly Color SurfaceRaised = Obsidian600;
    public static readonly Color SurfaceInset = Obsidian800;
    public static readonly Color SurfaceSlot = Hex("0A0E16");
    public static readonly Color SurfaceOverlay = new(0.031f, 0.043f, 0.067f, 0.78f);
    public static readonly Color SurfaceScrim = new(0.031f, 0.043f, 0.067f, 0.92f);
    public static readonly Color SurfaceGlass = new(0.075f, 0.098f, 0.149f, 0.72f);

    // ---- Semantic strokes -------------------------------------------------
    public static readonly Color BorderPanel = Hex("2A3348");
    public static readonly Color BorderBevelTop = new(0.812f, 0.843f, 0.894f, 0.14f);
    public static readonly Color BorderBevelBottom = new(0.031f, 0.043f, 0.067f, 0.7f);
    public static readonly Color BorderStrong = Steel500;
    public static readonly Color BorderAccent = Brass500;
    public static readonly Color BorderFocus = Arcane400;

    // ---- Semantic text ----------------------------------------------------
    public static readonly Color TextPrimary = Steel050;
    public static readonly Color TextSecondary = Steel200;
    public static readonly Color TextMuted = Steel400;
    public static readonly Color TextDisabled = Hex("4A5568");
    public static readonly Color TextAccent = Brass400;
    public static readonly Color TextArcane = Arcane400;
    public static readonly Color TextInverse = Obsidian900;

    // ---- States and game semantics ---------------------------------------
    public static readonly Color StateDanger = Threat500;
    public static readonly Color StateWarning = Ember500;
    public static readonly Color StateSuccess = Venom500;
    public static readonly Color StateInfo = Arcane500;

    public static readonly Color ResGold = Brass500;
    public static readonly Color BarHp = Venom500;
    public static readonly Color BarHpLow = Threat500;
    public static readonly Color BarShield = Arcane500;
    public static readonly Color BarArmor = Steel300;
    public static readonly Color BarTrack = Hex("0A0E16");
    public static readonly Color Lives = Threat400;
    public static readonly Color WaveIdle = Steel400;
    public static readonly Color WaveActive = Ember500;
    public static readonly Color WaveBoss = Soul500;

    // ---- Type scale -------------------------------------------------------
    public const int SizeDisplayXl = 56, SizeDisplayLg = 40, SizeDisplayMd = 30, SizeDisplaySm = 22;
    public const int SizeTitle = 18, SizeBodyLg = 16, SizeBody = 14, SizeCaption = 12, SizeMicro = 11;
    public const int SizeStatXl = 34, SizeStat = 20, SizeStatSm = 15;

    /// <summary>Letter spacing, in pixels at the size it is applied to. Labels
    /// are widely tracked all-caps micro type — it is most of the look.</summary>
    public const float TrackingLabel = 1.5f;    // .14em at 11px
    public const float TrackingMicro = 2.0f;    // .18em at 11px

    // ---- Spacing ----------------------------------------------------------
    public const int Space1 = 2, Space2 = 4, Space3 = 6, Space4 = 8, Space5 = 12;
    public const int Space6 = 16, Space7 = 20, Space8 = 24, Space9 = 32, Space10 = 40;
    public const int Space11 = 48, Space12 = 64, Space13 = 80;

    public const int PadPanel = 16, PadPanelTight = 12;
    public const int GapInline = 8, GapStack = 12, GapSection = 24;

    // HUD layout constants — fixed chrome sizes.
    public const int HudTopbarH = 56, HudDockH = 112, HudRailW = 264, HudEdge = 16;
    public const int ControlSm = 28, ControlMd = 36, ControlLg = 46, Slot = 56, SlotLg = 72;

    // ---- Effects ----------------------------------------------------------
    public const int ChamferSm = 6, ChamferMd = 10, ChamferLg = 16;
    public const int StrokeHairline = 1, StrokePanel = 1, StrokeEmphasis = 2;

    public static readonly Color ShadowDrop = new(0.031f, 0.043f, 0.067f, 0.9f);
    public static readonly Color GlowBrass = new(0.784f, 0.608f, 0.235f, 0.28f);
    public static readonly Color GlowArcane = new(0.184f, 0.706f, 0.745f, 0.30f);
    public static readonly Color GlowThreat = new(0.788f, 0.231f, 0.157f, 0.34f);

    // Motion.
    public const float DurInstant = 0.08f, DurFast = 0.14f, DurBase = 0.22f;
    public const float DurSlow = 0.40f, DurAmbient = 1.60f;

    // ---- Fonts ------------------------------------------------------------
    // Design named Oxanium / Barlow / JetBrains Mono. Loaded from
    // game/assets/fonts/; if a file is missing the engine default is used and
    // only the weight and size roles survive.
    private static readonly Dictionary<string, FontFile?> FontCache = new();

    private static FontFile? Load(string file)
    {
        if (FontCache.TryGetValue(file, out var cached)) return cached;
        string path = $"res://assets/fonts/{file}.ttf";
        var font = ResourceLoader.Exists(path) ? GD.Load<FontFile>(path) : null;
        FontCache[file] = font;
        return font;
    }

    /// <summary>Headings, panel titles and all-caps labels.</summary>
    public static FontFile? Display => Load("Oxanium-Bold");
    public static FontFile? DisplayHeavy => Load("Oxanium-ExtraBold");
    /// <summary>Body copy and descriptions.</summary>
    public static FontFile? Body => Load("Barlow-Regular");
    public static FontFile? BodyStrong => Load("Barlow-SemiBold");
    /// <summary>Every numeral in the game — tabular so counters do not jitter.</summary>
    public static FontFile? Mono => Load("JetBrainsMono-Bold");
    public static FontFile? MonoRegular => Load("JetBrainsMono-Regular");
}
