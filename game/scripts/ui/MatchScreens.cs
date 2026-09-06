using Godot;
using System.Collections.Generic;
using System.Linq;
using DeepField.Sim;
using DeepField.Sim.Content;

namespace DeepField.Game.Ui;

/// <summary>The non-HUD in-match surfaces: intermission recap with the next
/// wave previewed, the end-of-match scoreboard, the pause/system menu, and the
/// connection/status overlay clients live in before the world arrives.</summary>
public partial class MatchScreens : CanvasLayer
{
    public System.Action? OnResume;
    public System.Action? OnLeave;
    public System.Action? OnSave;
    public System.Action? OnLoad;
    public System.Action<bool>? OnToggleDamageNumbers;

    private Control _intermission = null!;
    private VBoxContainer _intermissionBody = null!;
    private KitPanel _intermissionPanel = null!;
    private Label _intermissionTitle = null!;
    private Control _endScreen = null!;
    private VBoxContainer _endBody = null!;
    private Control _pause = null!;
    private Control _howTo = null!;
    private Control _status = null!;
    private Label _statusText = null!;
    private Label _statusDetail = null!;

    public bool PauseOpen => _pause.Visible;

    public override void _Ready()
    {
        Layer = 2;
        BuildIntermission();
        BuildEndScreen();
        BuildPause();
        BuildHowTo();
        BuildStatus();
    }

    // =====================================================================
    // Intermission
    // =====================================================================

    private void BuildIntermission()
    {
        _intermission = new Control { MouseFilter = Control.MouseFilterEnum.Ignore, Visible = false };
        _intermission.SetAnchorsAndOffsetsPreset(Control.LayoutPreset.FullRect);
        AddChild(_intermission);

        // Top-centre and wide, per design — it has to be readable while you
        // are still walking around building, not a modal that stops play.
        _intermissionPanel = new KitPanel("Next wave", Tokens.Brass500, hazard: true);
        _intermissionPanel.SetAnchorsPreset(Control.LayoutPreset.CenterTop);
        _intermissionPanel.Position = new Vector2(-410, 84);
        _intermissionPanel.CustomMinimumSize = new Vector2(820, 0);
        _intermission.AddChild(_intermissionPanel);

        _intermissionTitle = Kit.Label("", Tokens.TextSecondary);
        _intermissionPanel.HeaderTrailing(_intermissionTitle);
        _intermissionBody = _intermissionPanel.Body;
    }

    /// <summary>Wave composition is a pure function of (seed, map, wave,
    /// players), so the client previews the next wave honestly without asking
    /// the server — and the same call the sim will make is the one drawn here.</summary>
    public void ShowIntermission(GameView view, MapDef map, uint seed, int lastWaveLeaks)
    {
        _intermission.Visible = true;
        foreach (var child in _intermissionBody.GetChildren()) child.QueueFree();

        int next = view.Wave + 1;
        bool more = next < map.TotalWaves;
        _intermissionTitle.Text = more
            ? $"wave {next + 1} of {map.TotalWaves}"
            : "final wave cleared";

        if (!more)
        {
            _intermissionBody.AddChild(Kit.Paragraph(
                "Hold what you have — the core survives or it does not.",
                Tokens.SizeBody, Tokens.TextSecondary));
            return;
        }

        int players = Mathf.Max(1, view.Players.Count(p => p.Connected));
        var plan = WavePlan.PlanWave(seed, map, next, players);

        // Split by the lane each group walks, because that is the decision the
        // panel exists to inform: ground coverage or air coverage.
        var lanes = plan
            .GroupBy(e => map.Routes[e.RouteIndex].Layer)
            .OrderBy(g => g.Key == EnemyLayer.Air ? 1 : 0)
            .ToList();

        var debuts = plan.Select(e => e.DefId).Distinct()
            .Where(id => !SeenBefore(seed, map, next, players, id)).ToList();

        var grid = Kit.Row(Tokens.Space6);
        _intermissionBody.AddChild(grid);

        foreach (var lane in lanes)
        {
            var column = Kit.Col(Tokens.Space3);
            column.SizeFlagsHorizontal = Control.SizeFlags.ExpandFill;
            column.AddChild(Kit.Label(lane.Key == EnemyLayer.Air ? "air lane" : "ground lane"));

            foreach (var group in lane.GroupBy(e => e.DefId).OrderByDescending(g => g.Count()))
            {
                var strip = Kit.Surface(Tokens.SurfaceInset, Tokens.BorderPanel, 4f, shadow: false);
                var row = Kit.Row(Tokens.Space4);
                strip.AddChild(row);

                row.AddChild(Kit.Icon($"enemy_{group.Key}", Tokens.TextSecondary, 22));
                var name = Kit.Body(group.Key, Tokens.SizeBody, Tokens.TextPrimary);
                name.SizeFlagsHorizontal = Control.SizeFlags.ExpandFill;
                row.AddChild(name);
                if (debuts.Contains(group.Key)) row.AddChild(Kit.TagDanger("new"));
                row.AddChild(Kit.Numeral($"×{group.Count()}", Tokens.SizeStatSm, Tokens.TextPrimary));

                column.AddChild(strip);
            }
            grid.AddChild(column);
        }

        // The debut card: what it is and what answers it. This is the "one to
        // learn on" convention — a new enemy arrives with its counter stated.
        foreach (string debut in debuts)
        {
            var card = Kit.Card();
            var column = Kit.Col(Tokens.Space3);
            card.AddChild(column);
            column.AddChild(Kit.Label(debut, Tokens.TextAccent));
            column.AddChild(Kit.Paragraph(Hint(debut), Tokens.SizeCaption, Tokens.TextSecondary));

            var counters = Kit.Row(Tokens.Space3);
            foreach (string counter in Counters(debut)) counters.AddChild(Kit.Tag(counter));
            column.AddChild(counters);
            _intermissionBody.AddChild(card);
        }

        _intermissionBody.AddChild(Kit.Rule());

        var footer = Kit.Row(Tokens.Space6);
        footer.AddChild(Kit.Label("last wave"));
        footer.AddChild(view.Wave < 0
            ? Kit.Body("—", Tokens.SizeCaption, Tokens.TextMuted)
            : Kit.Body(lastWaveLeaks > 0 ? $"leaked {lastWaveLeaks}" : "held clean",
                Tokens.SizeCaption, lastWaveLeaks > 0 ? UiTheme.Warn : UiTheme.Good));
        footer.AddChild(Kit.Label("core"));
        footer.AddChild(Kit.Numeral(view.Lives.ToString(), Tokens.SizeStatSm,
            view.Lives <= 5 ? UiTheme.Danger : Tokens.Lives));
        footer.AddChild(Kit.Spacer());
        footer.AddChild(Kit.Label("start early"));
        footer.AddChild(Kit.Key("F"));
        _intermissionBody.AddChild(footer);
    }

    /// <summary>True if this type has already appeared in an earlier wave, so
    /// only genuine debuts get called out.</summary>
    private static bool SeenBefore(uint seed, MapDef map, int wave, int players, string defId)
    {
        for (int i = 0; i < wave; i++)
            if (WavePlan.PlanWave(seed, map, i, players).Any(e => e.DefId == defId)) return true;
        return false;
    }

    private static string Hint(string defId) => defId switch
    {
        "skiff" => "Flyer — ground towers cannot reach it.",
        "aegis" => "Armoured 140° front plate; the rear takes bonus damage.",
        "monolith" => "Blocks tower sightlines for everything in its shadow.",
        "warden" => "A shield soaks damage, blocks burn, and regrows after a lull.",
        "mole" => "Burrows, and is only targetable during its surface windows.",
        "cluster" => "Splits into five low-hp motes when it dies.",
        "mote" => "Fast swarm that spreads across the lane width.",
        "drifter" => "The baseline walker — the reference for everything else.",
        _ => "New contact.",
    };

    /// <summary>What answers this enemy. Stated as tags rather than prose so a
    /// player scanning between builds gets it without reading a sentence.</summary>
    private static string[] Counters(string defId) => defId switch
    {
        "skiff" => new[] { "skywatch", "deck sockets" },
        "aegis" => new[] { "flank it", "shred", "ap ammo" },
        "monolith" => new[] { "splash", "reposition" },
        "warden" => new[] { "burst the shield", "poison later" },
        "mole" => new[] { "traps", "surface windows" },
        "cluster" => new[] { "splash", "pre-place" },
        "mote" => new[] { "nova", "spread coverage" },
        "drifter" => new[] { "anything" },
        _ => new[] { "improvise" },
    };

    public void HideIntermission() => _intermission.Visible = false;

    // =====================================================================
    // End of match
    // =====================================================================

    private void BuildEndScreen()
    {
        _endScreen = new Control { MouseFilter = Control.MouseFilterEnum.Ignore, Visible = false };
        _endScreen.SetAnchorsAndOffsetsPreset(Control.LayoutPreset.FullRect);
        AddChild(_endScreen);

        var backdrop = new ColorRect { Color = new Color(0.02f, 0.03f, 0.05f, 0.75f) };
        backdrop.SetAnchorsAndOffsetsPreset(Control.LayoutPreset.FullRect);
        _endScreen.AddChild(backdrop);

        var card = UiTheme.Card();
        card.SetAnchorsPreset(Control.LayoutPreset.Center);
        card.Position = new Vector2(-260, -160);
        card.CustomMinimumSize = new Vector2(520, 0);
        _endScreen.AddChild(card);

        _endBody = new VBoxContainer();
        card.AddChild(_endBody);
    }

    public void ShowEnd(GameView view, bool victory, int xpBanked, string factionId,
        Dictionary<int, int> killsByPlayer, int reactions)
    {
        _endScreen.Visible = true;
        foreach (var child in _endBody.GetChildren()) child.QueueFree();

        _endBody.AddChild(UiTheme.Text(victory ? "VICTORY" : "DEFEAT", 32,
            victory ? UiTheme.Good : UiTheme.Danger));
        _endBody.AddChild(UiTheme.Text(
            $"wave {view.Wave + 1} of {view.TotalWaves}   ·   {view.Lives} lives remaining", 13, UiTheme.InkDim));
        _endBody.AddChild(new HSeparator());

        foreach (var player in view.Players.OrderByDescending(p => killsByPlayer.GetValueOrDefault(p.Id)))
        {
            var row = new HBoxContainer();
            row.AddThemeConstantOverride("separation", 12);
            row.AddChild(new TextureRect
            {
                Texture = UiTheme.Icon($"faction_{player.FactionId}", UiTheme.Faction(player.FactionId)),
                Modulate = UiTheme.Faction(player.FactionId),
                CustomMinimumSize = new Vector2(20, 20),
                StretchMode = TextureRect.StretchModeEnum.KeepAspectCentered,
                ExpandMode = TextureRect.ExpandModeEnum.IgnoreSize,
            });
            var name = UiTheme.Text(player.Name, 14);
            name.CustomMinimumSize = new Vector2(120, 0);
            row.AddChild(name);
            row.AddChild(UiTheme.Text($"{killsByPlayer.GetValueOrDefault(player.Id)} kills", 13));
            row.AddChild(UiTheme.Text($"{player.MatchXp} xp", 13, UiTheme.InkDim));
            _endBody.AddChild(row);
        }

        _endBody.AddChild(new HSeparator());
        _endBody.AddChild(UiTheme.Text($"{reactions} reactions triggered", 12, UiTheme.InkDim));
        if (xpBanked > 0)
            _endBody.AddChild(UiTheme.Text($"+{xpBanked} {factionId} xp banked", 13, UiTheme.Good));

        var leave = new Button { Text = "RETURN TO LOBBY" };
        leave.Pressed += () => OnLeave?.Invoke();
        _endBody.AddChild(leave);
    }

    public void HideEnd() => _endScreen.Visible = false;

    // =====================================================================
    // Pause / system
    // =====================================================================

    private void BuildPause()
    {
        _pause = new Control { Visible = false };
        _pause.SetAnchorsAndOffsetsPreset(Control.LayoutPreset.FullRect);
        AddChild(_pause);

        var backdrop = new ColorRect
        {
            Color = new Color(0.02f, 0.03f, 0.05f, 0.85f),
            MouseFilter = Control.MouseFilterEnum.Stop,
        };
        backdrop.SetAnchorsAndOffsetsPreset(Control.LayoutPreset.FullRect);
        _pause.AddChild(backdrop);

        var columns = new HBoxContainer();
        columns.SetAnchorsPreset(Control.LayoutPreset.Center);
        columns.Position = new Vector2(-330, -170);
        columns.AddThemeConstantOverride("separation", 16);
        _pause.AddChild(columns);

        // Menu
        var menuCard = UiTheme.Card();
        menuCard.CustomMinimumSize = new Vector2(260, 0);
        var menu = new VBoxContainer();
        menu.AddThemeConstantOverride("separation", 8);
        menu.AddChild(UiTheme.Text("PAUSED", 22));
        menu.AddChild(UiTheme.Text("(multiplayer keeps running)", 11, UiTheme.InkDim));

        var resume = new Button { Text = "RESUME" };
        resume.Pressed += () => OnResume?.Invoke();
        menu.AddChild(resume);

        var save = new Button { Text = "SAVE (solo)" };
        save.Pressed += () => OnSave?.Invoke();
        menu.AddChild(save);

        var load = new Button { Text = "LOAD (solo)" };
        load.Pressed += () => OnLoad?.Invoke();
        menu.AddChild(load);

        var damageToggle = new CheckBox { Text = "damage numbers", ButtonPressed = true };
        damageToggle.Toggled += on => OnToggleDamageNumbers?.Invoke(on);
        menu.AddChild(damageToggle);

        var leave = new Button { Text = "LEAVE MATCH" };
        leave.Pressed += () => OnLeave?.Invoke();
        menu.AddChild(leave);

        menuCard.AddChild(menu);
        columns.AddChild(menuCard);

        var help = new KitButton("How to play", KitButton.Tone.Secondary);
        help.Pressed += ShowHowTo;
        menu.AddChild(help);
    }

    // =====================================================================
    // How to play — four cards, one idea each
    // =====================================================================

    /// <summary>Design's four cards: build, shoot, react, hold. One idea per
    /// card with a big step numeral, an icon cluster and two lines, because
    /// this is read once on first launch and then never again — anything that
    /// needs a second read has failed.</summary>
    private void BuildHowTo()
    {
        _howTo = new Control { Visible = false };
        _howTo.SetAnchorsAndOffsetsPreset(Control.LayoutPreset.FullRect);
        AddChild(_howTo);

        var backdrop = new ColorRect
        {
            Color = Tokens.Obsidian900,
            MouseFilter = Control.MouseFilterEnum.Stop,
        };
        backdrop.SetAnchorsAndOffsetsPreset(Control.LayoutPreset.FullRect);
        _howTo.AddChild(backdrop);
        _howTo.AddChild(new KitGrid());

        var frame = new VBoxContainer();
        frame.SetAnchorsAndOffsetsPreset(Control.LayoutPreset.FullRect);
        frame.AddThemeConstantOverride("separation", Tokens.Space6);
        _howTo.AddChild(frame);

        var title = Kit.Col(Tokens.Space2);
        title.Alignment = BoxContainer.AlignmentMode.Center;
        title.AddChild(Kit.Label("how to play", Tokens.TextAccent));
        var headline = Kit.Title("Build. Shoot. React.", Tokens.SizeDisplayLg);
        headline.HorizontalAlignment = HorizontalAlignment.Center;
        title.AddChild(headline);

        var titleMargin = new MarginContainer();
        titleMargin.AddThemeConstantOverride("margin_top", Tokens.Space10);
        titleMargin.AddChild(title);
        frame.AddChild(titleMargin);

        var cardsMargin = new MarginContainer { SizeFlagsVertical = Control.SizeFlags.ExpandFill };
        foreach (string side in new[] { "left", "right" })
            cardsMargin.AddThemeConstantOverride($"margin_{side}", Tokens.Space12);
        frame.AddChild(cardsMargin);

        var cards = Kit.Row(Tokens.Space7);
        cardsMargin.AddChild(cards);

        var steps = new (string Key, string Title, string Body, string[] Icons)[]
        {
            ("E", "Build on sockets",
                "Hold E at a socket ring and pick from the wheel. Green rings are on the ground, "
                + "blue ones on the decks — those are the only sockets that reach the air lane.",
                new[] { "tower_lance", "tower_nova", "tower_barricade" }),
            ("LMB", "Shoot what towers miss",
                "Your gun is a tower that moves. Towers answer coverage; you answer priority and "
                + "geometry — flank an Aegis, break a Warden's shield, catch a Mole surfacing.",
                new[] { "weapon_rifle", "ammo_ap", "status_mark" }),
            ("Q", "React together",
                "Statuses combine. Chill from a Singularity plus burn from Ember is Thermal Shock; "
                + "chill plus shock is Flash Freeze. Both halves are needed, so they are a team play.",
                new[] { "status_chill", "status_burn", "reaction_thermalshock" }),
            ("R", "Hold the core",
                "Every leak costs a core life, whatever leaked. Revive a downed teammate with R, and "
                + "start a wave early with F when you are ready — it pays.",
                new[] { "enemy_monolith", "scrap_alloy", "faction_forge" }),
        };

        for (int i = 0; i < steps.Length; i++)
        {
            var (key, stepTitle, body, icons) = steps[i];
            var panel = new KitPanel($"step {i + 1}", i == 0 ? Tokens.Brass500 : null);
            panel.SizeFlagsHorizontal = Control.SizeFlags.ExpandFill;
            panel.HeaderTrailing(Kit.Key(key));
            cards.AddChild(panel);

            var numeral = Kit.Numeral($"0{i + 1}", Tokens.SizeDisplayXl, Tokens.Obsidian400);
            panel.Body.AddChild(numeral);

            var well = Kit.Surface(Tokens.SurfaceInset, Tokens.BorderPanel, Tokens.ChamferSm, shadow: false);
            well.CustomMinimumSize = new Vector2(0, 150);
            var wellRow = Kit.Row(Tokens.Space6);
            wellRow.Alignment = BoxContainer.AlignmentMode.Center;
            foreach (string icon in icons)
                wellRow.AddChild(Kit.Icon(icon, Tokens.Brass400, 44));
            well.AddChild(wellRow);
            panel.Body.AddChild(well);

            panel.Body.AddChild(Kit.Title(stepTitle, Tokens.SizeDisplaySm));
            panel.Body.AddChild(Kit.Paragraph(body, Tokens.SizeBody, Tokens.TextSecondary));
        }

        var actions = Kit.Row(Tokens.Space4);
        actions.Alignment = BoxContainer.AlignmentMode.Center;
        var close = new KitButton("Back", KitButton.Tone.Primary, Tokens.ControlLg);
        close.Pressed += HideHowTo;
        actions.AddChild(close);

        var actionsMargin = new MarginContainer();
        actionsMargin.AddThemeConstantOverride("margin_bottom", Tokens.Space10);
        actionsMargin.AddChild(actions);
        frame.AddChild(actionsMargin);
    }

    public void ShowHowTo() => _howTo.Visible = true;
    public void HideHowTo() => _howTo.Visible = false;
    public bool HowToOpen => _howTo.Visible;

    public void ShowPause() => _pause.Visible = true;
    public void HidePause() => _pause.Visible = false;

    // =====================================================================
    // Connection / status overlay
    // =====================================================================

    private void BuildStatus()
    {
        _status = new Control { MouseFilter = Control.MouseFilterEnum.Ignore, Visible = false };
        _status.SetAnchorsAndOffsetsPreset(Control.LayoutPreset.FullRect);
        AddChild(_status);

        var card = UiTheme.Card();
        card.SetAnchorsPreset(Control.LayoutPreset.Center);
        card.Position = new Vector2(-230, -60);
        card.CustomMinimumSize = new Vector2(460, 0);
        _status.AddChild(card);

        var body = new VBoxContainer();
        _statusText = UiTheme.Text("", 18);
        _statusText.HorizontalAlignment = HorizontalAlignment.Center;
        body.AddChild(_statusText);

        _statusDetail = UiTheme.Text("", 12, UiTheme.InkDim);
        _statusDetail.HorizontalAlignment = HorizontalAlignment.Center;
        _statusDetail.AutowrapMode = TextServer.AutowrapMode.WordSmart;
        _statusDetail.CustomMinimumSize = new Vector2(430, 0);
        body.AddChild(_statusDetail);

        card.AddChild(body);
    }

    public void ShowStatus(string headline, string detail = "")
    {
        _status.Visible = true;
        _statusText.Text = headline;
        _statusDetail.Text = detail;
    }

    public void HideStatus() => _status.Visible = false;
}
