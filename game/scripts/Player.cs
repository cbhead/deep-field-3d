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
    private const float BaseMouseSensitivity = 0.0022f;

    /// <summary>Multiplier from the profile, applied on top of the base rate so
    /// the setting means the same thing regardless of what the base becomes.</summary>
    public float SensitivityScale = 1f;
    private const float InteractRange = 9f;

    private Camera3D _camera = null!;

    public void SetFieldOfView(int degrees) { if (_camera is not null) _camera.Fov = degrees; }
    private GameRoot _root = null!;
    private Area3D _sensor = null!;
    private float _pitch;
    private double _fireCooldown;
    private double _meleeCooldown;
    private bool _triggerHeld;
    private Node3D? _viewModel;
    private string _viewModelFor = "";

    /// <summary>The gun in your hands. Design ships weapon_&lt;id&gt;_vm.glb and
    /// hands_&lt;faction&gt;.glb and nothing had ever instanced either, so the
    /// first-person view was a floating crosshair — you could not see what you
    /// were holding, which is most of what a shooter's feel is.
    ///
    /// Parented to the camera at a fixed offset rather than framed by one: a
    /// viewmodel is authored to sit exactly here, which is why this needs none
    /// of the bounds-fitting the gunsmith bench does.</summary>
    private void RefreshViewModel(string weaponId, string factionId)
    {
        if (_viewModelFor == weaponId && GodotObject.IsInstanceValid(_viewModel)) return;
        if (GodotObject.IsInstanceValid(_viewModel)) _viewModel!.QueueFree();
        _viewModelFor = weaponId;

        var rig = new Node3D { Position = new Vector3(0.24f, -0.20f, -0.5f) };
        var gun = AssetLibrary.TryInstantiate($"weapon_{weaponId}_vm");
        if (gun is not null) rig.AddChild(gun);

        var hands = AssetLibrary.TryInstantiate($"hands_{factionId}")
                    ?? AssetLibrary.TryInstantiate("hands_firstperson");
        if (hands is not null) rig.AddChild(hands);

        if (gun is null && hands is null) { _viewModel = null; return; }

        _viewModel = rig;
        _camera.AddChild(rig);
    }

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

    /// <summary>Set by the review shots to hold the build or upgrade key down
    /// for as long as the picture needs. Both surfaces exist only while their
    /// key is held.</summary>
    public bool HoldingBuild { get; set; }
    public bool HoldingUpgrade { get; set; }

    /// <summary>Stand at <paramref name="from"/> and look at <paramref
    /// name="target"/>. Yaw lives on the body and pitch on the camera, which is
    /// why this cannot be a single LookAt from outside — and why the review
    /// shots ask for it here instead of reaching into the rig.
    ///
    /// The surface being photographed has to have its subject in frame: a build
    /// wheel whose ghost is off-screen says nothing about the ghost.</summary>
    public void AimFrom(Vector3 from, Vector3 target)
    {
        GlobalPosition = from;
        var to = target - from;
        var flat = to with { Y = 0f };
        if (flat.Length() < 0.01f) return;

        // Same convention as every other heading in the client: a node at yaw
        // zero faces -Z, so the heading is half a turn from atan2(x, z).
        Rotation = new Vector3(0f, Mathf.Atan2(flat.X, flat.Z) + Mathf.Pi, 0f);
        _pitch = Mathf.Clamp(Mathf.Atan2(to.Y - 1.6f, flat.Length()), -1.5f, 1.5f);
        _camera.Rotation = new Vector3(_pitch, 0f, 0f);
    }

    // Mouse look lives in _Input so no Control node can consume motion events.
    public override void _Input(InputEvent @event)
    {
        switch (@event)
        {
            case InputEventMouseMotion motion:
                // While a radial menu is open the same motion steers it.
                if (_root.WheelOpen) { _root.SteerWheel(motion.Relative); break; }
                if (Input.MouseMode != Input.MouseModeEnum.Captured) break;
                RotateY(-motion.Relative.X * BaseMouseSensitivity * SensitivityScale);
                _pitch = Mathf.Clamp(_pitch - motion.Relative.Y * BaseMouseSensitivity * SensitivityScale, -1.5f, 1.5f);
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
            // R is contextual: it revives when somebody needs reviving and
            // reloads otherwise. Reload wants R — it is where every hand
            // already expects it — and revive was there first, so the two share
            // it on the only rule that never guesses wrong: a downed teammate
            // in range is always the more urgent of the two.
            case Key.R when _root.NearestDownedPlayer(GlobalPosition, Balance.ReviveRangeMeters) < 0:
                _root.Submit(new Command.Reload(_root.LocalPlayerId));
                break;
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

        RefreshViewModel(_root.CurrentWeaponId(), _root.LocalFactionId);

        // Fire: blocked while a menu owns the mouse or a build surface is open.
        _fireCooldown -= delta;
        if (!uiOwnsInput && !_root.WheelOpen && !_root.UpgradeOpen
            && Input.MouseMode == Input.MouseModeEnum.Captured
            && _fireCooldown <= 0)
        {
            var weapon = Weapons.All[_root.CurrentWeaponId()];
            bool held = Input.IsMouseButtonPressed(MouseButton.Left);
            // Semi-automatic weapons want a click each. Holding the button in
            // front of the lane should not be a strategy, and for a pistol it
            // was the strongest one available.
            bool wants = weapon.Automatic ? held : (held && !_triggerHeld);
            _triggerHeld = held;

            if (wants)
            {
                _fireCooldown = 1.0 / weapon.ShotsPerSecond;
                Fire(weapon);
            }
        }
        else if (Input.MouseMode == Input.MouseModeEnum.Captured)
        {
            _triggerHeld = Input.IsMouseButtonPressed(MouseButton.Left);
        }

        // Melee on right mouse. The server resolves the arc from the aim
        // point, so the client sends where you are looking and nothing else —
        // no target list, no raycast, nothing to disagree about.
        _meleeCooldown -= delta;
        if (!uiOwnsInput && !_root.WheelOpen && !_root.UpgradeOpen
            && Input.MouseMode == Input.MouseModeEnum.Captured
            && Input.IsMouseButtonPressed(MouseButton.Right)
            && _meleeCooldown <= 0)
        {
            var melee = Melee.All[_root.CurrentMeleeId()];
            _meleeCooldown = 1.0 / melee.SwingsPerSecond;
            var aim = _camera.GlobalPosition + (-_camera.GlobalTransform.Basis.Z) * melee.ReachMeters;
            _root.Submit(new Command.PlayerMelee(_root.LocalPlayerId,
                new Vec3(aim.X, aim.Y, aim.Z)));
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

        // Both of these are hold-to-open, and this method is what enforces it:
        // release the key and the wheel confirms, the panel closes. A review
        // shot that opened either surface by calling into GameRoot therefore
        // had it taken away again on the very next frame, which is why both
        // presets photographed whatever was underneath. The shot holds the key
        // instead of working around the controller.
        bool buildHeld = HoldingBuild || Input.IsPhysicalKeyPressed(Key.E);
        bool upgradeHeld = HoldingUpgrade || Input.IsPhysicalKeyPressed(Key.U);

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
