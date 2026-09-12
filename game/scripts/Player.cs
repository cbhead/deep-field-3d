using Godot;
using System.Linq;
using DeepField.Game.Ui;
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

    /// <summary>Cargo lift. Slower than the ladder it competes with (4 m/s), so
    /// the lift buys you a hands-free climb and costs you time — "slow enough
    /// that taking it is a decision", which is what the Spire's own comment
    /// always claimed it was and never was, having no handler at all.</summary>
    private const float LiftSpeed = 3f;

    /// <summary>How far below the rider's origin the car deck tracks. The
    /// capsule's origin sits at its centre, so this is half its height plus a
    /// little clearance.</summary>
    private const float CarDeckDrop = 1.4f;
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
    private Node3D? _gun;
    private Node3D? _hands;
    private ReloadAnimation? _reload;
    private Node3D? _muzzle;
    private string _viewModelFor = "";
    private Vector3 _viewModelRest;
    private float _recoil;

    /// <summary>The gun in your hands, assembled the way the armory assembles
    /// it: design's viewmodel with the modules you fitted on their mounts, and
    /// the faction's hands in the pose that platform is held in.
    ///
    /// Design authors every viewmodel with the grip at the origin and the bore
    /// along -Z, and the hands in the same frame, so the two only have to share
    /// a parent. The parent sits low and right of the camera; the numbers are
    /// per pose because a pistol is held closer than a rifle's handguard.
    /// Hands ship in one pose per file (rifle); pistol and tool poses are
    /// exported beside them as hands_&lt;faction&gt;_&lt;pose&gt;, and a missing
    /// pose falls back to the rifle one rather than to nothing.</summary>
    private void RefreshViewModel(string weaponId, string factionId)
    {
        var fitted = _root.View.Local?.AttachmentsFor(weaponId)
            ?? new System.Collections.Generic.Dictionary<AttachmentSlot, string>();
        string key = $"{weaponId}|{factionId}|" + string.Join(",", fitted.OrderBy(kv => kv.Key).Select(kv => $"{kv.Key}={kv.Value}"));
        if (_viewModelFor == key && GodotObject.IsInstanceValid(_viewModel)) return;
        // A reload in progress holds nodes of the model about to be freed.
        _reload?.Cancel();
        _reload = null;
        if (GodotObject.IsInstanceValid(_viewModel)) _viewModel!.QueueFree();
        _viewModelFor = key;
        _muzzle = null;

        string pose = HandsPose(weaponId);
        _viewModelRest = pose switch
        {
            "pistol" => new Vector3(0.20f, -0.22f, -0.42f),
            "tool" => new Vector3(0.22f, -0.26f, -0.40f),
            _ => new Vector3(0.18f, -0.25f, -0.36f),
        };
        var rig = new Node3D { Position = _viewModelRest };

        var gun = WeaponAssembly.Build(weaponId, fitted, world: false);
        _gun = gun;
        if (gun is not null)
        {
            rig.AddChild(gun);
            // Where the round leaves: the fitted barrel's own muzzle mount if
            // there is one, else the platform's.
            _muzzle = gun.FindChild("attach_mount_muzzle", true, false) as Node3D
                ?? gun.FindChild($"{weaponId.ToLowerInvariant()}_mount_muzzle", true, false) as Node3D;
        }

        // The rifle pose is the base file; only the other poses carry a suffix.
        // Asked for in that order so the audit never sees a name that does
        // not exist as a file.
        string suffix = pose == "rifle" ? "" : $"_{pose}";
        var hands = AssetLibrary.TryInstantiate($"hands_{factionId}{suffix}")
                    ?? AssetLibrary.TryInstantiate($"hands_firstperson{suffix}")
                    ?? AssetLibrary.TryInstantiate($"hands_{factionId}")
                    ?? AssetLibrary.TryInstantiate("hands_firstperson");
        if (hands is not null) rig.AddChild(hands);
        _hands = hands;

        if (gun is null && hands is null) { _viewModel = null; _gun = null; _hands = null; return; }

        // A viewmodel throws no shadow: it would paint the shape of your own
        // gun across the floor in front of you.
        MapKit.NoShadow(rig);
        _viewModel = rig;
        _camera.AddChild(rig);
    }

    /// <summary>Design's weapon.handsPose: how each platform is held.</summary>
    private static string HandsPose(string weaponId) => weaponId switch
    {
        "sidearm" or "emberPistol" => "pistol",
        "wrench" => "tool",
        _ => "rifle",
    };

    /// <summary>The muzzle in world space, for the shot's flash and streak.
    /// Falls back to a point in front of the camera when there is no model.</summary>
    public Vector3 MuzzleWorld => _muzzle is not null && GodotObject.IsInstanceValid(_muzzle)
        ? _muzzle.GlobalPosition
        : _camera.GlobalPosition + (-_camera.GlobalTransform.Basis.Z) * 0.6f + _camera.GlobalTransform.Basis.X * 0.2f - _camera.GlobalTransform.Basis.Y * 0.15f;

    /// <summary>Recoil: the rig kicks back and up a few centimetres and eases
    /// home. Procedural, like every animation in this build.</summary>
    private void TickViewModel(double delta)
    {
        if (_viewModel is null || !GodotObject.IsInstanceValid(_viewModel)) return;
        _recoil = Mathf.MoveToward(_recoil, 0f, (float)delta * 6f);
        _viewModel.Position = _viewModelRest + new Vector3(0f, _recoil * 0.02f, _recoil * 0.05f);
        _viewModel.Rotation = new Vector3(_recoil * 0.06f, 0f, 0f);
        if (_reload is not null)
        {
            _reload.Tick((float)delta);
            if (_reload.Finished) _reload = null;
        }
    }

    /// <summary>The sim started a reload on the weapon in these hands. The
    /// animation is client-only and cosmetic: it plays the sim's duration,
    /// and the sim's <c>Reloaded</c> is what ends it, so a reload that the
    /// sim cut short (death, a swap) never leaves a magazine in the air.</summary>
    public void OnReloadStarted(string weaponId, float seconds)
    {
        if (_viewModel is null || !GodotObject.IsInstanceValid(_viewModel)) return;
        if (!string.Equals(weaponId, _root.CurrentWeaponId(), System.StringComparison.OrdinalIgnoreCase)) return;
        _reload?.Cancel();
        _reload = new ReloadAnimation(_viewModel, _gun, _hands, _root, weaponId, _root.LocalFactionId, seconds);
    }

    public void OnReloaded()
    {
        _reload?.Finish();
        _reload = null;
    }

    /// <summary>For the review shots and the reload probe: play the held
    /// weapon's reload without asking the sim.</summary>
    public void BeginReloadForReview(float seconds) => OnReloadStarted(_root.CurrentWeaponId(), seconds);

    public bool Reloading => _reload is { Finished: false };
    public ReloadAnimation.State? ReloadSnapshot() => _reload?.Snapshot();
    public bool HasWeaponModel => _gun is not null && GodotObject.IsInstanceValid(_gun);

    private bool _onLadder;
    private Vector3? _zipTarget;

    /// <summary>Cargo-lift ride: the Y the car is carrying us to, or null. Rides
    /// are committed — you board and you arrive — because a lift you can step
    /// off mid-shaft is a fall, and the Spire's whole §4.1 lesson is about
    /// climbs that cost a wave.</summary>
    private float? _liftTargetY;
    private Node3D? _liftCar;

    // --- Vehicles ---------------------------------------------------------
    /// <summary>Being in a seat is not remembered here, it is read from the
    /// view every frame — the sim owns seats, so refusals and evictions cost
    /// nothing: if it never seated you, you never sit, and if it throws you
    /// out you are standing on the next frame.</summary>
    private bool _seated;
    private CollisionShape3D _hull = null!;
    private float _seatYaw;
    private const float SeatLookLimit = 2.4f;   // ±137°, so a passenger can shoot behind

    /// <summary>The vehicle the crosshair is on, or empty.</summary>
    private string _aimVehicleId = "";
    private float _boardHold;
    private bool _boardPressed;
    private const float BoardHoldSeconds = 0.35f;

    // --- Teleport network -------------------------------------------------
    /// <summary>A teleport is a decision you have to stand still for. The
    /// charge is short enough not to be a chore and long enough that stepping
    /// onto a pad mid-fight is a bet, and leaving the pad cancels it — which
    /// is what stops it being an escape button.</summary>
    private const float TeleportChargeSeconds = 1.5f;
    /// <summary>Personal, and long: the network is a way to be where the wave
    /// is, not a way to be everywhere. Three hundred and twenty metres is the
    /// point of the map, and a pad you can use every few seconds deletes it.</summary>
    private const float TeleportCooldownSeconds = 20f;

    private float _teleportCharge = -1f;
    private string _teleportFromPad = "";
    private Vector3 _teleportDestination;
    private float _teleportCooldown;

    public bool Teleporting => _teleportCharge >= 0f;
    public float TeleportCooldown => _teleportCooldown;

    /// <summary>The pad under the player's feet, or empty.</summary>
    private string PadHere()
    {
        foreach (var area in _sensor.GetOverlappingAreas())
            if ((string)area.GetMeta("kind", "") == "teleporter")
                return (string)area.GetMeta("pad_id", "");
        return "";
    }

    /// <summary>Start charging toward a pad. Called by the picker on release;
    /// the move itself happens when the charge runs out, on the pad.</summary>
    public void BeginTeleport(string toPadId, Vector3 destination)
    {
        if (_teleportCooldown > 0f) return;
        _teleportFromPad = PadHere();
        if (_teleportFromPad.Length == 0) return;
        _teleportDestination = destination;
        _teleportCharge = TeleportChargeSeconds;
        _root.RefreshPadArt(_teleportFromPad, onCooldown: false);
        GD.Print($"[teleport] {_teleportFromPad} -> {toPadId}");
    }

    private void CancelTeleport()
    {
        if (!Teleporting) return;
        _teleportCharge = -1f;
        _teleportFromPad = "";
        _root.RefreshPadArt("", _teleportCooldown > 0f);
    }

    // What the player is currently looking at (refreshed each physics frame).
    private string _aimSocketId = "";
    private string _aimGateId = "";
    private int _aimEnemyId = -1;

    public override void _Ready()
    {
        _root = GetParent<GameRoot>();

        CollisionLayer = 1 << 4;
        // Vehicles are solid to someone on foot, so a parked one is a thing
        // you walk around rather than through.
        CollisionMask = 1 | (1 << 6);
        _hull = new CollisionShape3D
        {
            Shape = new CapsuleShape3D { Radius = 0.4f, Height = 1.8f },
            Position = new Vector3(0, 0.9f, 0),
        };
        AddChild(_hull);

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
    /// <summary>Set by the traversal probe so a headless run can climb without
    /// a keyboard.</summary>
    public bool ClimbHeld { get; set; }

    /// <summary>World-space direction the probe wants walked, or zero. Applied
    /// as movement input so it goes through the same collision the player
    /// does — the point is to prove a deck can be walked onto, and teleporting
    /// there would prove nothing.</summary>
    public Vector3 WalkHeld { get; set; }

    /// <summary>Standing on something, for the probe's "did they get up there
    /// or are they stuck against the underside" question.</summary>
    public bool Standing => IsOnFloor();

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
                if (_root.RadialOpen) { _root.SteerWheel(motion.Relative); break; }
                if (Input.MouseMode != Input.MouseModeEnum.Captured) break;
                // In a seat the body faces wherever the vehicle does, so
                // looking is an offset from that rather than a heading of your
                // own — clamped, because a rider who can face the boot is a
                // rider whose gun points through the car.
                if (_seated)
                {
                    _seatYaw = Mathf.Clamp(
                        _seatYaw - motion.Relative.X * BaseMouseSensitivity * SensitivityScale,
                        -SeatLookLimit, SeatLookLimit);
                }
                else RotateY(-motion.Relative.X * BaseMouseSensitivity * SensitivityScale);
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
        if (_root.RadialOpen) _root.WheelSelect(oneBased - 1);
        else if (_root.UpgradeOpen) _root.UpgradeKey(oneBased);
    }

    public override void _PhysicsProcess(double delta)
    {
        UpdateAim();
        UpdateBuildSurfaces(delta);

        _teleportCooldown = Mathf.Max(0f, _teleportCooldown - (float)delta);

        // Charging: rooted to the pad, and stepping off it cancels. The
        // position is set rather than travelled to, so the network is not a
        // zipline with a longer cable — it is somewhere else on the map, and
        // the standing still is the whole price.
        if (Teleporting)
        {
            if (PadHere() != _teleportFromPad) CancelTeleport();
            else
            {
                Velocity = Vector3.Zero;
                MoveAndSlide();
                _teleportCharge -= (float)delta;
                if (_teleportCharge <= 0f)
                {
                    _root.Vfx.Teleport(GlobalPosition);
                    GlobalPosition = _teleportDestination + Vector3.Up * 0.3f;
                    _root.Vfx.Teleport(GlobalPosition);
                    _teleportCharge = -1f;
                    _teleportFromPad = "";
                    _teleportCooldown = TeleportCooldownSeconds;
                    _root.RefreshPadArt("", onCooldown: true);
                }
                return;
            }
        }
        else if (_teleportCooldown <= 0f)
        {
            _root.RefreshPadArt("", onCooldown: false);
        }

        // Riding. Derived from the view rather than remembered, so a seat the
        // sim refused or took away needs no handling here at all.
        if (_root.View.SeatOf(_root.LocalPlayerId) is { } seated
            && _root.VehicleNode(seated.Vehicle.Id) is { } ride)
        {
            if (!_seated) TakeSeat(ride);
            var anchor = ride.Seats.Length > 0
                ? ride.Seats[Mathf.Min(seated.Seat, ride.Seats.Length - 1)]
                : ride;
            // Carried, not driven into place: the vehicle's physics is the
            // only physics in play, and a capsule trying to keep up with it
            // would fight every wall the vehicle slides along.
            GlobalPosition = anchor.GlobalPosition;
            Rotation = new Vector3(0f, ride.Rotation.Y + _seatYaw, 0f);
            Velocity = Vector3.Zero;

            // The driver steers; a passenger is free to do the useful thing a
            // passenger can do, which is shoot from a moving vehicle. The
            // vehicle reads the keyboard itself when it is being driven
            // locally, so there is nothing to forward — these are only here to
            // make sure a seat change stops the last driver's input dead.
            if (seated.Seat != 0 || _root.UiCapturesMouse)
            {
                ride.ThrottleHeld = 0f;
                ride.SteerHeld = 0f;
                ride.HandbrakeHeld = false;
            }

            RefreshViewModel(_root.CurrentWeaponId(), _root.LocalFactionId);
            TickViewModel(delta);
            TickWeapons(delta, uiOwnsInput: _root.UiCapturesMouse);
            return;
        }
        if (_seated) LeaveSeat();

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

        // Cargo lift: a vertical ride, slow on purpose. Horizontal input is
        // dead while the car moves, so a lift is a commitment the way the
        // zipline is rather than a faster ladder.
        if (_liftTargetY is { } liftY)
        {
            float gap = liftY - GlobalPosition.Y;
            if (Mathf.Abs(gap) < 0.2f)
            {
                _liftTargetY = null;
                _liftCar = null;
            }
            else
            {
                Velocity = new Vector3(0, Mathf.Sign(gap) * LiftSpeed, 0);
                MoveAndSlide();
                // Keep the deck just under our feet rather than level with
                // them: a static body driven into a CharacterBody3D on the same
                // frame it is moved resolves as a shove, not a floor.
                if (_liftCar is not null)
                    _liftCar.GlobalPosition = _liftCar.GlobalPosition with
                        { Y = GlobalPosition.Y - CarDeckDrop };
                return;
            }
        }

        bool uiOwnsInput = _root.UiCapturesMouse;
        _onLadder = !uiOwnsInput && InArea(AreaKinds.Ladder);

        var velocity = Velocity;

        if (_onLadder)
        {
            float climb = 0f;
            if (Input.IsPhysicalKeyPressed(Key.W) || ClimbHeld) climb = ClimbSpeed;
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
        if (WalkHeld != Vector3.Zero)
        {
            direction = WalkHeld.Normalized();
            input.X = 1;                        // so a walk off a ladder is allowed
        }
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
        TickViewModel(delta);

        TickWeapons(delta, uiOwnsInput);

        if (!uiOwnsInput && Input.IsPhysicalKeyPressed(Key.R))
            TryRevive();
    }


    /// <summary>Trigger, swing and their cooldowns. Extracted because a
    /// passenger in a moving vehicle is not walking but is very much still
    /// shooting — that is the entire point of the second seat.</summary>
    private void TickWeapons(double delta, bool uiOwnsInput)
    {
        // Fire: blocked while a menu owns the mouse or a build surface is open.
        _fireCooldown -= delta;
        if (!uiOwnsInput && !_root.RadialOpen && !_root.UpgradeOpen && !Teleporting
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
        if (!uiOwnsInput && !_root.RadialOpen && !_root.UpgradeOpen
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
    }

    // =====================================================================
    // Aim + contextual surfaces
    // =====================================================================

    private void UpdateAim()
    {
        _aimSocketId = "";
        _aimGateId = "";
        _aimEnemyId = -1;

        // Enemies first (they're what you shoot), then sockets (what you build on).
        if (Raycast(80f, worldMask: 1, areaMask: 1 << 1) is { } shot
            && shot.Collider is Area3D area && area.HasMeta("enemy_id"))
            _aimEnemyId = area.GetMeta("enemy_id").AsInt32();

        if (Raycast(InteractRange, worldMask: 1 | (1 << 3), areaMask: 0) is { } reach
            && reach.Collider is StaticBody3D body)
        {
            if (body.HasMeta("socket_id")) _aimSocketId = body.GetMeta("socket_id").AsString();
            // Levers share the socket layer and the same reach, so what the
            // prompt offers is what the sim will accept.
            if (body.HasMeta("gate_id")) _aimGateId = body.GetMeta("gate_id").AsString();
        }

        _aimVehicleId = "";
        if (Raycast(InteractRange, worldMask: 1 | (1 << 6), areaMask: 0) is { } near
            && near.Collider is Node vehicle && vehicle.HasMeta("vehicle_id"))
            _aimVehicleId = vehicle.GetMeta("vehicle_id").AsString();

        _root.ReportAim(_aimEnemyId);
        _root.SetHint(BuildHint());
    }

    private string BuildHint()
    {
        if (_root.PickerOpen) return "steer to a pad · release E to go · 1-6 to pick";
        if (_root.WheelOpen) return "steer to a wedge · release E to build · 1-6 to pick";
        if (_root.UpgradeOpen) return "1-3 upgrade a path · hold X to sell · release U to close";
        if (_seated)
        {
            var seat = _root.View.SeatOf(_root.LocalPlayerId);
            return seat is { Seat: 0 }
                ? "[E] get out · WASD drive · Space handbrake"
                : "[E] get out · you can still shoot from here";
        }
        if (_aimVehicleId.Length > 0 && _root.VehicleOffer(_aimVehicleId) is { } offer)
        {
            string what = offer.Label.ToLowerInvariant();
            if (offer.Seat < 0) return $"the {what} is full";
            return offer.Seat == 0
                ? $"[E] drive the {what}   ·   [hold E] ride along"
                : $"[E] ride in the {what}";
        }
        if (Teleporting) return $"teleporting… {_teleportCharge:0.0}s · step off to cancel";
        if (PadHere().Length > 0)
            return _teleportCooldown > 0f
                ? $"teleporter recharging · {_teleportCooldown:0}s"
                : "[hold E] choose a destination";
        if (_aimGateId.Length > 0) return _root.GateHint(_aimGateId);
        if (InArea(AreaKinds.Armory)) return "[Tab] armory   ·   [5] recraft blueprint";
        if (InArea(AreaKinds.Zipline)) return "[E] ride the zipline";
        if (InArea(AreaKinds.Elevator)) return LiftHint();
        if (InArea(AreaKinds.Ladder)) return "[W] climb";
        if (InArea(AreaKinds.ControlPoint)) return "control point";

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
            if (_root.PickerOpen) _root.CancelTeleportPicker();
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

        // In a seat, E is the way out and nothing else. Edge-triggered: held
        // through a whole frame it would board and unboard forever.
        if (_seated)
        {
            if (buildHeld && !_boardPressed)
                _root.Submit(new Command.ExitVehicle(_root.LocalPlayerId));
            _boardPressed = buildHeld;
            return;
        }

        // Looking at a vehicle: tap E for the best free seat, hold it for the
        // passenger side. Two players walking up to one buggy both want the
        // wheel, and the sim refuses the second — so the hold exists to let
        // the second one say what they actually meant.
        if (_aimVehicleId.Length > 0 && _root.VehicleOffer(_aimVehicleId) is { } offer)
        {
            if (buildHeld) _boardHold += (float)delta;
            bool released = !buildHeld && _boardPressed;
            _boardPressed = buildHeld;
            if (released && offer.Seat >= 0)
            {
                // Held long enough, and there is a passenger seat free: ride.
                int seat = _boardHold >= BoardHoldSeconds && offer.Seat == 0
                    && _root.View.VehicleById(offer.Id) is { } v && v.Seats.Length > 1 && v.SeatFree(1)
                    ? 1 : offer.Seat;
                _root.Submit(new Command.EnterVehicle(_root.LocalPlayerId, offer.Id, seat));
            }
            if (!buildHeld) _boardHold = 0f;
            if (buildHeld || released) return;
        }
        else
        {
            _boardPressed = buildHeld;
            _boardHold = 0f;
        }

        // A pad under your feet outranks everything: you cannot build on one,
        // and a player holding E while standing on a teleporter means the
        // teleporter. Release commits, the same as the other two surfaces.
        if (buildHeld && !_root.RadialOpen && !Teleporting
            && _teleportCooldown <= 0f && PadHere().Length > 0)
        {
            _root.OpenTeleportPicker(PadHere());
            return;
        }
        if (!buildHeld && _root.PickerOpen)
        {
            _root.ConfirmTeleportPicker();
            return;
        }

        // E in a zipline volume rides instead of building — traversal wins,
        // since you can't build on a zipline anyway.
        if (buildHeld && !_root.WheelOpen && InArea(AreaKinds.Zipline))
        {
            foreach (var area in _sensor.GetOverlappingAreas())
                if ((string)area.GetMeta("kind", "") == AreaKinds.Zipline)
                    _zipTarget = (Vector3)area.GetMeta("zip_end");
            return;
        }

        // E at a lever flips it. Ahead of the build wheel because a lever is
        // not on a socket, so the two can never both be offered.
        if (buildHeld && !_boardPressed && !_root.WheelOpen && _aimGateId.Length > 0)
        {
            _boardPressed = true;
            _root.Submit(new Command.OperateGate(_root.LocalPlayerId, _aimGateId));
            return;
        }

        // E in a lift car sends it to whichever stop you are not at. Same
        // precedence as the zipline: traversal beats building, and there is
        // nothing to build on a lift.
        // `_liftTargetY is null` is load-bearing, not a guard against double
        // work: this method runs *before* the ride each frame, so re-deciding
        // mid-shaft would pick "the stop I am furthest from" against a Y that
        // is now halfway, and the car would turn round at the midpoint and
        // oscillate there forever.
        if (buildHeld && !_root.WheelOpen && _liftTargetY is null && InArea(AreaKinds.Elevator))
        {
            foreach (var area in _sensor.GetOverlappingAreas())
            {
                if ((string)area.GetMeta("kind", "") != AreaKinds.Elevator) continue;
                float bottom = (float)area.GetMeta(AreaKinds.LiftBottomMeta, 0f);
                float top = (float)area.GetMeta(AreaKinds.LiftTopMeta, 0f);
                // A lift whose stops are the same height serves nothing. That
                // is authored, not broken — see AreaKinds.LiftTopMeta — so it
                // refuses the ride rather than pretending to move.
                if (Mathf.Abs(top - bottom) < 0.5f) continue;
                _liftTargetY = Mathf.Abs(GlobalPosition.Y - bottom)
                    < Mathf.Abs(GlobalPosition.Y - top) ? top : bottom;
                // The car rides with you. Its own body is the thing the shaft
                // art hangs off, so leaving it parked would put you on a lift
                // you can watch yourself leave behind.
                _liftCar = area.GetParent() as Node3D;
            }
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

    /// <summary>Get in: the capsule stops colliding with anything (the
    /// vehicle is what collides now) and the eye drops to the seat's height,
    /// which on the trike is most of what makes it a trike.</summary>
    private void TakeSeat(Vehicle ride)
    {
        _seated = true;
        _lastRide = ride;
        _seatYaw = 0f;
        _hull.Disabled = true;
        _camera.Position = new Vector3(0, ride.Def.SeatedEyeHeight, 0);
        _zipTarget = null;
        CancelTeleport();
    }

    /// <summary>Get out, beside it rather than inside it: a player left on the
    /// vehicle's own origin is a player standing in its collision box.</summary>
    private void LeaveSeat()
    {
        _seated = false;
        _hull.Disabled = false;
        _camera.Position = new Vector3(0, 1.6f, 0);
        Rotation = new Vector3(0f, Rotation.Y + _seatYaw, 0f);
        _seatYaw = 0f;
        if (_lastRide is { } ride && GodotObject.IsInstanceValid(ride))
        {
            float side = ride.Def.BodySize.X * 0.5f + 0.9f;
            GlobalPosition = ride.GlobalPosition
                + ride.GlobalTransform.Basis.X * side + Vector3.Up * 0.4f;
        }
        _lastRide = null;
    }

    private Vehicle? _lastRide;

    /// <summary>A lift that goes nowhere says so, rather than offering a ride
    /// that does nothing. On the Spire that is every lift — see
    /// <see cref="AreaKinds.LiftTopMeta"/>.</summary>
    private string LiftHint()
    {
        foreach (var area in _sensor.GetOverlappingAreas())
        {
            if ((string)area.GetMeta("kind", "") != AreaKinds.Elevator) continue;
            float bottom = (float)area.GetMeta(AreaKinds.LiftBottomMeta, 0f);
            float top = (float)area.GetMeta(AreaKinds.LiftTopMeta, 0f);
            if (Mathf.Abs(top - bottom) >= 0.5f) return "[E] ride the lift";
        }
        return "lift — out of service";
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

        // The shot is drawn now, from the muzzle to wherever it stopped; the
        // sim says separately whether it hurt. Instant feel, nothing waits.
        var end = hit?.Position ?? _camera.GlobalPosition + (-_camera.GlobalTransform.Basis.Z) * weapon.RangeMeters;
        string ammo = _root.View.Local?.AmmoFor(weapon.Id) ?? "standard";
        _root.Vfx.GunShot(MuzzleWorld, end, ammo, hit is not null);
        _recoil = 1f;
    }

    /// <summary>For the review shots: one round down the lane so the frame
    /// has a flash and a streak in it.</summary>
    public void FireForReview() => Fire(Weapons.All[_root.CurrentWeaponId()]);

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
