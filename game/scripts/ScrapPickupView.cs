using Godot;
using DeepField.Game.Ui;
using DeepField.Sim.Content;

namespace DeepField.Game;

/// <summary>Scrap lying on the floor, drawn with design's model for its type
/// (`pickup_alloy`, `pickup_flux`, …). Design authors each one with a
/// <c>pickup_&lt;type&gt;_spin</c> group above a ground ring: the ring stays
/// put and marks the spot, the group turns and bobs. The note on the Prime
/// Core says it should "read as the rarest thing on the floor", and it carries
/// its own light column to do it.
///
/// The model is the whole of the readability budget here — no billboard, no
/// label. A player should learn the silhouettes, and four of them in a pile
/// with text over each would be worse than four shapes.</summary>
public partial class ScrapPickupView : Node3D
{
    private Node3D? _spin;
    private float _time;
    private float _phase;
    /// <summary>Design lifts each spin group to its own authored height
    /// (`grp('pickup_alloy_spin', [0, .25, 0])`). The bob rides on top of that
    /// rather than replacing it, or every drop sinks into its ground ring.</summary>
    private float _restY;

    public static ScrapPickupView Spawn(Node parent, ScrapType type, Vector3 position, int amount)
    {
        var view = new ScrapPickupView { Position = position };
        parent.AddChild(view);
        string asset = $"pickup_{type.ToString().ToLowerInvariant()}";
        var model = AssetLibrary.Instantiate(asset, () => Placeholder(type));
        view.AddChild(model);
        // A drop is a small bright thing on a dark floor; it does not need to
        // cast a shadow, and a pile of them casting shadows reads as clutter.
        MapKit.NoShadow(model);
        view._spin = model.FindChild($"{asset}_spin", recursive: true, owned: false) as Node3D;
        view._restY = view._spin?.Position.Y ?? 0f;
        // Deterministic per drop, so two pickups side by side are not in
        // lockstep — the pair reads as two things rather than one wide thing.
        view._phase = (amount * 0.7f + (int)type * 1.3f) % Mathf.Tau;
        return view;
    }

    /// <summary>Until design's model is imported: a diamond in the scrap's own
    /// colour, the same shape the UI uses for that type.</summary>
    private static Node3D Placeholder(ScrapType type)
    {
        var root = new Node3D();
        var color = UiTheme.Scrap(type);
        root.AddChild(new MeshInstance3D
        {
            Mesh = new SphereMesh { Radius = 0.18f, Height = 0.36f, RadialSegments = 6, Rings = 3 },
            Position = new Vector3(0f, 0.3f, 0f),
            MaterialOverride = new StandardMaterial3D
            {
                AlbedoColor = color, EmissionEnabled = true, Emission = color, EmissionEnergyMultiplier = 1.4f,
            },
        });
        return root;
    }

    public override void _Process(double delta)
    {
        _time += (float)delta;
        if (_spin is null || !IsInstanceValid(_spin)) return;
        _spin.Rotation = new Vector3(0f, _time * 1.4f, 0f);
        _spin.Position = _spin.Position with { Y = _restY + 0.08f * Mathf.Sin(_time * 2.2f + _phase) };
    }
}
