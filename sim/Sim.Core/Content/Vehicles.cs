namespace DeepField.Sim.Content;

/// <summary>A drivable vehicle, as much of one as the sim needs to know about.
///
/// Seats are the sim's business: two players must not both take the wheel, and
/// a seat has to survive a disconnect, a down and a resume. Motion is not — the
/// driver's client integrates the thing and streams the result, exactly as it
/// streams the driver's own avatar, so handling (acceleration, steering rate,
/// grip per surface) lives in the client's table where it can be tuned by feel
/// without touching a deterministic file.
///
/// MaxSpeedMetersPerSec is kept here anyway, as the one number a later
/// plausibility clamp on VehicleSync would need something to clamp against.
/// BoardRadiusMeters is the reach check on EnterVehicle: the same trust
/// PlayerHit and Revive extend to a client, and the same slack.</summary>
public sealed record VehicleDef(
    string Id,
    int Seats,
    float BoardRadiusMeters = 4f,
    float MaxSpeedMetersPerSec = 14f);

public static class Vehicles
{
    /// <summary>A road car with two seats. Quick enough on tarmac and hopeless
    /// off it; the client's grip table is where that lives.</summary>
    public static readonly VehicleDef Buggy = new("buggy", Seats: 2, MaxSpeedMetersPerSec: 16f);

    /// <summary>A utility vehicle: two abreast and a bed, at home in a field.</summary>
    public static readonly VehicleDef Dagator = new("dagator", Seats: 2, MaxSpeedMetersPerSec: 15f);

    /// <summary>A drift trike. One seat, because there is nowhere to put a
    /// second person that is not the floor.</summary>
    public static readonly VehicleDef Grnmchn = new("grnmchn", Seats: 1, MaxSpeedMetersPerSec: 11f);

    /// <summary>A sport quad: the fastest thing on the map and a single saddle.</summary>
    public static readonly VehicleDef Vehickle = new("vehickle", Seats: 1, MaxSpeedMetersPerSec: 22f);

    public static readonly IReadOnlyDictionary<string, VehicleDef> All =
        new Dictionary<string, VehicleDef>
        {
            [Buggy.Id] = Buggy,
            [Dagator.Id] = Dagator,
            [Grnmchn.Id] = Grnmchn,
            [Vehickle.Id] = Vehickle,
        };
}
