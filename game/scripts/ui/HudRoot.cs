using Godot;
using System.Collections.Generic;
using System.Linq;
using DeepField.Sim;
using DeepField.Sim.Content;

namespace DeepField.Game.Ui;

public enum CrosshairState { Neutral, Hit, Kill, Shielded, Armored, Burrowed }

/// <summary>In-match HUD: four clusters (economy, wave, vitals, loadout), a
/// crosshair that reports what your shots are actually doing, a teammate strip,
/// and a rolling event feed. Reads GameView only, so it is identical in solo,
/// host, and client modes.</summary>
public partial class HudRoot : CanvasLayer
{
    // Economy (top-left)
    private Label _credits = null!;
    private Label _lives = null!;
    private HBoxContainer _teamScrap = null!;
    private HBoxContainer _personalScrap = null!;

    // Wave (top-center)
    private Label _waveLine = null!;
    private Label _phaseLine = null!;

    // Vitals (bottom-left)
    private ProgressBar _hpBar = null!;
    private Label _hpText = null!;
    private Label _downedText = null!;

    // Loadout (bottom-right)
    private Label _weaponName = null!;
    private Label _buildSummary = null!;
    private Label _abilityText = null!;
    private TextureRect _abilityIcon = null!;
    private Control _abilityCooldown = null!;
    private float _abilityFraction;

    // Teammates (top-right)
    private VBoxContainer _teammates = null!;

    // Feed + crosshair
    private VBoxContainer _feed = null!;
    private readonly List<(Label Label, double Expiry)> _feedLines = new();
    private Control _crosshair = null!;
    private CrosshairState _crosshairState = CrosshairState.Neutral;
    private double _crosshairHold;
    private Label _capturePrompt = null!;
    private Label _hint = null!;

    private double _elapsed;

    public override void _Ready()
    {
        Layer = 1;

        var root = new Control();
        root.SetAnchorsPreset(Control.LayoutPreset.FullRect);
        root.MouseFilter = Control.MouseFilterEnum.Ignore;
        AddChild(root);

        BuildEconomy(root);
        BuildWaveCluster(root);
        BuildVitals(root);
        BuildLoadout(root);
        BuildTeammates(root);
        BuildFeed(root);
        BuildCrosshair(root);
    }

    // =====================================================================
    // Construction
    // =====================================================================

    private void BuildEconomy(Control root)
    {
        var card = UiTheme.Card();
        card.Position = new Vector2(16, 14);
        root.AddChild(card);

        var column = new VBoxContainer();
        card.AddChild(column);

        var top = new HBoxContainer();
        top.AddThemeConstantOverride("separation", 14);
        _credits = UiTheme.Text("0c", 18, UiTheme.Warn);
        _lives = UiTheme.Text("20 lives", 18);
        top.AddChild(_credits);
        top.AddChild(_lives);
        column.AddChild(top);

        column.AddChild(UiTheme.Text("team scrap", 10, UiTheme.InkDim));
        _teamScrap = new HBoxContainer();
        _teamScrap.AddThemeConstantOverride("separation", 10);
        column.AddChild(_teamScrap);

        column.AddChild(UiTheme.Text("yours", 10, UiTheme.InkDim));
        _personalScrap = new HBoxContainer();
        _personalScrap.AddThemeConstantOverride("separation", 10);
        column.AddChild(_personalScrap);
    }

    private void BuildWaveCluster(Control root)
    {
        var card = UiTheme.Card();
        card.SetAnchorsPreset(Control.LayoutPreset.CenterTop);
        card.Position = new Vector2(-110, 14);
        card.CustomMinimumSize = new Vector2(220, 0);
        root.AddChild(card);

        var column = new VBoxContainer();
        column.Alignment = BoxContainer.AlignmentMode.Center;
        card.AddChild(column);

        _waveLine = UiTheme.Text("", 17);
        _waveLine.HorizontalAlignment = HorizontalAlignment.Center;
        column.AddChild(_waveLine);

        _phaseLine = UiTheme.Text("", 12, UiTheme.InkDim);
        _phaseLine.HorizontalAlignment = HorizontalAlignment.Center;
        column.AddChild(_phaseLine);
    }

    private void BuildVitals(Control root)
    {
        var card = UiTheme.Card();
        card.SetAnchorsPreset(Control.LayoutPreset.BottomLeft);
        card.Position = new Vector2(16, -96);
        root.AddChild(card);

        var column = new VBoxContainer();
        card.AddChild(column);

        var row = new HBoxContainer();
        row.AddThemeConstantOverride("separation", 8);
        _hpBar = UiTheme.Meter(100, Balance.PlayerMaxHp, UiTheme.Good, new Vector2(180, 14));
        row.AddChild(_hpBar);
        _hpText = UiTheme.Text("100", 14);
        row.AddChild(_hpText);
        column.AddChild(row);

        _downedText = UiTheme.Text("", 13, UiTheme.Danger);
        column.AddChild(_downedText);
    }

    private void BuildLoadout(Control root)
    {
        var card = UiTheme.Card();
        card.SetAnchorsPreset(Control.LayoutPreset.BottomRight);
        card.Position = new Vector2(-260, -96);
        card.CustomMinimumSize = new Vector2(240, 0);
        root.AddChild(card);

        var column = new VBoxContainer();
        card.AddChild(column);

        _weaponName = UiTheme.Text("SIDEARM", 16);
        _weaponName.HorizontalAlignment = HorizontalAlignment.Right;
        column.AddChild(_weaponName);

        _buildSummary = UiTheme.Text("", 11, UiTheme.InkDim);
        _buildSummary.HorizontalAlignment = HorizontalAlignment.Right;
        column.AddChild(_buildSummary);

        var abilityRow = new HBoxContainer();
        abilityRow.Alignment = BoxContainer.AlignmentMode.End;
        abilityRow.AddThemeConstantOverride("separation", 8);

        _abilityText = UiTheme.Text("", 13);
        abilityRow.AddChild(_abilityText);

        _abilityCooldown = new Control { CustomMinimumSize = new Vector2(34, 34) };
        _abilityCooldown.Draw += DrawAbilityCooldown;
        abilityRow.AddChild(_abilityCooldown);

        _abilityIcon = new TextureRect
        {
            CustomMinimumSize = new Vector2(0, 0),
            StretchMode = TextureRect.StretchModeEnum.KeepAspectCentered,
        };
        abilityRow.AddChild(_abilityIcon);

        column.AddChild(abilityRow);
    }

    private void BuildTeammates(Control root)
    {
        _teammates = new VBoxContainer();
        _teammates.SetAnchorsPreset(Control.LayoutPreset.TopRight);
        _teammates.Position = new Vector2(-230, 14);
        _teammates.CustomMinimumSize = new Vector2(210, 0);
        root.AddChild(_teammates);
    }

    private void BuildFeed(Control root)
    {
        _feed = new VBoxContainer();
        _feed.SetAnchorsPreset(Control.LayoutPreset.CenterLeft);
        _feed.Position = new Vector2(16, -40);
        root.AddChild(_feed);
    }

    private void BuildCrosshair(Control root)
    {
        _crosshair = new Control
        {
            AnchorLeft = 0.5f, AnchorTop = 0.5f, AnchorRight = 0.5f, AnchorBottom = 0.5f,
            OffsetLeft = -24, OffsetTop = -24, OffsetRight = 24, OffsetBottom = 24,
            MouseFilter = Control.MouseFilterEnum.Ignore,
        };
        _crosshair.Draw += DrawCrosshair;
        root.AddChild(_crosshair);

        _capturePrompt = UiTheme.Text("CLICK TO CAPTURE MOUSE", 14, UiTheme.Warn);
        _capturePrompt.SetAnchorsPreset(Control.LayoutPreset.Center);
        _capturePrompt.Position = new Vector2(-100, 40);
        _capturePrompt.Visible = false;
        root.AddChild(_capturePrompt);

        _hint = UiTheme.Text("", 12, UiTheme.InkDim);
        _hint.SetAnchorsPreset(Control.LayoutPreset.CenterBottom);
        _hint.Position = new Vector2(-160, -130);
        _hint.CustomMinimumSize = new Vector2(320, 0);
        _hint.HorizontalAlignment = HorizontalAlignment.Center;
        root.AddChild(_hint);
    }

    // =====================================================================
    // Per-frame update
    // =====================================================================

    public void Refresh(GameView view, double delta, bool mouseCaptured, string hint)
    {
        _elapsed += delta;
        if (!view.Valid) return;

        // --- economy
        _credits.Text = $"{view.Money}c";
        _lives.Text = $"{view.Lives} lives";
        _lives.AddThemeColorOverride("font_color",
            view.Lives <= 5 ? UiTheme.Danger : UiTheme.Ink);
        // Low-lives alarm pulses so it registers peripherally.
        if (view.Lives <= 5)
            _lives.Modulate = new Color(1, 1, 1, 0.55f + 0.45f * Mathf.Abs(Mathf.Sin((float)_elapsed * 4f)));
        else
            _lives.Modulate = Colors.White;

        RebuildScrap(_teamScrap, type => view.TeamScrapOf(type));
        RebuildScrap(_personalScrap, type => view.PersonalScrapOf(type));

        // --- wave
        _waveLine.Text = view.Phase switch
        {
            MatchPhase.Victory => "VICTORY",
            MatchPhase.Defeat => "DEFEAT",
            _ => view.Wave < 0 ? "STANDBY" : $"WAVE {view.Wave + 1} / {view.TotalWaves}",
        };
        _phaseLine.Text = view.Phase switch
        {
            MatchPhase.Intermission => $"next in {Mathf.Max(0, view.PhaseTimer):0.0}s   ·   [F] start now",
            MatchPhase.Wave => $"{view.EnemiesRemaining} remaining",
            _ => "",
        };

        // --- vitals
        var local = view.Local;
        if (local is not null)
        {
            _hpBar.Value = local.Hp;
            _hpBar.AddThemeStyleboxOverride("fill", new StyleBoxFlat
            {
                BgColor = local.Hp > 60 ? UiTheme.Good : local.Hp > 25 ? UiTheme.Warn : UiTheme.Danger,
            });
            _hpText.Text = $"{local.Hp:0}";
            _downedText.Text = local.Downed ? "DOWNED — a teammate can revive you" : "";

            // --- loadout
            _weaponName.Text = local.WeaponId.ToUpperInvariant();
            _abilityFraction = local.AbilityCooldown <= 0f ? 0f
                : Mathf.Clamp(local.AbilityCooldown / AbilityCooldownFor(local.FactionId), 0f, 1f);
            _abilityText.Text = local.AbilityCooldown <= 0f
                ? "[Q] READY"
                : $"{local.AbilityCooldown:0.0}s";
            _abilityText.AddThemeColorOverride("font_color",
                local.AbilityCooldown <= 0f ? UiTheme.Good : UiTheme.InkDim);
            _abilityIcon.Texture = UiTheme.Icon($"faction_{local.FactionId}", UiTheme.Faction(local.FactionId));
            _abilityIcon.CustomMinimumSize = new Vector2(26, 26);
            _abilityCooldown.QueueRedraw();
        }

        RebuildTeammates(view);

        // --- feed
        for (int i = _feedLines.Count - 1; i >= 0; i--)
        {
            if (_elapsed < _feedLines[i].Expiry) continue;
            _feedLines[i].Label.QueueFree();
            _feedLines.RemoveAt(i);
        }

        // --- crosshair
        _crosshairHold = Mathf.Max(0, _crosshairHold - delta);
        if (_crosshairHold <= 0 && _crosshairState is CrosshairState.Hit or CrosshairState.Kill)
            _crosshairState = CrosshairState.Neutral;
        _crosshair.QueueRedraw();

        _capturePrompt.Visible = !mouseCaptured;
        _hint.Text = hint;
    }

    private static float AbilityCooldownFor(string factionId) =>
        Factions.All.TryGetValue(factionId, out var faction) ? faction.CooldownSeconds : 30f;

    private void RebuildScrap(HBoxContainer row, System.Func<ScrapType, int> amount)
    {
        foreach (var child in row.GetChildren()) child.QueueFree();
        foreach (ScrapType type in System.Enum.GetValues<ScrapType>())
        {
            int value = amount(type);
            if (value <= 0) continue;
            row.AddChild(UiTheme.CountChip($"scrap_{type.ToString().ToLowerInvariant()}",
                value, UiTheme.Scrap(type)));
        }
        if (row.GetChildCount() == 0) row.AddChild(UiTheme.Text("—", 12, UiTheme.Disabled));
    }

    private void RebuildTeammates(GameView view)
    {
        foreach (var child in _teammates.GetChildren()) child.QueueFree();

        var local = view.Local;
        foreach (var player in view.Players.Where(p => p.Id != view.LocalPlayerId))
        {
            var card = UiTheme.Card(UiTheme.Panel with { A = 0.7f });
            var row = new HBoxContainer();
            row.AddThemeConstantOverride("separation", 6);

            row.AddChild(new TextureRect
            {
                Texture = UiTheme.Icon($"faction_{player.FactionId}", UiTheme.Faction(player.FactionId)),
                CustomMinimumSize = new Vector2(18, 18),
                StretchMode = TextureRect.StretchModeEnum.KeepAspectCentered,
            });

            var name = UiTheme.Text(player.Name, 12,
                player.Connected ? UiTheme.Ink : UiTheme.Disabled);
            name.CustomMinimumSize = new Vector2(70, 0);
            row.AddChild(name);

            if (!player.Connected)
            {
                row.AddChild(UiTheme.Text("disconnected", 11, UiTheme.Disabled));
            }
            else if (player.Downed)
            {
                float distance = local is null ? 0 : local.Pos.DistanceTo(player.Pos);
                row.AddChild(UiTheme.Text($"DOWN {distance:0}m", 11, UiTheme.Danger));
            }
            else
            {
                row.AddChild(UiTheme.Meter(player.Hp, Balance.PlayerMaxHp,
                    player.Hp > 40 ? UiTheme.Good : UiTheme.Warn, new Vector2(60, 8)));
            }

            card.AddChild(row);
            _teammates.AddChild(card);
        }
    }

    // =====================================================================
    // Feed + crosshair
    // =====================================================================

    public void Post(string message, Color? color = null)
    {
        var label = UiTheme.Text(message, 14, color ?? UiTheme.Warn);
        _feed.AddChild(label);
        _feedLines.Add((label, _elapsed + 3.5));

        // Keep the feed to three lines — it's peripheral, not a log.
        while (_feedLines.Count > 3)
        {
            _feedLines[0].Label.QueueFree();
            _feedLines.RemoveAt(0);
        }
    }

    public void SetCrosshair(CrosshairState state, double hold = 0.15)
    {
        // Aim-context states (shielded/armored/burrowed) are continuous; hit and
        // kill are momentary and outrank them while their hold lasts.
        if (_crosshairHold > 0 && state is not (CrosshairState.Hit or CrosshairState.Kill)) return;
        _crosshairState = state;
        _crosshairHold = state is CrosshairState.Hit or CrosshairState.Kill ? hold : 0;
    }

    private void DrawCrosshair()
    {
        var center = _crosshair.Size * 0.5f;
        var (color, gap, length, thickness) = _crosshairState switch
        {
            CrosshairState.Hit => (new Color(1f, 0.45f, 0.25f), 3f, 8f, 2.5f),
            CrosshairState.Kill => (new Color(1f, 0.25f, 0.2f), 6f, 10f, 3f),
            CrosshairState.Shielded => (new Color(0.45f, 0.8f, 1f), 5f, 7f, 2f),
            CrosshairState.Armored => (new Color(0.9f, 0.75f, 0.35f), 5f, 7f, 2f),
            CrosshairState.Burrowed => (UiTheme.Disabled, 7f, 5f, 1.5f),
            _ => (new Color(1, 1, 1, 0.8f), 4f, 6f, 1.5f),
        };

        foreach (var dir in new[] { Vector2.Up, Vector2.Down, Vector2.Left, Vector2.Right })
            _crosshair.DrawLine(center + dir * gap, center + dir * (gap + length), color, thickness);

        if (_crosshairState == CrosshairState.Shielded)
            _crosshair.DrawArc(center, gap + length + 3f, 0, Mathf.Tau, 24, color, 1.5f);
        if (_crosshairState == CrosshairState.Burrowed)
            _crosshair.DrawLine(center + new Vector2(-8, -8), center + new Vector2(8, 8), color, 1.5f);
    }

    private void DrawAbilityCooldown()
    {
        var center = _abilityCooldown.Size * 0.5f;
        float radius = 15f;
        _abilityCooldown.DrawArc(center, radius, 0, Mathf.Tau, 28, new Color(0, 0, 0, 0.5f), 3f);
        if (_abilityFraction > 0f)
        {
            // Sweep drains clockwise from 12 o'clock as the cooldown expires.
            _abilityCooldown.DrawArc(center, radius, -Mathf.Pi / 2f,
                -Mathf.Pi / 2f + Mathf.Tau * _abilityFraction, 28, UiTheme.Warn, 3f);
        }
        else
        {
            _abilityCooldown.DrawArc(center, radius, 0, Mathf.Tau, 28, UiTheme.Good, 3f);
        }
    }
}
