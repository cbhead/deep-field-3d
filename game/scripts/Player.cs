using Godot;
using System.Linq;
using DeepField.Sim;
using DeepField.Sim.Content;

namespace DeepField.Game;

/// <summary>Client-authoritative FPS controller. Movement and traversal never
/// touch the sim; shooting, building, abilities and revives enter it as
/// commands via GameRoot.Submit.</summary>
public partial class Player : CharacterBody3D
{
    private const float MoveSpeed = 6.5f;
    private const float SprintSpeed = 10f;
    private const float JumpVelocity = 4.8f;
    private const float ClimbSpeed = 4f;
    private const float ZipSpeed = 14f;
    private const float MouseSensitivity = 0.0022f;

    private Camera3D _camera = null!;
    private GameRoot _root = null!;
    private Area3D _sensor = null!;   // overlaps traversal/armory areas (layer 6)
    private float _pitch;
    private double _fireCooldown;

    private bool _onLadder;
    private Vector3? _zipTarget;

    public override void _Ready()
    {
        _root = GetParent<GameRoot>();

        CollisionLayer = 1 << 4;
        CollisionMask = 1;
        AddChild(new CollisionShape3D
        {
            Shape = new CapsuleShape3D { Radius = 0.4f, Height = 1.8f },
            Position = new Vector3(0, 0.9f, 0),
        });

        // Area sensor: traversal volumes and the armory detect the player, and
        // the player queries what it's inside of.
        _sensor = new Area3D { CollisionLayer = 1 << 4, CollisionMask = 1 << 5 };
        _sensor.AddChild(new CollisionShape3D
        {
            Shape = new CapsuleShape3D { Radius = 0.5f, Height = 1.9f },
            Position = new Vector3(0, 0.9f, 0),
        });
        AddChild(_sensor);

        _camera = new Camera3D { Position = new Vector3(0, 1.6f, 0), Fov = 80 };
        AddChild(_camera);

        CallDeferred(nameof(CaptureMouse));
    }

    private void CaptureMouse() => Input.MouseMode = Input.MouseModeEnum.Captured;

    // Mouse look lives in _Input so no Control node can consume motion events.
    public override void _Input(InputEvent @event)
    {
        switch (@event)
        {
            case InputEventMouseMotion motion when Input.MouseMode == Input.MouseModeEnum.Captured:
                RotateY(-motion.Relative.X * MouseSensitivity);
                _pitch = Mathf.Clamp(_pitch - motion.Relative.Y * MouseSensitivity, -1.5f, 1.5f);
                _camera.Rotation = new Vector3(_pitch, 0, 0);
                break;

            case InputEventMouseButton { Pressed: true, ButtonIndex: MouseButton.Left }
                when Input.MouseMode != Input.MouseModeEnum.Captured:
                Input.MouseMode = Input.MouseModeEnum.Captured;
                break;
        }
    }

    public override void _UnhandledInput(InputEvent @event)
    {
        if (@event is not InputEventKey { Pressed: true, Echo: false } key) return;

        switch (key.Keycode)
        {
            case Key.Escape:
                Input.MouseMode = Input.MouseMode == Input.MouseModeEnum.Captured
                    ? Input.MouseModeEnum.Visible
                    : Input.MouseModeEnum.Captured;
                break;

            case Key.E: TryBuildOrRide(); break;
            case Key.U: TryUpgrade(); break;
            case Key.F: _root.Submit(new Command.StartWave(_root.LocalPlayerId)); break;
            case Key.Q: UseAbility(); break;
            case Key.Key1: ArmoryKey("sidearm"); break;
            case Key.Key2: ArmoryKey("rifle"); break;
            case Key.Key3: ArmoryKey("scattergun"); break;
            case Key.Key4: ArmoryKey("emberPistol"); break;
            case Key.F9: _root.SaveGame(); break;
            case Key.F10: _root.LoadGame(); break;
            case Key.Key5:
                if (_sensor.GetOverlappingAreas().Any(a => (string)a.GetMeta("kind", "") == "armory"))
                    _root.RecraftBlueprint(_root.CurrentWeaponId());
                else
                    _root.Toast("blueprint recraft works at the armory station");
                break;
        }
    }

    public override void _PhysicsProcess(double delta)
    {
        // Zipline ride: kinematic slide to the end point, cancel on arrival.
        if (_zipTarget is { } zip)
        {
            var toEnd = zip - GlobalPosition;
            if (toEnd.Length() < 1.2f)
            {
                _zipTarget = null;
            }
            else
            {
                Velocity = toEnd.Normalized() * ZipSpeed;
                MoveAndSlide();
                return;
            }
        }

        _onLadder = _sensor.GetOverlappingAreas().Any(a => (string)a.GetMeta("kind", "") == "ladder");

        var velocity = Velocity;

        if (_onLadder)
        {
            // W climbs, S descends; gravity off while on the ladder.
            float climb = 0f;
            if (Input.IsPhysicalKeyPressed(Key.W)) climb = ClimbSpeed;
            else if (Input.IsPhysicalKeyPressed(Key.S)) climb = -ClimbSpeed;
            velocity.Y = climb;
        }
        else if (!IsOnFloor())
        {
            velocity.Y -= 9.8f * (float)delta;
        }
        else if (Input.IsPhysicalKeyPressed(Key.Space))
        {
            velocity.Y = JumpVelocity;
        }

        // Launcher pads fling on contact.
        foreach (var area in _sensor.GetOverlappingAreas())
        {
            if ((string)area.GetMeta("kind", "") == "launcher" && IsOnFloor())
            {
                var launch = (Vector3)area.GetMeta("launch_velocity");
                velocity = launch;
            }
        }

        var input = Vector2.Zero;
        if (Input.IsPhysicalKeyPressed(Key.W)) input.Y -= 1;
        if (Input.IsPhysicalKeyPressed(Key.S)) input.Y += 1;
        if (Input.IsPhysicalKeyPressed(Key.A)) input.X -= 1;
        if (Input.IsPhysicalKeyPressed(Key.D)) input.X += 1;

        float speed = Input.IsPhysicalKeyPressed(Key.Shift) ? SprintSpeed : MoveSpeed;
        var direction = (Transform.Basis * new Vector3(input.X, 0, input.Y)).Normalized();
        if (!_onLadder || input.X != 0)
        {
            velocity.X = direction.X * speed;
            velocity.Z = direction.Z * speed;
        }
        else
        {
            velocity.X = 0; velocity.Z = 0;
        }

        Velocity = velocity;
        MoveAndSlide();

        // Held actions.
        _fireCooldown -= delta;
        if (Input.MouseMode == Input.MouseModeEnum.Captured
            && Input.IsMouseButtonPressed(MouseButton.Left)
            && _fireCooldown <= 0)
        {
            var weapon = Weapons.All[_root.CurrentWeaponId()];
            _fireCooldown = 1.0 / weapon.ShotsPerSecond;
            Fire(weapon);
        }

        if (Input.IsPhysicalKeyPressed(Key.R))
            TryRevive();
    }

    private void Fire(WeaponDef weapon)
    {
        var hit = Raycast(weapon.RangeMeters, worldMask: 1, areaMask: 1 << 1);
        if (hit is { } result && result.Collider is Area3D area && area.HasMeta("enemy_id"))
            _root.Submit(new Command.PlayerHit(_root.LocalPlayerId, area.GetMeta("enemy_id").AsInt32(), weapon.Id));
    }

    private void TryBuildOrRide()
    {
        // Zipline first: standing in the start volume rides it.
        foreach (var area in _sensor.GetOverlappingAreas())
        {
            if ((string)area.GetMeta("kind", "") == "zipline")
            {
                _zipTarget = (Vector3)area.GetMeta("zip_end");
                return;
            }
        }

        var hit = Raycast(8f, worldMask: 1 | (1 << 3), areaMask: 0);
        if (hit is { } result && result.Collider is StaticBody3D body && body.HasMeta("socket_id"))
        {
            string socketId = body.GetMeta("socket_id").AsString();
            if (!_root.SocketOccupied(socketId))
                _root.Submit(new Command.PlaceTower(_root.LocalPlayerId, PickTowerFor(socketId), socketId));
        }
    }

    /// <summary>M1 build palette: number of towers is small enough that context
    /// picks — wall sockets get Skywatch (they overlook the air lane), trap
    /// sockets refuse sim-side, ground sockets cycle by what's placed already.</summary>
    private string PickTowerFor(string socketId)
    {
        var map = _root.CurrentMap();
        var socket = map.Sockets.First(s => s.Id == socketId);
        if (socket.Tag == SocketTag.Wall) return "skywatch";
        if (socket.Tag == SocketTag.Barricade) return "barricade";
        if (socket.Tag == SocketTag.Trap)
        {
            int placedTraps = map.Sockets.Count(s => s.Tag == SocketTag.Trap && _root.SocketOccupied(s.Id));
            return placedTraps switch { 0 => "tar", 1 => "spike", _ => "launcher" };
        }

        // Ground cycle: lance, arc, singularity, nova, then lances.
        int placed = map.Sockets.Count(s => s.Tag == SocketTag.Ground && _root.SocketOccupied(s.Id));
        return placed switch { 1 => "arc", 2 => "singularity", 3 => "nova", _ => "lance" };
    }

    private void TryUpgrade()
    {
        // Aim at a tower's socket to level its damage path (full path picker
        // arrives with the real build UI).
        var hit = Raycast(8f, worldMask: 1 | (1 << 3), areaMask: 0);
        if (hit is { } result && result.Collider is StaticBody3D body && body.HasMeta("socket_id"))
        {
            string socketId = body.GetMeta("socket_id").AsString();
            int towerId = _root.TowerIdAtSocket(socketId);
            if (towerId >= 0)
                _root.Submit(new Command.UpgradeTower(_root.LocalPlayerId, towerId, 0));
        }
    }

    private void UseAbility()
    {
        var aim = Raycast(40f, worldMask: 1, areaMask: 1 << 1);
        var target = aim?.Position ?? (_camera.GlobalPosition + (-_camera.GlobalTransform.Basis.Z) * 20f);
        _root.Submit(new Command.UseAbility(_root.LocalPlayerId, new Vec3(target.X, target.Y, target.Z)));
    }

    private void TryRevive()
    {
        int downed = _root.NearestDownedPlayer(GlobalPosition, Balance.ReviveRangeMeters);
        if (downed >= 0)
            _root.Submit(new Command.Revive(_root.LocalPlayerId, downed));
    }

    private void ArmoryKey(string weaponId)
    {
        bool nearArmory = _sensor.GetOverlappingAreas().Any(a => (string)a.GetMeta("kind", "") == "armory");
        if (!nearArmory)
        {
            _root.Toast("armory keys work at the armory station");
            return;
        }
        // Buy if unowned (sim refuses politely if broke), then select.
        _root.Submit(new Command.BuyWeapon(_root.LocalPlayerId, weaponId));
        _root.Submit(new Command.SelectWeapon(_root.LocalPlayerId, weaponId));
    }

    private (GodotObject Collider, Vector3 Position)? Raycast(float range, uint worldMask, uint areaMask)
    {
        var from = _camera.GlobalPosition;
        var to = from + (-_camera.GlobalTransform.Basis.Z) * range;

        var query = PhysicsRayQueryParameters3D.Create(from, to, worldMask | areaMask);
        query.CollideWithAreas = areaMask != 0;
        query.CollideWithBodies = worldMask != 0;
        query.Exclude = new Godot.Collections.Array<Rid> { GetRid() };

        var result = GetWorld3D().DirectSpaceState.IntersectRay(query);
        if (result.Count == 0) return null;
        return ((GodotObject)result["collider"], (Vector3)result["position"]);
    }
}
