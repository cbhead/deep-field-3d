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
    /// <summary>The made roads — a network, not four stubs. Authored by design
    /// in its level file (docs/design/models/levels.js, "vroads") against the
    /// same clearance rule the client enforces, and adopted here because two
    /// of its corrections are real: the straight from (10, 22) to (35, 13) ran
    /// through the Vehickle house's north-west corner, 2 m of centreline inside
    /// the footprint and 5 m once the carriageway is counted, so the county
    /// road now bows through (16, 12); and its tail turns twice on the way to
    /// the south road for the same reason at the Grnmchn house. Vehicle
    /// handling reads grip, acceleration and top speed off these lines, so
    /// where a road runs is gameplay and what it is drawn with is not.</summary>
    public static readonly SurfaceRoad[] Roads =
    {
        // County road: north boundary, past the Vehickle drive, down to the south road.
        new(Surface.Asphalt, new[]
        {
            new Vector3(-54, 0, 80), new Vector3(-54, 0, 45), new Vector3(-36, 0, 54),
            new Vector3(0, 0, 54), new Vector3(10, 0, 22), new Vector3(16, 0, 12), new Vector3(35, 0, 13),
            new Vector3(52, 0, -14), new Vector3(64, 0, -42), new Vector3(70, 0, -58), new Vector3(72, 0, -66),
        }, 6f),
        // South road: a through road, so it runs off both edges.
        new(Surface.Asphalt, new[]
        {
            new Vector3(-160, 0, -66), new Vector3(-45, 0, -66), new Vector3(97, 0, -66), new Vector3(160, 0, -66),
        }, 6f),
        // Barn drive: the barn's vehicle door is on +Z, so the gravel runs up
        // the east flank and turns onto the apron — which is why the Gator can
        // be driven out of the shed rather than parked facing a wall.
        new(Surface.Gravel, new[]
        {
            new Vector3(-108, 0, 66), new Vector3(-95, 0, 66), new Vector3(-90, 0, 64), new Vector3(-88, 0, 58),
            new Vector3(-89, 0, 45), new Vector3(-72, 0, 47), new Vector3(-61, 0, 49), new Vector3(-54, 0, 45),
        }, 4f),
        // Buggy house spur: the only way to the west house, off the barn drive.
        new(Surface.Gravel, new[]
        {
            new Vector3(-124, 0, -1), new Vector3(-120, 0, 2), new Vector3(-112, 0, 14),
            new Vector3(-100, 0, 30), new Vector3(-89, 0, 45),
        }, 4f),
        // Grnmchn spur: off the county road's last bend to the -X front door,
        // approaching from the west so it never crosses the footprint.
        new(Surface.Gravel, new[] { new Vector3(70, 0, -58), new Vector3(75, 0, -52), new Vector3(78, 0, -50) }, 4f),
    };

    /// <summary>The Vehickle house's circular drive: twelve 30° arcs of
    /// asphalt round this centre, hollow in the middle.</summary>
    public static readonly Vector3 DriveCentre = new(24, 0, 4);
    public const float DriveRadius = 12f;
    public const float DriveWidth = 6f;

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
