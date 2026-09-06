using Godot;
using System.Collections.Generic;
using DeepField.Sim;
using DeepField.Sim.Content;
using SimWorld = DeepField.Sim.World;

namespace DeepField.Game;

/// <summary>Owns the in-process sim (single-player = local loopback, the same
/// seam the dedicated server slots into at M1) and syncs the view to it.
/// The view reads sim state every frame and writes back only through commands.</summary>
public partial class GameRoot : Node3D
{
    private SimWorld _world = null!;
    private double _accumulator;

    private readonly Dictionary<int, Node3D> _enemyViews = new();
    private readonly Dictionary<int, Node3D> _projectileViews = new();
    private readonly Dictionary<int, Node3D> _towerViews = new();

    private Label _hudLabel = null!;
    private Label _toastLabel = null!;
    private Label _capturePrompt = null!;
    private ColorRect _crosshair = null!;
    private double _toastTimer;
    private double _hitFlashTimer;

    private const uint Seed = 12345;
    private const float SimTierY = 0f;

    public MapDef Map => _world.Map;

    public override void _Ready()
    {
        _world = new SimWorld(Seed, Maps.TestLane);

        BuildEnvironment();
        BuildGraybox();
        BuildHud();

        var player = new Player { Name = "Player" };
        AddChild(player);
        player.GlobalPosition = ToGd(Maps.TestLane.HeroSpawn) + new Vector3(0, 1.2f, 0);
        // Face the lane (it sits at +Z from the hero spawn; default forward is -Z).
        player.RotationDegrees = new Vector3(0, 180, 0);
    }

    public override void _Process(double delta)
    {
        // Fixed 30 Hz accumulator; render rate never touches sim behavior.
        _accumulator = Mathf.Min(_accumulator + delta, 0.25);
        while (_accumulator >= Balance.Dt)
        {
            Step.Advance(_world);
            DrainEvents();
            _accumulator -= Balance.Dt;
        }

        SyncEnemyViews();
        SyncProjectileViews();
        UpdateHud(delta);
    }

    // ---- Command entry points (called by Player) -------------------------

    public void EnqueuePlayerHit(int enemyId) =>
        _world.Enqueue(new Command.PlayerHit(1, enemyId, Weapons.Sidearm.Id));

    public void EnqueuePlaceTower(string socketId) =>
        _world.Enqueue(new Command.PlaceTower(1, Towers.Lance.Id, socketId));

    public void EnqueueStartWave() =>
        _world.Enqueue(new Command.StartWave(1));

    public bool SocketOccupied(string socketId) =>
        _world.Towers.Exists(t => t.SocketId == socketId);

    // ---- Sim → view sync -------------------------------------------------

    private void DrainEvents()
    {
        foreach (var e in _world.Events)
        {
            switch (e)
            {
                case SimEvent.TowerPlaced placed:
                {
                    var socket = _world.Map.Sockets[0];
                    foreach (var s in _world.Map.Sockets)
                        if (s.Id == placed.SocketId) socket = s;
                    _towerViews[placed.TowerId] = SpawnTowerView(ToGd(socket.Pos));
                    break;
                }
                case SimEvent.BuildRejected rejected:
                    Toast($"build rejected: {rejected.Reason}");
                    break;
                case SimEvent.WaveStarted started:
                    Toast($"wave {started.WaveIndex + 1} — {started.EnemyCount} inbound");
                    break;
                case SimEvent.EnemyLeaked:
                    Toast("breach! core hit");
                    break;
                case SimEvent.EnemyDamaged damaged when damaged.Source.StartsWith("player"):
                    _hitFlashTimer = 0.12;
                    break;
                case SimEvent.EnemyDied died when died.Source.StartsWith("player"):
                    _hitFlashTimer = 0.3;
                    Toast($"+{died.Bounty} credits");
                    break;
                case SimEvent.MatchEnded ended:
                    Toast(ended.Victory ? "VICTORY" : "DEFEAT");
                    break;
            }
        }
    }

    private void SyncEnemyViews()
    {
        foreach (var enemy in _world.Enemies)
        {
            if (!_enemyViews.TryGetValue(enemy.Id, out var view))
            {
                view = SpawnEnemyView(enemy.Id);
                _enemyViews[enemy.Id] = view;
            }
            view.Position = ToGd(enemy.Pos);

            // Health readability: tint toward red as hp drops.
            var mesh = view.GetNode<MeshInstance3D>("Mesh");
            if (mesh.MaterialOverride is StandardMaterial3D mat)
            {
                float t = enemy.Hp / enemy.MaxHp;
                mat.AlbedoColor = new Color(0.9f, 0.25f + 0.55f * t, 0.2f + 0.5f * t);
            }
        }

        // Mark-and-sweep disposal, the worldView.ts pattern.
        var live = new HashSet<int>();
        foreach (var enemy in _world.Enemies) live.Add(enemy.Id);
        var stale = new List<int>();
        foreach (var id in _enemyViews.Keys)
            if (!live.Contains(id)) stale.Add(id);
        foreach (var id in stale)
        {
            _enemyViews[id].QueueFree();
            _enemyViews.Remove(id);
        }
    }

    private void SyncProjectileViews()
    {
        foreach (var projectile in _world.Projectiles)
        {
            if (!_projectileViews.TryGetValue(projectile.Id, out var view))
            {
                view = SpawnProjectileView();
                _projectileViews[projectile.Id] = view;
            }
            view.Position = ToGd(projectile.Pos);
        }

        var live = new HashSet<int>();
        foreach (var projectile in _world.Projectiles) live.Add(projectile.Id);
        var stale = new List<int>();
        foreach (var id in _projectileViews.Keys)
            if (!live.Contains(id)) stale.Add(id);
        foreach (var id in stale)
        {
            _projectileViews[id].QueueFree();
            _projectileViews.Remove(id);
        }
    }

    private void UpdateHud(double delta)
    {
        string phase = _world.Phase switch
        {
            MatchPhase.Intermission => $"intermission {_world.PhaseTimer:0.0}s  [F] start wave",
            MatchPhase.Wave => $"wave {_world.WaveIndex + 1}/{_world.Map.TotalWaves}",
            MatchPhase.Victory => "VICTORY",
            MatchPhase.Defeat => "DEFEAT",
            _ => "",
        };
        _hudLabel.Text = $"credits {_world.Money}    lives {_world.Lives}    {phase}\n" +
                         "[E] build lance on socket   [LMB] fire   [Esc] mouse";

        if (_toastTimer > 0)
        {
            _toastTimer -= delta;
            if (_toastTimer <= 0) _toastLabel.Text = "";
        }

        // Hit feedback: crosshair flashes and swells on player damage/kill.
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

        _capturePrompt.Visible = Input.MouseMode != Input.MouseModeEnum.Captured;
    }

    private void Toast(string message)
    {
        _toastLabel.Text = message;
        _toastTimer = 2.5;
    }

    // ---- Graybox construction -------------------------------------------

    private void BuildEnvironment()
    {
        var sun = new DirectionalLight3D { ShadowEnabled = true };
        sun.RotationDegrees = new Vector3(-55, -30, 0);
        AddChild(sun);

        var env = new Godot.Environment
        {
            BackgroundMode = Godot.Environment.BGMode.Sky,
            Sky = new Sky { SkyMaterial = new ProceduralSkyMaterial() },
            AmbientLightSource = Godot.Environment.AmbientSource.Sky,
            AmbientLightEnergy = 0.6f,
        };
        AddChild(new WorldEnvironment { Environment = env });
    }

    private void BuildGraybox()
    {
        // Ground slab (collision layer 1 = world).
        AddStaticBox(new Vector3(0, -0.5f, 0), new Vector3(90, 1, 60), new Color(0.35f, 0.38f, 0.4f), layer: 1);

        // Path ribbon: one thin box per route leg, slightly raised so it reads.
        var route = Maps.TestLane.Route;
        for (int i = 0; i < route.Count - 1; i++)
        {
            var a = ToGd(route[i]);
            var b = ToGd(route[i + 1]);
            var mid = (a + b) * 0.5f + new Vector3(0, 0.06f, 0);
            var length = (b - a).Length();
            var horizontal = new Vector3(b.X - a.X, 0, b.Z - a.Z);
            var box = AddStaticBox(mid, new Vector3(length, 0.1f, 3.4f), new Color(0.2f, 0.22f, 0.27f), layer: 0);
            box.Rotation = new Vector3(0, Mathf.Atan2(-horizontal.Z, horizontal.X), 0);
        }

        // Spawn + goal markers.
        AddStaticBox(ToGd(route[0]) + new Vector3(0, 1.5f, 0), new Vector3(3.5f, 3f, 3.5f), new Color(0.75f, 0.45f, 0.15f), layer: 0);
        AddStaticBox(ToGd(route[^1]) + new Vector3(0, 1.5f, 0), new Vector3(3.5f, 3f, 3.5f), new Color(0.2f, 0.55f, 0.85f), layer: 0);

        // Tower sockets: cylinders on collision layer 4, tagged with socket_id.
        foreach (var socket in Maps.TestLane.Sockets)
        {
            var body = new StaticBody3D { CollisionLayer = 1 << 3, CollisionMask = 0 };
            var mesh = new MeshInstance3D
            {
                Mesh = new CylinderMesh { TopRadius = 1.1f, BottomRadius = 1.1f, Height = 0.25f },
                MaterialOverride = new StandardMaterial3D
                {
                    AlbedoColor = new Color(0.5f, 0.75f, 0.5f),
                    EmissionEnabled = true,
                    Emission = new Color(0.15f, 0.4f, 0.15f),
                },
            };
            var shape = new CollisionShape3D
            {
                Shape = new CylinderShape3D { Radius = 1.2f, Height = 1.2f },
            };
            body.AddChild(mesh);
            body.AddChild(shape);
            body.Position = ToGd(socket.Pos) + new Vector3(0, 0.12f, 0);
            body.SetMeta("socket_id", socket.Id);
            AddChild(body);
        }
    }

    private StaticBody3D AddStaticBox(Vector3 position, Vector3 size, Color color, uint layer)
    {
        var body = new StaticBody3D { CollisionLayer = layer, CollisionMask = 0 };
        body.AddChild(new MeshInstance3D
        {
            Mesh = new BoxMesh { Size = size },
            MaterialOverride = new StandardMaterial3D { AlbedoColor = color },
        });
        if (layer != 0)
            body.AddChild(new CollisionShape3D { Shape = new BoxShape3D { Size = size } });
        body.Position = position;
        AddChild(body);
        return body;
    }

    private Node3D SpawnEnemyView(int enemyId)
    {
        var root = new Node3D();

        var mesh = new MeshInstance3D
        {
            Name = "Mesh",
            Mesh = new CapsuleMesh { Radius = 0.45f, Height = 1.6f },
            MaterialOverride = new StandardMaterial3D { AlbedoColor = new Color(0.9f, 0.8f, 0.7f) },
            Position = new Vector3(0, 0.8f, 0),
        };
        root.AddChild(mesh);

        // Hurtbox for player hitscan (layer 2). Area, not body — enemies have no
        // physics presence for movement, exactly per the plan.
        var area = new Area3D { CollisionLayer = 1 << 1, CollisionMask = 0 };
        area.AddChild(new CollisionShape3D
        {
            Shape = new CapsuleShape3D { Radius = 0.5f, Height = 1.7f },
            Position = new Vector3(0, 0.8f, 0),
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
        root.AddChild(new MeshInstance3D
        {
            Mesh = new CylinderMesh { TopRadius = 0.12f, BottomRadius = 0.12f, Height = 1.1f },
            MaterialOverride = new StandardMaterial3D { AlbedoColor = new Color(0.85f, 0.85f, 0.9f) },
            Position = new Vector3(0, 2.4f, 0.5f),
            RotationDegrees = new Vector3(90, 0, 0),
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

    private void BuildHud()
    {
        var canvas = new CanvasLayer();

        _hudLabel = new Label
        {
            Position = new Vector2(16, 12),
            Theme = null,
        };
        canvas.AddChild(_hudLabel);

        _toastLabel = new Label
        {
            Position = new Vector2(16, 64),
            Modulate = new Color(1f, 0.85f, 0.4f),
        };
        canvas.AddChild(_toastLabel);

        // Crosshair (pivot centered so the hit-flash swell stays centered).
        // MouseFilter MUST be Ignore: in captured mode the hidden cursor is pinned
        // at screen center — right on this rect — and ColorRect's default filter
        // (Stop) would eat every mouse-motion event before the player sees it.
        _crosshair = new ColorRect
        {
            Color = new Color(1, 1, 1, 0.8f),
            AnchorLeft = 0.5f, AnchorTop = 0.5f, AnchorRight = 0.5f, AnchorBottom = 0.5f,
            OffsetLeft = -2, OffsetTop = -2, OffsetRight = 2, OffsetBottom = 2,
            PivotOffset = new Vector2(2, 2),
            MouseFilter = Control.MouseFilterEnum.Ignore,
        };
        canvas.AddChild(_crosshair);

        // Shown whenever the OS cursor is loose (macOS ignores capture until the
        // window has focus, and Esc releases it) — one click recaptures.
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

    public static Vector3 ToGd(Vec3 v) => new(v.X, v.Y, v.Z);
}
