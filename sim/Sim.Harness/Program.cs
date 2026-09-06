using DeepField.Harness;
using DeepField.Sim;
using DeepField.Sim.Content;

// The gate suite: every check prints PASS/FAIL; a single failure fails the run.
// This file is the M0 seed of the 2D game's 671-gate culture — gates only grow.

int failures = 0;

void Gate(string name, bool ok, string detail = "")
{
    Console.WriteLine($"{(ok ? "PASS" : "FAIL")}  {name}{(detail.Length > 0 ? $"  ({detail})" : "")}");
    if (!ok) failures++;
}

const uint Seed = 20260906;

// --- Gate 1: determinism — same seed + same bot => byte-identical event log, twice.
{
    var a = MatchRunner.Run(Seed, new PlayerBot(Seed) { PlayerId = 1, Accuracy = 0.6f, Uptime = 0.8f });
    var b = MatchRunner.Run(Seed, new PlayerBot(Seed) { PlayerId = 1, Accuracy = 0.6f, Uptime = 0.8f });
    Gate("determinism: identical event logs across runs", a.EventLogHash == b.EventLogHash,
        $"hash {a.EventLogHash[..12]}");
}

// --- Gate 2: conservation — every spawned enemy is killed or leaked, never lost.
{
    var r = MatchRunner.Run(Seed, new PlayerBot(Seed) { PlayerId = 1, Accuracy = 0.6f, Uptime = 0.8f });
    Gate("conservation: spawned == kills + leaks",
        r.Spawned == r.TowerKills + r.PlayerKills + r.Leaked,
        $"spawned {r.Spawned} = towers {r.TowerKills} + player {r.PlayerKills} + leaked {r.Leaked}");
}

// --- Gate 3: the mid-band player + greedy towers clears the M0 campaign.
{
    var r = MatchRunner.Run(Seed, new PlayerBot(Seed) { PlayerId = 1, Accuracy = 0.6f, Uptime = 0.8f });
    Gate("mid-band bot + towers achieve victory", r.Victory,
        $"waves {r.WavesCleared}/{Maps.TestLane.TotalWaves}, lives {r.LivesLeft}");
}

// --- Gate 4: towers-only holds the floor (no player shooting at all).
{
    var r = MatchRunner.Run(Seed, null);
    Gate("towers-only survives (floor)", r.Victory && r.LivesLeft > 0,
        $"waves {r.WavesCleared}, lives {r.LivesLeft}, leaked {r.Leaked}");
}

// --- Gate 5: the player matters — skill band shifts the outcome measurably.
{
    var low = MatchRunner.Run(Seed, new PlayerBot(Seed) { PlayerId = 1, Accuracy = 0.4f, Uptime = 0.6f });
    var high = MatchRunner.Run(Seed, new PlayerBot(Seed) { PlayerId = 1, Accuracy = 0.8f, Uptime = 0.9f });
    Gate("skill band: high-skill bot out-kills low-skill bot", high.PlayerKills > low.PlayerKills,
        $"high {high.PlayerKills} vs low {low.PlayerKills}");
}

// --- Gate 6: serialization round-trip — resume mid-match, identical continuation.
{
    var world = new World(Seed, Maps.TestLane);
    world.Enqueue(new Command.PlaceTower(0, Towers.Lance.Id, "s2"));
    for (int i = 0; i < Balance.TickHz * 20; i++) Step.Advance(world);

    var resumed = Serialization.Deserialize(Serialization.Serialize(world));

    var logA = new List<string>();
    var logB = new List<string>();
    for (int i = 0; i < Balance.TickHz * 10; i++)
    {
        Step.Advance(world);
        logA.AddRange(world.Events.Select(e => e.LogLine()));
        Step.Advance(resumed);
        logB.AddRange(resumed.Events.Select(e => e.LogLine()));
    }
    Gate("serialization: resumed world continues identically",
        MatchRunner.HashLog(logA) == MatchRunner.HashLog(logB),
        $"{logA.Count} events compared");
}

// --- Gate 7: refusal paths answer out loud.
{
    var world = new World(Seed, Maps.TestLane);
    world.Enqueue(new Command.PlaceTower(0, Towers.Lance.Id, "nope"));
    world.Enqueue(new Command.PlaceTower(0, "phantom", "s1"));
    Step.Advance(world);
    var reasons = world.Events.OfType<SimEvent.BuildRejected>().Select(e => e.Reason).ToList();
    Gate("refusals: unknown socket + unknown tower rejected with reasons",
        reasons.Contains("unknownSocket") && reasons.Contains("unknownTower"),
        string.Join(",", reasons));
}

Console.WriteLine();
Console.WriteLine(failures == 0 ? "ALL GATES GREEN" : $"{failures} GATE(S) FAILED");
return failures == 0 ? 0 : 1;
