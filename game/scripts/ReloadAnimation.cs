using Godot;
using System.Linq;
using DeepField.Sim.Content;

namespace DeepField.Game;

/// <summary>The reload, played on the first-person viewmodel from the parts
/// design delivered for it (docs/FORWARD-MANIFEST-reload.md §8).
///
/// Nothing here is a clip or a skin. Every motion is a transform on a named
/// node or a swap of a whole model, which is the constraint that document
/// set and the reason the drop could ship as static parts with correct
/// pivots: the magazine's pivot sits on its seated face, so dropping it is a
/// translation and a tip rather than a swing about the weapon origin; the
/// action carries its own travel in <c>extras.recoil</c>; the off-hand poses
/// are one posed left arm each, rooted where the platform's own <c>hand_l</c>
/// is, and carry a <c>hands_mount_magazine</c> node that says where the fresh
/// magazine rides.
///
/// The poses were measured against the Sidearm. On every other platform the
/// pose is placed so that its own magazine mount lands on that weapon's feed
/// point (<c>&lt;w&gt;_mount_magwell</c>, or the Scattergun's loading port), so
/// one set of three files serves the armoury without a constant per weapon.
///
/// Missing-node policy: each step tests the node it needs and skips. A
/// platform with no viewmodel plays nothing; a faction whose poses have not
/// been delivered still drops and reseats the magazine and cycles the
/// action; a magazine prop that has not landed still gets the hands.</summary>
public sealed partial class ReloadAnimation
{
    private readonly Node3D _rig;
    private readonly Node3D? _gun;
    private readonly Node3D? _hands;
    private readonly Node _world;            // where a dropped magazine falls (world space, real gravity)
    private readonly string _id;             // lowercase: node and file prefix
    private readonly string _faction;
    private readonly float _seconds;
    private readonly bool _tubeFed;
    private readonly int _shells;

    private readonly Node3D? _magazine;
    private readonly Node3D? _feed;
    private readonly Node3D? _release;
    private readonly Node3D? _action;
    private readonly Node3D? _handL;
    private readonly Vector3 _releaseRest;
    private readonly Vector3 _actionRest;
    private readonly Vector3 _actionAxis;
    private readonly float _actionTravel;
    private readonly Vector3 _dropAxis;
    private readonly Vector3 _dropTip;
    private readonly float _dropClear;

    private float _t;
    private bool _done;
    private Node3D? _pose;                   // the off-hand currently shown
    private string _poseName = "";
    private FallingProp? _spent;             // the magazine on its way to the floor
    private bool _dropped;                   // the drop is one beat, not a state
    private Node3D? _fresh;                  // the magazine (or shell) in the off-hand
    private Vector3 _poseRest;
    private int _shellsFed = -1;

    public bool Finished => _done;

    /// <summary>What the animation is doing right now, for the probe: which
    /// pose is up, whether the magazine and the platform's own left hand are
    /// showing, and whether the props exist. Null flags mean the model has no
    /// such node, which the probe treats as "nothing to check" rather than
    /// as a failure.</summary>
    public readonly record struct State(string Pose, bool? MagazineVisible, bool? HandVisible,
        bool FreshProp, bool SpentProp, float ActionOffset, bool TubeFed);

    public State Snapshot() => new(
        _pose is not null && Valid(_pose) ? _poseName : "",
        _magazine is not null && Valid(_magazine) ? _magazine.Visible : null,
        _handL is not null && Valid(_handL) ? _handL.Visible : null,
        _fresh is not null && Valid(_fresh),
        _spent is not null && Valid(_spent),
        _action is not null && Valid(_action) ? (_action.Position - _actionRest).Length() : 0f,
        _tubeFed);

    public ReloadAnimation(Node3D rig, Node3D? gun, Node3D? hands, Node world,
        string weaponId, string factionId, float seconds)
    {
        _rig = rig; _gun = gun; _hands = hands; _world = world;
        _id = weaponId.ToLowerInvariant();
        _faction = factionId;
        _seconds = Mathf.Max(0.3f, seconds);

        if (gun is not null)
        {
            _magazine = Find(gun, $"{_id}_magazine");
            _feed = Find(gun, $"{_id}_mount_magwell")
                ?? Find(gun, "scattergun_mount_loadport")
                ?? Find(gun, $"{_id}_mount_magazine");
            _release = Find(gun, $"{_id}_mag_release");
            _action = FindWithExtra(gun, "recoil");
            if (_release is not null) _releaseRest = _release.Position;
            if (_action is not null)
            {
                _actionRest = _action.Position;
                var recoil = Extra(_action, "recoil");
                _actionAxis = recoil is not null && recoil.TryGetValue("axis", out var axis) ? ToVector(axis, Vector3.Back) : Vector3.Back;
                _actionTravel = recoil is not null && recoil.TryGetValue("travel", out var travel) ? (float)travel.AsDouble() : 0.02f;
            }
        }
        _handL = hands is not null ? Find(hands, "hand_l") : null;

        // Drop direction and the clearance before the tip, from the magazine's
        // own extras; the Sidearm's numbers stand in when a file has none.
        _dropAxis = Vector3.Down; _dropTip = Vector3.Right; _dropClear = 0.015f;
        if (_magazine is not null && Extra(_magazine, "drop") is { } drop)
        {
            if (drop.TryGetValue("axis", out var a)) _dropAxis = ToVector(a, Vector3.Down);
            if (drop.TryGetValue("tip", out var tip)) _dropTip = ToVector(tip, Vector3.Right);
            if (drop.TryGetValue("clear", out var clear)) _dropClear = (float)clear.AsDouble();
        }

        _tubeFed = _magazine is null && _feed is not null && _feed.Name == "scattergun_mount_loadport";
        _shells = Weapons.All.TryGetValue(weaponId, out var def) ? System.Math.Max(1, def.MagazineSize) : 6;
    }

    // =====================================================================
    // Timeline
    // =====================================================================

    /// <summary>Advance by a frame. Fractions of the reload's own length, so a
    /// 1.3 s pistol and a 2.4 s scattergun play the same beats at their own
    /// pace — <c>ReloadSeconds</c> is per weapon and the sim's clock is the
    /// only one that counts.</summary>
    public void Tick(float delta)
    {
        if (_done) return;
        if (!GodotObject.IsInstanceValid(_rig)) { _done = true; return; }
        _t += delta;
        float u = _t / _seconds;
        if (u >= 1f) { Finish(); return; }

        if (_tubeFed) TickTubeFed(u);
        else TickMagazineFed(u);
    }

    private void TickMagazineFed(float u)
    {
        // The release is pressed toward the bore and lets go — a few
        // millimetres, which is what a thumb does.
        if (_release is not null && Valid(_release))
        {
            float k = Mathf.Clamp(u / 0.10f, 0f, 1f);
            var toward = new Vector3(-Mathf.Sign(_releaseRest.X == 0f ? -1f : _releaseRest.X), 0f, 0f);
            _release.Position = _releaseRest + toward * 0.002f * Mathf.Sin(k * Mathf.Pi);
        }

        if (u >= 0.05f && u < 0.35f) ShowPose("magout", withFresh: false);
        // One drop per reload. This used to be gated on the magazine being
        // visible, and from 72% the step below shows it again — so every tick
        // of the last quarter hid it, dropped another, and showed it, and a
        // player walking through a reload left a trail of magazines that
        // nothing ever freed.
        if (u >= 0.10f && !_dropped)
        {
            _dropped = true;
            if (_magazine is not null && Valid(_magazine)) _magazine.Visible = false;
            DropSpent();
        }
        if (u >= 0.35f && u < 0.72f)
        {
            ShowPose("magin", withFresh: true);
            // The fresh magazine comes up from low and behind, and eases home.
            if (_pose is not null && Valid(_pose))
            {
                float k = Mathf.Clamp((u - 0.35f) / 0.35f, 0f, 1f);
                float eased = 1f - (1f - k) * (1f - k);
                _pose.Position = _poseRest + new Vector3(0f, -0.08f, 0.04f) * (1f - eased);
            }
        }
        if (u >= 0.72f)
        {
            if (_magazine is not null && Valid(_magazine)) _magazine.Visible = true;
            FreeFresh();
            // Only a rifle racks after a change; a pistol's slide was locked
            // back on the empty magazine and drops on its own.
            if (_id == "rifle" && _action is not null && Valid(_action))
            {
                ShowPose("charge", withFresh: false, atRigOrigin: true);
                CycleAction(u, 0.75f, 0.87f, 1.0f);
            }
            else HidePose();
        }
    }

    private void TickTubeFed(float u)
    {
        // Shells through the loading port, one at a time: the off-hand holds
        // at the port and each shell slides in over the tail of its window.
        if (u >= 0.05f && u < 0.85f)
        {
            ShowPose("magin", withFresh: true);
            float w = 0.80f / _shells;
            int k = Mathf.Clamp((int)((u - 0.05f) / w), 0, _shells - 1);
            if (k != _shellsFed) { _shellsFed = k; if (_fresh is not null && Valid(_fresh)) _fresh.Position = Vector3.Zero; }
            float local = ((u - 0.05f) - k * w) / w;
            if (_fresh is not null && Valid(_fresh))
                _fresh.Position = new Vector3(0f, 0f, 0.06f * Mathf.Clamp((local - 0.6f) / 0.4f, 0f, 1f));
        }
        if (u >= 0.85f)
        {
            FreeFresh();
            if (_action is not null && Valid(_action))
            {
                ShowPose("charge", withFresh: false, atRigOrigin: true);
                CycleAction(u, 0.85f, 0.93f, 1.0f);
            }
            else HidePose();
        }
    }

    /// <summary>The action travels along its own recoil axis and returns —
    /// the same node and the same numbers a shot would move.</summary>
    private void CycleAction(float u, float from, float peak, float to)
    {
        if (_action is null || !Valid(_action)) return;
        float k = u < peak
            ? Mathf.Clamp((u - from) / (peak - from), 0f, 1f)
            : 1f - Mathf.Clamp((u - peak) / (to - peak), 0f, 1f);
        _action.Position = _actionRest + _actionAxis * _actionTravel * k;
    }

    // =====================================================================
    // Parts
    // =====================================================================

    /// <summary>Swap in one of the off-hand poses. The pose replaces the
    /// platform's own left hand, so that is hidden for as long as a pose is
    /// showing; and it is placed so that its magazine mount lands on this
    /// weapon's feed point, which is what makes a Sidearm-measured pose sit
    /// right on a rifle's magwell or the scattergun's loading port.</summary>
    private void ShowPose(string name, bool withFresh, bool atRigOrigin = false)
    {
        if (_poseName == name && _pose is not null && Valid(_pose)) return;
        HidePose();
        var pose = AssetLibrary.TryInstantiate($"hands_{_faction}_{name}")
                   ?? AssetLibrary.TryInstantiate($"hands_firstperson_{name}");
        if (pose is null) return;
        MapKit.NoShadow(pose);
        _rig.AddChild(pose);
        _pose = pose; _poseName = name;
        if (_handL is not null && Valid(_handL)) _handL.Visible = false;

        var mount = Find(pose, "hands_mount_magazine");
        if (!atRigOrigin && _feed is not null && Valid(_feed))
        {
            var feedLocal = _rig.ToLocal(_feed.GlobalPosition);
            var mountLocal = mount is not null ? _rig.ToLocal(mount.GlobalPosition) : Vector3.Zero;
            pose.Position = feedLocal - mountLocal;
        }
        _poseRest = pose.Position;

        if (withFresh && mount is not null)
        {
            var fresh = AssetLibrary.TryInstantiate(_tubeFed ? "weapon_scattergun_shell" : $"weapon_{_id}_magazine");
            if (fresh is not null) { MapKit.NoShadow(fresh); mount.AddChild(fresh); _fresh = fresh; }
        }
    }

    private void HidePose()
    {
        FreeFresh();
        if (_pose is not null && Valid(_pose)) { _pose.QueueFree(); }
        _pose = null; _poseName = "";
        if (_handL is not null && Valid(_handL)) _handL.Visible = true;
    }

    private void FreeFresh()
    {
        if (_fresh is not null && Valid(_fresh)) _fresh.QueueFree();
        _fresh = null;
    }

    /// <summary>The spent magazine leaves the gun in the world, not on the
    /// camera: it is spawned at the feed point's world transform and falls
    /// under real gravity, so it drops toward the floor whatever the player is
    /// looking at. Along its drop axis first, then a tip once it is clear of
    /// the well — the pivot on the seated face is what makes that a tip and
    /// not a swing through the frame.</summary>
    private void DropSpent()
    {
        var prop = AssetLibrary.TryInstantiate(_tubeFed ? "weapon_scattergun_shell" : $"weapon_{_id}_magazine");
        if (prop is null) return;
        MapKit.NoShadow(prop);
        var at = _feed is not null && Valid(_feed) ? _feed.GlobalTransform
            : (_magazine is not null && Valid(_magazine) ? _magazine.GlobalTransform : _rig.GlobalTransform);
        var falling = new FallingProp
        {
            Velocity = at.Basis * _dropAxis * 0.6f,
            TipAxis = _dropTip,
            Clear = _dropClear,
        };
        _world.AddChild(falling);
        falling.GlobalTransform = at;
        falling.AddChild(prop);
        _spent = falling;
    }

    /// <summary>A dropped magazine is its own node with its own clock: it falls
    /// under real gravity, tips once it is clear of the well, and frees itself
    /// after a metre and a half or a second and a quarter, whichever first —
    /// whatever the animation that dropped it is doing by then, including
    /// being gone. Nothing about its life depends on anyone remembering it.</summary>
    private sealed partial class FallingProp : Node3D
    {
        public Vector3 Velocity;
        public Vector3 TipAxis = Vector3.Right;
        public float Clear = 0.015f;
        private float _travelled;
        private float _age;

        public override void _PhysicsProcess(double delta)
        {
            float dt = (float)delta;
            _age += dt;
            Velocity += Vector3.Down * 9.8f * dt;
            var step = Velocity * dt;
            GlobalPosition += step;
            _travelled += step.Length();
            if (_travelled > Clear)
            {
                float tip = Mathf.Clamp((_travelled - Clear) / 0.12f, 0f, 1f) * Mathf.DegToRad(30f);
                Rotate(GlobalTransform.Basis * TipAxis, tip * dt * 8f);
            }
            if (_travelled > 1.5f || _age > 1.25f) QueueFree();
        }
    }

    // =====================================================================
    // Lifecycle
    // =====================================================================

    /// <summary>The sim says the magazine is full, or the clock ran out:
    /// everything back where it was.</summary>
    public void Finish()
    {
        if (_done) return;
        _done = true;
        HidePose();
        if (_magazine is not null && Valid(_magazine)) _magazine.Visible = true;
        if (_release is not null && Valid(_release)) _release.Position = _releaseRest;
        if (_action is not null && Valid(_action)) _action.Position = _actionRest;
        // A magazine still in the air keeps falling; it frees itself.
    }

    /// <summary>Weapon switched, model rebuilt, or the player died: nothing
    /// may be left holding a node that is about to be freed.</summary>
    public void Cancel()
    {
        Finish();
        if (_spent is not null && Valid(_spent)) _spent.QueueFree();
        _spent = null;
    }

    /// <summary>For the probe: how many dropped magazines are still in the
    /// world under <paramref name="world"/>. After a reload has ended and the
    /// prop's own clock has run, the answer is zero or the trail is back.</summary>
    public static int StrayProps(Node world)
        => world.GetChildren().Count(c => c is FallingProp);

    // =====================================================================
    // Helpers
    // =====================================================================

    private static bool Valid(GodotObject o) => GodotObject.IsInstanceValid(o);

    private static Node3D? Find(Node root, string name) => root.FindChild(name, true, false) as Node3D;

    /// <summary>Design authors a moving part's contract in its userData, which
    /// arrives as glTF extras and lands on the node as metadata — the same
    /// seam the tower rigs read their cosmetic-spin cue through.</summary>
    private static Godot.Collections.Dictionary? Extra(Node node, string key)
    {
        if (!node.HasMeta("extras")) return null;
        var extras = node.GetMeta("extras");
        if (extras.VariantType != Variant.Type.Dictionary) return null;
        return extras.AsGodotDictionary().TryGetValue(key, out var v) && v.VariantType == Variant.Type.Dictionary
            ? v.AsGodotDictionary() : null;
    }

    private static Node3D? FindWithExtra(Node root, string key)
    {
        if (root is Node3D n3 && Extra(root, key) is not null) return n3;
        foreach (var child in root.GetChildren())
            if (FindWithExtra(child, key) is { } hit) return hit;
        return null;
    }

    private static Vector3 ToVector(Variant v, Vector3 fallback)
    {
        if (v.VariantType != Variant.Type.Array) return fallback;
        var arr = v.AsGodotArray();
        if (arr.Count < 3) return fallback;
        var out3 = new Vector3((float)arr[0].AsDouble(), (float)arr[1].AsDouble(), (float)arr[2].AsDouble());
        return out3.LengthSquared() < 1e-8f ? fallback : out3;
    }
}
