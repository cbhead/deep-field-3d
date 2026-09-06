using Godot;
using System.Collections.Generic;
using System.Linq;
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
    private readonly Dictionary<int, Node3D> _avatarViews = new();

    private Label _hudLabel = null!;
    private Label _toastLabel = null!;
    private Label _capturePrompt = null!;
    private ColorRect _crosshair = null!;
    private Control _lobbyUi = null!;
    private double _toastTimer, _hitFlashTimer;

    public int LocalPlayerId => _net.LocalPlayerId;

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

        BuildHud();
        BuildLobby();

        // Headless smoke-test seam: --join <ip> connects straight from boot.
        for (int i = 0; i < args.Length - 1; i++)
        {
            if (args[i] == "--join")
            {
                _playerName = "smoke";
                StartClient(args[i + 1], "forge");
                break;
            }
        }
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
        _world = new SimWorld(FreshSeed(), _map) { WaitForPlayers = true };
        _net.HostServer(port, _world);
        _net.ServerEnqueue = c => _world.Enqueue(c);

        _info = new InfoServer { Name = "Info" };
        AddChild(_info);
        _info.StatusProvider = () => new Godot.Collections.Dictionary
        {
            ["app"] = "deepfield-3d",
            ["map"] = _world.Map.Id,
            ["players"] = _world.ConnectedPlayerCount,
            ["wave"] = _world.WaveIndex + 1,
            ["phase"] = _world.Phase.ToString(),
        };
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
        _info.StatusProvider = () => new Godot.Collections.Dictionary
        {
            ["app"] = "deepfield-3d",
            ["map"] = _world!.Map.Id,
            ["players"] = _world.ConnectedPlayerCount,
            ["wave"] = _world.WaveIndex + 1,
            ["phase"] = _world.Phase.ToString(),
        };
        _info.Start(Protocol.DefaultPort);
        Toast(_info.TailscaleIp is { } ip
            ? $"hosting — invite: {ip}:{Protocol.DefaultPort}"
            : "hosting (no tailscale ip found — friends need your LAN ip)");
    }

    public void StartClient(string address, string faction)
    {
        Mode = RunMode.Client;
        _factionId = faction;
        _lobbyUi.Visible = false;
        Toast($"connecting to {address}…");

        // "100.x.x.x" or "100.x.x.x:8791" both work.
        int port = Protocol.DefaultPort;
        if (address.Contains(':'))
        {
            var parts = address.Split(':');
            address = parts[0];
            if (int.TryParse(parts[1], out int parsed)) port = parsed;
        }

        Multiplayer.ConnectedToServer += () =>
            _net.SendHello(_playerName, _factionId, _profile.LevelFor(_factionId));
        Multiplayer.ConnectionFailed += () => Toast("connection failed");
        _net.JoinServer(address, port);
    }

    private void BeginLocalWorld(string faction)
    {
        _factionId = faction;
        _lobbyUi.Visible = false;
        _world = new SimWorld(FreshSeed(), _map);
        _world.Enqueue(new Command.Join(1, _playerName, faction, _profile.LevelFor(faction)));
        BuildLevel(_map);
        SpawnLocalPlayer();
    }

    private void SpawnLocalPlayer()
    {
        _player = new Player { Name = "Player" };
        AddChild(_player);
        _player.GlobalPosition = ToGd(_map.HeroSpawn) + new Vector3(0, 1.2f, 0);
        _player.RotationDegrees = new Vector3(0, 180, 0);
    }

    private static uint FreshSeed() => (uint)(Time.GetTicksMsec() & 0xFFFFFFFF) ^ 0x9E3779B9u;

    // =====================================================================
    // Frame loop
    // =====================================================================

    public override void _Process(double delta)
    {
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

        if (Mode != RunMode.Dedicated)
            UpdateHud(delta);
    }

    private void StepAuthoritative(double delta)
    {
        _tickEvents.Clear();
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
            foreach (var tower in _shadow.Towers)
                _towerViews[tower.Id] = SpawnTowerView(ToGd(tower.Pos));
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
        return world.Towers.Any(t => t.SocketId == socketId);
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
                    OnTowerPlaced(placed.TowerId, placed.SocketId);
                    break;
                case SimEvent.TowerSold sold:
                    if (_towerViews.Remove(sold.TowerId, out var view)) view.QueueFree();
                    break;
                case SimEvent.BuildRejected rejected: Toast($"build rejected: {rejected.Reason}"); break;
                case SimEvent.UpgradeRejected rejected: Toast($"upgrade rejected: {rejected.Reason}"); break;
                case SimEvent.PurchaseRejected rejected: Toast($"armory: {rejected.Reason}"); break;
                case SimEvent.JoinRejected rejected: Toast($"join rejected: {rejected.Reason}"); break;
                case SimEvent.WaveStarted started: Toast($"wave {started.WaveIndex + 1} — {started.EnemyCount} inbound"); break;
                case SimEvent.EnemyLeaked: Toast("breach! core hit"); break;
                case SimEvent.ReactionTriggered: Toast("THERMAL SHOCK"); break;
                case SimEvent.PlayerDowned downed when downed.PlayerId == LocalPlayerId: Toast("DOWN — a teammate can revive you"); break;
                case SimEvent.MatchEnded ended:
                    Toast(ended.Victory ? "VICTORY" : "DEFEAT");
                    BankLocalXp();
                    break;
                case SimEvent.AttachmentCrafted crafted when crafted.PlayerId == LocalPlayerId:
                    _profile.RecordAttachment(crafted.WeaponId,
                        Attachments.All[crafted.AttachmentId].Slot.ToString(), crafted.AttachmentId);
                    Toast($"crafted {crafted.AttachmentId}");
                    break;
                case SimEvent.AmmoSelected ammo when ammo.PlayerId == LocalPlayerId:
                    _profile.RecordAmmo(ammo.WeaponId, ammo.AmmoId);
                    Toast($"ammo: {ammo.AmmoId}");
                    break;
                case SimEvent.CraftRejected rejected when rejected.PlayerId == LocalPlayerId:
                    Toast($"craft: {rejected.Reason}");
                    break;
                case SimEvent.EnemyDamaged damaged when damaged.Source == $"player{LocalPlayerId}":
                    _hitFlashTimer = 0.12; break;
                case SimEvent.EnemyDied died when died.Source == $"player{LocalPlayerId}":
                    _hitFlashTimer = 0.3; Toast($"+{died.Bounty} credits"); break;
            }
        }
    }

    private void HandleEventLine(string line)
    {
        var p = line.Split(' ');
        if (p.Length < 2) return;
        switch (p[1])
        {
            case "towerPlaced": OnTowerPlaced(int.Parse(p[2]), p[4]); break;
            case "towerSold":
                if (_towerViews.Remove(int.Parse(p[2]), out var view)) view.QueueFree();
                break;
            case "buildRejected": Toast($"build rejected: {p[5]}"); break;
            case "waveStarted": Toast($"wave {int.Parse(p[2]) + 1} — {p[3]} inbound"); break;
            case "enemyLeaked": Toast("breach! core hit"); break;
            case "reaction": Toast("THERMAL SHOCK"); break;
            case "playerDowned" when int.Parse(p[2]) == LocalPlayerId: Toast("DOWN — a teammate can revive you"); break;
            case "matchEnded": Toast(p[2] == "victory" ? "VICTORY" : "DEFEAT"); BankLocalXp(); break;
            case "attachmentCrafted" when int.Parse(p[2]) == LocalPlayerId:
                _profile.RecordAttachment(p[3], Attachments.All[p[4]].Slot.ToString(), p[4]);
                Toast($"crafted {p[4]}");
                break;
            case "ammoSelected" when int.Parse(p[2]) == LocalPlayerId:
                _profile.RecordAmmo(p[3], p[4]);
                Toast($"ammo: {p[4]}");
                break;
            case "enemyDamaged" when p[4] == $"player{LocalPlayerId}": _hitFlashTimer = 0.12; break;
            case "enemyDied" when p[5] == $"player{LocalPlayerId}": _hitFlashTimer = 0.3; break;
        }
    }

    private void OnTowerPlaced(int towerId, string socketId)
    {
        var socket = _map.Sockets.FirstOrDefault(s => s.Id == socketId);
        if (socket is null) return;
        var view = SpawnTowerView(ToGd(socket.Pos));
        view.SetMeta("socket_id", socketId);
        _towerViews[towerId] = view;
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
            TintEnemy(view, enemy.Hp / enemy.MaxHp, Protocol.PackStatusBits(enemy));
        }
        SweepViews(_enemyViews, _world.Enemies.Select(e => e.Id));
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
        }
        SweepViews(_enemyViews, newest.Enemies.Select(s => s.Id));
    }

    private void SyncProjectileViews()
    {
        foreach (var projectile in _world!.Projectiles)
        {
            if (!_projectileViews.TryGetValue(projectile.Id, out var view))
            {
                view = SpawnProjectileView();
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
            UpdateAvatarView(p.Id, new Vector3(p.Pos.X, p.Pos.Y, p.Pos.Z), p.Downed);
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
                (bool)entry["downed"]);
        }
        SweepViews(_avatarViews, seen);
    }

    private void UpdateAvatarView(int playerId, Vector3 pos, bool downed)
    {
        if (!_avatarViews.TryGetValue(playerId, out var view))
        {
            view = new Node3D();
            view.AddChild(new MeshInstance3D
            {
                Name = "Mesh",
                Mesh = new CapsuleMesh { Radius = 0.4f, Height = 1.8f },
                MaterialOverride = new StandardMaterial3D { AlbedoColor = new Color(0.3f, 0.8f, 0.9f) },
                Position = new Vector3(0, 0.9f, 0),
            });
            AddChild(view);
            _avatarViews[playerId] = view;
        }
        // Smooth the 8 Hz meta rate.
        view.Position = view.Position.Lerp(pos, 0.35f);
        var mesh = view.GetNode<MeshInstance3D>("Mesh");
        mesh.RotationDegrees = downed ? new Vector3(0, 0, 90) : Vector3.Zero;
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

    private static void TintEnemy(Node3D view, float hpFraction, byte statusBits)
    {
        var mesh = view.GetNode<MeshInstance3D>("Mesh");
        if (mesh.MaterialOverride is not StandardMaterial3D mat) return;

        bool chilled = (statusBits & (1 << (int)Channel.Movement)) != 0;
        bool burning = (statusBits & (1 << (int)Channel.Thermal)) != 0;
        bool marked = (statusBits & (1 << (int)Channel.Vulnerability)) != 0;

        // Status tint beats hp tint (shader pass replaces this at the art pass).
        if (burning) mat.AlbedoColor = new Color(1f, 0.45f, 0.1f);
        else if (chilled) mat.AlbedoColor = new Color(0.5f, 0.75f, 1f);
        else mat.AlbedoColor = new Color(0.9f, 0.25f + 0.55f * hpFraction, 0.2f + 0.5f * hpFraction);

        mat.EmissionEnabled = marked;
        if (marked) mat.Emission = new Color(1f, 0.9f, 0.2f);
    }

    // =====================================================================
    // Level construction (graybox Foundry)
    // =====================================================================

    private void BuildLevel(MapDef map)
    {
        BuildEnvironment();

        // Ground slab.
        AddStaticBox(new Vector3(0, -0.5f, 0), new Vector3(110, 1, 80), new Color(0.35f, 0.38f, 0.4f), layer: 1);

        // Route ribbons (ground solid, air translucent).
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
            }
            // Spawn + goal markers per route.
            AddStaticBox(ToGd(route.Waypoints[0]) + new Vector3(0, 1.5f, 0), new Vector3(3f, 3f, 3f), new Color(0.75f, 0.45f, 0.15f), layer: 0);
            AddStaticBox(ToGd(route.Waypoints[^1]) + new Vector3(0, 1.5f, 0), new Vector3(3f, 3f, 3f), new Color(0.2f, 0.55f, 0.85f), layer: 0);
        }

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
        }

        if (map.Id == "foundry") BuildFoundryStructures();

        // Armory station.
        var armory = AddStaticBox(ToGd(map.ArmoryPos) + new Vector3(0, 1.25f, 0),
            new Vector3(2.5f, 2.5f, 2.5f), new Color(0.8f, 0.7f, 0.2f), layer: 1);
        armory.AddChild(MakeArea("armory", new BoxShape3D { Size = new Vector3(7, 4, 7) }));
    }

    /// <summary>Upper deck + traversal: ladder, zipline, launcher, vent, control
    /// point. All client-side geometry — the sim sees hero stations only.</summary>
    private void BuildFoundryStructures()
    {
        // Upper deck platform (walkable).
        AddStaticBox(new Vector3(2, 5.8f, -17), new Vector3(28, 0.4f, 10), new Color(0.45f, 0.48f, 0.55f), layer: 1);
        // Deck guard rail (visual).
        AddStaticBox(new Vector3(2, 6.6f, -12.2f), new Vector3(28, 1.0f, 0.2f), new Color(0.5f, 0.53f, 0.6f), layer: 0);
        // Deck support pillars.
        AddStaticBox(new Vector3(-10, 2.9f, -17), new Vector3(1.2f, 5.8f, 1.2f), new Color(0.4f, 0.42f, 0.48f), layer: 1);
        AddStaticBox(new Vector3(14, 2.9f, -17), new Vector3(1.2f, 5.8f, 1.2f), new Color(0.4f, 0.42f, 0.48f), layer: 1);

        // Ladder up the deck's south face: an Area3D volume Player climbs inside.
        var ladderVisual = AddStaticBox(new Vector3(-8f, 3f, -12.4f), new Vector3(1.2f, 6f, 0.15f), new Color(0.7f, 0.6f, 0.3f), layer: 0);
        ladderVisual.AddChild(MakeArea("ladder", new BoxShape3D { Size = new Vector3(1.6f, 6.4f, 1.4f) }));

        // Hero launcher: pad in the spawn yard that flings you onto the deck.
        var launcher = AddStaticBox(new Vector3(-14, 0.15f, -20), new Vector3(2.2f, 0.3f, 2.2f), new Color(0.9f, 0.5f, 0.9f), layer: 1);
        var launchArea = MakeArea("launcher", new BoxShape3D { Size = new Vector3(2.2f, 1.2f, 2.2f) });
        launchArea.SetMeta("launch_velocity", new Vector3(11f, 12f, 2.5f)); // arcs onto the deck
        launcher.AddChild(launchArea);

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

        // Vent tunnel under the deck: spawn yard → mid-lane shortcut (hero-only).
        AddStaticBox(new Vector3(0, 1.4f, -6.5f), new Vector3(3.0f, 0.25f, 14f), new Color(0.3f, 0.32f, 0.36f), layer: 1); // tunnel roof
        AddStaticBox(new Vector3(-1.8f, 0.7f, -6.5f), new Vector3(0.25f, 1.4f, 14f), new Color(0.3f, 0.32f, 0.36f), layer: 1);
        AddStaticBox(new Vector3(1.8f, 0.7f, -6.5f), new Vector3(0.25f, 1.4f, 14f), new Color(0.3f, 0.32f, 0.36f), layer: 1);

        // Control point: stand on it to open the bonus wall socket sightline
        // (sim hookup lands at M2 with operated elements; sweepInert visual now).
        var controlPad = AddStaticBox(new Vector3(26, 0.1f, -6), new Vector3(3f, 0.2f, 3f), new Color(0.3f, 0.9f, 0.6f), layer: 1);
        controlPad.AddChild(MakeArea("controlPoint", new BoxShape3D { Size = new Vector3(3f, 1.5f, 3f) }));
    }

    private static Area3D MakeArea(string kind, Shape3D shape)
    {
        var area = new Area3D { CollisionLayer = 1 << 5, CollisionMask = 1 << 4 };
        area.AddChild(new CollisionShape3D { Shape = shape });
        area.SetMeta("kind", kind);
        return area;
    }

    private void BuildEnvironment()
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
        var root = new Node3D();
        float scale = defId switch
        {
            "mote" => 0.55f, "monolith" => 2.4f, "aegis" => 1.4f, "skiff" => 0.9f, _ => 1f,
        };
        var mesh = new MeshInstance3D
        {
            Name = "Mesh",
            Mesh = new CapsuleMesh { Radius = 0.45f * scale, Height = 1.6f * scale },
            MaterialOverride = new StandardMaterial3D { AlbedoColor = new Color(0.9f, 0.8f, 0.7f) },
            Position = new Vector3(0, 0.8f * scale, 0),
        };
        root.AddChild(mesh);

        var area = new Area3D { CollisionLayer = 1 << 1, CollisionMask = 0 };
        area.AddChild(new CollisionShape3D
        {
            Shape = new CapsuleShape3D { Radius = 0.5f * scale, Height = 1.7f * scale },
            Position = new Vector3(0, 0.8f * scale, 0),
        });
        area.SetMeta("enemy_id", enemyId);
        root.AddChild(area);

        AddChild(root);
        return root;
    }

    private Node3D SpawnTowerView(Vector3 socketPos)
    {
        var root = new Node3D();
        root.AddChild(new MeshInstance3D
        {
            Mesh = new BoxMesh { Size = new Vector3(1.2f, 2.6f, 1.2f) },
            MaterialOverride = new StandardMaterial3D { AlbedoColor = new Color(0.55f, 0.6f, 0.85f) },
            Position = new Vector3(0, 1.3f, 0),
        });
        root.Position = socketPos;
        AddChild(root);
        return root;
    }

    private Node3D SpawnProjectileView()
    {
        var root = new Node3D();
        root.AddChild(new MeshInstance3D
        {
            Mesh = new SphereMesh { Radius = 0.15f, Height = 0.3f },
            MaterialOverride = new StandardMaterial3D
            {
                AlbedoColor = new Color(1f, 0.9f, 0.3f),
                EmissionEnabled = true,
                Emission = new Color(1f, 0.8f, 0.2f),
            },
        });
        AddChild(root);
        return root;
    }

    // =====================================================================
    // HUD + lobby
    // =====================================================================

    private void BuildHud()
    {
        var canvas = new CanvasLayer { Name = "Hud" };

        _hudLabel = new Label { Position = new Vector2(16, 12) };
        canvas.AddChild(_hudLabel);

        _toastLabel = new Label { Position = new Vector2(16, 110), Modulate = new Color(1f, 0.85f, 0.4f) };
        canvas.AddChild(_toastLabel);

        _crosshair = new ColorRect
        {
            Color = new Color(1, 1, 1, 0.8f),
            AnchorLeft = 0.5f, AnchorTop = 0.5f, AnchorRight = 0.5f, AnchorBottom = 0.5f,
            OffsetLeft = -2, OffsetTop = -2, OffsetRight = 2, OffsetBottom = 2,
            PivotOffset = new Vector2(2, 2),
            MouseFilter = Control.MouseFilterEnum.Ignore,
        };
        canvas.AddChild(_crosshair);

        _capturePrompt = new Label
        {
            Text = "CLICK TO CAPTURE MOUSE",
            Modulate = new Color(1f, 0.9f, 0.3f),
            AnchorLeft = 0.5f, AnchorTop = 0.5f, AnchorRight = 0.5f, AnchorBottom = 0.5f,
            OffsetLeft = -110, OffsetTop = 30,
            Visible = false,
        };
        canvas.AddChild(_capturePrompt);

        AddChild(canvas);
    }

    private void BuildLobby()
    {
        _lobbyUi = new PanelContainer
        {
            AnchorLeft = 0.5f, AnchorTop = 0.5f, AnchorRight = 0.5f, AnchorBottom = 0.5f,
            OffsetLeft = -220, OffsetTop = -170, OffsetRight = 220, OffsetBottom = 170,
        };
        var vbox = new VBoxContainer();
        _lobbyUi.AddChild(vbox);

        vbox.AddChild(new Label { Text = "DEEP FIELD 3D — FOUNDRY", HorizontalAlignment = HorizontalAlignment.Center });

        var nameEdit = new LineEdit { PlaceholderText = "name", Text = System.Environment.UserName };
        vbox.AddChild(nameEdit);

        vbox.AddChild(new Label { Text = "faction (one per player — ability levels persist across matches):" });
        var factionButtons = new Dictionary<string, CheckBox>();
        var descriptions = new Dictionary<string, string>
        {
            ["forge"] = "Forge — Overdrive surge + build discount",
            ["ember"] = "Ember — Ignition Wave + longer burns",
            ["tempest"] = "Tempest — Chain Surge (shock) + fire rate",
        };
        foreach (var (id, text) in descriptions)
        {
            var button = new CheckBox
            {
                Text = $"{text}   [Lv{_profile.LevelFor(id)}  {_profile.FactionXp.GetValueOrDefault(id, 0)}xp]",
                ButtonPressed = id == _profile.PreferredFaction,
            };
            string captured = id;
            button.Toggled += on =>
            {
                if (!on) return;
                foreach (var (otherId, other) in factionButtons)
                    if (otherId != captured) other.ButtonPressed = false;
                _profile.PreferredFaction = captured;
                _profile.Save();
            };
            factionButtons[id] = button;
            vbox.AddChild(button);
        }

        string Faction() =>
            factionButtons.FirstOrDefault(kv => kv.Value.ButtonPressed).Key ?? "ember";
        void Grab()
        {
            _playerName = nameEdit.Text.Length > 0 ? nameEdit.Text : "player";
            _profile.Name = _playerName;
            _profile.Save();
        }

        var soloBtn = new Button { Text = "SOLO" };
        soloBtn.Pressed += () => { Grab(); StartSolo(Faction()); };
        vbox.AddChild(soloBtn);

        var hostBtn = new Button { Text = "HOST (friends join via your tailscale ip)" };
        hostBtn.Pressed += () => { Grab(); StartHost(Faction()); };
        vbox.AddChild(hostBtn);

        var joinRow = new HBoxContainer();
        var ipEdit = new LineEdit { PlaceholderText = "host ip (100.x.x.x)", CustomMinimumSize = new Vector2(240, 0) };
        var joinBtn = new Button { Text = "JOIN" };
        joinBtn.Pressed += () => { Grab(); if (ipEdit.Text.Length > 0) StartClient(ipEdit.Text.Trim(), Faction()); };
        joinRow.AddChild(ipEdit);
        joinRow.AddChild(joinBtn);
        vbox.AddChild(joinRow);

        var hud = GetNode<CanvasLayer>("Hud");
        hud.AddChild(_lobbyUi);
    }

    private void UpdateHud(double delta)
    {
        if (Mode == RunMode.Lobby) { _capturePrompt.Visible = false; return; }

        string status;
        string vitals = "";
        if (_world is not null)
        {
            string phase = _world.Phase switch
            {
                MatchPhase.Intermission => $"intermission {_world.PhaseTimer:0.0}s  [F] start",
                MatchPhase.Wave => $"wave {_world.WaveIndex + 1}/{_world.Map.TotalWaves}",
                MatchPhase.Victory => "VICTORY",
                MatchPhase.Defeat => "DEFEAT",
                _ => "",
            };
            string scrap = string.Join("  ", _world.TeamScrap.Select(kv => $"{kv.Key}:{kv.Value}"));
            status = $"credits {_world.Money}    lives {_world.Lives}    {phase}\nteam scrap  {scrap}";
            if (_world.Players.TryGetValue(LocalPlayerId, out var me))
                vitals = $"\nhp {me.Hp:0}    weapon {me.WeaponId}    ability {(me.AbilityCooldown <= 0 ? "READY [Q]" : $"{me.AbilityCooldown:0.0}s")}"
                    + (me.Downed ? "    !! DOWNED !!" : "");
        }
        else if (_net.Meta is { } meta)
        {
            int phaseInt = (int)meta["phase"];
            string phase = phaseInt switch
            {
                0 => $"intermission {(float)meta["phaseTimer"]:0.0}s  [F] start",
                1 => $"wave {(int)meta["wave"] + 1}/{(int)meta["totalWaves"]}",
                2 => "VICTORY", 3 => "DEFEAT", _ => "",
            };
            var teamScrap = meta["teamScrap"].AsGodotDictionary();
            string scrap = string.Join("  ", teamScrap.Keys.Select(k => $"{k}:{teamScrap[k]}"));
            status = $"credits {(int)meta["money"]}    lives {(int)meta["lives"]}    {phase}\nteam scrap  {scrap}";
            foreach (Godot.Collections.Dictionary entry in meta["players"].AsGodotArray())
            {
                if ((int)entry["id"] != LocalPlayerId) continue;
                float cd = (float)entry["abilityCd"];
                vitals = $"\nhp {(float)entry["hp"]:0}    weapon {entry["weapon"]}    ability {(cd <= 0 ? "READY [Q]" : $"{cd:0.0}s")}"
                    + ((bool)entry["downed"] ? "    !! DOWNED !!" : "");
            }
        }
        else status = "connecting…";

        _hudLabel.Text = status + vitals +
            "\n[E] build   [U] upgrade   [Q] ability   [R] revive   [1-4] armory (near station)   [LMB] fire";

        if (_toastTimer > 0)
        {
            _toastTimer -= delta;
            if (_toastTimer <= 0) _toastLabel.Text = "";
        }

        if (_hitFlashTimer > 0)
        {
            _hitFlashTimer -= delta;
            _crosshair.Color = new Color(1f, 0.35f, 0.2f, 1f);
            _crosshair.Scale = new Vector2(2.2f, 2.2f);
        }
        else
        {
            _crosshair.Color = new Color(1, 1, 1, 0.8f);
            _crosshair.Scale = Vector2.One;
        }

        _capturePrompt.Visible = _player is not null && Input.MouseMode != Input.MouseModeEnum.Captured;
    }

    private void BankLocalXp()
    {
        if (_xpBanked) return;
        int xp = 0;
        if (_world is not null && _world.Players.TryGetValue(LocalPlayerId, out var me))
            xp = me.MatchXp;
        else if (_net.Meta is { } meta)
        {
            foreach (Godot.Collections.Dictionary entry in meta["players"].AsGodotArray())
                if ((int)entry["id"] == LocalPlayerId) xp = (int)entry["xp"];
        }
        if (xp > 0)
        {
            _xpBanked = true;
            _profile.BankXp(_factionId, xp);
            Toast($"+{xp} {_factionId} xp banked (Lv{_profile.LevelFor(_factionId)})");
        }
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

    public void Toast(string message)
    {
        if (Mode == RunMode.Dedicated) { GD.Print($"[match] {message}"); return; }
        _toastLabel.Text = message;
        _toastTimer = 2.5;
    }

    public static Vector3 ToGd(Vec3 v) => new(v.X, v.Y, v.Z);
}
