using Godot;
using System.Linq;
using DeepField.Sim.Content;

namespace DeepField.Game.Ui;

/// <summary>The 3D half of the build wheel: a translucent preview of the
/// selected structure on the aimed socket, plus its coverage.
///
/// Coverage is drawn by <see cref="CoverageRings"/>, the same node the upgrade
/// panel uses, so what you preview and what you own are the same picture: a
/// ground ring for ground-only towers, a dome for anything that reaches air,
/// and an inner dead-zone ring for mortars — min range is invisible otherwise
/// and is the most common "why didn't it shoot" surprise.</summary>
public partial class BuildGhost : Node3D
{
    private Node3D _body = null!;
    private StandardMaterial3D _bodyMaterial = null!;
    private CoverageRings _rings = null!;

    private string _shownDef = "";

    public override void _Ready()
    {
        Visible = false;

        _bodyMaterial = GhostMaterial(UiTheme.Accent);
        _body = new Node3D();
        AddChild(_body);

        _rings = new CoverageRings();
        AddChild(_rings);
    }

    public void Hide3D() => Visible = false;

    public void Show(Vector3 socketPos, string defId, bool affordable)
    {
        Position = socketPos;
        Visible = true;

        var tint = affordable ? UiTheme.Accent : UiTheme.Danger;
        _bodyMaterial.AlbedoColor = tint with { A = 0.42f };
        _rings.Tint(tint);

        if (defId == _shownDef) return;
        _shownDef = defId;

        // The ghost is the real structure model (or the same graybox the world
        // uses), rendered translucent — so what you preview is what you get.
        SwapBody(defId);

        if (Traps.All.TryGetValue(defId, out var trap))
        {
            _rings.ShowFor(defId, trap.TriggerRadius, 0f, reachesAir: false);
            return;
        }

        // The ghost previews a fresh build, which is always at level 1 — the
        // preview ring belongs to the upgrade panel, where there is a purchase
        // to preview.
        var def = Towers.All[defId];
        _rings.ShowFor(defId, def.RangeMeters, def.MinRangeMeters,
            def.TargetLayers.Contains(EnemyLayer.Air));
        _rings.HidePreview();
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

    private static StandardMaterial3D GhostMaterial(Color color) => new()
    {
        AlbedoColor = color with { A = 0.42f },
        Transparency = BaseMaterial3D.TransparencyEnum.Alpha,
        ShadingMode = BaseMaterial3D.ShadingModeEnum.Unshaded,
        CullMode = BaseMaterial3D.CullModeEnum.Disabled,
    };
}
