using Godot;
using System.Collections.Generic;
using System.Linq;

namespace DeepField.Game;

/// <summary>A drivable vehicle: a kinematic body with an arcade handling model.
///
/// Not a VehicleBody3D, and the reasons are this codebase's rather than
/// taste. Motion has to be client-authoritative and streamed as a position and
/// a yaw, which is what a kinematic body already is — a rigid body would have
/// to be frozen and interpolated on every peer but the driver's, so two
/// mechanisms would exist where one is needed. Every probe in this repo drives
/// the controller through held-input hooks, and a kinematic model answers them
/// frame for frame. And the one thing a physics vehicle is good at, suspension
/// over relief, buys nothing on a ground plane that is flat by construction.
///
/// The model is four numbers doing the work: a throttle that moves a scalar
/// speed, a steering lever that lags the stick, a yaw rate that needs forward
/// roll (unless the thing steers on levers), and a grip that pulls the velocity
/// onto the heading. Drift is not a special case — it is what low grip does.</summary>
public partial class Vehicle : CharacterBody3D
{
    public string Id = "";
    public VehicleHandling Def = null!;

    /// <summary>Seat anchors, index 0 the driver. From the model's named nodes
    /// when it has them, from the hull when it does not.</summary>
    public Node3D[] Seats = System.Array.Empty<Node3D>();

    /// <summary>The local player is in seat 0, so this client integrates it.
    /// Everyone else follows what the driver last said.</summary>
    public bool LocallyDriven;
    public Vector3 NetPosition;
    public float NetYaw;

    // Probe hooks, the same idea as Player.WalkHeld: a headless run has no
    // keyboard, and a probe that moved the vehicle by setting its position
    // would prove nothing about whether it can be driven.
    public float ThrottleHeld;
    public float SteerHeld;
    public bool HandbrakeHeld;

    private float _speed;
    private float _steer;
    private readonly List<(Node3D Node, bool Steers)> _wheels = new();

    public float Speed => _speed;
    public Surface SurfaceUnder { get; private set; } = Surface.Grass;

    public override void _Ready()
    {
        // Its own layer, so a raycast can find it to board and the player
        // capsule bumps into it when it is parked. Masks the world and other
        // vehicles; never the player, who is either inside it or beside it.
        CollisionLayer = 1 << 6;
        CollisionMask = 1 | (1 << 6);
        SetMeta("vehicle_id", Id);
        AddChild(new CollisionShape3D
        {
            Shape = new BoxShape3D { Size = Def.BodySize },
            Position = new Vector3(0, Def.BodySize.Y * 0.5f, 0),
        });

        // Vehicles are authored nose along -Z, the integration contract's
        // forward, which is also the basis vector this controller drives on.
        // Every other kit piece with a front faces +Z and is turned by a yaw
        // helper; a vehicle is not, because "forward" here is a direction of
        // travel rather than a thing to point at something.
        var art = AssetLibrary.Instantiate($"vehicle_{Def.Id}", () => Placeholders.Vehicle(Def));
        art.Name = "Art";
        AddChild(art);
        ResolveSeats(art);
        ResolveWheels(art);
    }

    /// <summary>Design authors a seat as an empty at the cushion. Without one,
    /// the hull says where to sit — a driver slightly off-centre and a
    /// passenger on the other side — so a graybox is drivable on day one and a
    /// delivered model is drivable better.</summary>
    private void ResolveSeats(Node3D art)
    {
        var seats = new List<Node3D>();
        foreach (string name in new[] { "seat_driver", "seat_passenger" })
        {
            if (art.FindChild(name, true, false) is Node3D seat) seats.Add(seat);
            else break;
        }

        if (seats.Count == 0)
        {
            int count = DeepField.Sim.Content.Vehicles.All[Def.Id].Seats;
            for (int i = 0; i < count; i++)
            {
                float side = count > 1 ? (i == 0 ? -0.35f : 0.35f) : 0f;
                var fallback = new Node3D
                {
                    Name = i == 0 ? "SeatDriver" : "SeatPassenger",
                    Position = new Vector3(side, Def.BodySize.Y * 0.45f, 0.1f),
                };
                AddChild(fallback);
                seats.Add(fallback);
            }
        }
        Seats = seats.ToArray();
    }

    /// <summary>Wheels spin from the speed and the front pair turns with the
    /// lever. Both are cosmetic: nothing here is load-bearing, so a model
    /// without named wheels simply has still ones.</summary>
    private void ResolveWheels(Node3D art)
    {
        foreach (string name in new[] { "wheel_fl", "wheel_fr", "wheel_rl", "wheel_rr",
                                        "wheel_0", "wheel_1", "wheel_2" })
        {
            if (art.FindChild(name, true, false) is not Node3D wheel) continue;
            bool steers = name is "wheel_fl" or "wheel_fr" or "wheel_0";
            _wheels.Add((wheel, steers));
        }
    }

    public override void _PhysicsProcess(double delta)
    {
        if (LocallyDriven) Drive((float)delta);
        else Follow((float)delta);
        SpinWheels((float)delta);
    }

    private void Drive(float delta)
    {
        float throttle = ThrottleHeld != 0f ? ThrottleHeld
            : Input.IsPhysicalKeyPressed(Key.W) ? 1f
            : Input.IsPhysicalKeyPressed(Key.S) ? -1f : 0f;
        float steerInput = SteerHeld != 0f ? SteerHeld
            : (Input.IsPhysicalKeyPressed(Key.A) ? 1f : 0f)
              - (Input.IsPhysicalKeyPressed(Key.D) ? 1f : 0f);
        bool handbrake = HandbrakeHeld || Input.IsPhysicalKeyPressed(Key.Space);

        SurfaceUnder = Surfaces.At(GlobalPosition);
        var ground = Def.Surfaces[SurfaceUnder];
        float top = Def.TopSpeed * ground.TopSpeed;

        // The steering lever chases the stick at its own rate. A low rate is
        // what "answers slowly" means, and it is felt long before the turn
        // radius is.
        _steer = Mathf.MoveToward(_steer, steerInput, Def.SteerResponse * delta);

        // S brakes a forward roll and reverses once stopped, which is one key
        // doing the thing a driver expects rather than two doing it correctly.
        if (handbrake) _speed = Mathf.MoveToward(_speed, 0f, Def.Brake * 0.8f * delta);
        else if (throttle > 0f) _speed = Mathf.MoveToward(_speed, top, Def.Accel * ground.Accel * delta);
        else if (throttle < 0f)
            _speed = _speed > 0.3f
                ? Mathf.MoveToward(_speed, 0f, Def.Brake * delta)
                : Mathf.MoveToward(_speed, -Def.ReverseSpeed * ground.TopSpeed,
                    Def.Accel * 0.5f * ground.Accel * delta);
        else _speed = Mathf.MoveToward(_speed, 0f, Def.Drag * delta);

        float fade = 1f - Def.HighSpeedSteerFade
            * Mathf.Clamp(Mathf.Abs(_speed) / Mathf.Max(top, 0.01f), 0f, 1f);
        float yawRate;
        if (Def.Differential)
        {
            // Levers drive the wheels, so it turns whether or not it is going
            // anywhere, and pulling the same lever means the same direction in
            // reverse. That is the trike, and it is why it can spin on a drive.
            yawRate = _steer * Def.MaxYawRate * fade;
        }
        else
        {
            // A steered wheel only turns a car that is rolling, and the faster
            // it rolls the less it turns.
            float rolling = Mathf.Clamp(Mathf.Abs(_speed) / Def.SteerFullSpeed, 0f, 1f);
            yawRate = _steer * Def.MaxYawRate * rolling * fade * Mathf.Sign(_speed == 0f ? 1f : _speed);
        }
        RotateY(yawRate * delta);

        // Grip: how fast the velocity comes round to the heading. High is
        // planted; low and the car keeps going the way it was pointed a moment
        // ago, which is a drift and needs no separate mechanism.
        var forward = -GlobalTransform.Basis.Z;
        var planar = new Vector3(Velocity.X, 0f, Velocity.Z);
        float grip = Def.Grip * ground.Grip * (handbrake ? Def.HandbrakeGrip : 1f);
        planar = planar.Lerp(forward * _speed, 1f - Mathf.Exp(-grip * delta));

        float fall = IsOnFloor() ? -1f : Velocity.Y - 9.8f * delta;
        Velocity = new Vector3(planar.X, fall, planar.Z);
        MoveAndSlide();

        // A wall is a fact. Without this the model keeps believing it is doing
        // twenty into a tree and the wheels keep spinning.
        //
        // A wall, specifically. Asking whether anything was hit catches the
        // ground every single frame — the body is standing on it — and each
        // frame's acceleration was being thrown away and replaced by whatever
        // survived one tick of grip, which converges on nought. It read as a
        // vehicle with a working throttle that does not move.
        if (IsOnWall())
            _speed = Mathf.Clamp(Velocity.Dot(forward), -Def.ReverseSpeed, top);
    }

    private void Follow(float delta)
    {
        // Somebody else is driving. Ease toward where they say it is, and snap
        // when that is far enough away to be a respawn rather than a corner.
        if (GlobalPosition.DistanceTo(NetPosition) > 20f)
        {
            GlobalPosition = NetPosition;
            Rotation = new Vector3(0, NetYaw, 0);
            _speed = 0f;
            return;
        }
        float k = 1f - Mathf.Exp(-12f * delta);
        var was = GlobalPosition;
        GlobalPosition = GlobalPosition.Lerp(NetPosition, k);
        Rotation = new Vector3(0f, Mathf.LerpAngle(Rotation.Y, NetYaw, k), 0f);
        // Wheels spin off actual displacement here, so a vehicle being watched
        // does not roll along with locked wheels.
        _speed = delta > 0f ? was.DistanceTo(GlobalPosition) / delta : 0f;
    }

    private void SpinWheels(float delta)
    {
        if (_wheels.Count == 0) return;
        float omega = _speed / Mathf.Max(Def.WheelRadius, 0.05f);
        foreach (var (wheel, steers) in _wheels)
        {
            wheel.RotateX(-omega * delta);
            if (steers && !Def.Differential)
                wheel.Rotation = wheel.Rotation with { Y = _steer * 0.55f };
        }
    }
}
