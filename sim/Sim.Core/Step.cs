using DeepField.Sim.Content;

namespace DeepField.Sim;

/// <summary>The deterministic tick. Phase order is a spec carried from the 2D
/// game's step.ts and must not be reordered casually:
/// ApplyCommands → UpdateWaves → UpdateStatuses → MoveEnemies → FireTowers →
/// StepTowerProjectiles → StepPlayerOrdnance → ResolveDeaths → Cleanup → CheckEndState.</summary>
public static class Step
{
    public static void Advance(World w)
    {
        if (w.IsOver) return;

        w.Tick++;
        w.Events.Clear();

        ApplyCommands(w);
        UpdateWaves(w);
        UpdateStatuses(w);
        MoveEnemies(w);
        FireTowers(w);
        StepTowerProjectiles(w);
        StepPlayerOrdnance(w);
        ResolveDeaths(w);
        Cleanup(w);
        CheckEndState(w);
    }

    private static void ApplyCommands(World w)
    {
        foreach (var command in w.PendingCommands)
        {
            switch (command)
            {
                case Command.PlaceTower place:
                    ApplyPlaceTower(w, place);
                    break;

                case Command.SellTower sell:
                    ApplySellTower(w, sell);
                    break;

                case Command.StartWave:
                    if (w.Phase == MatchPhase.Intermission)
                        w.PhaseTimer = 0f;
                    break;

                case Command.PlayerHit hit:
                    ApplyPlayerHit(w, hit);
                    break;
            }
        }

        w.PendingCommands.Clear();

        // Weapon cooldowns tick down here so a hit later in this tick sees fresh state.
        foreach (int playerId in w.WeaponCooldowns.Keys.ToList())
            w.WeaponCooldowns[playerId] = MathF.Max(0f, w.WeaponCooldowns[playerId] - Balance.Dt);
    }

    private static void ApplyPlaceTower(World w, Command.PlaceTower place)
    {
        if (!Towers.All.TryGetValue(place.TowerId, out var def))
        {
            w.Emit(new SimEvent.BuildRejected(place.PlayerId, place.TowerId, place.SocketId, "unknownTower"));
            return;
        }

        var socket = w.Map.Sockets.FirstOrDefault(s => s.Id == place.SocketId);
        if (socket is null)
        {
            w.Emit(new SimEvent.BuildRejected(place.PlayerId, place.TowerId, place.SocketId, "unknownSocket"));
            return;
        }

        if (w.Towers.Any(t => t.SocketId == place.SocketId))
        {
            w.Emit(new SimEvent.BuildRejected(place.PlayerId, place.TowerId, place.SocketId, "occupied"));
            return;
        }

        if (w.Money < def.Cost)
        {
            w.Emit(new SimEvent.BuildRejected(place.PlayerId, place.TowerId, place.SocketId, "insufficientFunds"));
            return;
        }

        w.Money -= def.Cost;
        var tower = new Tower
        {
            Id = w.NextId(),
            DefId = def.Id,
            SocketId = socket.Id,
            Pos = socket.Pos,
            Spent = def.Cost,
        };
        w.Towers.Add(tower);
        w.Emit(new SimEvent.TowerPlaced(tower.Id, def.Id, socket.Id, place.PlayerId));
    }

    private static void ApplySellTower(World w, Command.SellTower sell)
    {
        var tower = w.Towers.FirstOrDefault(t => t.Id == sell.TowerId);
        if (tower is null) return;

        int refund = tower.Spent * 7 / 10;
        w.Money += refund;
        w.Towers.Remove(tower);
        w.Emit(new SimEvent.TowerSold(tower.Id, refund));
    }

    private static void ApplyPlayerHit(World w, Command.PlayerHit hit)
    {
        if (!Weapons.All.TryGetValue(hit.WeaponId, out var weapon)) return;

        // Sanity check, not anti-cheat: rate-limit to the weapon's fire rate.
        float cooldown = w.WeaponCooldowns.GetValueOrDefault(hit.PlayerId, 0f);
        if (cooldown > 0f) return;

        var enemy = w.Enemies.FirstOrDefault(e => e.Id == hit.EnemyId && !e.Dead);
        if (enemy is null) return;

        w.WeaponCooldowns[hit.PlayerId] = 1f / weapon.ShotsPerSecond;
        Damage(w, enemy, weapon.Damage, $"player{hit.PlayerId}");
    }

    private static void UpdateWaves(World w)
    {
        if (w.Phase == MatchPhase.Intermission)
        {
            w.PhaseTimer -= Balance.Dt;
            if (w.PhaseTimer <= 0f)
            {
                w.WaveIndex++;
                w.PendingSpawns = WavePlan.PlanWave(w.Seed, w.WaveIndex);
                w.WaveStartTick = w.Tick;
                w.Phase = MatchPhase.Wave;
                w.Emit(new SimEvent.WaveStarted(w.WaveIndex, w.PendingSpawns.Count));
            }
            return;
        }

        if (w.Phase != MatchPhase.Wave) return;

        long waveTick = w.Tick - w.WaveStartTick;
        for (int i = w.PendingSpawns.Count - 1; i >= 0; i--)
        {
            var entry = w.PendingSpawns[i];
            if (entry.TickOffset > waveTick) continue;

            var def = Enemies.All[entry.DefId];
            var enemy = new Enemy
            {
                Id = w.NextId(),
                DefId = def.Id,
                Hp = def.Hp * entry.HpFactor,
                MaxHp = def.Hp * entry.HpFactor,
                Pos = w.Map.Route[0],
                Bounty = def.Bounty,
                LeakDamage = def.LeakDamage,
                WaveIndex = w.WaveIndex,
            };
            w.Enemies.Add(enemy);
            w.Emit(new SimEvent.EnemySpawned(enemy.Id, def.Id, w.WaveIndex));
            w.PendingSpawns.RemoveAt(i);
        }
    }

    /// <summary>M0 stub — grows into the channel system (DoT ticks, expiry, cc decay,
    /// reactions) at M1+. Positioned before movement, per the 2D tick-order spec.</summary>
    private static void UpdateStatuses(World w)
    {
    }

    private static void MoveEnemies(World w)
    {
        foreach (var enemy in w.Enemies)
        {
            if (enemy.Dead) continue;

            var def = Enemies.All[enemy.DefId];
            float remaining = def.SpeedMetersPerSec * Balance.Dt;

            while (remaining > 0f && enemy.Leg < w.LegLengths.Length)
            {
                float legLeft = w.LegLengths[enemy.Leg] - enemy.LegProgress;
                if (remaining < legLeft)
                {
                    enemy.LegProgress += remaining;
                    enemy.TotalTraveled += remaining;
                    remaining = 0f;
                }
                else
                {
                    enemy.TotalTraveled += legLeft;
                    remaining -= legLeft;
                    enemy.Leg++;
                    enemy.LegProgress = 0f;
                }
            }

            if (enemy.Leg >= w.LegLengths.Length)
            {
                // Reached the goal: leak.
                enemy.Dead = true;
                w.Lives -= enemy.LeakDamage;
                w.Emit(new SimEvent.EnemyLeaked(enemy.Id, enemy.DefId, enemy.LeakDamage));
                continue;
            }

            var a = w.Map.Route[enemy.Leg];
            var b = w.Map.Route[enemy.Leg + 1];
            enemy.Pos = Vec3.Lerp(a, b, enemy.LegProgress / w.LegLengths[enemy.Leg]);
        }
    }

    private static void FireTowers(World w)
    {
        foreach (var tower in w.Towers)
        {
            tower.Cooldown = MathF.Max(0f, tower.Cooldown - Balance.Dt);
            if (tower.Cooldown > 0f) continue;

            var def = Towers.All[tower.DefId];

            // "First" targeting: furthest along the route, within range.
            Enemy? target = null;
            float best = -1f;
            foreach (var enemy in w.Enemies)
            {
                if (enemy.Dead) continue;
                if (tower.Pos.DistanceTo(enemy.Pos) > def.RangeMeters) continue;
                if (enemy.TotalTraveled > best)
                {
                    best = enemy.TotalTraveled;
                    target = enemy;
                }
            }

            if (target is null) continue;

            tower.Cooldown = 1f / def.ShotsPerSecond;
            w.Projectiles.Add(new Projectile
            {
                Id = w.NextId(),
                FiredBy = tower.Id,
                TargetId = target.Id,
                Pos = tower.Pos + new Vec3(0f, 1.5f, 0f),
                Speed = def.ProjectileSpeed,
                Damage = def.Damage,
            });
            w.Emit(new SimEvent.TowerFired(tower.Id, target.Id));
        }
    }

    private static void StepTowerProjectiles(World w)
    {
        foreach (var projectile in w.Projectiles)
        {
            if (projectile.Dead) continue;

            var target = w.Enemies.FirstOrDefault(e => e.Id == projectile.TargetId && !e.Dead);
            if (target is null)
            {
                projectile.Dead = true;
                continue;
            }

            var aim = target.Pos + new Vec3(0f, 0.8f, 0f);
            float stepLength = projectile.Speed * Balance.Dt;
            float distance = projectile.Pos.DistanceTo(aim);

            if (distance <= stepLength + Balance.ProjectileHitRadius)
            {
                projectile.Dead = true;
                var tower = w.Towers.FirstOrDefault(t => t.Id == projectile.FiredBy);
                Damage(w, target, projectile.Damage, $"tower{projectile.FiredBy}");
                if (tower is not null)
                {
                    tower.DamageDealt += projectile.Damage;
                    if (target.Dead) tower.Kills++;
                }
            }
            else
            {
                projectile.Pos += (aim - projectile.Pos).Normalized() * stepLength;
            }
        }
    }

    /// <summary>M0 stub — physical player ordnance (grenades etc.) arrives with M2+.</summary>
    private static void StepPlayerOrdnance(World w)
    {
    }

    /// <summary>M0 stub — split-on-death (Cluster) and death auras arrive with the roster.</summary>
    private static void ResolveDeaths(World w)
    {
    }

    private static void Cleanup(World w)
    {
        w.Enemies.RemoveAll(e => e.Dead);
        w.Projectiles.RemoveAll(p => p.Dead);
    }

    private static void CheckEndState(World w)
    {
        if (w.Lives <= 0)
        {
            w.Lives = 0;
            w.Phase = MatchPhase.Defeat;
            w.Emit(new SimEvent.MatchEnded(false, w.WaveIndex, 0));
            return;
        }

        if (w.Phase == MatchPhase.Wave && w.PendingSpawns.Count == 0 && w.Enemies.Count == 0)
        {
            w.Emit(new SimEvent.WaveCleared(w.WaveIndex));

            if (w.WaveIndex + 1 >= w.Map.TotalWaves)
            {
                w.Phase = MatchPhase.Victory;
                w.Emit(new SimEvent.MatchEnded(true, w.WaveIndex + 1, w.Lives));
            }
            else
            {
                w.Phase = MatchPhase.Intermission;
                w.PhaseTimer = Balance.IntermissionSeconds;
            }
        }
    }

    private static void Damage(World w, Enemy enemy, float amount, string source)
    {
        if (enemy.Dead) return;

        enemy.Hp -= amount;
        w.Emit(new SimEvent.EnemyDamaged(enemy.Id, amount, source));

        if (enemy.Hp <= 0f)
        {
            enemy.Dead = true;
            w.Money += enemy.Bounty;
            w.Emit(new SimEvent.EnemyDied(enemy.Id, enemy.DefId, enemy.Bounty, source));
        }
    }
}
