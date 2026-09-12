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
    private readonly Dictionary<int, ScrapPickupView> _pickupViews = new();
    private readonly HashSet<int> _livePickups = new();
    private readonly List<int> _collectedPickups = new();
    private readonly Dictionary<int, Node3D> _towerViews = new();
    /// <summary>Design's yaw/pitch/spin nodes per tower view, resolved once per
    /// build of the view (an upgrade rebuilds it, so the entry is keyed on the
    /// view too).</summary>
    private readonly Dictionary<int, (Node3D View, TowerRig Rig)> _towerRigs = new();
    /// <summary>Far plane for the review-shot cameras: past design's 700 m
    /// skybox dome, or the shot has no sky in it.</summary>
    private const float SkyFar = 1000f;
    /// <summary>--audit-sockets prints every build pad's world height beside
    /// its socket's. Cheap, and it is how the wall-socket pads were caught
    /// rendering on the floor instead of on the deck.</summary>
    private bool _auditSockets;
    private bool _dumpAssets;
    private bool _dumpHits;
    private List<string>? _aimReport;         // --shot aim: per-frame tracking samples
    private float _aimWorstPitch;             // worst settled elevation error, degrees
    /// <summary>Settled samples to gather before calling it. The elevation a
    /// flyer demands changes all the way along its strand, so a verdict taken
    /// after a couple of seconds is a verdict about whichever moment it caught:
    /// the same Filament measured 3.6 degrees out at one point on the lane and
    /// 12.5 at another, pinned against its ceiling. The air run samples the
    /// whole pass and keeps the worst.</summary>
    private int _aimSampleTarget = 200;
    private string _aimReportPath = "";
    private float _aimHeld;                   // seconds this turret has held a target
    private float _aimWorst;                  // worst |angle| between a turret and its target
    private int _aimSettled;                  // samples taken after the turn settled
    private float _aimElapsed;                // seconds the probe has been running
    private List<string>? _siegeReport;       // --shot siege: what a player would see
    private string _siegeReportPath = "";
    private float _siegeElapsed;
    private int _siegeLastLog = -1;
    private int _siegeSawBar;                 // frames with a bar up at partial health
    private bool _siegeCapture;               // photograph it instead of scoring it
    private bool _siegeSawStructure;          // the barricade existed at least once

    /// <summary>Deferred staging for the review shots: each entry waits a
    /// number of frames, then runs. Commands are queued and applied on the next
    /// tick, and views are built from the events that tick drains, so a preset
    /// that does everything in one frame photographs a world where nothing it
    /// asked for has happened yet. Both surfaces that were unreviewable were
    /// unreviewable for exactly that reason.</summary>
    private readonly Queue<(int Frames, System.Action Step)> _shotStages = new();
    private int _stageWaited;
    private string? _intermissionShotPath;
    private int _intermissionLastWave = -1;
    private int _intermissionFireBeat;        // throttles the hero so towers get a share
    private int _lastWaveEnemyCount;          // what the wave announced, for the kill split
    private string? _shotPath;
    private string _shotView = "eye";
    private int _shotCountdown;
    private List<Vector3> _laneMouths = new();
    private Node3D? _coreView;
    private Node3D? _coreHitView;
    private double _coreHitTimer;
    private readonly Dictionary<string, StaticBody3D> _socketBodies = new();
    private readonly Dictionary<string, SocketTag> _socketTags = new();
    private readonly Dictionary<int, Node3D> _avatarViews = new();

    // UI surfaces (M2.5). GameRoot owns state; these are views that raise
    // commands back through Submit().
    private HudRoot _hud = null!;
    private MatchScreens _screens = null!;
    private BuildWheel _wheel = null!;
    private TeleportPicker _picker = null!;
    private UpgradePanel _upgrade = null!;
    private ArmoryScreen _armory = null!;
    private BuildGhost _ghost = null!;
    /// <summary>The coverage of the structure the upgrade panel is open on.
    /// A price and a multiplier do not tell a player whether the next Range
    /// level reaches the corner they keep leaking from; the ring on the deck
    /// does.</summary>
    private CoverageRings _upgradeRings = null!;
    private WorldMarkers _markers = null!;
    private LobbyScreen _lobby = null!;
    private CanvasLayer _overlay = null!;
    /// <summary>Design's mesh effects — flashes, streaks, impacts — driven
    /// client-side (see Vfx.cs). The player fires straight into it.</summary>
    public Vfx Vfx { get; private set; } = null!;
    private bool _shotFire;                   // --shot vm-<weapon>: fire one round before the capture
    private float _launchAfter = -1f;         // --launch-after: seconds until a hosted party launches itself
    private List<string>? _traversalReport;   // --shot <map> <txt> traversal
    private string _traversalReportPath = "";
    private int _traversalIndex = -1;
    private float _traversalTimer;
    private readonly List<(string Id, Vector3 Base, Vector3 Deck)> _climbs = new();
    private readonly List<string> _unreachable = new();
    private float _partyElapsed;

    private readonly GameView _view = new();
    private readonly Dictionary<int, EnemyOverhead> _overheads = new();
    private readonly Dictionary<int, StructureOverhead> _structureBars = new();
    private readonly Dictionary<int, int> _killsByPlayer = new();
    private int _reactionCount;
    private int _lastWaveLeaks;

    /// <summary>Each player's match kill total as it stood when the wave began.
    /// The recap is headed "last wave" and GameView carries match totals only,
    /// so printing those under that heading would be a wrong number rather than
    /// a missing one. Subtracting the snapshot is what makes it true.</summary>
    private readonly Dictionary<int, int> _killsAtWaveStart = new();

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
    public override void _ExitTree()
    {
        GD.Print(AssetLibrary.Summary());

        // --dump-assets makes a played match report everything it asked for,
        // which is the only way to see the map kit: the audit walks content
        // tables and never builds a level.
        if (_dumpAssets)
        {
            foreach (string name in AssetLibrary.Requested) GD.Print($"[asset-audit] requested {name}");
            foreach (string id in UiTheme.RequestedIcons) GD.Print($"[asset-audit] requested icon_{id}");
        foreach (string id in UiTheme.MissingIcons) GD.Print($"[asset-audit] MISSING icon_{id}");
        }

        // Let the cached Godot resources go before the runtime does. Godot
        // tears down the SceneTree ahead of mono, and a static cache holding a
        // Texture2D or a FontFile keeps its binding alive past that point — at
        // which the engine aborts with "script_bindings.is_empty()" and spews
        // leaked-reference errors. It fires after the match has already
        // finished successfully, so CI read it as flake rather than as a
        // teardown bug, and it got steadily worse as the UI grew because more
        // of the design system ended up cached.
        //
        // This runs unconditionally, which is why the dump above is now a
        // block rather than an early return: a diagnostic flag must not decide
        // whether the engine shuts down cleanly.
        UiTheme.ReleaseCaches();
        Tokens.ReleaseCaches();
        Kit.ReleaseCaches();
        AssetLibrary.ReleaseCaches();
        TowerRig.ReleaseCaches();
        // Same rule for the view maps this class keeps: they hold managed
        // wrappers for nodes the SceneTree is about to take away, and anything
        // still referenced when mono shuts down is reported as leaked.
        _pickupViews.Clear();
        _projectileViews.Clear();
        _towerViews.Clear();
        _enemyViews.Clear();
        _teleportPads.Clear();
        _vehicleNodes.Clear();
        _warped.Clear();
        _padState.Clear();
        _footprints.Clear();
        Surfaces.Clear();
        _avatarViews.Clear();
        _towerRigs.Clear();
    }

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
        _auditSockets = System.Array.IndexOf(args, "--audit-sockets") >= 0;
        _dumpAssets = System.Array.IndexOf(args, "--dump-assets") >= 0;

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
            // --host: open a party from the command line (the Host button),
            // --launch-after N: launch it N seconds later. Together they are
            // the loopback smoke for the party lobby.
            if (args[i] == "--host")
            {
                // Flags to the right of --host are read first: StartHost is
                // the end of the parse.
                for (int j = i + 1; j + 1 < args.Length; j++)
                    if (args[j] == "--launch-after" && float.TryParse(args[j + 1], out float later)) _launchAfter = later;
                _playerName = "host";
                StartHost(_profile.PreferredFaction);
                return;
            }
            if (args[i] == "--launch-after" && i + 1 < args.Length && float.TryParse(args[i + 1], out float after))
                _launchAfter = after;
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
                // Lobby and other pre-match surfaces are captured where they
                // live — starting a match would tear them down.
                if (_shotView == "howto")
                {
                    _lobby.Visible = false;      // it sits above the match layers
                    _screens.ShowHowTo();
                    _shotView = "eye";
                    _shotCountdown = 4;
                    return;
                }
                if (_shotView is "lobby" or "sector")
                {
                    if (_shotView == "sector") _lobby.ShowSectorTab();
                    _shotView = "eye";
                    return;
                }
                if (_shotView == "party")
                {
                    // The lobby as a party: hosting from the lobby, before launch.
                    StartHost("ember");
                    _shotView = "eye";
                    _shotCountdown = 30;
                    return;
                }
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
    /// <summary>Headless runs use the dummy audio driver, where the bus list can
    /// be empty — touching bus 0 unconditionally is a crash waiting for a CI
    /// machine without a sound device.</summary>
    private static void SetMasterVolume(float volume)
    {
        if (AudioServer.GetBusCount() < 1) return;
        AudioServer.SetBusVolumeDb(0, volume <= 0.001f ? -80f : Mathf.LinearToDb(volume));
    }

    private void CaptureShot()
    {
        string path = _shotPath!;
        _shotPath = null;

        // Surface shots: open the UI being reviewed, then capture next frame.
        if (_shotView == "connection")
        {
            _shotPath = path;
            _shotView = "eye";
            _shotCountdown = 4;
            _hud.Visible = false;
            _screens.ShowConnection("Version mismatch",
                "The host is running a different build.",
                MatchScreens.ConnectionTone.Danger,
                NetworkManager.BuildLabel(), "0.2.1 · p2",
                actions: new[] { "Back" });
            return;
        }

        if (_shotView == "pause")
        {
            _shotPath = path;
            _shotView = "eye";
            _shotCountdown = 4;
            _screens.BindSettings(_profile);
            _screens.ShowPause();
            return;
        }

        if (_shotView == "endmatch" && _world is not null)
        {
            _shotPath = path;
            _shotView = "eye";
            _shotCountdown = 4;
            _hud.Visible = false;
            // Real counters on a real world — the screen renders what the sim
            // recorded, so a wrong column here is a wrong column in a match.
            var me = _world.Players[LocalPlayerId];
            me.Kills = 37; me.DamageDealt = 18420f; me.TowersBuilt = 6; me.Revives = 2;
            RebuildView();
            _screens.ShowEnd(_view, victory: true, 140, me.FactionId,
                new Dictionary<int, int>(), 41);
            return;
        }

        // Aim check: build a lance on the socket nearest the route, start the
        // wave, and let it run while the probe reports how far the turret's
        // forward is from the enemy it picked. Zero means it points at what it
        // is shooting; 180 was the bug.
        if (_shotView == "aim" && _world is not null)
        {
            // Nearest to any waypoint of the walked ground route, not to one
            // route's midpoint: the first pick sat 28 m off the lane the
            // enemies actually took, so nothing ever entered the lance's range.
            string best = _map.Sockets[0].Id; float bestD = float.MaxValue;
            foreach (var s in _map.Sockets)
            {
                if (s.Tag is not (SocketTag.Ground or SocketTag.Wall)) continue;
                foreach (var route in _map.Routes)
                {
                    if (route.Layer != EnemyLayer.Ground) continue;
                    foreach (var w in route.Waypoints)
                    {
                        float d = s.Pos.DistanceTo(w);
                        if (d < bestD) { bestD = d; best = s.Id; }
                    }
                }
            }
            // Submit and let the normal loop tick and drain. Advancing the sim
            // by hand here consumed the TowerPlaced event before the view layer
            // saw it, so the tower existed with nothing to turn.
            Submit(new Command.PlaceTower(LocalPlayerId, "lance", best));
            Submit(new Command.StartWave(LocalPlayerId));
            // Godot's stdout is buffered and mono's headless teardown aborts
            // before it flushes, so the verdict goes to a file or it is lost.
            _aimReportPath = path;
            _aimReport = new List<string> { $"lance on {best}, {bestD:0.0} m from the route" };
            _shotView = "eye";
            return;
        }

        // Air tracking: the same question as "aim", asked about the axis that
        // one could never see. That probe builds a Lance, which cannot target
        // air at all, and flattened both vectors to the ground plane before
        // taking the angle — so a barrel aimed level at a Skiff fifteen metres
        // overhead scored a perfect zero. Elevation has never been measured on
        // this project, and Arc has no rig row in the manifest at all, so it
        // falls back to a 26 degree ceiling.
        if (_shotView == "aimair" && _world is not null)
        {
            var routes = _map.Routes.ToArray();
            int strandIndex = System.Array.FindIndex(routes, r => r.Layer == EnemyLayer.Air);
            if (strandIndex >= 0)
            {
                var strand = routes[strandIndex];
                // Partway along the first leg, where the strand has climbed but
                // is still near the pads at the mouth.
                var a = strand.Waypoints[0];
                var b = strand.Waypoints[1];
                var at = new Vec3(a.X + (b.X - a.X) * 0.5f, a.Y + (b.Y - a.Y) * 0.5f,
                                  a.Z + (b.Z - a.Z) * 0.5f);

                _world.Money = 1000000;
                var taken = new HashSet<string>();
                foreach (string defId in new[] { "skywatch", "arc", "filament" })
                {
                    var def = Towers.All[defId];
                    var pad = _map.Sockets
                        .Where(s => s.Tag is SocketTag.Ground or SocketTag.Wall)
                        .Where(s => !taken.Contains(s.Id))
                        .Where(s => s.Pos.DistanceTo(at) <= def.RangeMeters)
                        .OrderBy(s => s.Pos.DistanceTo(at))
                        .FirstOrDefault();
                    if (pad is null) { GD.Print($"[aimair] no pad reaches the strand for {defId}"); continue; }
                    taken.Add(pad.Id);
                    // Submitted, not advanced by hand: advancing here consumes
                    // the TowerPlaced event before the view layer sees it, and
                    // the tower then exists with nothing to turn.
                    Submit(new Command.PlaceTower(LocalPlayerId, defId, pad.Id));
                }

                _world.Enemies.Add(new Enemy
                {
                    Id = _world.NextId(), DefId = "skiff", Hp = 1000000f, MaxHp = 1000000f,
                    RouteIndex = strandIndex, Leg = 0, LegProgress = 0.5f,
                    Facing = new Vec3(1, 0, 0), Bounty = 0, LeakDamage = 1,
                });
            }
            _aimReportPath = path;
            _aimReport = new List<string> { $"air tracking on {_map.Id}" };
            _aimSampleTarget = 4000;      // the whole flight, not a window of it
            _shotView = "eye";
            return;
        }

        // Siege check: build a barricade, walk a Ram into it, and watch what the
        // player would see. Bought because the structure-health work shipped
        // once with nothing having ever destroyed a tower in a client — solo
        // builds nothing, so no ordinary run reaches this path at all.
        //
        // "siege" measures and writes a verdict; "siegeshot" waits until the
        // barricade is visibly hurt and photographs it. The numbers only say a
        // bar exists — the picture is what says it is the right size, in the
        // right place, and not buried inside the model.
        if (_shotView is "siege" or "siegeshot" && _world is not null)
        {
            _siegeCapture = _shotView == "siegeshot";
            Submit(new Command.PlaceTower(LocalPlayerId, "barricade", "b1"));
            _world.Enemies.Add(new Enemy
            {
                Id = _world.NextId(), DefId = "ram",
                Hp = 100_000f, MaxHp = 100_000f,   // the demolition is the subject, not the kill
                Facing = new Vec3(1, 0, 0), Bounty = 0, LeakDamage = 2,
                RouteIndex = 0, Leg = 0, LegProgress = 0f,
            });
            _siegeReportPath = path;
            _siegeReport = new List<string> { "barricade on b1, one ram walking route 0" };
            _shotView = "eye";
            return;
        }

        // The intermission worth reviewing is the one between waves, not the one
        // before the first: only that one has a recap to show. So this plays a
        // wave — towers down, wave started, time scaled up — and shoots when the
        // phase comes back round with kills, leaks and a scrap delta behind it.
        if (_shotView == "intermission" && _world is not null)
        {
            _shotView = "eye";
            _shotPath = null;
            _intermissionShotPath = path;
            Stage(0, () =>
            {
                _world.Money += 600;
                foreach (var socket in TwoBuildableSockets())
                    Submit(new Command.PlaceTower(LocalPlayerId, "lance", socket.Id));
            });
            Stage(4, () =>
            {
                Submit(new Command.StartWave(LocalPlayerId));
                // Real time would be a minute of nothing happening. The sim
                // still ticks off delta and every event still drains, so the
                // recap is built the ordinary way — just sooner.
                Engine.TimeScale = 8f;
            });
            return;
        }

        // Stage review: one lance per breakpoint, lined up, so the ten levels
        // design drew can be compared side by side. The usage audit proves the
        // modules resolve; only a picture proves the tower keeps changing
        // shape past the halfway point the sim used to stop at.
        if (_shotView == "stages")
        {
            // The intermission briefing draws over the middle of the screen,
            // which is exactly where a lineup goes.
            if (_world is not null) Submit(new Command.StartWave(LocalPlayerId));
            var origin = new Vector3(0f, 0.1f, -13f);
            int slot = 0;
            foreach (int level in new[] { 1, 4, 7, 10 })
            {
                var levels = new int[Towers.All["lance"].UpgradePaths.Count];
                // Levels count purchases and the rig draws purchases + 1, so
                // the module on screen is s{level} for the level asked for.
                for (int i = 0; i < levels.Length; i++) levels[i] = level - 1;
                var at = origin + new Vector3((slot - 1.5f) * 3.2f, 0f, 0f);
                SpawnStructureView("lance", at, levels);   // parents itself
                slot++;
            }
            if (_player is not null)
                _player.AimFrom(origin + new Vector3(0f, 2.3f, 7.5f), origin + new Vector3(0f, 1.5f, 0f));
            _shotView = "eye";
            _shotPath = path;
            _shotCountdown = 6;
            return;
        }

        // Pack a Punch review: stock the bench, buy three levels, and open the
        // armory on the weapon so the card shows a level and a next price.
        if (_shotView == "pap" && _world is not null)
        {
            if (_world.Players.TryGetValue(LocalPlayerId, out var me))
            {
                me.Scrap[ScrapType.Alloy] = 5000;
                me.Scrap[ScrapType.Gravium] = 12;
                Submit(new Command.SelectWeapon(LocalPlayerId, "sidearm"));
                // Four, so the card is sitting on the level-5 milestone and
                // shows its gravium price rather than the plain one.
                for (int i = 0; i < 4; i++) Submit(new Command.PackAPunch(LocalPlayerId, "sidearm"));
                Step.Advance(_world);
                var build = me.BuildFor("sidearm");
                GD.Print($"[pap] sidearm level {build.PackLevel}, next costs {build.NextPackCost} "
                    + $"+ {build.NextPackGravium} gravium, "
                    + $"damage x{build.DamageFactor(false):0.00}, rate x{build.RateFactor():0.00}, "
                    + $"alloy left {me.Scrap[ScrapType.Alloy]}");
                RebuildView();
            }
            _shotView = "armory";
            _shotPath = path;
            return;
        }

        // Scrap review: kill something at the player's feet so the floor has
        // drops on it, and photograph them before the magnet takes them.
        if (_shotView == "scrap" && _world is not null)
        {
            Submit(new Command.StartWave(LocalPlayerId));
            // Spawns arrive on tick offsets, so the frame after StartWave has
            // an empty field. Wait for the lane to put something on the map.
            for (int i = 0; i < Balance.TickHz * 20 && _world.Enemies.Count < 4; i++) Step.Advance(_world);

            // An enemy's position is its route progress, so moving one does not
            // stick: it is back on the lane by the next tick and its drop lands
            // there. Kill them where they walk, then take the picture from the
            // scrap rather than staging the scrap for the picture.
            foreach (var enemy in _world.Enemies) enemy.Hp = 1f;
            // Three shots a second: a burst inside one tick is refused on
            // cooldown and only the first round ever lands.
            foreach (var enemy in _world.Enemies.ToList())
            {
                Submit(new Command.PlayerHit(LocalPlayerId, enemy.Id, "sidearm"));
                for (int i = 0; i < 12; i++) Step.Advance(_world);
            }
            RebuildView();

            if (_world.Pickups.Count > 0 && _player is not null)
            {
                var centre = Vector3.Zero;
                foreach (var pickup in _world.Pickups) centre += ToGd(pickup.Pos);
                centre /= _world.Pickups.Count;
                // Stand back past the magnet radius, or the shot is of a floor
                // that was just tidied up.
                _player.AimFrom(centre + new Vector3(0f, 2.6f, 7f), centre);
                Submit(new Command.PlayerSync(LocalPlayerId,
                    new Vec3(centre.X, centre.Y, centre.Z + 7f)));
                Step.Advance(_world);
                RebuildView();
            }

            _shotView = "eye";
            _shotPath = path;
            _shotCountdown = 6;
            return;
        }

        // First-person review: buy and equip a platform, fit a barrel, and
        // fire one round on the frame before the capture so the flash and the
        // streak are in the picture.
        if (_shotView.StartsWith("vm-") && _world is not null)
        {
            string weapon = _shotView[3..];
            if (_world.Players.TryGetValue(LocalPlayerId, out var me) && Weapons.All.ContainsKey(weapon))
            {
                _world.Money = 1000;
                me.Scrap[ScrapType.Alloy] = 40;
                Submit(new Command.BuyWeapon(LocalPlayerId, weapon));
                Submit(new Command.SelectWeapon(LocalPlayerId, weapon));
                Submit(new Command.CraftAttachment(LocalPlayerId, weapon, "longBarrel"));
                Step.Advance(_world);
                Submit(new Command.StartWave(LocalPlayerId));
                Step.Advance(_world);
                RebuildView();
            }
            _shotView = "eye";
            _shotPath = path;
            _shotCountdown = 40;
            _shotFire = true;
            return;
        }

        // Traversal: every deck a player is expected to build on has to be one
        // they can actually stand on. A ladder that tops out against the
        // underside of the deck it serves looks completely correct from every
        // angle and from the map file, and leaves a whole tier — and the air
        // lane it was the only answer to — unusable.
        // Map validation: MAP-AUTHORING.md §4, measured on the assembled map.
        //
        // The rules this checks are not style. Each one is a defect this
        // project actually shipped, and the coverage rules in particular have
        // never been checked by anything — a map can pass every gate, every
        // smoke run and every screenshot review while having an enemy lane no
        // tower can reach. That is the difference between a map that builds
        // and a map that plays.
        if (_shotView == "validate")
        {
            var report = new List<string> { $"map validation on {_map.Id}" };
            var failures = new List<string>();
            // Sampled once and shared: rebuilding this inside each rule made
            // the probe allocate its way through several thousand vectors, and
            // on Switchyard that was enough to shift GC past mono's headless
            // teardown and abort the process after the report was written.
            var samples = RouteSamples();
            var sockets = BuildableSockets();
            RuleTiersAreReachable(sockets, report, failures);
            RuleRoutesAreCovered(samples, sockets, report, failures);
            RuleNoSocketCoversNothing(samples, sockets, report, failures);
            RuleFallbackIsCovered(samples, sockets, report, failures);
            RuleWallSocketsAreFooted(sockets, report, failures);
            RuleLanesAreClear(samples, report, failures);
            ReportAirAnswers(samples, sockets, report);

            // The count is what CI ratchets on, so it goes in the file in a
            // form a script can read without parsing prose.
            report.Add($"violations={failures.Count}");
            foreach (string failure in failures.Take(30)) report.Add(failure);
            WriteProbe(report, path, failures.Count == 0, failures.Count == 0
                ? "PASS: every rule this probe can measure holds"
                : $"FAIL: {failures.Count} violation(s)");
            return;
        }

        // Railway review: the yard is laid out in code from a handful of
        // constants, and the failure it kept producing was not ugly — it was
        // *wrong*, and wrong in a way a screenshot hides. A running line
        // through four tower pads and a depot road through the spawn both
        // looked perfectly like a railway. So measure it: every metre of track
        // against every socket, every walked route and every place a player is
        // put down.
        if (_shotView == "railway")
        {
            var report = new List<string> { $"railway probe on {_map.Id}" };
            var conflicts = new List<string>();

            // The cutting is the exception to all of this: it is track the
            // ground route is *supposed* to run inside.
            var stands = new List<(string What, Vector3 At)>();
            foreach (var socket in _map.Sockets)
            {
                var at = ToGd(socket.Pos);
                if (at.Y > 1f) continue;                     // deck sockets are storeys above the rails
                stands.Add(($"socket {socket.Id}", at));
            }
            stands.Add(("hero spawn", ToGd(_map.HeroSpawn)));
            stands.Add(("armory", ToGd(_map.ArmoryPos)));
            foreach (var station in _map.HeroStations)
            {
                var at = ToGd(station.Pos);
                if (at.Y > 1f) continue;
                stands.Add(($"hero station {station.Id}", at));
            }

            // A lane module is 3.4 m wide, so its own half-width is 1.7 m;
            // three metres of centre-line clearance is a pad or a person
            // standing beside the track rather than in it.
            const float Clearance = 3f;
            foreach (var (what, at) in stands)
            {
                float nearest = float.MaxValue;
                foreach (var (from, to) in _railRuns)
                    nearest = Mathf.Min(nearest, DistanceToSegment(Flat(at), Flat(from), Flat(to)));
                if (nearest < Clearance)
                    conflicts.Add($"{what} is {nearest:0.0} m from laid track");
            }

            // And the walked routes: an enemy lane is a haul road, and a haul
            // road that runs *along* a running line for thirty metres is the
            // same mistake in a different costume.
            foreach (var route in _map.Routes)
            {
                if (route.Layer != EnemyLayer.Ground) continue;
                for (int i = 0; i < route.Waypoints.Count - 1; i++)
                {
                    if (route.IsTeleportLeg(i)) continue;
                    var a = ToGd(route.Waypoints[i]);
                    var b = ToGd(route.Waypoints[i + 1]);
                    for (int k = 0; k <= 10; k++)
                    {
                        var at = a.Lerp(b, k / 10f);
                        foreach (var (from, to) in _railRuns)
                        {
                            float d = DistanceToSegment(Flat(at), Flat(from), Flat(to));
                            if (d < Clearance)
                                conflicts.Add($"route {route.Id} runs {d:0.0} m from laid track "
                                    + $"at ({at.X:0},{at.Z:0})");
                        }
                    }
                }
            }

            report.Add($"{_railRuns.Count} run(s) of track laid, ladder at 1:{LadderRatio():0.0}");
            // A yard turnout is a 1:6 to 1:8. Anything sharper is a set of
            // points no track engineer would sign off, and it is the single
            // thing that stops a throat reading as a throat.
            if (LadderRatio() < 6f)
                conflicts.Add($"ladder is 1:{LadderRatio():0.0} — sharper than a 1:6 yard turnout");
            report.Add(_cutSpan is { } span
                ? $"freight cut runs ({span.From.X:0},{span.From.Z:0}) → ({span.To.X:0},{span.To.Z:0})"
                : "freight cut: NOT PLACED");
            foreach (string conflict in conflicts.Distinct().Take(20)) report.Add(conflict);

            bool ok = conflicts.Count == 0 && _cutSpan is not null;
            WriteProbe(report, path, ok, ok
                ? "PASS: no track runs through anything a player stands on"
                : $"FAIL: {conflicts.Distinct().Count()} conflict(s)");
            return;
        }

        if (_shotView == "traversal")
        {
            _shotView = "eye";
            _traversalReportPath = path;
            _traversalReport = new List<string> { $"traversal probe on {_map.Id}" };
            foreach (var area in FindLadderAreas())
            {
                var centre = area.GlobalPosition;
                float half = LadderHalfHeight(area);
                // Foot of the rungs, wherever they start: on the Spire a ladder
                // can begin twenty metres up, and standing the probe at y=0.6
                // would have it climbing thin air.
                var foot = new Vector3(centre.X, centre.Y - half + 0.6f, centre.Z);
                _climbs.Add((Describe(area), foot, centre + new Vector3(0f, half, 0f)));
            }
            _traversalReport.Add($"{_climbs.Count} climb(s) found");
            _traversalIndex = 0;
            _traversalTimer = 0f;
            return;
        }

        if (_shotView == "teleport")
        {
            _shotView = "eye";
            _teleportReportPath = path;
            _teleportReport = new List<string>
            {
                $"teleport probe on {_map.Id}",
                $"{_teleportPads.Count} pad(s): "
                    + string.Join(", ", _teleportPads.Select(x => $"{x.Id} at ({x.At.X:0},{x.At.Z:0})")),
            };
            if (_teleportPads.Count < 2)
            {
                WriteProbe(_teleportReport, _teleportReportPath, false,
                    "INCONCLUSIVE: a network needs two pads and this map has "
                    + $"{_teleportPads.Count}");
            }
            _teleportTimer = 0f;
            return;
        }

        if (_shotView == "vehicle")
        {
            _shotView = "eye";
            _vehicleReportPath = path;
            _vehicleReport = new List<string>
            {
                $"vehicle probe on {_map.Id}",
                $"{_map.Vehicles.Count} parked: "
                    + string.Join(", ", _map.Vehicles.Select(v => $"{v.Id} ({v.DefId})")),
                // Handling is read off authored road lines rather than from
                // the meshes, so what the lines say is the only thing that
                // makes a road a road.
                SurfaceLine("the county road", new Vector3(-54, 0, 50), Surface.Asphalt),
                SurfaceLine("the south road", new Vector3(0, 0, -66), Surface.Asphalt),
                SurfaceLine("the barn drive", new Vector3(-75, 0, 47), Surface.Gravel),
                SurfaceLine("the pond", new Vector3(19, 0, -36), Surface.Water),
                SurfaceLine("the east field", new Vector3(120, 0, 0), Surface.Grass),
            };
            if (_map.Vehicles.Count == 0)
                WriteProbe(_vehicleReport, _vehicleReportPath, false,
                    "INCONCLUSIVE: this map parks no vehicles");
            _vehicleIndex = 0;
            _vehicleTimer = 0f;
            return;
        }

        if (_shotView is "armory" or "blueprints" or "wheel" or "upgrade")
        {
            string surface = _shotView;
            _shotView = "eye";
            _shotPath = path;
            _shotCountdown = 4;
            if (surface is "armory" or "blueprints")
            {
                // Stage a fitted attachment so the shot proves modules mount on
                // the weapon. A fresh match has no scrap, so without this the
                // only thing a screenshot could show is an unmodified gun.
                if (_world is not null && _world.Players.TryGetValue(LocalPlayerId, out var smith))
                {
                    // Every type, because a platform is bought with scrap now:
                    // with an empty pocket every Buy button is correctly
                    // disabled and the click probe has nothing to press.
                    foreach (ScrapType type in System.Enum.GetValues<ScrapType>())
                        smith.Scrap[type] = 40;
                    Submit(new Command.CraftAttachment(LocalPlayerId, "sidearm", "longBarrel"));
                    Step.Advance(_world);
                    RebuildView();
                }
                ToggleArmory();
                if (surface == "blueprints") _armory.ShowBlueprints();
                else _dumpHits = true;
                // Headless has no window: the root viewport is a 64×64 stub,
                // and Godot's hit test finds nothing at any point on a frame
                // laid out at full size — which is why this probe reported
                // every control inert for months. Give the root a real size
                // and the layout a few frames to settle before the click.
                if (DisplayServer.WindowGetSize() == Vector2I.Zero)
                    GetTree().Root.Size = new Vector2I(1440, 810);
            }
            // Both of these used to open their surface in this one frame and
            // shoot four frames later, which produced two pictures of the
            // intermission panel: a fresh match is in intermission, and that
            // panel draws over everything until a wave is running. Neither
            // surface has ever actually been reviewed.
            else if (surface == "wheel")
            {
                _shotPath = null;
                var socket = BuildableSocket();
                Stage(0, () => Submit(new Command.StartWave(LocalPlayerId)));
                Stage(4, () =>
                {
                    AimAt(socket.Pos, back: 7f, height: 3f);
                    if (_player is not null) _player.HoldingBuild = true;
                    OpenBuildWheel(socket.Id);
                    // Pick a wedge: the ghost and its range ring only exist for
                    // a selection, and they are half of what this screen is.
                    WheelSelect(0);
                });
                Stage(2, () => ShootSurface(path, "wheel", _wheel.IsOpen));
            }
            else if (surface == "upgrade" && _world is not null)
            {
                _shotPath = null;
                var socket = BuildableSocket();
                Stage(0, () =>
                {
                    Submit(new Command.StartWave(LocalPlayerId));
                    Submit(new Command.PlaceTower(LocalPlayerId, "lance", socket.Id));
                });
                Stage(4, () =>
                {
                    // Levels and scrap, so the panel shows filled pips and a
                    // breakpoint recipe with have/need rather than a row of
                    // zeroes and a locked tier nobody can read.
                    //
                    // Damage goes all the way to L10 and Range stops partway:
                    // the first proves the art keeps stepping past the halfway
                    // point the sim used to stop at, and the second leaves a
                    // purchase for the reach row and the preview ring to be
                    // about.
                    if (_view.AtSocket(socket.Id) is { } placed)
                    {
                        _world.Money += 20000;
                        foreach (var type in System.Enum.GetValues<ScrapType>())
                            _world.TeamScrap[type] = 200;
                        for (int i = 0; i < 9; i++)
                            Submit(new Command.UpgradeTower(LocalPlayerId, placed.Id, 0));
                        for (int i = 0; i < 3; i++)
                            Submit(new Command.UpgradeTower(LocalPlayerId, placed.Id, 1));
                    }
                });
                Stage(6, () =>
                {
                    // Far enough back and high enough that the coverage ring
                    // is inside the frame — a ring photographed from two paces
                    // away is a picture of a tower.
                    AimAt(socket.Pos, back: 12f, height: 6f);
                    if (_player is not null) _player.HoldingUpgrade = true;
                    OpenUpgradePanel(socket.Id);
                    if (_view.AtSocket(socket.Id) is { } shown)
                        GD.Print($"[upgrade] lance damage L{shown.PathLevels[0] + 1}, "
                            + $"range L{shown.PathLevels[1] + 1}, reach "
                            + $"{TowerMath.Range(Towers.All[shown.DefId], shown.PathLevels, _map, _view.Wave):0.0} m");
                });
                Stage(2, () => ShootSurface(path, "upgrade", _upgrade.IsOpen));
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
                // Framed off the field rather than a literal: 95 m up sees a
                // 110 x 80 yard whole and a third of a 320 x 160 farm.
                "top" => (new Vector3(0, Mathf.Max(_map.FieldX, _map.FieldZ * 1.6f) * 0.87f, 1), Vector3.Zero),
                "iso" => (new Vector3(-_map.HalfX * 0.95f, _map.HalfX * 0.84f, _map.HalfZ * 1.3f),
                          new Vector3(0, 0, -2)),
                "iso2" => (new Vector3(_map.HalfX * 1.02f, _map.HalfX * 0.73f, -_map.HalfZ * 1.15f),
                           new Vector3(0, 0, -2)),
                "lane" => (new Vector3(-46, 14, -26), new Vector3(0, 0, 4)),
                // Fixture checks: the gate enemies walk out of, the yard the
                // players spawn into, and the air strand over the map.
                "gate" => (new Vector3(-30, 9, 22), new Vector3(-40, 2, 0)),
                "yard" => (new Vector3(4, 4f, -28), new Vector3(-6, 1.6f, -23)),
                // Switchyard's two railway features, at eye level, because
                // that is the only height at which "does this read as a
                // railway" is a real question: down the freight cut, and
                // across the yard from the apron.
                "cut" => (new Vector3(-26, 2.6f, -3.4f), new Vector3(-8, 1.4f, -1f)),
                "yardline" => (new Vector3(-40, 5f, 13), new Vector3(0, 1.2f, 27)),
                "tunnel" => (new Vector3(11, 5, -3), new Vector3(0, 1, -8)),
                "air" => (new Vector3(-14, 24, 38), new Vector3(0, 8, 0)),
                "core" => (new Vector3(20, 10, 26), new Vector3(36, 2, 6)),
                "deck" => (new Vector3(30, 15, 6), new Vector3(6, 6, -10)),
                _ => (new Vector3(0, 60, 60), Vector3.Zero),
            };
            var camera = new Camera3D { Position = from, Far = SkyFar };
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

        // A UI probe runs before the capture, so --shot armory doubles as the
        // only automated check that this screen is operable rather than merely
        // drawn. CI greps for ERROR, so an inert control fails the build.
        if (_dumpHits)
        {
            if (!_armory.ProbeClick()) _armory.DumpHitAreas();
        }
        // Headless has no framebuffer: reading one logs an engine ERROR, and
        // CI treats any ERROR in a probe log as a failure. The probes and the
        // surface verdict files are what a headless run is for; the png is
        // for a windowed one.
        if (DisplayServer.GetName() == "headless")
        {
            GD.Print($"[shot] headless run — no frame to write for {path}");
        }
        else
        {
            var image = GetViewport().GetTexture().GetImage();
            image.SavePng(path);
            GD.Print($"[shot] wrote {path}  ({image.GetWidth()}x{image.GetHeight()})");
        }
        GetTree().Quit();
    }

    private void AuditAssets()
    {
        _map = Maps.All["foundry"];
        var built = new List<Node>();

        // Copy lives client-side by convention, so the harness cannot reach it
        // and this is the only place that checks a faction's passive has been
        // written in English rather than left as its content id.
        foreach (string gap in LobbyScreen.UnwrittenPassives())
            GD.PrintErr($"ERROR: faction passive has no wording: {gap}");

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

        // What the armory bench and the first-person view build: every
        // platform with every module on it, each round on the ammo rail, the
        // hands in every pose, and the effects a shot draws. None of these
        // come up in a played match that never opens the armory or fires.
        foreach (var weapon in Weapons.All.Values)
        {
            var everything = Attachments.All.Values.GroupBy(a => a.Slot).ToDictionary(g => g.Key, g => g.First().Id);
            if (WeaponAssembly.Build(weapon.Id, everything) is { } world) built.Add(world);
            if (WeaponAssembly.Build(weapon.Id, everything, world: false) is { } vm) built.Add(vm);
        }
        foreach (var def in Attachments.All.Values) built.Add(AssetLibrary.Instantiate($"attach_{def.Id}", () => new Node3D()));
        // Scrap on the floor: a solo match only drops it when the player kills
        // something, which no automated run does, so the audit asks for the
        // models directly rather than reporting them unused forever.
        foreach (ScrapType type in System.Enum.GetValues<ScrapType>())
            built.Add(AssetLibrary.Instantiate($"pickup_{type.ToString().ToLowerInvariant()}", () => new Node3D()));
        built.Add(AssetLibrary.Instantiate("pickup_primecore", () => new Node3D()));
        foreach (var def in Ammo.All.Values)
        {
            built.Add(AssetLibrary.Instantiate($"ammo_{def.Id}", () => new Node3D()));
            built.Add(AssetLibrary.Instantiate($"vfx_tracer_{def.Id}", () => new Node3D()));
        }
        foreach (string effect in new[] { "vfx_muzzle_lance", "vfx_impact_lance", "vfx_muzzle_nova", "vfx_impact_nova", "vfx_muzzle_arc", "vfx_impact_arc", "vfx_muzzle_skywatch", "vfx_impact_skywatch" })
            built.Add(AssetLibrary.Instantiate(effect, () => new Node3D()));
        foreach (var def in Factions.All.Values)
        {
            built.Add(AssetLibrary.Instantiate($"hero_{def.Id}", () => Placeholders.Hero(def.Id)));
            built.Add(AssetLibrary.Instantiate($"hero_{def.Id}_downed", () => Placeholders.Hero(def.Id)));
            foreach (string suffix in new[] { "", "_pistol", "_tool" })
                built.Add(AssetLibrary.Instantiate($"hands_{def.Id}{suffix}", () => new Node3D()));
        }

        // Icons follow the same content tables the UI derives them from, so
        // the audit asks for exactly what a played match would ask for.
        foreach (var def in Enemies.All.Values) UiTheme.Icon($"enemy_{def.Id}");
        foreach (var def in Towers.All.Values) { UiTheme.Icon($"tower_{def.Id}"); foreach (var path in def.UpgradePaths) UiTheme.Icon($"path_{path.Id}"); }
        foreach (var def in Traps.All.Values) UiTheme.Icon($"trap_{def.Id}");
        foreach (var def in Weapons.All.Values) UiTheme.Icon($"weapon_{def.Id}");
        foreach (var def in Attachments.All.Values) UiTheme.Icon($"attach_{def.Id}");
        foreach (var def in Ammo.All.Values) UiTheme.Icon($"ammo_{def.Id}");
        foreach (var def in Factions.All.Values) { UiTheme.Icon($"faction_{def.Id}"); UiTheme.Icon($"ability_{def.AbilityId}"); }
        foreach (var type in System.Enum.GetValues<ScrapType>()) UiTheme.Icon($"scrap_{type.ToString().ToLowerInvariant()}");
        foreach (string status in new[] { "burn", "chill", "freeze", "shock", "shred", "mark", "reveal" })
            UiTheme.Icon($"status_{status}");
        foreach (string reaction in new[] { "thermalshock", "flashfreeze" })
            UiTheme.Icon($"reaction_{reaction}");

        GD.Print($"[asset-audit] built {built.Count} views over {AssetLibrary.Requested.Count} asset names");
        foreach (string name in AssetLibrary.Requested) GD.Print($"[asset-audit] requested {name}");
        foreach (string id in UiTheme.RequestedIcons) GD.Print($"[asset-audit] requested icon_{id}");
        foreach (string id in UiTheme.MissingIcons) GD.Print($"[asset-audit] MISSING icon_{id}");
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
        // Hosting opens a party, not a match: the world exists and holds in
        // the lobby until the host launches, so friends can join, pick their
        // factions against each other's, and start together.
        BeginLocalWorld(faction, lobby: true);
        int port = ParsePort(OS.GetCmdlineUserArgs());
        _net.HostServer(port, _world!);
        _net.ServerEnqueue = c => _world!.Enqueue(c);

        _info = new InfoServer { Name = "Info" };
        AddChild(_info);
        _info.StatusProvider = ServerInfo;
        _info.Start(port);
        string invite = _info.TailscaleIp is { } ip
            ? $"invite: {ip}:{port}"
            : "no tailscale ip found — friends need your LAN ip";
        Post($"hosting — {invite}");
        _lobby.SetStatus($"hosting — {invite} — launch when everyone has picked");
        GD.Print($"[party] hosting on udp:{port}, holding in the lobby");
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

    private void BeginLocalWorld(string faction, bool lobby = false)
    {
        _factionId = faction;
        _seed = FreshSeed();
        _world = new SimWorld(_seed, _map) { Lobby = lobby, Endless = _lobby.EndlessSelected };
        _world.Enqueue(new Command.Join(1, _playerName, faction, _profile.LevelFor(faction)));
        BuildLevel(_map);
        SpawnLocalPlayer();
        _hud.BestEndlessWave = _profile.BestWave.GetValueOrDefault($"{_map.Id}:endless", 0);
        if (lobby) { _lobby.EnterParty(isHost: true); Input.MouseMode = Input.MouseModeEnum.Visible; }
        else _lobby.Visible = false;
        _markers.ShowDamageNumbers = _profile.ShowDamageNumbers;
        _hud.Scale = new Vector2(_profile.HudScale, _profile.HudScale);
        SetMasterVolume(_profile.MasterVolume);
    }

    private void SpawnLocalPlayer()
    {
        _player = new Player { Name = "Player" };
        AddChild(_player);
        _player.GlobalPosition = ToGd(_map.HeroSpawn) + new Vector3(0, 1.2f, 0);
        _player.RotationDegrees = new Vector3(0, 180, 0);
        // Saved look settings apply to the freshly spawned camera.
        _player.SensitivityScale = _profile.MouseSensitivity;
        _player.SetFieldOfView(_profile.FieldOfView);
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
        TickShotStages();
        TickIntermissionShot();
        if (_traversalReport is not null) TickTraversalProbe(delta);
        if (_teleportReport is not null) TickTeleportProbe(delta);
        if (_vehicleReport is not null) TickVehicleProbe(delta);
        if (_shotPath is not null && _shotFire && _shotCountdown == 2 && _player is not null) { _player.FireForReview(); _shotFire = false; }
        if (_shotPath is not null && --_shotCountdown <= 0) CaptureShot();
        TickCoreFlash(delta);

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
        SyncStructureHealth();
        SyncPickupViews();
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

        // The party screen owns the frame until the launch seat launches.
        if (_lobby.InParty)
        {
            _lobby.RefreshParty(_view);
            _partyElapsed += (float)delta;
            if (_launchAfter >= 0f && _view.Lobby && _partyElapsed >= _launchAfter && Mode == RunMode.Host)
            {
                Submit(new Command.Launch(LocalPlayerId));
                _launchAfter = -1f;
            }
            if (!_view.Lobby)
            {
                _lobby.LeaveParty();
                Input.MouseMode = Input.MouseModeEnum.Captured;
                GD.Print("[party] match started");
            }
            return;
        }

        if (_world is not null && _world.Players.TryGetValue(LocalPlayerId, out var me))
            _hud.SetBleedout(me.BleedoutTimer);
        _hud.Refresh(_view, delta, Input.MouseMode == Input.MouseModeEnum.Captured, _hint);
        if (_wheel.IsOpen) _wheel.Refresh(_view);
        if (_upgrade.IsOpen) _upgrade.Refresh(_view);
        UpdateUpgradeRings();
        if (_armory.IsOpen) _armory.Refresh(_view);

        // Intermission panel rides the phase, not an event, so a late joiner
        // sees it immediately.
        if (_view.Phase == MatchPhase.Intermission && !_matchOver)
            _screens.ShowIntermission(_view, _map, _seed, _lastWaveLeaks, KillsThisWave());
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
                SendVehicleSync();
            }
            Step.Advance(_world!);
            foreach (var e in _world!.Events) _tickEvents.Add(e.LogLine());
            if (Mode != RunMode.Dedicated) DrainLocalEvents();
            _accumulator -= Balance.Dt;
        }

        _net.ServerBroadcast(delta, _tickEvents);

        if (Mode != RunMode.Dedicated)
        {
            SyncVehicles();
            SyncEnemyViewsLocal(delta);
            AimTowers(delta);
            SyncProjectileViews();
            SyncAvatarsFromWorld();
        }
    }

    private void StepClient(double delta)
    {
        if (_net.JoinError is { } error)
        {
            // A refusal is a screen, not a toast — the player has to act on it.
            if (error == "versionMismatch")
                _screens.ShowConnection("Version mismatch",
                    "The host is running a different build.",
                    MatchScreens.ConnectionTone.Danger,
                    _net.JoinErrorYours, _net.JoinErrorHost,
                    actions: new[] { "Back" });
            else if (error == "factionTaken")
                _screens.ShowConnection("Faction taken",
                    "One faction per player — pick another and rejoin.",
                    MatchScreens.ConnectionTone.Danger,
                    actions: new[] { "Back" });
            else
                _screens.ShowConnection("Could not join", error,
                    MatchScreens.ConnectionTone.Danger, actions: new[] { "Back" });

            Input.MouseMode = Input.MouseModeEnum.Visible;
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
            _hud.BestEndlessWave = _profile.BestWave.GetValueOrDefault($"{_map.Id}:endless", 0);
            // The world is here: the syncing card comes down. It used to stay
            // up for the whole match, because nothing ever told it to go.
            _screens.HideStatus();
            if (_shadow.Lobby)
            {
                _lobby.EnterParty(isHost: false);
                Input.MouseMode = Input.MouseModeEnum.Visible;
                GD.Print("[party] in the lobby, waiting for the host to launch");
            }
            else _lobby.Visible = false;
            GD.Print($"[client] joined seat {LocalPlayerId} on map {_map.Id}");
            Toast($"joined as seat {LocalPlayerId}");
        }

        if (_player is null) return;

        foreach (var line in _net.PendingEvents) HandleEventLine(line);
        _net.PendingEvents.Clear();

        var pos = _player.GlobalPosition;
        _net.SendAvatar(pos, _player.Rotation.Y);
        SendVehicleSync();

        SyncVehicles();
        SyncEnemyViewsRemote(delta);
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

    /// <summary>The wrench until someone buys otherwise, and never empty —
    /// which is the whole point of it: a player with no money and no scrap is
    /// still armed.</summary>
    /// <summary>The local player's faction, for anything that needs to pick a
    /// per-faction asset — the first-person hands, today.</summary>
    public string LocalFactionId => _factionId;

    public string CurrentMeleeId()
    {
        if (Mode != RunMode.Client && _world is not null
            && _world.Players.TryGetValue(LocalPlayerId, out var p))
            return p.MeleeId;
        return "wrench";
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
                case SimEvent.TowerFired fired:
                    OnTowerFired(fired.TowerId);
                    break;

                // A demolished structure has to leave the map, or the Ram's
                // work is invisible and the player keeps counting on a tower
                // that stopped firing. Damage ticks thirty times a second and
                // is deliberately not announced — the loss is the event.
                case SimEvent.StructureDestroyed wrecked:
                    ReleaseStructureView(wrecked.TowerId);
                    Post($"{wrecked.DefId} destroyed", UiTheme.Danger);
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
                case SimEvent.PackedAPunch packed when packed.PlayerId == LocalPlayerId:
                    Post($"{packed.WeaponId} packed to level {packed.Level}", UiTheme.Accent);
                    break;

                case SimEvent.ScrapCollected collected when collected.PlayerId == LocalPlayerId:
                    Post($"+{collected.Amount} {collected.ScrapType.ToLowerInvariant()}",
                        UiTheme.Scrap(System.Enum.Parse<ScrapType>(collected.ScrapType)));
                    break;

                case SimEvent.JoinRejected rejected when _lobby.InParty && rejected.PlayerId == LocalPlayerId:
                    _lobby.SetStatus(Explain(rejected.Reason));
                    break;
                case SimEvent.JoinRejected rejected:
                    _screens.ShowStatus("JOIN REFUSED", Explain(rejected.Reason));
                    break;
                case SimEvent.FactionChanged changed:
                    if (changed.PlayerId == LocalPlayerId) { _factionId = changed.FactionId; _lobby.SetStatus(""); }
                    break;
                case SimEvent.MatchLaunched launched:
                    GD.Print($"[party] launched by seat {launched.ByPlayerId}");
                    Post("launched — first wave incoming", UiTheme.Warn);
                    break;

                case SimEvent.WaveStarted started:
                    _screens.HideIntermission();
                    _lastWaveLeaks = 0;
                    SnapshotKills();
                    _lastWaveEnemyCount = started.EnemyCount;
                    Post($"wave {started.WaveIndex + 1} — {started.EnemyCount} inbound");
                    break;
                case SimEvent.WaveCleared cleared:
                    Post($"wave {cleared.WaveIndex + 1} cleared", UiTheme.Good);
                    break;
                case SimEvent.EnemyLeaked leaked:
                    _lastWaveLeaks++;
                    Post("BREACH — core hit", UiTheme.Danger);
                    FlashCore();
                    OnBreach(leaked.EnemyId);
                    break;
                case SimEvent.EnemyTeleported warp:
                    OnEnemyTeleported(warp.EnemyId, new Vector3(warp.X, warp.Y, warp.Z));
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
            case "structureDestroyed":
                ReleaseStructureView(int.Parse(p[2]));
                Post($"{p[3]} destroyed", UiTheme.Danger);
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
                SnapshotKills();
                _lastWaveEnemyCount = int.Parse(p[3]);
                Post($"wave {int.Parse(p[2]) + 1} — {p[3]} inbound");
                break;
            case "waveCleared": Post($"wave {int.Parse(p[2]) + 1} cleared", UiTheme.Good); break;
            case "enemyLeaked":
                _lastWaveLeaks++;
                Post("BREACH — core hit", UiTheme.Danger);
                FlashCore();
                OnBreach(int.Parse(p[2]));
                break;
            case "reaction": _reactionCount++; OnReaction(int.Parse(p[2]), p[3]); break;
            case "packedAPunch" when int.Parse(p[2]) == LocalPlayerId:
                Post($"{p[3]} packed to level {p[4]}", UiTheme.Accent);
                break;

            case "scrapCollected" when int.Parse(p[3]) == LocalPlayerId:
                Post($"+{p[5]} {p[4].ToLowerInvariant()}", UiTheme.Scrap(System.Enum.Parse<ScrapType>(p[4])));
                break;

            case "joinRejected" when _lobby.InParty && int.Parse(p[2]) == LocalPlayerId:
                _lobby.SetStatus(Explain(p[3]));
                break;
            case "factionChanged":
                if (int.Parse(p[2]) == LocalPlayerId) { _factionId = p[3]; _lobby.SetStatus(""); }
                break;
            case "matchLaunched":
                GD.Print($"[party] launched by seat {p[2]}");
                Post("launched — first wave incoming", UiTheme.Warn);
                break;
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
            case "enemyTeleported":
                OnEnemyTeleported(int.Parse(p[2]), new Vector3(
                    float.Parse(p[5], System.Globalization.CultureInfo.InvariantCulture),
                    float.Parse(p[6], System.Globalization.CultureInfo.InvariantCulture),
                    float.Parse(p[7], System.Globalization.CultureInfo.InvariantCulture)));
                break;
        }
    }

    // ---- Shared event reactions (identical in every mode) -----------------

    /// <summary>An enemy just crossed a warp gate.
    ///
    /// A burst at each end, and a note that this one's view must not be
    /// interpolated on the next sync — otherwise a network client watches it
    /// glide two hundred metres across the fields at walking speed, through
    /// the buildings, which is both wrong and the most conspicuous thing on
    /// the map. The distance rule in the sync does most of the work; this is
    /// the belt to its braces, and it is the half that survives the event
    /// arriving before the snapshot.</summary>
    private void OnEnemyTeleported(int enemyId, Vector3 to)
    {
        if (_enemyViews.TryGetValue(enemyId, out var view) && IsInstanceValid(view))
            Vfx.EnemyWarp(view.GlobalPosition);
        Vfx.EnemyWarp(to);
        _warped.Add(enemyId);
    }

    /// <summary>Enemies that warped since their view was last placed.</summary>
    private readonly HashSet<int> _warped = new();

    /// <summary>Design's flash at the rig's muzzle, facing where the barrel
    /// points. Only the host sees these: TowerFired is not relayed, and a
    /// client's projectile views tell the same story a beat later.</summary>
    private void OnTowerFired(int towerId)
    {
        if (!_towerViews.TryGetValue(towerId, out var view) || !IsInstanceValid(view)) return;
        string defId = (string)view.GetMeta("def_id", "");
        if (defId.Length == 0) return;
        var rig = RigFor(towerId, view, defId);
        if (rig.Muzzle is null || !IsInstanceValid(rig.Muzzle)) return;
        Vfx.TowerFired(defId, rig.Muzzle.GlobalPosition, rig.Forward);
    }

    private void OnEnemyDamaged(int enemyId, float amount, string source)
    {
        bool mine = source == $"player{LocalPlayerId}";
        if (mine) _hud.SetCrosshair(CrosshairState.Hit);

        if (EnemyWorldPos(enemyId) is { } pos)
        {
            // A teammate's shot never crosses the wire as a shot; the damage
            // it did does. Draw their streak from where they stand.
            if (!mine && source.StartsWith("player") && int.TryParse(source[6..], out int who)
                && _avatarViews.TryGetValue(who, out var avatar) && IsInstanceValid(avatar))
                Vfx.RemoteShot(avatar.Position + new Vector3(0f, 1.5f, 0f), pos + new Vector3(0f, 1f, 0f));

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

        // Campaign progress. A loss records how deep the run got, because
        // "best wave 9 of 12" is what tells a player whether they are close;
        // a win additionally opens the next sector.
        //
        // Every mode records, clients included. Profiles are per-player and
        // local, so someone who joined a friend's match and helped clear
        // sector two has cleared sector two — locking them out of it at home
        // because the world lived on the host's machine would be absurd. The
        // wave number is safe to read here in any mode: it rides the meta
        // channel, and MatchEnded reaches clients the same as anyone.
        _profile.RecordResult(_view.Endless ? $"{_map.Id}:endless" : _map.Id, _view.Wave + 1, victory);

        _screens.HideIntermission();
        _screens.ShowEnd(_view, victory, _bankedXp, _factionId, _killsByPlayer, _reactionCount);
        Input.MouseMode = Input.MouseModeEnum.Visible;
    }

    /// <summary>Point the player at a leak they didn't see. The leaking enemy's
    /// view is already gone this frame, so fall back to the route's goal.</summary>
    /// <summary>Flashes the core to its struck state for a beat. A leak costs
    /// a life wherever it happened, so the core itself should show it.</summary>
    private void FlashCore()
    {
        if (_coreHitView is null || _coreView is null) return;
        _coreHitView.Visible = true;
        _coreView.Visible = false;
        _coreHitTimer = 1.2;
    }

    private void TickCoreFlash(double delta)
    {
        if (_coreHitTimer <= 0) return;
        _coreHitTimer -= delta;
        if (_coreHitTimer > 0) return;
        if (_coreHitView is not null) _coreHitView.Visible = false;
        if (_coreView is not null) _coreView.Visible = true;
    }

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
        string chassisAsset = AssetLibrary.StructureAsset(defId);
        var chassis = AssetLibrary.Instantiate(chassisAsset, () => Placeholders.Structure(defId));
        chassis.Name = "Body";
        root.AddChild(chassis);
        AddChild(root);

        if (levels is null || !Towers.All.TryGetValue(defId, out var def)) return root;
        for (int i = 0; i < levels.Length && i < def.UpgradePaths.Count; i++)
        {
            if (levels[i] <= 0) continue;
            string pathId = def.UpgradePaths[i].Id;
            int stage = Mathf.Clamp(levels[i] + 1, 1, 10);

            string moduleAsset = $"tower_{defId}_{pathId}_s{stage}";
            var module = AssetLibrary.TryInstantiate(moduleAsset);
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
            MergeRig(Unwrap(module, moduleAsset), Unwrap(chassis, chassisAsset));
            root.RemoveChild(module);
            module.QueueFree();
        }
        return root;
    }

    /// <summary>Godot's glTF import keeps the file's single root node and puts
    /// it under an extra scene root, so a chassis arrives as
    /// AuxScene → tower_lance_chassis → lance_foot / lance_yaw, and a module as
    /// AuxScene → tower_lance_damage_s4 → lance_yaw → lance_pitch. The merge has
    /// to start below the wrapper or the two trees never share a name and the
    /// module lands whole under the chassis — which renders identically at
    /// rest, and is why nothing noticed until the rig started moving.</summary>
    private static Node Unwrap(Node scene, string asset)
        => scene.GetNodeOrNull<Node3D>(asset.ToLowerInvariant()) ?? scene;

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

    private void SyncEnemyViewsLocal(double delta)
    {
        foreach (var enemy in _world!.Enemies)
        {
            if (!_enemyViews.TryGetValue(enemy.Id, out var view))
            {
                view = SpawnEnemyView(enemy.Id, enemy.DefId);
                _enemyViews[enemy.Id] = view;
            }
            view.Position = ToGd(enemy.Pos);
            // Forward is whatever the current leg points at, so a walker turns
            // through a corner instead of sliding round it sideways. The sim
            // already keeps Facing per leg; nothing was reading it.
            //
            // Except out of a warp gate, where the new leg can point anywhere:
            // easing into it would have the thing pirouette on the pad for
            // half a second before walking off.
            if (_warped.Remove(enemy.Id)) FaceAlongInstantly(view, enemy.Facing);
            else FaceAlong(view, enemy.Facing, delta);
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

    /// <summary>Health bars over damaged structures, read from GameView rather
    /// than from the world, so a network client watches its towers come down
    /// exactly the way the host does.
    ///
    /// The bar is the whole point of the pass: before the Ram, a structure was
    /// either standing or gone, and the first news of a demolition was the
    /// toast telling you it had already finished.</summary>
    private void SyncStructureHealth()
    {
        if (!_view.Valid) return;
        var camera = GetViewport().GetCamera3D();

        foreach (var structure in _view.Structures)
        {
            if (!_towerViews.TryGetValue(structure.Id, out var view) || !IsInstanceValid(view)) continue;

            if (!_structureBars.TryGetValue(structure.Id, out var bar) || !IsInstanceValid(bar))
            {
                bar = new StructureOverhead { TopHeight = StructureBarHeight(view) };
                view.AddChild(bar);
                _structureBars[structure.Id] = bar;
            }

            float distance = camera is null ? 0f : camera.GlobalPosition.DistanceTo(view.Position);
            bar.Set(structure.HpFraction, distance);
            RefreshBarricadeArt(structure.Id, structure.DefId, structure.HpFraction, view);
        }

        // Views are freed with the structure they hang under, so this only has
        // to drop the stale keys.
        foreach (int id in _structureBars.Keys.Where(id => !_towerViews.ContainsKey(id)).ToList())
            _structureBars.Remove(id);

        if (_siegeReport is not null) WatchSiege();
    }

    /// <summary>Scrap on the floor, from whichever source this process has —
    /// the world on a host, the meta channel on a client. Views follow the
    /// sim's list: one appears when something drops it and goes when the sim
    /// says it is gone, whether that was a player collecting it or its timer
    /// banking it to the team.</summary>
    private void SyncPickupViews()
    {
        if (!_view.Valid) return;
        // Nothing on the floor and nothing drawn: the common case by far, and
        // it used to allocate a set, a projection and a list every frame in
        // order to iterate an empty list. That churn is what pushed the
        // headless teardown over the edge on CI — see the note beside
        // ReleaseCaches: what survives to mono's shutdown gets reported as a
        // leak, and more garbage means more survivors.
        if (_view.Pickups.Count == 0 && _pickupViews.Count == 0) return;

        foreach (var pickup in _view.Pickups)
        {
            if (_pickupViews.TryGetValue(pickup.Id, out var existing) && IsInstanceValid(existing))
            {
                // The drop drifts once a player is close enough to pull it.
                existing.Position = pickup.Pos;
                continue;
            }
            _pickupViews[pickup.Id] = ScrapPickupView.Spawn(this, pickup.Type, pickup.Pos, pickup.Amount);
        }

        // Reused rather than rebuilt: this runs every frame a drop exists.
        _livePickups.Clear();
        foreach (var pickup in _view.Pickups) _livePickups.Add(pickup.Id);
        _collectedPickups.Clear();
        foreach (int id in _pickupViews.Keys)
            if (!_livePickups.Contains(id)) _collectedPickups.Add(id);
        foreach (int id in _collectedPickups)
        {
            if (_pickupViews.TryGetValue(id, out var view) && IsInstanceValid(view)) view.QueueFree();
            _pickupViews.Remove(id);
        }
    }

    /// <summary>Design ships the barricade in three states — intact, damaged,
    /// broken — for exactly this: a wall at a third of its health that still
    /// looks new tells the player nothing the bar has not already said, and
    /// the bar hides at distance. Same body swap as the trap plates; the wall
    /// has no rig, so nothing has to survive the swap.</summary>
    private void RefreshBarricadeArt(int structureId, string defId, float hpFraction, Node3D view)
    {
        if (defId != "barricade") return;
        string asset = hpFraction < BarricadeBrokenBelow ? "tower_barricade_broken"
            : hpFraction < BarricadeDamagedBelow ? "tower_barricade_damaged"
            : "tower_barricade";
        if ((string)view.GetMeta("structure_state", "tower_barricade") == asset) return;
        if (!AssetLibrary.Has(asset)) return;

        if (view.GetNodeOrNull<Node3D>("Body") is { } old)
        {
            view.RemoveChild(old);
            old.QueueFree();
        }
        var body = AssetLibrary.Instantiate(asset, () => Placeholders.Structure(defId));
        body.Name = "Body";
        view.AddChild(body);
        view.SetMeta("structure_state", asset);
    }

    private const float BarricadeDamagedBelow = 2f / 3f;
    private const float BarricadeBrokenBelow = 1f / 3f;

    /// <summary>Records what a player would actually see while a Ram works on a
    /// barricade: the bar's fraction, whether it is on screen, and whether the
    /// structure leaves the map when it falls. Passing needs all three — a bar
    /// visible at some partial health, and the view gone afterwards.</summary>
    private void WatchSiege()
    {
        var report = _siegeReport!;
        _siegeElapsed += (float)GetProcessDeltaTime();

        var barricade = _view.Structures.FirstOrDefault(s => s.DefId == "barricade");
        if (barricade is not null)
        {
            _siegeSawStructure = true;
            bool onScreen = _structureBars.TryGetValue(barricade.Id, out var bar)
                            && IsInstanceValid(bar) && bar.Visible;
            if (onScreen && barricade.HpFraction > 0.001f && barricade.HpFraction < 0.999f)
                _siegeSawBar++;

            // Photograph it half-eaten: full health hides the bar and rubble has
            // none, so there is exactly one interesting moment to catch.
            if (_siegeCapture && onScreen && barricade.HpFraction < 0.55f
                && _towerViews.TryGetValue(barricade.Id, out var wreck))
            {
                var target = wreck.Position;
                var eye = new Camera3D { Position = target + new Vector3(7f, 4.5f, 7f), Far = SkyFar };
                AddChild(eye);
                eye.LookAt(target + new Vector3(0f, 1.2f, 0f), Vector3.Up);
                eye.MakeCurrent();

                // Record what the bar is supposed to be reading, next to the
                // picture of it. Eyeballing a fill fraction off a screenshot is
                // how a bar that is drawing the wrong number looks fine.
                System.IO.File.WriteAllText(_siegeReportPath + ".txt",
                    $"captured at hp {barricade.HpFraction:0.000}, bar height {StructureBarHeight(wreck):0.00} m\n");

                _siegeReport = null;          // the picture is the report
                _shotPath = _siegeReportPath;
                _shotCountdown = 2;           // one frame for the camera to take
                return;
            }

            if (Mathf.FloorToInt(_siegeElapsed) > _siegeLastLog)
            {
                _siegeLastLog = Mathf.FloorToInt(_siegeElapsed);
                report.Add($"{_siegeElapsed:0}s hp {barricade.HpFraction:0.00} bar {(onScreen ? "shown" : "hidden")}");
            }
            if (_siegeElapsed < 120f) return;
            WriteProbe(report, _siegeReportPath, false,
                "INCONCLUSIVE: the barricade never fell, so removal was never exercised");
            return;
        }

        // Absent from the read model means two different things, and conflating
        // them made this probe pass by luck: PlaceTower is queued, so for the
        // first frames the barricade has not been built yet, and reading that
        // as "destroyed" reported a demolition at zero seconds that had not
        // happened. Only absence after it has been seen is destruction.
        if (!_siegeSawStructure)
        {
            if (_siegeElapsed < 120f) return;
            WriteProbe(report, _siegeReportPath, false,
                "INCONCLUSIVE: the barricade was never built, so nothing was measured");
            return;
        }

        // Gone after being there: the sim destroyed it. The view must have gone
        // with it, which is the half that was never tested before.
        bool viewGone = !_towerViews.Values.Any(v => IsInstanceValid(v)
                                                    && (string)v.GetMeta("def_id", "") == "barricade");
        report.Add($"{_siegeElapsed:0}s barricade destroyed | bar seen mid-demolition {_siegeSawBar} frames " +
                   $"| view removed {viewGone}");
        bool pass = _siegeSawBar > 0 && viewGone;
        WriteProbe(report, _siegeReportPath, pass, pass
            ? "PASS: the demolition was visible and the wreck left the map"
            : _siegeSawBar == 0
                ? "FAIL: the barricade fell without a health bar ever showing"
                : "FAIL: the structure was destroyed but its view is still standing");
    }

    /// <summary>Where the bar sits, measured off the model instead of assumed.
    /// Structures run from a flat trap plate to a Nova on a mast, so a single
    /// constant would float over the traps and sit inside the towers.
    ///
    /// Every corner of every mesh is transformed into the view's own space
    /// first. Taking a mesh's local AABB and adding that one node's position —
    /// which is what this did at first — ignores every level of nesting above
    /// it, and design's models are rigs several deep. The bar came out low
    /// enough to be buried in the barricade's top plate, which is the kind of
    /// thing only a screenshot tells you.</summary>
    private static float StructureBarHeight(Node3D view)
    {
        float top = 0f;
        var toLocal = view.GlobalTransform.AffineInverse();
        foreach (var visual in view.FindChildren("*", nameof(VisualInstance3D), true, false)
                     .OfType<VisualInstance3D>())
        {
            var aabb = visual.GetAabb();
            var xform = toLocal * visual.GlobalTransform;
            for (int corner = 0; corner < 8; corner++)
                top = Mathf.Max(top, (xform * aabb.GetEndpoint(corner)).Y);
        }
        return Mathf.Max(1.2f, top + 0.45f);
    }

    private void SyncEnemyViewsRemote(double delta)
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
            // Nothing in this game moves more than a metre between two
            // snapshots, so a jump of twenty is a warp gate and not a walk.
            // Measured rather than trusted to the event alone: the event line
            // and the snapshot arrive on different channels, and whichever
            // lands first, the enemy must not be seen gliding across the map.
            bool jumped = _warped.Remove(snap.Id)
                || (prevById.TryGetValue(snap.Id, out var before)
                    && before.Pos.DistanceTo(snap.Pos) > 20f);
            var target = !jumped && prevById.TryGetValue(snap.Id, out var prev)
                ? Vec3.Lerp(prev.Pos, snap.Pos, t)
                : snap.Pos;
            view.Position = ToGd(target);
            // Snapshots carry the yaw the server computed, so a client sees the
            // same turn rather than a differently-oriented crowd.
            if (jumped) view.Rotation = new Vector3(view.Rotation.X, GodotYaw(snap.Yaw), view.Rotation.Z);
            else TurnTowards(view, GodotYaw(snap.Yaw), delta);
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
        // A round that is no longer in the sim landed this tick; its view's
        // last position is the hit, and design's impact goes there.
        var live = new HashSet<int>(_world.Projectiles.Select(p => p.Id));
        foreach (var (id, view) in _projectileViews)
            if (!live.Contains(id) && IsInstanceValid(view))
                Vfx.ProjectileLanded((string)view.GetMeta("def_id", "lance"), view.Position);
        SweepViews(_projectileViews, _world.Projectiles.Select(p => p.Id));

        foreach (var trap in _world.Traps)
            RefreshTrapArt(trap.Id, trap.DefId, trap.ChargesLeft, Traps.All[trap.DefId].Charges);
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
        // A teammate who used a pad is somewhere else entirely; easing them
        // there walks their avatar through four buildings on the way.
        view.Position = view.Position.DistanceTo(pos) > 20f ? pos : view.Position.Lerp(pos, 0.35f);

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
    /// <summary>Yaw a view towards a heading, easing rather than snapping — an
    /// enemy that flips facing at a waypoint reads as a glitch, and the turn is
    /// most of what sells a corner.</summary>
    private static void FaceAlong(Node3D view, Vec3 facing, double delta)
    {
        if (facing.X * facing.X + facing.Z * facing.Z < 1e-4f) return;
        TurnTowards(view, GodotYaw(Mathf.Atan2(facing.X, facing.Z)), delta);
    }

    /// <summary>Set a heading outright. For arrivals: a view that was just
    /// put somewhere else has no previous heading worth easing from.</summary>
    private static void FaceAlongInstantly(Node3D view, Vec3 facing)
    {
        if (facing.X * facing.X + facing.Z * facing.Z < 1e-4f) return;
        view.Rotation = new Vector3(view.Rotation.X,
            GodotYaw(Mathf.Atan2(facing.X, facing.Z)), view.Rotation.Z);
    }

    private static void TurnTowards(Node3D view, float yaw, double delta)
    {
        float current = view.Rotation.Y;
        // Shortest way round, so a turn across the seam does not spin the long
        // way for no reason.
        float diff = Mathf.Wrap(yaw - current, -Mathf.Pi, Mathf.Pi);
        float step = Mathf.Min(1f, (float)delta * TurnRateRadians);
        view.Rotation = new Vector3(view.Rotation.X, current + diff * step, view.Rotation.Z);
    }

    private const float TurnRateRadians = 9f;

    /// <summary>The sim and the wire both express a heading as atan2(x, z).
    /// Godot models face -Z, so that heading is half a turn from the yaw a node
    /// actually needs — towers tracked correctly and pointed their backs at
    /// what they were shooting. Converted here, at the one place both the local
    /// and the snapshot paths pass through, rather than by changing the wire
    /// format and bumping the protocol over a rendering convention.</summary>
    private static float GodotYaw(float heading) => heading + Mathf.Pi;

    /// <summary>Towers track what they are shooting at. A turret frozen on its
    /// build angle while firing reads as broken, and the swing is the only cue
    /// a player has for what a tower has decided to prioritise.
    ///
    /// Aim is computed client-side from the rule the sim targets by — furthest
    /// along the route, in range, on a layer this tower can hit. It is
    /// cosmetic, so it sits on the pull-continuous side of the split rather
    /// than in a snapshot; a frame of disagreement costs nothing.</summary>
    private void AimTowers(double delta)
    {
        if (_world is null) return;
        // The probe stops itself. A verification run that can outlive the thing
        // it is verifying is how eight Godot processes once ran for a day.
        if (_aimReport is not null)
        {
            float was = _aimElapsed;
            _aimElapsed += (float)delta;
            if (Mathf.FloorToInt(_aimElapsed) > Mathf.FloorToInt(was))
            {
                float near = float.MaxValue;
                foreach (var t in _world.Towers)
                    foreach (var e in _world.Enemies)
                        near = Mathf.Min(near, t.Pos.DistanceTo(e.Pos));
                _aimReport.Add($"{_aimElapsed:0}s wave {_world.WaveIndex} {_world.Phase} " +
                               $"enemies={_world.Enemies.Count} towers={_world.Towers.Count} " +
                               $"views={_towerViews.Count} nearest={near:0.0}m");
                // A wave that has not started yields no target and the probe
                // reports nothing rather than an answer. Keep asking.
                if (_world.Enemies.Count == 0) Submit(new Command.StartWave(LocalPlayerId));
            }
            if (_aimElapsed > 90f) { FinishAimProbe(); return; }
        }

        foreach (var tower in _world.Towers)
        {
            if (!_towerViews.TryGetValue(tower.Id, out var view) || !IsInstanceValid(view)) continue;
            var def = Towers.All[tower.DefId];
            if (def.RangeMeters <= 0f) continue;      // a barricade has nothing to aim
            var rig = RigFor(tower.Id, view, tower.DefId);

            Enemy? best = null;
            float bestTraveled = -1f;
            foreach (var enemy in _world.Enemies)
            {
                if (enemy.Dead || enemy.Burrowed) continue;
                if (!def.TargetLayers.Contains(Enemies.All[enemy.DefId].Layer)) continue;
                if (tower.Pos.DistanceTo(enemy.Pos) > def.RangeMeters * 1.15f) continue;
                if (enemy.TotalTraveled <= bestTraveled) continue;
                bestTraveled = enemy.TotalTraveled;
                best = enemy;
            }
            if (best is null)                         // hold the last heading
            {
                rig.Spin(delta, engaged: false);
                continue;
            }
            rig.Spin(delta, engaged: true);
            bool settled;
            if (rig.Articulated)
            {
                // Aim at the body, not the feet: the sim keeps an enemy at its
                // spine base, and a barrel pitched at the ground under a Skiff
                // reads as pointing at nothing. Yaw, pitch, limits and slew
                // rates are design's (TowerRig); the whole view stays put so
                // the foot never turns.
                float height = _enemyViews.TryGetValue(best.Id, out var target) && target.HasMeta("head_height")
                    ? (float)target.GetMeta("head_height") * 0.5f
                    : 1.0f;
                settled = rig.AimAt(ToGd(best.Pos) + new Vector3(0f, height, 0f), delta);
            }
            else if (rig.Spins.Count > 0)
            {
                // An aura tower has no rig and never points — turning it would
                // lie about how it works.
                continue;
            }
            else
            {
                // A graybox has neither rig nor spin group and still turns as
                // a whole. The turn is eased, so the first frames on a new
                // target are legitimately off.
                var to = best.Pos - tower.Pos;
                if (to.X * to.X + to.Z * to.Z < 1e-4f) continue;
                TurnTowards(view, GodotYaw(Mathf.Atan2(to.X, to.Z)), delta);
                settled = _aimHeld > 0.5f;
            }

            if (_aimReport is not null)
            {
                var bore = rig.Articulated ? rig.Forward : -view.GlobalTransform.Basis.Z;
                var toTarget = ToGd(best.Pos) - ToGd(tower.Pos);
                var fwd = bore with { Y = 0 };
                var want = toTarget with { Y = 0 };
                if (fwd.Length() > 0.01f && want.Length() > 0.01f)
                {
                    float err = Mathf.Abs(Mathf.RadToDeg(
                        fwd.Normalized().SignedAngleTo(want.Normalized(), Vector3.Up)));

                    // Yaw alone was the whole measurement for months, and it
                    // cannot see the axis that matters against a flyer: both
                    // vectors had their Y flattened before the angle was taken,
                    // so a turret aimed flat at a target fifteen metres up
                    // scored a perfect zero. Elevation is measured on its own.
                    float haveEl = Mathf.RadToDeg(Mathf.Asin(
                        Mathf.Clamp(bore.Normalized().Y, -1f, 1f)));
                    float wantEl = Mathf.RadToDeg(Mathf.Asin(
                        Mathf.Clamp(toTarget.Normalized().Y, -1f, 1f)));
                    float pitchErr = Mathf.Abs(wantEl - haveEl);
                    if (settled) _aimWorstPitch = Mathf.Max(_aimWorstPitch, pitchErr);
                    // A rigged turret slews at design's rate — a Lance needs two
                    // seconds for a half turn — so only frames where the rig is
                    // no longer rate-limited say whether it faces what it is
                    // shooting. Frames spent swinging are the design, not a miss.
                    _aimHeld += (float)delta;
                    _aimReport.Add($"{_aimHeld:0.00}s {tower.DefId} -> {best.DefId} "
                        + $"yaw={err:0.0} pitch={pitchErr:0.0} (want {wantEl:0.0} have {haveEl:0.0})"
                        + $"{(settled ? "" : " (slewing)")}");
                    if (settled) { _aimWorst = Mathf.Max(_aimWorst, err); _aimSettled++; }
                }
                if (_aimSettled >= _aimSampleTarget) { FinishAimProbe(); return; }
            }
        }
    }

    /// <summary>Resolved once per view; a rebuilt view (upgrade) resolves again
    /// because the modules' parts were merged onto new rig nodes.</summary>
    private TowerRig RigFor(int towerId, Node3D view, string defId)
    {
        if (_towerRigs.TryGetValue(towerId, out var cached) && cached.View == view) return cached.Rig;
        var rig = TowerRig.Resolve(view, defId);
        _towerRigs[towerId] = (view, rig);
        return rig;
    }

    /// <summary>Writes the aim probe's samples and its verdict, then quits with
    /// a failing status if any settled sample was more than five degrees off.
    /// Five is generous — the eased turn overshoots slightly — but it is two
    /// orders of magnitude away from the 180 this was written to catch.</summary>
    private void FinishAimProbe()
    {
        var report = _aimReport!;
        _aimReport = null;
        // Ten degrees of elevation error on a target fifteen metres up is a
        // barrel visibly pointing past it. Yaw is held tighter because nothing
        // rate-limits it once settled.
        bool aimed = _aimSettled > 0 && _aimWorst < 5f && _aimWorstPitch < 10f;
        report.Add($"worst yaw error once settled: {_aimWorst:0.0} deg over {_aimSettled} samples");
        report.Add($"worst elevation error once settled: {_aimWorstPitch:0.0} deg");
        WriteProbe(report, _aimReportPath, aimed, _aimSettled == 0
            ? "INCONCLUSIVE: no tower ever held a target long enough to measure"
            : aimed ? "PASS: the turret faces what it is shooting, in both axes"
            : _aimWorst >= 5f ? "FAIL: the turret is tracking but pointed away in yaw"
                              : $"FAIL: the turret cannot elevate onto its target "
                                + $"({_aimWorstPitch:0.0} deg short)");
    }

    private void SnapshotKills()
    {
        _killsAtWaveStart.Clear();
        if (!_view.Valid) return;
        foreach (var player in _view.Players) _killsAtWaveStart[player.Id] = player.Kills;
    }

    /// <summary>What each connected player killed during the wave just ended.
    /// A player who joined mid-wave has no snapshot, so their whole total
    /// counts — everything they have is from this wave anyway.</summary>
    private List<(string Name, int Kills)> KillsThisWave()
    {
        var rows = new List<(string Name, int Kills)>();
        if (!_view.Valid) return rows;
        foreach (var player in _view.Players)
        {
            if (!player.Connected) continue;
            int before = _killsAtWaveStart.TryGetValue(player.Id, out int at) ? at : 0;
            rows.Add((player.Name, Mathf.Max(0, player.Kills - before)));
        }
        return rows;
    }

    private void Stage(int frames, System.Action step) => _shotStages.Enqueue((frames, step));

    /// <summary>Arms the capture and records, beside the picture, whether the
    /// surface it is supposed to be of is actually open. Every one of these
    /// presets has at some point photographed a different screen than the one
    /// it was named for, and a png cannot fail a build.</summary>
    private void ShootSurface(string path, string surface, bool open)
    {
        System.IO.File.WriteAllText(path + ".txt",
            open ? $"{surface.ToUpperInvariant()} OPEN\n" : $"{surface.ToUpperInvariant()} NOT OPEN\n");
        _shotPath = path;
        _shotCountdown = 2;
    }

    /// <summary>Two ground sockets near the lane, for shots that want a defence
    /// rather than a single tower.</summary>
    private List<SocketDef> TwoBuildableSockets()
    {
        var ranked = new List<(SocketDef Socket, float Distance)>();
        foreach (var s in _map.Sockets)
        {
            if (s.Tag != SocketTag.Ground) continue;
            float best = float.MaxValue;
            foreach (var route in _map.Routes)
            {
                if (route.Layer != EnemyLayer.Ground) continue;
                foreach (var w in route.Waypoints) best = Mathf.Min(best, s.Pos.DistanceTo(w));
            }
            ranked.Add((s, best));
        }
        ranked.Sort((a, b) => a.Distance.CompareTo(b.Distance));
        return ranked.Take(2).Select(r => r.Socket).ToList();
    }

    /// <summary>Watches for the wave to end so the intermission shot catches the
    /// panel with a recap behind it.</summary>
    private void TickIntermissionShot()
    {
        if (_intermissionShotPath is null || _view is not { Valid: true }) return;

        // Shoot while the wave runs, but not at every opportunity. Left alone,
        // the towers do all the killing and the recap's new line is a truthful
        // nought that demonstrates nothing; firing flat out, the hero takes the
        // entire wave and it demonstrates the opposite falsehood. A recap is a
        // split, so the shot should be of one — this is the harness bot's
        // Uptime idea with the arithmetic done by a counter.
        //
        // The fire goes through the ordinary PlayerHit path, so the sim still
        // refuses anything out of range or on cooldown: a hero shooting, not
        // kills being handed out.
        // One shot per four seconds of game time. The divisor has to be this
        // large to bite at all: the wave runs at eight times speed, so a game
        // second is only 7.5 frames, and the sidearm's own three-a-second
        // cooldown swallowed a quarter-cadence whole — throttling to one frame
        // in four moved the split by nothing, which is the tell that the dial
        // was not connected to the outcome.
        //
        // Cadence is the right lever rather than range or aim, because the
        // sidearm reaches sixty metres against a lance's twelve: an
        // unthrottled hero picks the entire lane clean from wherever it stands.
        const int framesPerShot = 12;         // 7.5 frames per game second, so ~1.6s
        if (_world is not null && _view.Phase == MatchPhase.Wave
            && _intermissionFireBeat++ % framesPerShot == 0
            && _world.Players.TryGetValue(LocalPlayerId, out var shooter))
        {
            Enemy? nearest = null;
            float best = float.MaxValue;
            foreach (var enemy in _world.Enemies)
            {
                if (enemy.Dead || enemy.Burrowed) continue;
                float d = shooter.Pos.DistanceTo(enemy.Pos);
                if (d < best) { best = d; nearest = enemy; }
            }
            if (nearest is not null)
                Submit(new Command.PlayerHit(LocalPlayerId, nearest.Id, shooter.WeaponId));
        }

        if (_view.Wave < 0 || _view.Phase != MatchPhase.Intermission) return;
        if (_view.Wave <= _intermissionLastWave) return;    // same intermission, later frame
        _intermissionLastWave = _view.Wave;

        // Shoot the second intermission, not the first. After one wave a
        // per-wave count and a match total are the same number, so a shot there
        // proves nothing about the subtraction that makes "last wave" true.
        if (_view.Wave == 0)
        {
            Submit(new Command.StartWave(LocalPlayerId));
            return;
        }

        Engine.TimeScale = 1f;
        string path = _intermissionShotPath;
        _intermissionShotPath = null;
        // One frame at normal speed before the shot, so the panel is laid out
        // and the world behind it is not mid-blur from the fast-forward.
        Stage(2, () =>
        {
            // Record the recap's numbers beside the picture, so the kill line
            // can be checked against the sim rather than read off a screenshot.
            string rows = string.Join(", ", KillsThisWave().Select(r => $"{r.Name}={r.Kills}"));
            ShootSurface(path, "intermission", _view.Phase == MatchPhase.Intermission);
            string totals = string.Join(", ", _view.Players.Select(pl => $"{pl.Name}={pl.Kills}"));
            int hero = KillsThisWave().Sum(r => r.Kills);
            int towers = Mathf.Max(0, _lastWaveEnemyCount - _lastWaveLeaks - hero);
            System.IO.File.AppendAllText(path + ".txt",
                $"wave {_view.Wave} recap: {_lastWaveEnemyCount} inbound, " +
                $"hero {hero}, towers {towers}, leaked {_lastWaveLeaks}\n" +
                $"kills this wave {rows} | match totals {totals}\n");
        });
    }

    private void TickShotStages()
    {
        if (_shotStages.Count == 0) return;
        var (frames, step) = _shotStages.Peek();
        if (_stageWaited < frames) { _stageWaited++; return; }
        _shotStages.Dequeue();
        _stageWaited = 0;
        step();
    }

    /// <summary>Stands the hero where a surface's subject is in shot. Sockets
    /// and towers are on the ground, so this backs off along the map's own
    /// forward and looks slightly down.</summary>
    private void AimAt(Vec3 target, float back, float height)
    {
        if (_player is null) return;
        var t = ToGd(target);
        _player.AimFrom(t + ClearOffset(t, back, height), t);
    }

    /// <summary>Where to stand to actually see <paramref name="target"/>.
    ///
    /// Every review shot used to back off along the same fixed diagonal, which
    /// works on open deck and fails silently everywhere else: on Foundry the
    /// socket nearest the lane sits beside a raised walkway, so the "picture of
    /// a maxed tower" was a picture of the walkway's underside. Nothing in the
    /// log said so — the tower was built, the panel was open, the shot was
    /// written. Only the image knew.
    ///
    /// So try twelve headings and take the first with an unobstructed line to
    /// the subject, falling back to the old diagonal when the subject is boxed
    /// in on every side.</summary>
    private Vector3 ClearOffset(Vector3 target, float back, float height)
    {
        var space = GetWorld3D().DirectSpaceState;
        var eye = target + new Vector3(0f, 1.2f, 0f);
        for (int i = 0; i < 12; i++)
        {
            float angle = Mathf.Tau * i / 12f;
            var offset = new Vector3(Mathf.Sin(angle) * back, height, Mathf.Cos(angle) * back);
            var query = PhysicsRayQueryParameters3D.Create(target + offset, eye, collisionMask: 1);
            if (space.IntersectRay(query).Count == 0) return offset;
        }
        return new Vector3(back * 0.7f, height, back * 0.7f);
    }

    /// <summary>The ground socket closest to a walked route — where a player
    /// would actually build, and therefore what these shots should be of.
    /// Sockets[0] is whatever the map file happens to list first, which on
    /// Foundry is nowhere near the fighting.</summary>
    private SocketDef BuildableSocket()
    {
        SocketDef best = _map.Sockets[0];
        float bestD = float.MaxValue;
        foreach (var s in _map.Sockets)
        {
            if (s.Tag != SocketTag.Ground || _view.SocketOccupied(s.Id)) continue;
            foreach (var route in _map.Routes)
            {
                if (route.Layer != EnemyLayer.Ground) continue;
                foreach (var w in route.Waypoints)
                {
                    float d = s.Pos.DistanceTo(w);
                    if (d < bestD) { bestD = d; best = s; }
                }
            }
        }
        return best;
    }

    /// <summary>Writes a probe's samples and verdict, then quits with a status
    /// CI can read. The file matters: Godot buffers stdout and mono's headless
    /// teardown aborts before flushing it, so a probe that only printed would
    /// report nothing at all on the machine that most needs to hear it.</summary>
    private static float LadderHalfHeight(Area3D area)
    {
        foreach (var child in area.GetChildren())
            if (child is CollisionShape3D shape && shape.Shape is BoxShape3D box)
                return box.Size.Y * 0.5f;
        return 3f;
    }

    /// <summary>A ladder named by where it is, so the report reads as a place
    /// rather than as an engine node id.</summary>
    private static string Describe(Area3D area)
    {
        var p = area.GlobalPosition;
        return $"ladder at ({p.X:0.#}, {p.Y:0.#}, {p.Z:0.#})";
    }

    /// <summary>Somewhere to step off at the top: the nearest wall socket
    /// within reach of the rungs' head, or nothing, in which case the probe
    /// just lets go and sees whether the player is left standing.</summary>
    private Vector3? StepOffTarget(Vector3 head)
    {
        Vector3? best = null;
        float bestDistance = 10f * 10f;
        foreach (var socket in _map.Sockets)
        {
            if (socket.Tag != SocketTag.Wall) continue;
            var pos = ToGd(socket.Pos);
            if (Mathf.Abs(pos.Y - head.Y) > 4f) continue;
            float distance = (pos with { Y = 0f } - head with { Y = 0f }).LengthSquared();
            if (distance >= bestDistance) continue;
            bestDistance = distance;
            best = pos;
        }
        return best;
    }

    /// <summary>Every ladder volume in the level, in a stable order.</summary>
    private List<Area3D> FindLadderAreas()
    {
        var found = new List<Area3D>();
        void Walk(Node n)
        {
            if (n is Area3D area && (string)area.GetMeta("kind", "") == "ladder") found.Add(area);
            foreach (var child in n.GetChildren()) Walk(child);
        }
        Walk(this);
        found.Sort((a, b) => a.GlobalPosition.X != b.GlobalPosition.X
            ? a.GlobalPosition.X.CompareTo(b.GlobalPosition.X)
            : a.GlobalPosition.Z.CompareTo(b.GlobalPosition.Z));
        return found;
    }

    /// <summary>Drives the probe: put the player at the foot of each climb,
    /// hold the climb for four seconds, and record where they ended up and
    /// whether they were standing on anything when they got there.</summary>
    // =====================================================================
    // Vehicle probe
    // =====================================================================

    private List<string>? _vehicleReport;
    private string _vehicleReportPath = "";
    private int _vehicleIndex;
    private float _vehicleTimer;
    private readonly List<string> _undrivable = new();

    /// <summary>Every vehicle on the map: get in, drive, turn, hit the edge of
    /// the world, get out standing.
    ///
    /// Each of those is a separate thing that can be silently wrong and none
    /// of them is visible in a screenshot. A seat the sim refuses looks like a
    /// player standing next to a car. A vehicle that moves on the driver's
    /// screen and nowhere else looks perfect until someone else is watching.
    /// A containment wall that is decoration looks like a map right up to the
    /// moment somebody drives off it at twenty metres a second.</summary>
    private void TickVehicleProbe(double delta)
    {
        if (_vehicleReport is null || _player is null || _map.Vehicles.Count == 0) return;

        if (_vehicleIndex >= _map.Vehicles.Count)
        {
            bool pass = _undrivable.Count == 0;
            _vehicleReport.Add(pass
                ? "every vehicle takes a driver, moves, turns, stops at the edge and lets them out"
                : "undrivable: " + string.Join("; ", _undrivable));
            WriteProbe(_vehicleReport, _vehicleReportPath, pass,
                pass ? "PASS: every vehicle on this map can be driven"
                     : "FAIL: a vehicle does not do what a vehicle has to do");
            return;
        }

        var spawn = _map.Vehicles[_vehicleIndex];
        var node = VehicleNode(spawn.Id);
        if (node is null)
        {
            _undrivable.Add($"{spawn.Id} has no node at all");
            _vehicleIndex++;
            _vehicleTimer = 0f;
            return;
        }

        float before = _vehicleTimer;
        _vehicleTimer += (float)delta;

        // Stand beside it. The sim measures the distance off the position the
        // player last streamed, so the walk over has to happen a tick before
        // the ask — which is exactly what a player pressing E has already done
        // and a probe that teleports has not.
        if (before <= 0f)
        {
            _player.GlobalPosition = node.GlobalPosition + Vector3.Up * 1f;
            _vehicleStart = node.GlobalPosition;
            _vehicleStartYaw = node.Rotation.Y;
            return;
        }
        if (_vehicleTimer < 0.3f) return;
        if (before < 0.3f)
        {
            Submit(new Command.EnterVehicle(LocalPlayerId, spawn.Id, 0));
            return;
        }
        if (_vehicleTimer < 0.8f) return;
        if (before < 0.8f)
        {
            if (_view.SeatOf(LocalPlayerId) is not { } seated || seated.Vehicle.Id != spawn.Id)
            {
                _undrivable.Add($"{spawn.Id} refused the driver's seat");
                _vehicleIndex++;
                _vehicleTimer = 0f;
                return;
            }
            node.ThrottleHeld = 1f;
            return;
        }

        // Three seconds flat out, then two turning.
        if (_vehicleTimer < 3.8f) return;
        if (before < 3.8f)
        {
            _vehicleMoved = _vehicleStart.DistanceTo(node.GlobalPosition);
            // What the handling table says this thing should be doing on the
            // ground it is on. Checked, because the surface multipliers are
            // the whole difference between four vehicles and one vehicle with
            // four models, and nothing else would notice them going missing.
            _vehicleTopSpeed = node.Def.TopSpeed * node.Def.Surfaces[node.SurfaceUnder].TopSpeed;
            _vehicleReached = node.Speed;
            node.SteerHeld = 1f;
            return;
        }
        if (_vehicleTimer < 5.8f) return;
        if (before < 5.8f)
        {
            _vehicleTurned = Mathf.RadToDeg(Mathf.Abs(
                Mathf.Wrap(node.Rotation.Y - _vehicleStartYaw, -Mathf.Pi, Mathf.Pi)));
            // Aim at the nearest edge and hold the throttle down: the wall has
            // to be the thing that stops it.
            node.SteerHeld = 0f;
            float toEast = _map.HalfX - node.GlobalPosition.X;
            float toWest = _map.HalfX + node.GlobalPosition.X;
            node.Rotation = new Vector3(0,
                Mathf.Atan2(toEast < toWest ? -1f : 1f, 0f) + Mathf.Pi * 0.5f, 0);
            return;
        }
        if (_vehicleTimer < 12f) return;
        if (before < 12f)
        {
            // Out, and then a beat: the player is put back beside the vehicle
            // on the frame the seat clears, and moving them to the next one in
            // that same frame has them teleported away and dropped back.
            Submit(new Command.ExitVehicle(LocalPlayerId));
            node.ThrottleHeld = 0f;
            node.SteerHeld = 0f;
            return;
        }
        if (_vehicleTimer < 12.6f) return;

        bool contained = Mathf.Abs(node.GlobalPosition.X) <= _map.HalfX
            && Mathf.Abs(node.GlobalPosition.Z) <= _map.HalfZ
            && node.GlobalPosition.Y > -2f;
        bool gotOut = _view.SeatOf(LocalPlayerId) is null && _player.Standing;
        if (!gotOut) _undrivable.Add($"{spawn.Id} did not let the driver out standing");

        // The sim has to have been told where it went, or a passenger's client
        // is watching a parked car while the driver is a field away.
        float simOff = _world?.Vehicles.FirstOrDefault(v => v.Id == spawn.Id) is { } known
            ? ToGd(known.Pos).DistanceTo(node.GlobalPosition) : 999f;

        // Deliberately generous, because this is a correctness check and not
        // a tuning one: it has to pass the worst-handling vehicle on the worst
        // surface it can be parked on. The Buggy in a field does nine metres
        // in three seconds and turns forty-four degrees in two, and that is
        // the Buggy working — it is a road car with the handling of one.
        // What is measured tightly instead is whether it got as fast as the
        // table says it should on the ground it was on.
        bool quick = _vehicleReached > _vehicleTopSpeed * 0.55f;
        bool ok = _vehicleMoved > 6f && _vehicleTurned > 25f && quick
            && contained && simOff < 3f && gotOut;
        _vehicleReport.Add($"{spawn.Id} ({spawn.DefId}): drove {_vehicleMoved:0} m in 3 s on "
            + $"{node.SurfaceUnder.ToString().ToLowerInvariant()}, reached {_vehicleReached:0.0} "
            + $"of {_vehicleTopSpeed:0.0} m/s, turned {_vehicleTurned:0}° in 2 s, "
            + (contained ? "held inside the field" : "LEFT THE FIELD")
            + $", the sim has it {simOff:0.0} m from where it is");
        if (!quick)
            _undrivable.Add($"{spawn.Id} reached {_vehicleReached:0.0} m/s where its table "
                + $"says {_vehicleTopSpeed:0.0} on {node.SurfaceUnder}");
        if (!ok)
            _undrivable.Add($"{spawn.Id} moved {_vehicleMoved:0} m in 3 s, turned {_vehicleTurned:0}°, "
                + (contained ? "" : "escaped the field, ") + $"sim off by {simOff:0.0} m");

        _vehicleIndex++;
        _vehicleTimer = 0f;
    }

    /// <summary>A surface answer with the answer it should have given, so the
    /// report says whether the road lines are where the map thinks they are
    /// rather than leaving a reader to check four coordinates by hand.</summary>
    private string SurfaceLine(string what, Vector3 at, Surface expected)
    {
        var got = Surfaces.At(at);
        if (got != expected) _undrivable.Add($"{what} at ({at.X:0},{at.Z:0}) reads {got}, not {expected}");
        return $"{what} at ({at.X:0},{at.Z:0}) is {got}" + (got == expected ? "" : $" — expected {expected}");
    }

    private Vector3 _vehicleStart;
    private float _vehicleStartYaw;
    private float _vehicleMoved;
    private float _vehicleTurned;
    private float _vehicleTopSpeed;
    private float _vehicleReached;

    // =====================================================================
    // Teleport probe
    // =====================================================================

    private List<string>? _teleportReport;
    private string _teleportReportPath = "";
    private float _teleportTimer;

    /// <summary>Does the pad network actually move a player, and does it then
    /// refuse to do it again?
    ///
    /// Both halves have to be measured, and neither is visible in a
    /// screenshot. The Spire has shipped teleport pads since M3 that were
    /// coloured boxes: the asset name never resolved, the pad id was on the
    /// wrong node, and no code handled the area kind at all. Every review shot
    /// of that map contained two pads and none of them were pads.</summary>
    private void TickTeleportProbe(double delta)
    {
        if (_teleportReport is null || _player is null || _teleportPads.Count < 2) return;
        float before = _teleportTimer;
        _teleportTimer += (float)delta;

        var from = _teleportPads[0];
        var to = _teleportPads[1];

        // Stand on the first pad and hold E: the picker has to open, which is
        // what proves the area, its pad_id and the hold both work.
        if (before <= 0f)
        {
            _player.GlobalPosition = from.At + Vector3.Up * 0.6f;
            _player.HoldingBuild = false;
            return;
        }
        if (_teleportTimer < 0.6f) return;
        if (before < 0.6f)
        {
            _player.HoldingBuild = true;
            return;
        }
        if (_teleportTimer < 1.0f) return;
        if (before < 1.0f)
        {
            if (!PickerOpen)
            {
                _teleportReport.Add($"holding E on {from.Id} did not open the picker");
                WriteProbe(_teleportReport, _teleportReportPath, false,
                    "FAIL: a pad underfoot offered nothing");
                return;
            }
            // Pick the pad we mean rather than whatever is nearest, so the
            // arrival can be checked against a known destination.
            int index = 0;
            for (int i = 1; i < _teleportPads.Count; i++)
                if (_teleportPads[i].Id == to.Id) index = i - 1;
            WheelSelect(index);
            _player.HoldingBuild = false;       // release commits
            _teleportReport.Add($"picker opened on {from.Id}, chose {to.Id}");
            return;
        }

        // The charge is a second and a half of standing still; give it three.
        if (_teleportTimer < 3.2f) return;
        if (before < 3.2f)
        {
            float off = _player.GlobalPosition.DistanceTo(to.At);
            bool arrived = off < 2.5f && _player.Standing;
            _teleportReport.Add($"{from.Id} -> {to.Id}: arrived {off:0.0} m off, "
                + (_player.Standing ? "standing" : "NOT STANDING")
                + $", cooldown {_player.TeleportCooldown:0.0}s");
            // The sim has to agree: a player who moved and never told it is a
            // player taking contact damage where they used to be.
            if (_world is not null && _world.Players.TryGetValue(LocalPlayerId, out var seen))
            {
                float simOff = ToGd(seen.Pos).DistanceTo(to.At);
                _teleportReport.Add($"the sim has them {simOff:0.0} m from {to.Id}");
                if (simOff > 4f) arrived = false;
            }
            if (!arrived)
            {
                WriteProbe(_teleportReport, _teleportReportPath, false,
                    "FAIL: the network did not put the player where it said");
                return;
            }
            // And now it must refuse: the cooldown is the only thing stopping
            // a pad network from deleting the distance the map is made of.
            _player.HoldingBuild = true;
            return;
        }

        if (_teleportTimer < 3.8f) return;
        bool refused = !PickerOpen;
        _player.HoldingBuild = false;
        _teleportReport.Add(refused
            ? $"holding E again was refused, {_player.TeleportCooldown:0}s left on the cooldown"
            : "the picker opened again while the cooldown was still running");
        WriteProbe(_teleportReport, _teleportReportPath, refused,
            refused ? "PASS: the network moves you once and then makes you wait"
                    : "FAIL: the cooldown does not hold");
    }

    private void TickTraversalProbe(double delta)
    {
        if (_traversalReport is null || _player is null) return;

        if (_traversalIndex >= _climbs.Count)
        {
            // The question every map can answer: does each ladder leave you
            // standing on something at the top of its own rungs? Asking
            // instead that every wall socket sit under a ladder would be a
            // lie on the Spire, where the routes climb and you walk the tiers.
            bool pass = _climbs.Count > 0 && _unreachable.Count == 0;
            _traversalReport.Add(_unreachable.Count == 0
                ? "every ladder leaves you standing at the top of it"
                : "ladders that go nowhere: " + string.Join("; ", _unreachable));
            WriteProbe(_traversalReport, _traversalReportPath, pass,
                pass ? "PASS: every climb this map offers lands on something"
                     : "FAIL: a climb tops out against the thing it was meant to reach");
            return;
        }

        var climb = _climbs[_traversalIndex];
        if (_traversalTimer <= 0f)
        {
            _player.GlobalPosition = climb.Base;
            _player.ClimbHeld = true;
            _player.WalkHeld = Vector3.Zero;
        }
        _traversalTimer += (float)delta;

        // Climb, then walk at the deck this ladder serves and let go: if there
        // is something to stand on, they end up standing on it at its height.
        if (_traversalTimer < 4f) return;
        if (_traversalTimer < 6.5f)
        {
            _player.ClimbHeld = false;
            // Step off toward whatever the rungs' head is next to.
            if (StepOffTarget(climb.Deck) is { } target)
            {
                var toDeck = (target - _player.GlobalPosition) with { Y = 0f };
                _player.WalkHeld = toDeck.LengthSquared() > 0.01f ? toDeck : Vector3.Zero;
            }
            return;
        }

        _player.WalkHeld = Vector3.Zero;
        float reached = _player.GlobalPosition.Y;
        // Two metres of slack: the step off lands you on the deck, which sits
        // a little under the head of the rungs.
        bool landed = _player.Standing && reached >= climb.Deck.Y - 2f;
        _traversalReport.Add($"{climb.Id}: rungs end at {climb.Deck.Y:0.0} m, " +
            $"left standing at {reached:0.0} m " + (landed ? "-> lands on it" : "-> GOES NOWHERE"));
        if (!landed)
            _unreachable.Add($"{climb.Id} tops out at {reached:0.0} m for rungs ending at {climb.Deck.Y:0.0} m");
        _traversalIndex++;
        _traversalTimer = 0f;
    }

    private void WriteProbe(List<string> report, string path, bool pass, string verdict)
    {
        report.Add(verdict);
        System.IO.File.WriteAllLines(path, report);
        GetTree().Quit(pass ? 0 : 1);
    }

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
        int nodesBefore = GetTree().GetNodeCount();
        BuildEnvironment(map);

        // Ground slab — one collider, dressed with the 20 m terrain tiles.
        // Tiles are laid symmetrically so the slab is covered edge to edge;
        // an off-centre run leaves bare collider showing at one end.
        var ground = AddStaticBox(new Vector3(0, -0.5f, 0),
            new Vector3(map.FieldX, 1, map.FieldZ), new Color(0.35f, 0.38f, 0.4f), layer: 1);
        if (AssetLibrary.Has($"{map.Id}_terrain"))
        {
            MapKit.HideBox(ground);
            // Tile centres from the field rather than from a literal. The old
            // -50..50 by -30..30 is what this produces for a 110 x 80 map, so
            // the three maps that were that size are laid exactly as before.
            float lastX = Mathf.Ceil(map.HalfX / 20f) * 20f - 10f;
            float lastZ = Mathf.Ceil(map.HalfZ / 20f) * 20f - 10f;
            // One instanced draw per distinct mesh instead of a subtree per
            // copy. Switchyard's tile is 221 parts, so twenty-four of them was
            // five thousand nodes and five thousand draw calls for one flat
            // field; the Toaster's field is six times the size. The two things
            // that tile needed doing to it per copy both survive: the sidings
            // come off the prototype before anything is measured, and the
            // half-turn that breaks the lattice is in the transform.
            var tiles = new List<Transform3D>();
            for (float x = -lastX; x <= lastX + 0.01f; x += 20f)
                for (float z = -lastZ; z <= lastZ + 0.01f; z += 20f)
                {
                    // One tile repeated puts its puddle, its weed tufts and its
                    // drain grates on a perfect lattice, which reads as
                    // wallpaper rather than ground. A half-turn on alternate
                    // tiles breaks the repeat and keeps the drain and the
                    // ballast grain running east-west with the yard, which a
                    // quarter-turn would not.
                    bool turned = map.Id == "switchyard"
                        && ((int)(x / 20f) + (int)(z / 20f)) % 2 != 0;
                    var basis = turned
                        ? Basis.FromEuler(new Vector3(0, Mathf.Pi, 0))
                        : Basis.Identity;
                    tiles.Add(new Transform3D(basis,
                        new Vector3(x, MapKit.GroundLocal(ground), z)));
                }

            // The tile bakes in two "disused sidings" at its local z +/-6.
            // Tiled six by four, that is eight full-width tracks laid straight
            // across the map on a grid — through the lanes, through the
            // sockets, through the spawn, with no throat, no turnout and no
            // buffer at either end. They out-number and contradict every metre
            // of track laid deliberately, which is the single biggest reason
            // this map read as a railway that makes no sense. The ballast bed
            // is what the tile is for; the track is laid in
            // BuildSwitchyardRailway or it does not exist.
            string[] buried = map.Id == "switchyard"
                ? new[] { "terrain_siding-6", "terrain_siding6" }
                : System.Array.Empty<string>();

            int drawn = MapKit.InstancedChunked(ground, $"{map.Id}_terrain", tiles, 80f,
                castShadow: false, visibleFrom: 0f, visibleTo: 0f, hide: buried);
            GD.Print($"[map] terrain: {tiles.Count} tiles in {drawn} multimesh(es)");
        }

        // Lane surfaces.
        foreach (var route in map.Routes)
        {
            bool air = route.Layer == EnemyLayer.Air;
            var color = air ? new Color(0.5f, 0.6f, 0.9f, 0.25f) : new Color(0.2f, 0.22f, 0.27f);
            for (int i = 0; i < route.Waypoints.Count - 1; i++)
            {
                // A warp leg is not walked, so there is no road to draw. Laying
                // one would paint a two-hundred-metre diagonal of roadway
                // across the fields, through the buildings, joining two pads
                // that are deliberately nowhere near each other.
                if (route.IsTeleportLeg(i)) continue;
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
                {
                    MapKit.MountRun(box, $"{map.Id}_path_ground", (b - a).Length(), 4f,
                        alongX: true, MapKit.GroundLocal(box), 0f,
                        "lane_marker", "lane_marker_cap");
                    // Switchyard's lane module carries a rail down its centre,
                    // and the routes it is laid along turn square corners —
                    // which is why the map read as a railway that made no
                    // sense. The lane keeps its ballast, kerbs and markers and
                    // loses the rail: it is the yard's haul road, and the
                    // actual track is laid as track in BuildSwitchyardRailway.
                    if (map.Id == "switchyard") MapKit.HideNamed(box, "lane_track");
                }
            }

            if (air) BuildAirLaneSupports(route);
        }

        BuildLaneMouths(map);
        BuildWarpGates(map);

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
            if (_auditSockets && body.GetNodeOrNull<Node3D>("SocketArt") is { } pad)
                GD.Print($"[socket] {socket.Id} tag={socket.Tag} socketY={body.Position.Y:0.00} "
                    + $"padY={pad.GlobalPosition.Y:0.00}");
        }

        if (map.Id == "foundry") BuildFoundryStructures();
        if (map.Id == "switchyard") BuildSwitchyardStructures();
        if (map.Id == "spire") BuildSpireStructures();
        if (map.Id == "toaster") BuildToasterStructures(map);

        BuildVehicles(map);

        // Armory station.
        var armory = AddStaticBox(ToGd(map.ArmoryPos) + new Vector3(0, 1.25f, 0),
            new Vector3(2.5f, 2.5f, 2.5f), new Color(0.8f, 0.7f, 0.2f), layer: 1);
        armory.AddChild(MakeArea("armory", new BoxShape3D { Size = new Vector3(7, 4, 7) }));
        // Sits on the ground like every other kit piece and faces the spot
        // players spawn at. The +2.1 that used to be here was a guess at a
        // pivot offset and left the kiosk hanging in mid-air.
        MapKit.Mount(armory, "shared_armory_kiosk", MapKit.GroundLocal(armory),
            MapKit.YawTowards(ToGd(map.HeroSpawn) - ToGd(map.ArmoryPos)));

        // What the map cost, in the currency that actually scales. Every
        // placement in this client used to be a node subtree, so a field was
        // thousands of them and nobody ever counted; a number in the log is
        // the cheapest possible ratchet against a map that quietly doubles.
        GD.Print($"[map] {map.Id} built {GetTree().GetNodeCount() - nodesBefore} nodes "
            + $"on a {map.FieldX:0} x {map.FieldZ:0} m field");
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
                var core = MapKit.Prop(this, "shared_core", end, MapKit.YawTowards(-inbound));
                if (core is not null) { core.Name = "Core"; _coreView = core; }
                // The hit state is a second model, hidden until a leak lands.
                var struck = MapKit.Prop(this, "shared_core_hit", end, MapKit.YawTowards(-inbound));
                if (struck is not null) { struck.Name = "CoreHit"; struck.Visible = false; _coreHitView = struck; }
            }
        }
        _laneMouths = gates.Concat(cores).ToList();
    }

    /// <summary>A gate at each end of every teleport leg.
    ///
    /// The sim moves an enemy from one pad to the other in a tick; without
    /// something drawn there it reads as enemies vanishing in an empty field
    /// and a different wave appearing in another one. The silhouette is
    /// deliberately not the player's teleporter — that is a flat hex pad a
    /// player has been taught to stand on and hold E, and a map must never put
    /// a thing that means "stand here" where the answer is "do not". Until
    /// design ships the arch, the spawn portal stands in: it already reads as
    /// "enemies come out of here", which is exactly right at the far end and
    /// forgivable at the near one.
    ///
    /// Both ends join the lane mouths, so scenery, scatter and the boundary
    /// wall leave them the same hole they leave a spawn gate.</summary>
    private void BuildWarpGates(MapDef map)
    {
        string asset = AssetLibrary.Has("shared_warp_gate_idle")
            ? "shared_warp_gate_idle" : "shared_spawn_portal";
        var placed = new List<Vector3>();

        void Gate(Vector3 at, Vector3 facing)
        {
            if (placed.Any(p => p.DistanceTo(at) < 4f)) return;
            placed.Add(at);
            MapKit.Prop(this, asset, at, MapKit.YawTowards(facing));
        }

        foreach (var route in map.Routes)
            for (int i = 0; i < route.Waypoints.Count - 1; i++)
            {
                if (!route.IsTeleportLeg(i)) continue;
                // The departure gate faces the way the enemy was walking when
                // it arrived; the arrival gate faces the way it leaves. Both
                // legs exist, because a teleport leg is never first or last.
                Gate(ToGd(route.Waypoints[i]),
                    ToGd(route.Waypoints[i]) - ToGd(route.Waypoints[i - 1]));
                Gate(ToGd(route.Waypoints[i + 1]),
                    ToGd(route.Waypoints[i + 2]) - ToGd(route.Waypoints[i + 1]));
            }

        _laneMouths = _laneMouths.Concat(placed).ToList();
        if (placed.Count > 0) GD.Print($"[map] {placed.Count} warp gate(s) using {asset}");
    }

    /// <summary>One pad in the player's teleport network.
    ///
    /// Three things were wrong with the Spire's version and all three were
    /// invisible: the asset name had no state suffix so nothing ever resolved,
    /// the pad_id went on the StaticBody while the player reads metadata off
    /// the Area, and no code anywhere handled the "teleporter" kind. It has
    /// been a coloured box you walk over since M3.</summary>
    private void AddTeleportPad(string id, string label, Vector3 at)
    {
        var body = AddStaticBox(at + new Vector3(0, 0.1f, 0), new Vector3(3f, 0.2f, 3f),
            new Color(0.35f, 0.7f, 0.85f), layer: 0);
        var area = MakeArea("teleporter", new BoxShape3D { Size = new Vector3(3.2f, 2.5f, 3.2f) });
        area.SetMeta("pad_id", id);
        body.AddChild(area);
        _teleportPads.Add(new TeleportPad(id, label, at, body));
        SetPadArt(id, "idle");
    }

    /// <summary>Where the teleport network's pads are, in the order a picker
    /// should list them.</summary>
    public sealed record TeleportPad(string Id, string Label, Vector3 At, StaticBody3D Body);
    private readonly List<TeleportPad> _teleportPads = new();
    public IReadOnlyList<TeleportPad> TeleportPads => _teleportPads;

    /// <summary>Swaps a pad between its three delivered states, the same way
    /// RefreshSocketArt swaps a socket between empty and occupied.</summary>
    public void SetPadArt(string id, string state)
    {
        var pad = _teleportPads.FirstOrDefault(p => p.Id == id);
        if (pad is null || !IsInstanceValid(pad.Body)) return;
        if (pad.Body.GetNodeOrNull<Node3D>("PadArt") is { } stale)
        {
            pad.Body.RemoveChild(stale);
            stale.QueueFree();
        }
        if (!MapKit.Mount(pad.Body, $"shared_teleporter_pad_{state}", MapKit.GroundLocal(pad.Body))) return;
        if (pad.Body.GetChild(pad.Body.GetChildCount() - 1) is Node3D art) art.Name = "PadArt";
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
            if (route.IsTeleportLeg(i)) continue;
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
            {
                // Nothing walks a warp leg, so nothing has to be kept off it.
                // Treating the chord as lane sterilises a strip right across
                // the map and leaves the fields with no scenery in them.
                if (route.IsTeleportLeg(i)) continue;
                if (DistanceToSegment(Flat(at), Flat(ToGd(route.Waypoints[i])),
                        Flat(ToGd(route.Waypoints[i + 1]))) < clearance) return true;
            }
        }

        // Buildings are registered as they are built; scenery and scatter stay
        // out of them, which Blocked could not know from routes and sockets
        // alone — nothing else on the map has an inside.
        foreach (var footprint in _footprints)
            if (footprint.HasPoint(new Vector2(at.X, at.Z))) return true;
        return false;
    }

    /// <summary>Building footprints in XZ, with their margin already added.</summary>
    private readonly List<Rect2> _footprints = new();

    private static Vector3 Flat(Vector3 v) => new(v.X, 0, v.Z);

    private static float DistanceToSegment(Vector3 point, Vector3 a, Vector3 b)
    {
        var ab = b - a;
        float lengthSq = ab.LengthSquared();
        if (lengthSq < 0.001f) return point.DistanceTo(a);
        float t = Mathf.Clamp((point - a).Dot(ab) / lengthSq, 0f, 1f);
        return point.DistanceTo(a + ab * t);
    }

    /// <summary>Swaps a trap to the state its charges say it is in. Design
    /// delivered armed / triggered / spent for every trap because charges and
    /// rearm are gameplay information — a spent plate that still looks armed is
    /// a lie the player pays for.</summary>
    private void RefreshTrapArt(int trapId, string defId, int chargesLeft, int maxCharges)
    {
        if (!_towerViews.TryGetValue(trapId, out var view)) return;

        string state = chargesLeft <= 0 ? "spent"
            : chargesLeft < maxCharges ? "triggered"
            : "armed";
        string asset = defId switch
        {
            "spike" => $"trap_spike_{state}",
            "tar" => chargesLeft <= 0 ? "trap_tar_depleted" : "trap_tar_full",
            "launcher" => chargesLeft <= 0 ? "trap_launcher_rearming"
                : chargesLeft < maxCharges ? "trap_launcher_fired" : "trap_launcher_charged",
            _ => "",
        };
        if (asset.Length == 0 || !AssetLibrary.Has(asset)) return;
        if ((string)view.GetMeta("trap_state", "") == asset) return;

        if (view.GetNodeOrNull<Node3D>("Body") is { } old)
        {
            view.RemoveChild(old);
            old.QueueFree();
        }
        var body = AssetLibrary.Instantiate(asset, () => Placeholders.Structure(defId));
        body.Name = "Body";
        view.AddChild(body);
        view.SetMeta("trap_state", asset);
    }

    /// <summary>Tears down a sold structure and hands its socket back to the
    /// empty-pad art.</summary>
    private void ReleaseStructureView(int towerId)
    {
        if (!_towerViews.Remove(towerId, out var view)) return;
        _towerRigs.Remove(towerId);
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
        // At the socket, not at world zero. GroundLocal is for map kit authored
        // at true world height — a terrain tile or a deck segment carries its
        // own elevation. A build pad belongs wherever its socket is, so using
        // that rule here dropped every wall socket's pad onto the floor
        // directly under the deck and left the deck itself unmarked.
        var art = MapKit.Mount(body, asset, 0f);
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
        // shares a footprint with the vent tunnel's mouth — and then off
        // z=-12.4, which was a metre *inside* the deck it served: the climb
        // stopped dead against the underside at 3.8 m for a 6 m deck, so this
        // was a ladder to nowhere and the launcher was the only way up. The
        // traversal probe found it; the deck's south face is z=-12, so the
        // rungs belong just clear of it.
        var ladderVisual = AddStaticBox(new Vector3(-11f, 3f, -11.3f), new Vector3(1.2f, 6f, 0.15f), new Color(0.7f, 0.6f, 0.3f), layer: 0);
        ladderVisual.AddChild(MakeArea("ladder", new BoxShape3D { Size = new Vector3(1.6f, 7f, 1.6f) }));
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

    /// <summary>An invisible wall at the edge of the playable field.
    ///
    /// The perimeter has always been decoration — boundary wall props with no
    /// collision — so a player who walked past the slab fell out of the world
    /// and nothing caught them. That was survivable on a yard you could see
    /// the far side of. On a map three hundred metres across, with vehicles
    /// that do twenty metres a second, it is not: the edge has to be a thing
    /// you hit. Hidden rather than drawn, because what the player should see
    /// there is the treeline.</summary>
    private void BuildContainment(float halfX, float halfZ)
    {
        const float thickness = 1f, height = 8f;
        foreach (var (at, size) in new (Vector3, Vector3)[]
        {
            (new Vector3(0, height * 0.5f, -halfZ), new Vector3(halfX * 2f + thickness, height, thickness)),
            (new Vector3(0, height * 0.5f, halfZ), new Vector3(halfX * 2f + thickness, height, thickness)),
            (new Vector3(-halfX, height * 0.5f, 0), new Vector3(thickness, height, halfZ * 2f + thickness)),
            (new Vector3(halfX, height * 0.5f, 0), new Vector3(thickness, height, halfZ * 2f + thickness)),
        })
        {
            MapKit.HideBox(AddStaticBox(at, size, new Color(0.3f, 0.3f, 0.3f), layer: 1));
        }
    }

    /// <summary>Ground clutter on a fixed lattice — deterministic placement so
    /// two clients render the same world without syncing anything.
    ///
    /// Kept to the outfield and refused anywhere near the lane, a socket or a
    /// gate. Scattering into the playable middle is what made the first pass
    /// look like litter rather than a working yard.</summary>
    private void ScatterTerrain(string asset, float halfX, float halfZ, int attempts = 22)
    {
        if (!AssetLibrary.Has(asset)) return;
        int placed = 0, refused = 0;

        // Twenty-two was the number for a 110 x 80 yard. A map six times the
        // area with the same count is not sparse, it is empty, so the caller
        // says how many — and the lattice below spreads whatever it is given
        // across whatever field it is given.
        for (int i = 0; i < attempts; i++)
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
    /// <summary>The Spire: five floor plates, the stair well that threads them,
    /// the fire escape up the east face, and the roof the core sits on.
    ///
    /// Built as real collision because the whole map is a traversal problem —
    /// on Foundry a player who falls lands on the yard, here they land five
    /// storeys down and have to climb again, and that cost is the point.
    ///
    /// This is also where M3's traversal set debuts: a cargo lift that actually
    /// serves the height, a pair of teleport pads for the rotation the lift is
    /// too slow for, and two sniper nests that only pay if you commit to
    /// reaching them.</summary>
    private void BuildSpireStructures()
    {
        // Floor plates. The atrium is left open so the stair reads as a well
        // rather than as five separate rooms.
        for (int floor = 1; floor <= 4; floor++)
        {
            float y = floor * 10f;
            foreach (var (cx, cz, sx, sz) in new[]
            {
                (-13f, 0f, 12f, 40f),      // west wing
                (13f, 0f, 12f, 40f),       // east wing
                (0f, -15f, 14f, 10f),      // north bridge
                (0f, 15f, 14f, 10f),       // south bridge
            })
            {
                var plate = AddStaticBox(new Vector3(cx, y - 0.2f, cz),
                    new Vector3(sx, 0.4f, sz), new Color(0.42f, 0.45f, 0.52f), layer: 1);
                MapKit.MountRun(plate, "spire_floor", Mathf.Max(sx, sz), 4f,
                    alongX: sx >= sz, MapKit.GroundLocal(plate));
            }
        }

        // Exterior shell: four faces with the east one cut away for the fire
        // escape, so the outside route is visible from inside the building.
        foreach (var (cx, cz, sx, sz) in new[]
        {
            (0f, -20f, 40f, 1f),
            (-20f, 0f, 1f, 40f),
            (0f, 20f, 40f, 1f),
        })
        {
            var wall = AddStaticBox(new Vector3(cx, 20f, cz),
                new Vector3(sx, 40f, sz), new Color(0.34f, 0.36f, 0.42f), layer: 1);
            MapKit.MountRun(wall, "spire_facade", Mathf.Max(sx, sz), 8f,
                alongX: sx >= sz, MapKit.GroundLocal(wall));
            MapKit.NoShadow(wall);
        }

        // Fire escape: the zigzag the escape route walks, as real landings.
        foreach (float y in new[] { 10f, 20f, 30f })
        {
            var landing = AddStaticBox(new Vector3(20f, y - 0.2f, 10f - (y - 10f) * 0.8f),
                new Vector3(6f, 0.4f, 8f), new Color(0.5f, 0.42f, 0.3f), layer: 1);
            MapKit.MountRun(landing, "spire_fireescape", 8f, 3f, alongX: false,
                MapKit.GroundLocal(landing));
        }

        // Roof and the core it carries.
        var roof = AddStaticBox(new Vector3(0f, 39.8f, 0f), new Vector3(40f, 0.4f, 40f),
            new Color(0.4f, 0.43f, 0.5f), layer: 1);
        MapKit.MountRun(roof, "spire_roof", 40f, 8f, alongX: true, MapKit.GroundLocal(roof));

        // --- M3 traversal debut ------------------------------------------

        // Cargo lift: the honest way to the top and slow enough that taking it
        // is a decision. Serves the full height of the shaft.
        var liftShaft = AddStaticBox(new Vector3(-17f, 20f, -17f), new Vector3(5f, 40f, 5f),
            new Color(0.3f, 0.32f, 0.38f), layer: 0);
        MapKit.MountRun(liftShaft, "shared_elevator_shaft", 40f, 5f, alongX: false,
            MapKit.GroundLocal(liftShaft));
        MapKit.NoShadow(liftShaft);
        var lift = AddStaticBox(new Vector3(-17f, 0.6f, -17f), new Vector3(4f, 0.4f, 4f),
            new Color(0.62f, 0.55f, 0.28f), layer: 1);
        lift.AddChild(MakeArea("elevator", new BoxShape3D { Size = new Vector3(4.4f, 3f, 4.4f) }));
        MapKit.Mount(lift, "shared_elevator", MapKit.GroundLocal(lift));

        // Teleport pads: lobby to roof and back, for the rotation the lift is
        // too slow to serve. Paired, so using one is committing to the other end.
        AddTeleportPad("padGround", "LOBBY", new Vector3(-24f, 0.2f, -6f));
        AddTeleportPad("padRoof", "ROOF", new Vector3(-6f, 40.2f, 12f));

        // Sniper nests: reachable only by committing to the climb, and they see
        // the stair well the ground floor cannot. The plan's rule that every map
        // has a vantage towers cannot cover and a traversing hero can.
        foreach (var pos in new[] { new Vector3(17f, 30.2f, -17f), new Vector3(-17f, 30.2f, 17f) })
        {
            var nest = AddStaticBox(pos, new Vector3(5f, 0.4f, 5f),
                new Color(0.45f, 0.4f, 0.3f), layer: 1);
            nest.AddChild(MakeArea("nest", new BoxShape3D { Size = new Vector3(5f, 3f, 5f) }));
            MapKit.Mount(nest, "shared_snipernest", MapKit.GroundLocal(nest));
        }

        // Ladders between floors on the west side, so the stair is not the only
        // way up on foot and a downed player has a route back.
        //
        // The west wing runs to x=-19, and these stood at exactly x=-19: every
        // one of the four climbed into the underside of the floor it served and
        // stopped two metres short, on a map whose entire shape is climbing.
        // The rungs belong just clear of the edge — same fix as Foundry's deck
        // ladder and both of Switchyard's, all found by the traversal probe.
        for (int floor = 0; floor < 4; floor++)
        {
            var ladder = AddStaticBox(new Vector3(-20f, floor * 10f + 5f, 6f),
                new Vector3(1.2f, 10f, 0.15f), new Color(0.7f, 0.6f, 0.3f), layer: 0);
            ladder.AddChild(MakeArea("ladder", new BoxShape3D { Size = new Vector3(1.6f, 11f, 1.6f) }));
            MapKit.Mount(ladder, "shared_ladder", MapKit.GroundLocal(ladder));
        }

        // One zipline down, because a map that is only climbable is a map you
        // spend the intermission walking.
        var anchor = AddStaticBox(new Vector3(6f, 40.5f, -14f), new Vector3(1f, 1.4f, 1f),
            new Color(0.55f, 0.5f, 0.35f), layer: 0);
        var spireZip = MakeArea("zipline", new BoxShape3D { Size = new Vector3(2.4f, 2.6f, 2.4f) });
        // "zip_to" on the body, where nothing reads it. The player reads
        // "zip_end" off the Area, so the one zipline down from a forty-metre
        // roof has never carried anyone since the day it was written.
        spireZip.SetMeta("zip_end", new Vector3(-26f, 1f, -6f));
        anchor.AddChild(spireZip);
        MapKit.Mount(anchor, "shared_zipline_anchor", MapKit.GroundLocal(anchor));
    }

    private void BuildSwitchyardStructures()
    {
        // Mid deck (y=5), moved north from z=-18 to sit over the long route's
        // southern leg and within a Nova's reach of the freight cut. Where it
        // was, it saw only what the catwalk saw and none of the air, so it was
        // a climb with nothing at the top of it.
        var midDeck = AddStaticBox(new Vector3(-7, 4.8f, -12), new Vector3(26, 0.4f, 8), new Color(0.45f, 0.48f, 0.55f), layer: 1);
        MapKit.MountRun(midDeck, "switchyard_middeck", 26f, 4f, alongX: true, MapKit.GroundLocal(midDeck));
        // Columns under the deck's two long edges, not down its centre line.
        // The centre line is z −12, and the long route's southern leg runs
        // along z −12 from x −5 to 15 — so three of these stood in the lane
        // enemies walk. They are solid, so a player is stopped by them; the
        // enemies, having no pathfinding, simply walked through. Edges are
        // where a deck wants its columns anyway.
        foreach (float columnX in new[] { -17f, -8f, 1f })
            foreach (float columnZ in new[] { -15f, -9f })
            {
                var midColumn = AddStaticBox(new Vector3(columnX, 2.4f, columnZ), new Vector3(1.2f, 4.8f, 1.2f), new Color(0.4f, 0.42f, 0.48f), layer: 1);
                MapKit.Mount(midColumn, "switchyard_column", MapKit.GroundLocal(midColumn));
            }

        // Upper catwalk (y=10) carrying w3/w4 over the air lane. Extended west
        // to x=-16 so its climb has somewhere to land clear of the lane.
        // Depth 8 rather than 5: w3 (z=6) and w4 (z=0) were both half a metre
        // off the edge, so a tower built on either hung in space.
        // Ten metres deep rather than eight. The catwalk exists to cover the
        // air strand and carried its sockets in the middle twenty metres of
        // its thirty, so both ends of the strand were out of reach; the pads
        // that fix that need a clear metre of deck on every side, and at eight
        // deep the north row had nowhere to stand that was not the edge.
        var catwalk = AddStaticBox(new Vector3(-1, 9.8f, 3), new Vector3(30, 0.4f, 10), new Color(0.5f, 0.52f, 0.6f), layer: 1);
        MapKit.MountRun(catwalk, "switchyard_catwalk", 30f, 4f, alongX: true, MapKit.GroundLocal(catwalk));
        // Two of these stood in a lane as well: the one at x −6 in the long
        // route's north-south leg at x −5, and the one at x 12 in the freight
        // cut's diagonal. Moved to where neither route passes — which is not a
        // regular spacing, because the routes are not regularly spaced either.
        foreach (var (columnX, columnZ) in new[] { (-14f, 3f), (-2f, 3f), (12f, 6f) })
        {
            var column = AddStaticBox(new Vector3(columnX, 4.9f, columnZ), new Vector3(1.2f, 9.8f, 1.2f), new Color(0.4f, 0.42f, 0.48f), layer: 1);
            MapKit.Mount(column, "switchyard_column", MapKit.GroundLocal(column));
        }

        // Ladders: yard → mid deck, and ground → catwalk.
        //
        // Both of these used to stand *under* the deck they served. A climb
        // there tops out against the slab's underside — the probe measured 2.8
        // and 7.8 metres for decks at 5 and 10 — so the mid deck was awkward
        // and the catwalk was unreachable, which took the whole air lane with
        // it because those four sockets are the only ones that cover it. The
        // fix is the same both times: stand the rungs off the edge so the
        // climb is beside the deck rather than beneath it, and run them a
        // little past the surface so there is something to step off onto.
        // `--shot switchyard <txt> traversal` is what keeps them honest.
        //
        // Mid deck spans z −16…−8, so the yard side of it is z < −16.
        var ladder1 = AddStaticBox(new Vector3(-14f, 3f, -16.8f), new Vector3(1.2f, 6f, 0.15f), new Color(0.7f, 0.6f, 0.3f), layer: 0);
        ladder1.AddChild(MakeArea("ladder", new BoxShape3D { Size = new Vector3(1.6f, 7f, 1.6f) }));
        MapKit.Mount(ladder1, "shared_ladder", MapKit.GroundLocal(ladder1));
        MapKit.Prop(ladder1, "shared_ladder", new Vector3(0, MapKit.GroundLocal(ladder1) + 3.2f, 0));

        // Catwalk spans x −16…14, so its west face is x < −16.
        var ladder2 = AddStaticBox(new Vector3(-16.9f, 5.5f, 3f), new Vector3(0.15f, 11f, 1.2f), new Color(0.7f, 0.6f, 0.3f), layer: 0);
        ladder2.AddChild(MakeArea("ladder", new BoxShape3D { Size = new Vector3(1.6f, 12f, 1.6f) }));
        MapKit.Mount(ladder2, "shared_ladder", MapKit.GroundLocal(ladder2), yawDegrees: 90f);
        MapKit.Prop(ladder2, "shared_ladder", new Vector3(0, MapKit.GroundLocal(ladder2) + 6.5f, 0), 90f);

        // Launcher pad: spawn yard straight onto the mid deck.
        var launcher = AddStaticBox(new Vector3(8, 0.15f, -22), new Vector3(2.2f, 0.3f, 2.2f), new Color(0.9f, 0.5f, 0.9f), layer: 1);
        var launchArea = MakeArea("launcher", new BoxShape3D { Size = new Vector3(2.2f, 1.2f, 2.2f) });
        // Re-aimed with the deck: an 11 m/s hop is back at deck height after
        // 1.6 s, so this is the ten metres west and ten north to land on it.
        launchArea.SetMeta("launch_velocity", new Vector3(-9.3f, 11f, 6.2f));
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

        BuildFreightCut();
        BuildSwitchyardRailway();
    }

    // =====================================================================
    // Switchyard: the yard itself
    // =====================================================================

    /// <summary>Track centres. Four roads at 4 m, which is about what a
    /// classification yard uses (real ones sit near 3 m; the lane module is
    /// 3.4 m wide, so 4 m is the tightest that does not overlap).</summary>
    // The yard's plan, in one place. The playfield — every ground socket,
    // both ground routes and the spawn — lives between z −18 and z +16, so the
    // running line and its roads sit north of z 19 and the depot road sits
    // south of z −28, and nothing a player stands on has a rail through it.
    private const float YardMainZ = 20.5f, YardRoadA = 24.5f, YardRoadB = 28f, YardRoadC = 31.5f;
    // The ladder's angle is the angle of every turnout on it, and a yard
    // turnout is a 1:6 to 1:8 — sharper than a running line's 1:8 to 1:12
    // because nothing moves fast in a yard, but not sharper than 1:6. The lead
    // ran -38 → 16 for the same 11 m of offset, which is 1:4.9: sharper than
    // any turnout that has ever been laid, and the reason the throat read as
    // tracks merging rather than as points. Stretched to 66 m it is 1:6.0, and
    // the probe holds it there.
    private const float LeadFromX = -42f, LeadToX = 24f, BufferX = 37f;
    private const float DepotZ = -32f;
    private const float NorthWallZ = 35f, SouthWallZ = -37f;

    /// <summary>Where the freight cut ended up, for the railway probe.</summary>
    private (Vector3 From, Vector3 To)? _cutSpan;

    /// <summary>Every run of track laid, so the railway probe can measure the
    /// yard rather than anyone squinting at a screenshot of it.</summary>
    private readonly List<(Vector3 From, Vector3 To)> _railRuns = new();

    /// <summary>The yard, laid out the way a yard is actually laid out.
    ///
    /// What was here was a single track following the enemy routes — which
    /// turn square corners — plus railcars, containers, buffer stops and
    /// signals dropped at whatever angle, none of them on a track. Track does
    /// not turn ninety degrees and rolling stock does not stand across the
    /// sleepers, so the whole thing read as scenery that had never seen a
    /// railway.
    ///
    /// The arrangement here is a simple ladder, the standard one: a main line
    /// with a diagonal lead off it, and each siding peeling from the lead in
    /// turn. Every turnout branches the same way, which is the prototype rule —
    /// it lets one worker line the whole ladder from one side without crossing
    /// a track. The lead climbs at 1 in 5. A real turnout is 1 in 8 to 1 in 12
    /// and this is steeper, because a prototype ladder for four roads wants
    /// most of a hundred metres and the map is ninety-four wide; 1 in 5 is the
    /// shallowest that fits and still reads as a fan rather than a corner.
    ///
    /// Everything else follows from the track: cars stand on roads in rakes,
    /// buffer stops close the dead ends and face the way a car would arrive,
    /// signals stand at the throat where the turnouts are, and the overbridges
    /// cross the roads square rather than lying alongside them.</summary>
    private void BuildSwitchyardRailway()
    {
        // --- The running line, straight through the yard along its north
        // side, and the depot road that serves the loading dock the players
        // spawn on.
        //
        // Both used to sit further in. The running line was at z 16, which is
        // where g10, g11, g21 and g19 are: four tower pads standing in the
        // four-foot of a live main line. The depot road was at z −22, which is
        // the hero station, four metres from the spawn point. Track is laid
        // clear of anything a player stands on now, and the railway probe
        // (`--shot switchyard <txt> railway`) is what keeps it that way.
        RailRun(new Vector3(-46, 0, YardMainZ), new Vector3(44, 0, YardMainZ));
        RailRun(new Vector3(-44, 0, DepotZ), new Vector3(24, 0, DepotZ));
        MapKit.Prop(this, "switchyard_dress_buffer", new Vector3(25.5f, 0, DepotZ), 90f);

        // --- The throat. One lead leaves the running line and climbs across
        // the yard; each road leaves the lead where the lead reaches that
        // road's centre. That is what makes this a ladder rather than a fan:
        // every turnout branches the same way, off the same lead, in order —
        // which is the arrangement a real classification yard is built to.
        var leadFrom = new Vector3(LeadFromX, 0, YardMainZ);
        var leadTo = new Vector3(LeadToX, 0, YardRoadC);
        RailRun(leadFrom, leadTo);

        foreach (float z in new[] { YardRoadA, YardRoadB, YardRoadC })
        {
            float x = TurnoutX(z);
            RailRun(new Vector3(x, 0, z), new Vector3(BufferX - 1.5f, 0, z));
            // Facing the way a car arrives: these roads are entered from the
            // west, so the stop looks west.
            MapKit.Prop(this, "switchyard_dress_buffer", new Vector3(BufferX, 0, z), 90f);
        }

        // --- Rolling stock, standing on the roads in rakes, east of the
        // turnout that serves them. A 12 m car every 12.6 m reads as coupled
        // without the buffers interpenetrating.
        foreach (var (z, firstX, count) in new[] { (YardRoadA, 6f, 3), (YardRoadB, 14f, 2), (YardRoadC, 26f, 1) })
            for (int i = 0; i < count; i++)
                MapKit.Prop(this, "switchyard_dress_railcar", new Vector3(firstX + i * 12.6f, 0, z));

        // --- Container terminal, in the yard's north-west corner: west of
        // every turnout and north of the lead, so it stands beside the railway
        // rather than on it. Two high, which is what the model is drawn for.
        for (int row = 0; row < 3; row++)
        {
            float x = -39f + row * 6.4f;
            foreach (float z in new[] { YardRoadB, YardRoadC })
            {
                MapKit.Prop(this, "switchyard_dress_container", new Vector3(x, 0, z));
                if (z == YardRoadB) MapKit.Prop(this, "switchyard_dress_container", new Vector3(x, 2.6f, z));
            }
        }

        // --- Signals stand where they mean something: at the throat, at each
        // turnout, and at the yard's east exit. On the running line's south
        // side, never on the road they govern.
        // The throat, a turnout partway up the ladder, and the yard's east
        // exit. Only three: the mast is eight metres tall and this band of the
        // map is fifteen deep, so one per turnout would be a forest. Design
        // has no switch stand delivered — see the design-system notes — so a
        // signal standing at the points is what says "the track divides here".
        foreach (float x in new[] { LeadFromX, TurnoutX(YardRoadB), 40f })
            MapKit.Prop(this, "switchyard_dress_signaltower", new Vector3(x, 0, YardMainZ - 2.5f));
        MapKit.Prop(this, "switchyard_dress_signaltower", new Vector3(-40, 0, DepotZ - 2.5f));

        // --- Two overbridges across the yard. They cross the roads square, on
        // piers set outside the outermost track — the thing that was missing
        // was any structure that reads as spanning the railway rather than
        // running along beside it. Decoration: no collision, well clear of the
        // decks the player actually fights on.
        SwitchyardOverbridge(-30f);
        SwitchyardOverbridge(30f);

        // --- Portals where the running line leaves the map. A railway that
        // stops at the edge of the world is the other half of why this read as
        // set dressing; now it comes from somewhere and goes somewhere.
        SwitchyardPortal(new Vector3(-45, 0, YardMainZ));
        SwitchyardPortal(new Vector3(43, 0, YardMainZ));
        SwitchyardPortal(new Vector3(-45, 0, DepotZ));

        // --- Retaining walls hold the yard's boundary, which is what design
        // drew them for ("perimeter + tier faces"). They were briefly run down
        // both sides of the freight cut instead: six metres tall, ten metres
        // apart, straight through the middle of the map — a canyon across the
        // playfield that hid the yard behind it and that enemies walked
        // through, because dressing has no collision. The cut is at grade and
        // reads through its own channel module; it does not want walls.
        for (float x = -45f; x <= 45f; x += 10f)
        {
            MapKit.Prop(this, "switchyard_retainingwall", new Vector3(x, 0, NorthWallZ), 180f);
            MapKit.Prop(this, "switchyard_retainingwall", new Vector3(x, 0, SouthWallZ), 0f);
        }

        ScatterTerrain("switchyard_terrain_scatter", 47f, 32f);
    }

    /// <summary>Where the climbing lead crosses a road's centre — the
    /// turnout that serves it.</summary>
    private static float TurnoutX(float roadZ) =>
        LeadFromX + (LeadToX - LeadFromX) * (roadZ - YardMainZ) / (YardRoadC - YardMainZ);

    /// <summary>The ladder's ratio, quoted the way a railway quotes it: one
    /// across for N along.</summary>
    private static float LadderRatio() =>
        (LeadToX - LeadFromX) / (YardRoadC - YardMainZ);

    /// <summary>The freight cut: the one piece of railway inside the
    /// playfield, and the map's whole lesson — the fast route that b1 closes.
    ///
    /// Design draws it as a 4 m run of track at grade between 1.2 m concrete
    /// kerb walls, and says in as many words: place it only where no other
    /// route or socket crosses. It was laid along the entire shortcut route
    /// with <c>YawTowards</c>, which aligns a piece's +Z — so every module sat
    /// a quarter-turn wrong, its 4 m of track running across the lane and its
    /// kerb walls straddling it, spaced by the module's width rather than its
    /// length. Nine of those end to end is not a cutting; it is a row of
    /// concrete gates over the shortcut, and it is most of what made this map
    /// unreadable.
    ///
    /// So: walk the route, keep the longest stretch that is genuinely clear of
    /// every socket and of the other ground route, and lay the module along
    /// its own length down that. One cutting, in one place, that a player can
    /// learn.</summary>
    private void BuildFreightCut()
    {
        var shortcut = System.Array.Find(_map.Routes.ToArray(), r => r.Id == "groundShort");
        if (shortcut is null || !AssetLibrary.Has("switchyard_cut_channel")) return;

        // Sample the route every 4 m — the module's run length, so the pieces
        // meet end to end instead of leaving 5.6 m gaps between them.
        const float Step = 4f;
        var samples = new List<(Vector3 At, Vector3 Along, bool Clear)>();
        for (int i = 0; i < shortcut.Waypoints.Count - 1; i++)
        {
            var a = ToGd(shortcut.Waypoints[i]);
            var b = ToGd(shortcut.Waypoints[i + 1]);
            var along = (b - a).Normalized();
            int count = Mathf.Max(1, Mathf.FloorToInt((b - a).Length() / Step));
            for (int k = 0; k < count; k++)
                samples.Add((a + along * ((k + 0.5f) * Step), along, false));
        }

        // The kerb walls stand 3.4 m either side of the centre line, so a tower
        // pad needs about four metres to be outside the channel rather than
        // in it, and the other ground route needs a little more than that to
        // pass without being walled in. Barricade and trap sockets are exempt:
        // b1 is the gate at the cut's mouth and the path plates are laid in
        // the lane on purpose — both belong inside the channel, not clear of
        // it.
        const float SocketClearance = 4.2f, RouteClearance = 4.5f;
        for (int i = 0; i < samples.Count; i++)
        {
            var at = samples[i].At;
            bool clear = true;
            foreach (var socket in _map.Sockets)
            {
                if (socket.Tag is SocketTag.Barricade or SocketTag.Trap) continue;
                var pos = ToGd(socket.Pos);
                if (pos.Y > 1f) continue;                    // deck sockets pass overhead
                if (new Vector2(pos.X - at.X, pos.Z - at.Z).Length() < SocketClearance) { clear = false; break; }
            }
            if (clear) clear = !CrossesOtherGroundRoute(at, RouteClearance, shortcut.Id);
            samples[i] = (samples[i].At, samples[i].Along, clear);
        }

        // The longest clear stretch, and only that one: two disconnected stubs
        // of cutting say less than one continuous run of it.
        int bestStart = -1, bestLength = 0, runStart = -1;
        for (int i = 0; i <= samples.Count; i++)
        {
            bool clear = i < samples.Count && samples[i].Clear;
            if (clear) { if (runStart < 0) runStart = i; continue; }
            if (runStart >= 0 && i - runStart > bestLength) { bestLength = i - runStart; bestStart = runStart; }
            runStart = -1;
        }

        // Three modules is twelve metres, about the shortest run that reads as
        // a cutting rather than as a piece of wall someone dropped.
        if (bestStart < 0 || bestLength < 3)
        {
            GD.PrintErr("ERROR: no stretch of the shortcut is clear enough for the freight cut");
            return;
        }

        for (int i = bestStart; i < bestStart + bestLength; i++)
        {
            var (at, along, _) = samples[i];
            if (MapKit.Prop(this, "switchyard_cut_channel", new Vector3(at.X, 0, at.Z),
                    MapKit.YawAlongX(along)) is { } piece)
                MapKit.ThinOut(piece, i - bestStart, "cut_lamp_post", "cut_lamp");
        }
        _cutSpan = (samples[bestStart].At, samples[bestStart + bestLength - 1].At);
    }

    // =====================================================================
    // Map validation — MAP-AUTHORING.md §4
    // =====================================================================

    /// <summary>Samples every ground and air route at 4 m, which is the lane
    /// module's own repeat and fine enough that a hole in coverage cannot hide
    /// between two samples.
    ///
    /// The sampler itself is the sim's — RouteDef.Samples — so the validator,
    /// the harness and the map file agree on what a sample is. It skips
    /// teleport legs, which nothing walks, and it carries the distance since
    /// the route was entered, which resets at every warp pad. That number is
    /// what the apron is measured against: a straight-line distance from the
    /// spawn gate says nothing on a route that leaves the gate and comes back
    /// past it two hundred metres later.</summary>
    private List<(RouteDef Route, Vector3 At, float Since)> RouteSamples()
    {
        var samples = new List<(RouteDef, Vector3, float)>();
        foreach (var route in _map.Routes)
            foreach (var sample in route.Samples(4f))
                samples.Add((route, ToGd(sample.At), sample.MetersSinceEntry));
        return samples;
    }

    /// <summary>How far into a route the spawn apron reaches.
    ///
    /// The coverage rules are about the lane, not the gate. An enemy standing
    /// on its spawn point has the entire walk still ahead of it and nothing is
    /// lost by not shooting it there — while demanding three pads within reach
    /// of the map edge would force every map to grow a cluster of build pads
    /// at the mouth, which is worse design than the hole it closes. It pays
    /// players to turtle on the entrance.
    ///
    /// Eight metres, which is two lane modules: enough to cover the spawn
    /// point itself and nothing more. Holes further in are holes.</summary>
    private const float SpawnApron = 8f;

    /// <summary>Whether a tower built on this socket could shoot something at
    /// <paramref name="target"/>.
    ///
    /// Level 1, because a lane that is only covered once someone has paid for
    /// upgrades is a lane that is not covered on the wave it first matters.
    /// Damage only: Detector and Singularity reach flyers but cannot kill one,
    /// and a lane defended entirely by towers that do no damage is not
    /// defended. Range is 3D — the whole point of an air lane is that it is
    /// above you.</summary>
    private static bool CanCover(Vector3 socket, Vector3 target, EnemyLayer layer)
    {
        float distance = socket.DistanceTo(target);
        foreach (var def in Towers.All.Values)
        {
            if (def.Kind == TowerKind.Barricade || def.Damage <= 0f) continue;
            if (!def.TargetLayers.Contains(layer)) continue;
            if (distance <= def.RangeMeters && distance >= def.MinRangeMeters) return true;
        }
        return false;
    }

    private List<(SocketDef Def, Vector3 At)> BuildableSockets()
    {
        var list = new List<(SocketDef, Vector3)>();
        foreach (var socket in _map.Sockets)
            if (socket.Tag is SocketTag.Ground or SocketTag.Wall)
                list.Add((socket, ToGd(socket.Pos)));
        return list;
    }

    /// <summary>§4.1 — every tier that carries a socket can be got to.
    ///
    /// A tier with no way up is not a hard map, it is a dead one: on
    /// Switchyard the catwalk carried the only four sockets that reach the air
    /// strand, so one unreachable tier took a whole enemy layer with it.</summary>
    private void RuleTiersAreReachable(List<(SocketDef Def, Vector3 At)> sockets,
        List<string> report, List<string> failures)
    {
        var tops = new List<Vector3>();
        void Walk(Node n)
        {
            if (n is Area3D area)
            {
                string kind = (string)area.GetMeta("kind", "");
                // Where this piece of traversal leaves you.
                if (kind == "ladder")
                    tops.Add(area.GlobalPosition + new Vector3(0, LadderHalfHeight(area), 0));
                else if (kind == "zipline" && area.HasMeta("zip_end"))
                    tops.Add((Vector3)area.GetMeta("zip_end"));
                else if (kind is "launcher" or "teleporter" or "elevator")
                    tops.Add(area.GlobalPosition);
            }
            foreach (var child in n.GetChildren()) Walk(child);
        }
        Walk(this);

        int unreachable = 0;
        foreach (var (def, at) in BuildableSockets())
        {
            if (at.Y < 1.5f) continue;                    // ground tier needs no climb
            bool served = tops.Any(t => Mathf.Abs(t.Y - at.Y) < 3f
                && (t with { Y = 0 }).DistanceTo(at with { Y = 0 }) < 30f);
            if (served) continue;
            unreachable++;
            failures.Add($"§4.1 socket {def.Id} at y {at.Y:0.0} has no traversal within reach");
        }
        report.Add($"§4.1 tiers reachable — {tops.Count} traversal exit(s), {unreachable} orphaned socket(s)");
    }

    /// <summary>§4.4 and §4.5 — every stretch of every lane is within reach of
    /// at least three build pads, so there is a choice about how to answer it
    /// rather than one forced tower.</summary>
    private void RuleRoutesAreCovered(List<(RouteDef Route, Vector3 At, float Since)> samples,
        List<(SocketDef Def, Vector3 At)> sockets, List<string> report, List<string> failures)
    {
        const int Want = 3;
        foreach (var route in _map.Routes)
        {
            var layer = route.Layer;
            int worst = int.MaxValue;
            Vector3 worstAt = Vector3.Zero;
            int thin = 0, count = 0;
            // Where the thin stretches actually are, as spans rather than a
            // tally. A count says a lane is under-covered; a span says which
            // twenty metres of it, which is the difference between knowing
            // there is a problem and being able to place a socket.
            var spans = new List<(Vector3 From, Vector3 To, int Worst)>();
            (Vector3 From, Vector3 To, int Worst)? open = null;
            foreach (var (r, at, since) in samples)
            {
                if (r.Id != route.Id) continue;
                if (since < SpawnApron) continue;
                count++;
                int cover = sockets.Count(s => CanCover(s.At, at, layer));
                if (cover < worst) { worst = cover; worstAt = at; }
                if (cover < Want)
                {
                    thin++;
                    open = open is { } o ? (o.From, at, Mathf.Min(o.Worst, cover)) : (at, at, cover);
                }
                else if (open is { } done) { spans.Add(done); open = null; }
            }
            if (open is { } last) spans.Add(last);
            if (count == 0) continue;

            string rule = layer == EnemyLayer.Air ? "§4.5" : "§4.4";
            report.Add($"{rule} route {route.Id} ({layer}) — thinnest point has {worst} pad(s), "
                + $"{thin}/{count} sample(s) under {Want}");
            foreach (var span in spans)
                failures.Add($"{rule} route {route.Id} under-covered from "
                    + $"({span.From.X:0},{span.From.Y:0},{span.From.Z:0}) to "
                    + $"({span.To.X:0},{span.To.Y:0},{span.To.Z:0}) "
                    + $"— {span.Worst} pad(s) at its thinnest");
        }
    }

    /// <summary>Which towers actually answer this map's flyers, and from
    /// where.
    ///
    /// Reporting only, but it is the number that matters most and the one the
    /// def table hides. Skywatch, Arc and Filament all list
    /// <c>EnemyLayer.Air</c>, so reading the table says three towers answer
    /// flyers. A tower on a ground pad stands at y 0 and spends its whole
    /// range budget climbing, and the strand cruises at 13 to 15 metres — over
    /// Arc's 11 and Filament's 12 — so from the yard those two never fire a
    /// shot at one. They are deck weapons against air. Nothing anywhere says
    /// so, which is why "which towers combat flyers" has one answer on paper
    /// and a different one in a match.
    ///
    /// §4.5 stays a pass/fail on total coverage; this is the breakdown that
    /// says whether a player who never climbs has an answer at all.</summary>
    private void ReportAirAnswers(List<(RouteDef Route, Vector3 At, float Since)> samples,
        List<(SocketDef Def, Vector3 At)> sockets, List<string> report)
    {
        var air = samples.Where(s => s.Route.Layer == EnemyLayer.Air).ToList();
        if (air.Count == 0) return;

        float peak = air.Max(s => s.At.Y);
        report.Add($"air answers — strand peaks at {peak:0.0} m");

        foreach (var def in Towers.All.Values)
        {
            if (def.Kind == TowerKind.Barricade || def.Damage <= 0f) continue;
            if (!def.TargetLayers.Contains(EnemyLayer.Air)) continue;

            int fromGround = 0, fromDeck = 0;
            foreach (var (socket, at) in sockets)
            {
                bool reaches = air.Any(s => at.DistanceTo(s.At) <= def.RangeMeters
                    && at.DistanceTo(s.At) >= def.MinRangeMeters);
                if (!reaches) continue;
                if (socket.Tag == SocketTag.Ground) fromGround++; else fromDeck++;
            }
            int covered = air.Count(s => sockets.Any(p =>
                p.At.DistanceTo(s.At) <= def.RangeMeters && p.At.DistanceTo(s.At) >= def.MinRangeMeters));

            report.Add($"  {def.Id} r{def.RangeMeters:0} — {fromGround} ground pad(s), "
                + $"{fromDeck} deck pad(s), covers {covered}/{air.Count} of the strand"
                + (def.RangeMeters < peak ? "  [cannot reach the peak from the ground at all]" : ""));
        }
    }

    /// <summary>§4.6 — a pad that reaches no lane at any point is a pad nobody
    /// will ever build on, and it is usually a sign the lane moved and the pad
    /// did not.</summary>
    private void RuleNoSocketCoversNothing(List<(RouteDef Route, Vector3 At, float Since)> samples,
        List<(SocketDef Def, Vector3 At)> sockets, List<string> report, List<string> failures)
    {
        int dead = 0;
        foreach (var (def, at) in BuildableSockets())
        {
            if (samples.Any(x => CanCover(at, x.At, x.Route.Layer))) continue;
            dead++;
            failures.Add($"§4.6 socket {def.Id} at ({at.X:0},{at.Y:0},{at.Z:0}) reaches no lane");
        }
        report.Add($"§4.6 dead pads — {dead}");
    }

    /// <summary>§4.7 — closing a shortcut must not hand the player a long way
    /// round that nothing covers. The gate is the map's central decision; it is
    /// only a decision if both answers are playable.</summary>
    private void RuleFallbackIsCovered(List<(RouteDef Route, Vector3 At, float Since)> samples,
        List<(SocketDef Def, Vector3 At)> sockets, List<string> report, List<string> failures)
    {
        var gated = _map.Routes.Where(r => r.BarricadeGate is not null).ToList();
        if (gated.Count == 0) { report.Add("§4.7 no gated shortcut on this map"); return; }

        foreach (var route in gated)
        {
            var fallback = _map.Routes.FirstOrDefault(r => r.Id == route.FallbackRouteId);
            if (fallback is null)
            {
                failures.Add($"§4.7 route {route.Id} is gated by {route.BarricadeGate} "
                    + "but names no fallback");
                continue;
            }
            int thin = 0, count = 0;
            foreach (var (r, at, since) in samples)
            {
                if (r.Id != fallback.Id) continue;
                if (since < SpawnApron) continue;
                count++;
                if (sockets.Count(s => CanCover(s.At, at, fallback.Layer)) < 3) thin++;
            }
            report.Add($"§4.7 {route.Id} falls back to {fallback.Id} — "
                + $"{thin}/{count} sample(s) under-covered");
            if (thin > 0)
                failures.Add($"§4.7 closing {route.BarricadeGate} sends enemies down {fallback.Id}, "
                    + $"which is under-covered at {thin} of {count} points");
        }
    }

    /// <summary>§4.9 — a deck socket has to be standing on its deck, a metre
    /// clear of the edge. w3 and w4 were both half a metre off the edge of the
    /// catwalk, so a tower built on either hung in space.</summary>
    private void RuleWallSocketsAreFooted(List<(SocketDef Def, Vector3 At)> sockets,
        List<string> report, List<string> failures)
    {
        var space = GetWorld3D().DirectSpaceState;
        int floating = 0, checkedCount = 0;
        foreach (var (def, at) in BuildableSockets())
        {
            if (def.Tag != SocketTag.Wall) continue;
            checkedCount++;
            foreach (var offset in new[]
            {
                Vector3.Zero, new Vector3(1f, 0, 0), new Vector3(-1f, 0, 0),
                new Vector3(0, 0, 1f), new Vector3(0, 0, -1f),
            })
            {
                var from = at + offset + new Vector3(0, 0.6f, 0);
                var query = PhysicsRayQueryParameters3D.Create(from, from - new Vector3(0, 2f, 0),
                    collisionMask: 1);
                if (space.IntersectRay(query).Count > 0) continue;
                floating++;
                failures.Add($"§4.9 socket {def.Id} has nothing under it "
                    + $"{offset.Length():0} m to the {(offset == Vector3.Zero ? "centre" : "side")}");
                break;
            }
        }
        report.Add($"§4.9 deck pads footed — {checkedCount - floating}/{checkedCount}");
    }

    /// <summary>§4.8 and §4.10 — nothing solid standing in a walked lane, and
    /// enough headroom over it to walk under.
    ///
    /// Decoration has no collision, so a wall across a lane does not stop
    /// anything; it just means the map does not know where its own gameplay
    /// is. Solid geometry in a lane is worse — it is a wall enemies walk
    /// through, which is how the Switchyard retaining walls read before they
    /// were moved to the perimeter.</summary>
    private void RuleLanesAreClear(List<(RouteDef Route, Vector3 At, float Since)> samples,
        List<string> report, List<string> failures)
    {
        var space = GetWorld3D().DirectSpaceState;
        // One shape and one query object for the whole sweep. A BoxShape3D per
        // sample is a physics-server resource per sample, and creating and
        // dropping seventy of them left Godot's shutdown racing its own mutex
        // — the report was written and the process still aborted after it.
        var box = new BoxShape3D { Size = new Vector3(3.4f, 1.6f, 3.4f) };
        var shape = new PhysicsShapeQueryParameters3D { Shape = box, CollisionMask = 1 };
        int blocked = 0, lowClearance = 0, count = 0;
        foreach (var (route, at, _) in samples)
        {
            if (route.Layer != EnemyLayer.Ground) continue;
            count++;

            // Knee to head height, across the lane's own width.
            shape.Transform = new Transform3D(Basis.Identity, at + new Vector3(0, 1.1f, 0));
            if (space.IntersectShape(shape, maxResults: 1).Count > 0)
            {
                blocked++;
                failures.Add($"§4.8 lane {route.Id} is blocked by solid geometry "
                    + $"at ({at.X:0},{at.Z:0})");
            }

            // Three metres of headroom: a player and an enemy both pass.
            var up = PhysicsRayQueryParameters3D.Create(
                at + new Vector3(0, 2f, 0), at + new Vector3(0, 3f, 0), collisionMask: 1);
            if (space.IntersectRay(up).Count > 0)
            {
                lowClearance++;
                failures.Add($"§4.10 lane {route.Id} has under 3 m of headroom "
                    + $"at ({at.X:0},{at.Z:0})");
            }
        }
        // Let the physics-server resources go while the tree is still up.
        // Held until teardown, Godot's shutdown races its own mutex and aborts
        // the process after the report has already been written — which reads
        // in CI as a probe that crashed rather than one that failed.
        shape.Shape = null;
        shape.Dispose();
        box.Dispose();

        report.Add($"§4.8/§4.10 lanes clear — {count - blocked}/{count} unobstructed, "
            + $"{count - lowClearance}/{count} with headroom");
    }

    /// <summary>True if any ground route other than <paramref name="exceptId"/>
    /// passes within <paramref name="radius"/> of a point. The cutting has to
    /// stay off the long route as well as off the sockets — a walled channel
    /// laid across the switchback would wall in the route it is not about.</summary>
    private bool CrossesOtherGroundRoute(Vector3 at, float radius, string exceptId)
    {
        foreach (var route in _map.Routes)
        {
            if (route.Layer != EnemyLayer.Ground || route.Id == exceptId) continue;
            for (int i = 0; i < route.Waypoints.Count - 1; i++)
            {
                if (route.IsTeleportLeg(i)) continue;
                var a = ToGd(route.Waypoints[i]);
                var b = ToGd(route.Waypoints[i + 1]);
                if (DistanceToSegment(Flat(at), Flat(a), Flat(b)) < radius) return true;
            }
        }
        return false;
    }

    /// <summary>A run of track between two points: the lane module every 4 m,
    /// turned to lie along the run. This is the only thing in the map that
    /// should carry a rail.</summary>
    private void RailRun(Vector3 from, Vector3 to)
    {
        var along = to - from;
        float span = along.Length();
        if (span < 1f) return;
        float yaw = MapKit.YawAlongX(along.Normalized());
        int count = Mathf.Max(1, Mathf.RoundToInt(span / 4f));
        for (int i = 0; i < count; i++)
        {
            var at = from + along * ((i + 0.5f) / count);
            if (MapKit.Prop(this, "switchyard_path_ground", new Vector3(at.X, 0.02f, at.Z), yaw) is { } piece)
                MapKit.ThinOut(piece, i, "lane_marker", "lane_marker_cap");
        }
        _railRuns.Add((from, to));
    }

    /// <summary>An overbridge across the roads: a deck on two piers, set
    /// outside the outermost track. Seven metres of headroom, which is about
    /// the twenty-two feet a real structure over a railway is held to.</summary>
    private void SwitchyardOverbridge(float x)
    {
        // The span reaches from just outside the running line to the yard's
        // back wall. It used to start at z 12, which is inside the playfield —
        // so both bridges put a pier and eleven metres of deck out over the
        // ground the players fight on, and the structure that was meant to
        // read as crossing the railway crossed the battlefield instead.
        const float DeckY = 7f, SouthZ = 18.5f, NorthZ = NorthWallZ;
        var deck = AddStaticBox(new Vector3(x, DeckY, (SouthZ + NorthZ) * 0.5f),
            new Vector3(5f, 0.4f, NorthZ - SouthZ), new Color(0.44f, 0.46f, 0.53f), layer: 0);
        MapKit.MountRun(deck, "switchyard_middeck", NorthZ - SouthZ, 4f,
            alongX: false, MapKit.GroundLocal(deck), 90f);

        foreach (float z in new[] { SouthZ, NorthZ })
        {
            var pier = AddStaticBox(new Vector3(x, DeckY * 0.5f, z),
                new Vector3(1.6f, DeckY, 1.6f), new Color(0.4f, 0.42f, 0.48f), layer: 0);
            MapKit.Mount(pier, "switchyard_column", MapKit.GroundLocal(pier));
        }
    }

    /// <summary>A portal where a road leaves the map: two abutments faced with
    /// retaining wall and a header across them, so the line runs into something
    /// instead of stopping in open ground.</summary>
    private void SwitchyardPortal(Vector3 at)
    {
        foreach (int side in new[] { -1, 1 })
        {
            var abutment = AddStaticBox(at + new Vector3(0, 3f, side * 4f),
                new Vector3(3f, 6f, 4f), new Color(0.38f, 0.39f, 0.44f), layer: 1);
            MapKit.Mount(abutment, "switchyard_retainingwall", MapKit.GroundLocal(abutment), 90f);
        }
        AddStaticBox(at + new Vector3(0, 6.4f, 0), new Vector3(3f, 1.6f, 12f),
            new Color(0.34f, 0.35f, 0.4f), layer: 1);
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
        // Shadows, sized to the map rather than left at the default.
        //
        // Godot's default is four cascades out to a hundred metres, which is
        // generous on a 110 x 80 yard and ruinous on a farm three hundred
        // metres across: every cascade re-renders everything inside it, and
        // this map has a treeline. Two splits, the near one sharp, and a
        // distance that comes off the field — beyond which the ground is flat
        // grass and a shadow on it says nothing.
        float shadowRange = Mathf.Max(100f, Mathf.Max(map.FieldX, map.FieldZ) * 0.4f);
        var sun = new DirectionalLight3D
        {
            ShadowEnabled = true,
            DirectionalShadowMode = DirectionalLight3D.ShadowMode.Parallel2Splits,
            DirectionalShadowMaxDistance = shadowRange,
            DirectionalShadowSplit1 = 0.15f,
            DirectionalShadowFadeStart = 0.85f,
            DirectionalShadowBlendSplits = true,
            ShadowBias = 0.04f,
            ShadowNormalBias = 1.5f,
        };
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

        // Design's skybox is real geometry, not a cubemap — a 700 m dome that
        // gives each map its own horizon. The procedural sky stays underneath
        // as the ambient light source.
        if (MapKit.Prop(this, $"{map.Id}_skybox", Vector3.Zero) is { } dome)
        {
            MapKit.NoShadow(dome);
            MapKit.SeenFromInside(dome);
        }
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
        root.SetMeta("def_id", towerDefId);
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
        // Settings that need something outside the pause screen to apply them.
        _screens.OnConnectionAction = _ =>
        {
            _screens.HideStatus();
            GetTree().Quit();       // same exit the leave button uses
        };

        _screens.OnFieldOfView = degrees => _player?.SetFieldOfView(degrees);
        _screens.OnSensitivity = scale => { if (_player is not null) _player.SensitivityScale = scale; };
        _screens.OnHudScale = scale => _hud.Scale = new Vector2(scale, scale);
        _screens.OnVolume = SetMasterVolume;

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
        _picker = new TeleportPicker { Name = "TeleportPicker" };
        _overlay.AddChild(_picker);

        _upgrade = new UpgradePanel { Name = "UpgradePanel" };
        _overlay.AddChild(_upgrade);

        _armory = new ArmoryScreen { Name = "Armory" };
        _armory.Submit = Submit;
        _armory.RecraftBlueprint = RecraftBlueprint;
        _armory.HasBlueprint = weaponId => _profile.BlueprintSlots.ContainsKey(weaponId);
        _armory.BlueprintSlots = weaponId => _profile.BlueprintSlots.TryGetValue(weaponId, out var slots) ? slots : null;
        _armory.BlueprintAmmo = weaponId => _profile.BlueprintAmmo.TryGetValue(weaponId, out var ammo) ? ammo : null;
        _armory.SaveBlueprint = SaveBlueprint;
        _overlay.AddChild(_armory);

        _lobby = new LobbyScreen { Name = "Lobby" };
        _overlay.AddChild(_lobby);
        _lobby.Build(_profile);
        _lobby.OnSolo = faction => { _map = _lobby.SelectedMap; _playerName = _lobby.PlayerName; StartSolo(faction); };
        _lobby.OnHost = faction => { _map = _lobby.SelectedMap; _playerName = _lobby.PlayerName; StartHost(faction); };
        _lobby.OnJoin = (address, faction) => { _playerName = _lobby.PlayerName; StartClient(address, faction); };
        _lobby.OnPickFaction = faction => Submit(new Command.SetFaction(LocalPlayerId, faction, _profile.LevelFor(faction)));
        _lobby.OnLaunch = () => Submit(new Command.Launch(LocalPlayerId));
        _lobby.OnLeave = () => GetTree().Quit();

        // World-space marker layer.
        _markers = new WorldMarkers { Name = "Markers" };
        AddChild(_markers);
        Vfx = new Vfx { Name = "Vfx" };
        AddChild(Vfx);

        _ghost = new BuildGhost { Name = "BuildGhost" };
        AddChild(_ghost);

        _upgradeRings = new CoverageRings { Name = "UpgradeRings" };
        AddChild(_upgradeRings);
        _upgradeRings.Hide3D();
    }

    // =====================================================================
    // UI entry points called by Player
    // =====================================================================

    /// <summary>True while any surface owns the mouse — Player suspends look
    /// and fire while these are up.</summary>
    public bool UiCapturesMouse => _armory.IsOpen || _screens.PauseOpen || _lobby.Visible;

    public bool WheelOpen => _wheel.IsOpen;
    public bool UpgradeOpen => _upgrade.IsOpen;

    /// <summary>Any radial is up, so mouse motion steers it instead of the
    /// camera and a number key picks a wedge instead of doing nothing. Two of
    /// these exist now; everything that used to ask about the build wheel
    /// specifically meant this.</summary>
    public bool RadialOpen => _wheel.IsOpen || _picker.IsOpen;
    public bool PickerOpen => _picker.IsOpen;

    /// <summary>Every pad but the one you are standing on, nearest first.</summary>
    public void OpenTeleportPicker(string fromPadId)
    {
        var from = _teleportPads.FirstOrDefault(p => p.Id == fromPadId);
        if (from is null) return;
        _picker.Open(from.Label, _teleportPads
            .Where(p => p.Id != fromPadId)
            .Select(p => new TeleportPicker.Destination(p.Id, p.Label, from.At.DistanceTo(p.At)))
            .OrderBy(d => d.Metres));
    }

    /// <summary>Release: start the charge toward the highlighted pad. The
    /// player is not moved here — standing still on the pad for the charge is
    /// the cost, and leaving it cancels.</summary>
    public void ConfirmTeleportPicker()
    {
        var pick = _picker.Selection;
        _picker.Close();
        if (pick is null) return;
        var destination = _teleportPads.FirstOrDefault(p => p.Id == pick.Id);
        if (destination is null) return;
        _player?.BeginTeleport(destination.Id, destination.At);
    }

    public void CancelTeleportPicker() => _picker.Close();

    /// <summary>Every pad shows the cooldown, not just the one you used: the
    /// cooldown is personal, so what the player needs to know is whether the
    /// network is available at all, and four pads disagreeing about that is a
    /// worse lie than all four being pessimistic.</summary>
    public void RefreshPadArt(string chargingPadId, bool onCooldown)
    {
        foreach (var pad in _teleportPads)
        {
            string state = pad.Id == chargingPadId ? "charged" : onCooldown ? "cooldown" : "idle";
            if (_padState.TryGetValue(pad.Id, out string? shown) && shown == state) continue;
            _padState[pad.Id] = state;
            SetPadArt(pad.Id, state);
        }
    }

    private readonly Dictionary<string, string> _padState = new();

    public void OpenBuildWheel(string socketId)
    {
        var socket = _map.Sockets.FirstOrDefault(s => s.Id == socketId);
        if (socket is null || _view.SocketOccupied(socketId)) return;
        _wheel.Open(socket, _view);
    }

    public void SteerWheel(Vector2 relative)
    {
        if (_picker.IsOpen) { _picker.Steer(relative); return; }
        _wheel.Steer(relative);
        UpdateGhost();
    }

    public void WheelSelect(int index)
    {
        if (_picker.IsOpen) { _picker.SelectIndex(index); return; }
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
        _upgrade.Open(structure, _view, _map);
        UpdateUpgradeRings();
    }

    /// <summary>Draws the open panel's structure coverage on the deck, and the
    /// wider ring the next Range level would buy. Re-read every frame rather
    /// than once on open, because an upgrade lands while the panel is up and a
    /// ring that does not grow with the purchase is the one thing worse than
    /// no ring at all. Weather is in the number too — fog shrinks what a tower
    /// sees, and that is exactly the wave a player checks.</summary>
    private void UpdateUpgradeRings()
    {
        if (!_upgrade.IsOpen || _upgrade.TargetId < 0) { _upgradeRings.Hide3D(); return; }

        var structure = _view.Structures.FirstOrDefault(s => s.Id == _upgrade.TargetId);
        if (structure is null || structure.IsTrap
            || !Towers.All.TryGetValue(structure.DefId, out var def))
        {
            _upgradeRings.Hide3D();
            return;
        }

        var socket = _map.Sockets.FirstOrDefault(s => s.Id == structure.SocketId);
        if (socket is null) { _upgradeRings.Hide3D(); return; }

        _upgradeRings.Position = ToGd(socket.Pos);
        _upgradeRings.ShowForTower(def, structure.PathLevels, _map, _view.Wave);
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
            _upgradeRings.Hide3D();
        }
    }

    public void CloseUpgradePanel()
    {
        _upgrade.Close();
        _upgradeRings.Hide3D();
    }

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
            _screens.BindSettings(_profile);   // show what is actually saved
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
    /// <summary>Writes the build as it stands into the profile. Crafting already
    /// records each module as it lands, so this mostly matters for the ammo and
    /// for saving a build on purpose from the Blueprints tab.</summary>
    public void SaveBlueprint(string weaponId)
    {
        if (_view.Local is not { } local) return;
        foreach (var (slot, attachmentId) in local.AttachmentsFor(weaponId))
            _profile.RecordAttachment(weaponId, slot.ToString(), attachmentId);
        _profile.RecordAmmo(weaponId, local.AmmoFor(weaponId));
    }

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
