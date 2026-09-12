using Godot;

namespace DeepField.Game;

/// <summary>The Toaster's authored geography, in one place because three
/// separate things read it: the level builder draws it, the surface lookup
/// answers vehicle handling from it, and the teleport network is a list of
/// positions on it. Sim coordinates, metres, +X east, +Z north.
///
/// The routes, sockets and vehicle parking live in Maps.Toaster; everything
/// here is the property the map is drawn on rather than the game played on it.</summary>
public static class ToasterLayout
{
    /// <summary>The made roads. A vehicle's grip, acceleration and top speed
    /// are read off these, so where a road runs is gameplay and what it is
    /// drawn with is not.</summary>
    public static readonly SurfaceRoad[] Roads =
    {
        // The county road in from the north, then the loop east past the house.
        new(Surface.Asphalt, new[]
        {
            new Vector3(-54, 0, 68), new Vector3(-54, 0, 45), new Vector3(-36, 0, 54),
            new Vector3(0, 0, 54), new Vector3(10, 0, 22), new Vector3(35, 0, 13),
        }, 6f),
        // The south road along the bottom of the property.
        new(Surface.Asphalt, new[] { new Vector3(-45, 0, -66), new Vector3(97, 0, -66) }, 6f),
        // The gravel drive up to the barn.
        new(Surface.Gravel, new[] { new Vector3(-89, 0, 45), new Vector3(-61, 0, 49) }, 4f),
    };

    /// <summary>Thirty-four metres across rather than the forty-odd it is on
    /// the ground: three routes pass within twenty-two metres of its centre,
    /// and a bank that comes closer than the lane's own width to any of them
    /// is a wall across the road, not scenery.</summary>
    public static readonly Vector3 PondCentre = new(19, 0, -36);
    public const float PondRadius = 17f;

    /// <summary>The player teleport network. Three pads are inside buildings
    /// and the fourth is at the core, which is the one place a player always
    /// wants to be able to get back to.</summary>
    public static readonly (string Id, string Label, Vector3 At)[] Pads =
    {
        ("padCore", "THE CORE", new Vector3(-52, 0, -14)),
        ("padBuggy", "BUGGY HOUSE", new Vector3(-134, 0, -12)),
        ("padVehickle", "VEHICKLE HOUSE", new Vector3(23, 0, 31)),
        ("padGrnmchn", "GRNMCHN HOUSE", new Vector3(86, 0, -48)),
    };

    /// <summary>How deep the treeline is, and therefore where the invisible
    /// wall stands: at the inner edge of it, with the wood behind as the
    /// visible reason the world ends.</summary>
    public const float BeltDepth = 12f;
}
