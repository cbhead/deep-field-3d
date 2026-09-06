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
    private Node3D _body = null!;
    private StandardMaterial3D _bodyMaterial = null!;
    private MeshInstance3D _rangeRing = null!;
    private MeshInstance3D _minRing = null!;
    private MeshInstance3D _airDome = null!;

    private string _shownDef = "";

    public override void _Ready()
    {
        Visible = false;

        _bodyMaterial = GhostMaterial(UiTheme.Accent);
        _body = new Node3D();
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
        _bodyMaterial.AlbedoColor = tint with { A = 0.42f };
        ((StandardMaterial3D)_rangeRing.MaterialOverride).AlbedoColor = tint with { A = 0.30f };

        if (defId == _shownDef) return;
        _shownDef = defId;

        // The ghost is the real structure model (or the same graybox the world
        // uses), rendered translucent — so what you preview is what you get.
        SwapBody(defId);

        if (Traps.All.TryGetValue(defId, out var trap))
        {
            SetRing(_rangeRing, trap.TriggerRadius);
            SetRing(_minRing, 0f);
            SetDome(0f);
            return;
        }

        var def = Towers.All[defId];
        SetRing(_rangeRing, def.RangeMeters);
        SetRing(_minRing, def.MinRangeMeters);
        SetDome(def.TargetLayers.Contains(EnemyLayer.Air) ? def.RangeMeters : 0f);
    }

    /// <summary>Rebuilds the ghost body from whichever asset the def resolves
    /// to, forcing the translucent material onto every surface it contains.</summary>
    private void SwapBody(string defId)
    {
        foreach (var child in _body.GetChildren())
        {
            _body.RemoveChild(child);
            child.QueueFree();
        }

        // Chassis only — the ghost previews what you're about to buy, which is
        // always the base tower.
        var model = AssetLibrary.Instantiate(
            AssetLibrary.StructureAsset(defId), () => Placeholders.Structure(defId));
        _body.AddChild(model);
        Ghostify(model);
    }

    private void Ghostify(Node node)
    {
        if (node is MeshInstance3D mesh) mesh.MaterialOverride = _bodyMaterial;
        foreach (var child in node.GetChildren()) Ghostify(child);
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
