using Godot;
using System.Linq;
using DeepField.Sim.Content;

namespace DeepField.Game.Ui;

/// <summary>What a structure covers, drawn in the world: a ground ring at its
/// reach, a red inner ring for a mortar's dead zone, and a dome over the whole
/// thing when it can shoot air.
///
/// Built for the build ghost first, and reused by the upgrade panel — the two
/// have to draw the same picture or "what I bought" and "what I have" look
/// like different towers. The upgrade case adds one ring the ghost has no use
/// for: <see cref="ShowPreview"/> draws where the reach *would* be after the
/// next Range purchase, because the number in the panel does not tell a player
/// whether that buys the corner they keep leaking from.</summary>
public partial class CoverageRings : Node3D
{
    private MeshInstance3D? _range;
    private MeshInstance3D? _min;
    private MeshInstance3D? _dome;
    private MeshInstance3D? _preview;

    /// <summary>The preview reads as "more", so it is the accent colour at
    /// full strength while the ring you already own sits back — the eye should
    /// land on the gain, not on the status quo.</summary>
    private static readonly Color PreviewColor = new(0.55f, 0.95f, 0.6f);

    public override void _Ready() => Build();

    /// <summary>Built on demand, not only in <c>_Ready</c>: a parent that is
    /// not in the tree yet gets its children's <c>_Ready</c> deferred, and the
    /// upgrade rings are hidden by their owner on the same line they are
    /// created.</summary>
    private void Build()
    {
        if (_range is not null) return;
        _range = AddRing(UiTheme.Accent, 0.28f);
        _min = AddRing(UiTheme.Danger, 0.28f);
        _dome = AddRing(new Color(0.5f, 0.75f, 1f), 0.28f);
        _preview = AddRing(PreviewColor, 0.55f);
    }

    private MeshInstance3D AddRing(Color color, float alpha)
    {
        var mesh = new MeshInstance3D
        {
            MaterialOverride = new StandardMaterial3D
            {
                AlbedoColor = color with { A = alpha },
                Transparency = BaseMaterial3D.TransparencyEnum.Alpha,
                ShadingMode = BaseMaterial3D.ShadingModeEnum.Unshaded,
                CullMode = BaseMaterial3D.CullModeEnum.Disabled,
            },
            Visible = false,
            // A coverage ring is a diagram laid over the map, not a thing on
            // it: it must not darken the deck it sits on or catch the tower's
            // own light.
            CastShadow = GeometryInstance3D.ShadowCastingSetting.Off,
        };
        AddChild(mesh);
        return mesh;
    }

    public void Tint(Color color)
    {
        Build();
        ((StandardMaterial3D)_range!.MaterialOverride).AlbedoColor = color with { A = 0.30f };
    }

    /// <summary>The coverage a structure has right now.</summary>
    public void ShowFor(string defId, float rangeMeters, float minRangeMeters, bool reachesAir)
    {
        Build();
        Visible = true;
        SetRing(_range!, rangeMeters);
        SetRing(_min!, minRangeMeters);
        SetDome(reachesAir ? rangeMeters : 0f);
    }

    /// <summary>Reach after one more purchase. Passing a radius that is not
    /// bigger than the current one hides it — there is nothing to show when
    /// the path is capped, or when the path being bought is not the one that
    /// moves range.</summary>
    public void ShowPreview(float rangeMeters, float currentMeters)
    {
        Build();
        if (rangeMeters <= currentMeters + 0.05f) { _preview!.Visible = false; return; }
        SetRing(_preview!, rangeMeters, thickness: 0.2f, height: 0.14f);
    }

    public void HidePreview()
    {
        Build();
        _preview!.Visible = false;
    }

    public void Hide3D()
    {
        Visible = false;
        HidePreview();
    }

    private void SetRing(MeshInstance3D ring, float radius, float thickness = 0.12f, float height = 0.08f)
    {
        if (radius <= 0.01f) { ring.Visible = false; return; }
        ring.Visible = true;
        ring.Mesh = new TorusMesh { InnerRadius = radius - thickness, OuterRadius = radius + thickness };
        ring.Position = new Vector3(0, height, 0);
    }

    private void SetDome(float radius)
    {
        Build();
        if (radius <= 0.01f) { _dome!.Visible = false; return; }
        _dome!.Visible = true;
        _dome.Mesh = new SphereMesh
        {
            Radius = radius, Height = radius * 2f,
            IsHemisphere = true, RadialSegments = 24, Rings = 8,
        };
    }

    /// <summary>Coverage for a tower def at the levels it has bought, read
    /// through the same maths the sim fights with.</summary>
    public void ShowForTower(TowerDef def, int[] pathLevels, MapDef map, int waveIndex)
    {
        float range = TowerMath.Range(def, pathLevels, map, waveIndex);
        if (range <= 0.01f) { Hide3D(); return; }
        ShowFor(def.Id, range, def.MinRangeMeters,
            def.TargetLayers.Contains(EnemyLayer.Air));

        int rangePath = TowerMath.RangePathIndex(def);
        if (rangePath < 0) { HidePreview(); return; }
        ShowPreview(TowerMath.RangeAfterUpgrade(def, pathLevels, rangePath, map, waveIndex), range);
    }
}
