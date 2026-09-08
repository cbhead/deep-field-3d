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
    private Label _phaseLabel = null!;
    private Label _phaseLine = null!;
    private Label _aliveLine = null!;
    private KitPips _wavePips = null!;
    private Label _threatLine = null!;
    /// <summary>The profile's deepest endless run on this map; the HUD shows
    /// it beside the threat so the number to beat is always in view.</summary>
    public int BestEndlessWave;
    private KitDiamond _coreDiamond = null!;

    // Vitals (bottom-left)
    private KitBar _hpBar = null!;
    private Label _hpText = null!;
    private Label _downedText = null!;
    private PanelContainer _abilitySlot = null!;
    private KitDiamond _factionChip = null!;

    // Loadout (bottom-right)
    private Label _weaponName = null!;
    private Label _buildSummary = null!;
    private Label _abilityText = null!;
    private Label _ammoLabel = null!;
    private TextureRect _abilityIcon = null!;
    private TextureRect _weaponIcon = null!;
    private TextureRect _ammoIcon = null!;
    private VBoxContainer _weaponSlots = null!;
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

    // Downed presentation + off-screen breach direction.
    private Control _vignette = null!;
    private Label _downedTimer = null!;
    private Control _breachArrow = null!;
    private float _breachAngle;
    private double _breachHold;
    private float _downedFraction;
    private bool _isDowned;

    private double _elapsed;

    public override void _Ready()
    {
        Layer = 1;

        var root = new Control();
        root.SetAnchorsAndOffsetsPreset(Control.LayoutPreset.FullRect);
        root.MouseFilter = Control.MouseFilterEnum.Ignore;
        AddChild(root);

        BuildEconomy(root);
        BuildWaveCluster(root);
        BuildVitals(root);
        BuildLoadout(root);
        BuildTeammates(root);
        BuildFeed(root);
        BuildCrosshair(root);
        BuildDownedOverlay(root);
        BuildBreachArrow(root);
    }

    private void BuildDownedOverlay(Control root)
    {
        _vignette = new Control { MouseFilter = Control.MouseFilterEnum.Ignore, Visible = false };
        _vignette.SetAnchorsAndOffsetsPreset(Control.LayoutPreset.FullRect);
        _vignette.Draw += DrawVignette;
        root.AddChild(_vignette);

        _downedTimer = UiTheme.Text("", 20, UiTheme.Danger);
        _downedTimer.SetAnchorsPreset(Control.LayoutPreset.Center);
        _downedTimer.Position = new Vector2(-140, 70);
        _downedTimer.CustomMinimumSize = new Vector2(280, 0);
        _downedTimer.HorizontalAlignment = HorizontalAlignment.Center;
        _downedTimer.Visible = false;
        root.AddChild(_downedTimer);
    }

    private void BuildBreachArrow(Control root)
    {
        _breachArrow = new Control
        {
            AnchorLeft = 0.5f, AnchorTop = 0.5f, AnchorRight = 0.5f, AnchorBottom = 0.5f,
            OffsetLeft = -120, OffsetTop = -120, OffsetRight = 120, OffsetBottom = 120,
            MouseFilter = Control.MouseFilterEnum.Ignore,
            Visible = false,
        };
        _breachArrow.Draw += DrawBreachArrow;
        root.AddChild(_breachArrow);
    }

    /// <summary>Points at where the core was hit when it happened off-screen —
    /// a leak you didn't see is the one you most need to locate.</summary>
    public void FlagBreach(Vector3 worldPos, Camera3D? camera)
    {
        if (camera is null) return;
        if (!camera.IsPositionBehind(worldPos))
        {
            var onScreen = camera.UnprojectPosition(worldPos);
            var viewport = _breachArrow.GetViewportRect().Size;
            if (onScreen.X > 0 && onScreen.X < viewport.X && onScreen.Y > 0 && onScreen.Y < viewport.Y)
                return;   // visible: the player can see it happen
        }

        var toTarget = worldPos - camera.GlobalPosition;
        var local = camera.GlobalTransform.Basis.Inverse() * toTarget;
        _breachAngle = Mathf.Atan2(local.X, -local.Z);
        _breachHold = 2.5;
    }

    private void DrawVignette()
    {
        var size = _vignette.Size;
        float pulse = 0.35f + 0.15f * Mathf.Sin((float)_elapsed * 3f);
        // Cheap vignette: stacked translucent border bands.
        for (int i = 0; i < 10; i++)
        {
            float inset = i * 16f;
            var color = new Color(0.5f, 0.03f, 0.05f, pulse * (1f - i / 10f) * 0.25f);
            _vignette.DrawRect(new Rect2(inset, inset, size.X - inset * 2, size.Y - inset * 2),
                color, filled: false, width: 16f);
        }
    }

    private void DrawBreachArrow()
    {
        var center = _breachArrow.Size * 0.5f;
        var dir = new Vector2(Mathf.Sin(_breachAngle), -Mathf.Cos(_breachAngle));
        var tip = center + dir * 110f;
        var left = center + dir.Rotated(2.5f) * 26f;
        var right = center + dir.Rotated(-2.5f) * 26f;

        float alpha = Mathf.Clamp((float)_breachHold, 0f, 1f);
        var color = UiTheme.Danger with { A = alpha };
        _breachArrow.DrawColoredPolygon(new[] { tip, tip + (left - center) * 0.35f, tip + (right - center) * 0.35f }, color);
        _breachArrow.DrawLine(center + dir * 70f, tip, color, 3f);
    }

    // =====================================================================
    // Construction
    // =====================================================================

    /// <summary>Top-left: the wallet. A brass diamond and one big tabular
    /// numeral, then the scrap types as icon + count, dimmed at zero so an
    /// empty type reads as "none" rather than as a live resource.</summary>
    private void BuildEconomy(Control root)
    {
        var card = Kit.Glass();
        card.Position = new Vector2(Tokens.HudEdge, Tokens.HudEdge);
        root.AddChild(card);

        var column = Kit.Col(Tokens.GapInline);
        card.AddChild(column);

        var top = Kit.Row(10);
        top.AddChild(new KitDiamond(12f, Tokens.ResGold));
        _credits = Kit.Numeral("0", 28, Tokens.TextAccent);
        top.AddChild(_credits);
        top.AddChild(Kit.Label("credits"));
        column.AddChild(top);

        column.AddChild(Kit.Rule());

        _teamScrap = Kit.Row(14);
        column.AddChild(_teamScrap);

        column.AddChild(Kit.Label("yours", Tokens.TextDisabled));
        _personalScrap = Kit.Row(14);
        column.AddChild(_personalScrap);
    }

    /// <summary>Top-centre: wave, phase clock, a pip per wave in the campaign,
    /// enemies alive, and the core. Design puts three diamonds here for lives;
    /// this map carries twenty, so it is a diamond plus a numeral that flips to
    /// threat red under the alarm threshold.</summary>
    private void BuildWaveCluster(Control root)
    {
        var card = Kit.Glass();
        card.SetAnchorsPreset(Control.LayoutPreset.CenterTop);
        card.Position = new Vector2(-230, Tokens.HudEdge);
        card.CustomMinimumSize = new Vector2(460, 0);
        root.AddChild(card);

        var row = Kit.Row(Tokens.Space7);
        row.Alignment = BoxContainer.AlignmentMode.Center;
        card.AddChild(row);

        var wave = Kit.Col(2);
        wave.Alignment = BoxContainer.AlignmentMode.Center;
        wave.AddChild(Kit.Label("wave"));
        _waveLine = Kit.Numeral("--", Tokens.SizeStat, Tokens.WaveIdle);
        wave.AddChild(_waveLine);
        row.AddChild(wave);

        row.AddChild(VerticalRule());

        var phase = Kit.Col(2);
        _phaseLabel = Kit.Label("standby");
        phase.AddChild(_phaseLabel);
        _phaseLine = Kit.Numeral("", Tokens.SizeStatSm, Tokens.TextSecondary);
        phase.AddChild(_phaseLine);
        _wavePips = new KitPips();
        _wavePips.Breakpoints = System.Array.Empty<int>();
        phase.AddChild(_wavePips);
        // Endless swaps the pip strip for an open counter: threat and best.
        _threatLine = Kit.Numeral("", Tokens.SizeStatSm, Tokens.WaveBoss);
        _threatLine.Visible = false;
        phase.AddChild(_threatLine);
        row.AddChild(phase);

        row.AddChild(VerticalRule());

        var alive = Kit.Col(2);
        alive.Alignment = BoxContainer.AlignmentMode.Center;
        alive.AddChild(Kit.Label("alive"));
        _aliveLine = Kit.Numeral("0", Tokens.SizeStatSm, Tokens.TextPrimary);
        alive.AddChild(_aliveLine);
        row.AddChild(alive);

        var core = Kit.Col(2);
        core.Alignment = BoxContainer.AlignmentMode.Center;
        core.AddChild(Kit.Label("core"));
        var coreRow = Kit.Row(4);
        coreRow.Alignment = BoxContainer.AlignmentMode.Center;
        _coreDiamond = new KitDiamond(14f, Tokens.Lives);
        coreRow.AddChild(_coreDiamond);
        _lives = Kit.Numeral("20", Tokens.SizeStatSm, Tokens.Lives);
        coreRow.AddChild(_lives);
        core.AddChild(coreRow);
        row.AddChild(core);
    }

    private static Control VerticalRule() => new ColorRect
    {
        Color = Tokens.BorderPanel,
        CustomMinimumSize = new Vector2(1, 36),
    };

    /// <summary>Bottom-left: the faction ability as a 72px slot that glows when
    /// it is off cooldown, beside hp. The ability is the thing that makes one
    /// player different from another, so it gets the largest single element on
    /// the bar.</summary>
    private void BuildVitals(Control root)
    {
        var card = Kit.Glass();
        card.SetAnchorsPreset(Control.LayoutPreset.BottomLeft);
        card.Position = new Vector2(Tokens.HudEdge, -110);
        card.CustomMinimumSize = new Vector2(420, 0);
        root.AddChild(card);

        var row = Kit.Row(14);
        card.AddChild(row);

        _abilitySlot = Kit.SlotBox(size: Tokens.SlotLg);
        row.AddChild(_abilitySlot);

        _abilityIcon = Kit.SlotIcon(null, Tokens.TextPrimary, 36);
        _abilitySlot.AddChild(_abilityIcon);

        // The sweep and the "Q" cap sit over the icon.
        _abilityCooldown = new Control { MouseFilter = Control.MouseFilterEnum.Ignore };
        _abilityCooldown.SetAnchorsAndOffsetsPreset(Control.LayoutPreset.FullRect);
        _abilityCooldown.Draw += DrawAbilityCooldown;
        _abilitySlot.AddChild(_abilityCooldown);

        var column = Kit.Col(6);
        column.SizeFlagsHorizontal = Control.SizeFlags.ExpandFill;
        row.AddChild(column);

        var identity = Kit.Row(6);
        _factionChip = new KitDiamond(12f, Tokens.Brass400);
        identity.AddChild(_factionChip);
        _abilityText = Kit.Label("", Tokens.TextSecondary);
        identity.AddChild(_abilityText);
        column.AddChild(identity);

        var hpRow = Kit.Row();
        hpRow.AddChild(Kit.Label("hp"));
        _hpText = Kit.Numeral("100/100", Tokens.SizeCaption, Tokens.TextSecondary);
        _hpText.SizeFlagsHorizontal = Control.SizeFlags.ExpandFill;
        _hpText.HorizontalAlignment = HorizontalAlignment.Right;
        hpRow.AddChild(_hpText);
        column.AddChild(hpRow);

        _hpBar = Kit.Bar(1f, Tokens.BarHp, 14f);
        column.AddChild(_hpBar);

        _downedText = Kit.Body("", Tokens.SizeCaption, UiTheme.Danger);
        column.AddChild(_downedText);
    }

    /// <summary>Bottom-right: what you are holding. Design's frame carries a
    /// magazine and reserve count; the sim has neither — weapons fire on a
    /// cooldown with no ammo pool — so this shows the weapon, its ammo type and
    /// its build instead of inventing numbers. The mag readout arrives with the
    /// ammo-quantity system.</summary>
    private void BuildLoadout(Control root)
    {
        var card = Kit.Glass();
        card.SetAnchorsPreset(Control.LayoutPreset.BottomRight);
        card.Position = new Vector2(-436, -110);
        card.CustomMinimumSize = new Vector2(420, 0);
        root.AddChild(card);

        var row = Kit.Row(14);
        card.AddChild(row);

        var column = Kit.Col(6);
        column.SizeFlagsHorizontal = Control.SizeFlags.ExpandFill;
        column.Alignment = BoxContainer.AlignmentMode.End;
        row.AddChild(column);

        var nameRow = Kit.Row(8);
        nameRow.Alignment = BoxContainer.AlignmentMode.End;
        _weaponName = Kit.Title("SIDEARM", Tokens.SizeDisplaySm);
        nameRow.AddChild(_weaponName);
        _weaponIcon = Kit.SlotIcon(null, Tokens.TextPrimary, 28);
        nameRow.AddChild(_weaponIcon);
        column.AddChild(nameRow);

        var ammoRow = Kit.Row(8);
        ammoRow.Alignment = BoxContainer.AlignmentMode.End;
        _ammoIcon = Kit.SlotIcon(null, Tokens.TextPrimary, 18);
        ammoRow.AddChild(_ammoIcon);
        _ammoLabel = Kit.Label("standard", Tokens.TextSecondary);
        ammoRow.AddChild(_ammoLabel);
        column.AddChild(ammoRow);

        _buildSummary = Kit.Body("", Tokens.SizeCaption, Tokens.TextMuted);
        _buildSummary.HorizontalAlignment = HorizontalAlignment.Right;
        column.AddChild(_buildSummary);

        _weaponSlots = Kit.Col(6);
        row.AddChild(_weaponSlots);
    }

    /// <summary>The weapons you own as numbered slots, the held one selected.
    /// Rebuilt only when the set or the selection changes — this runs every
    /// frame otherwise.</summary>
    private string _slotSignature = "";

    private void RebuildWeaponSlots(PlayerView local)
    {
        string signature = string.Join(",", local.OwnedWeapons) + "|" + local.WeaponId;
        if (signature == _slotSignature) return;
        _slotSignature = signature;

        foreach (var child in _weaponSlots.GetChildren()) child.QueueFree();

        foreach (string weaponId in local.OwnedWeapons)
        {
            bool held = weaponId == local.WeaponId;
            var slot = Kit.SlotBox(held, 52);
            slot.AddChild(Kit.SlotIcon(UiTheme.Icon($"weapon_{weaponId}", Tokens.TextSecondary),
                held ? Tokens.Arcane400 : Tokens.TextDisabled, 26));
            _weaponSlots.AddChild(slot);
        }
    }

    private void BuildTeammates(Control root)
    {
        _teammates = Kit.Col(Tokens.GapInline);
        _teammates.Position = new Vector2(Tokens.HudEdge, 140);
        _teammates.CustomMinimumSize = new Vector2(230, 0);
        root.AddChild(_teammates);
    }

    /// <summary>Toast feed, top-right under the teammate strip in design's
    /// frame — kept right so it never fights the economy cluster.</summary>
    private void BuildFeed(Control root)
    {
        _feed = Kit.Col(6);
        _feed.SetAnchorsPreset(Control.LayoutPreset.TopRight);
        _feed.Position = new Vector2(-360, 140);
        _feed.CustomMinimumSize = new Vector2(344, 0);
        _feed.Alignment = BoxContainer.AlignmentMode.End;
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
        _credits.Text = view.Money.ToString();

        RebuildScrap(_teamScrap, type => view.TeamScrapOf(type));
        RebuildScrap(_personalScrap, type => view.PersonalScrapOf(type));

        // --- wave
        bool active = view.Phase == MatchPhase.Wave;
        _waveLine.Text = view.Wave < 0 ? "--" : $"{view.Wave + 1:00}";
        _waveLine.AddThemeColorOverride("font_color", active ? Tokens.WaveActive : Tokens.WaveIdle);

        _phaseLabel.Text = view.Phase switch
        {
            MatchPhase.Victory => "VICTORY",
            MatchPhase.Defeat => "DEFEAT",
            MatchPhase.Wave => "ACTIVE",
            _ => "STANDBY",
        };
        _phaseLine.Text = view.Phase switch
        {
            MatchPhase.Intermission => $"{Mathf.Max(0, view.PhaseTimer):0.0}s  ·  [F]",
            MatchPhase.Wave => $"{view.EnemiesRemaining} left",
            _ => "",
        };
        _wavePips.Set(Mathf.Max(0, view.Wave + 1), Mathf.Max(1, view.TotalWaves));
        _wavePips.Visible = !view.Endless;
        _threatLine.Visible = view.Endless;
        if (view.Endless)
        {
            _threatLine.Text = $"threat ×{view.Threat:0.0}" + (BestEndlessWave > 0 ? $"  ·  best {BestEndlessWave}" : "");
            if (active) _waveLine.AddThemeColorOverride("font_color", Tokens.WaveBoss);
        }
        _aliveLine.Text = view.EnemiesRemaining.ToString();

        // Lives: a single tabular numeral beside the core diamond, flipping to
        // threat red and pulsing under the alarm threshold.
        _lives.Text = view.Lives.ToString();
        bool alarm = view.Lives <= 5;
        _lives.AddThemeColorOverride("font_color", alarm ? UiTheme.Danger : Tokens.Lives);
        _coreDiamond.Color = alarm ? UiTheme.Danger : Tokens.Lives;
        _coreDiamond.QueueRedraw();
        float pulse = alarm
            ? 0.55f + 0.45f * Mathf.Abs(Mathf.Sin((float)_elapsed * 4f))
            : 1f;
        _lives.Modulate = new Color(1, 1, 1, pulse);
        _coreDiamond.Modulate = new Color(1, 1, 1, pulse);

        // --- vitals
        var local = view.Local;
        if (local is not null)
        {
            // Design's hp rule is a single threshold: green until 30%, then
            // threat red. No amber middle band.
            float hpFraction = local.Hp / Balance.PlayerMaxHp;
            _hpBar.Set(hpFraction, Kit.HpColor(hpFraction));
            _hpText.Text = $"{local.Hp:0}/{Balance.PlayerMaxHp:0}";
            _downedText.Text = local.Downed ? "DOWNED — a teammate can revive you" : "";

            _isDowned = local.Downed;
            _vignette.Visible = _isDowned;
            _downedTimer.Visible = _isDowned;
            if (_isDowned)
            {
                _vignette.QueueRedraw();
                _downedTimer.Text = _downedFraction > 0
                    ? $"BLEEDING OUT — {_downedFraction:0}s"
                    : "BLEEDING OUT";
            }

            // --- ability (bottom-left, beside hp)
            var factionColor = UiTheme.Faction(local.FactionId);
            bool ready = local.AbilityCooldown <= 0f;
            _abilityFraction = ready ? 0f
                : Mathf.Clamp(local.AbilityCooldown / AbilityCooldownFor(local.FactionId), 0f, 1f);

            string abilityName = Factions.All.TryGetValue(local.FactionId, out var faction)
                ? faction.AbilityId : "ability";
            _abilityText.Text = (ready ? $"{local.FactionId} · {abilityName}"
                : $"{abilityName} · {local.AbilityCooldown:0.0}s").ToUpperInvariant();
            _abilityText.AddThemeColorOverride("font_color",
                ready ? Tokens.TextSecondary : Tokens.TextMuted);

            _factionChip.Color = factionColor;
            _factionChip.QueueRedraw();

            _abilityIcon.Texture = UiTheme.Icon($"ability_{abilityName.ToLowerInvariant().Replace(" ", "")}", factionColor);
            _abilityIcon.Modulate = ready ? factionColor : Tokens.TextDisabled;
            // The slot itself carries the ready state — brass edge and glow off
            // cooldown, plain stroke while it recharges.
            _abilitySlot.AddThemeStyleboxOverride("panel", new ChamferBox
            {
                Fill = Tokens.SurfaceSlot,
                Stroke = ready ? Tokens.Brass500 : Tokens.BorderStrong,
                Glow = ready ? Tokens.GlowBrass : new Color(0, 0, 0, 0),
                Chamfer = Tokens.ChamferSm,
                DropShadow = false,
            });
            _abilityCooldown.QueueRedraw();

            // --- loadout
            _weaponName.Text = local.WeaponId.ToUpperInvariant();
            _weaponIcon.Texture = UiTheme.Icon($"weapon_{local.WeaponId}", Tokens.TextPrimary);
            _weaponIcon.Modulate = Tokens.TextPrimary;

            string ammo = local.AmmoFor(local.WeaponId);
            _ammoLabel.Text = ammo.ToUpperInvariant();
            _ammoIcon.Texture = UiTheme.Icon($"ammo_{ammo}", Tokens.TextSecondary);
            _ammoIcon.Modulate = Tokens.TextSecondary;

            RebuildWeaponSlots(local);
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

        if (_breachHold > 0)
        {
            _breachHold -= delta;
            _breachArrow.Visible = _breachHold > 0;
            _breachArrow.QueueRedraw();
        }
    }

    /// <summary>Bleedout seconds remaining, for the downed overlay (authoritative
    /// modes only; clients see the state without the exact clock).</summary>
    public void SetBleedout(float seconds) => _downedFraction = seconds;

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
                Modulate = UiTheme.Faction(player.FactionId),
                CustomMinimumSize = new Vector2(18, 18),
                StretchMode = TextureRect.StretchModeEnum.KeepAspectCentered,
                ExpandMode = TextureRect.ExpandModeEnum.IgnoreSize,
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
