namespace DeepField.Sim.Content;

public static class Maps
{
    /// <summary>M0 graybox: one S-curve ground lane, four tower sockets flanking it.
    /// Y is up; the lane runs on the ground plane (Y = 0).</summary>
    public static readonly MapDef TestLane = new(
        Id: "testlane",
        Route: new[]
        {
            new Vec3(-30f, 0f, 0f),
            new Vec3(-12f, 0f, 0f),
            new Vec3(-12f, 0f, 10f),
            new Vec3(8f, 0f, 10f),
            new Vec3(8f, 0f, -6f),
            new Vec3(30f, 0f, -6f),
        },
        Sockets: new[]
        {
            new SocketDef("s1", new Vec3(-16f, 0f, 5f)),
            new SocketDef("s2", new Vec3(-6f, 0f, 6f)),
            new SocketDef("s3", new Vec3(4f, 0f, 2f)),
            new SocketDef("s4", new Vec3(12f, 0f, -2f)),
        },
        HeroSpawn: new Vec3(0f, 0f, -14f),
        TotalWaves: 3);

    public static readonly IReadOnlyDictionary<string, MapDef> All =
        new Dictionary<string, MapDef>
        {
            [TestLane.Id] = TestLane,
        };
}
