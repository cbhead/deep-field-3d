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
        BuildStatus();
    }

    // =====================================================================
    // Intermission
    // =====================================================================

    private void BuildIntermission()
    {
        _intermission = new Control { MouseFilter = Control.MouseFilterEnum.Ignore, Visible = false };
        _intermission.SetAnchorsPreset(Control.LayoutPreset.FullRect);
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
                    CustomMinimumSize = new Vector2(22, 22),
                    StretchMode = TextureRect.StretchModeEnum.KeepAspectCentered,
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
        _endScreen.SetAnchorsPreset(Control.LayoutPreset.FullRect);
        AddChild(_endScreen);

        var backdrop = new ColorRect { Color = new Color(0.02f, 0.03f, 0.05f, 0.75f) };
        backdrop.SetAnchorsPreset(Control.LayoutPreset.FullRect);
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
                CustomMinimumSize = new Vector2(20, 20),
                StretchMode = TextureRect.StretchModeEnum.KeepAspectCentered,
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
        _pause.SetAnchorsPreset(Control.LayoutPreset.FullRect);
        AddChild(_pause);

        var backdrop = new ColorRect
        {
            Color = new Color(0.02f, 0.03f, 0.05f, 0.85f),
            MouseFilter = Control.MouseFilterEnum.Stop,
        };
        backdrop.SetAnchorsPreset(Control.LayoutPreset.FullRect);
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

        // How to play
        var helpCard = UiTheme.Card();
        helpCard.CustomMinimumSize = new Vector2(380, 0);
        var help = new VBoxContainer();
        help.AddChild(UiTheme.Text("HOW TO PLAY", 16));
        foreach (var (title, body) in new[]
        {
            ("BUILD", "Hold E at a green socket for the build wheel. Wall sockets see the air lane; "
                + "path plates take traps. Hold U on a tower to level a path — level 4 also costs scrap."),
            ("SHOOT", "Your gun is the flex layer: towers cover, you prioritize. Flank an Aegis, "
                + "break a Warden's shield, catch a Mole in its surface window."),
            ("COMBINE", "Statuses react. Chill a target then burn it for Thermal Shock; chill then "
                + "shock for Flash Freeze. Different players' kits are meant to overlap."),
            ("SPEND", "Kills drop scrap by enemy type. Team scrap buys tower breakpoints; your own "
                + "scrap builds your gun at the armory (Tab)."),
        })
        {
            help.AddChild(UiTheme.Text(title, 13, UiTheme.Accent));
            var text = UiTheme.Text(body, 11, UiTheme.InkDim);
            text.AutowrapMode = TextServer.AutowrapMode.WordSmart;
            text.CustomMinimumSize = new Vector2(350, 0);
            help.AddChild(text);
        }
        helpCard.AddChild(help);
        columns.AddChild(helpCard);
    }

    public void ShowPause() => _pause.Visible = true;
    public void HidePause() => _pause.Visible = false;

    // =====================================================================
    // Connection / status overlay
    // =====================================================================

    private void BuildStatus()
    {
        _status = new Control { MouseFilter = Control.MouseFilterEnum.Ignore, Visible = false };
        _status.SetAnchorsPreset(Control.LayoutPreset.FullRect);
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
