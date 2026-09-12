using Godot;
using System.Collections.Generic;

namespace DeepField.Game;

public partial class GameRoot
{
    /// <summary>Which wall of a building, in lane terms: north is -Z, the same
    /// convention every heading in this client uses.</summary>
    private enum Side { North, South, East, West }

    /// <summary>An opening in a wall, measured from the centre of that wall
    /// along it. There is no door mechanic and no door: a door that closes is
    /// a wall, and the sim cannot model one that opens.</summary>
    private sealed record Door(Side Side, float Offset, float Width = 2.4f, float Height = 2.6f);

    /// <summary>A building a player can walk into and stand on top of.
    ///
    /// Every map before this one was decks over a yard, so "a volume" meant a
    /// slab and the client had no idea what an inside was. A building is the
    /// same graybox idea taken round four sides: a floor, wall runs split
    /// around each opening with a lintel closing the gap above it, and a roof
    /// that is a walkable surface rather than a lid — which is what lets a wall
    /// socket sit up there and a ladder serve it.
    ///
    /// The whole thing stays AddStaticBox, so the validator sees real volumes,
    /// the traversal probe can climb it, and a delivered shell is hung over the
    /// same colliders exactly as a deck module is hung over a deck.</summary>
    private void BuildHouse(string name, Vector3 centre, float width, float depth,
        float wallHeight, IReadOnlyList<Door> doors, Side ladderSide, float ladderOffset,
        string? shellAsset = null, string? roofAsset = null)
    {
        const float thickness = 0.3f;
        var wallColor = new Color(0.52f, 0.49f, 0.44f);
        var roofColor = new Color(0.38f, 0.36f, 0.34f);
        var made = new List<StaticBody3D>();

        // Floor: a low plinth rather than a step, so the capsule's rounded base
        // walks onto it without any step-up logic to get wrong.
        made.Add(AddStaticBox(centre + new Vector3(0, 0.075f, 0),
            new Vector3(width, 0.15f, depth), wallColor.Darkened(0.15f), layer: 1));

        // Roof: walkable, which is the point. Wall sockets go on it, the ladder
        // tops out level with it, and on a map with no decks it is the only
        // tier there is.
        float roofY = wallHeight + 0.15f;
        made.Add(AddStaticBox(centre + new Vector3(0, roofY, 0),
            new Vector3(width + thickness, 0.3f, depth + thickness), roofColor, layer: 1));

        foreach (var side in new[] { Side.North, Side.South, Side.East, Side.West })
        {
            bool alongX = side is Side.North or Side.South;
            float length = alongX ? width : depth;
            var wallCentre = centre + side switch
            {
                Side.North => new Vector3(0, 0, -depth * 0.5f),
                Side.South => new Vector3(0, 0, depth * 0.5f),
                Side.East => new Vector3(width * 0.5f, 0, 0),
                _ => new Vector3(-width * 0.5f, 0, 0),
            };

            var openings = new List<Door>();
            foreach (var door in doors) if (door.Side == side) openings.Add(door);
            openings.Sort((a, b) => a.Offset.CompareTo(b.Offset));

            float cursor = -length * 0.5f;
            foreach (var door in openings)
            {
                float left = Mathf.Max(cursor, door.Offset - door.Width * 0.5f);
                float right = Mathf.Min(length * 0.5f, door.Offset + door.Width * 0.5f);
                WallRun(made, wallCentre, alongX, cursor, left, 0f, wallHeight, thickness, wallColor);
                // The wall above the opening. Without it a doorway reads as a
                // slot cut from the ground to the eaves.
                WallRun(made, wallCentre, alongX, left, right, door.Height, wallHeight, thickness, wallColor);
                cursor = right;
            }
            WallRun(made, wallCentre, alongX, cursor, length * 0.5f, 0f, wallHeight, thickness, wallColor);
        }

        // A way up, outside, topping out level with the roof rather than under
        // it — the one mistake this project has made three times.
        bool ladderAlongX = ladderSide is Side.North or Side.South;
        var ladderAt = centre + ladderSide switch
        {
            Side.North => new Vector3(ladderOffset, 0, -depth * 0.5f - 0.5f),
            Side.South => new Vector3(ladderOffset, 0, depth * 0.5f + 0.5f),
            Side.East => new Vector3(width * 0.5f + 0.5f, 0, ladderOffset),
            _ => new Vector3(-width * 0.5f - 0.5f, 0, ladderOffset),
        };
        float climb = roofY + 0.4f;
        var rungs = AddStaticBox(ladderAt + new Vector3(0, climb * 0.5f, 0),
            ladderAlongX ? new Vector3(1.2f, climb, 0.15f) : new Vector3(0.15f, climb, 1.2f),
            new Color(0.7f, 0.6f, 0.3f), layer: 0);
        rungs.AddChild(MakeArea("ladder", new BoxShape3D { Size = new Vector3(1.8f, climb + 1.4f, 1.8f) }));
        MapKit.Mount(rungs, "shared_ladder", MapKit.GroundLocal(rungs),
            scale: new Vector3(1f, climb / 6f, 1f));

        // Scenery, scatter and the tree belt stay out of the building. Nothing
        // else on a map has an inside, so Blocked could not have known.
        _footprints.Add(new Rect2(centre.X - width * 0.5f - 2f, centre.Z - depth * 0.5f - 2f,
            width + 4f, depth + 4f));

        // One authored shell over the whole graybox, the deck-module rule: the
        // colliders stay exactly as they are and only the boxes stop drawing.
        if (shellAsset is not null && MapKit.Prop(this, shellAsset, centre) is not null)
            foreach (var body in made) MapKit.HideBox(body);
        if (roofAsset is not null)
            MapKit.Prop(this, roofAsset, centre + new Vector3(0, roofY + 0.15f, 0));
        GD.Print($"[map] {name}: {made.Count} volume(s), roof at {roofY:0.0} m");
    }

    /// <summary>One stretch of wall between two offsets along its side, from
    /// <paramref name="fromY"/> up to the eaves. Skipped when the stretch has
    /// no width, which is what an opening at the very end of a wall leaves.</summary>
    private void WallRun(List<StaticBody3D> made, Vector3 wallCentre, bool alongX,
        float from, float to, float fromY, float toY, float thickness, Color color)
    {
        float span = to - from;
        if (span < 0.05f || toY - fromY < 0.05f) return;
        float mid = (from + to) * 0.5f;
        var at = wallCentre + (alongX ? new Vector3(mid, 0, 0) : new Vector3(0, 0, mid))
            + new Vector3(0, (fromY + toY) * 0.5f, 0);
        var size = alongX
            ? new Vector3(span, toY - fromY, thickness)
            : new Vector3(thickness, toY - fromY, span);
        made.Add(AddStaticBox(at, size, color, layer: 1));
    }
}
