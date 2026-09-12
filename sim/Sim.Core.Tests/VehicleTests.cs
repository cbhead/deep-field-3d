using System.Linq;
using DeepField.Sim;
using DeepField.Sim.Content;
using Xunit;

/// <summary>Vehicles, and the one half of them the sim owns.
///
/// Movement is the driver's client's business, exactly as an avatar's is. A
/// seat is not: it is the only part two clients can usefully disagree about, so
/// every rule below is about who is allowed to be sitting where, and about the
/// seat surviving the four ways a player leaves the world without saying so.</summary>
public class VehicleTests
{
    private const string Van = "van";
    private const string Quad = "quad";

    /// <summary>Testlane with two vehicles parked on it: a two-seater and a
    /// single. Keeps the map id so the wave tables resolve.</summary>
    private static MapDef Yard() => Maps.TestLane with
    {
        VehiclesOrNull = new[]
        {
            new VehicleSpawnDef(Van, "buggy", new Vec3(0f, 0f, -14f), 90f),
            new VehicleSpawnDef(Quad, "vehickle", new Vec3(20f, 0f, -14f), 0f),
        },
    };

    private static World Party(int players = 2)
    {
        var w = new World(11, Yard());
        var factions = new[] { "ember", "forge", "tempest" };
        for (int i = 0; i < players; i++)
            w.Enqueue(new Command.Join(i + 1, $"p{i + 1}", factions[i]));
        Step.Advance(w);
        // Standing at the van, which is where boarding is judged from.
        foreach (var player in w.Players.Values) player.Pos = new Vec3(0f, 0f, -13f);
        return w;
    }

    private static string? Refusal(World w) =>
        w.Events.OfType<SimEvent.VehicleRejected>().LastOrDefault()?.Reason;

    [Fact]
    public void AMapParksItsVehiclesWithEverySeatEmpty()
    {
        var w = new World(11, Yard());
        var van = w.Vehicles.Single(v => v.Id == Van);
        Assert.Equal(2, van.Seats.Length);
        Assert.Single(w.Vehicles.Single(v => v.Id == Quad).Seats);
        Assert.All(van.Seats, occupant => Assert.Equal(0, occupant));
        Assert.Equal(new Vec3(0f, 0f, -14f), van.Pos);
        // Maps that never heard of vehicles carry none, rather than null.
        Assert.Empty(new World(11, Maps.Foundry).Vehicles);
    }

    [Fact]
    public void TakingASeatTakesIt()
    {
        var w = Party();
        w.Enqueue(new Command.EnterVehicle(1, Van, 0));
        Step.Advance(w);

        Assert.Equal(1, w.Vehicles.Single(v => v.Id == Van).Seats[0]);
        Assert.Equal(1, w.Vehicles.Single(v => v.Id == Van).DriverId);
        var entered = w.Events.OfType<SimEvent.VehicleEntered>().Single();
        Assert.Equal(0, entered.SeatIndex);
    }

    [Fact]
    public void TwoPlayersCannotShareOneSeat()
    {
        var w = Party();
        w.Enqueue(new Command.EnterVehicle(1, Van, 0));
        Step.Advance(w);
        w.Enqueue(new Command.EnterVehicle(2, Van, 0));
        Step.Advance(w);

        Assert.Equal("seatTaken", Refusal(w));
        Assert.Equal(1, w.Vehicles.Single(v => v.Id == Van).Seats[0]);

        // The other seat is still free, which is the point of a two-seater.
        w.Enqueue(new Command.EnterVehicle(2, Van, 1));
        Step.Advance(w);
        Assert.Equal(2, w.Vehicles.Single(v => v.Id == Van).Seats[1]);
    }

    [Fact]
    public void YouHaveToBeStandingNextToIt()
    {
        var w = Party();
        w.Players[1].Pos = new Vec3(30f, 0f, 30f);
        w.Enqueue(new Command.EnterVehicle(1, Van, 0));
        Step.Advance(w);

        Assert.Equal("notNear", Refusal(w));
        Assert.Equal(0, w.Vehicles.Single(v => v.Id == Van).Seats[0]);
    }

    [Fact]
    public void OneSeatPerPlayerAndTheSeatHasToExist()
    {
        var w = Party();
        w.Enqueue(new Command.EnterVehicle(1, Van, 0));
        Step.Advance(w);

        w.Players[1].Pos = new Vec3(20f, 0f, -13f);
        w.Enqueue(new Command.EnterVehicle(1, Quad, 0));
        Step.Advance(w);
        Assert.Equal("alreadySeated", Refusal(w));

        w.Enqueue(new Command.EnterVehicle(2, Quad, 1));   // a quad has one saddle
        Step.Advance(w);
        Assert.Equal("badSeat", Refusal(w));

        w.Enqueue(new Command.EnterVehicle(2, "tractor", 0));
        Step.Advance(w);
        Assert.Equal("unknownVehicle", Refusal(w));
    }

    [Fact]
    public void ADownedPlayerCannotBoard()
    {
        var w = Party();
        w.Players[1].Downed = true;
        w.Enqueue(new Command.EnterVehicle(1, Van, 0));
        Step.Advance(w);

        Assert.Equal("downed", Refusal(w));
    }

    [Fact]
    public void GettingOutFreesTheSeat()
    {
        var w = Party();
        w.Enqueue(new Command.EnterVehicle(1, Van, 0));
        Step.Advance(w);
        w.Enqueue(new Command.ExitVehicle(1));
        Step.Advance(w);

        Assert.Equal(0, w.Vehicles.Single(v => v.Id == Van).Seats[0]);
        Assert.Equal("left", w.Events.OfType<SimEvent.VehicleExited>().Single().Reason);

        // And asking to get out of nothing says nothing.
        w.Enqueue(new Command.ExitVehicle(1));
        Step.Advance(w);
        Assert.Empty(w.Events.OfType<SimEvent.VehicleExited>());
    }

    [Fact]
    public void OnlyTheDriverMovesIt()
    {
        var w = Party();
        w.Enqueue(new Command.EnterVehicle(1, Van, 0));
        w.Enqueue(new Command.EnterVehicle(2, Van, 1));
        Step.Advance(w);
        var van = w.Vehicles.Single(v => v.Id == Van);

        w.Enqueue(new Command.VehicleSync(2, Van, new Vec3(99f, 0f, 99f), 10f));
        Step.Advance(w);
        Assert.Equal(new Vec3(0f, 0f, -14f), van.Pos);

        w.Enqueue(new Command.VehicleSync(1, Van, new Vec3(8f, 0f, -9f), 45f));
        Step.Advance(w);
        Assert.Equal(new Vec3(8f, 0f, -9f), van.Pos);
        Assert.Equal(45f, van.YawDegrees);
    }

    [Fact]
    public void DisconnectingEmptiesTheSeat()
    {
        var w = Party();
        w.Enqueue(new Command.EnterVehicle(1, Van, 0));
        Step.Advance(w);
        w.Enqueue(new Command.Leave(1));
        Step.Advance(w);

        // A held seat on a driver who is gone is a vehicle nobody can use again.
        Assert.Equal(0, w.Vehicles.Single(v => v.Id == Van).Seats[0]);
        Assert.Equal("disconnected", w.Events.OfType<SimEvent.VehicleExited>().Single().Reason);
    }

    [Fact]
    public void GoingDownEmptiesTheSeat()
    {
        var w = Party();
        w.Enqueue(new Command.EnterVehicle(1, Van, 0));
        Step.Advance(w);

        // Something with contact damage, walking over them. The enemy's
        // position is recomputed from its route every tick, so the player is
        // put on the lane rather than the enemy on the player.
        var def = Enemies.All["drifter"];
        w.Enemies.Add(new Enemy
        {
            Id = w.NextId(), DefId = def.Id, Hp = def.Hp, MaxHp = def.Hp,
        });
        w.Players[1].Pos = w.Map.Routes[0].Waypoints[0];
        w.Players[1].Hp = 1f;
        for (int i = 0; i < 30 && !w.Players[1].Downed && w.Players[1].RespawnTimer <= 0f; i++)
            Step.Advance(w);

        Assert.True(w.Players[1].Downed || w.Players[1].RespawnTimer > 0f);
        Assert.Equal(0, w.Vehicles.Single(v => v.Id == Van).Seats[0]);
        Assert.Contains(w.Events.OfType<SimEvent.VehicleExited>(), e => e.Reason == "downed");
    }

    [Fact]
    public void ASeatedPlayerStillStreamsTheirOwnPosition()
    {
        // Contact damage, revive range and scrap pickup all read PlayerState.Pos,
        // so a passenger being carried has to keep reporting where the seat is.
        var w = Party();
        w.Enqueue(new Command.EnterVehicle(1, Van, 0));
        Step.Advance(w);
        w.Enqueue(new Command.PlayerSync(1, new Vec3(4f, 1f, -6f)));
        Step.Advance(w);

        Assert.Equal(new Vec3(4f, 1f, -6f), w.Players[1].Pos);
    }

    [Fact]
    public void SeatsAndPositionsSurviveASave()
    {
        // On a shipped map, because a resumed world rebuilds its vehicles from
        // the map in the registry — a fixture map assembled in this file is not
        // in it, and a save of one would come back with nothing parked.
        var w = new World(11, Maps.Toaster);
        w.Enqueue(new Command.Join(1, "p1", "ember"));
        w.Enqueue(new Command.Join(2, "p2", "forge"));
        Step.Advance(w);
        var buggy = w.Vehicles.First(v => v.DefId == "buggy");
        foreach (var player in w.Players.Values) player.Pos = buggy.Pos;
        w.Enqueue(new Command.EnterVehicle(1, buggy.Id, 0));
        w.Enqueue(new Command.EnterVehicle(2, buggy.Id, 1));
        Step.Advance(w);
        w.Enqueue(new Command.VehicleSync(1, buggy.Id, new Vec3(3f, 0f, -4f), 33f));
        Step.Advance(w);

        var resumed = Serialization.Deserialize(Serialization.Serialize(w));
        var loaded = resumed.Vehicles.Single(v => v.Id == buggy.Id);
        Assert.Equal(new[] { 1, 2 }, loaded.Seats);
        Assert.Equal(new Vec3(3f, 0f, -4f), loaded.Pos);
        Assert.Equal(33f, loaded.YawDegrees);
        Assert.Equal(1, resumed.SeatOf(1)!.Value.Vehicle.DriverId);
    }

    [Fact]
    public void ASaveFromBeforeVehiclesExistedStillLoads()
    {
        var w = new World(11, Maps.Toaster);
        var doc = System.Text.Json.Nodes.JsonNode.Parse(Serialization.Serialize(w))!.AsObject();
        // A file written by a build that had never heard of vehicles simply
        // has no such property, and the map parks them all over again.
        Assert.True(doc.Remove("Vehicles"));
        var resumed = Serialization.Deserialize(doc.ToJsonString());

        Assert.Equal(Maps.Toaster.Vehicles.Count, resumed.Vehicles.Count);
        Assert.All(resumed.Vehicles, v => Assert.All(v.Seats, seat => Assert.Equal(0, seat)));
    }
}
