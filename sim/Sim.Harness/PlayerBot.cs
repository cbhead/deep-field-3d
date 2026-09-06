using DeepField.Sim;
using DeepField.Sim.Content;
using DeepField.Sim.Util;

namespace DeepField.Harness;

/// <summary>The synthetic hero: a data-driven abstraction of a competent player,
/// not a physics bot. It joins with a faction, teleports between authored hero
/// stations (paying a reposition delay), shoots with imperfect accuracy/uptime,
/// and fires its faction ability on cooldown. Balance is swept across a band of
/// these; read results as a floor, not a forecast.</summary>
public sealed class PlayerBot
{
    public required int PlayerId { get; init; }
    public required string FactionId { get; init; }
    public required float Accuracy { get; init; }        // chance an attempted shot lands
    public required float Uptime { get; init; }          // fraction of time actively shooting
    public float RepositionSeconds { get; init; } = 1.5f; // travel cost between stations

    private readonly Rng _rng;
    private float _shotTimer;
    private float _travelTimer;
    private int _stationIndex;
    private bool _joined;

    public PlayerBot(uint seed)
    {
        _rng = RngStreams.StreamFor(seed, RngStreams.Bot, 0);
    }

    public void Act(World world)
    {
        if (!_joined)
        {
            world.Enqueue(new Command.Join(PlayerId, $"bot{PlayerId}", FactionId));
            _joined = true;
            return;
        }

        if (!world.Players.TryGetValue(PlayerId, out var self)) return;

        // Reposition: chase the station nearest the frontmost enemy.
        var front = FrontEnemy(world);
        if (front is not null && world.Map.HeroStations.Count > 0)
        {
            int bestStation = _stationIndex;
            float bestDist = float.MaxValue;
            for (int i = 0; i < world.Map.HeroStations.Count; i++)
            {
                float d = world.Map.HeroStations[i].Pos.DistanceTo(front.Pos);
                if (d < bestDist) { bestDist = d; bestStation = i; }
            }
            if (bestStation != _stationIndex && _travelTimer <= 0f)
            {
                _stationIndex = bestStation;
                _travelTimer = RepositionSeconds;   // in transit: no shooting
            }
        }

        if (_travelTimer > 0f)
        {
            _travelTimer -= Balance.Dt;
            return;
        }

        var stationPos = world.Map.HeroStations.Count > 0
            ? world.Map.HeroStations[_stationIndex].Pos
            : world.Map.HeroSpawn;
        world.Enqueue(new Command.PlayerSync(PlayerId, stationPos));

        if (world.Phase != MatchPhase.Wave) return;
        if (!self.Alive) return;

        // Faction ability on cooldown, aimed at the front enemy.
        if (self.AbilityCooldown <= 0f && front is not null)
            world.Enqueue(new Command.UseAbility(PlayerId, front.Pos));

        _shotTimer -= Balance.Dt;
        if (_shotTimer > 0f) return;

        var weapon = Weapons.All[self.WeaponId];
        _shotTimer = 1f / weapon.ShotsPerSecond;

        if (_rng.NextFloat() >= Uptime) return;    // repositioning, looking elsewhere
        if (front is null) return;
        if (stationPos.DistanceTo(front.Pos) > weapon.RangeMeters) return;

        if (_rng.NextFloat() < Accuracy)
            world.Enqueue(new Command.PlayerHit(PlayerId, front.Id, weapon.Id));
    }

    private static Enemy? FrontEnemy(World world)
    {
        Enemy? best = null;
        float bestTraveled = -1f;
        foreach (var enemy in world.Enemies)
        {
            if (enemy.Dead) continue;
            if (enemy.TotalTraveled > bestTraveled)
            {
                bestTraveled = enemy.TotalTraveled;
                best = enemy;
            }
        }
        return best;
    }
}
