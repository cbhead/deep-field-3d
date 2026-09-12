using System.Text.Json;
using DeepField.Sim.Content;

namespace DeepField.Sim;

/// <summary>Full-world snapshot for drop-in join, save games, and replay
/// checkpoints. JSON at M1; the wire format tightens when it becomes a
/// bandwidth problem (it is not one at 4 players on a tailnet).</summary>
public static class Serialization
{
    private sealed record StatusState(int Channel, string StatusId, float TimeLeft, string Source);
    private sealed record EnemyState(
        int Id, string DefId, float Hp, float MaxHp, int RouteIndex, int Leg, float LegProgress,
        float TotalTraveled, float LateralOffset, float FacingX, float FacingY, float FacingZ,
        int Bounty, int LeakDamage, int WaveIndex, List<StatusState> Statuses,
        float Shield, float ShieldTimer, float CcResist, bool Burrowed);
    private sealed record TowerState(
        int Id, string DefId, string SocketId, float Cooldown, int Spent, int Kills,
        float DamageDealt, int[] PathLevels, float BuffTimer, float BuffFactor,
        int RampTargetId = -1, float RampSeconds = 0f,
        // Trailing optional: a save written before structures could be damaged
        // loads with Hp 0, which would read as "already rubble", so a missing
        // value is restored to the def's full health rather than taken
        // literally. -1 is the sentinel because 0 is a legitimate value only
        // for towers that cannot be damaged at all.
        float Hp = -1f);
    private sealed record TrapState(int Id, string DefId, string SocketId, int ChargesLeft, float RearmTimer);
    private sealed record ProjectileState(
        int Id, int FiredBy, int TargetId, float X, float Y, float Z,
        float Speed, float Damage, float SplashRadius, float SplashFalloff);
    private sealed record SpawnState(string DefId, int TickOffset, float HpFactor, int RouteIndex, float LateralOffset);
    private sealed record PickupState(int Id, string Type, int Amount, float X, float Y, float Z, float Life, float GroundY = 0f);
    private sealed record VehicleState(string Id, float X, float Y, float Z, float Yaw, int[] Seats);
    private sealed record PlayerStateDto(
        int Id, string Name, string FactionId, int FactionLevel, int MatchXp, float X, float Y, float Z,
        float Hp, bool Downed, float BleedoutTimer, float ReviveProgress, float RespawnTimer,
        float RegenDelay, float WeaponCooldown, float AbilityCooldown,
        string WeaponId, List<string> OwnedWeapons, Dictionary<string, int> Scrap, bool Connected,
        Dictionary<string, Dictionary<string, string>>? Builds, Dictionary<string, string>? BuildAmmo,
        List<string>? CraftedAmmo,
        int Kills = 0, float DamageDealt = 0f, int Builds2 = 0, int Revives = 0,
        // Melee rides as trailing optionals for the same reason the match stats
        // do: a save written before melee existed must still load, and it does
        // — the player arrives holding the wrench, which is what they would
        // have had.
        string? MeleeId = null, List<string>? OwnedMelee = null,
        Dictionary<string, Dictionary<string, string>>? MeleeBuilds = null,
        Dictionary<string, int>? MeleeMastery = null, float MeleeCooldown = 0f,
        // Pack a Punch, same trailing-optional rule: a save written before it
        // existed loads with every weapon at level 0, which is what it had.
        Dictionary<string, int>? PackLevels = null);

    private sealed record WorldState(
        uint Seed, long Tick, string MapId, int Money, int Lives,
        Dictionary<string, int> TeamScrap,
        int Phase, float PhaseTimer, int WaveIndex, long WaveStartTick,
        List<EnemyState> Enemies, List<TowerState> Towers, List<ProjectileState> Projectiles,
        List<SpawnState> PendingSpawns, List<PlayerStateDto> Players,
        List<TrapState>? Traps, int NextId, bool Lobby = false, bool Endless = false,
        List<PickupState>? Pickups = null,
        // Trailing optional, same rule as everything else here: a save written
        // before vehicles existed loads with every vehicle parked where the map
        // put it and every seat empty, which is what it had.
        List<VehicleState>? Vehicles = null);

    public static string Serialize(World w)
    {
        var state = new WorldState(
            w.Seed, w.Tick, w.Map.Id, w.Money, w.Lives,
            w.TeamScrap.ToDictionary(kv => kv.Key.ToString(), kv => kv.Value),
            (int)w.Phase, w.PhaseTimer, w.WaveIndex, w.WaveStartTick,
            w.Enemies.Select(e => new EnemyState(
                e.Id, e.DefId, e.Hp, e.MaxHp, e.RouteIndex, e.Leg, e.LegProgress,
                e.TotalTraveled, e.LateralOffset, e.Facing.X, e.Facing.Y, e.Facing.Z,
                e.Bounty, e.LeakDamage, e.WaveIndex, ActiveStatuses(e),
                e.Shield, e.ShieldTimer, e.CcResist, e.Burrowed)).ToList(),
            w.Towers.Select(t => new TowerState(
                t.Id, t.DefId, t.SocketId, t.Cooldown, t.Spent, t.Kills,
                t.DamageDealt, t.PathLevels, t.BuffTimer, t.BuffFactor,
                t.RampTargetId, t.RampSeconds, t.Hp)).ToList(),
            w.Projectiles.Where(p => !p.Dead).Select(p => new ProjectileState(
                p.Id, p.FiredBy, p.TargetId, p.Pos.X, p.Pos.Y, p.Pos.Z,
                p.Speed, p.Damage, p.SplashRadius, p.SplashFalloff)).ToList(),
            w.PendingSpawns.Select(s => new SpawnState(s.DefId, s.TickOffset, s.HpFactor, s.RouteIndex, s.LateralOffset)).ToList(),
            w.Players.Values.Select(p => new PlayerStateDto(
                p.Id, p.Name, p.FactionId, p.FactionLevel, p.MatchXp, p.Pos.X, p.Pos.Y, p.Pos.Z,
                p.Hp, p.Downed, p.BleedoutTimer, p.ReviveProgress, p.RespawnTimer,
                p.RegenDelay, p.WeaponCooldown, p.AbilityCooldown,
                p.WeaponId, p.OwnedWeapons.OrderBy(x => x, StringComparer.Ordinal).ToList(),
                p.Scrap.ToDictionary(kv => kv.Key.ToString(), kv => kv.Value), p.Connected,
                p.Builds.ToDictionary(kv => kv.Key,
                    kv => kv.Value.Attachments.ToDictionary(a => a.Key.ToString(), a => a.Value)),
                p.Builds.ToDictionary(kv => kv.Key, kv => kv.Value.AmmoId),
                p.CraftedAmmo.OrderBy(x => x, StringComparer.Ordinal).ToList(),
                p.Kills, p.DamageDealt, p.TowersBuilt, p.Revives,
                p.MeleeId, p.OwnedMelee.OrderBy(x => x, StringComparer.Ordinal).ToList(),
                p.MeleeBuilds.ToDictionary(kv => kv.Key,
                    kv => kv.Value.Attachments.ToDictionary(a => a.Key.ToString(), a => a.Value)),
                p.MeleeBuilds.ToDictionary(kv => kv.Key, kv => kv.Value.MasteryLevel),
                p.MeleeCooldown,
                p.Builds.ToDictionary(kv => kv.Key, kv => kv.Value.PackLevel))).ToList(),
            w.Traps.Select(t => new TrapState(t.Id, t.DefId, t.SocketId, t.ChargesLeft, t.RearmTimer)).ToList(),
            w.NextIdValue, w.Lobby, w.Endless,
            w.Pickups.Select(p => new PickupState(
                p.Id, p.Type.ToString(), p.Amount, p.Pos.X, p.Pos.Y, p.Pos.Z, p.Life, p.GroundY)).ToList(),
            w.Vehicles.Select(v => new VehicleState(
                v.Id, v.Pos.X, v.Pos.Y, v.Pos.Z, v.YawDegrees, v.Seats)).ToList());
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
            Lobby = state.Lobby,
            Endless = state.Endless,
        };

        foreach (var (typeName, amount) in state.TeamScrap)
            world.TeamScrap[System.Enum.Parse<ScrapType>(typeName)] = amount;

        foreach (var e in state.Enemies)
        {
            var enemy = new Enemy
            {
                Id = e.Id, DefId = e.DefId, Hp = e.Hp, MaxHp = e.MaxHp,
                RouteIndex = e.RouteIndex, Leg = e.Leg, LegProgress = e.LegProgress,
                TotalTraveled = e.TotalTraveled, LateralOffset = e.LateralOffset,
                Facing = new Vec3(e.FacingX, e.FacingY, e.FacingZ),
                Bounty = e.Bounty, LeakDamage = e.LeakDamage, WaveIndex = e.WaveIndex,
                Shield = e.Shield, ShieldTimer = e.ShieldTimer,
                CcResist = e.CcResist, Burrowed = e.Burrowed,
            };
            foreach (var s in e.Statuses)
            {
                enemy.Statuses[s.Channel] = new StatusSlot
                {
                    StatusId = s.StatusId, TimeLeft = s.TimeLeft, Source = s.Source,
                };
            }
            RecomputePosition(world, enemy);
            world.Enemies.Add(enemy);
        }

        foreach (var t in state.Towers)
        {
            var socket = world.Map.Sockets.First(s => s.Id == t.SocketId);
            world.Towers.Add(new Tower
            {
                Id = t.Id, DefId = t.DefId, SocketId = t.SocketId, Pos = socket.Pos,
                Cooldown = t.Cooldown, Spent = t.Spent, Kills = t.Kills, DamageDealt = t.DamageDealt,
                PathLevels = t.PathLevels, BuffTimer = t.BuffTimer, BuffFactor = t.BuffFactor,
                RampTargetId = t.RampTargetId, RampSeconds = t.RampSeconds,
                Hp = t.Hp >= 0f ? t.Hp : Towers.All[t.DefId].StructureHp,
            });
        }

        foreach (var p in state.Projectiles)
        {
            world.Projectiles.Add(new Projectile
            {
                Id = p.Id, FiredBy = p.FiredBy, TargetId = p.TargetId,
                Pos = new Vec3(p.X, p.Y, p.Z), Speed = p.Speed, Damage = p.Damage,
                SplashRadius = p.SplashRadius, SplashFalloff = p.SplashFalloff,
            });
        }

        foreach (var p in state.Pickups ?? new List<PickupState>())
        {
            world.Pickups.Add(new ScrapPickup
            {
                Id = p.Id, Type = System.Enum.Parse<ScrapType>(p.Type), Amount = p.Amount,
                Pos = new Vec3(p.X, p.Y, p.Z), Life = p.Life, GroundY = p.GroundY,
            });
        }

        world.PendingSpawns = state.PendingSpawns
            .Select(s => new SpawnEntry(s.DefId, s.TickOffset, s.HpFactor, s.RouteIndex, s.LateralOffset)).ToList();

        foreach (var p in state.Players)
        {
            var player = new PlayerState
            {
                Id = p.Id, Name = p.Name, FactionId = p.FactionId,
                FactionLevel = p.FactionLevel == 0 ? 1 : p.FactionLevel, MatchXp = p.MatchXp,
                Pos = new Vec3(p.X, p.Y, p.Z),
                Hp = p.Hp, Downed = p.Downed, BleedoutTimer = p.BleedoutTimer,
                ReviveProgress = p.ReviveProgress, RespawnTimer = p.RespawnTimer,
                RegenDelay = p.RegenDelay, WeaponCooldown = p.WeaponCooldown,
                AbilityCooldown = p.AbilityCooldown, WeaponId = p.WeaponId,
                OwnedWeapons = new HashSet<string>(p.OwnedWeapons), Connected = p.Connected,
                Kills = p.Kills, DamageDealt = p.DamageDealt,
                TowersBuilt = p.Builds2, Revives = p.Revives,
                MeleeId = string.IsNullOrEmpty(p.MeleeId) ? "wrench" : p.MeleeId,
                MeleeCooldown = p.MeleeCooldown,
            };
            if (p.OwnedMelee is { Count: > 0 })
                player.OwnedMelee = new HashSet<string>(p.OwnedMelee);
            foreach (var (meleeId, slots) in p.MeleeBuilds ?? new())
            {
                var mb = player.MeleeBuildFor(meleeId);
                foreach (var (slotName, attachmentId) in slots)
                    mb.Attachments[System.Enum.Parse<MeleeSlot>(slotName)] = attachmentId;
            }
            foreach (var (meleeId, level) in p.MeleeMastery ?? new())
                player.MeleeBuildFor(meleeId).MasteryLevel = level;
            foreach (var (typeName, amount) in p.Scrap)
                player.Scrap[System.Enum.Parse<ScrapType>(typeName)] = amount;
            foreach (var (weaponId, slots) in p.Builds ?? new())
            {
                var build = player.BuildFor(weaponId);
                foreach (var (slotName, attachmentId) in slots)
                    build.Attachments[System.Enum.Parse<AttachmentSlot>(slotName)] = attachmentId;
                if (p.PackLevels is { } packs && packs.TryGetValue(weaponId, out int packLevel))
                    build.PackLevel = packLevel;
                if (p.BuildAmmo is { } ammoMap && ammoMap.TryGetValue(weaponId, out var ammoId))
                    build.AmmoId = ammoId;
            }
            if (p.CraftedAmmo is { } crafted)
                player.CraftedAmmo = new HashSet<string>(crafted);
            world.Players[p.Id] = player;
        }

        foreach (var t in state.Traps ?? new List<TrapState>())
        {
            var socket = world.Map.Sockets.First(s => s.Id == t.SocketId);
            world.Traps.Add(new Trap
            {
                Id = t.Id, DefId = t.DefId, SocketId = t.SocketId, Pos = socket.Pos,
                ChargesLeft = t.ChargesLeft, RearmTimer = t.RearmTimer,
            });
        }

        // The map already parked every vehicle when the world was built, so a
        // saved one is an overlay by id rather than a list to rebuild: a save
        // from before a map gained a vehicle still loads, and one naming a
        // vehicle the map has since dropped is ignored instead of throwing.
        foreach (var v in state.Vehicles ?? new List<VehicleState>())
        {
            var vehicle = world.Vehicles.FirstOrDefault(x => x.Id == v.Id);
            if (vehicle is null) continue;
            vehicle.Pos = new Vec3(v.X, v.Y, v.Z);
            vehicle.YawDegrees = v.Yaw;
            for (int i = 0; i < vehicle.Seats.Length && i < v.Seats.Length; i++)
                vehicle.Seats[i] = v.Seats[i];
        }

        world.EnsureNextIdAtLeast(state.NextId);
        return world;
    }

    private static List<StatusState> ActiveStatuses(Enemy e)
    {
        var list = new List<StatusState>();
        for (int c = 0; c < e.Statuses.Length; c++)
        {
            if (e.Statuses[c].Active)
                list.Add(new StatusState(c, e.Statuses[c].StatusId!, e.Statuses[c].TimeLeft, e.Statuses[c].Source));
        }
        return list;
    }

    private static void RecomputePosition(World world, Enemy enemy)
    {
        var waypoints = world.Map.Routes[enemy.RouteIndex].Waypoints;
        var legs = world.RouteLegLengths[enemy.RouteIndex];
        int leg = System.Math.Min(enemy.Leg, legs.Length - 1);
        float legLength = legs[leg];
        var a = waypoints[leg];
        var b = waypoints[leg + 1];
        var spine = Vec3.Lerp(a, b, legLength > 0f ? enemy.LegProgress / legLength : 0f);
        var facing = (b - a).Normalized();
        var perp = new Vec3(-facing.Z, 0f, facing.X);
        enemy.Pos = spine + perp * enemy.LateralOffset;
    }
}
