using Godot;
using System.Linq;
using DeepField.Sim.Content;

namespace DeepField.Game.Ui;

/// <summary>The 3D half of the build wheel: a translucent preview of the
/// selected structure on the aimed socket, plus its coverage.
///
/// Range is shown as a ground ring for ground-only towers, a dome for anything
/// that reaches air, and mortars additionally get an inner dead-zone ring —
/// min range is invisible otherwise and is the most common "why didn't it
/// shoot" surprise.</summary>
public partial class BuildGhost : Node3D
{
    private MeshInstance3D _body = null!;
    private MeshInstance3D _rangeRing = null!;
    private MeshInstance3D _minRing = null!;
    private MeshInstance3D _airDome = null!;

    private string _shownDef = "";

    public override void _Ready()
    {
        Visible = false;

        _body = new MeshInstance3D { MaterialOverride = GhostMaterial(UiTheme.Accent) };
        AddChild(_body);

        _rangeRing = new MeshInstance3D { MaterialOverride = RingMaterial(UiTheme.Accent) };
        AddChild(_rangeRing);

        _minRing = new MeshInstance3D { MaterialOverride = RingMaterial(UiTheme.Danger) };
        AddChild(_minRing);

        _airDome = new MeshInstance3D { MaterialOverride = RingMaterial(new Color(0.5f, 0.75f, 1f)) };
        AddChild(_airDome);
    }

    public void Hide3D() => Visible = false;

    public void Show(Vector3 socketPos, string defId, bool affordable)
    {
        Position = socketPos;
        Visible = true;

        var tint = affordable ? UiTheme.Accent : UiTheme.Danger;
        ((StandardMaterial3D)_body.MaterialOverride).AlbedoColor = tint with { A = 0.42f };
        ((StandardMaterial3D)_rangeRing.MaterialOverride).AlbedoColor = tint with { A = 0.30f };

        if (defId == _shownDef) return;
        _shownDef = defId;

        // Body proxy — replaced by design's real models via the same seam that
        // GameRoot.SpawnTowerView uses (docs/DESIGN-BRIEF.md §3.2/§3.3).
        if (Traps.All.TryGetValue(defId, out var trap))
        {
            _body.Mesh = new CylinderMesh { TopRadius = 1.5f, BottomRadius = 1.5f, Height = 0.2f };
            _body.Position = new Vector3(0, 0.2f, 0);
            SetRing(_rangeRing, trap.TriggerRadius);
            SetRing(_minRing, 0f);
            SetDome(0f);
            return;
        }

        var def = Towers.All[defId];
        if (def.Kind == TowerKind.Barricade)
        {
            _body.Mesh = new BoxMesh { Size = new Vector3(4.5f, 2.2f, 0.8f) };
            _body.Position = new Vector3(0, 1.1f, 0);
        }
        else
        {
            _body.Mesh = new BoxMesh { Size = new Vector3(1.2f, 2.6f, 1.2f) };
            _body.Position = new Vector3(0, 1.3f, 0);
        }

        SetRing(_rangeRing, def.RangeMeters);
        SetRing(_minRing, def.MinRangeMeters);
        SetDome(def.TargetLayers.Contains(EnemyLayer.Air) ? def.RangeMeters : 0f);
    }

    private void SetRing(MeshInstance3D ring, float radius)
    {
        if (radius <= 0.01f) { ring.Visible = false; return; }
        ring.Visible = true;
        ring.Mesh = new TorusMesh { InnerRadius = radius - 0.12f, OuterRadius = radius + 0.12f };
        ring.Position = new Vector3(0, 0.08f, 0);
    }

    private void SetDome(float radius)
    {
        if (radius <= 0.01f) { _airDome.Visible = false; return; }
        _airDome.Visible = true;
        _airDome.Mesh = new SphereMesh
        {
            Radius = radius, Height = radius * 2f,
            IsHemisphere = true, RadialSegments = 24, Rings = 8,
        };
    }

    private static StandardMaterial3D GhostMaterial(Color color) => new()
    {
        AlbedoColor = color with { A = 0.42f },
        Transparency = BaseMaterial3D.TransparencyEnum.Alpha,
        ShadingMode = BaseMaterial3D.ShadingModeEnum.Unshaded,
        CullMode = BaseMaterial3D.CullModeEnum.Disabled,
    };

    private static StandardMaterial3D RingMaterial(Color color) => new()
    {
        AlbedoColor = color with { A = 0.28f },
        Transparency = BaseMaterial3D.TransparencyEnum.Alpha,
        ShadingMode = BaseMaterial3D.ShadingModeEnum.Unshaded,
        CullMode = BaseMaterial3D.CullModeEnum.Disabled,
    };
}
