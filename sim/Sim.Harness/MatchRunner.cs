using System.Security.Cryptography;
using System.Text;
using DeepField.Sim;
using DeepField.Sim.Content;

namespace DeepField.Harness;

public sealed record MatchResult(
    bool Victory,
    int WavesCleared,
    int LivesLeft,
    int Spawned,
    int TowerKills,
    int PlayerKills,
    int Leaked,
    int Reactions,
    long Ticks,
    Dictionary<ScrapType, int> TeamScrapEnd,
    string EventLogHash,
    List<string> EventLog);

/// <summary>Runs a full seeded match headlessly with a scripted builder and any
/// number of PlayerBots — the M1 seed of campaign.ts/sweep.ts.</summary>
public static class MatchRunner
{
    private const long MaxTicks = Balance.TickHz * 1800; // 30 min safety cap

    /// <summary>Deterministic build orders per map: "tower:socket" placed as
    /// money allows, then damage-path upgrades with the surplus. A floor policy,
    /// not a forecast — same doctrine as the 2D greedy builder.</summary>
    /// <summary>The switchyard floor policy with its Detector removed, for the
    /// conditions gate. Same list, same order, one tower missing — so any
    /// difference in outcome is attributable to that tower and nothing else.</summary>
    public static string[] SwitchyardWithoutDetector =>
        BuildOrders["switchyard"].Where(e => !e.StartsWith("detector:")).ToArray();

    private static readonly Dictionary<string, string[]> BuildOrders = new()
    {
        ["testlane"] = new[] { "lance:s2", "lance:s3", "lance:s1", "lance:s4" },
        ["foundry"] = new[]
        {
            "lance:g2", "lance:g5", "skywatch:g4", "singularity:g3",
            "nova:g1", "skywatch:w2", "lance:g6", "lance:w1",
        },
        ["switchyard"] = new[]
        {
            // Air moved up: a Skywatch parked mid-map on the ground can no
            // longer touch the strand's high middle, so the scripted team buys
            // its first anti-air at the west mouth where the lane still dips
            // low, and covers the middle from the catwalk.
            "lance:g3", "skywatch:g8", "arc:g6", "skywatch:w3",
            "barricade:b1", "nova:g4", "skywatch:w9",
            // Shade debuts at W9 and nothing already on this list can see it,
            // so the floor policy buys eyes before it needs them — mid-map,
            // on the stretch of ground route its field covers most (29%).
            "detector:g14", "singularity:g2", "lance:w1",
            "tar:t2", "nova:g7", "lance:w2", "lance:g1",
            "spike:t4", "skywatch:w4",
        },
    };

    public static MatchResult Run(uint seed, MapDef map, params PlayerBot[] bots) =>
        RunWithBuild(seed, map, null, bots);

    /// <summary>As above, but with the floor policy's build list overridden —
    /// how the harness asks "what does this tower actually buy us?" by running
    /// the same seed, map and bot with and without it.</summary>
    public static MatchResult RunWithBuild(uint seed, MapDef map, string[]? buildOverride, params PlayerBot[] bots)
    {
        var world = new World(seed, map);
        var log = new List<string>();
        int spawned = 0, towerKills = 0, playerKills = 0, leaked = 0, reactions = 0, wavesCleared = 0;
        bool victory = false;
        var buildOrder = buildOverride ?? BuildOrders[map.Id];
        int buildCursor = 0;

        while (!world.IsOver && world.Tick < MaxTicks)
        {
            // Scripted builder (attributed to no player: team policy).
            if (buildCursor < buildOrder.Length)
            {
                var parts = buildOrder[buildCursor].Split(':');
                int cost = Towers.All.TryGetValue(parts[0], out var towerDef)
                    ? towerDef.Cost
                    : Traps.All[parts[0]].Cost;
                if (world.Money >= cost)
                {
                    world.Enqueue(new Command.PlaceTower(0, parts[0], parts[1]));
                    buildCursor++;
                }
            }
            else if (world.Money > 150 && world.Towers.Count > 0)
            {
                // Surplus into damage paths, round-robin by tick for determinism.
                var tower = world.Towers[(int)(world.Tick % world.Towers.Count)];
                world.Enqueue(new Command.UpgradeTower(0, tower.Id, 0));
            }

            foreach (var bot in bots)
                bot.Act(world);

            Step.Advance(world);

            foreach (var e in world.Events)
            {
                log.Add(e.LogLine());
                switch (e)
                {
                    case SimEvent.EnemySpawned: spawned++; break;
                    case SimEvent.EnemyLeaked: leaked++; break;
                    case SimEvent.WaveCleared: wavesCleared++; break;
                    case SimEvent.ReactionTriggered: reactions++; break;
                    case SimEvent.EnemyDied died:
                        if (died.Source.StartsWith("tower")) towerKills++;
                        else playerKills++;
                        break;
                    case SimEvent.MatchEnded ended: victory = ended.Victory; break;
                }
            }
        }

        return new MatchResult(
            victory, wavesCleared, world.Lives, spawned, towerKills, playerKills, leaked, reactions,
            world.Tick, new Dictionary<ScrapType, int>(world.TeamScrap), HashLog(log), log);
    }

    public static string HashLog(IEnumerable<string> log)
    {
        var bytes = SHA256.HashData(Encoding.UTF8.GetBytes(string.Join("\n", log)));
        return Convert.ToHexString(bytes);
    }
}
