using Godot;
using System.Collections.Generic;
using DeepField.Sim.Content;
using System.Linq;

namespace DeepField.Game;

/// <summary>Design's aim contract for a tower model
/// (game/assets/structures/README-towers.md, "Tower Aim Rigs"):
/// <c>&lt;id&gt;_foot</c> never moves, <c>&lt;id&gt;_yaw</c> turns about +Y,
/// <c>&lt;id&gt;_pitch</c> elevates about X with <c>rotation.x = -pitch</c>, and
/// <c>&lt;id&gt;_muzzle</c> is where a round leaves. Aura towers have no rig at
/// all — an aura tower that pointed at things would lie about how it works —
/// and carry a cosmetic <c>_spin</c> group instead.
///
/// Limits and slew rates are design's numbers, read from the manifest that
/// ships beside the models: a Nova's 22° pitch floor *is* its 5 m minimum
/// range expressed in geometry, and a Lance that a fast crosser out-slews is
/// meant to miss. The graybox has none of these nodes, so a placeholder still
/// turns as a whole (see GameRoot.AimTowers).</summary>
public sealed class TowerRig
{
    public readonly record struct Spec(
        float YawMin, float YawMax, float PitchMin, float PitchMax,
        float TraverseDegPerSec, float ElevateDegPerSec)
    {
        /// <summary>Used when the manifest has no row: a full-circle mount at
        /// the Lance's rates, so a rigged model with no entry still tracks.</summary>
        public static readonly Spec Default = new(-180f, 180f, -8f, 26f, 90f, 60f);

        /// <summary>The same fallback for anything that can shoot air, with a
        /// ceiling it can actually use.
        ///
        /// Arc has no manifest row and targets air, so it inherited a 26 degree
        /// ceiling — and a Skiff on the strand sits sixty degrees up from a pad
        /// near it. The barrel would have parked at its limit, a third of the
        /// way to the target, while the sim dealt full damage. Ground-only
        /// towers keep the tighter ceiling because for them it is correct: a
        /// Lance that cannot crank up to eighty degrees is a Lance, not a bug.</summary>
        public static readonly Spec DefaultAir = new(-180f, 180f, -8f, 85f, 90f, 60f);
    }

    public Node3D? Yaw { get; private init; }
    public Node3D? Pitch { get; private init; }
    public Node3D? Muzzle { get; private init; }
    public IReadOnlyList<Node3D> Spins { get; private init; } = System.Array.Empty<Node3D>();
    public Spec Limits { get; private init; }

    /// <summary>A model that can be pointed. Without a yaw node the view is
    /// either an aura tower (spin only) or a graybox (turn the root).</summary>
    public bool Articulated => Yaw is not null;

    /// <summary>Which way the bore points now, in world space. Design authors
    /// the barrel along the yaw node's local +Z (the export flips the root by
    /// half a turn so the file faces -Z); the aim probe reads this so it can
    /// judge the turret rather than the socket.</summary>
    public Vector3 Forward => Pitch is not null ? Pitch.GlobalTransform.Basis.Z
        : Yaw is not null ? Yaw.GlobalTransform.Basis.Z
        : Vector3.Forward;

    private static readonly System.Text.RegularExpressions.Regex SpinName =
        new(@"_(spin|radar)$", System.Text.RegularExpressions.RegexOptions.Compiled);

    public static TowerRig Resolve(Node3D view, string defId)
    {
        var spins = new List<Node3D>();
        Collect(view, spins);
        return new TowerRig
        {
            Yaw = view.FindChild($"{defId}_yaw", recursive: true, owned: false) as Node3D,
            Pitch = view.FindChild($"{defId}_pitch", recursive: true, owned: false) as Node3D,
            Muzzle = view.FindChild($"{defId}_muzzle", recursive: true, owned: false) as Node3D,
            Spins = spins,
            Limits = Manifest.TryGetValue(defId, out var spec) ? spec : FallbackFor(defId),
        };
    }

    /// <summary>Which fallback a tower with no manifest row gets. The only one
    /// this actually changes today is Arc; Detector and Singularity are auras
    /// with nothing to point, and a Barricade has no weapon at all.</summary>
    private static Spec FallbackFor(string defId) =>
        Towers.All.TryGetValue(defId, out var def)
            && def.TargetLayers.Contains(EnemyLayer.Air)
                ? Spec.DefaultAir
                : Spec.Default;

    /// <summary>Cosmetic spin groups: design names them <c>_spin</c> and tags
    /// them <c>extras.role = cosmeticSpin</c> (Godot keeps glTF extras as the
    /// node's "extras" metadata). The Skywatch's radar is one, and it lives
    /// under the yaw node, so both cues are checked rather than the name alone.</summary>
    private static void Collect(Node node, List<Node3D> spins)
    {
        if (node is Node3D n3 && (SpinName.IsMatch(n3.Name) || IsCosmeticSpin(n3)))
            spins.Add(n3);
        foreach (var child in node.GetChildren()) Collect(child, spins);
    }

    private static bool IsCosmeticSpin(Node node)
    {
        if (!node.HasMeta("extras")) return false;
        var extras = node.GetMeta("extras");
        return extras.VariantType == Variant.Type.Dictionary
            && extras.AsGodotDictionary().TryGetValue("role", out var role)
            && role.AsString() == "cosmeticSpin";
    }

    /// <summary>Slews the rig towards a world-space point, no faster than the
    /// model's traverse and elevation rates, and never past its limits. The
    /// target is the point the barrel wants; whether the sim hits is decided
    /// elsewhere — this is the pull-continuous cosmetic side of the split.
    /// Returns true once the rig is no longer rate-limited on either axis —
    /// it is on the target, or holding a limit against it — which is what the
    /// aim probe treats as settled.</summary>
    public bool AimAt(Vector3 target, double delta)
    {
        if (Yaw is null) return false;
        bool settled = true;
        var parent = Yaw.GetParent<Node3D>();
        var local = parent.ToLocal(target);
        if (local.X * local.X + local.Z * local.Z > 1e-4f)
        {
            // Bore along local +Z, so the heading is atan2(x, z) in the parent's
            // frame — the same frame design's viewer drives the rig in.
            float want = Mathf.RadToDeg(Mathf.Atan2(local.X, local.Z));
            float have = Yaw.RotationDegrees.Y;
            float step = (float)delta * Limits.TraverseDegPerSec;
            float next;
            if (Limits.YawMax - Limits.YawMin >= 360f)
            {
                // Unlimited mount: shortest way round, across the seam.
                float diff = Mathf.Wrap(want - have, -180f, 180f);
                next = have + Mathf.Clamp(diff, -step, step);
                settled = Mathf.Abs(diff) <= step;
            }
            else
            {
                want = Mathf.Clamp(Mathf.Wrap(want, -180f, 180f), Limits.YawMin, Limits.YawMax);
                next = have + Mathf.Clamp(want - have, -step, step);
                settled = Mathf.Abs(want - have) <= step;
            }
            Yaw.RotationDegrees = new Vector3(0f, next, 0f);
        }

        if (Pitch is null) return settled;
        var from = Pitch.GlobalPosition;
        var to = target - from;
        float horizontal = Mathf.Sqrt(to.X * to.X + to.Z * to.Z);
        if (horizontal < 1e-3f && Mathf.Abs(to.Y) < 1e-3f) return settled;
        float wantPitch = Mathf.Clamp(Mathf.RadToDeg(Mathf.Atan2(to.Y, horizontal)),
            Limits.PitchMin, Limits.PitchMax);
        // Barrel-up is negative X on the pitch node (design's convention).
        float havePitch = -Pitch.RotationDegrees.X;
        float elevate = (float)delta * Limits.ElevateDegPerSec;
        float nextPitch = havePitch + Mathf.Clamp(wantPitch - havePitch, -elevate, elevate);
        Pitch.RotationDegrees = new Vector3(-nextPitch, 0f, 0f);
        return settled && Mathf.Abs(wantPitch - havePitch) <= elevate;
    }

    /// <summary>Turns the cosmetic groups. Faster while the tower has something
    /// in reach, so an aura tower reads as working rather than decorating.</summary>
    public void Spin(double delta, bool engaged)
    {
        if (Spins.Count == 0) return;
        float step = (float)delta * (engaged ? EngagedSpinDegPerSec : IdleSpinDegPerSec);
        foreach (var spin in Spins)
            spin.RotateY(Mathf.DegToRad(step));
    }

    private const float IdleSpinDegPerSec = 25f;
    private const float EngagedSpinDegPerSec = 110f;

    // ---------------------------------------------------------------------
    // Manifest: design's limits, keyed by tower id
    // ---------------------------------------------------------------------

    private const string ManifestPath = "res://assets/structures/manifest.json";
    private static Dictionary<string, Spec>? _manifest;

    /// <summary>Rig rows from design's <c>manifest.json</c>. The file is design's
    /// receipt for the drop (bounds, tri counts, rig node names, limits) and
    /// ships next to the models, so the numbers live in one place. Absent or
    /// unreadable, every rigged tower falls back to <see cref="Spec.Default"/>
    /// and says so once.</summary>
    private static Dictionary<string, Spec> Manifest => _manifest ??= LoadManifest();

    private static Dictionary<string, Spec> LoadManifest()
    {
        var specs = new Dictionary<string, Spec>();
        if (!FileAccess.FileExists(ManifestPath))
        {
            GD.Print($"[rig] {ManifestPath} not found; rigged towers use default limits");
            return specs;
        }
        using var file = FileAccess.Open(ManifestPath, FileAccess.ModeFlags.Read);
        var json = new Json();
        if (file is null || json.Parse(file.GetAsText()) != Error.Ok)
        {
            GD.PushWarning($"[rig] could not parse {ManifestPath}; rigged towers use default limits");
            return specs;
        }
        var root = json.Data.AsGodotDictionary();
        if (!root.TryGetValue("files", out var files)) return specs;
        foreach (var entry in files.AsGodotArray())
        {
            var row = entry.AsGodotDictionary();
            if (!row.TryGetValue("kind", out var kind) || kind.AsString() != "chassis") continue;
            if (!row.TryGetValue("rig", out var rigVar) || rigVar.VariantType != Variant.Type.Dictionary) continue;
            var rig = rigVar.AsGodotDictionary();
            var yaw = rig["yawLimits"].AsGodotArray();
            var pitch = rig["pitchLimits"].AsGodotArray();
            specs[row["tower"].AsString()] = new Spec(
                (float)yaw[0].AsDouble(), (float)yaw[1].AsDouble(),
                (float)pitch[0].AsDouble(), (float)pitch[1].AsDouble(),
                (float)rig["traverseDegPerSec"].AsDouble(), (float)rig["elevateDegPerSec"].AsDouble());
        }
        return specs;
    }

    /// <summary>Static Godot resources must be released before the SceneTree
    /// tears down (see UiTheme.ReleaseCaches); the manifest holds none, but the
    /// cache is cleared with the rest so a re-entered match re-reads a fresh drop.</summary>
    public static void ReleaseCaches() => _manifest = null;
}
