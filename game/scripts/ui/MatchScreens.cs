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

        var card = UiTheme.Card();
        card.SetAnchorsPreset(Control.LayoutPreset.CenterTop);
        card.Position = new Vector2(-230, 96);
        card.CustomMinimumSize = new Vector2(460, 0);
        _intermission.AddChild(card);

        _intermissionBody = new VBoxContainer();
        card.AddChild(_intermissionBody);
    }

    /// <summary>Wave composition is a pure function of (seed, map, wave, players),
    /// so the client can preview the next wave honestly without asking the server.</summary>
    public void ShowIntermission(GameView view, MapDef map, uint seed, int lastWaveLeaks)
    {
        _intermission.Visible = true;
        foreach (var child in _intermissionBody.GetChildren()) child.QueueFree();

        int next = view.Wave + 1;
        _intermissionBody.AddChild(UiTheme.Text(
            next < map.TotalWaves ? $"NEXT: WAVE {next + 1} of {map.TotalWaves}" : "FINAL WAVE CLEARED",
            18));

        if (view.Wave >= 0)
            _intermissionBody.AddChild(UiTheme.Text(
                lastWaveLeaks > 0
                    ? $"last wave leaked {lastWaveLeaks} — {view.Lives} lives left"
                    : $"last wave held clean — {view.Lives} lives",
                12, lastWaveLeaks > 0 ? UiTheme.Warn : UiTheme.Good));

        if (next < map.TotalWaves)
        {
            int players = Mathf.Max(1, view.Players.Count(p => p.Connected));
            var plan = WavePlan.PlanWave(seed, map, next, players);
            var composition = plan.GroupBy(e => e.DefId)
                .OrderByDescending(g => g.Count())
                .ToList();

            var row = new HBoxContainer();
            row.AddThemeConstantOverride("separation", 12);
            foreach (var group in composition)
            {
                var chip = new HBoxContainer();
                chip.AddThemeConstantOverride("separation", 4);
                chip.AddChild(new TextureRect
                {
                    Texture = UiTheme.Icon($"enemy_{group.Key}", UiTheme.Accent),
                    Modulate = UiTheme.Ink,
                    CustomMinimumSize = new Vector2(22, 22),
                    StretchMode = TextureRect.StretchModeEnum.KeepAspectCentered,
                ExpandMode = TextureRect.ExpandModeEnum.IgnoreSize,
                    TooltipText = group.Key,
                });
                chip.AddChild(UiTheme.Text($"{group.Key} ×{group.Count()}", 12));
                row.AddChild(chip);
            }
            _intermissionBody.AddChild(row);

            // Call out debuts — the "one to learn on" convention deserves a shout.
            var debuts = composition.Select(g => g.Key)
                .Where(id => !SeenBefore(seed, map, next, players, id)).ToList();
            if (debuts.Count > 0)
                _intermissionBody.AddChild(UiTheme.Text(
                    $"NEW: {string.Join(", ", debuts)} — {string.Join("; ", debuts.Select(Hint))}",
                    12, UiTheme.Warn));
        }

        _intermissionBody.AddChild(UiTheme.Text("[F] start now", 12, UiTheme.InkDim));
    }

    private static bool SeenBefore(uint seed, MapDef map, int wave, int players, string defId)
    {
        for (int i = 0; i < wave; i++)
            if (WavePlan.PlanWave(seed, map, i, players).Any(e => e.DefId == defId)) return true;
        return false;
    }

    private static string Hint(string defId) => defId switch
    {
        "skiff" => "flyer, ground towers can't reach it",
        "aegis" => "armored front — hit it from behind",
        "monolith" => "blocks tower sightlines",
        "warden" => "shield soaks damage and regrows",
        "mole" => "burrows; only hittable when surfaced",
        "cluster" => "splits into five on death",
        "mote" => "fast swarm, spreads wide",
        _ => "new contact",
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
