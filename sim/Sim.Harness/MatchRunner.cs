using System.Linq;
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
    List<string> EventLog,
    // Personal scrap only reaches a player by being walked over now, so the
    // sweep has to be able to see whether that loop actually closes.
    Dictionary<ScrapType, int>? PersonalScrapEnd = null,
    int ScrapCollected = 0,
    int ScrapExpired = 0);

/// <summary>Runs a full seeded match headlessly with a scripted builder and any
/// number of PlayerBots — the M1 seed of campaign.ts/sweep.ts.</summary>
public static class MatchRunner
{
    /// <summary>Safety cap, sized to the map rather than fixed. Thirty minutes
    /// was generous for a 126 m lane and is not for a 471 m one: a map whose
    /// enemies walk four times as far takes four times as long to leak, and a
    /// run cut off by the cap reads as a floor policy that held when it is
    /// really a clock that ran out.</summary>
    private static long MaxTicksFor(World world)
    {
        float longest = 0f;
        foreach (float walk in world.RouteWalkLengths) longest = MathF.Max(longest, walk);
        return Balance.TickHz * (longest > 300f ? 3600 : 1800);
    }

    /// <summary>Deterministic build orders per map: "tower:socket" placed as
    /// money allows, then damage-path upgrades with the surplus. A floor policy,
    /// not a forecast — same doctrine as the 2D greedy builder.</summary>
    /// <summary>The switchyard floor policy with its Detector removed, for the
    /// conditions gate. Same list, same order, one tower missing — so any
    /// difference in outcome is attributable to that tower and nothing else.</summary>
    /// <summary>The floor policy for a map, for gates that need to vary one
    /// pick and hold the rest still. Read-only: the lists themselves are the
    /// reference builds and a gate that edited one would change every other
    /// gate's meaning.</summary>
    public static IReadOnlyList<string> FloorPolicy(string mapId) => BuildOrders[mapId];

    public static string[] SwitchyardWithoutDetector =>
        BuildOrders["switchyard"].Where(e => !e.StartsWith("detector:")).ToArray();

    private static readonly Dictionary<string, string[]> BuildOrders = new()
    {
        ["testlane"] = new[] { "lance:s2", "lance:s3", "lance:s1", "lance:s4" },
        ["foundry"] = new[]
        {
            // Deck picks are the front lip (w5/w6), not the middle row. The
            // rows are 4 m apart and both reach the lane in clear weather, so
            // the old choice looked equivalent and measured worse: under Fog a
            // middle-row lance stops reaching the lane entirely, and the floor
            // died on Foundry's weather wave every time. The lip is also just
            // where a player would stand.
            "lance:g2", "lance:g5", "skywatch:g4", "singularity:g3",
            "nova:g1", "skywatch:w6", "lance:g6", "lance:w5",
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
        // Spire: the core is on the roof, so the build reads bottom-to-top and
        // the last entries matter most — anything that leaks arrives at the
        // thing you are defending. Roof anti-air is not optional here because
        // the air route skips every floor between the street and the core.
        //
        // **This one is generated, not hand-tuned, and the fixture comparison
        // for this map is therefore vacuous — say so rather than let a row of
        // near-identical percentages imply agreement.** Every other map's list
        // is a human's build that `FloorPolicy` is measured against. The Spire
        // was re-authored in M5 and not one of its sockets survived, so there
        // was no human build left to compare with: this is `FloorPolicy.Build`
        // taken as authored. It earns a place here as the map's floor and as a
        // regression fixture against the *generator* changing, and it will
        // become a real fixture the first time someone plays this map and
        // beats it by hand.
        ["spire"] = new[]
        {
            "nova:w5", "nova:w6", "lance:g7", "skywatch:w25",
            "skywatch:w37", "lance:w19", "lance:w20", "lance:w21",
            "lance:w30", "skywatch:w10", "nova:w31", "lance:g9",
            "nova:g13", "nova:w24", "skywatch:g2", "nova:w18",
        },
        // The Toaster: three ground routes, and the only ground all three
        // share is the last 140 m of drive into the core. So the floor policy
        // buys that corridor first and works outward, which is also what a
        // player does the first time a wave comes out of a warp pad behind
        // them. Picked on measured coverage: g55/g49/g53/g52/g47-g49 sit on the
        // stretch every route ends on, and the air strand runs 9 m over that
        // same stretch, so the Skywatches pay twice.
        //
        // The Detector is placed by a different measure and it matters: Shades
        // debut on the west route at wave 8, which arrives at the core from the
        // opposite side to everything else. Scored on lane coverage it would go
        // on the drive with the rest and see none of them.
        ["toaster"] = new[]
        {
            "lance:g55", "lance:g49", "skywatch:g47", "nova:g53",
            "lance:g42", "skywatch:g38", "arc:g48", "detector:g63",
            "nova:g40", "lance:g52", "skywatch:g57", "singularity:g54",
            "tar:t11", "lance:g61", "nova:g45", "spike:t2",
        },
    };

    /// <summary>The first entry in the build order whose socket is now empty,
    /// or null if everything the policy built is still standing.</summary>
    private static string? RebuildTarget(World world, string[] buildOrder)
    {
        foreach (string entry in buildOrder)
        {
            var parts = entry.Split(':');
            if (!Towers.All.ContainsKey(parts[0])) continue;      // traps rearm themselves
            if (Towers.All[parts[0]].StructureHp <= 0f) continue; // cannot be destroyed
            if (!world.Towers.Any(t => t.SocketId == parts[1])) return entry;
        }
        return null;
    }

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

        long maxTicks = MaxTicksFor(world);
        while (!world.IsOver && world.Tick < maxTicks)
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
            // Rebuild what got demolished, before spending on anything else.
            // The policy was a fixed list that could only ever go forwards,
            // which was fine while nothing could take a structure away. The Ram
            // takes the barricade away, and a floor policy that will not
            // replace it is not measuring a floor — it is measuring what
            // happens when a player watches their defence get dismantled and
            // does nothing. A real team rebuilds; so does this.
            else if (RebuildTarget(world, buildOrder) is { } rebuild)
            {
                var parts = rebuild.Split(':');
                int cost = Towers.All[parts[0]].Cost;
                if (world.Money >= cost)
                    world.Enqueue(new Command.PlaceTower(0, parts[0], parts[1]));
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

        var personal = new Dictionary<ScrapType, int>();
        foreach (var p in world.Players.Values)
            foreach (var (type, amount) in p.Scrap)
                personal[type] = personal.GetValueOrDefault(type) + amount;

        return new MatchResult(
            victory, wavesCleared, world.Lives, spawned, towerKills, playerKills, leaked, reactions,
            world.Tick, new Dictionary<ScrapType, int>(world.TeamScrap), HashLog(log), log,
            personal,
            log.Count(l => l.Contains("scrapCollected")),
            log.Count(l => l.Contains("scrapExpired")));
    }

    public static string HashLog(IEnumerable<string> log)
    {
        var bytes = SHA256.HashData(Encoding.UTF8.GetBytes(string.Join("\n", log)));
        return Convert.ToHexString(bytes);
    }
}
