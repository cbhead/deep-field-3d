using Godot;

namespace DeepField.Game.Ui;

/// <summary>One billboarded bar quad in world space, draining rightward.
///
/// Extracted when structures gained health: an enemy losing hp and a tower
/// being demolished are the same statement to a player, and two copies of this
/// would have drifted the moment one of them was restyled.</summary>
public partial class WorldBar : MeshInstance3D
{
    private float _alpha = 1f;

    public static WorldBar Make(Color color, float depthNudge, Vector2 size)
    {
        return new WorldBar
        {
            Mesh = new QuadMesh { Size = size },
            MaterialOverride = new StandardMaterial3D
            {
                AlbedoColor = color,
                ShadingMode = BaseMaterial3D.ShadingModeEnum.Unshaded,
                BillboardMode = BaseMaterial3D.BillboardModeEnum.Enabled,
                // A billboard rebuilds the model-view basis to face the camera
                // and throws the node's scale away with it unless told not to.
                // Every bar in the game has therefore been drawing full width
                // and relying on its leftward shift to look like it was
                // draining: a barricade measured at 0.55 health rendered a bar
                // reading about 0.88. Nothing about that is visible until you
                // photograph one and count the pixels against the number.
                BillboardKeepScale = true,
                Transparency = BaseMaterial3D.TransparencyEnum.Alpha,
                NoDepthTest = false,
                RenderPriority = 1,
            },
            Position = new Vector3(0, 0, depthNudge),
        };
    }

    /// <summary>Fill to a fraction of full width at the given opacity. Quads
    /// scale about their centre, so the bar is shifted left by half of what it
    /// lost — otherwise it would shrink towards the middle from both ends.</summary>
    public void Set(float fraction, float alpha)
    {
        fraction = Mathf.Clamp(fraction, 0f, 1f);
        float width = Mesh is QuadMesh quad ? quad.Size.X : 1.1f;
        Scale = new Vector3(fraction, 1f, 1f);
        Position = new Vector3(-(1f - fraction) * width * 0.5f, Position.Y, Position.Z);
        _alpha = alpha;
        if (MaterialOverride is StandardMaterial3D material)
            material.AlbedoColor = material.AlbedoColor with { A = alpha };
    }

    /// <summary>Recolours without disturbing the opacity the distance fade set.</summary>
    public void Tint(Color color)
    {
        if (MaterialOverride is not StandardMaterial3D material) return;
        material.AlbedoColor = color with { A = _alpha };
        material.Emission = color;
    }
}
