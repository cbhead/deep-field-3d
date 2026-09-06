using Godot;
using System.Collections.Generic;
using System.Linq;
using DeepField.Game.Ui;
using DeepField.Sim;
using DeepField.Sim.Content;
using SimWorld = DeepField.Sim.World;

namespace DeepField.Game;

public enum RunMode { Lobby, Solo, Host, Dedicated, Client }

/// <summary>Owns whichever half of the game this process is: the authoritative
/// sim (solo/host/dedicated) or the rendered view of a remote one (client).
/// The view reads state every frame and writes back only through commands.</summary>
public partial class GameRoot : Node3D
{
    public RunMode Mode { get; private set; } = RunMode.Lobby;

    private SimWorld? _world;                 // authoritative sim (not on pure clients)
    private SimWorld? _shadow;                // client: deserialized join snapshot (map/towers reference)
    private NetworkManager _net = null!;
    private InfoServer? _info;
    private double _accumulator;
    private readonly List<string> _tickEvents = new();

    private Player? _player;
    private MapDef _map = Maps.Foundry;
    private string _factionId = "ember";
    private string _playerName = "player";
    private Profile _profile = null!;
    private bool _xpBanked;

    private readonly Dictionary<int, Node3D> _enemyViews = new();
    private readonly Dictionary<int, Node3D> _projectileViews = new();
    private readonly Dictionary<int, Node3D> _towerViews = new();
    private string? _shotPath;
    private string _shotView = "eye";
    private int _shotCountdown;
    private List<Vector3> _laneMouths = new();
    private readonly Dictionary<string, StaticBody3D> _socketBodies = new();
    private readonly Dictionary<string, SocketTag> _socketTags = new();
    private readonly Dictionary<int, Node3D> _avatarViews = new();

    // UI surfaces (M2.5). GameRoot owns state; these are views that raise
    // commands back through Submit().
    private HudRoot _hud = null!;
    private MatchScreens _screens = null!;
    private BuildWheel _wheel = null!;
    private UpgradePanel _upgrade = null!;
    private ArmoryScreen _armory = null!;
    private BuildGhost _ghost = null!;
    private WorldMarkers _markers = null!;
    private LobbyScreen _lobby = null!;
    private CanvasLayer _overlay = null!;

    private readonly GameView _view = new();
    private readonly Dictionary<int, EnemyOverhead> _overheads = new();
    private readonly Dictionary<int, int> _killsByPlayer = new();
    private int _reactionCount;
    private int _lastWaveLeaks;
    private uint _seed;
    private bool _matchOver;
    private bool _paused;
    private int _bankedXp;
    private string _hint = "";

    /// <summary>Client-side aim feedback source (the sim isn't here to ask).</summary>
    private readonly Dictionary<int, (bool Burrowed, bool Shielded)> _lastSnapshotBits = new();

    public int LocalPlayerId => _net.LocalPlayerId;
    public GameView View => _view;

    /// <summary>Art coverage, printed once per run. Headless smoke lanes grep
    /// this, and it answers "is the build actually using the new models?"
    /// without opening the game.</summary>
    public override void _ExitTree() => GD.Print(AssetLibrary.Summary());

    public override void _Ready()
    {
        _net = new NetworkManager { Name = "Net" };
        AddChild(_net);
        _profile = Profile.Load();
        _playerName = _profile.Name;

        var args = OS.GetCmdlineUserArgs();
        if (args.Contains("--server"))
        {
            int port = ParsePort(args);
            StartDedicated(port);
            return;
        }

        BuildUi();

        // Headless smoke-test seams: --join <ip> connects from boot, --solo
        // starts a local match immediately (both exercise the full UI stack).
        for (int i = 0; i < args.Length; i++)
        {
            if (args[i] == "--join" && i + 1 < args.Length)
            {
                _playerName = "smoke";
                StartClient(args[i + 1], "forge");
                return;
            }
            if (args[i] == "--solo")
            {
                if (i + 1 < args.Length && Maps.All.TryGetValue(args[i + 1], out var chosen))
                    _map = chosen;
                _playerName = "smoke";
                StartSolo("ember");
                GD.Print($"[solo] started on {_map.Id}");
                return;
            }
            // --shot <map> <path> [eye|iso|top|<x,y,z>]: solo match, settle,
            // save a frame, quit. Needs a real renderer, so run it windowed.
            // This is how layout and art get reviewed without playing.
            if (args[i] == "--shot" && i + 2 < args.Length)
            {
                if (Maps.All.TryGetValue(args[i + 1], out var shotMap)) _map = shotMap;
                _playerName = "shot";
                _shotPath = args[i + 2];
                _shotView = i + 3 < args.Length ? args[i + 3] : "eye";
                _shotCountdown = 90;
                StartSolo("ember");
                return;
            }
            if (args[i] == "--asset-audit")
            {
                AuditAssets();
                GetTree().Quit();
                return;
            }
        }
    }

    /// <summary>Instantiates one view of every enemy, structure, projectile and
    /// hero in the content tables, then reports coverage.
    ///
    /// A normal solo smoke run only touches whatever happens to spawn in thirty
    /// seconds, which left the structure and projectile paths untested. This
    /// walks the tables directly, so a def whose view construction throws — or
    /// a model whose name doesn't match what the code asks for — fails CI
    /// instead of failing in front of a player.</summary>
    private void CaptureShot()
    {
        string path = _shotPath!;
        _shotPath = null;

        // Surface shots: open the UI being reviewed, then capture next frame.
        if (_shotView is "armory" or "wheel" or "upgrade")
        {
            string surface = _shotView;
            _shotView = "eye";
            _shotPath = path;
            _shotCountdown = 4;
            if (surface == "armory") ToggleArmory();
            else if (surface == "wheel") OpenBuildWheel(_map.Sockets[0].Id);
            else if (surface == "upgrade" && _world is not null)
            {
                Submit(new Command.PlaceTower(LocalPlayerId, "lance", _map.Sockets[0].Id));
                for (int i = 0; i < 3; i++) Step.Advance(_world);
                RebuildView();
                OpenUpgradePanel(_map.Sockets[0].Id);
            }
            return;
        }

        // Layout review needs to see the whole map, not the player's eyeline.
        // The camera has to exist for a frame before the viewport shows it, so
        // this pass only stages it and re-arms the countdown.
        if (_shotView != "eye")
        {
            var (from, look) = _shotView switch
            {
                "top" => (new Vector3(0, 95, 1), Vector3.Zero),
                "iso" => (new Vector3(-52, 46, 52), new Vector3(0, 0, -2)),
                "iso2" => (new Vector3(56, 40, -46), new Vector3(0, 0, -2)),
                "lane" => (new Vector3(-46, 14, -26), new Vector3(0, 0, 4)),
                // Fixture checks: the gate enemies walk out of, the yard the
                // players spawn into, and the air strand over the map.
                "gate" => (new Vector3(-30, 9, 22), new Vector3(-40, 2, 0)),
                "yard" => (new Vector3(4, 4f, -28), new Vector3(-6, 1.6f, -23)),
                "tunnel" => (new Vector3(11, 5, -3), new Vector3(0, 1, -8)),
                "air" => (new Vector3(-14, 24, 38), new Vector3(0, 8, 0)),
                "core" => (new Vector3(20, 10, 26), new Vector3(36, 2, 6)),
                "deck" => (new Vector3(30, 15, 6), new Vector3(6, 6, -10)),
                _ => (new Vector3(0, 60, 60), Vector3.Zero),
            };
            var camera = new Camera3D { Position = from, Far = 500f };
            AddChild(camera);
            camera.LookAt(look, Vector3.Up);
            camera.MakeCurrent();

            _shotPath = path;
            _shotView = "eye";
            _shotCountdown = 4;
            _hud?.Hide();
            _screens?.Hide();
            return;
        }

        var image = GetViewport().GetTexture().GetImage();
        image.SavePng(path);
        GD.Print($"[shot] wrote {path}  ({image.GetWidth()}x{image.GetHeight()})");
        GetTree().Quit();
    }

    private void AuditAssets()
    {
        _map = Maps.All["foundry"];
        var built = new List<Node>();

        foreach (var def in Enemies.All.Values)
            built.Add(SpawnEnemyView(built.Count, def.Id));

        foreach (var def in Towers.All.Values)
        {
            // Every path at every level, which is also the only exercise the
            // rig-merge gets outside a real match.
            for (int level = 0; level <= 10; level++)
            {
                var levels = new int[def.UpgradePaths.Count];
                for (int p = 0; p < levels.Length; p++) levels[p] = level;
                built.Add(SpawnStructureView(def.Id, Vector3.Zero, levels));
            }
            built.Add(SpawnProjectileView(def.Id));
        }

        foreach (var def in Traps.All.Values)
            built.Add(SpawnStructureView(def.Id, Vector3.Zero, null));

        foreach (var def in Factions.All.Values)
        {
            built.Add(AssetLibrary.Instantiate($"hero_{def.Id}", () => Placeholders.Hero(def.Id)));
            built.Add(AssetLibrary.Instantiate($"hero_{def.Id}_downed", () => Placeholders.Hero(def.Id)));
        }

        GD.Print($"[asset-audit] built {built.Count} views over {AssetLibrary.Requested.Count} asset names");
        foreach (string name in AssetLibrary.Missing)
            GD.Print($"[asset-audit] placeholder: {name} -> {AssetLibrary.PathFor(name)}");

        // Free eagerly: quitting with 600 live meshes makes the headless
        // renderer complain about leaked RIDs, which reads as a failure.
        foreach (var node in built) node.Free();
    }

    private static int ParsePort(string[] args)
    {
        for (int i = 0; i < args.Length - 1; i++)
            if (args[i] == "--port" && int.TryParse(args[i + 1], out int p)) return p;
        return Protocol.DefaultPort;
    }

    // =====================================================================
    // Mode startup
    // =====================================================================

    private void StartDedicated(int port)
    {
        Mode = RunMode.Dedicated;
        var args = OS.GetCmdlineUserArgs();
        for (int i = 0; i < args.Length - 1; i++)
            if (args[i] == "--map" && Maps.All.TryGetValue(args[i + 1], out var chosen))
                _map = chosen;
        _world = new SimWorld(FreshSeed(), _map) { WaitForPlayers = true };
        _net.HostServer(port, _world);
        _net.ServerEnqueue = c => _world.Enqueue(c);

        _info = new InfoServer { Name = "Info" };
        AddChild(_info);
        _info.StatusProvider = ServerInfo;
        _info.Start(port);
        GD.Print($"[server] deepfield-3d dedicated on udp:{port}, map {_map.Id}, protocol v{Protocol.Version}");
    }

    public void StartSolo(string faction)
    {
        Mode = RunMode.Solo;
        BeginLocalWorld(faction);
    }

    public void StartHost(string faction)
    {
        Mode = RunMode.Host;
        BeginLocalWorld(faction);
        _net.HostServer(Protocol.DefaultPort, _world!);
        _net.ServerEnqueue = c => _world!.Enqueue(c);

        _info = new InfoServer { Name = "Info" };
        AddChild(_info);
        _info.StatusProvider = ServerInfo;
        _info.Start(Protocol.DefaultPort);
        Post(_info.TailscaleIp is { } ip
            ? $"hosting — invite: {ip}:{Protocol.DefaultPort}"
            : "hosting (no tailscale ip found — friends need your LAN ip)");
    }

    public void StartClient(string address, string faction)
    {
        Mode = RunMode.Client;
        _factionId = faction;
        _lobby.Visible = false;
        _screens.ShowStatus("CONNECTING", $"reaching {address}…");

        // "100.x.x.x" or "100.x.x.x:8791" both work.
        int port = Protocol.DefaultPort;
        if (address.Contains(':'))
        {
            var parts = address.Split(':');
            address = parts[0];
            if (int.TryParse(parts[1], out int parsed)) port = parsed;
        }

        Multiplayer.ConnectedToServer += () =>
        {
            _net.SendHello(_playerName, _factionId, _profile.LevelFor(_factionId));
            _screens.ShowStatus("SYNCING", "receiving the world — you'll drop in at the next intermission");
        };
        Multiplayer.ConnectionFailed += () => _screens.ShowStatus("CONNECTION FAILED",
            "check the host's address and that you're on their tailnet, then try again");
        _net.JoinServer(address, port);
    }

    private void BeginLocalWorld(string faction)
    {
        _factionId = faction;
        _lobby.Visible = false;
        _seed = FreshSeed();
        _world = new SimWorld(_seed, _map);
        _world.Enqueue(new Command.Join(1, _playerName, faction, _profile.LevelFor(faction)));
        BuildLevel(_map);
        SpawnLocalPlayer();
        _markers.ShowDamageNumbers = _profile.ShowDamageNumbers;
    }

    private void SpawnLocalPlayer()
    {
        _player = new Player { Name = "Player" };
        AddChild(_player);
        _player.GlobalPosition = ToGd(_map.HeroSpawn) + new Vector3(0, 1.2f, 0);
        _player.RotationDegrees = new Vector3(0, 180, 0);
    }

    private static uint FreshSeed() => (uint)(Time.GetTicksMsec() & 0xFFFFFFFF) ^ 0x9E3779B9u;

    /// <summary>Mission Control's window into a live match.</summary>
    private Godot.Collections.Dictionary ServerInfo() => new()
    {
        ["app"] = "deepfield-3d",
        ["map"] = _world!.Map.Id,
        ["players"] = _world.ConnectedPlayerCount,
        ["wave"] = _world.WaveIndex + 1,
        ["totalWaves"] = _world.Map.TotalWaves,
        ["phase"] = _world.Phase.ToString(),
        ["lives"] = _world.Lives,
        ["money"] = _world.Money,
    };

    // ---- Solo save/load (F9/F10): the serialization layer doing double duty --

    public void SaveGame()
    {
        if (Mode != RunMode.Solo || _world is null) { Toast("save is solo-only"); return; }
        using var file = FileAccess.Open($"user://save-{_map.Id}.json", FileAccess.ModeFlags.Write);
        file.StoreString(Serialization.Serialize(_world));
        Toast("saved");
    }

    public void LoadGame()
    {
        if (Mode != RunMode.Solo) { Toast("load is solo-only"); return; }
        string path = $"user://save-{_map.Id}.json";
        if (!FileAccess.FileExists(path)) { Toast("no save for this map"); return; }
        using var file = FileAccess.Open(path, FileAccess.ModeFlags.Read);
        _world = Serialization.Deserialize(file.GetAsText());

        // Rebuild structure views from the restored world.
        foreach (var view in _towerViews.Values) view.QueueFree();
        _towerViews.Clear();
        foreach (var tower in _world.Towers)
            OnTowerPlaced(tower.Id, tower.DefId, tower.SocketId, tower.PathLevels);
        foreach (var trap in _world.Traps) OnTowerPlaced(trap.Id, trap.DefId, trap.SocketId);
        _xpBanked = false;
        Toast($"loaded — wave {_world.WaveIndex + 1}, {_world.Lives} lives");
    }

    // =====================================================================
    // Frame loop
    // =====================================================================

    public override void _Process(double delta)
    {
        if (_shotPath is not null && --_shotCountdown <= 0) CaptureShot();

        switch (Mode)
        {
            case RunMode.Lobby:
                return;

            case RunMode.Solo:
            case RunMode.Host:
            case RunMode.Dedicated:
                StepAuthoritative(delta);
                break;

            case RunMode.Client:
                StepClient(delta);
                break;
        }

        if (Mode == RunMode.Dedicated) return;

        RebuildView();
        RefreshUi(delta);
    }

    /// <summary>One read model per frame, from whichever source this process
    /// has. Everything in ui/ reads this and nothing else.</summary>
    private void RebuildView()
    {
        if (Mode != RunMode.Client && _world is not null)
            _view.FromWorld(_world, LocalPlayerId);
        else if (_net.Meta is { } meta)
            _view.FromMeta(meta, LocalPlayerId);
    }

    private void RefreshUi(double delta)
    {
        if (!_view.Valid) return;

        if (_world is not null && _world.Players.TryGetValue(LocalPlayerId, out var me))
            _hud.SetBleedout(me.BleedoutTimer);
        _hud.Refresh(_view, delta, Input.MouseMode == Input.MouseModeEnum.Captured, _hint);
        if (_wheel.IsOpen) _wheel.Refresh(_view);
        if (_upgrade.IsOpen) _upgrade.Refresh(_view);
        if (_armory.IsOpen) _armory.Refresh(_view);

        // Intermission panel rides the phase, not an event, so a late joiner
        // sees it immediately.
        if (_view.Phase == MatchPhase.Intermission && !_matchOver)
            _screens.ShowIntermission(_view, _map, _seed, _lastWaveLeaks);
        else
            _screens.HideIntermission();

        UpdateReviveBeacons();
    }

    private void UpdateReviveBeacons()
    {
        var local = _view.Local;
        foreach (var player in _view.Players)
        {
            if (player.Id == LocalPlayerId) continue;
            float distance = local is null ? 0f : local.Pos.DistanceTo(player.Pos);
            _markers.SetReviveBeacon(player.Id, player.Pos, player.Name, distance,
                player.Downed && player.Connected);
        }
    }

    private void StepAuthoritative(double delta)
    {
        _tickEvents.Clear();

        // Solo pause freezes the sim; hosts keep running for everyone else.
        if (_paused && Mode == RunMode.Solo) return;

        _accumulator = Mathf.Min(_accumulator + delta, 0.25);
        while (_accumulator >= Balance.Dt)
        {
            if (_player is not null)
            {
                var p = _player.GlobalPosition;
                _world!.Enqueue(new Command.PlayerSync(LocalPlayerId, new Vec3(p.X, p.Y, p.Z)));
            }
            Step.Advance(_world!);
            foreach (var e in _world!.Events) _tickEvents.Add(e.LogLine());
            if (Mode != RunMode.Dedicated) DrainLocalEvents();
            _accumulator -= Balance.Dt;
        }

        _net.ServerBroadcast(delta, _tickEvents);

        if (Mode != RunMode.Dedicated)
        {
            SyncEnemyViewsLocal();
            SyncProjectileViews();
            SyncAvatarsFromWorld();
        }
    }

    private void StepClient(double delta)
    {
        if (_net.JoinError is { } error)
        {
            Toast(error);
            _net.JoinError = null;
            return;
        }

        // Late-join full-state sync arrived: build the level + existing towers.
        if (_net.PendingWorldJson is { } json)
        {
            _net.PendingWorldJson = null;
            _shadow = Serialization.Deserialize(json);
            _map = _shadow.Map;
            BuildLevel(_map);
            // Traps were missing from this loop, so a late joiner saw an empty
            // path plate where the host had a Spike.
            foreach (var tower in _shadow.Towers)
                OnTowerPlaced(tower.Id, tower.DefId, tower.SocketId, tower.PathLevels);
            foreach (var trap in _shadow.Traps)
                OnTowerPlaced(trap.Id, trap.DefId, trap.SocketId);
            SpawnLocalPlayer();
            GD.Print($"[client] joined seat {LocalPlayerId} on map {_map.Id}");
            Toast($"joined as seat {LocalPlayerId}");
        }

        if (_player is null) return;

        foreach (var line in _net.PendingEvents) HandleEventLine(line);
        _net.PendingEvents.Clear();

        var pos = _player.GlobalPosition;
        _net.SendAvatar(pos, _player.Rotation.Y);

        SyncEnemyViewsRemote();
        SyncAvatarsFromMeta();
    }

    // =====================================================================
    // Commands in (from Player) — one seam for all modes.
    // =====================================================================

    public void Submit(Command command)
    {
        if (Mode == RunMode.Client) _net.SendCommand(command);
        else _world?.Enqueue(command);
    }

    public bool SocketOccupied(string socketId)
    {
        var world = _world ?? _shadow;
        if (world is null) return false;
        if (Mode == RunMode.Client)
            return _towerViews.Values.Any(v => (string)v.GetMeta("socket_id", "") == socketId);
        return world.Towers.Any(t => t.SocketId == socketId)
            || world.Traps.Any(t => t.SocketId == socketId);
    }

    public string CurrentWeaponId()
    {
        if (Mode != RunMode.Client && _world is not null
            && _world.Players.TryGetValue(LocalPlayerId, out var p))
            return p.WeaponId;
        if (_net.Meta is { } meta && meta["players"].AsGodotArray() is { } players)
        {
            foreach (Godot.Collections.Dictionary entry in players)
                if ((int)entry["id"] == LocalPlayerId) return (string)entry["weapon"];
        }
        return "sidearm";
    }

    public MapDef CurrentMap() => _map;

    public int TowerIdAtSocket(string socketId)
    {
        if (Mode != RunMode.Client && _world is not null)
            return _world.Towers.FirstOrDefault(t => t.SocketId == socketId)?.Id ?? -1;
        foreach (var (id, view) in _towerViews)
            if ((string)view.GetMeta("socket_id", "") == socketId) return id;
        return -1;
    }

    public int NearestDownedPlayer(Vector3 from, float range)
    {
        if (Mode != RunMode.Client && _world is not null)
        {
            foreach (var p in _world.Players.Values)
            {
                if (!p.Downed || p.Id == LocalPlayerId) continue;
                if (new Vector3(p.Pos.X, p.Pos.Y, p.Pos.Z).DistanceTo(from) <= range) return p.Id;
            }
            return -1;
        }
        if (_net.Meta is { } meta)
        {
            foreach (Godot.Collections.Dictionary entry in meta["players"].AsGodotArray())
            {
                if (!(bool)entry["downed"] || (int)entry["id"] == LocalPlayerId) continue;
                var pos = new Vector3((float)entry["x"], (float)entry["y"], (float)entry["z"]);
                if (pos.DistanceTo(from) <= range) return (int)entry["id"];
            }
        }
        return -1;
    }

    // =====================================================================
    // Event handling (client parses the reliable line channel)
    // =====================================================================

    private void DrainLocalEvents()
    {
        foreach (var e in _world!.Events)
        {
            switch (e)
            {
                case SimEvent.TowerPlaced placed:
                    OnTowerPlaced(placed.TowerId, placed.DefId, placed.SocketId);
                    break;
                case SimEvent.TowerUpgraded upgraded:
                    OnTowerUpgraded(upgraded.TowerId, upgraded.PathId, upgraded.NewLevel);
                    break;
                case SimEvent.TowerSold sold:
                    ReleaseStructureView(sold.TowerId);
                    break;

                // Refusals answer at the surface that caused them, not only in
                // the feed — the player is looking at the wheel, not the corner.
                case SimEvent.BuildRejected rejected:
                    _wheel.ShowRefusal(Explain(rejected.Reason));
                    Post($"build refused: {Explain(rejected.Reason)}", UiTheme.Danger);
                    break;
                case SimEvent.UpgradeRejected rejected:
                    _upgrade.ShowRefusal(Explain(rejected.Reason));
                    Post($"upgrade refused: {Explain(rejected.Reason)}", UiTheme.Danger);
                    break;
                case SimEvent.PurchaseRejected rejected when rejected.PlayerId == LocalPlayerId:
                    _armory.ShowNotice(Explain(rejected.Reason));
                    Post($"armory: {Explain(rejected.Reason)}", UiTheme.Danger);
                    break;
                case SimEvent.CraftRejected rejected when rejected.PlayerId == LocalPlayerId:
                    _armory.ShowNotice(Explain(rejected.Reason));
                    Post($"craft: {Explain(rejected.Reason)}", UiTheme.Danger);
                    break;
                case SimEvent.JoinRejected rejected:
                    _screens.ShowStatus("JOIN REFUSED", Explain(rejected.Reason));
                    break;

                case SimEvent.WaveStarted started:
                    _screens.HideIntermission();
                    _lastWaveLeaks = 0;
                    Post($"wave {started.WaveIndex + 1} — {started.EnemyCount} inbound");
                    break;
                case SimEvent.WaveCleared cleared:
                    Post($"wave {cleared.WaveIndex + 1} cleared", UiTheme.Good);
                    break;
                case SimEvent.EnemyLeaked leaked:
                    _lastWaveLeaks++;
                    Post("BREACH — core hit", UiTheme.Danger);
                    OnBreach(leaked.EnemyId);
                    break;

                case SimEvent.ReactionTriggered reaction:
                    _reactionCount++;
                    OnReaction(reaction.EnemyId, reaction.ReactionId);
                    break;

                case SimEvent.PlayerDowned downed:
                    Post(downed.PlayerId == LocalPlayerId
                        ? "DOWNED — hold on, a teammate can revive you"
                        : $"{NameOf(downed.PlayerId)} is down", UiTheme.Danger);
                    break;
                case SimEvent.PlayerRevived revived:
                    Post($"{NameOf(revived.PlayerId)} revived", UiTheme.Good);
                    break;

                case SimEvent.MatchEnded ended:
                    OnMatchEnded(ended.Victory);
                    break;

                case SimEvent.AttachmentCrafted crafted when crafted.PlayerId == LocalPlayerId:
                    _profile.RecordAttachment(crafted.WeaponId,
                        Attachments.All[crafted.AttachmentId].Slot.ToString(), crafted.AttachmentId);
                    _armory.ShowNotice($"crafted {crafted.AttachmentId}");
                    Post($"crafted {crafted.AttachmentId}", UiTheme.Good);
                    break;
                case SimEvent.AmmoSelected ammo when ammo.PlayerId == LocalPlayerId:
                    _profile.RecordAmmo(ammo.WeaponId, ammo.AmmoId);
                    _armory.ShowNotice($"loaded {ammo.AmmoId}");
                    break;

                case SimEvent.EnemyDamaged damaged:
                    OnEnemyDamaged(damaged.EnemyId, damaged.Amount, damaged.Source);
                    break;
                case SimEvent.EnemyDied died:
                    OnEnemyDied(died.EnemyId, died.Bounty, died.Source);
                    break;
            }
        }
    }

    private void HandleEventLine(string line)
    {
        var p = line.Split(' ');
        if (p.Length < 2) return;
        switch (p[1])
        {
            case "towerPlaced": OnTowerPlaced(int.Parse(p[2]), p[3], p[4]); break;
            case "towerUpgraded": OnTowerUpgraded(int.Parse(p[2]), p[3], int.Parse(p[4])); break;
            case "towerSold":
                ReleaseStructureView(int.Parse(p[2]));
                break;
            case "buildRejected":
                _wheel.ShowRefusal(Explain(p[5]));
                Post($"build refused: {Explain(p[5])}", UiTheme.Danger);
                break;
            case "upgradeRejected":
                _upgrade.ShowRefusal(Explain(p[4]));
                Post($"upgrade refused: {Explain(p[4])}", UiTheme.Danger);
                break;
            case "craftRejected" when int.Parse(p[2]) == LocalPlayerId:
                _armory.ShowNotice(Explain(p[4]));
                break;
            case "waveStarted":
                _screens.HideIntermission();
                _lastWaveLeaks = 0;
                Post($"wave {int.Parse(p[2]) + 1} — {p[3]} inbound");
                break;
            case "waveCleared": Post($"wave {int.Parse(p[2]) + 1} cleared", UiTheme.Good); break;
            case "enemyLeaked":
                _lastWaveLeaks++;
                Post("BREACH — core hit", UiTheme.Danger);
                OnBreach(int.Parse(p[2]));
                break;
            case "reaction": _reactionCount++; OnReaction(int.Parse(p[2]), p[3]); break;
            case "playerDowned":
                Post(int.Parse(p[2]) == LocalPlayerId
                    ? "DOWNED — hold on, a teammate can revive you"
                    : $"{NameOf(int.Parse(p[2]))} is down", UiTheme.Danger);
                break;
            case "playerRevived": Post($"{NameOf(int.Parse(p[2]))} revived", UiTheme.Good); break;
            case "matchEnded": OnMatchEnded(p[2] == "victory"); break;
            case "attachmentCrafted" when int.Parse(p[2]) == LocalPlayerId:
                _profile.RecordAttachment(p[3], Attachments.All[p[4]].Slot.ToString(), p[4]);
                _armory.ShowNotice($"crafted {p[4]}");
                break;
            case "ammoSelected" when int.Parse(p[2]) == LocalPlayerId:
                _profile.RecordAmmo(p[3], p[4]);
                _armory.ShowNotice($"loaded {p[4]}");
                break;
            case "enemyDamaged":
                OnEnemyDamaged(int.Parse(p[2]), float.Parse(p[3],
                    System.Globalization.CultureInfo.InvariantCulture), p[4]);
                break;
            case "enemyDied": OnEnemyDied(int.Parse(p[2]), int.Parse(p[4]), p[5]); break;
        }
    }

    // ---- Shared event reactions (identical in every mode) -----------------

    private void OnEnemyDamaged(int enemyId, float amount, string source)
    {
        bool mine = source == $"player{LocalPlayerId}";
        if (mine) _hud.SetCrosshair(CrosshairState.Hit);

        if (EnemyWorldPos(enemyId) is { } pos)
        {
            var color = source.StartsWith("player")
                ? (mine ? UiTheme.Ink : UiTheme.InkDim)
                : source.StartsWith("tower") ? UiTheme.Accent : UiTheme.Warn;
            _markers.DamageNumber(pos, amount, color);
        }
    }

    private void OnEnemyDied(int enemyId, int bounty, string source)
    {
        if (source.StartsWith("player") && int.TryParse(source[6..], out int killer))
        {
            _killsByPlayer[killer] = _killsByPlayer.GetValueOrDefault(killer) + 1;
            if (killer == LocalPlayerId)
            {
                _hud.SetCrosshair(CrosshairState.Kill, 0.3);
                Post($"+{bounty}c", UiTheme.Warn);
            }
        }
        _overheads.Remove(enemyId);
        _lastSnapshotBits.Remove(enemyId);
    }

    private void OnReaction(int enemyId, string reactionId)
    {
        string label = reactionId switch
        {
            "thermalShock" => "THERMAL SHOCK",
            "flashFreeze" => "FLASH FREEZE",
            _ => reactionId.ToUpperInvariant(),
        };
        var color = reactionId == "flashFreeze" ? UiTheme.Status("freeze") : UiTheme.Status("burn");
        if (EnemyWorldPos(enemyId) is { } pos) _markers.Callout(pos, label, color);
        Post(label, color);
    }

    private void OnMatchEnded(bool victory)
    {
        if (_matchOver) return;
        _matchOver = true;
        BankLocalXp();
        _screens.HideIntermission();
        _screens.ShowEnd(_view, victory, _bankedXp, _factionId, _killsByPlayer, _reactionCount);
        Input.MouseMode = Input.MouseModeEnum.Visible;
    }

    /// <summary>Point the player at a leak they didn't see. The leaking enemy's
    /// view is already gone this frame, so fall back to the route's goal.</summary>
    private void OnBreach(int enemyId)
    {
        var where = EnemyWorldPos(enemyId)
            ?? ToGd(_map.Routes[0].Waypoints[^1]);
        _hud.FlagBreach(where, GetViewport().GetCamera3D());
    }

    private Vector3? EnemyWorldPos(int enemyId) =>
        _enemyViews.TryGetValue(enemyId, out var view) && IsInstanceValid(view)
            ? view.Position
            : null;

    private string NameOf(int playerId) =>
        _view.Players.FirstOrDefault(p => p.Id == playerId)?.Name ?? $"player {playerId}";

    /// <summary>Sim refusal codes are terse by design; the UI says them in words.</summary>
    private static string Explain(string reason) => reason switch
    {
        "insufficientFunds" => "not enough credits",
        "insufficientScrap" => "not enough scrap",
        "occupied" => "socket already taken",
        "unknownSocket" => "no socket there",
        "unknownTower" => "unknown structure",
        "wrongSocketTag" => "wrong socket type for that",
        "trapSocket" => "that plate takes traps",
        "maxLevel" => "already at max level",
        "unknownPath" => "no such upgrade path",
        "factionTaken" => "another player already has that faction",
        "unknownFaction" => "unknown faction",
        "alreadyOwned" => "already owned",
        "weaponNotOwned" => "buy the weapon first",
        "unknownAttachment" => "unknown attachment",
        "unknownAmmo" => "unknown ammo",
        _ => reason,
    };

    private void OnTowerPlaced(int towerId, string defId, string socketId, int[]? pathLevels = null)
    {
        var socket = _map.Sockets.FirstOrDefault(s => s.Id == socketId);
        if (socket is null) return;
        if (_towerViews.Remove(towerId, out var existing))
        {
            RemoveChild(existing);
            existing.QueueFree();
        }

        int levelCount = Towers.All.TryGetValue(defId, out var towerDef)
            ? towerDef.UpgradePaths.Count : 0;
        int[] levels = pathLevels ?? new int[levelCount];

        var view = SpawnStructureView(defId, ToGd(socket.Pos), levels);
        view.SetMeta("socket_id", socketId);
        view.SetMeta("def_id", defId);
        view.SetMeta("levels", levels);
        _towerViews[towerId] = view;
        RefreshSocketArt(socketId, occupied: true);
    }

    /// <summary>Every tower, trap and barricade in the game comes from here —
    /// design's model if it has shipped, the graybox otherwise. Traps request
    /// their armed state, the one they spend most of their life in.
    ///
    /// A tower is chassis + one cumulative stage module per upgraded path.
    /// Design's chassis already *is* the level-1 state and `_s1` is
    /// deliberately empty, so the sim's path level N (0 = unupgraded) asks for
    /// stage N+1.</summary>
    private Node3D SpawnStructureView(string defId, Vector3 pos, int[]? levels)
    {
        var root = new Node3D { Position = pos };
        var chassis = AssetLibrary.Instantiate(
            AssetLibrary.StructureAsset(defId), () => Placeholders.Structure(defId));
        chassis.Name = "Body";
        root.AddChild(chassis);
        AddChild(root);

        if (levels is null || !Towers.All.TryGetValue(defId, out var def)) return root;
        for (int i = 0; i < levels.Length && i < def.UpgradePaths.Count; i++)
        {
            if (levels[i] <= 0) continue;
            string pathId = def.UpgradePaths[i].Id;
            int stage = Mathf.Clamp(levels[i] + 1, 1, 10);

            var module = AssetLibrary.TryInstantiate($"tower_{defId}_{pathId}_s{stage}");
            if (module is null)
            {
                var stand = Placeholders.TowerModule(pathId, i, levels[i]);
                stand.Name = $"path_{pathId}";
                root.AddChild(stand);
                continue;
            }

            // Design's modules mirror the chassis rig skeleton so a part that
            // must swing with the barrel sits under the same-named node; move
            // the parts across and drop the now-empty scaffold.
            root.AddChild(module);
            MergeRig(module, chassis);
            root.RemoveChild(module);
            module.QueueFree();
        }
        return root;
    }

    /// <summary>Moves a module's parts onto the chassis nodes of the same name,
    /// descending through matching rig empties so pitch parts land under pitch
    /// (game/assets/structures/README-towers.md).</summary>
    private static void MergeRig(Node module, Node chassis)
    {
        foreach (var child in module.GetChildren())
        {
            var match = chassis.GetNodeOrNull<Node3D>(child.Name.ToString());
            if (match is not null)
            {
                MergeRig(child, match);      // rig node: keep descending
                continue;
            }
            module.RemoveChild(child);       // real part: the chassis adopts it
            chassis.AddChild(child);
        }
    }

    /// <summary>Upgrades were invisible before this: a level-5 Lance looked
    /// exactly like the one you just paid 75 for.
    ///
    /// The whole view is rebuilt rather than patched, because stage modules are
    /// cumulative and their parts get reparented into the chassis — there is no
    /// single node left to swap out.</summary>
    private void OnTowerUpgraded(int towerId, string pathId, int newLevel)
    {
        if (!_towerViews.TryGetValue(towerId, out var view)) return;
        string defId = (string)view.GetMeta("def_id", "");
        string socketId = (string)view.GetMeta("socket_id", "");
        if (defId.Length == 0 || !Towers.All.TryGetValue(defId, out var def)) return;

        int index = -1;
        for (int i = 0; i < def.UpgradePaths.Count; i++)
            if (def.UpgradePaths[i].Id == pathId) index = i;
        if (index < 0) return;

        int[] levels = view.HasMeta("levels")
            ? (int[])view.GetMeta("levels")
            : new int[def.UpgradePaths.Count];
        if (index >= levels.Length) return;
        levels[index] = newLevel;

        OnTowerPlaced(towerId, defId, socketId, levels);
    }

    // =====================================================================
    // View sync
    // =====================================================================

    private void SyncEnemyViewsLocal()
    {
        foreach (var enemy in _world!.Enemies)
        {
            if (!_enemyViews.TryGetValue(enemy.Id, out var view))
            {
                view = SpawnEnemyView(enemy.Id, enemy.DefId);
                _enemyViews[enemy.Id] = view;
            }
            view.Position = ToGd(enemy.Pos);
            byte bits = Protocol.PackStatusBits(enemy);
            TintEnemy(view, enemy.Hp / enemy.MaxHp, bits);

            float maxShield = Enemies.All[enemy.DefId].Shield;
            float shieldFraction = maxShield > 0f
                ? enemy.Shield / (maxShield * (enemy.MaxHp / Enemies.All[enemy.DefId].Hp))
                : 0f;
            UpdateEnemyStates(view, enemy.Burrowed, shieldFraction);
            UpdateOverhead(enemy.Id, view, enemy.Hp / enemy.MaxHp, shieldFraction,
                bits, enemy.Burrowed);
        }
        SweepViews(_enemyViews, _world.Enemies.Select(e => e.Id));
        SweepOverheads(_world.Enemies.Select(e => e.Id));
    }

    /// <summary>Attaches (once) and updates the billboarded bar/status cluster
    /// above an enemy. Overheads live under the enemy view so they follow it and
    /// die with it.</summary>
    private void UpdateOverhead(int enemyId, Node3D view, float hpFraction,
        float shieldFraction, byte statusBits, bool burrowed)
    {
        if (!_overheads.TryGetValue(enemyId, out var overhead) || !IsInstanceValid(overhead))
        {
            overhead = new EnemyOverhead { HeadHeight = view.HasMeta("head_height")
                ? (float)view.GetMeta("head_height") : 2.0f };
            view.AddChild(overhead);
            _overheads[enemyId] = overhead;
        }

        var camera = GetViewport().GetCamera3D();
        float distance = camera is null ? 0f : camera.GlobalPosition.DistanceTo(view.Position);
        overhead.Set(hpFraction, shieldFraction, statusBits, burrowed, distance);
    }

    private void SweepOverheads(IEnumerable<int> liveIds)
    {
        var live = new HashSet<int>(liveIds);
        foreach (int id in _overheads.Keys.Where(id => !live.Contains(id)).ToList())
            _overheads.Remove(id);
    }

    private void SyncEnemyViewsRemote()
    {
        if (_net.SnapshotBuffer.Count == 0) return;

        // Interpolate between the two most recent snapshots, ~1 interval behind.
        var newest = _net.SnapshotBuffer[^1];
        var previous = _net.SnapshotBuffer.Count > 1 ? _net.SnapshotBuffer[^2] : newest;
        float t = 0.5f; // fixed midpoint blend: simple and smooth enough at 15 Hz

        var prevById = previous.Enemies.ToDictionary(s => s.Id);
        foreach (var snap in newest.Enemies)
        {
            if (!_enemyViews.TryGetValue(snap.Id, out var view))
            {
                view = SpawnEnemyView(snap.Id, snap.DefId);
                _enemyViews[snap.Id] = view;
            }
            var target = prevById.TryGetValue(snap.Id, out var prev)
                ? Vec3.Lerp(prev.Pos, snap.Pos, t)
                : snap.Pos;
            view.Position = ToGd(target);
            TintEnemy(view, snap.HpFraction, snap.StatusBits);

            // Snapshots carry no shield channel of their own; a Warden that
            // still has shield reads as full hp with the shield bit unset, so
            // the client shows hp only and lets the crosshair report shielded.
            bool burrowed = snap.HpFraction <= 0f;
            UpdateEnemyStates(view, burrowed, 0f);
            UpdateOverhead(snap.Id, view, snap.HpFraction, 0f, snap.StatusBits, burrowed);
            _lastSnapshotBits[snap.Id] = (burrowed, Shielded: false);
        }
        SweepViews(_enemyViews, newest.Enemies.Select(s => s.Id));
        SweepOverheads(newest.Enemies.Select(s => s.Id));
    }

    private void SyncProjectileViews()
    {
        foreach (var projectile in _world!.Projectiles)
        {
            if (!_projectileViews.TryGetValue(projectile.Id, out var view))
            {
                string firedBy = _world.Towers.FirstOrDefault(t => t.Id == projectile.FiredBy)?.DefId ?? "lance";
                view = SpawnProjectileView(firedBy);
                _projectileViews[projectile.Id] = view;
            }
            view.Position = ToGd(projectile.Pos);
        }
        SweepViews(_projectileViews, _world.Projectiles.Select(p => p.Id));
    }

    private void SyncAvatarsFromWorld()
    {
        foreach (var p in _world!.Players.Values)
        {
            if (p.Id == LocalPlayerId || !p.Connected) continue;
            UpdateAvatarView(p.Id, new Vector3(p.Pos.X, p.Pos.Y, p.Pos.Z), p.Downed, p.FactionId);
        }
        SweepViews(_avatarViews,
            _world.Players.Values.Where(p => p.Id != LocalPlayerId && p.Connected).Select(p => p.Id));
    }

    private void SyncAvatarsFromMeta()
    {
        if (_net.Meta is not { } meta) return;
        var seen = new List<int>();
        foreach (Godot.Collections.Dictionary entry in meta["players"].AsGodotArray())
        {
            int id = (int)entry["id"];
            if (id == LocalPlayerId || !(bool)entry["connected"]) continue;
            seen.Add(id);
            UpdateAvatarView(id,
                new Vector3((float)entry["x"], (float)entry["y"], (float)entry["z"]),
                (bool)entry["downed"], (string)entry["faction"]);
        }
        SweepViews(_avatarViews, seen);
    }

    private void UpdateAvatarView(int playerId, Vector3 pos, bool downed, string factionId)
    {
        if (!_avatarViews.TryGetValue(playerId, out var view))
        {
            view = new Node3D();
            var body = AssetLibrary.Instantiate($"hero_{factionId}", () => Placeholders.Hero(factionId));
            body.Name = "Body";
            view.AddChild(body);

            // Design ships a posed downed model per faction; falling back to
            // tipping the standing one over keeps the graybox readable.
            var floored = AssetLibrary.TryInstantiate($"hero_{factionId}_downed");
            if (floored is not null)
            {
                floored.Name = "Downed";
                floored.Visible = false;
                view.AddChild(floored);
            }

            AddChild(view);
            _avatarViews[playerId] = view;
        }
        // Smooth the 8 Hz meta rate.
        view.Position = view.Position.Lerp(pos, 0.35f);

        var upright = view.GetNode<Node3D>("Body");
        if (view.GetNodeOrNull<Node3D>("Downed") is { } pose)
        {
            upright.Visible = !downed;
            pose.Visible = downed;
        }
        else
        {
            upright.RotationDegrees = downed ? new Vector3(0, 0, 90) : Vector3.Zero;
        }
    }

    private static void SweepViews(Dictionary<int, Node3D> views, IEnumerable<int> liveIds)
    {
        var live = new HashSet<int>(liveIds);
        var stale = views.Keys.Where(id => !live.Contains(id)).ToList();
        foreach (var id in stale)
        {
            views[id].QueueFree();
            views.Remove(id);
        }
    }

    /// <summary>Status and damage as a modulation of whatever the model
    /// shipped with — never a replacement. Colours and the hp behaviour come
    /// from design's spec (docs/PALETTE.md §Statuses, §Enemies).</summary>
    private static void TintEnemy(Node3D view, float hpFraction, byte statusBits)
    {
        if (view is not TintableView tintable) return;

        bool chilled = (statusBits & (1 << (int)Channel.Movement)) != 0;
        bool burning = (statusBits & (1 << (int)Channel.Thermal)) != 0;
        bool marked = (statusBits & (1 << (int)Channel.Vulnerability)) != 0;
        // Shock and freeze share the control channel, so the packed snapshot
        // can't tell them apart; the overhead icons make the same call.
        bool controlled = (statusBits & (1 << (int)Channel.Control)) != 0;

        // Hard control reads over everything, then thermal, then movement.
        string? tint = controlled ? "freeze" : burning ? "burn" : chilled ? "chill" : null;
        Color? blend = tint is null ? null : UiTheme.Status(tint);

        // Burn glows; mark is emissive-only, so it never repaints a silhouette
        // and can coexist with a status tint.
        Color? emission = tint == "burn" ? UiTheme.Status("burn")
            : marked ? UiTheme.Status("mark")
            : null;

        tintable.Apply(blend, blend is null ? 0f : 0.7f,
            UiTheme.HpLow, 1f - hpFraction, emission);
    }

    /// <summary>Shows the state variant that matches the sim: shield bubble
    /// while a Warden still has one, dirt mound while a Mole is under.</summary>
    private static void UpdateEnemyStates(Node3D view, bool burrowed, float shieldFraction)
    {
        if (view.GetNodeOrNull<Node3D>("Shield") is { } shield)
            shield.Visible = shieldFraction > 0.01f;

        if (view.GetNodeOrNull<Node3D>("Burrowed") is not { } mound) return;
        mound.Visible = burrowed;
        if (view.GetNodeOrNull<Node3D>("Body") is { } body) body.Visible = !burrowed;
    }

    // =====================================================================
    // Level construction (graybox Foundry)
    // =====================================================================

    private void BuildLevel(MapDef map)
    {
        BuildEnvironment(map);

        // Ground slab — one collider, dressed with the 20 m terrain tiles.
        // Tiles are laid symmetrically so the slab is covered edge to edge;
        // an off-centre run leaves bare collider showing at one end.
        var ground = AddStaticBox(new Vector3(0, -0.5f, 0), new Vector3(110, 1, 80), new Color(0.35f, 0.38f, 0.4f), layer: 1);
        if (AssetLibrary.Has($"{map.Id}_terrain"))
        {
            MapKit.HideBox(ground);
            for (float x = -50f; x <= 50f; x += 20f)
                for (float z = -30f; z <= 30f; z += 20f)
                    MapKit.Prop(ground, $"{map.Id}_terrain", new Vector3(x, MapKit.GroundLocal(ground), z));
        }

        // Lane surfaces.
        foreach (var route in map.Routes)
        {
            bool air = route.Layer == EnemyLayer.Air;
            var color = air ? new Color(0.5f, 0.6f, 0.9f, 0.25f) : new Color(0.2f, 0.22f, 0.27f);
            for (int i = 0; i < route.Waypoints.Count - 1; i++)
            {
                var a = ToGd(route.Waypoints[i]);
                var b = ToGd(route.Waypoints[i + 1]);
                var mid = (a + b) * 0.5f + new Vector3(0, air ? 0f : 0.06f, 0);
                var box = AddStaticBox(mid, new Vector3((b - a).Length(), air ? 0.15f : 0.1f, air ? 1.2f : 3.4f), color, layer: 0, transparent: air);
                var horizontal = b - a;
                box.Rotation = new Vector3(0, Mathf.Atan2(-horizontal.Z, horizontal.X), 0);

                // The ground lane becomes real roadway. The air lane keeps its
                // faint ribbon: pylons are placed separately, standing on the
                // ground, because hanging them off a ribbon 9 m up put a row of
                // masts in the sky.
                if (!air)
                    MapKit.MountRun(box, $"{map.Id}_path_ground", (b - a).Length(), 4f,
                        alongX: true, MapKit.GroundLocal(box));
            }

            if (air) BuildAirLaneSupports(route);
        }

        BuildLaneMouths(map);

        // Sockets.
        foreach (var socket in map.Sockets)
        {
            var color = socket.Tag switch
            {
                SocketTag.Wall => new Color(0.45f, 0.6f, 0.85f),
                SocketTag.Trap => new Color(0.6f, 0.5f, 0.3f),
                _ => new Color(0.5f, 0.75f, 0.5f),
            };
            var body = new StaticBody3D { CollisionLayer = 1 << 3, CollisionMask = 0 };
            body.AddChild(new MeshInstance3D
            {
                Mesh = new CylinderMesh { TopRadius = 1.1f, BottomRadius = 1.1f, Height = 0.25f },
                MaterialOverride = new StandardMaterial3D
                {
                    AlbedoColor = color, EmissionEnabled = true, Emission = color * 0.4f,
                },
            });
            body.AddChild(new CollisionShape3D { Shape = new CylinderShape3D { Radius = 1.2f, Height = 1.2f } });
            body.Position = ToGd(socket.Pos) + new Vector3(0, 0.12f, 0);
            body.SetMeta("socket_id", socket.Id);
            AddChild(body);

            // Empty vs occupied are different models; RefreshSocketArt swaps
            // them as towers come and go.
            _socketBodies[socket.Id] = body;
            _socketTags[socket.Id] = socket.Tag;
            RefreshSocketArt(socket.Id, occupied: false);
        }

        if (map.Id == "foundry") BuildFoundryStructures();
        if (map.Id == "switchyard") BuildSwitchyardStructures();

        // Armory station.
        var armory = AddStaticBox(ToGd(map.ArmoryPos) + new Vector3(0, 1.25f, 0),
            new Vector3(2.5f, 2.5f, 2.5f), new Color(0.8f, 0.7f, 0.2f), layer: 1);
        armory.AddChild(MakeArea("armory", new BoxShape3D { Size = new Vector3(7, 4, 7) }));
        // Sits on the ground like every other kit piece and faces the spot
        // players spawn at. The +2.1 that used to be here was a guess at a
        // pivot offset and left the kiosk hanging in mid-air.
        MapKit.Mount(armory, "shared_armory_kiosk", MapKit.GroundLocal(armory),
            MapKit.YawTowards(ToGd(map.HeroSpawn) - ToGd(map.ArmoryPos)));
    }

    /// <summary>The gate enemies come out of and the core they are walking at.
    ///
    /// Both are oriented along the lane: a gate turned the wrong way reads as
    /// enemies walking out through a solid wall, which is exactly what it did
    /// before. Switchyard's two ground routes share a mouth and an end, so
    /// these are placed per distinct position — otherwise two portals sit in
    /// the same six metres of space fighting over the depth buffer.</summary>
    private void BuildLaneMouths(MapDef map)
    {
        var gates = new List<Vector3>();
        var cores = new List<Vector3>();

        foreach (var route in map.Routes)
        {
            if (route.Layer == EnemyLayer.Air) continue;   // the air lane has no gate
            if (route.Waypoints.Count < 2) continue;

            var start = ToGd(route.Waypoints[0]);
            var outbound = ToGd(route.Waypoints[1]) - start;
            if (!gates.Any(p => p.DistanceTo(start) < 4f))
            {
                gates.Add(start);
                MapKit.Prop(this, "shared_spawn_portal", start, MapKit.YawTowards(outbound));
            }

            var end = ToGd(route.Waypoints[^1]);
            var inbound = end - ToGd(route.Waypoints[^2]);
            if (!cores.Any(p => p.DistanceTo(end) < 4f))
            {
                cores.Add(end);
                // The core faces back up the lane, at the thing coming for it.
                MapKit.Prop(this, "shared_core", end, MapKit.YawTowards(-inbound));
            }
        }
        _laneMouths = gates.Concat(cores).ToList();
    }

    /// <summary>Masts under the air lane, standing on the ground, spaced along
    /// the strand. The mast is authored 9.66 m tall; the lane now climbs to 13
    /// or 15 across the middle of a map, so each one is stretched to meet the
    /// height it is holding up — which also makes the climb legible from the
    /// ground. Skipped where a mast would land on the lane or a socket.</summary>
    private void BuildAirLaneSupports(RouteDef route)
    {
        if (!AssetLibrary.Has("shared_airlane_pylon")) return;

        const float spacing = 13f;
        for (int i = 0; i < route.Waypoints.Count - 1; i++)
        {
            var a = ToGd(route.Waypoints[i]);
            var b = ToGd(route.Waypoints[i + 1]);
            float span = new Vector2(b.X - a.X, b.Z - a.Z).Length();
            int count = Mathf.Max(1, Mathf.FloorToInt(span / spacing));

            for (int step = 0; step <= count; step++)
            {
                if (i > 0 && step == 0) continue;              // shared corner
                var at = a.Lerp(b, count == 0 ? 0f : (float)step / count);
                var foot = new Vector3(at.X, 0, at.Z);
                if (Blocked(foot, 6f)) continue;
                const float mastHeight = 9.66f;
                MapKit.Prop(this, "shared_airlane_pylon", foot, 0f,
                    new Vector3(1f, Mathf.Max(1f, at.Y / mastHeight), 1f));
            }
        }
    }

    /// <summary>True when a spot is too close to something the player uses —
    /// the lane, a socket, a lane mouth, or the places players stand. Scenery
    /// that ignores this is how a map ends up with a girder through the
    /// roadway, or a gantry planted in the spawn yard.</summary>
    private bool Blocked(Vector3 at, float clearance)
    {
        // Sockets get their own, tighter clearance. A build pad is 2.3 m across
        // and what matters is not burying it — a crate eight metres away is
        // scenery, not an obstruction. Holding scenery to the full lane
        // clearance around 42 pads empties the map instead of dressing it.
        const float socketClearance = 4.5f;
        foreach (var socket in _map.Sockets)
            if (Flat(ToGd(socket.Pos)).DistanceTo(Flat(at)) < socketClearance) return true;

        foreach (var mouth in _laneMouths)
            if (Flat(mouth).DistanceTo(Flat(at)) < clearance + 3f) return true;

        // Player space: where they spawn, where they shop, and every station
        // the traversal graph expects them to be able to stand on.
        if (Flat(ToGd(_map.HeroSpawn)).DistanceTo(Flat(at)) < clearance + 6f) return true;
        if (Flat(ToGd(_map.ArmoryPos)).DistanceTo(Flat(at)) < clearance + 4f) return true;
        foreach (var station in _map.HeroStations)
            if (Flat(ToGd(station.Pos)).DistanceTo(Flat(at)) < clearance) return true;

        foreach (var route in _map.Routes)
        {
            if (route.Layer == EnemyLayer.Air) continue;
            for (int i = 0; i < route.Waypoints.Count - 1; i++)
                if (DistanceToSegment(Flat(at), Flat(ToGd(route.Waypoints[i])),
                        Flat(ToGd(route.Waypoints[i + 1]))) < clearance) return true;
        }
        return false;
    }

    private static Vector3 Flat(Vector3 v) => new(v.X, 0, v.Z);

    private static float DistanceToSegment(Vector3 point, Vector3 a, Vector3 b)
    {
        var ab = b - a;
        float lengthSq = ab.LengthSquared();
        if (lengthSq < 0.001f) return point.DistanceTo(a);
        float t = Mathf.Clamp((point - a).Dot(ab) / lengthSq, 0f, 1f);
        return point.DistanceTo(a + ab * t);
    }

    /// <summary>Tears down a sold structure and hands its socket back to the
    /// empty-pad art.</summary>
    private void ReleaseStructureView(int towerId)
    {
        if (!_towerViews.Remove(towerId, out var view)) return;
        string socketId = (string)view.GetMeta("socket_id", "");
        RemoveChild(view);
        view.QueueFree();
        if (socketId.Length > 0) RefreshSocketArt(socketId, occupied: false);
    }

    /// <summary>Shows the empty-socket marker or the occupied base plate. The
    /// pad is the only thing telling a player where they may build, so it has
    /// to change the moment something lands on it.</summary>
    private void RefreshSocketArt(string socketId, bool occupied)
    {
        if (!_socketBodies.TryGetValue(socketId, out var body) || !IsInstanceValid(body)) return;
        if (!_socketTags.TryGetValue(socketId, out var tag)) return;

        string family = tag switch
        {
            SocketTag.Wall => "wall",
            SocketTag.Trap => "trap",
            SocketTag.Barricade => "barricade",
            _ => "ground",
        };
        // Only ground and wall sockets have a distinct occupied plate.
        string asset = occupied && family is "ground" or "wall"
            ? $"socket_{family}_base"
            : $"socket_{family}_empty";
        if (!AssetLibrary.Has(asset)) return;

        if (body.GetNodeOrNull<Node3D>("SocketArt") is { } stale)
        {
            body.RemoveChild(stale);
            stale.QueueFree();
        }
        var art = MapKit.Mount(body, asset, MapKit.GroundLocal(body));
        if (art && body.GetChild(body.GetChildCount() - 1) is Node3D placed) placed.Name = "SocketArt";
    }

    /// <summary>Upper deck + traversal: ladder, zipline, launcher, vent, control
    /// point. All client-side geometry — the sim sees hero stations only.</summary>
    private void BuildFoundryStructures()
    {
        // Upper deck platform (walkable).
        var deck = AddStaticBox(new Vector3(2, 5.8f, -17), new Vector3(28, 0.4f, 10), new Color(0.45f, 0.48f, 0.55f), layer: 1);
        MapKit.MountRun(deck, "foundry_deck", 28f, 4f, alongX: true, MapKit.GroundLocal(deck));
        // Deck guard rail (visual).
        var rail = AddStaticBox(new Vector3(2, 6.6f, -12.2f), new Vector3(28, 1.0f, 0.2f), new Color(0.5f, 0.53f, 0.6f), layer: 0);
        MapKit.MountRun(rail, "foundry_deck_rail", 28f, 4f, alongX: true, MapKit.GroundLocal(rail) + 6.0f);
        // Deck support pillars.
        var pillarWest = AddStaticBox(new Vector3(-10, 2.9f, -17), new Vector3(1.2f, 5.8f, 1.2f), new Color(0.4f, 0.42f, 0.48f), layer: 1);
        MapKit.Mount(pillarWest, "foundry_pillar", MapKit.GroundLocal(pillarWest));
        var pillarEast = AddStaticBox(new Vector3(14, 2.9f, -17), new Vector3(1.2f, 5.8f, 1.2f), new Color(0.4f, 0.42f, 0.48f), layer: 1);
        MapKit.Mount(pillarEast, "foundry_pillar", MapKit.GroundLocal(pillarEast));

        // Gantry bridge: the deck reaches north across the lane's elbow. This
        // is what gives the upper level a job — sockets w10/w11 hang directly
        // over the corner enemies have to turn, and a hero standing here is
        // shooting down into it. Without the bridge the deck was a flat slab
        // whose sightlines the ground already had.
        var bridge = AddStaticBox(new Vector3(9, 5.8f, -6), new Vector3(4, 0.4f, 12), new Color(0.45f, 0.48f, 0.55f), layer: 1);
        MapKit.MountRun(bridge, "foundry_deck", 12f, 4f, alongX: false, MapKit.GroundLocal(bridge), 90f);
        foreach (float railX in new[] { -2.1f, 2.1f })
        {
            var bridgeRail = AddStaticBox(new Vector3(9 + railX, 6.6f, -6), new Vector3(0.2f, 1.0f, 12), new Color(0.5f, 0.53f, 0.6f), layer: 0);
            MapKit.MountRun(bridgeRail, "foundry_deck_rail", 12f, 4f, alongX: false, MapKit.GroundLocal(bridgeRail) + 6.0f, 90f);
        }
        var bridgeLeg = AddStaticBox(new Vector3(9, 2.9f, -1.2f), new Vector3(1.2f, 5.8f, 1.2f), new Color(0.4f, 0.42f, 0.48f), layer: 1);
        MapKit.Mount(bridgeLeg, "foundry_pillar", MapKit.GroundLocal(bridgeLeg));

        // Two ways up, so the deck isn't a one-way trip ending in the zipline.
        // The back ladder is safe; this one puts you on the bridge tip in the
        // middle of the fight.
        var bridgeLadder = AddStaticBox(new Vector3(9f, 3f, 0.5f), new Vector3(1.2f, 6f, 0.15f), new Color(0.7f, 0.6f, 0.3f), layer: 0);
        bridgeLadder.AddChild(MakeArea("ladder", new BoxShape3D { Size = new Vector3(1.6f, 6.4f, 1.4f) }));
        MapKit.Mount(bridgeLadder, "shared_ladder", MapKit.GroundLocal(bridgeLadder));

        // Ladder up the deck's south face. Moved off x=-8 so it no longer
        // shares a footprint with the vent tunnel's mouth.
        var ladderVisual = AddStaticBox(new Vector3(-11f, 3f, -12.4f), new Vector3(1.2f, 6f, 0.15f), new Color(0.7f, 0.6f, 0.3f), layer: 0);
        ladderVisual.AddChild(MakeArea("ladder", new BoxShape3D { Size = new Vector3(1.6f, 6.4f, 1.4f) }));
        MapKit.Mount(ladderVisual, "shared_ladder", MapKit.GroundLocal(ladderVisual));

        // Hero launcher: pad in the spawn yard that flings you onto the deck.
        var launcher = AddStaticBox(new Vector3(-14, 0.15f, -20), new Vector3(2.2f, 0.3f, 2.2f), new Color(0.9f, 0.5f, 0.9f), layer: 1);
        var launchArea = MakeArea("launcher", new BoxShape3D { Size = new Vector3(2.2f, 1.2f, 2.2f) });
        launchArea.SetMeta("launch_velocity", new Vector3(11f, 12f, 2.5f)); // arcs onto the deck
        launcher.AddChild(launchArea);
        MapKit.Mount(launcher, "shared_launcher_idle", MapKit.GroundLocal(launcher));

        // Zipline: deck east edge down to the core gate. Player rides on interact.
        var zipStart = new Vector3(14f, 6.8f, -14f);
        var zipEnd = new Vector3(28f, 1.6f, 2f);
        var zipAnchor = AddStaticBox(zipStart + new Vector3(0, 0.6f, 0), new Vector3(0.4f, 1.2f, 0.4f), new Color(0.85f, 0.8f, 0.4f), layer: 0);
        var mid = (zipStart + zipEnd) * 0.5f;
        var cable = AddStaticBox(mid, new Vector3((zipEnd - zipStart).Length(), 0.06f, 0.06f), new Color(0.8f, 0.8f, 0.8f), layer: 0);
        var d = zipEnd - zipStart;
        cable.Rotation = new Vector3(0, Mathf.Atan2(-d.Z, d.X), -Mathf.Atan2(d.Y, new Vector2(d.X, d.Z).Length()));
        var zipArea = MakeArea("zipline", new BoxShape3D { Size = new Vector3(2.5f, 2.5f, 2.5f) });
        zipArea.SetMeta("zip_end", zipEnd);
        zipAnchor.AddChild(zipArea);
        DressZipline(zipAnchor, cable, zipStart, zipEnd);

        // Vent tunnel: the hero-only shortcut from under the deck out to the
        // mid-lane station. Two things were wrong with it.
        //
        // It ran along x=0, which is the enemy lane's north-south leg — the
        // shortcut was laid on top of the road it was meant to bypass. It now
        // sits at x=-8 and surfaces beside the midLane hero station.
        //
        // And its bore was 1.275 m under a 1.8 m player capsule, so nobody
        // could ever walk through it. Clearance is 2.5 m now, with the model
        // stretched to match — a tube that reads the same at any height.
        // Pushed south so a third of its run is genuinely under the deck —
        // otherwise it reads as a shipping container abandoned in the yard
        // rather than a way through.
        const float bore = 2.5f;
        const float ventX = -8f, ventZ = -9f, ventLength = 14f;
        var ventRoof = AddStaticBox(new Vector3(ventX, bore + 0.13f, ventZ), new Vector3(3.0f, 0.25f, ventLength), new Color(0.3f, 0.32f, 0.36f), layer: 1);
        var ventWest = AddStaticBox(new Vector3(ventX - 1.8f, bore * 0.5f, ventZ), new Vector3(0.25f, bore, ventLength), new Color(0.3f, 0.32f, 0.36f), layer: 1);
        var ventEast = AddStaticBox(new Vector3(ventX + 1.8f, bore * 0.5f, ventZ), new Vector3(0.25f, bore, ventLength), new Color(0.3f, 0.32f, 0.36f), layer: 1);
        // One tunnel model covers all three colliders, so the other two just
        // stop drawing.
        if (MapKit.Mount(ventRoof, "foundry_vent_tunnel", MapKit.GroundLocal(ventRoof),
                scale: new Vector3(1f, bore / 1.4f, 1f)))
        {
            MapKit.HideBox(ventWest);
            MapKit.HideBox(ventEast);
        }
        // Grates at both mouths so the tube reads as something you enter.
        MapKit.Prop(this, "shared_vent_grate", new Vector3(ventX, 0, ventZ + ventLength * 0.5f), 0f);
        MapKit.Prop(this, "shared_vent_grate", new Vector3(ventX, 0, ventZ - ventLength * 0.5f), 180f);

        // Control point: stand on it to open the bonus wall socket sightline
        // (sim hookup lands at M2 with operated elements; sweepInert visual now).
        var controlPad = AddStaticBox(new Vector3(26, 0.1f, -6), new Vector3(3f, 0.2f, 3f), new Color(0.3f, 0.9f, 0.6f), layer: 1);
        controlPad.AddChild(MakeArea("controlPoint", new BoxShape3D { Size = new Vector3(3f, 1.5f, 3f) }));
        MapKit.Mount(controlPad, "shared_controlpoint_neutral", MapKit.GroundLocal(controlPad));

        // Perimeter sized to the lane: the ground route runs x −40 → +36, so
        // the wall stands two metres past each mouth and the gates read as
        // openings in it. Everything else is dressing at layer 0.
        BuildBoundary("foundry_wall_boundary", 42f, 30f);

        // Dressing clusters where the theme wants it — the melt floor west, the
        // gantry over the yard, pipe runs hugging the walls — and every piece
        // is clearance-checked against the lane and the sockets.
        DressProp("foundry_dress_crucible", new Vector3(-33, 0, 16), 20f);
        DressProp("foundry_dress_crucible", new Vector3(-26, 0, 22), -10f);
        DressProp("foundry_dress_gantry", new Vector3(24, 0, -26));
        DressProp("foundry_dress_gantry", new Vector3(-16, 0, 25), 90f);
        DressProp("foundry_dress_pipes", new Vector3(-38, 0, -18), 90f);
        DressProp("foundry_dress_pipes", new Vector3(38, 0, -16), -90f);
        DressProp("foundry_dress_pipes", new Vector3(30, 0, 24), 180f);
        DressProp("foundry_dress_lightrig", new Vector3(-36, 0, -14));
        DressProp("foundry_dress_lightrig", new Vector3(28, 0, 20));
        DressProp("foundry_dress_lightrig", new Vector3(6, 0, 26));
        DressProp("foundry_dress_steamvent", new Vector3(-38, 0, 12));
        DressProp("foundry_dress_steamvent", new Vector3(34, 0, -12));
        ScatterTerrain("foundry_terrain_scatter", 42f, 30f);
    }

    /// <summary>Places a prop only if it clears the lane, the sockets and the
    /// gates; logs it when it doesn't, so a bad coordinate is a line in the
    /// output rather than a girder standing in the roadway.</summary>
    private void DressProp(string asset, Vector3 at, float yaw = 0f)
    {
        if (Blocked(at, 7f))
        {
            GD.Print($"[map] dressing refused: {asset} at {at} is too close to play space");
            return;
        }
        MapKit.Prop(this, asset, at, yaw);
    }

    /// <summary>Anchor at each end plus the stretched cable. The graybox cable
    /// box is a rotated sliver, which the model would inherit, so the span is
    /// mounted in world space instead.</summary>
    private void DressZipline(Node3D anchorBody, Node3D cableBody, Vector3 from, Vector3 to)
    {
        if (!AssetLibrary.Has("shared_zipline_anchor")) return;
        MapKit.Mount(anchorBody, "shared_zipline_anchor", MapKit.GroundLocal(anchorBody) + from.Y);
        MapKit.Prop(this, "shared_zipline_anchor", new Vector3(to.X, to.Y, to.Z));
        MapKit.HideBox(cableBody);
        MapKit.MountSpan(this, "shared_zipline_cable", from, to);
        MapKit.Prop(this, "shared_zipline_trolley", from + (to - from) * 0.06f);
    }

    /// <summary>Boundary wall around the play area, laid so the run closes
    /// exactly on the corners rather than overshooting them.
    ///
    /// Segments are skipped where a lane mouth sits, which is what turns a gate
    /// into an opening in the wall instead of a shed parked in an empty field.
    /// Call this after BuildLaneMouths.</summary>
    private void BuildBoundary(string asset, float halfX, float halfZ)
    {
        if (!AssetLibrary.Has(asset)) return;
        const float segment = 10.8f;

        void Run(Vector3 from, Vector3 to, float yaw)
        {
            int count = Mathf.Max(1, Mathf.RoundToInt(from.DistanceTo(to) / segment));
            for (int i = 0; i < count; i++)
            {
                var at = from.Lerp(to, (i + 0.5f) / count);
                // Leave a hole where enemies come in or the core sits.
                if (_laneMouths.Any(m => Flat(m).DistanceTo(Flat(at)) < 7f)) continue;
                MapKit.Prop(this, asset, at, yaw);
            }
        }

        Run(new Vector3(-halfX, 0, -halfZ), new Vector3(halfX, 0, -halfZ), 0f);
        Run(new Vector3(-halfX, 0, halfZ), new Vector3(halfX, 0, halfZ), 180f);
        Run(new Vector3(-halfX, 0, -halfZ), new Vector3(-halfX, 0, halfZ), 90f);
        Run(new Vector3(halfX, 0, -halfZ), new Vector3(halfX, 0, halfZ), -90f);
    }

    /// <summary>Ground clutter on a fixed lattice — deterministic placement so
    /// two clients render the same world without syncing anything.
    ///
    /// Kept to the outfield and refused anywhere near the lane, a socket or a
    /// gate. Scattering into the playable middle is what made the first pass
    /// look like litter rather than a working yard.</summary>
    private void ScatterTerrain(string asset, float halfX, float halfZ)
    {
        if (!AssetLibrary.Has(asset)) return;
        int placed = 0, refused = 0;

        for (int i = 0; i < 22; i++)
        {
            // Fixed lattice, no RNG: the sim's streams stay untouched and every
            // client draws the identical world.
            float x = -halfX + 4f + (i * 31 % (int)(halfX * 2 - 8));
            float z = -halfZ + 4f + (i * 43 % (int)(halfZ * 2 - 8));
            var at = new Vector3(x, 0, z);
            if (Blocked(at, 9f)) { refused++; continue; }
            MapKit.Prop(this, asset, at, i * 47f % 360f);
            placed++;
        }
        GD.Print($"[map] scatter {asset}: {placed} placed, {refused} refused for clearance");
    }

    /// <summary>Three tiers: mid deck over the yard, catwalk over the air lane.
    /// Ladders chain tier to tier; the launcher skips straight to the deck.</summary>
    private void BuildSwitchyardStructures()
    {
        // Mid deck (y=5) with its wall sockets w1/w2.
        var midDeck = AddStaticBox(new Vector3(-6, 4.8f, -18), new Vector3(24, 0.4f, 8), new Color(0.45f, 0.48f, 0.55f), layer: 1);
        MapKit.MountRun(midDeck, "switchyard_middeck", 24f, 4f, alongX: true, MapKit.GroundLocal(midDeck));
        var midColumn = AddStaticBox(new Vector3(-6, 2.4f, -18), new Vector3(1.2f, 4.8f, 1.2f), new Color(0.4f, 0.42f, 0.48f), layer: 1);
        MapKit.Mount(midColumn, "switchyard_column", MapKit.GroundLocal(midColumn));

        // Upper catwalk (y=10) carrying w3/w4 over the air lane. Extended west
        // to x=-16 so its climb has somewhere to land clear of the lane.
        // Depth 8 rather than 5: w3 (z=6) and w4 (z=0) were both half a metre
        // off the edge, so a tower built on either hung in space.
        var catwalk = AddStaticBox(new Vector3(-1, 9.8f, 3), new Vector3(30, 0.4f, 8), new Color(0.5f, 0.52f, 0.6f), layer: 1);
        MapKit.MountRun(catwalk, "switchyard_catwalk", 30f, 4f, alongX: true, MapKit.GroundLocal(catwalk));
        foreach (float columnX in new[] { -14f, -6f, 12f })
        {
            var column = AddStaticBox(new Vector3(columnX, 4.9f, 3), new Vector3(1.2f, 9.8f, 1.2f), new Color(0.4f, 0.42f, 0.48f), layer: 1);
            MapKit.Mount(column, "switchyard_column", MapKit.GroundLocal(column));
        }

        // Ladders: yard → mid deck, and ground → catwalk.
        var ladder1 = AddStaticBox(new Vector3(-14f, 2.5f, -14.2f), new Vector3(1.2f, 5f, 0.15f), new Color(0.7f, 0.6f, 0.3f), layer: 0);
        ladder1.AddChild(MakeArea("ladder", new BoxShape3D { Size = new Vector3(1.6f, 5.6f, 1.4f) }));
        MapKit.Mount(ladder1, "shared_ladder", MapKit.GroundLocal(ladder1));

        // This one used to start at y=4.8 in open air: it was drawn as a
        // mid-deck-to-catwalk climb, but those two decks are eight metres apart
        // in Z and never touch. It now runs from the ground to the catwalk's
        // west end, which is a climb you can actually begin.
        var ladder2 = AddStaticBox(new Vector3(-13f, 5f, 0.6f), new Vector3(1.2f, 10f, 0.15f), new Color(0.7f, 0.6f, 0.3f), layer: 0);
        ladder2.AddChild(MakeArea("ladder", new BoxShape3D { Size = new Vector3(1.6f, 10.4f, 1.4f) }));
        MapKit.Mount(ladder2, "shared_ladder", MapKit.GroundLocal(ladder2));
        MapKit.Prop(ladder2, "shared_ladder", new Vector3(0, MapKit.GroundLocal(ladder2) + 6.5f, 0));

        // Launcher pad: spawn yard straight onto the mid deck.
        var launcher = AddStaticBox(new Vector3(8, 0.15f, -22), new Vector3(2.2f, 0.3f, 2.2f), new Color(0.9f, 0.5f, 0.9f), layer: 1);
        var launchArea = MakeArea("launcher", new BoxShape3D { Size = new Vector3(2.2f, 1.2f, 2.2f) });
        launchArea.SetMeta("launch_velocity", new Vector3(-8f, 11f, 3f));
        launcher.AddChild(launchArea);
        MapKit.Mount(launcher, "shared_launcher_idle", MapKit.GroundLocal(launcher));

        // Zipline: catwalk down to the core gate.
        var zipStart = new Vector3(12f, 10.6f, 4f);
        var zipEnd = new Vector3(32f, 1.6f, 5f);
        var anchor = AddStaticBox(zipStart + new Vector3(0, 0.5f, 0), new Vector3(0.4f, 1f, 0.4f), new Color(0.85f, 0.8f, 0.4f), layer: 0);
        var zipArea = MakeArea("zipline", new BoxShape3D { Size = new Vector3(2.5f, 2.5f, 2.5f) });
        zipArea.SetMeta("zip_end", zipEnd);
        anchor.AddChild(zipArea);
        if (AssetLibrary.Has("shared_zipline_anchor"))
        {
            MapKit.Mount(anchor, "shared_zipline_anchor", MapKit.GroundLocal(anchor) + zipStart.Y);
            MapKit.Prop(this, "shared_zipline_anchor", zipEnd);
            MapKit.MountSpan(this, "shared_zipline_cable", zipStart, zipEnd);
            MapKit.Prop(this, "shared_zipline_trolley", zipStart + (zipEnd - zipStart) * 0.06f);
        }

        // The freight cut is the map's whole lesson — the fast shortcut b1
        // closes — so the channel is laid *along* it rather than parked in the
        // middle of the yard where it means nothing.
        var shortcut = System.Array.Find(_map.Routes.ToArray(), r => r.Id == "groundShort");
        if (shortcut is not null && AssetLibrary.Has("switchyard_cut_channel"))
        {
            for (int i = 0; i < shortcut.Waypoints.Count - 1; i++)
            {
                var a = ToGd(shortcut.Waypoints[i]);
                var b = ToGd(shortcut.Waypoints[i + 1]);
                float span = (b - a).Length();
                int count = Mathf.Max(1, Mathf.RoundToInt(span / 9.6f));
                for (int s = 0; s < count; s++)
                {
                    var at = a.Lerp(b, (s + 0.5f) / count);
                    MapKit.Prop(this, "switchyard_cut_channel", new Vector3(at.X, 0, at.Z),
                        MapKit.YawTowards(b - a));
                }
            }
        }

        // Retaining walls flank the switchbacks; boundary matches the lane,
        // which runs x −45 → +40.
        DressProp("switchyard_retainingwall", new Vector3(-33, 0, 20), 0f);
        DressProp("switchyard_retainingwall", new Vector3(33, 0, -22), 180f);
        BuildBoundary("foundry_wall_boundary", 47f, 32f);

        DressProp("switchyard_dress_railcar", new Vector3(-36, 0, 22));
        DressProp("switchyard_dress_railcar", new Vector3(26, 0, 26), 8f);
        DressProp("switchyard_dress_railcar", new Vector3(-30, 0, -26), 4f);
        DressProp("switchyard_dress_container", new Vector3(-42, 0, -22), 30f);
        DressProp("switchyard_dress_container", new Vector3(44, 0, -2), -15f);
        DressProp("switchyard_dress_container", new Vector3(20, 0, 27), 60f);
        DressProp("switchyard_dress_signaltower", new Vector3(-24, 0, 27));
        DressProp("switchyard_dress_signaltower", new Vector3(36, 0, -12));
        DressProp("switchyard_dress_buffer", new Vector3(44, 0, -18), -90f);
        DressProp("switchyard_dress_buffer", new Vector3(-44, 0, 12), 90f);
        ScatterTerrain("switchyard_terrain_scatter", 47f, 32f);
    }

    private static Area3D MakeArea(string kind, Shape3D shape)
    {
        var area = new Area3D { CollisionLayer = 1 << 5, CollisionMask = 1 << 4 };
        area.AddChild(new CollisionShape3D { Shape = shape });
        area.SetMeta("kind", kind);
        return area;
    }

    private void BuildEnvironment(MapDef map)
    {
        var sun = new DirectionalLight3D { ShadowEnabled = true };
        sun.RotationDegrees = new Vector3(-55, -30, 0);
        AddChild(sun);
        AddChild(new WorldEnvironment
        {
            Environment = new Godot.Environment
            {
                BackgroundMode = Godot.Environment.BGMode.Sky,
                Sky = new Sky { SkyMaterial = new ProceduralSkyMaterial() },
                AmbientLightSource = Godot.Environment.AmbientSource.Sky,
                AmbientLightEnergy = 0.6f,
            },
        });

        // Design's skybox is real geometry, not a cubemap — a 400 m dome that
        // gives each map its own horizon. The procedural sky stays underneath
        // as the ambient light source.
        if (MapKit.Prop(this, $"{map.Id}_skybox", Vector3.Zero) is { } dome)
            MapKit.NoShadow(dome);
    }

    private StaticBody3D AddStaticBox(Vector3 position, Vector3 size, Color color, uint layer, bool transparent = false)
    {
        var body = new StaticBody3D { CollisionLayer = layer, CollisionMask = 0 };
        var mat = new StandardMaterial3D { AlbedoColor = color };
        if (transparent) mat.Transparency = BaseMaterial3D.TransparencyEnum.Alpha;
        body.AddChild(new MeshInstance3D { Mesh = new BoxMesh { Size = size }, MaterialOverride = mat });
        if (layer != 0)
            body.AddChild(new CollisionShape3D { Shape = new BoxShape3D { Size = size } });
        body.Position = position;
        AddChild(body);
        return body;
    }

    private Node3D SpawnEnemyView(int enemyId, string defId)
    {
        var root = new TintableView();
        float scale = Placeholders.EnemyScale(defId);

        // The Mole's default state is named in the brief as its own model
        // because it has a second one; every other enemy is just enemy_<id>.
        string bodyAsset = defId == "mole" ? "enemy_mole_surfaced" : $"enemy_{defId}";
        var body = AssetLibrary.Instantiate(bodyAsset, () => Placeholders.Enemy(defId));
        body.Name = "Body";
        root.AddChild(body);

        // State variants ship as their own models (brief §3): the shield is a
        // separate mesh so it can pop and regrow, and the Mole swaps silhouette
        // rather than vanishing. Absent art, both degrade to a graybox.
        if (defId == "warden")
        {
            var shield = AssetLibrary.Instantiate("enemy_warden_shield", () => ShieldBubble(scale));
            shield.Name = "Shield";
            root.AddChild(shield);
        }
        else if (defId == "mole")
        {
            var burrowed = AssetLibrary.Instantiate("enemy_mole_burrowed", () => DirtMound(scale));
            burrowed.Name = "Burrowed";
            burrowed.Visible = false;
            root.AddChild(burrowed);
        }

        var area = new Area3D { CollisionLayer = 1 << 1, CollisionMask = 0 };
        area.AddChild(new CollisionShape3D
        {
            Shape = new CapsuleShape3D { Radius = 0.5f * scale, Height = 1.7f * scale },
            Position = new Vector3(0, 0.8f * scale, 0),
        });
        area.SetMeta("enemy_id", enemyId);
        root.AddChild(area);

        // Overheads sit just above the silhouette; scale drives the offset so a
        // Monolith's bar doesn't sit inside its chest.
        root.SetMeta("head_height", 1.9f * scale);
        root.SetMeta("def_id", defId);

        AddChild(root);
        root.Prepare();
        return root;
    }

    /// <summary>Graybox shield bubble — a separate node so it pops and regrows
    /// independently, exactly as enemy_warden_shield.glb will.</summary>
    private static Node3D ShieldBubble(float scale)
    {
        var root = new Node3D();
        root.AddChild(new MeshInstance3D
        {
            Mesh = new SphereMesh { Radius = 0.95f * scale, Height = 1.9f * scale },
            MaterialOverride = new StandardMaterial3D
            {
                AlbedoColor = new Color(0.35f, 0.85f, 0.95f, 0.30f),
                Transparency = BaseMaterial3D.TransparencyEnum.Alpha,
                EmissionEnabled = true,
                Emission = new Color(0.30f, 0.75f, 0.95f),
            },
            Position = new Vector3(0, 0.85f * scale, 0),
        });
        return root;
    }

    private static Node3D DirtMound(float scale)
    {
        var root = new Node3D();
        root.AddChild(new MeshInstance3D
        {
            Mesh = new CylinderMesh { TopRadius = 0.1f, BottomRadius = 0.85f * scale, Height = 0.4f * scale },
            MaterialOverride = new StandardMaterial3D { AlbedoColor = new Color(0.38f, 0.29f, 0.20f) },
            Position = new Vector3(0, 0.2f * scale, 0),
        });
        return root;
    }

    /// <summary>Each tower's round is its own asset (proj_lance_bolt,
    /// proj_nova_shell, …) so a Nova shell never reads as a Lance bolt.</summary>
    private Node3D SpawnProjectileView(string towerDefId)
    {
        string asset = towerDefId switch
        {
            "nova" => "proj_nova_shell",
            "arc" => "proj_arc_beam",
            "skywatch" => "proj_skywatch_bolt",
            "filament" => "proj_filament_beam",
            _ => "proj_lance_bolt",
        };

        var root = new Node3D();
        root.AddChild(AssetLibrary.Instantiate(asset, () => Placeholders.Projectile(towerDefId)));
        AddChild(root);
        return root;
    }

    // =====================================================================
    // UI construction (M2.5)
    // =====================================================================

    private void BuildUi()
    {
        _hud = new HudRoot { Name = "Hud" };
        AddChild(_hud);

        _screens = new MatchScreens { Name = "Screens" };
        AddChild(_screens);
        _screens.OnResume = CloseSystemMenu;
        _screens.OnLeave = () => GetTree().Quit();
        _screens.OnSave = SaveGame;
        _screens.OnLoad = LoadGame;
        _screens.OnToggleDamageNumbers = on =>
        {
            _markers.ShowDamageNumbers = on;
            _profile.ShowDamageNumbers = on;
            _profile.Save();
        };

        // Interactive surfaces live above the HUD and capture the mouse when open.
        _overlay = new CanvasLayer { Name = "Overlay", Layer = 3 };
        AddChild(_overlay);

        _wheel = new BuildWheel { Name = "BuildWheel" };
        _overlay.AddChild(_wheel);

        _upgrade = new UpgradePanel { Name = "UpgradePanel" };
        _overlay.AddChild(_upgrade);

        _armory = new ArmoryScreen { Name = "Armory" };
        _armory.Submit = Submit;
        _armory.RecraftBlueprint = RecraftBlueprint;
        _armory.HasBlueprint = weaponId => _profile.BlueprintSlots.ContainsKey(weaponId);
        _overlay.AddChild(_armory);

        _lobby = new LobbyScreen { Name = "Lobby" };
        _overlay.AddChild(_lobby);
        _lobby.Build(_profile);
        _lobby.OnSolo = faction => { _map = _lobby.SelectedMap; _playerName = _lobby.PlayerName; StartSolo(faction); };
        _lobby.OnHost = faction => { _map = _lobby.SelectedMap; _playerName = _lobby.PlayerName; StartHost(faction); };
        _lobby.OnJoin = (address, faction) => { _playerName = _lobby.PlayerName; StartClient(address, faction); };

        // World-space marker layer.
        _markers = new WorldMarkers { Name = "Markers" };
        AddChild(_markers);

        _ghost = new BuildGhost { Name = "BuildGhost" };
        AddChild(_ghost);
    }

    // =====================================================================
    // UI entry points called by Player
    // =====================================================================

    /// <summary>True while any surface owns the mouse — Player suspends look
    /// and fire while these are up.</summary>
    public bool UiCapturesMouse => _armory.IsOpen || _screens.PauseOpen;

    public bool WheelOpen => _wheel.IsOpen;
    public bool UpgradeOpen => _upgrade.IsOpen;

    public void OpenBuildWheel(string socketId)
    {
        var socket = _map.Sockets.FirstOrDefault(s => s.Id == socketId);
        if (socket is null || _view.SocketOccupied(socketId)) return;
        _wheel.Open(socket, _view);
    }

    public void SteerWheel(Vector2 relative)
    {
        _wheel.Steer(relative);
        UpdateGhost();
    }

    public void WheelSelect(int index)
    {
        _wheel.SelectIndex(index);
        UpdateGhost();
    }

    private void UpdateGhost()
    {
        if (!_wheel.IsOpen || _wheel.Selection is not { } option)
        {
            _ghost.Hide3D();
            return;
        }
        var socket = _map.Sockets.FirstOrDefault(s => s.Id == _wheel.SocketId);
        if (socket is null) { _ghost.Hide3D(); return; }
        _ghost.Show(ToGd(socket.Pos), option.DefId, _wheel.CanAfford(option));
    }

    /// <summary>Release: build the highlighted option (the sim still has final
    /// say — its refusal comes back as an event and lands on the wheel).</summary>
    public void ConfirmBuildWheel()
    {
        if (_wheel.Selection is { } option)
            Submit(new Command.PlaceTower(LocalPlayerId, option.DefId, _wheel.SocketId));
        _wheel.Close();
        _ghost.Hide3D();
    }

    public void CancelBuildWheel()
    {
        _wheel.Close();
        _ghost.Hide3D();
    }

    public void OpenUpgradePanel(string socketId)
    {
        if (_view.AtSocket(socketId) is not { } structure) return;
        _upgrade.Open(structure, _view);
    }

    public void UpgradeKey(int oneBased)
    {
        int path = _upgrade.PathForKey(oneBased);
        if (path >= 0 && _upgrade.TargetId >= 0)
            Submit(new Command.UpgradeTower(LocalPlayerId, _upgrade.TargetId, path));
    }

    public void TickUpgradeSell(double delta, bool held)
    {
        // The panel's Upgrade buttons raise intent; number keys still work.
        if (_upgrade.UpgradeRequested >= 0 && _upgrade.TargetId >= 0)
        {
            Submit(new Command.UpgradeTower(LocalPlayerId, _upgrade.TargetId, _upgrade.UpgradeRequested));
            _upgrade.ConsumeUpgrade();
        }

        _upgrade.TickSellHold(delta, held);
        if (_upgrade.SellRequested && _upgrade.TargetId >= 0)
        {
            Submit(new Command.SellTower(LocalPlayerId, _upgrade.TargetId));
            _upgrade.ConsumeSell();
            _upgrade.Close();
        }
    }

    public void CloseUpgradePanel() => _upgrade.Close();

    public void ToggleArmory()
    {
        // The armory is a full screen in design's set, not an overlay — the
        // in-match HUD stands down while it is up, or the wave cluster and the
        // vitals dock show through its backdrop.
        if (_armory.IsOpen)
        {
            _armory.Close();
            _hud.Visible = true;
            Input.MouseMode = Input.MouseModeEnum.Captured;
        }
        else
        {
            _armory.Open(_view);
            _hud.Visible = false;
            Input.MouseMode = Input.MouseModeEnum.Visible;
        }
    }

    public void ToggleSystemMenu()
    {
        if (_armory.IsOpen) { ToggleArmory(); return; }
        if (_screens.PauseOpen) CloseSystemMenu();
        else
        {
            _screens.ShowPause();
            Input.MouseMode = Input.MouseModeEnum.Visible;
            // Solo pauses outright; in multiplayer the world keeps running and
            // the menu says so.
            if (Mode == RunMode.Solo) _paused = true;
        }
    }

    private void CloseSystemMenu()
    {
        _screens.HidePause();
        _paused = false;
        Input.MouseMode = Input.MouseModeEnum.Captured;
    }

    /// <summary>Aim-context feedback: what would my next shot do to this target?</summary>
    public void ReportAim(int enemyId)
    {
        if (enemyId < 0) { _hud.SetCrosshair(CrosshairState.Neutral); return; }

        var enemy = _world?.Enemies.FirstOrDefault(e => e.Id == enemyId);
        if (enemy is not null)
        {
            var def = Enemies.All[enemy.DefId];
            if (enemy.Burrowed) _hud.SetCrosshair(CrosshairState.Burrowed);
            else if (enemy.Shield > 0f) _hud.SetCrosshair(CrosshairState.Shielded);
            else if (def.FrontArmorArcDegrees > 0f || def.FlatArmor > 0f)
                _hud.SetCrosshair(CrosshairState.Armored);
            else _hud.SetCrosshair(CrosshairState.Neutral);
            return;
        }

        // Client: the snapshot's status bits are what we have.
        if (_lastSnapshotBits.TryGetValue(enemyId, out var snap))
        {
            if (snap.Burrowed) _hud.SetCrosshair(CrosshairState.Burrowed);
            else if (snap.Shielded) _hud.SetCrosshair(CrosshairState.Shielded);
            else _hud.SetCrosshair(CrosshairState.Neutral);
        }
    }

    public void SetHint(string hint) => _hint = hint;

    private void BankLocalXp()
    {
        if (_xpBanked) return;
        int xp = _view.Local?.MatchXp ?? 0;
        if (xp <= 0) return;

        _xpBanked = true;
        _bankedXp = xp;
        int before = _profile.LevelFor(_factionId);
        _profile.BankXp(_factionId, xp);
        int after = _profile.LevelFor(_factionId);
        Post(after > before
            ? $"{_factionId} reached level {after}!"
            : $"+{xp} {_factionId} xp banked", UiTheme.Good);
    }

    /// <summary>Blueprint recraft: replay the saved build for a weapon as craft
    /// commands (the sim refuses whatever scrap can't cover).</summary>
    public void RecraftBlueprint(string weaponId)
    {
        if (_profile.BlueprintSlots.TryGetValue(weaponId, out var slots))
            foreach (var attachmentId in slots.Values)
                Submit(new Command.CraftAttachment(LocalPlayerId, weaponId, attachmentId));
        if (_profile.BlueprintAmmo.TryGetValue(weaponId, out var ammoId))
            Submit(new Command.SelectAmmo(LocalPlayerId, weaponId, ammoId));
    }

    /// <summary>Event feed line. Kept named Toast for call sites that predate
    /// the feed; Post is the preferred name.</summary>
    public void Toast(string message) => Post(message);

    public void Post(string message, Color? color = null)
    {
        if (Mode == RunMode.Dedicated) { GD.Print($"[match] {message}"); return; }
        _hud?.Post(message, color);
    }

    public static Vector3 ToGd(Vec3 v) => new(v.X, v.Y, v.Z);
}
