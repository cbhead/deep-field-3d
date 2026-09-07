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

    /// <summary>Fraction of engagements taken at contact range instead of at
    /// distance. Zero by default so every existing gate measures exactly what
    /// it measured before; the sweep raises it to price melee's scrap bonus
    /// against the risk of standing in contact damage.
    ///
    /// The bot does not model the risk directly — it has no positional fear —
    /// so read a melee-heavy column as an optimistic bound. That is the same
    /// caveat the accuracy band carries, and for the same reason.</summary>
    public float MeleeUptime { get; init; }

    /// <summary>Seconds spent getting around an enemy with a front arc before
    /// any shot lands on its back. Nothing is fired while moving, so the cost
    /// of flanking is measured in tempo — the rest of the wave walks on while
    /// you do it.
    ///
    /// Without this the bot could not answer a directional-armour enemy at all:
    /// it shoots from a hero station, stations sit ahead of the advance, and
    /// ahead is exactly where the armour is. The Aegis and the Ram both ask
    /// "will a player move?" and the synthetic hero could only ever say no,
    /// which made a designed-for counter read as an impossible enemy.</summary>
    public float FlankSeconds { get; init; } = 2.5f;

    /// <summary>How far behind the target the bot stands to shoot it. Outside
    /// contact range: the price of flanking here is time, not health, because
    /// the bot has no positional fear and charging it for a risk it cannot
    /// perceive would flatter the enemy rather than the player.</summary>
    private const float FlankStandoffMeters = 3.0f;

    private readonly Rng _rng;
    private float _shotTimer;
    private float _travelTimer;
    private int _stationIndex;
    private bool _joined;
    private float _flankTimer;
    private int _flankTargetId = -1;

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

        // Reposition: chase the station nearest the enemy being answered.
        var front = Target(world);
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

        // Flank anything whose front is armoured and whose back is not. The
        // walk around costs FlankSeconds of firing nothing, and it restarts if
        // the answer changes target, so a wave full of armour is expensive to
        // answer rather than free.
        if (front is not null && Flankable(front))
        {
            if (_flankTargetId != front.Id) { _flankTargetId = front.Id; _flankTimer = FlankSeconds; }
            if (_flankTimer > 0f)
            {
                _flankTimer -= Balance.Dt;
                return;                          // still walking round: no shots
            }
            var facing = front.Facing;
            float len = facing.Length();
            if (len > 0.01f) stationPos = front.Pos - facing * (FlankStandoffMeters / len);
        }
        else
        {
            _flankTargetId = -1;
        }

        world.Enqueue(new Command.PlayerSync(PlayerId, stationPos));

        if (world.Phase != MatchPhase.Wave) return;
        if (!self.Alive) return;

        // Faction ability on cooldown, aimed at the front enemy.
        if (self.AbilityCooldown <= 0f && front is not null)
            world.Enqueue(new Command.UseAbility(PlayerId, front.Pos));

        // Melee: closing to contact is modelled as being at the enemy rather
        // than at the station, because that is what walking up to something
        // means. The swing lands on its own cooldown, so a melee-leaning bot
        // does not also get to shoot on the same tick.
        if (MeleeUptime > 0f && front is not null && self.MeleeCooldown <= 0f
            && _rng.NextFloat() < MeleeUptime)
        {
            world.Enqueue(new Command.PlayerSync(PlayerId, front.Pos));
            world.Enqueue(new Command.PlayerMelee(PlayerId, front.Pos + new Vec3(0f, 0f, 1f)));
            return;
        }

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

    private static bool Flankable(Enemy enemy)
    {
        var def = Enemies.All[enemy.DefId];
        return def.FrontArmorArcDegrees > 0f && def.RearWeakFactor > 1f;
    }

    /// <summary>What the hero answers this tick: healers, then anything
    /// demolishing a structure, then the frontmost enemy.
    ///
    /// Frontmost alone was wrong the moment a siege enemy existed. A Ram stops
    /// to swing, so everything else overtakes it and it is never the front —
    /// the bot would walk past the thing dismantling its defence to shoot a
    /// drifter that was merely further along. In two full runs it fired at the
    /// Ram exactly never, which reads as "the Ram is unkillable" rather than
    /// "the bot is not looking at it".
    ///
    /// Healers outrank even that, and the measurement is why: with siege first,
    /// the bot put 771 damage into a 220 hp Ram and did not kill it, because
    /// the wave pairs it with a Mender and nobody was shooting the Mender.
    /// Prioritising is the Mender's designed counter and refusing to kill a
    /// healer is not a floor, it is a mistake — so the bot does not make it.
    ///
    /// All three tiers are target policy, which the plan names as a bot dial.
    /// Read the result as a competent player, not an optimal one: it still
    /// only tracks one target at a time.</summary>
    private static Enemy? Target(World world)
    {
        Enemy? healer = null, sieging = null, front = null;
        float healerTraveled = -1f, siegeTraveled = -1f, bestTraveled = -1f;

        foreach (var enemy in world.Enemies)
        {
            if (enemy.Dead) continue;
            if (Enemies.All[enemy.DefId].HealPerSecond > 0f && enemy.TotalTraveled > healerTraveled)
            {
                healerTraveled = enemy.TotalTraveled;
                healer = enemy;
            }
            if (enemy.Sieging && enemy.TotalTraveled > siegeTraveled)
            {
                siegeTraveled = enemy.TotalTraveled;
                sieging = enemy;
            }
            if (enemy.TotalTraveled > bestTraveled)
            {
                bestTraveled = enemy.TotalTraveled;
                front = enemy;
            }
        }
        return healer ?? sieging ?? front;
    }
}
