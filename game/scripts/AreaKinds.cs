namespace DeepField.Game;

/// <summary>Every `kind` string an <c>Area3D</c> built by the level can carry,
/// in one place, plus the two facts about each that used to be implicit and
/// disagreed.
///
/// The bug this file exists to make impossible: the map validator's §4.1
/// ("every tier carrying a socket is reachable") credited `elevator` as a way
/// up, and **no handler for `elevator` has ever existed** — `Player` reads
/// `ladder`, `zipline`, `launcher`, `teleporter`, `armory` and nothing else. So
/// a tier served only by a lift measured as reachable and was not, which is the
/// same defect as the ladder that topped out under its own deck, except that
/// this one was being actively certified by the check written to catch it.
///
/// `nest` was the other half: an `Area3D` with no handler, no hint and no
/// effect — a floor with a name. The mesh is dressing and stays; the area is
/// gone.
///
/// So: <see cref="Traversal"/> is what the validator may count as a way up, and
/// <see cref="Handled"/> is what <c>Player</c> actually does something about.
/// The validator asserts the first is a subset of the second, which means a
/// kind cannot be credited for reachability unless the player can use it.</summary>
public static class AreaKinds
{
    public const string Ladder = "ladder";
    public const string Zipline = "zipline";
    public const string Launcher = "launcher";
    public const string Teleporter = "teleporter";
    public const string Elevator = "elevator";
    public const string Armory = "armory";
    public const string ControlPoint = "controlPoint";

    /// <summary>Kinds the validator may count as getting a player to a tier.
    /// Every one of these must also be in <see cref="Handled"/>.</summary>
    public static readonly string[] Traversal =
        { Ladder, Zipline, Launcher, Teleporter, Elevator };

    /// <summary>Kinds <c>Player</c> reads and acts on. `armory` and
    /// `controlPoint` are handled but are not traversal — they get you nothing
    /// vertically.</summary>
    public static readonly string[] Handled =
        { Ladder, Zipline, Launcher, Teleporter, Elevator, Armory, ControlPoint };

    /// <summary>Meta key on an <see cref="Elevator"/> area: the **floor height**
    /// its car rises to — the Y a player stands at when it arrives, not the Y
    /// of the car body or of the area. See <c>Player.FloorToOrigin</c> for why
    /// that sentence is worth writing down.
    ///
    /// Authored rather than derived from the shaft, because a shaft that
    /// passes through a solid floor — which is exactly what the Spire's did —
    /// is a lift that serves nothing, and the map file should have to say so
    /// rather than the validator assuming the top of the tube is a destination.</summary>
    public const string LiftTopMeta = "lift_top";

    /// <summary>Meta key on an <see cref="Elevator"/> area: the floor height
    /// its car rests at, on the same terms as <see cref="LiftTopMeta"/>.</summary>
    public const string LiftBottomMeta = "lift_bottom";
}
