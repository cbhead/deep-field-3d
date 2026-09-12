using Godot;
using System.Collections.Generic;
using DeepField.Game.Ui;

namespace DeepField.Game;

/// <summary>Design's mesh-based effects, driven the way its brief asks
/// (docs/design/models/vfx.js, projectiles.js): every file is a static hero
/// frame the client scales, turns and fades over a short life, with named
/// sub-groups it can drive without lookups — <c>_spin</c> rotates about Y,
/// <c>_pulse</c> breathes, <c>_rise</c> lifts. Muzzle flashes face −Z and
/// scale from nothing; impacts scale up and fade; tracers are a unit-length
/// streak along −Z that is stretched to the hit.
///
/// Everything here is cosmetic and client-side. A tracer is drawn from the
/// muzzle to wherever the shot went the instant the trigger is pulled; the
/// sim decides separately whether it hurt, and says so through
/// EnemyDamaged. Nothing waits on the server to look like a gun.</summary>
public partial class Vfx : Node3D
{
    private sealed class Burst
    {
        public Node3D Node = null!;
        public float Life, Age;
        public float ScaleFrom, ScaleTo;
        public Vector3 Stretch = Vector3.One;   // applied on top of the scale curve (tracers)
        public Node3D? Spin, Pulse, Rise;
        public bool ShrinkOut;                   // tracers thin to nothing instead of scaling out
    }

    private readonly List<Burst> _live = new();

    /// <summary>Palette per ammo type, from design's tracer set: the round's
    /// hue is what tells a cryo streak from a standard one across the map.</summary>
    public static Color AmmoHue(string ammoId) => ammoId switch
    {
        "ap" => new Color("cfd7e4"),
        "hollowpoint" => new Color("e8862b"),
        "incendiary" => UiTheme.Status("burn"),
        "cryo" => UiTheme.Status("chill"),
        "shock" => UiTheme.Status("shock"),
        "toxin" => UiTheme.Status("poison"),
        _ => new Color("f4dca4"),
    };

    // ---------------------------------------------------------------------
    // Guns
    // ---------------------------------------------------------------------

    /// <summary>One shot from a hero weapon: flash at the muzzle, streak to
    /// the point the shot ended, a burst where it landed.</summary>
    public void GunShot(Vector3 muzzle, Vector3 hit, string ammoId, bool struckSomething)
    {
        var hue = AmmoHue(ammoId);
        var forward = hit - muzzle;
        if (forward.LengthSquared() < 1e-4f) return;

        Burst? flash = Spawn("vfx_muzzle_lance", muzzle, forward.Normalized(), life: 0.07f, from: 0.05f, to: 0.32f, tint: hue);
        _ = flash;

        string tracer = AssetLibrary.Has($"vfx_tracer_{ammoId}") ? $"vfx_tracer_{ammoId}" : "vfx_tracer_standard";
        // A little past the muzzle so the streak starts outside the flash.
        var start = muzzle + forward.Normalized() * 0.25f;
        if (Spawn(tracer, start, forward.Normalized(), life: 0.09f, from: 1f, to: 1f) is { } streak)
        {
            streak.Stretch = new Vector3(1f, 1f, Mathf.Max(0.1f, forward.Length() - 0.25f));
            streak.ShrinkOut = true;
        }

        if (struckSomething)
            Spawn("vfx_impact_lance", hit, -forward.Normalized(), life: 0.22f, from: 0.12f, to: 0.42f, tint: hue);
    }

    /// <summary>A teammate's shot, reconstructed from the damage it did: the
    /// command never crosses the wire, but the hit does, and a streak from
    /// their avatar to the target is what makes co-op fire legible.</summary>
    public void RemoteShot(Vector3 from, Vector3 hit)
    {
        var forward = hit - from;
        if (forward.LengthSquared() < 1e-4f) return;
        if (Spawn("vfx_tracer_standard", from, forward.Normalized(), life: 0.09f, from: 1f, to: 1f) is { } streak)
        {
            streak.Stretch = new Vector3(0.7f, 0.7f, forward.Length());
            streak.ShrinkOut = true;
        }
    }

    // ---------------------------------------------------------------------
    // Towers
    // ---------------------------------------------------------------------

    /// <summary>Design's per-tower flash at the rig's muzzle node, facing the
    /// way the barrel points. Aura towers have no muzzle and no flash.</summary>
    public void TowerFired(string towerDefId, Vector3 muzzle, Vector3 forward)
    {
        string asset = $"vfx_muzzle_{towerDefId}";
        if (!AssetLibrary.Has(asset)) return;
        Spawn(asset, muzzle, forward, life: 0.08f, from: 0.1f, to: 1f);
    }

    /// <summary>Where a tower's round stopped existing. The sim removes a
    /// projectile the tick it lands, so the view's last position is the hit.</summary>
    public void ProjectileLanded(string towerDefId, Vector3 at)
    {
        string asset = $"vfx_impact_{towerDefId}";
        if (!AssetLibrary.Has(asset)) return;
        // Nova's splash ring is authored flat on the ground; the rest burst
        // outward from the point.
        Spawn(asset, at, towerDefId == "nova" ? Vector3.Up : Vector3.Forward, life: 0.25f, from: 0.2f, to: 1f);
    }

    // ---------------------------------------------------------------------
    // Going somewhere else
    // ---------------------------------------------------------------------

    /// <summary>A player arriving or leaving on the pad network. Teal, and the
    /// same column at both ends, because what the effect has to say is "that
    /// person is now over there" — two different effects would read as two
    /// different things happening.</summary>
    public void Teleport(Vector3 at) => Warp(at, UiTheme.Status("chill"));

    /// <summary>An enemy crossing a warp gate. The same shape in the colour
    /// everything hostile is drawn in: a player has to be able to tell, from
    /// the far side of a field, which of the two just happened.</summary>
    public void EnemyWarp(Vector3 at) => Warp(at, new Color("e9614c"));

    private void Warp(Vector3 at, Color hue)
    {
        // Design's teleport burst if it has landed; the flat impact ring is
        // the stand-in, which is authored lying on the ground and is the
        // closest thing in the delivered set to a column of light.
        string asset = AssetLibrary.Has("vfx_teleport_burst") ? "vfx_teleport_burst" : "vfx_impact_nova";
        Spawn(asset, at + Vector3.Up * 0.2f, Vector3.Up, life: 0.45f, from: 0.3f, to: 1.6f, tint: hue);
    }

    // ---------------------------------------------------------------------

    private Burst? Spawn(string asset, Vector3 at, Vector3 forward, float life, float from, float to, Color? tint = null)
    {
        var node = AssetLibrary.TryInstantiate(asset);
        if (node is null) return null;
        MapKit.NoShadow(node);
        node.Position = at;
        AddChild(node);
        if (forward.LengthSquared() > 1e-6f)
        {
            var up = Mathf.Abs(forward.Normalized().Dot(Vector3.Up)) > 0.99f ? Vector3.Forward : Vector3.Up;
            node.LookAt(at + forward, up);
        }
        node.Scale = Vector3.One * from;
        if (tint is { } hue) Tint(node, hue);

        var burst = new Burst
        {
            Node = node, Life = life, ScaleFrom = from, ScaleTo = to,
            Spin = FindSuffix(node, "_spin"), Pulse = FindSuffix(node, "_pulse"), Rise = FindSuffix(node, "_rise"),
        };
        _live.Add(burst);
        return burst;
    }

    private static Node3D? FindSuffix(Node root, string suffix)
    {
        if (root is Node3D n3 && n3.Name.ToString().EndsWith(suffix)) return n3;
        foreach (var child in root.GetChildren())
            if (FindSuffix(child, suffix) is { } found) return found;
        return null;
    }

    /// <summary>Design's effect meshes carry their tower's energy hue; a gun's
    /// round carries its ammo's. Only the glowing parts take the tint — the
    /// chrome core and shards stay what they are.</summary>
    private static void Tint(Node node, Color hue)
    {
        if (node is MeshInstance3D mesh)
        {
            for (int i = 0; i < mesh.GetSurfaceOverrideMaterialCount(); i++)
            {
                if (mesh.GetActiveMaterial(i) is not StandardMaterial3D material || !material.EmissionEnabled) continue;
                var copy = (StandardMaterial3D)material.Duplicate();
                copy.AlbedoColor = hue;
                copy.Emission = hue;
                mesh.SetSurfaceOverrideMaterial(i, copy);
            }
        }
        foreach (var child in node.GetChildren()) Tint(child, hue);
    }

    public override void _Process(double delta)
    {
        for (int i = _live.Count - 1; i >= 0; i--)
        {
            var b = _live[i];
            b.Age += (float)delta;
            if (b.Age >= b.Life || !IsInstanceValid(b.Node))
            {
                if (IsInstanceValid(b.Node)) b.Node.QueueFree();
                _live.RemoveAt(i);
                continue;
            }
            float t = b.Age / b.Life;
            // Fast in, ease out: the flash is there at once and lingers a beat.
            float s = Mathf.Lerp(b.ScaleFrom, b.ScaleTo, 1f - (1f - t) * (1f - t));
            var scale = Vector3.One * s;
            if (b.ShrinkOut)
            {
                float thin = 1f - t;
                scale = new Vector3(thin, thin, 1f);
            }
            b.Node.Scale = scale * b.Stretch;
            if (b.Spin is not null) b.Spin.RotateY((float)delta * 6f);
            if (b.Pulse is not null) b.Pulse.Scale = Vector3.One * (1f + 0.25f * Mathf.Sin(t * Mathf.Pi * 2f));
            if (b.Rise is not null) b.Rise.Position += Vector3.Up * (float)delta * 0.8f;
        }
    }
}
