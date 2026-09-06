using DeepField.Sim.Content;

namespace DeepField.Sim;

/// <summary>The deterministic tick. Phase order is a spec carried from the 2D
/// game's step.ts (statuses before movement, fire before projectile step,
/// cleanup last). M1 adds UpdatePlayers between movement and tower fire:
/// ApplyCommands → UpdateWaves → UpdateStatuses → MoveEnemies → UpdatePlayers →
/// FireTowers → StepTowerProjectiles → StepPlayerOrdnance → ResolveDeaths →
/// Cleanup → CheckEndState.</summary>
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
        TriggerTraps(w);
        UpdatePlayers(w);
        FireTowers(w);
        StepTowerProjectiles(w);
        StepPlayerOrdnance(w);
        ResolveDeaths(w);
        Cleanup(w);
        CheckEndState(w);
    }

    // =====================================================================
    // Commands
    // =====================================================================

    private static void ApplyCommands(World w)
    {
        foreach (var command in w.PendingCommands)
        {
            switch (command)
            {
                case Command.Join join: ApplyJoin(w, join); break;
                case Command.Leave leave: ApplyLeave(w, leave); break;
                case Command.PlayerSync sync: ApplyPlayerSync(w, sync); break;
                case Command.PlaceTower place: ApplyPlaceTower(w, place); break;
                case Command.SellTower sell: ApplySellTower(w, sell); break;
                case Command.UpgradeTower upgrade: ApplyUpgradeTower(w, upgrade); break;
                case Command.StartWave:
                    if (w.Phase == MatchPhase.Intermission) w.PhaseTimer = 0f;
                    break;
                case Command.PlayerHit hit: ApplyPlayerHit(w, hit); break;
                case Command.BuyWeapon buy: ApplyBuyWeapon(w, buy); break;
                case Command.SelectWeapon select: ApplySelectWeapon(w, select); break;
                case Command.UseAbility ability: ApplyUseAbility(w, ability); break;
                case Command.Revive revive: ApplyRevive(w, revive); break;
                case Command.CraftAttachment craft: ApplyCraftAttachment(w, craft); break;
                case Command.SelectAmmo ammo: ApplySelectAmmo(w, ammo); break;
            }
        }
        w.PendingCommands.Clear();
    }

    private static void ApplyJoin(World w, Command.Join join)
    {
        if (w.Players.TryGetValue(join.PlayerId, out var existing))
        {
            existing.Connected = true;   // reconnect keeps seat and faction
            return;
        }

        if (!Factions.All.ContainsKey(join.FactionId))
        {
            w.Emit(new SimEvent.JoinRejected(join.PlayerId, "unknownFaction"));
            return;
        }

        // Faction exclusivity is a sim rule, not a lobby courtesy.
        foreach (var other in w.Players.Values)
        {
            if (other.FactionId == join.FactionId)
            {
                w.Emit(new SimEvent.JoinRejected(join.PlayerId, "factionTaken"));
                return;
            }
        }

        var player = new PlayerState
        {
            Id = join.PlayerId,
            Name = join.Name,
            FactionId = join.FactionId,
            Pos = w.Map.HeroSpawn,
        };

        // Catch-up scrap: team-median personal totals per type.
        foreach (ScrapType type in System.Enum.GetValues<ScrapType>())
        {
            var amounts = w.Players.Values.Select(p => p.Scrap.GetValueOrDefault(type, 0))
                .OrderBy(v => v).ToList();
            if (amounts.Count > 0)
                player.Scrap[type] = amounts[amounts.Count / 2];
        }

        w.Players[join.PlayerId] = player;
        w.Emit(new SimEvent.PlayerJoined(join.PlayerId, join.Name, join.FactionId));
    }

    private static void ApplyLeave(World w, Command.Leave leave)
    {
        // Seat and faction held for rejoin; scaling steps down at the next
        // wave boundary. The sim otherwise doesn't care.
        if (w.Players.TryGetValue(leave.PlayerId, out var player))
            player.Connected = false;
    }

    private static void ApplyPlayerSync(World w, Command.PlayerSync sync)
    {
        if (w.Players.TryGetValue(sync.PlayerId, out var player))
            player.Pos = sync.Pos;
    }

    private static void ApplyPlaceTower(World w, Command.PlaceTower place)
    {
        var socket = w.Map.Sockets.FirstOrDefault(s => s.Id == place.SocketId);
        if (socket is null)
        {
            w.Emit(new SimEvent.BuildRejected(place.PlayerId, place.TowerId, place.SocketId, "unknownSocket"));
            return;
        }

        // Trap defs route to the trap path; tags must match both ways.
        if (Traps.All.TryGetValue(place.TowerId, out var trapDef))
        {
            ApplyPlaceTrap(w, place, trapDef, socket);
            return;
        }

        if (!Towers.All.TryGetValue(place.TowerId, out var def))
        {
            w.Emit(new SimEvent.BuildRejected(place.PlayerId, place.TowerId, place.SocketId, "unknownTower"));
            return;
        }

        bool tagOk = def.Kind == TowerKind.Barricade
            ? socket.Tag == SocketTag.Barricade
            : socket.Tag is SocketTag.Ground or SocketTag.Wall;
        if (!tagOk)
        {
            w.Emit(new SimEvent.BuildRejected(place.PlayerId, place.TowerId, place.SocketId,
                socket.Tag == SocketTag.Trap ? "trapSocket" : "wrongSocketTag"));
            return;
        }

        if (w.Towers.Any(t => t.SocketId == place.SocketId))
        {
            w.Emit(new SimEvent.BuildRejected(place.PlayerId, place.TowerId, place.SocketId, "occupied"));
            return;
        }

        int cost = def.Cost;
        if (w.Players.TryGetValue(place.PlayerId, out var placer) && placer.FactionId == Factions.Forge.Id)
            cost = (int)(cost * Balance.ForgeBuildDiscount);

        if (w.Money < cost)
        {
            w.Emit(new SimEvent.BuildRejected(place.PlayerId, place.TowerId, place.SocketId, "insufficientFunds"));
            return;
        }

        w.Money -= cost;
        var tower = new Tower
        {
            Id = w.NextId(),
            DefId = def.Id,
            SocketId = socket.Id,
            Pos = socket.Pos,
            Spent = cost,
            PathLevels = new int[def.UpgradePaths.Count],
        };
        w.Towers.Add(tower);
        w.Emit(new SimEvent.TowerPlaced(tower.Id, def.Id, socket.Id, place.PlayerId));
    }

    private static void ApplyPlaceTrap(World w, Command.PlaceTower place, TrapDef def, SocketDef socket)
    {
        if (socket.Tag != SocketTag.Trap)
        {
            w.Emit(new SimEvent.BuildRejected(place.PlayerId, def.Id, socket.Id, "wrongSocketTag"));
            return;
        }
        if (w.Traps.Any(t => t.SocketId == socket.Id))
        {
            w.Emit(new SimEvent.BuildRejected(place.PlayerId, def.Id, socket.Id, "occupied"));
            return;
        }
        if (w.Money < def.Cost)
        {
            w.Emit(new SimEvent.BuildRejected(place.PlayerId, def.Id, socket.Id, "insufficientFunds"));
            return;
        }
        foreach (var (type, amount) in def.ScrapCost)
        {
            if (w.TeamScrap.GetValueOrDefault(type, 0) < amount)
            {
                w.Emit(new SimEvent.BuildRejected(place.PlayerId, def.Id, socket.Id, "insufficientScrap"));
                return;
            }
        }

        w.Money -= def.Cost;
        foreach (var (type, amount) in def.ScrapCost)
            w.TeamScrap[type] -= amount;

        var trap = new Trap
        {
            Id = w.NextId(),
            DefId = def.Id,
            SocketId = socket.Id,
            Pos = socket.Pos,
            ChargesLeft = def.Charges,
        };
        w.Traps.Add(trap);
        w.Emit(new SimEvent.TowerPlaced(trap.Id, def.Id, socket.Id, place.PlayerId));
    }

    private static void ApplySellTower(World w, Command.SellTower sell)
    {
        var tower = w.Towers.FirstOrDefault(t => t.Id == sell.TowerId);
        if (tower is null) return;

        int refund = tower.Spent * Balance.SellRefundPercent / 100;
        w.Money += refund;
        w.Towers.Remove(tower);
        w.Emit(new SimEvent.TowerSold(tower.Id, refund));
    }

    private static void ApplyUpgradeTower(World w, Command.UpgradeTower upgrade)
    {
        var tower = w.Towers.FirstOrDefault(t => t.Id == upgrade.TowerId);
        if (tower is null)
        {
            w.Emit(new SimEvent.UpgradeRejected(upgrade.PlayerId, upgrade.TowerId, "unknownTower"));
            return;
        }

        var def = Towers.All[tower.DefId];
        if (upgrade.PathIndex < 0 || upgrade.PathIndex >= def.UpgradePaths.Count)
        {
            w.Emit(new SimEvent.UpgradeRejected(upgrade.PlayerId, upgrade.TowerId, "unknownPath"));
            return;
        }

        var path = def.UpgradePaths[upgrade.PathIndex];
        int currentLevel = tower.PathLevels[upgrade.PathIndex];
        if (currentLevel >= path.LevelCosts.Count)
        {
            w.Emit(new SimEvent.UpgradeRejected(upgrade.PlayerId, upgrade.TowerId, "maxLevel"));
            return;
        }

        int moneyCost = path.LevelCosts[currentLevel];
        if (w.Money < moneyCost)
        {
            w.Emit(new SimEvent.UpgradeRejected(upgrade.PlayerId, upgrade.TowerId, "insufficientFunds"));
            return;
        }

        // L4 is the breakpoint level: it also costs the scrap recipe, from the
        // team pool — the enemy-dependent economy loop.
        bool isBreakpoint = currentLevel + 1 == 4;
        if (isBreakpoint)
        {
            foreach (var (type, amount) in path.BreakpointRecipe)
            {
                if (w.TeamScrap.GetValueOrDefault(type, 0) < amount)
                {
                    w.Emit(new SimEvent.UpgradeRejected(upgrade.PlayerId, upgrade.TowerId, "insufficientScrap"));
                    return;
                }
            }
            foreach (var (type, amount) in path.BreakpointRecipe)
                w.TeamScrap[type] -= amount;
        }

        w.Money -= moneyCost;
        tower.Spent += moneyCost;
        tower.PathLevels[upgrade.PathIndex] = currentLevel + 1;
        w.Emit(new SimEvent.TowerUpgraded(tower.Id, path.Id, currentLevel + 1));
    }

    private static void ApplyPlayerHit(World w, Command.PlayerHit hit)
    {
        if (!w.Players.TryGetValue(hit.PlayerId, out var player) || !player.Alive) return;
        if (!Weapons.All.TryGetValue(hit.WeaponId, out var weapon)) return;
        if (!player.OwnedWeapons.Contains(hit.WeaponId)) return;
        if (player.WeaponCooldown > 0f) return;

        var enemy = w.Enemies.FirstOrDefault(e => e.Id == hit.EnemyId && !e.Dead);
        if (enemy is null || enemy.Burrowed) return;

        var build = player.BuildFor(weapon.Id);
        var enemyDef = Enemies.All[enemy.DefId];
        bool armored = enemyDef.FlatArmor > 0f || enemyDef.FrontArmorArcDegrees > 0f;

        player.WeaponCooldown = 1f / (weapon.ShotsPerSecond * build.RateFactor());

        var applies = weapon.Applies.Concat(build.ExtraApplies()).ToList();
        Damage(w, enemy, weapon.Damage * build.DamageFactor(armored),
            $"player{hit.PlayerId}", player.Pos, applies, hit.PlayerId,
            ignoreFlatArmor: build.IgnoresFlatArmor);
    }

    private static void ApplyBuyWeapon(World w, Command.BuyWeapon buy)
    {
        if (!w.Players.TryGetValue(buy.PlayerId, out var player)) return;
        if (!Weapons.All.TryGetValue(buy.WeaponId, out var weapon))
        {
            w.Emit(new SimEvent.PurchaseRejected(buy.PlayerId, buy.WeaponId, "unknownWeapon"));
            return;
        }
        if (player.OwnedWeapons.Contains(buy.WeaponId))
        {
            w.Emit(new SimEvent.PurchaseRejected(buy.PlayerId, buy.WeaponId, "alreadyOwned"));
            return;
        }
        if (w.Money < weapon.Cost)
        {
            w.Emit(new SimEvent.PurchaseRejected(buy.PlayerId, buy.WeaponId, "insufficientFunds"));
            return;
        }

        w.Money -= weapon.Cost;
        player.OwnedWeapons.Add(buy.WeaponId);
        player.WeaponId = buy.WeaponId;
        w.Emit(new SimEvent.WeaponBought(buy.PlayerId, buy.WeaponId));
    }

    private static void ApplySelectWeapon(World w, Command.SelectWeapon select)
    {
        if (w.Players.TryGetValue(select.PlayerId, out var player)
            && player.OwnedWeapons.Contains(select.WeaponId))
            player.WeaponId = select.WeaponId;
    }

    private static bool PayScrap(PlayerState player, IReadOnlyDictionary<ScrapType, int> recipe)
    {
        foreach (var (type, amount) in recipe)
            if (player.Scrap.GetValueOrDefault(type, 0) < amount) return false;
        foreach (var (type, amount) in recipe)
            player.Scrap[type] -= amount;
        return true;
    }

    private static void ApplyCraftAttachment(World w, Command.CraftAttachment craft)
    {
        if (!w.Players.TryGetValue(craft.PlayerId, out var player)) return;
        if (!player.OwnedWeapons.Contains(craft.WeaponId))
        {
            w.Emit(new SimEvent.CraftRejected(craft.PlayerId, craft.AttachmentId, "weaponNotOwned"));
            return;
        }
        if (!Attachments.All.TryGetValue(craft.AttachmentId, out var def))
        {
            w.Emit(new SimEvent.CraftRejected(craft.PlayerId, craft.AttachmentId, "unknownAttachment"));
            return;
        }
        if (!PayScrap(player, def.Recipe))
        {
            w.Emit(new SimEvent.CraftRejected(craft.PlayerId, craft.AttachmentId, "insufficientScrap"));
            return;
        }

        player.BuildFor(craft.WeaponId).Attachments[def.Slot] = def.Id;
        w.Emit(new SimEvent.AttachmentCrafted(craft.PlayerId, craft.WeaponId, def.Id));
    }

    private static void ApplySelectAmmo(World w, Command.SelectAmmo select)
    {
        if (!w.Players.TryGetValue(select.PlayerId, out var player)) return;
        if (!Ammo.All.TryGetValue(select.AmmoId, out var def))
        {
            w.Emit(new SimEvent.CraftRejected(select.PlayerId, select.AmmoId, "unknownAmmo"));
            return;
        }
        if (!player.CraftedAmmo.Contains(def.Id))
        {
            if (!PayScrap(player, def.Recipe))
            {
                w.Emit(new SimEvent.CraftRejected(select.PlayerId, select.AmmoId, "insufficientScrap"));
                return;
            }
            player.CraftedAmmo.Add(def.Id);
        }

        player.BuildFor(select.WeaponId).AmmoId = def.Id;
        w.Emit(new SimEvent.AmmoSelected(select.PlayerId, select.WeaponId, def.Id));
    }

    private static void ApplyUseAbility(World w, Command.UseAbility ability)
    {
        if (!w.Players.TryGetValue(ability.PlayerId, out var player) || !player.Alive) return;
        if (player.AbilityCooldown > 0f) return;
        if (!Factions.All.TryGetValue(player.FactionId, out var faction)) return;

        player.AbilityCooldown = faction.CooldownSeconds;
        w.Emit(new SimEvent.AbilityUsed(ability.PlayerId, faction.AbilityId));

        switch (faction.AbilityId)
        {
            case "overdrive":
                foreach (var tower in w.Towers)
                {
                    if (tower.Pos.DistanceTo(player.Pos) <= faction.RadiusMeters)
                    {
                        tower.BuffTimer = faction.DurationSeconds;
                        tower.BuffFactor = faction.Magnitude;
                    }
                }
                break;

            case "ignitionWave":
                foreach (var enemy in w.Enemies)
                {
                    if (!enemy.Dead && enemy.Pos.DistanceTo(ability.TargetPos) <= faction.RadiusMeters)
                        ApplyStatus(w, enemy, Statuses.Burn.Id, $"player{player.Id}", player.Id);
                }
                break;
        }
    }

    private static void ApplyRevive(World w, Command.Revive revive)
    {
        if (!w.Players.TryGetValue(revive.PlayerId, out var reviver) || !reviver.Alive) return;
        if (!w.Players.TryGetValue(revive.TargetPlayerId, out var target) || !target.Downed) return;
        if (reviver.Pos.DistanceTo(target.Pos) > Balance.ReviveRangeMeters) return;

        // Held interaction: the client streams Revive while the key is held;
        // each command advances progress by one tick's worth.
        target.ReviveProgress += Balance.Dt;
        if (target.ReviveProgress >= Balance.ReviveSeconds)
        {
            target.Downed = false;
            target.BleedoutTimer = 0f;
            target.ReviveProgress = 0f;
            target.Hp = Balance.PlayerMaxHp * 0.5f;
            w.Emit(new SimEvent.PlayerRevived(target.Id, reviver.Id));
        }
    }

    // =====================================================================
    // Waves
    // =====================================================================

    private static void UpdateWaves(World w)
    {
        if (w.Phase == MatchPhase.Intermission)
        {
            if (w.WaitForPlayers && w.ConnectedPlayerCount == 0) return;
            w.PhaseTimer -= Balance.Dt;
            if (w.PhaseTimer <= 0f)
            {
                // Anyone in bleedout comes back at the wave boundary.
                foreach (var player in w.Players.Values)
                {
                    if (player.Downed && player.BleedoutTimer <= 0f)
                        RespawnPlayer(w, player);
                }

                w.WaveIndex++;
                int playerCount = System.Math.Max(1, w.ConnectedPlayerCount);
                w.PendingSpawns = WavePlan.PlanWave(w.Seed, w.Map, w.WaveIndex, playerCount);
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

            // Barricade route gating, resolved at spawn time (never mid-walk):
            // a gated shortcut with a living barricade sends the spawn down its
            // fallback route instead.
            int routeIndex = entry.RouteIndex;
            var routeDef = w.Map.Routes[routeIndex];
            if (routeDef.BarricadeGate is { } gate
                && w.Towers.Any(t => t.SocketId == gate && Towers.All[t.DefId].Kind == TowerKind.Barricade)
                && routeDef.FallbackRouteId is { } fallback)
            {
                for (int r = 0; r < w.Map.Routes.Count; r++)
                    if (w.Map.Routes[r].Id == fallback) { routeIndex = r; break; }
            }

            var route = w.Map.Routes[routeIndex];
            var enemy = new Enemy
            {
                Id = w.NextId(),
                DefId = def.Id,
                Hp = def.Hp * entry.HpFactor,
                MaxHp = def.Hp * entry.HpFactor,
                Shield = def.Shield * entry.HpFactor,
                RouteIndex = routeIndex,
                LateralOffset = entry.LateralOffset,
                Pos = route.Waypoints[0],
                Facing = (route.Waypoints[1] - route.Waypoints[0]).Normalized(),
                Bounty = def.Bounty,
                LeakDamage = def.LeakDamage,
                WaveIndex = w.WaveIndex,
            };
            w.Enemies.Add(enemy);
            w.Emit(new SimEvent.EnemySpawned(enemy.Id, def.Id, w.WaveIndex));
            w.PendingSpawns.RemoveAt(i);
        }
    }

    // =====================================================================
    // Statuses
    // =====================================================================

    private static void UpdateStatuses(World w)
    {
        foreach (var enemy in w.Enemies)
        {
            if (enemy.Dead) continue;

            bool controlled = false;
            for (int c = 0; c < enemy.Statuses.Length; c++)
            {
                ref var slot = ref enemy.Statuses[c];
                if (!slot.Active) continue;

                var def = Statuses.All[slot.StatusId!];
                if (def.DamagePerSecond > 0f)
                {
                    // DoT attributes to whoever applied it — bounty and scrap
                    // credit survive the burn.
                    Damage(w, enemy, def.DamagePerSecond * Balance.Dt, slot.Source, enemy.Pos, null,
                        SourcePlayerId(slot.Source));
                }
                if (def.HardControl) controlled = true;

                slot.TimeLeft -= Balance.Dt;
                if (slot.TimeLeft <= 0f)
                    slot.StatusId = null;
            }

            // CC-resist gauge: fills while controlled, decays otherwise.
            enemy.CcResist = controlled
                ? MathF.Min(1f, enemy.CcResist + Balance.CcResistFillPerSecond * Balance.Dt)
                : MathF.Max(0f, enemy.CcResist - Balance.CcResistDecayPerSecond * Balance.Dt);

            // Shield regen after a lull (Warden).
            var enemyDef = Enemies.All[enemy.DefId];
            if (enemyDef.Shield > 0f)
            {
                enemy.ShieldTimer = MathF.Max(0f, enemy.ShieldTimer - Balance.Dt);
                if (enemy.ShieldTimer <= 0f && enemy.Shield < enemyDef.Shield)
                    enemy.Shield = MathF.Min(enemyDef.Shield, enemy.Shield + Balance.ShieldRegenPerSecond * Balance.Dt);
            }
        }
    }

    /// <summary>Application-time resolution: reactions first (consume the active
    /// status, skip the incoming one), then strongest-wins within the channel.</summary>
    private static void ApplyStatus(World w, Enemy enemy, string statusId, string source, int? playerId)
    {
        var incoming = Statuses.All[statusId];

        // Reaction scan across all active channels.
        for (int c = 0; c < enemy.Statuses.Length; c++)
        {
            ref var slot = ref enemy.Statuses[c];
            if (!slot.Active) continue;

            var reaction = Reactions.Match(slot.StatusId!, statusId);
            if (reaction is null) continue;

            slot.StatusId = null;   // consume the active half
            float burst = enemy.MaxHp * reaction.BurstFraction;
            w.Emit(new SimEvent.ReactionTriggered(enemy.Id, reaction.Id, burst));
            if (burst > 0f)
                Damage(w, enemy, burst, source, enemy.Pos, null, playerId);

            // Reaction output goes straight into its slot — never re-scanned,
            // which is the closure guarantee (no reaction chains).
            if (reaction.EmitStatus is { } emitted && !enemy.Dead)
            {
                var emitDef = Statuses.All[emitted];
                if (!emitDef.HardControl || enemy.CcResist < 1f)
                {
                    ref var emitSlot = ref enemy.Statuses[(int)emitDef.Channel];
                    emitSlot.StatusId = emitDef.Id;
                    emitSlot.TimeLeft = emitDef.MaxDurationSeconds;
                    emitSlot.Source = source;
                    w.Emit(new SimEvent.StatusApplied(enemy.Id, emitDef.Id, source));
                }
            }
            return;                 // incoming status consumed by the reaction
        }

        // Burn cannot ignite a shielded target — the shield eats it whole.
        if (incoming.Channel == Channel.Thermal && enemy.Shield > 0f)
            return;

        // Hard control is gated by the cc-resist gauge.
        if (incoming.HardControl && enemy.CcResist >= 1f)
            return;

        float duration = incoming.MaxDurationSeconds;
        // Ember passive: that player's burns last longer.
        if (statusId == Statuses.Burn.Id && playerId is int pid
            && w.Players.TryGetValue(pid, out var applier)
            && applier.FactionId == Factions.Ember.Id)
            duration *= Balance.EmberBurnDurationFactor;

        ref var target = ref enemy.Statuses[(int)incoming.Channel];
        if (target.Active)
        {
            var active = Statuses.All[target.StatusId!];
            if (active.Id == incoming.Id)
            {
                target.TimeLeft = duration;   // refresh
                target.Source = source;
                return;
            }
            if (active.Magnitude >= incoming.Magnitude)
                return;                       // strongest wins, weaker ignored
        }

        target.StatusId = incoming.Id;
        target.TimeLeft = duration;
        target.Source = source;
        w.Emit(new SimEvent.StatusApplied(enemy.Id, incoming.Id, source));
    }

    // =====================================================================
    // Movement
    // =====================================================================

    private static void MoveEnemies(World w)
    {
        foreach (var enemy in w.Enemies)
        {
            if (enemy.Dead) continue;

            var def = Enemies.All[enemy.DefId];
            float speed = def.SpeedMetersPerSec;

            ref var movement = ref enemy.Statuses[(int)Channel.Movement];
            if (movement.Active)
                speed *= Statuses.All[movement.StatusId!].SpeedFactor;

            // Hard control: dead stop.
            ref var control = ref enemy.Statuses[(int)Channel.Control];
            if (control.Active && Statuses.All[control.StatusId!].HardControl)
                speed = 0f;

            // Burrow cycle (distance-based, deterministic): underground for the
            // first stretch of every cycle, surfaced for the rest.
            if (def.Burrower)
                enemy.Burrowed = enemy.TotalTraveled % Balance.BurrowCycleMeters < Balance.BurrowedMeters;

            var legs = w.RouteLegLengths[enemy.RouteIndex];
            var waypoints = w.Map.Routes[enemy.RouteIndex].Waypoints;
            float remaining = speed * Balance.Dt;

            while (remaining > 0f && enemy.Leg < legs.Length)
            {
                float legLeft = legs[enemy.Leg] - enemy.LegProgress;
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

            if (enemy.Leg >= legs.Length)
            {
                enemy.Dead = true;
                w.Lives -= enemy.LeakDamage;
                w.Emit(new SimEvent.EnemyLeaked(enemy.Id, enemy.DefId, enemy.LeakDamage));
                continue;
            }

            var a = waypoints[enemy.Leg];
            var b = waypoints[enemy.Leg + 1];
            enemy.Facing = (b - a).Normalized();
            var spine = Vec3.Lerp(a, b, enemy.LegProgress / legs[enemy.Leg]);

            // Lateral scatter: offset perpendicular to travel on the XZ plane.
            var perp = new Vec3(-enemy.Facing.Z, 0f, enemy.Facing.X);
            enemy.Pos = spine + perp * enemy.LateralOffset;
        }
    }

    // =====================================================================
    // Players
    // =====================================================================

    private static void UpdatePlayers(World w)
    {
        foreach (var player in w.Players.Values)
        {
            player.WeaponCooldown = MathF.Max(0f, player.WeaponCooldown - Balance.Dt);
            player.AbilityCooldown = MathF.Max(0f, player.AbilityCooldown - Balance.Dt);

            if (player.RespawnTimer > 0f)
            {
                player.RespawnTimer -= Balance.Dt;
                if (player.RespawnTimer <= 0f)
                    RespawnPlayer(w, player);
                continue;
            }

            if (player.Downed)
            {
                player.BleedoutTimer -= Balance.Dt;
                // Progress decays if nobody is holding the revive.
                player.ReviveProgress = MathF.Max(0f, player.ReviveProgress - Balance.Dt * 0.5f);
                continue;
            }

            if (!player.Connected) continue;

            // Contact damage from ground enemies standing in the hero.
            float contact = 0f;
            foreach (var enemy in w.Enemies)
            {
                if (enemy.Dead) continue;
                var def = Enemies.All[enemy.DefId];
                if (def.ContactDamage <= 0f) continue;
                if (enemy.Pos.DistanceTo(player.Pos) <= Balance.ContactRadiusMeters + 0.6f)
                    contact += def.ContactDamage;
            }

            if (contact > 0f)
            {
                player.Hp -= contact * Balance.Dt;
                player.RegenDelay = Balance.PlayerRegenDelaySeconds;
                w.Emit(new SimEvent.PlayerDamaged(player.Id, contact * Balance.Dt, "contact"));

                if (player.Hp <= 0f)
                {
                    player.Hp = 0f;
                    if (w.ConnectedPlayerCount <= 1)
                    {
                        player.RespawnTimer = Balance.SoloRespawnSeconds;
                    }
                    else
                    {
                        player.Downed = true;
                        player.BleedoutTimer = Balance.BleedoutSeconds;
                        player.ReviveProgress = 0f;
                    }
                    w.Emit(new SimEvent.PlayerDowned(player.Id));
                }
            }
            else
            {
                player.RegenDelay = MathF.Max(0f, player.RegenDelay - Balance.Dt);
                if (player.RegenDelay <= 0f && player.Hp < Balance.PlayerMaxHp)
                    player.Hp = MathF.Min(Balance.PlayerMaxHp, player.Hp + Balance.PlayerRegenPerSecond * Balance.Dt);
            }
        }
    }

    private static void RespawnPlayer(World w, PlayerState player)
    {
        player.Downed = false;
        player.RespawnTimer = 0f;
        player.BleedoutTimer = 0f;
        player.ReviveProgress = 0f;
        player.Hp = Balance.PlayerMaxHp;
        player.Pos = w.Map.HeroSpawn;
        w.Emit(new SimEvent.PlayerRespawned(player.Id));
    }

    // =====================================================================
    // Towers
    // =====================================================================

    private static void FireTowers(World w)
    {
        foreach (var tower in w.Towers)
        {
            var def = Towers.All[tower.DefId];

            if (tower.BuffTimer > 0f)
            {
                tower.BuffTimer -= Balance.Dt;
                if (tower.BuffTimer <= 0f) tower.BuffFactor = 1f;
            }

            if (def.Kind == TowerKind.Barricade)
                continue;   // no weapon; it works by existing (route gate)

            if (def.Kind == TowerKind.Tesla)
            {
                tower.Cooldown = MathF.Max(0f, tower.Cooldown - Balance.Dt);
                if (tower.Cooldown > 0f) continue;

                var first = PickTarget(w, tower, def);
                if (first is null) continue;

                tower.Cooldown = 1f / EffectiveRate(tower, def);
                w.Emit(new SimEvent.TowerFired(tower.Id, first.Id));

                // Instant arc: the first target, then hops to the nearest other
                // target within chain range, damage falling off per hop.
                float damage = EffectiveDamage(tower, def);
                var struck = first;
                var hitIds = new HashSet<int> { first.Id };
                DealTeslaDamage(w, tower, struck, damage, def);

                for (int hop = 0; hop < def.ChainJumps; hop++)
                {
                    Enemy? next = null;
                    float bestDist = def.ChainRange;
                    foreach (var candidate in w.Enemies)
                    {
                        if (candidate.Dead || candidate.Burrowed || hitIds.Contains(candidate.Id)) continue;
                        if (!def.TargetLayers.Contains(Enemies.All[candidate.DefId].Layer)) continue;
                        float dist = struck.Pos.DistanceTo(candidate.Pos);
                        if (dist < bestDist) { bestDist = dist; next = candidate; }
                    }
                    if (next is null) break;

                    damage *= def.ChainFalloff;
                    hitIds.Add(next.Id);
                    DealTeslaDamage(w, tower, next, damage, def);
                    struck = next;
                }
                continue;
            }

            if (def.Kind == TowerKind.ChillAura)
            {
                float auraRange = EffectiveRange(tower, def);
                foreach (var enemy in w.Enemies)
                {
                    if (enemy.Dead) continue;
                    if (!def.TargetLayers.Contains(Enemies.All[enemy.DefId].Layer)) continue;
                    if (tower.Pos.DistanceTo(enemy.Pos) > auraRange) continue;
                    foreach (var statusId in def.Applies)
                        ApplyStatus(w, enemy, statusId, $"tower{tower.Id}", null);
                }
                continue;
            }

            tower.Cooldown = MathF.Max(0f, tower.Cooldown - Balance.Dt);
            if (tower.Cooldown > 0f) continue;

            var target = PickTarget(w, tower, def);
            if (target is null) continue;

            tower.Cooldown = 1f / EffectiveRate(tower, def);
            w.Projectiles.Add(new Projectile
            {
                Id = w.NextId(),
                FiredBy = tower.Id,
                TargetId = target.Id,
                Pos = tower.Pos + new Vec3(0f, 1.5f, 0f),
                Speed = def.ProjectileSpeed,
                Damage = EffectiveDamage(tower, def),
                SplashRadius = def.SplashRadius,
                SplashFalloff = def.SplashFalloff,
            });
            w.Emit(new SimEvent.TowerFired(tower.Id, target.Id));
        }
    }

    private static Enemy? PickTarget(World w, Tower tower, TowerDef def)
    {
        float range = EffectiveRange(tower, def);
        Enemy? best = null;
        float bestTraveled = -1f;

        foreach (var enemy in w.Enemies)
        {
            if (enemy.Dead) continue;
            if (!def.TargetLayers.Contains(Enemies.All[enemy.DefId].Layer)) continue;

            if (enemy.Burrowed) continue;   // untargetable underground

            float distance = tower.Pos.DistanceTo(enemy.Pos);
            if (distance > range || distance < def.MinRangeMeters) continue;
            if (SightBlocked(w, tower.Pos, enemy)) continue;

            if (enemy.TotalTraveled > bestTraveled)
            {
                bestTraveled = enemy.TotalTraveled;
                best = enemy;
            }
        }
        return best;
    }

    /// <summary>Monolith is living cover: a sight-blocker standing between the
    /// tower and its target shields whatever walks behind it.</summary>
    private static bool SightBlocked(World w, Vec3 from, Enemy target)
    {
        foreach (var blocker in w.Enemies)
        {
            if (blocker.Dead || blocker.Id == target.Id) continue;
            if (!Enemies.All[blocker.DefId].BlocksSight) continue;

            var toTarget = target.Pos - from;
            var toBlocker = blocker.Pos - from;
            float targetDist = toTarget.Length();
            float blockerDist = toBlocker.Length();
            if (blockerDist >= targetDist) continue;

            // Perpendicular distance of the blocker from the fire line.
            var dir = toTarget * (1f / targetDist);
            float along = toBlocker.X * dir.X + toBlocker.Y * dir.Y + toBlocker.Z * dir.Z;
            if (along <= 0f) continue;
            var closest = from + dir * along;
            if (closest.DistanceTo(blocker.Pos) < 1.6f)
                return true;
        }
        return false;
    }

    private static float EffectiveDamage(Tower tower, TowerDef def) =>
        def.Damage * PathFactor(tower, def, "damage");

    private static float EffectiveRange(Tower tower, TowerDef def) =>
        def.RangeMeters * PathFactor(tower, def, "range");

    private static float EffectiveRate(Tower tower, TowerDef def) =>
        def.ShotsPerSecond * PathFactor(tower, def, "rate") * tower.BuffFactor;

    private static float PathFactor(Tower tower, TowerDef def, string pathId)
    {
        for (int i = 0; i < def.UpgradePaths.Count; i++)
        {
            if (def.UpgradePaths[i].Id == pathId)
                return MathF.Pow(def.UpgradePaths[i].PerLevelFactor, tower.PathLevels[i]);
        }
        return 1f;
    }

    // =====================================================================
    // Projectiles
    // =====================================================================

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
                var applies = tower is not null ? Towers.All[tower.DefId].Applies : null;

                if (projectile.SplashRadius > 0f)
                {
                    foreach (var enemy in w.Enemies)
                    {
                        if (enemy.Dead) continue;
                        float splashDist = enemy.Pos.DistanceTo(target.Pos);
                        if (splashDist > projectile.SplashRadius) continue;

                        float falloff = 1f - (1f - projectile.SplashFalloff)
                            * (splashDist / projectile.SplashRadius);
                        DealProjectileDamage(w, tower, projectile, enemy, projectile.Damage * falloff, applies);
                    }
                }
                else
                {
                    DealProjectileDamage(w, tower, projectile, target, projectile.Damage, applies);
                }
            }
            else
            {
                projectile.Pos += (aim - projectile.Pos).Normalized() * stepLength;
            }
        }
    }

    private static void DealTeslaDamage(World w, Tower tower, Enemy enemy, float amount, TowerDef def)
    {
        Damage(w, enemy, amount, $"tower{tower.Id}", tower.Pos, def.Applies, null);
        tower.DamageDealt += amount;
        if (enemy.Dead) tower.Kills++;
    }

    /// <summary>Armed traps fire on surfaced ground enemies in radius: one
    /// charge per trigger event, hitting everything inside at once.</summary>
    private static void TriggerTraps(World w)
    {
        foreach (var trap in w.Traps)
        {
            if (trap.RearmTimer > 0f)
            {
                trap.RearmTimer -= Balance.Dt;
                continue;
            }
            if (trap.ChargesLeft <= 0) continue;

            var def = Traps.All[trap.DefId];
            bool fired = false;

            foreach (var enemy in w.Enemies)
            {
                if (enemy.Dead || enemy.Burrowed) continue;
                var enemyDef = Enemies.All[enemy.DefId];
                if (enemyDef.Layer != EnemyLayer.Ground) continue;
                if (trap.Pos.DistanceTo(enemy.Pos) > def.TriggerRadius) continue;

                fired = true;
                if (def.Damage > 0f)
                    Damage(w, enemy, def.Damage, $"trap{trap.Id}", trap.Pos, null, null);
                if (def.Applies is { } statusId && !enemy.Dead)
                    ApplyStatus(w, enemy, statusId, $"trap{trap.Id}", null);
                if (def.KnockbackMeters > 0f && !enemy.Dead)
                    KnockBack(w, enemy, def.KnockbackMeters / MathF.Max(enemyDef.Mass, 0.25f));
            }

            if (fired)
            {
                trap.ChargesLeft--;
                trap.RearmTimer = def.RearmSeconds;
                w.Emit(new SimEvent.TowerFired(trap.Id, 0));
            }
        }
        w.Traps.RemoveAll(t => t.ChargesLeft <= 0 && t.RearmTimer <= 0f);
    }

    /// <summary>Knockback is an instantaneous route displacement, not a status:
    /// walk the enemy backward along its legs.</summary>
    private static void KnockBack(World w, Enemy enemy, float meters)
    {
        float remaining = meters;
        while (remaining > 0f)
        {
            if (enemy.LegProgress >= remaining)
            {
                enemy.LegProgress -= remaining;
                enemy.TotalTraveled -= remaining;
                break;
            }
            remaining -= enemy.LegProgress;
            enemy.TotalTraveled -= enemy.LegProgress;
            if (enemy.Leg == 0) { enemy.LegProgress = 0f; break; }
            enemy.Leg--;
            enemy.LegProgress = w.RouteLegLengths[enemy.RouteIndex][enemy.Leg];
        }
        enemy.TotalTraveled = MathF.Max(0f, enemy.TotalTraveled);
    }

    private static void DealProjectileDamage(
        World w, Tower? tower, Projectile projectile, Enemy enemy, float amount, IReadOnlyList<string>? applies)
    {
        Damage(w, enemy, amount, $"tower{projectile.FiredBy}", projectile.Pos, applies, null);
        if (tower is not null)
        {
            tower.DamageDealt += amount;
            if (enemy.Dead) tower.Kills++;
        }
    }

    /// <summary>M1 stub — physical player ordnance (grenades etc.) arrives with M2+.</summary>
    private static void StepPlayerOrdnance(World w)
    {
    }

    /// <summary>Split-on-death: dead Clusters birth their Motes at the parent's
    /// route position with seeded radial scatter. Children spawned here are
    /// stepped from next tick (they join the lists after this phase).</summary>
    private static void ResolveDeaths(World w)
    {
        var births = new List<Enemy>();
        foreach (var enemy in w.Enemies)
        {
            if (!enemy.Dead) continue;
            var def = Enemies.All[enemy.DefId];
            if (def.SplitInto is null || def.SplitCount <= 0) continue;
            if (enemy.Leg >= w.RouteLegLengths[enemy.RouteIndex].Length) continue; // leaked, not killed

            var childDef = Enemies.All[def.SplitInto];
            var rng = Util.RngStreams.StreamFor(w.Seed, "split", (uint)enemy.Id);
            float hpFactor = enemy.MaxHp / def.Hp;   // children inherit wave scaling

            for (int i = 0; i < def.SplitCount; i++)
            {
                var child = new Enemy
                {
                    Id = w.NextId(),
                    DefId = childDef.Id,
                    Hp = childDef.Hp * hpFactor,
                    MaxHp = childDef.Hp * hpFactor,
                    RouteIndex = enemy.RouteIndex,
                    Leg = enemy.Leg,
                    LegProgress = enemy.LegProgress,
                    TotalTraveled = enemy.TotalTraveled,
                    LateralOffset = (rng.NextFloat() - 0.5f) * MathF.Max(childDef.ScatterWidth, 2f),
                    Pos = enemy.Pos,
                    Facing = enemy.Facing,
                    Bounty = childDef.Bounty,
                    LeakDamage = childDef.LeakDamage,
                    WaveIndex = enemy.WaveIndex,
                };
                births.Add(child);
                w.Emit(new SimEvent.EnemySpawned(child.Id, child.DefId, child.WaveIndex));
            }
        }
        w.Enemies.AddRange(births);
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

    // =====================================================================
    // Damage
    // =====================================================================

    private static void Damage(
        World w, Enemy enemy, float amount, string source, Vec3 sourcePos,
        IReadOnlyList<string>? applies, int? playerId, bool ignoreFlatArmor = false)
    {
        if (enemy.Dead) return;

        var def = Enemies.All[enemy.DefId];

        // Directional armor (Aegis): reduced inside the front arc, amplified
        // from directly behind. DoT/reactions pass sourcePos == enemy.Pos and
        // skip the check.
        var fromSource = enemy.Pos - sourcePos;
        float sourceDist = fromSource.Length();
        if (def.FrontArmorArcDegrees > 0f && sourceDist > 0.01f)
        {
            var toSource = fromSource * (-1f / sourceDist);
            float dot = toSource.X * enemy.Facing.X + toSource.Y * enemy.Facing.Y + toSource.Z * enemy.Facing.Z;
            float angleDegrees = MathF.Acos(System.Math.Clamp(dot, -1f, 1f)) * (180f / MathF.PI);

            if (angleDegrees <= def.FrontArmorArcDegrees / 2f)
                amount *= def.FrontArmorFactor;
            else if (angleDegrees >= 150f)
                amount *= def.RearWeakFactor;
        }

        // Vulnerability channel (mark).
        ref var vulnerability = ref enemy.Statuses[(int)Channel.Vulnerability];
        if (vulnerability.Active)
            amount *= Statuses.All[vulnerability.StatusId!].DamageTakenFactor;

        // Flat armor per hit, softened by shred (defense channel). Shred also
        // weakens Aegis's front plate — the counter to armor you can't out-level.
        float flatArmor = def.FlatArmor;
        ref var defense = ref enemy.Statuses[(int)Channel.Defense];
        if (defense.Active)
        {
            flatArmor = MathF.Max(0f, flatArmor + Statuses.All[defense.StatusId!].ArmorDelta);
            if (def.FrontArmorArcDegrees > 0f)
                amount *= 1.35f;    // shredded plating: the front arc leaks
        }
        if (flatArmor > 0f && amount > 0f && !ignoreFlatArmor)
            amount = MathF.Max(0.5f, amount - flatArmor);

        // Shield soaks first (Warden) and resets its regen lull. Statuses still
        // apply through a shield — except burn, which ApplyStatus refuses while
        // shielded (the "can't ignite a shielded Warden" identity).
        float total = amount;
        if (enemy.Shield > 0f && amount > 0f)
        {
            enemy.ShieldTimer = Balance.ShieldRegenDelaySeconds;
            float soaked = MathF.Min(enemy.Shield, amount);
            enemy.Shield -= soaked;
            amount -= soaked;
        }

        enemy.Hp -= amount;
        w.Emit(new SimEvent.EnemyDamaged(enemy.Id, total, source));

        if (applies is not null)
        {
            foreach (var statusId in applies)
            {
                if (enemy.Dead) break;
                ApplyStatus(w, enemy, statusId, source, playerId);
            }
        }

        if (enemy.Hp <= 0f && !enemy.Dead)
        {
            enemy.Dead = true;
            w.Money += enemy.Bounty;
            w.Emit(new SimEvent.EnemyDied(enemy.Id, enemy.DefId, enemy.Bounty, source));
            DropScrap(w, enemy, def, playerId);
        }
    }

    /// <summary>Scrap split: a team share funds tower upgrades; the personal
    /// share funds the killer's gunsmith. M1 simplification: personal share goes
    /// to the killing player only (participation tracking arrives at M2); tower
    /// kills bank everything to the team.</summary>
    private static void DropScrap(World w, Enemy enemy, EnemyDef def, int? killerPlayerId)
    {
        if (def.ScrapYield.Count == 0) return;

        var parts = new List<string>();
        foreach (var (type, amount) in def.ScrapYield)
        {
            int teamShare = killerPlayerId is null
                ? amount
                : (int)MathF.Round(amount * Balance.ScrapTeamShare, MidpointRounding.AwayFromZero);
            int personal = amount - teamShare;

            if (teamShare > 0)
                w.TeamScrap[type] = w.TeamScrap.GetValueOrDefault(type, 0) + teamShare;
            if (personal > 0 && killerPlayerId is int pid && w.Players.TryGetValue(pid, out var killer))
                killer.Scrap[type] = killer.Scrap.GetValueOrDefault(type, 0) + personal;

            parts.Add($"{type}:{amount}");
        }

        w.Emit(new SimEvent.ScrapDropped(enemy.Id, string.Join(",", parts)));
    }

    private static int? SourcePlayerId(string source) =>
        source.StartsWith("player") && int.TryParse(source.AsSpan(6), out int id) ? id : null;
}
