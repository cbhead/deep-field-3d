using Godot;
using DeepField.Sim.Content;

namespace DeepField.Game;

/// <summary>Client-authoritative FPS controller (M0: keys polled directly, no
/// input map). Movement never touches the sim; shooting and building enter it
/// as commands via GameRoot.</summary>
public partial class Player : CharacterBody3D
{
    private const float MoveSpeed = 6.5f;
    private const float SprintSpeed = 10f;
    private const float JumpVelocity = 4.8f;
    private const float MouseSensitivity = 0.0022f;

    private Camera3D _camera = null!;
    private GameRoot _root = null!;
    private float _pitch;
    private double _fireCooldown;

    public override void _Ready()
    {
        _root = GetParent<GameRoot>();

        // Own collision: layer 5, collide with world (1).
        CollisionLayer = 1 << 4;
        CollisionMask = 1;
        AddChild(new CollisionShape3D
        {
            Shape = new CapsuleShape3D { Radius = 0.4f, Height = 1.8f },
            Position = new Vector3(0, 0.9f, 0),
        });

        _camera = new Camera3D { Position = new Vector3(0, 1.6f, 0), Fov = 80 };
        AddChild(_camera);

        // Capturing during _Ready is silently ignored on macOS before the window
        // has focus — defer it, and let any click recapture (see _UnhandledInput).
        CallDeferred(nameof(CaptureMouse));
    }

    private void CaptureMouse() => Input.MouseMode = Input.MouseModeEnum.Captured;

    // Mouse look lives in _Input, not _UnhandledInput, so no Control node can
    // ever consume the motion events out from under the camera.
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
        switch (@event)
        {
            case InputEventKey { Pressed: true, Keycode: Key.Escape }:
                Input.MouseMode = Input.MouseMode == Input.MouseModeEnum.Captured
                    ? Input.MouseModeEnum.Visible
                    : Input.MouseModeEnum.Captured;
                break;

            case InputEventKey { Pressed: true, Echo: false, Keycode: Key.E }:
                TryBuild();
                break;

            case InputEventKey { Pressed: true, Echo: false, Keycode: Key.F }:
                _root.EnqueueStartWave();
                break;
        }
    }

    public override void _PhysicsProcess(double delta)
    {
        var velocity = Velocity;

        if (!IsOnFloor())
            velocity.Y -= 9.8f * (float)delta;
        else if (Input.IsPhysicalKeyPressed(Key.Space))
            velocity.Y = JumpVelocity;

        var input = Vector2.Zero;
        if (Input.IsPhysicalKeyPressed(Key.W)) input.Y -= 1;
        if (Input.IsPhysicalKeyPressed(Key.S)) input.Y += 1;
        if (Input.IsPhysicalKeyPressed(Key.A)) input.X -= 1;
        if (Input.IsPhysicalKeyPressed(Key.D)) input.X += 1;

        float speed = Input.IsPhysicalKeyPressed(Key.Shift) ? SprintSpeed : MoveSpeed;
        var direction = (Transform.Basis * new Vector3(input.X, 0, input.Y)).Normalized();
        velocity.X = direction.X * speed;
        velocity.Z = direction.Z * speed;

        Velocity = velocity;
        MoveAndSlide();

        // Fire: held LMB, client-side rate matching the sidearm.
        _fireCooldown -= delta;
        if (Input.MouseMode == Input.MouseModeEnum.Captured
            && Input.IsMouseButtonPressed(MouseButton.Left)
            && _fireCooldown <= 0)
        {
            _fireCooldown = 1.0 / Weapons.Sidearm.ShotsPerSecond;
            Fire();
        }
    }

    private void Fire()
    {
        // Client raycast for instant feel; the sim validates and applies.
        var hit = Raycast(Weapons.Sidearm.RangeMeters, worldMask: 1, areaMask: 1 << 1);
        if (hit is { } result && result.Collider is Area3D area && area.HasMeta("enemy_id"))
            _root.EnqueuePlayerHit(area.GetMeta("enemy_id").AsInt32());
    }

    private void TryBuild()
    {
        var hit = Raycast(8f, worldMask: 1 | (1 << 3), areaMask: 0);
        if (hit is { } result && result.Collider is StaticBody3D body && body.HasMeta("socket_id"))
        {
            string socketId = body.GetMeta("socket_id").AsString();
            if (!_root.SocketOccupied(socketId))
                _root.EnqueuePlaceTower(socketId);
        }
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
