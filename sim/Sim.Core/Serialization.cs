using System.Text.Json;
using DeepField.Sim.Content;

namespace DeepField.Sim;

/// <summary>Full-world snapshot for drop-in join, save games, and replay
/// checkpoints. M0 keeps it simple JSON; the wire format tightens at M1.</summary>
public static class Serialization
{
    private sealed record EnemyState(int Id, string DefId, float Hp, float MaxHp, int Leg, float LegProgress, float TotalTraveled, int Bounty, int LeakDamage, int WaveIndex);
    private sealed record TowerState(int Id, string DefId, string SocketId, float Cooldown, int Spent, int Kills, float DamageDealt);
    private sealed record SpawnState(string DefId, int TickOffset, float HpFactor);
    private sealed record ProjectileState(int Id, int FiredBy, int TargetId, float X, float Y, float Z, float Speed, float Damage);

    private sealed record WorldState(
        uint Seed, long Tick, string MapId, int Money, int Lives,
        int Phase, float PhaseTimer, int WaveIndex, long WaveStartTick,
        List<EnemyState> Enemies, List<TowerState> Towers, List<SpawnState> PendingSpawns,
        List<ProjectileState> Projectiles, Dictionary<int, float> WeaponCooldowns, int NextId);

    public static string Serialize(World w)
    {
        var state = new WorldState(
            w.Seed, w.Tick, w.Map.Id, w.Money, w.Lives,
            (int)w.Phase, w.PhaseTimer, w.WaveIndex, w.WaveStartTick,
            w.Enemies.Select(e => new EnemyState(e.Id, e.DefId, e.Hp, e.MaxHp, e.Leg, e.LegProgress, e.TotalTraveled, e.Bounty, e.LeakDamage, e.WaveIndex)).ToList(),
            w.Towers.Select(t => new TowerState(t.Id, t.DefId, t.SocketId, t.Cooldown, t.Spent, t.Kills, t.DamageDealt)).ToList(),
            w.PendingSpawns.Select(s => new SpawnState(s.DefId, s.TickOffset, s.HpFactor)).ToList(),
            w.Projectiles.Where(p => !p.Dead).Select(p => new ProjectileState(p.Id, p.FiredBy, p.TargetId, p.Pos.X, p.Pos.Y, p.Pos.Z, p.Speed, p.Damage)).ToList(),
            new Dictionary<int, float>(w.WeaponCooldowns),
            w.NextIdValue);
        return JsonSerializer.Serialize(state);
    }

    public static World Deserialize(string json)
    {
        var state = JsonSerializer.Deserialize<WorldState>(json)
            ?? throw new InvalidOperationException("bad world state");

        var world = new World(state.Seed, Maps.All[state.MapId])
        {
            Tick = state.Tick,
            Money = state.Money,
            Lives = state.Lives,
            Phase = (MatchPhase)state.Phase,
            PhaseTimer = state.PhaseTimer,
            WaveIndex = state.WaveIndex,
            WaveStartTick = state.WaveStartTick,
        };

        foreach (var e in state.Enemies)
        {
            var enemy = new Enemy
            {
                Id = e.Id, DefId = e.DefId, Hp = e.Hp, MaxHp = e.MaxHp,
                Leg = e.Leg, LegProgress = e.LegProgress, TotalTraveled = e.TotalTraveled,
                Bounty = e.Bounty, LeakDamage = e.LeakDamage, WaveIndex = e.WaveIndex,
            };
            var a = world.Map.Route[enemy.Leg];
            var b = world.Map.Route[System.Math.Min(enemy.Leg + 1, world.Map.Route.Count - 1)];
            float legLength = world.LegLengths[System.Math.Min(enemy.Leg, world.LegLengths.Length - 1)];
            enemy.Pos = Vec3.Lerp(a, b, legLength > 0f ? enemy.LegProgress / legLength : 0f);
            world.Enemies.Add(enemy);
        }

        foreach (var t in state.Towers)
        {
            var socket = world.Map.Sockets.First(s => s.Id == t.SocketId);
            world.Towers.Add(new Tower
            {
                Id = t.Id, DefId = t.DefId, SocketId = t.SocketId, Pos = socket.Pos,
                Cooldown = t.Cooldown, Spent = t.Spent, Kills = t.Kills, DamageDealt = t.DamageDealt,
            });
        }

        world.PendingSpawns = state.PendingSpawns.Select(s => new SpawnEntry(s.DefId, s.TickOffset, s.HpFactor)).ToList();
        foreach (var p in state.Projectiles)
        {
            world.Projectiles.Add(new Projectile
            {
                Id = p.Id, FiredBy = p.FiredBy, TargetId = p.TargetId,
                Pos = new Vec3(p.X, p.Y, p.Z), Speed = p.Speed, Damage = p.Damage,
            });
        }
        world.WeaponCooldowns = new Dictionary<int, float>(state.WeaponCooldowns);

        world.EnsureNextIdAtLeast(state.NextId);
        return world;
    }

}
