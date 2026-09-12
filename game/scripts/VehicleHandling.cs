using Godot;
using System.Collections.Generic;

namespace DeepField.Game;

/// <summary>What a surface does to a vehicle: how hard its velocity is pulled
/// onto its heading, how fast it can accelerate, and how fast it will go.
/// Multipliers, so a vehicle's character is its own numbers and the ground only
/// bends them.</summary>
public sealed record SurfaceGrip(float Grip, float Accel, float TopSpeed);

/// <summary>The client's half of a vehicle.
///
/// None of this is in the sim, on purpose. The sim owns seats because two
/// clients can fight over one; it does not own motion, because motion has never
/// entered this sim and a handling model is a thing you tune by driving it
/// rather than by reasoning about it. The driver integrates and streams the
/// result, exactly as they already stream their own avatar.</summary>
public sealed record VehicleHandling(
    string Id,
    string Label,
    float TopSpeed,             // m/s on a 1.0 surface
    float ReverseSpeed,
    float Accel,                // m/s²
    float Brake,                // m/s² while braking a forward roll
    float Drag,                 // m/s² coasting with no input
    float SteerResponse,        // 1/s the steering answers the stick — low is vague
    float MaxYawRate,           // rad/s at full lock — low is a wide turn
    float SteerFullSpeed,       // m/s at which full yaw is available
    float HighSpeedSteerFade,   // 0..1 of that lost at top speed
    float Grip,                 // 1/s velocity is pulled onto the heading — low slides
    float HandbrakeGrip,        // multiplier with the handbrake down
    bool Differential,          // levers, not a wheel: yaw without rolling
    Vector3 BodySize,           // hull, width x height x length
    float SeatedEyeHeight,
    float WheelRadius,
    IReadOnlyDictionary<Surface, SurfaceGrip> Surfaces);

public static class VehicleHandlings
{
    private static Dictionary<Surface, SurfaceGrip> On(
        (float g, float a, float t) asphalt, (float g, float a, float t) gravel,
        (float g, float a, float t) grass, (float g, float a, float t) water) =>
        new()
        {
            [Surface.Asphalt] = new(asphalt.g, asphalt.a, asphalt.t),
            [Surface.Gravel] = new(gravel.g, gravel.a, gravel.t),
            [Surface.Grass] = new(grass.g, grass.a, grass.t),
            [Surface.Water] = new(water.g, water.a, water.t),
        };

    public static readonly IReadOnlyDictionary<string, VehicleHandling> All =
        new Dictionary<string, VehicleHandling>
        {
            // A road car with a rear engine and no idea what a field is. Medium
            // everything on tarmac and hopeless off it, and the handling is
            // deliberately the worst on the map: slow to answer the wheel
            // (SteerResponse) and a wide turn when it does (MaxYawRate). It is
            // the fastest way between two places that have a road between them
            // and the worst way anywhere else.
            ["buggy"] = new("buggy", "BUGGY",
                TopSpeed: 16f, ReverseSpeed: 5f, Accel: 5f, Brake: 9f, Drag: 2.5f,
                SteerResponse: 1.6f, MaxYawRate: 1.1f, SteerFullSpeed: 6f, HighSpeedSteerFade: 0.6f,
                Grip: 6f, HandbrakeGrip: 0.35f, Differential: false,
                BodySize: new Vector3(1.55f, 1.5f, 4.1f), SeatedEyeHeight: 1.15f, WheelRadius: 0.29f,
                Surfaces: On(asphalt: (1.0f, 1.0f, 1.0f), gravel: (0.7f, 0.75f, 0.8f),
                    grass: (0.5f, 0.5f, 0.55f), water: (0.3f, 0.2f, 0.15f))),

            // A utility vehicle: at home in a field, sluggish on tarmac, and
            // two things wrong with it on purpose. It will not turn in under a
            // barn's width (MaxYawRate), and its brakes are the weakest here —
            // which is only a problem because it is also quick off the mark.
            ["dagator"] = new("dagator", "DAGATOR",
                TopSpeed: 15f, ReverseSpeed: 5f, Accel: 7f, Brake: 4f, Drag: 3f,
                SteerResponse: 3f, MaxYawRate: 0.9f, SteerFullSpeed: 4f, HighSpeedSteerFade: 0.5f,
                Grip: 8f, HandbrakeGrip: 0.5f, Differential: false,
                BodySize: new Vector3(1.5f, 1.85f, 2.9f), SeatedEyeHeight: 1.0f, WheelRadius: 0.31f,
                Surfaces: On(asphalt: (1.0f, 0.85f, 0.85f), gravel: (1.0f, 1.0f, 1.0f),
                    grass: (0.95f, 1.0f, 1.0f), water: (0.4f, 0.3f, 0.2f))),

            // A drift trike. Steering is two levers rather than a wheel, so yaw
            // does not need forward roll at all (Differential) and it will spin
            // on the spot; and the back end is on plastic sleeves, so grip is a
            // third of anything else here and the rear steps out of every
            // corner whether or not that was the plan. Lowest seat on the map,
            // which is most of the joke.
            ["grnmchn"] = new("grnmchn", "GRNMCHN",
                TopSpeed: 11f, ReverseSpeed: 4f, Accel: 6f, Brake: 6f, Drag: 3f,
                SteerResponse: 6f, MaxYawRate: 2.6f, SteerFullSpeed: 0.5f, HighSpeedSteerFade: 0.2f,
                Grip: 2.2f, HandbrakeGrip: 0.6f, Differential: true,
                BodySize: new Vector3(0.9f, 0.9f, 1.9f), SeatedEyeHeight: 0.75f, WheelRadius: 0.25f,
                Surfaces: On(asphalt: (1.0f, 1.0f, 1.0f), gravel: (0.8f, 0.9f, 0.9f),
                    grass: (0.7f, 0.8f, 0.8f), water: (0.3f, 0.2f, 0.2f))),

            // A sport quad: the fastest thing here, the best brakes, the best
            // acceleration and the best steering — and knobbly tyres, which is
            // how "bad on roads" is expressed. Not a lower top speed on tarmac
            // but a third less grip, so it skates through a corner it would
            // have taken flat in a field. That is what an ATV does on a road,
            // and a speed cap would have said something else entirely.
            ["vehickle"] = new("vehickle", "VEHICKLE",
                TopSpeed: 22f, ReverseSpeed: 5f, Accel: 10f, Brake: 10f, Drag: 3f,
                SteerResponse: 5f, MaxYawRate: 1.9f, SteerFullSpeed: 3f, HighSpeedSteerFade: 0.45f,
                Grip: 11f, HandbrakeGrip: 0.3f, Differential: false,
                BodySize: new Vector3(1.16f, 1.13f, 1.85f), SeatedEyeHeight: 1.05f, WheelRadius: 0.28f,
                Surfaces: On(asphalt: (0.6f, 0.8f, 0.85f), gravel: (1.0f, 1.0f, 1.0f),
                    grass: (1.0f, 1.0f, 1.0f), water: (0.4f, 0.3f, 0.2f))),
        };

    public static VehicleHandling For(string defId) =>
        All.TryGetValue(defId, out var handling) ? handling : All["buggy"];
}
