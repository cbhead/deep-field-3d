using Godot;
using System.Collections.Generic;

namespace DeepField.Game;

/// <summary>What a vehicle is driving on.
///
/// Read from the map's authored roads and pond rather than from the meshes
/// under the wheels: the roads are props with no collision, a physics query per
/// wheel per frame would cost more than the whole vehicle, and a surface that
/// disagrees between two clients is a vehicle that arrives in two places. A
/// dozen segment tests against authored data is cheap, exact, and the same
/// answer everywhere.</summary>
public enum Surface
{
    Asphalt,
    Gravel,
    Grass,
    Water,
}

public sealed record SurfaceRoad(Surface Kind, Vector3[] Points, float Width);

public static class Surfaces
{
    /// <summary>Set per map by the level builder; empty means the whole field
    /// is grass, which is what every map before the Toaster is.</summary>
    public static IReadOnlyList<SurfaceRoad> Roads = System.Array.Empty<SurfaceRoad>();
    public static (Vector3 Centre, float Radius)? Water;
    /// <summary>A circular drive is an annulus of asphalt: the middle of it is
    /// grass, which is why it is not a road with a very short segment list.</summary>
    public static (Vector3 Centre, float Radius, float Width)? Drive;

    public static void Clear()
    {
        Roads = System.Array.Empty<SurfaceRoad>();
        Water = null;
        Drive = null;
    }

    public static Surface At(Vector3 position)
    {
        var flat = new Vector2(position.X, position.Z);
        if (Water is { } water
            && flat.DistanceTo(new Vector2(water.Centre.X, water.Centre.Z)) < water.Radius)
            return Surface.Water;

        if (Drive is { } drive
            && Mathf.Abs(flat.DistanceTo(new Vector2(drive.Centre.X, drive.Centre.Z)) - drive.Radius) <= drive.Width * 0.5f)
            return Surface.Asphalt;

        // Asphalt is listed first, so where the gravel drive meets the road the
        // road wins — which is what that junction looks like.
        foreach (var road in Roads)
            for (int i = 0; i < road.Points.Length - 1; i++)
                if (DistanceToSegment(flat,
                        new Vector2(road.Points[i].X, road.Points[i].Z),
                        new Vector2(road.Points[i + 1].X, road.Points[i + 1].Z))
                    <= road.Width * 0.5f) return road.Kind;

        return Surface.Grass;
    }

    /// <summary>Within <paramref name="margin"/> of any made road's carriageway
    /// edge. Scenery uses it to stay off the road; a fence line or a mailbox
    /// may stand at the kerb (margin 0), a shrub may not (margin 2) — a shrub
    /// on asphalt is the same mistake as an oak in it.</summary>
    public static bool NearRoad(Vector3 position, float margin)
    {
        var flat = new Vector2(position.X, position.Z);
        foreach (var road in Roads)
            for (int i = 0; i < road.Points.Length - 1; i++)
                if (DistanceToSegment(flat,
                        new Vector2(road.Points[i].X, road.Points[i].Z),
                        new Vector2(road.Points[i + 1].X, road.Points[i + 1].Z))
                    <= road.Width * 0.5f + margin) return true;
        if (Drive is { } drive
            && Mathf.Abs(flat.DistanceTo(new Vector2(drive.Centre.X, drive.Centre.Z)) - drive.Radius) <= drive.Width * 0.5f + margin)
            return true;
        return false;
    }

    public static float DistanceToSegment(Vector2 point, Vector2 a, Vector2 b)
    {
        var ab = b - a;
        float lengthSq = ab.LengthSquared();
        if (lengthSq < 0.0001f) return point.DistanceTo(a);
        float t = Mathf.Clamp((point - a).Dot(ab) / lengthSq, 0f, 1f);
        return point.DistanceTo(a + ab * t);
    }
}
