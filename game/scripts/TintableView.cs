using Godot;
using System.Collections.Generic;

namespace DeepField.Game;

/// <summary>A view whose materials can be modulated at runtime without
/// destroying what the artist authored.
///
/// This is the code half of the brief's rule that "albedo must tolerate
/// runtime modulation". The old tinting path assigned AlbedoColor outright,
/// which was fine while every enemy was a gray capsule and catastrophic the
/// moment a .glb lands — design's colors would be overwritten on frame one.
///
/// Instead: on first use we duplicate whatever material each surface shipped
/// with (so the shared resource is untouched), remember its base albedo and
/// emission, and thereafter express status and damage as a *blend* and a
/// *darkening factor* over that base. A burning Drifter still looks like
/// design's Drifter, lit on fire.
///
/// Surfaces using a ShaderMaterial are left alone — if design ships a custom
/// shader for a unit, it owns that unit's look and status reads through the
/// overhead icons and particles instead.</summary>
public partial class TintableView : Node3D
{
    private readonly struct Slot
    {
        public readonly StandardMaterial3D Material;
        public readonly Color Albedo;
        public readonly Color Emission;
        public readonly bool EmissionEnabled;

        public Slot(StandardMaterial3D material)
        {
            Material = material;
            Albedo = material.AlbedoColor;
            Emission = material.Emission;
            EmissionEnabled = material.EmissionEnabled;
        }
    }

    private readonly List<Slot> _slots = new();
    private bool _prepared;

    /// <summary>Walks the model once and takes private copies of its materials.
    /// Safe to call repeatedly; only the first call does work.</summary>
    public void Prepare()
    {
        if (_prepared) return;
        _prepared = true;
        Collect(this);
    }

    public bool CanTint => _slots.Count > 0;

    private void Collect(Node node)
    {
        if (node is MeshInstance3D mesh) CollectFrom(mesh);
        foreach (var child in node.GetChildren()) Collect(child);
    }

    private void CollectFrom(MeshInstance3D mesh)
    {
        // A material override beats surface overrides, so if one is present it
        // is the only thing worth duplicating.
        if (mesh.MaterialOverride is StandardMaterial3D over)
        {
            var copy = (StandardMaterial3D)over.Duplicate();
            mesh.MaterialOverride = copy;
            _slots.Add(new Slot(copy));
            return;
        }

        int surfaces = mesh.Mesh?.GetSurfaceCount() ?? 0;
        for (int s = 0; s < surfaces; s++)
        {
            if (mesh.GetActiveMaterial(s) is not StandardMaterial3D material) continue;
            var copy = (StandardMaterial3D)material.Duplicate();
            mesh.SetSurfaceOverrideMaterial(s, copy);
            _slots.Add(new Slot(copy));
        }
    }

    /// <summary>Modulate every surface: status blend first, then the damage
    /// lerp on top of it, then emission (null restores the model's own glow).
    /// Both stages are lerps over design's albedo rather than replacements —
    /// PALETTE.md specifies "base albedo lerps to #C93B28 as hp falls".</summary>
    public void Apply(Color? status, float statusWeight, Color damage, float damageWeight, Color? emission)
    {
        foreach (var slot in _slots)
        {
            Color albedo = status is { } tint
                ? slot.Albedo.Lerp(tint, statusWeight)
                : slot.Albedo;
            if (damageWeight > 0f) albedo = albedo.Lerp(damage, damageWeight);
            slot.Material.AlbedoColor = albedo;

            if (emission is { } glow)
            {
                slot.Material.EmissionEnabled = true;
                slot.Material.Emission = glow;
            }
            else
            {
                slot.Material.EmissionEnabled = slot.EmissionEnabled;
                slot.Material.Emission = slot.Emission;
            }
        }
    }
}
