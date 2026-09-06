using Godot;
using System.Linq;
using DeepField.Sim;
using DeepField.Sim.Content;

namespace DeepField.Game;

/// <summary>Client-authoritative FPS controller. Movement and traversal never
/// touch the sim; shooting, building, abilities and revives enter it as
/// commands via GameRoot.Submit.
///
/// Build and upgrade are hold-to-open surfaces: hold E at a socket for the
/// build wheel, hold U at a structure for the upgrade panel. While either is
/// open, mouse motion steers the menu instead of the camera.</summary>
public partial class Player : CharacterBody3D
{
    private const float MoveSpeed = 6.5f;
    private const float SprintSpeed = 10f;
    private const float JumpVelocity = 4.8f;
    private const float ClimbSpeed = 4f;
    private const float ZipSpeed = 14f;
    private const float MouseSensitivity = 0.0022f;
    private const float InteractRange = 9f;

    private Camera3D _camera = null!;
    private GameRoot _root = null!;
    private Area3D _sensor = null!;
    private float _pitch;
    private double _fireCooldown;

    private bool _onLadder;
    private Vector3? _zipTarget;

    // What the player is currently looking at (refreshed each physics frame).
    private string _aimSocketId = "";
    private int _aimEnemyId = -1;

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
            case InputEventMouseMotion motion:
                // While a radial menu is open the same motion steers it.
                if (_root.WheelOpen) { _root.SteerWheel(motion.Relative); break; }
                if (Input.MouseMode != Input.MouseModeEnum.Captured) break;
                RotateY(-motion.Relative.X * MouseSensitivity);
                _pitch = Mathf.Clamp(_pitch - motion.Relative.Y * MouseSensitivity, -1.5f, 1.5f);
                _camera.Rotation = new Vector3(_pitch, 0, 0);
                break;

            case InputEventMouseButton { Pressed: true, ButtonIndex: MouseButton.Left }
                when Input.MouseMode != Input.MouseModeEnum.Captured && !_root.UiCapturesMouse:
                Input.MouseMode = Input.MouseModeEnum.Captured;
                break;
        }
    }

    public override void _UnhandledInput(InputEvent @event)
    {
        if (@event is not InputEventKey { Pressed: true, Echo: false } key) return;

        switch (key.Keycode)
        {
            case Key.Escape: _root.ToggleSystemMenu(); break;
            case Key.Tab: _root.ToggleArmory(); break;
            case Key.F: _root.Submit(new Command.StartWave(_root.LocalPlayerId)); break;
            case Key.Q: UseAbility(); break;
            case Key.F9: _root.SaveGame(); break;
            case Key.F10: _root.LoadGame(); break;

            // Number keys mean "pick this wedge / this path" while a surface is
            // open, and nothing otherwise (the armory owns weapon switching).
            case Key.Key1: NumberKey(1); break;
            case Key.Key2: NumberKey(2); break;
            case Key.Key3: NumberKey(3); break;
            case Key.Key4: NumberKey(4); break;
            case Key.Key5: NumberKey(5); break;
            case Key.Key6: NumberKey(6); break;
        }
    }

    private void NumberKey(int oneBased)
    {
        if (_root.WheelOpen) _root.WheelSelect(oneBased - 1);
        else if (_root.UpgradeOpen) _root.UpgradeKey(oneBased);
    }

    public override void _PhysicsProcess(double delta)
    {
        UpdateAim();
        UpdateBuildSurfaces(delta);

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

        bool uiOwnsInput = _root.UiCapturesMouse;
        _onLadder = !uiOwnsInput && InArea("ladder");

        var velocity = Velocity;

        if (_onLadder)
        {
            float climb = 0f;
            if (Input.IsPhysicalKeyPressed(Key.W)) climb = ClimbSpeed;
            else if (Input.IsPhysicalKeyPressed(Key.S)) climb = -ClimbSpeed;
            velocity.Y = climb;
        }
        else if (!IsOnFloor())
        {
            velocity.Y -= 9.8f * (float)delta;
        }
        else if (!uiOwnsInput && Input.IsPhysicalKeyPressed(Key.Space))
        {
            velocity.Y = JumpVelocity;
        }

        foreach (var area in _sensor.GetOverlappingAreas())
        {
            if ((string)area.GetMeta("kind", "") == "launcher" && IsOnFloor())
                velocity = (Vector3)area.GetMeta("launch_velocity");
        }

        var input = Vector2.Zero;
        if (!uiOwnsInput)
        {
            if (Input.IsPhysicalKeyPressed(Key.W)) input.Y -= 1;
            if (Input.IsPhysicalKeyPressed(Key.S)) input.Y += 1;
            if (Input.IsPhysicalKeyPressed(Key.A)) input.X -= 1;
            if (Input.IsPhysicalKeyPressed(Key.D)) input.X += 1;
        }

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

        // Fire: blocked while a menu owns the mouse or a build surface is open.
        _fireCooldown -= delta;
        if (!uiOwnsInput && !_root.WheelOpen && !_root.UpgradeOpen
            && Input.MouseMode == Input.MouseModeEnum.Captured
            && Input.IsMouseButtonPressed(MouseButton.Left)
            && _fireCooldown <= 0)
        {
            var weapon = Weapons.All[_root.CurrentWeaponId()];
            _fireCooldown = 1.0 / weapon.ShotsPerSecond;
            Fire(weapon);
        }

        if (!uiOwnsInput && Input.IsPhysicalKeyPressed(Key.R))
            TryRevive();
    }

    // =====================================================================
    // Aim + contextual surfaces
    // =====================================================================

    private void UpdateAim()
    {
        _aimSocketId = "";
        _aimEnemyId = -1;

        // Enemies first (they're what you shoot), then sockets (what you build on).
        if (Raycast(80f, worldMask: 1, areaMask: 1 << 1) is { } shot
            && shot.Collider is Area3D area && area.HasMeta("enemy_id"))
            _aimEnemyId = area.GetMeta("enemy_id").AsInt32();

        if (Raycast(InteractRange, worldMask: 1 | (1 << 3), areaMask: 0) is { } reach
            && reach.Collider is StaticBody3D body && body.HasMeta("socket_id"))
            _aimSocketId = body.GetMeta("socket_id").AsString();

        _root.ReportAim(_aimEnemyId);
        _root.SetHint(BuildHint());
    }

    private string BuildHint()
    {
        if (_root.WheelOpen) return "steer to a wedge · release E to build · 1-6 to pick";
        if (_root.UpgradeOpen) return "1-3 upgrade a path · hold X to sell · release U to close";
        if (InArea("armory")) return "[Tab] armory   ·   [5] recraft blueprint";
        if (InArea("zipline")) return "[E] ride the zipline";
        if (InArea("ladder")) return "[W] climb";
        if (InArea("controlPoint")) return "control point";

        if (_aimSocketId.Length > 0)
        {
            var socket = _root.CurrentMap().Sockets.FirstOrDefault(s => s.Id == _aimSocketId);
            if (socket is null) return "";
            return _root.View.SocketOccupied(_aimSocketId)
                ? "[hold U] upgrade or sell"
                : $"[hold E] build on {socket.Tag.ToString().ToLowerInvariant()} socket";
        }
        return "";
    }

    /// <summary>Hold-to-open, release-to-commit for both build surfaces.</summary>
    private void UpdateBuildSurfaces(double delta)
    {
        if (_root.UiCapturesMouse)
        {
            if (_root.WheelOpen) _root.CancelBuildWheel();
            if (_root.UpgradeOpen) _root.CloseUpgradePanel();
            return;
        }

        bool buildHeld = Input.IsPhysicalKeyPressed(Key.E);
        bool upgradeHeld = Input.IsPhysicalKeyPressed(Key.U);

        // E in a zipline volume rides instead of building — traversal wins,
        // since you can't build on a zipline anyway.
        if (buildHeld && !_root.WheelOpen && InArea("zipline"))
        {
            foreach (var area in _sensor.GetOverlappingAreas())
                if ((string)area.GetMeta("kind", "") == "zipline")
                    _zipTarget = (Vector3)area.GetMeta("zip_end");
            return;
        }

        if (buildHeld && !_root.WheelOpen && _aimSocketId.Length > 0
            && !_root.View.SocketOccupied(_aimSocketId))
            _root.OpenBuildWheel(_aimSocketId);
        else if (!buildHeld && _root.WheelOpen)
            _root.ConfirmBuildWheel();

        if (upgradeHeld && !_root.UpgradeOpen && _aimSocketId.Length > 0
            && _root.View.SocketOccupied(_aimSocketId))
            _root.OpenUpgradePanel(_aimSocketId);
        else if (!upgradeHeld && _root.UpgradeOpen)
            _root.CloseUpgradePanel();

        if (_root.UpgradeOpen)
            _root.TickUpgradeSell(delta, Input.IsPhysicalKeyPressed(Key.X));
    }

    private bool InArea(string kind) =>
        _sensor.GetOverlappingAreas().Any(a => (string)a.GetMeta("kind", "") == kind);

    // =====================================================================
    // Actions
    // =====================================================================

    private void Fire(WeaponDef weapon)
    {
        var hit = Raycast(weapon.RangeMeters, worldMask: 1, areaMask: 1 << 1);
        if (hit is { } result && result.Collider is Area3D area && area.HasMeta("enemy_id"))
            _root.Submit(new Command.PlayerHit(_root.LocalPlayerId,
                area.GetMeta("enemy_id").AsInt32(), weapon.Id));
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
