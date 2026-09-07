using Godot;
using System.Collections.Generic;
using System.Linq;
using DeepField.Sim.Content;

namespace DeepField.Game.Ui;

/// <summary>Lobby and faction menu, built to design's frame.
///
/// Three faction columns, one per player. The sim refuses duplicates
/// ("factionTaken"), so the lobby's job is to make that rule visible *before*
/// anyone is refused — each column states whether it is yours, open, or taken,
/// and the level curve underneath is the argument for sticking with the one you
/// have been levelling.
///
/// The curve is the part worth getting right: cooldown, radius and magnitude
/// all improve per level, and drawing three lines with a marker at your current
/// level answers "what does levelling actually buy me" in one glance.</summary>
public partial class LobbyScreen : Control
{
    public System.Action<string>? OnSolo;            // faction
    public System.Action<string>? OnHost;            // faction
    public System.Action<string, string>? OnJoin;    // address, faction

    private Profile _profile = new();
    private string _faction = "ember";
    private MapDef _map = Maps.Foundry;

    private LineEdit _nameEdit = null!;
    private LineEdit _addressEdit = null!;
    private Label _status = null!;
    private Label _sectorLine = null!;
    /// <summary>One row, however many factions there are. A wrapping grid was
    /// tried and cut: two rows of this card need about 1280 px and a 1080p
    /// screen has roughly 870 to give, so the second row fell off the bottom —
    /// trading a horizontal overflow for a vertical one. The card content is
    /// what has to stay narrow instead.</summary>
    private HBoxContainer _columns = null!;
    private MarginContainer _pageHost = null!;
    private string _page = "factions";
    private readonly Dictionary<string, KitButton> _tabs = new();

    public string PlayerName => _nameEdit.Text.Length > 0 ? _nameEdit.Text : "player";
    public MapDef SelectedMap => _map;

    public void Build(Profile profile)
    {
        _profile = profile;
        _faction = profile.PreferredFaction;

        SetAnchorsAndOffsetsPreset(LayoutPreset.FullRect);

        var backdrop = new ColorRect { Color = Tokens.Obsidian900 };
        backdrop.SetAnchorsAndOffsetsPreset(LayoutPreset.FullRect);
        AddChild(backdrop);
        AddChild(new KitGrid());

        var frame = new VBoxContainer();
        frame.SetAnchorsAndOffsetsPreset(LayoutPreset.FullRect);
        frame.AddThemeConstantOverride("separation", 0);
        AddChild(frame);

        // --- header
        var (bar, header) = Kit.ScreenHeader("Lobby");
        bar.SizeFlagsHorizontal = SizeFlags.ExpandFill;
        frame.AddChild(bar);

        _nameEdit = new LineEdit { Text = profile.Name, CustomMinimumSize = new Vector2(200, 0) };
        header.AddChild(Kit.Label("callsign"));
        header.AddChild(_nameEdit);
        header.AddChild(Kit.Spacer());

        // Design splits the lobby into tabs; factions and sector are the two
        // that have anything behind them. Loadout is the in-match armory, so
        // it is not offered here rather than shown as a dead tab.
        foreach (var (id, label) in new[] { ("factions", "Factions"), ("sector", "Sector") })
        {
            string captured = id;
            var tab = new KitButton(label,
                id == _page ? KitButton.Tone.Primary : KitButton.Tone.Ghost, Tokens.ControlSm);
            tab.Pressed += () => { _page = captured; RebuildPage(); };
            header.AddChild(tab);
            _tabs[id] = tab;
        }

        header.AddChild(Kit.Spacer());
        _status = Kit.Body("", Tokens.SizeCaption, UiTheme.Warn);
        header.AddChild(_status);

        // --- faction columns
        var margin = new MarginContainer { SizeFlagsVertical = SizeFlags.ExpandFill };
        foreach (string side in new[] { "left", "right", "top", "bottom" })
            margin.AddThemeConstantOverride($"margin_{side}", Tokens.Space8);
        frame.AddChild(margin);

        _columns = Kit.Row(Tokens.Space7);
        margin.AddChild(_columns);
        _pageHost = margin;

        // --- launch strip
        var strip = Kit.Surface(Tokens.SurfacePanel, Tokens.BorderPanel);
        strip.SizeFlagsHorizontal = SizeFlags.ExpandFill;
        var stripRow = Kit.Row(Tokens.Space6);
        strip.AddChild(stripRow);

        stripRow.AddChild(Kit.Label("sector"));
        _sectorLine = Kit.Title("", Tokens.SizeBody);
        stripRow.AddChild(_sectorLine);

        var swap = new KitButton("Change", KitButton.Tone.Ghost, Tokens.ControlSm);
        swap.Pressed += CycleMap;
        stripRow.AddChild(swap);
        stripRow.AddChild(Kit.Spacer());

        _addressEdit = new LineEdit
        {
            PlaceholderText = "100.x.x.x",
            Text = profile.LastJoinAddress,
            CustomMinimumSize = new Vector2(170, 0),
        };
        stripRow.AddChild(_addressEdit);

        var join = new KitButton("Join", KitButton.Tone.Secondary);
        join.Pressed += () =>
        {
            if (_addressEdit.Text.Trim().Length == 0) { _status.Text = "enter the host's address"; return; }
            Persist();
            _profile.LastJoinAddress = _addressEdit.Text.Trim();
            _profile.Save();
            OnJoin?.Invoke(_addressEdit.Text.Trim(), _faction);
        };
        stripRow.AddChild(join);

        var solo = new KitButton("Solo", KitButton.Tone.Secondary);
        solo.Pressed += () => { Persist(); OnSolo?.Invoke(_faction); };
        stripRow.AddChild(solo);

        var host = new KitButton("Host", KitButton.Tone.Primary, Tokens.ControlLg);
        host.Pressed += () => { Persist(); OnHost?.Invoke(_faction); };
        stripRow.AddChild(host);

        var stripMargin = new MarginContainer();
        foreach (string side in new[] { "left", "right", "bottom" })
            stripMargin.AddThemeConstantOverride($"margin_{side}", Tokens.Space8);
        stripMargin.AddChild(strip);
        frame.AddChild(stripMargin);

        RebuildPage();
        RefreshSector();
    }

    /// <summary>Swaps the body between the faction columns and sector select.
    /// Both are grids of panels, so the page host just gets a new row.</summary>
    private void RebuildPage()
    {
        foreach (var (id, tab) in _tabs)
            tab.AddThemeStyleboxOverride("normal", new ChamferBox
            {
                Fill = id == _page ? Tokens.Brass500 : new Color(0, 0, 0, 0),
                Stroke = id == _page ? Tokens.Brass300 : Tokens.BorderPanel,
                Chamfer = 7f,
                DropShadow = id == _page,
            });

        foreach (var child in _columns.GetChildren()) child.QueueFree();
        if (_page == "sector") RebuildSectors();
        else RebuildFactions();
    }

    // =====================================================================
    // Sector select
    // =====================================================================

    private void RebuildSectors()
    {
        foreach (var map in Maps.All.Values)
        {
            if (map.Id == "testlane") continue;      // harness fixture, not a sector
            bool selected = map.Id == _map.Id;
            bool unlocked = Campaign.IsUnlocked(map.Id, _profile.ClearedSectors);
            bool cleared = _profile.ClearedSectors.Contains(map.Id);
            int best = _profile.BestWave.GetValueOrDefault(map.Id, 0);

            var panel = new KitPanel(map.Id, selected && unlocked ? Tokens.Brass500 : null);
            panel.SizeFlagsHorizontal = SizeFlags.ExpandFill;
            panel.HeaderTrailing(cleared ? Kit.TagBrass("cleared")
                : !unlocked ? Kit.Tag("locked")
                : Kit.Tag($"{map.TotalWaves} waves"));
            panel.Modulate = unlocked ? Colors.White : new Color(1, 1, 1, 0.45f);
            _columns.AddChild(panel);

            // The sketch takes the card's slack, for the same reason the hero
            // well does on the factions tab: it is the card's picture, and a
            // bigger route diagram is more readable than a gap under a small one.
            var sketch = new KitRouteSketch { Map = map };
            sketch.SizeFlagsVertical = SizeFlags.ExpandFill;
            panel.Body.AddChild(sketch);

            // Legend, so the sketch is readable without a caption.
            var legend = Kit.Row(Tokens.Space5);
            legend.AddChild(Kit.Label("ground", Tokens.Brass400));
            legend.AddChild(Kit.Label("air", Tokens.Arcane400));
            if (map.Routes.Any(r => r.BarricadeGate is not null))
                legend.AddChild(Kit.Label("shortcut", Tokens.Threat400));
            panel.Body.AddChild(legend);

            panel.Body.AddChild(Kit.Paragraph(Blurb(map.Id), Tokens.SizeCaption, Tokens.TextSecondary));

            // The barricade map's lesson is stated, because a player who does
            // not know about b1 will lose to the shortcut without seeing why.
            if (map.Routes.FirstOrDefault(r => r.BarricadeGate is not null) is { } gated)
                panel.Body.AddChild(Kit.Toast(
                    $"the short route needs {gated.BarricadeGate}", Tokens.Threat500));

            // Best result: the number that tells a player whether they are
            // close. Shown for a loss too, which is when it matters most.
            if (best > 0 && !cleared)
                panel.Body.AddChild(Kit.Toast(
                    $"best: wave {best} of {map.TotalWaves}", Tokens.Brass500));

            var counts = Kit.Row(Tokens.Space5);
            counts.AddChild(Kit.Label($"{map.Sockets.Count} sockets"));
            counts.AddChild(Kit.Label($"{map.HeroStations.Count} stations"));
            counts.AddChild(Kit.Label($"{map.Routes.Count} lanes"));
            panel.Body.AddChild(counts);

            string captured = map.Id;
            var footer = Kit.Row();
            footer.AddChild(Kit.Label(
                !unlocked ? "locked" : selected ? "selected" : "available"));
            footer.AddChild(Kit.Spacer());
            if (!unlocked)
            {
                // Name what opens it. A locked door with no sign is a bug
                // report; a locked door that says which key is a goal.
                int i = Campaign.IndexOf(map.Id);
                footer.AddChild(Kit.Label($"clear {Campaign.Sectors[i - 1]} first",
                    Tokens.TextSecondary));
            }
            else if (selected)
            {
                footer.AddChild(Kit.TagBrass("selected"));
            }
            else
            {
                var pick = new KitButton("Select", KitButton.Tone.Secondary, Tokens.ControlSm);
                pick.Pressed += () =>
                {
                    _map = Maps.All[captured];
                    RefreshSector();
                    RebuildPage();
                };
                footer.AddChild(pick);
            }
            panel.SetFooter(footer);
        }
    }

    /// <summary>One line on what the sector asks of you. Kept here rather than
    /// in MapDef because it is copy, not content the sim reads.</summary>
    private static string Blurb(string mapId) => mapId switch
    {
        "foundry" => "Two tiers. The ground lane winds under the deck and an air "
            + "strand climbs out of ground-tower reach across the middle.",
        "switchyard" => "Three tiers. The freight cut is fast and badly covered; "
            + "a barricade closes it and forces the long switchback past the kill-boxes.",
        "spire" => "A tower block, and the core is on the roof. Enemies climb: "
            + "the stair through the inside, the fire escape up the outside, and "
            + "flyers spiralling past both. The last two flights are shared, so "
            + "whatever you let through downstairs, you meet again at the top.",
        _ => "",
    };

    public void SetStatus(string message) => _status.Text = message;

    /// <summary>Used by --shot so the sector page can be reviewed.</summary>
    public void ShowSectorTab() { _page = "sector"; RebuildPage(); }

    private void Persist()
    {
        _profile.Name = PlayerName;
        _profile.PreferredFaction = _faction;
        _profile.Save();
    }

    private void CycleMap()
    {
        var playable = Maps.All.Values.Where(m => m.Id != "testlane").ToList();
        int index = playable.FindIndex(m => m.Id == _map.Id);
        _map = playable[(index + 1) % playable.Count];
        RefreshSector();
    }

    private void RefreshSector()
    {
        int sockets = _map.Sockets.Count;
        _sectorLine.Text =
            $"{_map.Id.ToUpperInvariant()} · {_map.TotalWaves} WAVES · {sockets} SOCKETS";
    }

    /// <summary>The sim stores a passive as an id; these are the effects it
    /// actually applies, read off Balance so the lobby cannot drift from the
    /// numbers the match uses.</summary>
    internal static string PassiveText(string passiveId) => passiveId switch
    {
        "buildDiscount" => $"−{(1f - Balance.ForgeBuildDiscount) * 100f:0}% build cost",
        "burnDuration" => $"+{(Balance.EmberBurnDurationFactor - 1f) * 100f:0}% burn duration",
        "reloadSpeed" => "+12% fire rate",
        "chilledBonus" => $"+{(Balance.GlacierChilledDamageFactor - 1f) * 100f:0}% vs slowed",
        "weakPoints" => "weak points shown",
        // Falling through prints the raw content id at the player, which is
        // exactly what Glacier and Specter did: both were added to the sim and
        // nobody taught this switch about them, so the lobby advertised
        // "chilledBonus" and "weakPoints" as if they were English. Plausible
        // enough to survive a review, which is why UnwrittenPassives below is
        // checked at boot rather than trusted.
        _ => passiveId,
    };

    /// <summary>Factions whose passive has no human wording — the switch above
    /// would show the player a content id. Empty is the only correct answer;
    /// the asset audit prints an ERROR for anything here and CI fails on it,
    /// because this exact gap shipped once already.</summary>
    public static IReadOnlyList<string> UnwrittenPassives() =>
        Factions.All.Values
            .Where(f => PassiveText(f.Passive) == f.Passive)
            .Select(f => $"{f.Id}/{f.Passive}")
            .ToList();

    /// <summary>The hero, actually rendered, with the faction icon beside it.
    /// The well used to hold a lone 64 px glyph in a box tall enough for a
    /// person — and design ships hero_forge.glb and its siblings, so the space
    /// the layout pass reclaimed is spent on the thing it was reclaimed for.
    ///
    /// Falls back to the icon alone where no model exists, which today is
    /// Glacier and Specter: both are on the forward manifest and undelivered,
    /// and a placeholder chip standing in for a person reads as a bug rather
    /// than as a gap.</summary>
    private static Control HeroPortrait(string factionId, Color accent)
    {
        var frame = new Control { MouseFilter = Control.MouseFilterEnum.Ignore };

        if (AssetLibrary.Has($"hero_{factionId}"))
        {
            var viewport = new SubViewport
            {
                TransparentBg = true,
                RenderTargetUpdateMode = SubViewport.UpdateMode.Always,
                Size = new Vector2I(360, 480),
                // Its own world, or the camera renders whatever level is
                // loaded. This happens to look fine in the lobby because there
                // is no level yet — which is exactly why the same code in the
                // armory rendered the inside of the Foundry instead of a gun.
                OwnWorld3D = true,
            };

            var stage = new Node3D();
            viewport.AddChild(stage);

            var hero = AssetLibrary.Instantiate($"hero_{factionId}", () => Placeholders.Hero(factionId));
            // Turned to face the camera, per the brief's stated contract:
            // models are Y-up, -Z forward, so at zero rotation a hero faces
            // away from a camera sitting on +Z. 156 is that half-turn with a
            // little off-axis so a flat-shaded figure does not read as a
            // silhouette.
            //
            // Worth saying plainly: these heroes have no faces at this fidelity
            // and I could not settle front from back by eye — I went round the
            // houses trying. This follows the contract design wrote down rather
            // than my reading of the render, and if a hero turns out to be
            // presenting its backpack, this is the number to flip.
            hero.RotationDegrees = new Vector3(0f, 156f, 0f);
            stage.AddChild(hero);

            // Framed on a 1.8 m capsule from slightly above, which is the pose
            // a character select uses — looking down a touch is friendlier than
            // looking up someone's chin.
            var camera = new Camera3D
            {
                Position = new Vector3(0f, 1.15f, 2.9f),
                RotationDegrees = new Vector3(-6f, 0f, 0f),
                Fov = 42f,
                Current = true,
            };
            stage.AddChild(camera);

            // A DirectionalLight3D shines along its own -Z, and the camera
            // looks the same way, so a key near zero yaw lights the side facing
            // the viewer. At 138 it was shining back at the camera and lighting
            // the hero's shoulder blades — which flattered the pose only while
            // the pose was backwards.
            var key = new DirectionalLight3D
            {
                RotationDegrees = new Vector3(-28f, 26f, 0f),
                LightEnergy = 1.5f,
                LightColor = accent.Lerp(Colors.White, 0.55f),
            };
            stage.AddChild(key);

            var fill = new DirectionalLight3D
            {
                RotationDegrees = new Vector3(-8f, -48f, 0f),
                LightEnergy = 0.45f,
                LightColor = Tokens.Arcane400.Lerp(Colors.White, 0.4f),
            };
            stage.AddChild(fill);

            var container = new SubViewportContainer { Stretch = true };
            container.SetAnchorsAndOffsetsPreset(Control.LayoutPreset.FullRect);
            container.MouseFilter = Control.MouseFilterEnum.Ignore;
            container.AddChild(viewport);
            frame.AddChild(container);
        }

        // The icon rides in the corner rather than the middle: it is the
        // faction's mark, and with a figure in the well it stops being the
        // subject. Design puts it bottom-left of the portrait.
        var badge = Kit.SlotIcon(UiTheme.Icon($"faction_{factionId}", accent), accent, 44);
        badge.SetAnchorsPreset(Control.LayoutPreset.BottomLeft);
        badge.Position = new Vector2(Tokens.Space4, -Tokens.Space4 - 44);
        frame.AddChild(badge);
        return frame;
    }

    // =====================================================================

    private void RebuildFactions()
    {
        foreach (var child in _columns.GetChildren()) child.QueueFree();

        foreach (var def in Factions.All.Values)
        {
            bool mine = def.Id == _faction;
            int level = _profile.LevelFor(def.Id);
            var accent = UiTheme.Faction(def.Id);

            var panel = new KitPanel(def.Id, mine ? Tokens.Brass500 : null);
            panel.SizeFlagsHorizontal = SizeFlags.ExpandFill;
            panel.HeaderTrailing(mine ? Kit.TagBrass("you") : Kit.Tag("open"));
            _columns.AddChild(panel);

            // Hero portrait well. The model is delivered; a lit well with the
            // faction diamond stands in until heroes render in the lobby.
            //
            // It is also the one element allowed to grow. Design laid this
            // screen out for three columns at 1920 wide; there are five
            // factions now and six planned, so the cards get narrower while the
            // column stays the same height — which at full screen left roughly
            // seven hundred pixels of nothing under every card. Everything else
            // keeps design's size and the well takes the slack, because a
            // taller picture box is a better hero portrait and dead air is not
            // anything. 220 is design's own minimum.
            var well = Kit.Surface(Tokens.SurfaceInset, Tokens.BorderPanel, Tokens.ChamferSm, shadow: false);
            well.CustomMinimumSize = new Vector2(0, 220);
            well.SizeFlagsVertical = SizeFlags.ExpandFill;
            var portrait = HeroPortrait(def.Id, accent);
            portrait.SizeFlagsHorizontal = SizeFlags.ExpandFill;
            portrait.SizeFlagsVertical = SizeFlags.ExpandFill;
            well.AddChild(portrait);
            panel.Body.AddChild(well);

            // Ability identity: what Q does, and what you get for free.
            var identity = Kit.Row();
            var left = Kit.Col(2);
            left.SizeFlagsHorizontal = SizeFlags.ExpandFill;
            left.AddChild(Kit.Title(def.AbilityId.ToUpperInvariant(), Tokens.SizeDisplaySm, accent));
            left.AddChild(Kit.Label($"Q · {def.CooldownSeconds:0}s · {def.RadiusMeters:0}m"));
            identity.AddChild(left);
            var right = Kit.Col(2);
            right.AddChild(Kit.Label("passive"));
            var passive = Kit.Body(PassiveText(def.Passive), Tokens.SizeCaption, Tokens.TextSecondary);
            passive.HorizontalAlignment = HorizontalAlignment.Right;
            right.AddChild(passive);
            identity.AddChild(right);
            panel.Body.AddChild(identity);

            // Level and progress toward the next one.
            int xp = _profile.FactionXp.TryGetValue(def.Id, out int banked) ? banked : 0;
            int span = level * 100;
            panel.Body.AddChild(Kit.Between(
                Kit.Label($"level {level} / 5"),
                Kit.Numeral($"{xp} / {span} xp", Tokens.SizeCaption, Tokens.TextSecondary)));
            panel.Body.AddChild(Kit.Bar(span == 0 ? 0f : Mathf.Clamp((float)xp / span, 0f, 1f),
                Tokens.Soul500, 8f));

            // The curve: three lines, a marker at where you are.
            panel.Body.AddChild(Kit.Label("level curve"));
            var curve = new KitCurve { Level = level };
            panel.Body.AddChild(curve);

            // The legend carries the actual multipliers at this level, not just
            // the three words — design shows "cooldown x0.94", and the numbers
            // are what make the curve mean something.
            // Stacked, not in a row: three multipliers side by side set the
            // card's minimum width, and five of those minimums are wider than a
            // 1080p screen. Vertically they cost nothing, because reclaiming
            // that space is the point of this pass.
            var legend = Kit.Col(2);
            legend.AddChild(Kit.Label(
                $"cooldown ×{Factions.CooldownFactor(level):0.00}", Tokens.Arcane400));
            legend.AddChild(Kit.Label(
                $"radius ×{Factions.RadiusFactor(level):0.00}", Tokens.Brass400));
            legend.AddChild(Kit.Label(
                $"magnitude ×{Factions.MagnitudeFactor(level):0.00}", Tokens.Soul400));
            panel.Body.AddChild(legend);

            // Variant slot. Design has this row and we never built it; it is
            // the reserved home for the high-level ability variants the plan
            // puts at M5, so today it states honestly what unlocks it rather
            // than showing a choice that does not exist.
            var variant = Kit.Row(Tokens.Space4);
            var slot = Kit.Surface(Tokens.SurfaceInset, Tokens.BorderPanel, Tokens.ChamferSm, shadow: false);
            slot.CustomMinimumSize = new Vector2(40, 40);
            variant.AddChild(slot);
            var variantText = Kit.Col(2);
            variantText.SizeFlagsHorizontal = SizeFlags.ExpandFill;
            variantText.AddChild(Kit.Label("variant slot"));
            variantText.AddChild(Kit.Body(
                level >= 2 ? "none yet — M5" : "unlocks at level 2",
                Tokens.SizeCaption, Tokens.TextSecondary));
            variant.AddChild(variantText);
            panel.Body.AddChild(variant);

            // Footer: pick it.
            string captured = def.Id;
            var footer = Kit.Row();
            footer.AddChild(Kit.Label(mine ? "selected" : "available"));
            footer.AddChild(Kit.Spacer());
            if (!mine)
            {
                var pick = new KitButton("Select", KitButton.Tone.Secondary, Tokens.ControlSm);
                pick.Pressed += () => { _faction = captured; RebuildFactions(); };
                footer.AddChild(pick);
            }
            else
            {
                footer.AddChild(Kit.TagBrass("ready"));
            }
            panel.SetFooter(footer);
        }
    }
}

/// <summary>The faction level curve: cooldown falls, radius and magnitude rise,
/// with a dashed marker at the level you are actually at. Design draws this as
/// an SVG polyline trio; the numbers are the same per-level factors the sim
/// applies, so the chart is a promise the game keeps.</summary>
public partial class KitCurve : Control
{
    public int Level = 1;

    public KitCurve() => CustomMinimumSize = new Vector2(0, 76);

    public override void _Draw()
    {
        if (Size.X < 10) return;

        DrawRect(new Rect2(Vector2.Zero, Size), Tokens.SurfaceInset);
        DrawRect(new Rect2(Vector2.Zero, Size), Tokens.BorderPanel, filled: false, width: 1f);

        // Same factors the faction system uses per level.
        // Each series gets its own third of the box; overlaid on one scale
        // they sit within a pixel of each other and read as a single line.
        Line(l => Mathf.Pow(0.94f, l), Tokens.Arcane400, invert: true, band: 0);
        Line(l => 1f + 0.06f * l, Tokens.Brass400, invert: false, band: 1);
        Line(l => 1f + 0.05f * l, Tokens.Soul400, invert: false, band: 2);

        float markerX = Size.X * (Level - 1) / 4f;
        for (float y = 0; y < Size.Y; y += 6)
            DrawLine(new Vector2(markerX, y), new Vector2(markerX, y + 3), Tokens.Steel300, 1f);

        void Line(System.Func<int, float> factor, Color color, bool invert, int band)
        {
            float bandHeight = (Size.Y - 12) / 3f;
            float bottom = Size.Y - 6 - band * bandHeight;
            var points = new Vector2[5];
            for (int i = 0; i < 5; i++)
            {
                float value = factor(i);
                float span = invert ? 1f - value : value - 1f;
                float norm = Mathf.Clamp(span / 0.26f, 0f, 1f);
                points[i] = new Vector2(Size.X * i / 4f, bottom - norm * (bandHeight - 4));
            }
            DrawPolyline(points, color, 1.5f);
        }
    }
}
