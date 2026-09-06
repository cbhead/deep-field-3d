using DeepField.Sim;
using DeepField.Sim.Content;
using DeepField.Sim.Util;

namespace DeepField.Harness;

/// <summary>The synthetic hero: a data-driven abstraction of a competent player,
/// not a physics bot. Balance is swept across a band of these (accuracy/uptime),
/// then the model gets recalibrated against real match-night DPS telemetry.
/// Read its results as a floor, not a forecast.</summary>
public sealed class PlayerBot
{
    public required int PlayerId { get; init; }
    public required float Accuracy { get; init; }   // chance an attempted shot lands
    public required float Uptime { get; init; }     // fraction of time actively shooting

    private readonly Rng _rng;
    private float _shotTimer;

    public PlayerBot(uint seed)
    {
        _rng = RngStreams.StreamFor(seed, RngStreams.Bot);
    }

    public void Act(World world)
    {
        if (world.Phase != MatchPhase.Wave) return;

        _shotTimer -= Balance.Dt;
        if (_shotTimer > 0f) return;

        var weapon = Weapons.Sidearm;
        _shotTimer = 1f / weapon.ShotsPerSecond;

        if (_rng.NextFloat() >= Uptime) return;   // repositioning, looking elsewhere

        // Perfect prioritization (the bot's one superpower): first along the route.
        Enemy? target = null;
        float best = -1f;
        foreach (var enemy in world.Enemies)
        {
            if (enemy.Dead) continue;
            if (enemy.TotalTraveled > best)
            {
                best = enemy.TotalTraveled;
                target = enemy;
            }
        }
        if (target is null) return;

        if (_rng.NextFloat() < Accuracy)
            world.Enqueue(new Command.PlayerHit(PlayerId, target.Id, weapon.Id));
    }
}
