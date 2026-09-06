using Godot;
using System.Collections.Generic;
using System.Linq;
using DeepField.Sim.Content;

namespace DeepField.Game.Ui;

/// <summary>Front door: name, faction (exclusive per lobby, levels persist),
/// map, and how to play — solo, host over Tailscale, or join an address.</summary>
public partial class LobbyScreen : Control
{
    public System.Action<string>? OnSolo;      // faction
    public System.Action<string>? OnHost;      // faction
    public System.Action<string, string>? OnJoin;   // address, faction

    private Profile _profile = null!;
    private string _faction = "ember";
    private MapDef _map = Maps.Foundry;
    private LineEdit _nameEdit = null!;
    private LineEdit _addressEdit = null!;
    private VBoxContainer _factionColumn = null!;
    private VBoxContainer _mapColumn = null!;
    private Label _status = null!;

    public string PlayerName => _nameEdit.Text.Length > 0 ? _nameEdit.Text : "player";
    public MapDef SelectedMap => _map;

    public void Build(Profile profile)
    {
        _profile = profile;
        _faction = profile.PreferredFaction;

        SetAnchorsAndOffsetsPreset(LayoutPreset.FullRect);

        var backdrop = new ColorRect { Color = new Color(0.03f, 0.04f, 0.06f, 0.96f) };
        backdrop.SetAnchorsAndOffsetsPreset(LayoutPreset.FullRect);
        AddChild(backdrop);

        var frame = new VBoxContainer();
        frame.SetAnchorsAndOffsetsPreset(LayoutPreset.FullRect);
        frame.OffsetLeft = 70; frame.OffsetTop = 44;
        frame.OffsetRight = -70; frame.OffsetBottom = -44;
        frame.AddThemeConstantOverride("separation", 12);
        AddChild(frame);

        frame.AddChild(UiTheme.Text("DEEP FIELD 3D", 30));
        frame.AddChild(UiTheme.Text("first-person co-op tower defense · 1–4 players", 13, UiTheme.InkDim));

        var nameRow = new HBoxContainer();
        nameRow.AddThemeConstantOverride("separation", 8);
        nameRow.AddChild(UiTheme.Text("name", 13, UiTheme.InkDim));
        _nameEdit = new LineEdit { Text = profile.Name, CustomMinimumSize = new Vector2(220, 0) };
        nameRow.AddChild(_nameEdit);
        frame.AddChild(nameRow);

        var columns = new HBoxContainer();
        columns.AddThemeConstantOverride("separation", 16);
        columns.SizeFlagsVertical = SizeFlags.ExpandFill;
        frame.AddChild(columns);

        // Factions
        var factionCard = UiTheme.Card();
        factionCard.CustomMinimumSize = new Vector2(430, 0);
        var factionBody = new VBoxContainer();
        factionBody.AddChild(UiTheme.Text("FACTION — one per lobby, levels persist", 12, UiTheme.InkDim));
        _factionColumn = new VBoxContainer();
        factionBody.AddChild(_factionColumn);
        factionCard.AddChild(factionBody);
        columns.AddChild(factionCard);

        // Maps
        var mapCard = UiTheme.Card();
        mapCard.CustomMinimumSize = new Vector2(320, 0);
        var mapBody = new VBoxContainer();
        mapBody.AddChild(UiTheme.Text("SECTOR", 12, UiTheme.InkDim));
        _mapColumn = new VBoxContainer();
        mapBody.AddChild(_mapColumn);
        mapCard.AddChild(mapBody);
        columns.AddChild(mapCard);

        // Play
        var playCard = UiTheme.Card();
        playCard.CustomMinimumSize = new Vector2(300, 0);
        var playBody = new VBoxContainer();
        playBody.AddThemeConstantOverride("separation", 8);
        playBody.AddChild(UiTheme.Text("PLAY", 12, UiTheme.InkDim));

        var solo = new Button { Text = "SOLO" };
        solo.Pressed += () => { Persist(); OnSolo?.Invoke(_faction); };
        playBody.AddChild(solo);

        var host = new Button { Text = "HOST" };
        host.Pressed += () => { Persist(); OnHost?.Invoke(_faction); };
        playBody.AddChild(host);
        playBody.AddChild(UiTheme.Text("friends join with your Tailscale IP", 11, UiTheme.InkDim));

        playBody.AddChild(new HSeparator());
        _addressEdit = new LineEdit
        {
            PlaceholderText = "100.x.x.x  (or ip:port)",
            Text = profile.LastJoinAddress,
        };
        playBody.AddChild(_addressEdit);
        var join = new Button { Text = "JOIN" };
        join.Pressed += () =>
        {
            if (_addressEdit.Text.Trim().Length == 0) { _status.Text = "enter the host's address"; return; }
            Persist();
            _profile.LastJoinAddress = _addressEdit.Text.Trim();
            _profile.Save();
            OnJoin?.Invoke(_addressEdit.Text.Trim(), _faction);
        };
        playBody.AddChild(join);

        _status = UiTheme.Text("", 12, UiTheme.Warn);
        playBody.AddChild(_status);

        playCard.AddChild(playBody);
        columns.AddChild(playCard);

        frame.AddChild(UiTheme.Text(
            "WASD move · Shift sprint · Space jump · hold E build · hold U upgrade · Q ability · "
            + "hold R revive · F start wave · Tab armory · Esc menu",
            11, UiTheme.InkDim));

        RebuildFactions();
        RebuildMaps();
    }

    public void SetStatus(string message) => _status.Text = message;

    private void Persist()
    {
        _profile.Name = PlayerName;
        _profile.PreferredFaction = _faction;
        _profile.Save();
    }

    private void RebuildFactions()
    {
        foreach (var child in _factionColumn.GetChildren()) child.QueueFree();

        foreach (var faction in Factions.All.Values)
        {
            int level = _profile.LevelFor(faction.Id);
            int xp = _profile.FactionXp.GetValueOrDefault(faction.Id, 0);
            int intoLevel = xp % Factions.XpPerLevel;
            bool selected = faction.Id == _faction;
            var accent = UiTheme.Faction(faction.Id);

            var card = UiTheme.Card(selected ? UiTheme.PanelRaised : UiTheme.Panel,
                selected ? accent : null);
            var body = new VBoxContainer();

            var header = new HBoxContainer();
            header.AddThemeConstantOverride("separation", 8);
            header.AddChild(new TextureRect
            {
                Texture = UiTheme.Icon($"faction_{faction.Id}", accent),
                Modulate = accent,
                CustomMinimumSize = new Vector2(24, 24),
                StretchMode = TextureRect.StretchModeEnum.KeepAspectCentered,
                ExpandMode = TextureRect.ExpandModeEnum.IgnoreSize,
            });
            header.AddChild(UiTheme.Text(faction.Id.ToUpperInvariant(), 16, accent));
            header.AddChild(UiTheme.Text($"Lv{level}", 13,
                level >= Factions.MaxLevel ? UiTheme.Good : UiTheme.Ink));
            body.AddChild(header);

            body.AddChild(UiTheme.Text(Describe(faction), 12, UiTheme.InkDim));

            // Level scaling made concrete, not implied.
            body.AddChild(UiTheme.Text(
                $"cooldown {faction.CooldownSeconds * Factions.CooldownFactor(level):0.#}s   ·   "
                + $"radius {faction.RadiusMeters * Factions.RadiusFactor(level):0.#}m",
                11, UiTheme.InkDim));

            if (level < Factions.MaxLevel)
            {
                var progress = new HBoxContainer();
                progress.AddThemeConstantOverride("separation", 6);
                progress.AddChild(UiTheme.Meter(intoLevel, Factions.XpPerLevel, accent, new Vector2(150, 8)));
                progress.AddChild(UiTheme.Text($"{intoLevel}/{Factions.XpPerLevel} xp", 11, UiTheme.InkDim));
                body.AddChild(progress);
            }
            else
            {
                body.AddChild(UiTheme.Text("mastered", 11, UiTheme.Good));
            }

            var pick = new Button { Text = selected ? "SELECTED" : "SELECT", Disabled = selected };
            string captured = faction.Id;
            pick.Pressed += () => { _faction = captured; RebuildFactions(); };
            body.AddChild(pick);

            card.AddChild(body);
            _factionColumn.AddChild(card);
        }
    }

    private static string Describe(FactionDef faction) => faction.AbilityId switch
    {
        "overdrive" => "Overdrive — surge nearby towers' fire rate.  Passive: cheaper builds.",
        "ignitionWave" => "Ignition Wave — burn a cone of enemies.  Passive: longer burns.",
        "chainSurge" => "Chain Surge — shock burst at your aim point.  Passive: faster fire.",
        _ => faction.AbilityId,
    };

    private void RebuildMaps()
    {
        foreach (var child in _mapColumn.GetChildren()) child.QueueFree();

        foreach (var map in new[] { Maps.Foundry, Maps.Switchyard })
        {
            bool selected = map.Id == _map.Id;
            var card = UiTheme.Card(selected ? UiTheme.PanelRaised : UiTheme.Panel,
                selected ? UiTheme.Accent : null);
            var body = new VBoxContainer();

            body.AddChild(UiTheme.Text(map.Id.ToUpperInvariant(), 16,
                selected ? UiTheme.Accent : UiTheme.Ink));
            body.AddChild(UiTheme.Text(Blurb(map.Id), 12, UiTheme.InkDim));
            body.AddChild(UiTheme.Text(
                $"{map.TotalWaves} waves · {map.Routes.Count} routes · "
                + $"{map.Sockets.Count(s => s.Tag != SocketTag.Trap)} build sockets",
                11, UiTheme.InkDim));

            var pick = new Button { Text = selected ? "SELECTED" : "SELECT", Disabled = selected };
            var captured = map;
            pick.Pressed += () => { _map = captured; RebuildMaps(); };
            body.AddChild(pick);

            card.AddChild(body);
            _mapColumn.AddChild(card);
        }
    }

    private static string Blurb(string mapId) => mapId switch
    {
        "foundry" => "Two tiers over a single yard. Air lane overhead, one zipline down to the core.",
        "switchyard" => "Three tiers. Barricade the freight cut to force the long switchback climb.",
        _ => "",
    };
}
