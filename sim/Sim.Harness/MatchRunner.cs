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
    long Ticks,
    string EventLogHash,
    List<string> EventLog);

/// <summary>Runs a full seeded match headlessly with a greedy builder and an
/// optional PlayerBot — the M0 seed of what grows into campaign.ts/sweep.ts.</summary>
public static class MatchRunner
{
    private const long MaxTicks = Balance.TickHz * 600; // 10 min safety cap

    public static MatchResult Run(uint seed, PlayerBot? bot)
    {
        var world = new World(seed, Maps.TestLane);
        var log = new List<string>();
        int spawned = 0, towerKills = 0, playerKills = 0, leaked = 0, wavesCleared = 0;
        bool victory = false;

        while (!world.IsOver && world.Tick < MaxTicks)
        {
            // Greedy builder: buy a Lance for the first free socket whenever affordable.
            var freeSocket = world.Map.Sockets.FirstOrDefault(
                s => world.Towers.All(t => t.SocketId != s.Id));
            if (freeSocket is not null && world.Money >= Towers.Lance.Cost)
                world.Enqueue(new Command.PlaceTower(0, Towers.Lance.Id, freeSocket.Id));

            bot?.Act(world);

            Step.Advance(world);

            foreach (var e in world.Events)
            {
                log.Add(e.LogLine());
                switch (e)
                {
                    case SimEvent.EnemySpawned: spawned++; break;
                    case SimEvent.EnemyLeaked: leaked++; break;
                    case SimEvent.WaveCleared: wavesCleared++; break;
                    case SimEvent.EnemyDied died:
                        if (died.Source.StartsWith("tower")) towerKills++;
                        else playerKills++;
                        break;
                    case SimEvent.MatchEnded ended: victory = ended.Victory; break;
                }
            }
        }

        return new MatchResult(
            victory, wavesCleared, world.Lives, spawned, towerKills, playerKills, leaked,
            world.Tick, HashLog(log), log);
    }

    public static string HashLog(IEnumerable<string> log)
    {
        var bytes = SHA256.HashData(Encoding.UTF8.GetBytes(string.Join("\n", log)));
        return Convert.ToHexString(bytes);
    }
}
