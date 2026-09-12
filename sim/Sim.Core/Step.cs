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
    /// <summary>Where a dropped pile of each scrap type lands relative to the
    /// body, so a Monolith's three drops read as three things rather than one
    /// pile. Golden angle (2.399963 rad), 0.6 m out, one per <see cref="ScrapType"/>
    /// in declaration order.
    ///
    /// A table rather than the `Cos`/`Sin` pair it used to be: the angle is a
    /// function of the type and nothing else, so there are exactly four answers
    /// and a transcendental in the tick bought nothing. It was already
    /// deliberately not RNG — this makes it deliberately not libm either. See
    /// <see cref="DetMath"/>.</summary>
    private static readonly Vec3[] ScrapDropOffsets =
    {
        new Vec3(0.600000000f, 0f, 0.000000000f),    // Alloy
        new Vec3(-0.442421234f, 0f, 0.405294278f),   // Flux
        new Vec3(0.052455160f, 0f, -0.597702649f),   // Plating
        new Vec3(0.365063645f, 0f, 0.476160199f),    // Gravium
    };

    public static void Advance(World w)
    {
        if (w.IsOver) return;

        w.Tick++;
        w.Events.Clear();

        ApplyCommands(w);
        UpdateWaves(w);
        UpdateStatuses(w);
        ApplyHealAuras(w);
        MoveEnemies(w);
        SiegeStructures(w);
        TriggerTraps(w);
        UpdatePlayers(w);
        FireTowers(w);
        StepTowerProjectiles(w);
        StepPlayerOrdnance(w);
        ResolveDeaths(w);
        UpdatePickups(w);
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
                case Command.SetFaction pick: ApplySetFaction(w, pick); break;
                case Command.Launch launch: ApplyLaunch(w, launch); break;
                case Command.PlayerSync sync: ApplyPlayerSync(w, sync); break;
                case Command.PlaceTower place: ApplyPlaceTower(w, place); break;
                case Command.SellTower sell: ApplySellTower(w, sell); break;
                case Command.UpgradeTower upgrade: ApplyUpgradeTower(w, upgrade); break;
                case Command.StartWave:
                    if (w.Phase == MatchPhase.Intermission && !w.Lobby) w.PhaseTimer = 0f;
                    break;
                case Command.PlayerHit hit: ApplyPlayerHit(w, hit); break;
                case Command.PlayerMelee swing: ApplyPlayerMelee(w, swing); break;
                case Command.Reload reload:
                    if (w.Players.TryGetValue(reload.PlayerId, out var reloader) && reloader.Alive)
                        BeginReload(w, reloader, reloader.WeaponId);
                    break;
                case Command.BuyMelee buyMelee: ApplyBuyMelee(w, buyMelee); break;
                case Command.CraftMeleeAttachment craftMelee: ApplyCraftMeleeAttachment(w, craftMelee); break;
                case Command.UpgradeMelee upMelee: ApplyUpgradeMelee(w, upMelee); break;
                case Command.BuyWeapon buy: ApplyBuyWeapon(w, buy); break;
                case Command.SelectWeapon select: ApplySelectWeapon(w, select); break;
                case Command.UseAbility ability: ApplyUseAbility(w, ability); break;
                case Command.Revive revive: ApplyRevive(w, revive); break;
                case Command.CraftAttachment craft: ApplyCraftAttachment(w, craft); break;
                case Command.SelectAmmo ammo: ApplySelectAmmo(w, ammo); break;
                case Command.PackAPunch pack: ApplyPackAPunch(w, pack); break;
                case Command.EnterVehicle enter: ApplyEnterVehicle(w, enter); break;
                case Command.ExitVehicle exit: LeaveSeat(w, exit.PlayerId, "left"); break;
                case Command.VehicleSync vsync: ApplyVehicleSync(w, vsync); break;
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
            FactionLevel = System.Math.Clamp(join.FactionLevel, 1, Factions.MaxLevel),
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

    /// <summary>Faction exclusivity holds in the lobby exactly as it does at
    /// join: the pick is refused, not silently reassigned.</summary>
    private static void ApplySetFaction(World w, Command.SetFaction pick)
    {
        if (!w.Lobby || !w.Players.TryGetValue(pick.PlayerId, out var player)) return;
        if (!Factions.All.ContainsKey(pick.FactionId))
        {
            w.Emit(new SimEvent.JoinRejected(pick.PlayerId, "unknownFaction"));
            return;
        }
        foreach (var other in w.Players.Values)
        {
            if (other.Id != player.Id && other.Connected && other.FactionId == pick.FactionId)
            {
                w.Emit(new SimEvent.JoinRejected(pick.PlayerId, "factionTaken"));
                return;
            }
        }
        player.FactionId = pick.FactionId;
        player.FactionLevel = System.Math.Clamp(pick.FactionLevel, 1, Factions.MaxLevel);
        w.Emit(new SimEvent.FactionChanged(player.Id, player.FactionId));
    }

    private static void ApplyLaunch(World w, Command.Launch launch)
    {
        if (!w.Lobby || launch.PlayerId != w.LaunchSeat) return;
        w.Lobby = false;
        w.Phase = MatchPhase.Intermission;
        w.PhaseTimer = Balance.IntermissionSeconds;
        w.Emit(new SimEvent.MatchLaunched(launch.PlayerId));
    }

    private static void ApplyLeave(World w, Command.Leave leave)
    {
        // Seat and faction held for rejoin; scaling steps down at the next
        // wave boundary. The sim otherwise doesn't care.
        if (w.Players.TryGetValue(leave.PlayerId, out var player))
            player.Connected = false;
        // Their vehicle seat is not held: the driver's client is what moved it,
        // and a disconnected driver is a buggy nobody can ever get into again.
        LeaveSeat(w, leave.PlayerId, "disconnected");
    }

    private static void ApplyPlayerSync(World w, Command.PlayerSync sync)
    {
        if (w.Players.TryGetValue(sync.PlayerId, out var player))
            player.Pos = sync.Pos;
    }

    // =====================================================================
    // Vehicles
    //
    // The sim arbitrates seats and nothing else. Where a vehicle is comes from
    // whoever is driving it, on the same trust that lets a client say where its
    // own avatar is — but a seat is the one thing two clients can disagree
    // about profitably, so that is decided here.
    // =====================================================================

    private static void ApplyEnterVehicle(World w, Command.EnterVehicle enter)
    {
        void Refuse(string reason) =>
            w.Emit(new SimEvent.VehicleRejected(enter.PlayerId, enter.VehicleId, reason));

        if (!w.Players.TryGetValue(enter.PlayerId, out var player)) return;
        var vehicle = w.Vehicles.FirstOrDefault(v => v.Id == enter.VehicleId);
        if (vehicle is null) { Refuse("unknownVehicle"); return; }
        if (!player.Alive) { Refuse("downed"); return; }
        if (enter.SeatIndex < 0 || enter.SeatIndex >= vehicle.Seats.Length) { Refuse("badSeat"); return; }
        if (w.SeatOf(player.Id) is not null) { Refuse("alreadySeated"); return; }
        if (vehicle.Seats[enter.SeatIndex] != 0) { Refuse("seatTaken"); return; }
        // Reach, measured on the last position the player streamed. Without it
        // a client could seat itself in a vehicle on the far side of the map.
        if (player.Pos.DistanceTo(vehicle.Pos) > Vehicles.All[vehicle.DefId].BoardRadiusMeters)
        {
            Refuse("notNear");
            return;
        }

        vehicle.Seats[enter.SeatIndex] = player.Id;
        w.Emit(new SimEvent.VehicleEntered(player.Id, vehicle.Id, enter.SeatIndex));
    }

    /// <summary>Empty whatever seat this player is in, if any. Called by the
    /// exit command and by every event that takes a player out of the world —
    /// going down, disconnecting, respawning — because a seat held by someone
    /// who is not there locks a vehicle for the rest of the match.</summary>
    private static void LeaveSeat(World w, int playerId, string reason)
    {
        if (w.SeatOf(playerId) is not { } seated) return;
        seated.Vehicle.Seats[seated.Seat] = 0;
        w.Emit(new SimEvent.VehicleExited(playerId, seated.Vehicle.Id, seated.Seat, reason));
    }

    private static void ApplyVehicleSync(World w, Command.VehicleSync sync)
    {
        var vehicle = w.Vehicles.FirstOrDefault(v => v.Id == sync.VehicleId);
        // Only the driver moves it. A passenger's client streams its own avatar
        // and nothing else, and a stranger's sync is dropped the way an unknown
        // PlayerSync is: silently, because it is a race, not an attack.
        if (vehicle is null || vehicle.DriverId != sync.PlayerId) return;
        vehicle.Pos = sync.Pos;
        vehicle.YawDegrees = sync.YawDegrees;
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
            Hp = def.StructureHp,
        };
        w.Towers.Add(tower);
        if (w.Players.TryGetValue(place.PlayerId, out var builder)) builder.TowersBuilt += 1;
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
        // PathLevels counts purchases, so a freshly built tower is at 0 and the
        // level the player sees is one more than that — design's L1, the
        // chassis. Everything below talks in the level the player sees.
        int currentLevel = tower.PathLevels[upgrade.PathIndex] + 1;
        if (currentLevel >= path.MaxLevel)
        {
            w.Emit(new SimEvent.UpgradeRejected(upgrade.PlayerId, upgrade.TowerId, "maxLevel"));
            return;
        }

        int nextLevel = currentLevel + 1;
        int moneyCost = path.LevelCosts[currentLevel - 1];
        if (w.Money < moneyCost)
        {
            w.Emit(new SimEvent.UpgradeRejected(upgrade.PlayerId, upgrade.TowerId, "insufficientFunds"));
            return;
        }

        // 4, 7 and 10 are the breakpoints — the levels design gave a silhouette
        // jump — and they cost scrap from the team pool on top of the money, in
        // rarer types the higher you go. That is the enemy-dependent economy
        // loop: a tenth level is paid for with Gravium, which only the heavies
        // drop, so maxing a path means having fought the things that drop it.
        if (path.RecipeFor(nextLevel) is { } recipe)
        {
            foreach (var (type, amount) in recipe)
            {
                if (w.TeamScrap.GetValueOrDefault(type, 0) < amount)
                {
                    w.Emit(new SimEvent.UpgradeRejected(upgrade.PlayerId, upgrade.TowerId, "insufficientScrap"));
                    return;
                }
            }
            foreach (var (type, amount) in recipe)
                w.TeamScrap[type] -= amount;
        }

        w.Money -= moneyCost;
        tower.Spent += moneyCost;
        tower.PathLevels[upgrade.PathIndex]++;
        w.Emit(new SimEvent.TowerUpgraded(tower.Id, path.Id, tower.PathLevels[upgrade.PathIndex]));
    }

    private static void ApplyPlayerHit(World w, Command.PlayerHit hit)
    {
        if (!w.Players.TryGetValue(hit.PlayerId, out var player) || !player.Alive) return;
        if (!Weapons.All.TryGetValue(hit.WeaponId, out var weapon)) return;
        if (!player.OwnedWeapons.Contains(hit.WeaponId)) return;
        if (player.WeaponCooldown > 0f) return;
        if (player.ReloadTimer > 0f) return;              // hands are busy

        // Empty magazine: start the reload rather than firing. Auto-reloading
        // on the dry click is the forgiving half of this — the punishing half
        // is that it takes real seconds, during which the lane keeps walking.
        int rounds = player.RoundsIn(weapon.Id);
        if (rounds <= 0)
        {
            BeginReload(w, player, weapon.Id);
            return;
        }
        var enemy = w.Enemies.FirstOrDefault(e => e.Id == hit.EnemyId && !e.Dead);
        if (enemy is null || enemy.Burrowed) return;

        var build = player.BuildFor(weapon.Id);

        // Until now the sim never range-checked a hit, which quietly made every
        // range stat in the gunsmith decorative — longBarrel's +25% and the
        // rangefinder optic drew delta bars for a number nothing read. The check
        // is deliberately generous (the client raycasts; this is the server's
        // sanity bound, per the netcode design), but it is a bound.
        if (player.Pos.DistanceTo(enemy.Pos) > EffectiveWeaponRange(w, weapon, build)) return;

        var enemyDef = Enemies.All[enemy.DefId];
        bool armored = enemyDef.FlatArmor > 0f || enemyDef.FrontArmorArcDegrees > 0f;

        float rate = weapon.ShotsPerSecond * build.RateFactor();
        if (player.FactionId == Factions.Tempest.Id) rate *= Balance.TempestRateFactor;
        player.WeaponCooldown = 1f / rate;

        // Spend the round once the shot is known to be real. Charging it at the
        // top cost ammo for hits on enemies that had already died and for shots
        // out of range — neither of which set a cooldown either, so the magazine
        // drained on shots that never happened and the reload rate was roughly
        // double what the numbers said.
        player.Magazine[weapon.Id] = rounds - 1;

        float damage = weapon.Damage * build.DamageFactor(armored);
        // Glacier passive: slowed things take more from this player. Checked on
        // the channel, not the status id, so tar counts and so will anything
        // that lands in the movement slot later.
        if (player.FactionId == Factions.Glacier.Id
            && enemy.Statuses[(int)Channel.Movement].Active)
            damage *= Balance.GlacierChilledDamageFactor;

        var applies = weapon.Applies.Concat(build.ExtraApplies()).ToList();
        Damage(w, enemy, damage,
            $"player{hit.PlayerId}", player.Pos, applies, hit.PlayerId,
            ignoreFlatArmor: build.IgnoresFlatArmor);
    }

    /// <summary>What a build can actually reach, weather included. Shared with
    /// the harness bot so it does not fire shots the sim will silently drop.</summary>
    public static float EffectiveWeaponRange(World w, WeaponDef weapon, WeaponBuild build)
    {
        float range = weapon.RangeMeters * build.RangeFactor();
        var condition = Conditions.ForWave(w.Map, w.WaveIndex);
        if (condition is not null) range *= condition.HeroRangeFactor;
        return range * Balance.HitRangeSlack;
    }

    /// <summary>A swing: everything alive inside the arc takes it at once.
    /// The server picks the targets rather than trusting a client list, which
    /// it can afford to do because melee reach is metres and not sightlines.
    ///
    /// Structures are checked first: swinging at a hurt one repairs it, which
    /// is what makes the wrench a tool as well as a weapon. This was left out
    /// when melee shipped because nothing could damage a structure yet — it
    /// would have been a branch no game state could reach — and lands now that
    /// the Ram exists to break things.</summary>
    private static void ApplyPlayerMelee(World w, Command.PlayerMelee swing)
    {
        if (!w.Players.TryGetValue(swing.PlayerId, out var player) || !player.Alive) return;
        if (player.MeleeCooldown > 0f) return;
        if (!Melee.All.TryGetValue(player.MeleeId, out var def)) return;

        var build = player.MeleeBuildFor(def.Id);
        float reach = def.ReachMeters * build.ReachFactor();
        float rate = def.SwingsPerSecond * build.SpeedFactor();
        if (player.FactionId == Factions.Tempest.Id) rate *= Balance.TempestRateFactor;
        player.MeleeCooldown = 1f / rate;

        // Repair before damage: a swing does one thing, and next to a hurt
        // barricade the thing you meant was the barricade.
        foreach (var structure in w.Towers)
        {
            var sdef = Towers.All[structure.DefId];
            if (sdef.StructureHp <= 0f || structure.Hp >= sdef.StructureHp) continue;
            if (player.Pos.DistanceTo(structure.Pos) > reach) continue;
            structure.Hp = MathF.Min(sdef.StructureHp,
                structure.Hp + def.Damage * Balance.MeleeRepairFactor);
            w.Emit(new SimEvent.StructureRepaired(structure.Id, structure.Hp));
            return;
        }

        var aim = swing.AimPoint - player.Pos;
        if (aim.Length() < 0.01f) aim = new Vec3(0f, 0f, 1f);
        aim = aim.Normalized();

        float damage = def.Damage * build.DamageFactor();
        var applies = def.Applies.Concat(build.ExtraApplies()).ToList();
        float cosArc = def.CosArc;
        bool connected = false;

        // Snapshot: a swing that kills a Cluster must not also hit the children
        // it just spat out. Same rule the splash pass follows.
        foreach (var enemy in w.Enemies.ToList())
        {
            if (enemy.Dead || enemy.Burrowed) continue;
            var to = enemy.Pos - player.Pos;
            float distance = to.Length();
            if (distance > reach || distance < 0.01f) continue;
            if (Vec3.Dot(to.Normalized(), aim) < cosArc) continue;

            connected = true;
            if (def.KnockbackMeters > 0f)
                KnockBack(w, enemy, def.KnockbackMeters / MathF.Max(Enemies.All[enemy.DefId].Mass, 0.25f));
            Damage(w, enemy, damage, $"player{player.Id}", player.Pos, applies,
                player.Id, melee: true);
        }

        w.Emit(new SimEvent.MeleeSwing(player.Id, def.Id, connected));
    }

    private static void ApplyBuyMelee(World w, Command.BuyMelee buy)
    {
        if (!w.Players.TryGetValue(buy.PlayerId, out var player)) return;
        if (!Melee.All.TryGetValue(buy.MeleeId, out var def))
        {
            w.Emit(new SimEvent.PurchaseRejected(buy.PlayerId, buy.MeleeId, "unknownMelee"));
            return;
        }
        if (!player.OwnedMelee.Contains(def.Id))
        {
            // Same rule as a ranged platform: a weapon in your hands is
            // bought with the scrap in your pocket.
            if (!PayScrap(player, def.Recipe))
            {
                w.Emit(new SimEvent.PurchaseRejected(buy.PlayerId, def.Id, "insufficientScrap"));
                return;
            }
            player.OwnedMelee.Add(def.Id);
        }
        player.MeleeId = def.Id;
        w.Emit(new SimEvent.MeleeSelected(buy.PlayerId, def.Id));
    }

    private static void ApplyCraftMeleeAttachment(World w, Command.CraftMeleeAttachment craft)
    {
        if (!w.Players.TryGetValue(craft.PlayerId, out var player)) return;
        if (!Melee.Attachments.TryGetValue(craft.AttachmentId, out var att))
        {
            w.Emit(new SimEvent.CraftRejected(craft.PlayerId, craft.AttachmentId, "unknownAttachment"));
            return;
        }
        if (!player.OwnedMelee.Contains(craft.MeleeId))
        {
            w.Emit(new SimEvent.CraftRejected(craft.PlayerId, craft.AttachmentId, "meleeNotOwned"));
            return;
        }
        if (!PayScrap(player, att.Recipe))
        {
            w.Emit(new SimEvent.CraftRejected(craft.PlayerId, craft.AttachmentId, "insufficientScrap"));
            return;
        }
        player.MeleeBuildFor(craft.MeleeId).Attachments[att.Slot] = att.Id;
        w.Emit(new SimEvent.AttachmentCrafted(craft.PlayerId, craft.MeleeId, att.Id));
    }

    private static void ApplyUpgradeMelee(World w, Command.UpgradeMelee up)
    {
        if (!w.Players.TryGetValue(up.PlayerId, out var player)) return;
        if (!Melee.All.TryGetValue(up.MeleeId, out var def)) return;
        if (!player.OwnedMelee.Contains(def.Id)) return;

        var build = player.MeleeBuildFor(def.Id);
        // Campaign caps at 5; 6-10 open with endless, the same gating the tower
        // grid uses, so one rule governs both ladders.
        if (build.MasteryLevel >= Melee.CampaignMasteryCap)
        {
            w.Emit(new SimEvent.CraftRejected(up.PlayerId, def.Id, "masteryCapped"));
            return;
        }
        int cost = def.MasteryCosts[build.MasteryLevel];
        if (w.Money < cost)
        {
            w.Emit(new SimEvent.CraftRejected(up.PlayerId, def.Id, "insufficientFunds"));
            return;
        }
        w.Money -= cost;
        build.MasteryLevel++;
        w.Emit(new SimEvent.MeleeMastery(up.PlayerId, def.Id, build.MasteryLevel));
    }

    /// <summary>Starts a reload if one is worth starting.</summary>
    private static void BeginReload(World w, PlayerState player, string weaponId)
    {
        if (player.ReloadTimer > 0f) return;
        var def = Weapons.All[weaponId];
        if (player.RoundsIn(weaponId) >= def.MagazineSize) return;   // already full
        player.ReloadTimer = def.ReloadSeconds;
        player.ReloadingWeapon = weaponId;
        w.Emit(new SimEvent.ReloadStarted(player.Id, weaponId, def.ReloadSeconds));
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
        // Personal scrap, not the shared wallet: see WeaponDef.Recipe.
        if (!PayScrap(player, weapon.Recipe))
        {
            w.Emit(new SimEvent.PurchaseRejected(buy.PlayerId, buy.WeaponId, "insufficientScrap"));
            return;
        }

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

    /// <summary>Pack a Punch. No level cap: the cost curve is what limits it,
    /// growing faster than the power does so each level buys less than the one
    /// before. Paid in Alloy, from the player's own scrap — this is a bench
    /// upgrade, not a team one.</summary>
    private static void ApplyPackAPunch(World w, Command.PackAPunch pack)
    {
        if (!w.Players.TryGetValue(pack.PlayerId, out var player)) return;
        if (!player.OwnedWeapons.Contains(pack.WeaponId))
        {
            w.Emit(new SimEvent.CraftRejected(pack.PlayerId, pack.WeaponId, "weaponNotOwned"));
            return;
        }
        if (!Weapons.All.ContainsKey(pack.WeaponId))
        {
            w.Emit(new SimEvent.CraftRejected(pack.PlayerId, pack.WeaponId, "unknownWeapon"));
            return;
        }

        var build = player.BuildFor(pack.WeaponId);
        int cost = build.NextPackCost;
        // Every fifth level wants Gravium as well, and it is charged all or
        // nothing with the Alloy — a player short on either pays neither.
        int gravium = build.NextPackGravium;
        if (player.Scrap.GetValueOrDefault(ScrapType.Alloy) < cost
            || player.Scrap.GetValueOrDefault(ScrapType.Gravium) < gravium)
        {
            w.Emit(new SimEvent.CraftRejected(pack.PlayerId, pack.WeaponId, "insufficientScrap"));
            return;
        }

        player.Scrap[ScrapType.Alloy] = player.Scrap.GetValueOrDefault(ScrapType.Alloy) - cost;
        if (gravium > 0)
            player.Scrap[ScrapType.Gravium] = player.Scrap.GetValueOrDefault(ScrapType.Gravium) - gravium;
        build.PackLevel++;
        w.Emit(new SimEvent.PackedAPunch(pack.PlayerId, pack.WeaponId, build.PackLevel, cost));
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

        int level = player.FactionLevel;
        player.AbilityCooldown = faction.CooldownSeconds * Factions.CooldownFactor(level);
        float radius = faction.RadiusMeters * Factions.RadiusFactor(level);
        float magnitude = faction.Magnitude * Factions.MagnitudeFactor(level);
        w.Emit(new SimEvent.AbilityUsed(ability.PlayerId, faction.AbilityId));

        switch (faction.AbilityId)
        {
            case "overdrive":
                foreach (var tower in w.Towers)
                {
                    if (tower.Pos.DistanceTo(player.Pos) <= radius)
                    {
                        tower.BuffTimer = faction.DurationSeconds;
                        tower.BuffFactor = magnitude;
                    }
                }
                break;

            case "ignitionWave":
                foreach (var enemy in w.Enemies)
                {
                    if (!enemy.Dead && !enemy.Burrowed && enemy.Pos.DistanceTo(ability.TargetPos) <= radius)
                        ApplyStatus(w, enemy, Statuses.Burn.Id, $"player{player.Id}", player.Id);
                }
                break;

            case "cryoField":
                foreach (var enemy in w.Enemies)
                {
                    if (!enemy.Dead && !enemy.Burrowed && enemy.Pos.DistanceTo(ability.TargetPos) <= radius)
                        ApplyStatus(w, enemy, Statuses.Chill.Id, $"player{player.Id}", player.Id);
                }
                break;

            case "revealPulse":
                // Map-wide on purpose: radius is 0 on the def because there is
                // no radius, and reading it as "reveals nothing" would be the
                // easy bug. Burrowed enemies are excluded — being underground
                // is not stealth, and the Mole's window is a timing question
                // that reveal has no business answering.
                foreach (var enemy in w.Enemies)
                {
                    if (!enemy.Dead && !enemy.Burrowed)
                        ApplyStatus(w, enemy, Statuses.Reveal.Id, $"player{player.Id}", player.Id);
                }
                break;

            case "chainSurge":
                foreach (var enemy in w.Enemies)
                {
                    if (enemy.Dead || enemy.Burrowed) continue;
                    if (enemy.Pos.DistanceTo(ability.TargetPos) > radius) continue;
                    Damage(w, enemy, magnitude, $"player{player.Id}", player.Pos,
                        new[] { Statuses.Shock.Id }, player.Id);
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
            reviver.MatchXp += 5;
            reviver.Revives += 1;
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
            if (w.Lobby) return;
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
            // Siege enemies are not turned away by a barricade — they walk at
            // it. Without this exemption a Ram can never reach the one thing it
            // exists to break: the barricade reroutes it, so it takes the long
            // way round and arrives as an expensive walker. The block is what
            // makes it choose the shortcut, not what stops it.
            if (routeDef.BarricadeGate is { } gate
                && def.StructureDps <= 0f
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
                ItineraryIndex = routeIndex,
                LateralOffset = entry.LateralOffset,
                Pos = route.Waypoints[0],
                Facing = (route.Waypoints[1] - route.Waypoints[0]).Normalized(),
                Bounty = ScaledBounty(def.Bounty, w.WaveIndex),
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
                    // credit survive the burn. Poison additionally bypasses
                    // armor and shields, which is its entire reason to exist.
                    Damage(w, enemy, def.DamagePerSecond * Balance.Dt, slot.Source, enemy.Pos, null,
                        SourcePlayerId(slot.Source),
                        ignoreFlatArmor: def.IgnoresArmor, ignoreShield: def.IgnoresShield);
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
            if (playerId is int reactor && w.Players.TryGetValue(reactor, out var reactorState))
                reactorState.MatchXp += 2;   // co-op combos pay
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

    /// <summary>Menders restore nearby allies. Runs after statuses so a poison
    /// tick and a heal tick resolve in the same order every time — the heal is
    /// meant to lose to poison, and determinism requires the phase order be
    /// fixed rather than incidental.</summary>
    private static void ApplyHealAuras(World w)
    {
        foreach (var healer in w.Enemies)
        {
            if (healer.Dead) continue;
            var healerDef = Enemies.All[healer.DefId];
            if (healerDef.HealPerSecond <= 0f) continue;

            float amount = healerDef.HealPerSecond * Balance.Dt;
            foreach (var ally in w.Enemies)
            {
                if (ally.Dead || ally.Id == healer.Id) continue;
                if (ally.Hp >= ally.MaxHp) continue;
                if (healer.Pos.DistanceTo(ally.Pos) > healerDef.HealRadius) continue;
                ally.Hp = MathF.Min(ally.MaxHp, ally.Hp + amount);
            }
        }
    }

    /// <summary>Which leg of the underlying route this enemy is on.
    ///
    /// Only the wire needs this: <see cref="SimEvent.EnemyTeleported"/> carries
    /// a leg index, it is in the hashed event log, and the client snaps a view
    /// on it. Route legs map one-to-one onto the itinerary's segments in order,
    /// so this is a translation rather than a second source of truth.</summary>
    private static int LegOf(World w, Enemy enemy) => enemy.RouteLeg(w);

    private static void MoveEnemies(World w)
    {
        foreach (var enemy in w.Enemies)
        {
            if (enemy.Dead) continue;

            var def = Enemies.All[enemy.DefId];
            float speed = def.SpeedMetersPerSec;

            // A Shade nobody has revealed moves faster — ignoring it costs you,
            // which is what stops stealth from being a pure tempo loss.
            if (def.Stealth && !enemy.Statuses[(int)Channel.Detection].Active)
                speed *= def.StealthSpeedBonus;

            ref var movement = ref enemy.Statuses[(int)Channel.Movement];
            if (movement.Active)
                speed *= Statuses.All[movement.StatusId!].SpeedFactor;

            // Enrage: a Ram that is losing hurries. Read from the def so the
            // dial is content, and applied before control so a frozen Ram is
            // still frozen — panic does not beat being frozen solid.
            if (def.EnrageBelowHpFraction > 0f && enemy.MaxHp > 0f
                && enemy.Hp / enemy.MaxHp <= def.EnrageBelowHpFraction)
                speed *= def.EnrageSpeedFactor;

            // Busy demolishing something: not advancing.
            if (enemy.Sieging) speed = 0f;

            // Hard control: dead stop.
            ref var control = ref enemy.Statuses[(int)Channel.Control];
            if (control.Active && Statuses.All[control.StatusId!].HardControl)
                speed = 0f;

            // Burrow cycle (distance-based, deterministic): underground for the
            // first stretch of every cycle, surfaced for the rest.
            if (def.Burrower)
                enemy.Burrowed = enemy.TotalTraveled % Balance.BurrowCycleMeters < Balance.BurrowedMeters;

            var itinerary = w.Graph.Itineraries[enemy.ItineraryIndex];
            var edgeSteps = w.ItineraryEdges[enemy.ItineraryIndex];
            float remaining = speed * Balance.Dt;
            int legCounter = LegOf(w, enemy);

            while (enemy.EdgeStep < edgeSteps.Length)
            {
                var edge = w.Graph.Edges[edgeSteps[enemy.EdgeStep]];
                var lengths = w.EdgeSegmentLengths[edgeSteps[enemy.EdgeStep]];

                // A warp is crossed the instant it is reached, whatever the
                // speed — a frozen, sieging or knocked-back enemy standing on a
                // departure pad still goes. That is not generosity: a
                // zero-length span is the one position this model cannot
                // represent (the lerp below would divide by it), so an enemy
                // must never be parked on one between ticks. Under the graph
                // that stops being a rule three gates enforce and becomes
                // structural: a warp is a whole edge, and an edge boundary is a
                // node, which is not a place anyone stands.
                if (edge.Kind == LaneEdgeKind.Warp)
                {
                    var pad = edge.Waypoints[^1];
                    w.Emit(new SimEvent.EnemyTeleported(
                        enemy.Id, itinerary.Id, legCounter, pad.X, pad.Y, pad.Z));
                    enemy.EdgeStep++;
                    enemy.Segment = 0;
                    enemy.SegmentProgress = 0f;
                    legCounter++;
                    continue;
                }
                if (remaining <= 0f) break;

                float segmentLeft = lengths[enemy.Segment] - enemy.SegmentProgress;
                if (remaining < segmentLeft)
                {
                    enemy.SegmentProgress += remaining;
                    enemy.TotalTraveled += remaining;
                    remaining = 0f;
                }
                else
                {
                    enemy.TotalTraveled += segmentLeft;
                    remaining -= segmentLeft;
                    enemy.Segment++;
                    enemy.SegmentProgress = 0f;
                    legCounter++;
                    if (enemy.Segment >= lengths.Length)
                    {
                        enemy.EdgeStep++;
                        enemy.Segment = 0;
                    }
                }
            }

            if (enemy.EdgeStep >= edgeSteps.Length)
            {
                enemy.Dead = true;
                w.Lives -= enemy.LeakDamage;
                w.Emit(new SimEvent.EnemyLeaked(enemy.Id, enemy.DefId, enemy.LeakDamage));
                continue;
            }

            var current = w.Graph.Edges[edgeSteps[enemy.EdgeStep]];
            var segments = w.EdgeSegmentLengths[edgeSteps[enemy.EdgeStep]];
            var a = current.Waypoints[enemy.Segment];
            var b = current.Waypoints[enemy.Segment + 1];
            enemy.Facing = (b - a).Normalized();
            var spine = Vec3.Lerp(a, b, enemy.SegmentProgress / segments[enemy.Segment]);

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
            player.MeleeCooldown = MathF.Max(0f, player.MeleeCooldown - Balance.Dt);

            // Reload ticks down wherever the player is; finishing it refills the
            // magazine from the unlimited reserve.
            if (player.ReloadTimer > 0f)
            {
                player.ReloadTimer = MathF.Max(0f, player.ReloadTimer - Balance.Dt);
                if (player.ReloadTimer <= 0f && player.ReloadingWeapon is { Length: > 0 } done)
                {
                    player.Magazine[done] = Weapons.All[done].MagazineSize;
                    player.ReloadingWeapon = "";
                    w.Emit(new SimEvent.Reloaded(player.Id, done));
                }
            }
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
                    // Whatever they were driving stops being theirs. A body
                    // bleeding out in the driver's seat is a vehicle nobody can
                    // use and a teammate who cannot reach them.
                    LeaveSeat(w, player.Id, "downed");
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
        // They are at the spawn now, so they are not in a seat wherever the
        // vehicle happens to be. Normally the down already emptied it; this
        // catches the paths that reach a respawn without one.
        LeaveSeat(w, player.Id, "respawned");
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

            if (def.Kind == TowerKind.Beam)
            {
                var beamTarget = PickTarget(w, tower, def);
                if (beamTarget is null)
                {
                    // Lost the target: the charge bleeds off rather than
                    // persisting, or a beam would be free burst on the next one.
                    tower.RampTargetId = -1;
                    tower.RampSeconds = 0f;
                    continue;
                }

                if (beamTarget.Id != tower.RampTargetId)
                {
                    tower.RampTargetId = beamTarget.Id;
                    tower.RampSeconds = 0f;      // switching costs the ramp
                }
                tower.RampSeconds += Balance.Dt;

                float rampRate = Balance.BeamRampPerSecond * PathFactor(tower, def, "ramp");
                float rampCap = Balance.BeamRampCap * PathFactor(tower, def, "peak");
                float ramp = MathF.Min(rampCap, 1f + rampRate * tower.RampSeconds);

                w.Emit(new SimEvent.TowerFired(tower.Id, beamTarget.Id));
                Damage(w, beamTarget, EffectiveDamage(tower, def) * ramp * Balance.Dt,
                    $"tower{tower.Id}", tower.Pos, def.Applies, null);
                continue;
            }

            if (def.Kind == TowerKind.Aura)
            {
                float auraRange = EffectiveRange(w, tower, def);
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
            if (target is null) { tower.LastTargetId = -1; continue; }

            // Night: the beat before a tower settles on something new. Charged
            // to the cooldown rather than a separate timer so it cannot stack
            // with itself, and so a resumed save carries it in one field.
            if (target.Id != tower.LastTargetId)
            {
                float delay = AcquisitionDelay(w, target);
                tower.LastTargetId = target.Id;
                if (delay > 0f) { tower.Cooldown = delay; continue; }
            }

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
        float range = EffectiveRange(w, tower, def);
        Enemy? best = null;
        float bestTraveled = -1f;

        foreach (var enemy in w.Enemies)
        {
            if (enemy.Dead) continue;
            if (!def.TargetLayers.Contains(Enemies.All[enemy.DefId].Layer)) continue;

            if (enemy.Burrowed) continue;   // untargetable underground

            // Stealth: towers need it revealed. Heroes are unaffected — they
            // shoot what they can see, which is the Shade's whole trade.
            var targetDef = Enemies.All[enemy.DefId];
            if (targetDef.Stealth && !enemy.Statuses[(int)Channel.Detection].Active) continue;

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

    /// <summary>Range, whatever the tower calls the path that grows it, and
    /// whatever the weather is doing to it. Deferred to <see
    /// cref="TowerMath"/> because the client draws a ring from the same
    /// numbers, and a ring that disagrees with what the tower shoots is worse
    /// than no ring.</summary>
    private static float EffectiveRange(World w, Tower tower, TowerDef def) =>
        TowerMath.Range(def, tower.PathLevels, w.Map, w.WaveIndex);

    /// <summary>Night's hesitation: a tower takes a beat to find something
    /// nobody has marked. Marked targets are acquired instantly, which is the
    /// whole reason mark is worth carrying into a night wave.</summary>
    private static float AcquisitionDelay(World w, Enemy target)
    {
        var condition = Conditions.ForWave(w.Map, w.WaveIndex);
        if (condition is null || condition.AcquisitionDelaySeconds <= 0f) return 0f;
        if (condition.AcquisitionDelayExemptsMarked
            && target.Statuses[(int)Channel.Vulnerability].Active) return 0f;
        return condition.AcquisitionDelaySeconds;
    }

    private static float EffectiveRate(Tower tower, TowerDef def) =>
        def.ShotsPerSecond * PathFactor(tower, def, "rate") * tower.BuffFactor;

    private static float PathFactor(Tower tower, TowerDef def, string pathId)
    {
        for (int i = 0; i < def.UpgradePaths.Count; i++)
        {
            if (def.UpgradePaths[i].Id == pathId)
                return DetMath.PowInt(def.UpgradePaths[i].PerLevelFactor, tower.PathLevels[i]);
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

    /// <summary>Rams stop and hit whatever structure is in reach. Runs after
    /// movement so one that just closed the distance swings the same tick, and
    /// before FireTowers so a structure destroyed this tick does not also get
    /// a shot off — a tower that is rubble should not be firing.</summary>
    private static void SiegeStructures(World w)
    {
        var destroyed = new List<Tower>();

        foreach (var enemy in w.Enemies)
        {
            if (enemy.Dead) continue;
            var def = Enemies.All[enemy.DefId];
            if (def.StructureDps <= 0f) continue;

            Tower? target = null;
            float nearest = float.MaxValue;
            foreach (var tower in w.Towers)
            {
                if (Towers.All[tower.DefId].StructureHp <= 0f) continue;   // indestructible
                float d = enemy.Pos.DistanceTo(tower.Pos);
                if (d <= def.StructureReach && d < nearest) { nearest = d; target = tower; }
            }
            if (target is null) { enemy.Sieging = false; continue; }

            // Stopping to swing is the trade: a Ram working on a barricade is a
            // Ram not advancing, so ignoring it costs structures and answering
            // it costs tempo. MoveEnemies reads this flag.
            enemy.Sieging = true;
            target.Hp -= def.StructureDps * Balance.Dt;
            w.Emit(new SimEvent.StructureDamaged(target.Id, enemy.Id, MathF.Max(0f, target.Hp)));
            if (target.Hp <= 0f && !destroyed.Contains(target)) destroyed.Add(target);
        }

        foreach (var tower in destroyed)
        {
            w.Towers.Remove(tower);
            w.Emit(new SimEvent.StructureDestroyed(tower.Id, tower.DefId, tower.SocketId));
        }
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

    /// <summary>Knockback is an instantaneous displacement along the lane, not a
    /// status: walk the enemy backward through the segments it came down, and
    /// across an edge boundary if it runs out of them.</summary>
    private static void KnockBack(World w, Enemy enemy, float meters)
    {
        var edgeSteps = w.ItineraryEdges[enemy.ItineraryIndex];
        float remaining = meters;
        while (remaining > 0f)
        {
            if (enemy.SegmentProgress >= remaining)
            {
                enemy.SegmentProgress -= remaining;
                enemy.TotalTraveled -= remaining;
                break;
            }
            remaining -= enemy.SegmentProgress;
            enemy.TotalTraveled -= enemy.SegmentProgress;

            if (enemy.Segment == 0)
            {
                // Off the front of this edge. The edge before it is where we
                // came from — unless it is a warp, and nothing pushes an enemy
                // back through one of those. The arrival pad is as far back as
                // a launcher gets it; the alternative is an enemy parked on a
                // zero-length span, or reappearing on the far side of the map
                // because somebody stood on a trap.
                if (enemy.EdgeStep == 0) { enemy.SegmentProgress = 0f; break; }
                var previous = w.Graph.Edges[edgeSteps[enemy.EdgeStep - 1]];
                if (previous.Kind == LaneEdgeKind.Warp) { enemy.SegmentProgress = 0f; break; }
                enemy.EdgeStep--;
                enemy.Segment = w.EdgeSegmentLengths[edgeSteps[enemy.EdgeStep]].Length;
            }

            enemy.Segment--;
            enemy.SegmentProgress = w.EdgeSegmentLengths[edgeSteps[enemy.EdgeStep]][enemy.Segment];
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
            // Leaked, not killed: a Cluster that reached the core does not get
            // to spit children at it.
            if (enemy.EdgeStep >= w.ItineraryEdges[enemy.ItineraryIndex].Length) continue;

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
                    ItineraryIndex = enemy.ItineraryIndex,
                    EdgeStep = enemy.EdgeStep,
                    Segment = enemy.Segment,
                    SegmentProgress = enemy.SegmentProgress,
                    TotalTraveled = enemy.TotalTraveled,
                    LateralOffset = (rng.NextFloat() - 0.5f) * MathF.Max(childDef.ScatterWidth, 2f),
                    Pos = enemy.Pos,
                    Facing = enemy.Facing,
                    // A split pays on the wave its parent came from, not on
                    // whatever wave happens to be running when it dies.
                    Bounty = ScaledBounty(childDef.Bounty, enemy.WaveIndex),
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
        w.Pickups.RemoveAll(p => p.Dead);
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

            // Endless has no last wave; the arc rolls over and the core is
            // the only thing that can end the run.
            if (!w.Endless && w.WaveIndex + 1 >= w.Map.TotalWaves)
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
        IReadOnlyList<string>? applies, int? playerId, bool ignoreFlatArmor = false,
        bool ignoreShield = false, bool melee = false)
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

            // Compared as cosines rather than as degrees — same test, no Acos.
            // The inequalities flip because cosine decreases as the angle grows.
            if (dot >= def.CosFrontArmorHalfArc)
                amount *= def.FrontArmorFactor;
            else if (dot <= EnemyDef.CosRearThreshold)
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
        if (enemy.Shield > 0f && amount > 0f && !ignoreShield)
        {
            enemy.ShieldTimer = Balance.ShieldRegenDelaySeconds;
            float soaked = MathF.Min(enemy.Shield, amount);
            enemy.Shield -= soaked;
            amount -= soaked;
        }

        enemy.Hp -= amount;
        w.Emit(new SimEvent.EnemyDamaged(enemy.Id, total, source));

        if (playerId is int dealer && w.Players.TryGetValue(dealer, out var dealerState))
            dealerState.DamageDealt += total;

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
            DropScrap(w, enemy, def, playerId, melee);

            // Faction XP: kills bank into the profile at match end.
            if (playerId is int killer && w.Players.TryGetValue(killer, out var killerState))
            {
                killerState.MatchXp += 1;
                killerState.Kills += 1;
            }
        }
    }

    /// <summary>Scrap split: a team share funds tower upgrades, a personal
    /// share funds a gunsmith. The split is the same whatever did the killing;
    /// what changes is where the personal half goes.
    ///
    /// <b>A player's kill is collected on the spot.</b> They shot it, the scrap
    /// is theirs, and making them walk to it only ever took it away — a Skiff
    /// dies over the air lane and its drop lands somewhere nobody can reach,
    /// which is a tax on using the weapon the map asks you to use.
    ///
    /// <b>A tower's kill hits the floor.</b> It used to bank the whole yield
    /// silently, so the majority of kills in a match produced income nobody
    /// ever saw. Now the personal half is a thing on the ground that anyone can
    /// walk over, and the choice of whether to go and get it is the player's:
    /// take it for your bench, or leave it and let it bank to the team when it
    /// times out. Nothing is lost either way, which is what keeps a defence
    /// from being something you can lose by being busy.</summary>
    private static void DropScrap(World w, Enemy enemy, EnemyDef def, int? killerPlayerId,
        bool meleeKill = false)
    {
        if (def.ScrapYield.Count == 0) return;

        var parts = new List<string>();
        // Scaled by the wave the enemy belongs to, not the wave running now: a
        // straggler killed after the next wave starts is still worth what its
        // own wave was worth.
        float waveScale = WavePlan.ScrapScale(enemy.WaveIndex);
        foreach (var (type, defAmount) in def.ScrapYield)
        {
            int baseAmount = Math.Max(1, (int)MathF.Round(defAmount * waveScale,
                MidpointRounding.AwayFromZero));
            // Melee's entire economic identity: standing in contact range pays
            // better. Applied to the whole yield, team share included, so a
            // melee player funds the team's towers as well as their own bench.
            int amount = meleeKill
                ? (int)MathF.Round(baseAmount * Balance.MeleeScrapBonus, MidpointRounding.AwayFromZero)
                : baseAmount;
            // One split, every kill. A tower's kill used to hand the team the
            // whole yield; it now keeps the same half everything else does.
            int teamShare = (int)MathF.Round(amount * Balance.ScrapTeamShare,
                MidpointRounding.AwayFromZero);
            int personal = amount - teamShare;

            // The team's half banks immediately: it pays for towers, which are
            // everyone's, and a defence that depended on someone walking to it
            // would be a defence you can lose by being busy.
            if (teamShare > 0)
                w.TeamScrap[type] = w.TeamScrap.GetValueOrDefault(type, 0) + teamShare;

            // Shot it yourself: it is already yours. PickupId 0 says this
            // scrap never touched the floor — there is no pickup to correlate
            // it with, and the client only ever wanted the "+2 alloy" out of
            // this event.
            if (personal > 0 && killerPlayerId is int killer
                && w.Players.TryGetValue(killer, out var killerScrap))
            {
                killerScrap.Scrap[type] = killerScrap.Scrap.GetValueOrDefault(type, 0) + personal;
                w.Emit(new SimEvent.ScrapCollected(0, killer, type.ToString(), personal));
                parts.Add($"{type}:{amount}");
                continue;
            }

            // Otherwise it hits the floor for whoever wants to walk over it.
            if (personal > 0)
            {
                // Spread by scrap type so a Monolith's three drops are three
                // things rather than one pile. Deterministic: the offset comes
                // from the type, never from an RNG stream.
                var offset = ScrapDropOffsets[(int)type];
                var pickup = new ScrapPickup
                {
                    Id = w.NextId(),
                    Type = type,
                    Amount = personal,
                    Pos = enemy.Pos + offset,
                    Life = Balance.ScrapPickupSeconds,
                    GroundY = GroundHeightUnder(w, enemy.Pos),
                };
                w.Pickups.Add(pickup);
                w.Emit(new SimEvent.ScrapSpawned(pickup.Id, type.ToString(), personal,
                    pickup.Pos.X, pickup.Pos.Y, pickup.Pos.Z));
            }

            parts.Add($"{type}:{amount}");
        }

        w.Emit(new SimEvent.ScrapDropped(enemy.Id, string.Join(",", parts)));
    }

    /// <summary>The height of the walkable surface under a point: the Y of the
    /// nearest ground-route waypoint at or below it. Ground routes are where
    /// enemies walk, so they are where a player can stand — which is the only
    /// definition of "floor" a sim with no physics has, and it is the right one
    /// on a map like the Spire where the floor is at forty different heights.
    /// Never above the drop: scrap falls, it does not climb.</summary>
    private static float GroundHeightUnder(World w, Vec3 pos)
    {
        float best = float.MaxValue, bestY = 0f;
        bool found = false;
        float lowest = float.MaxValue;

        foreach (var route in w.Map.Routes)
        {
            if (route.Layer != EnemyLayer.Ground) continue;
            foreach (var point in route.Waypoints)
            {
                lowest = MathF.Min(lowest, point.Y);
                if (point.Y > pos.Y + 0.5f) continue;
                float dx = point.X - pos.X, dz = point.Z - pos.Z;
                float distance = dx * dx + dz * dz;
                if (distance >= best) continue;
                best = distance;
                bestY = point.Y;
                found = true;
            }
        }
        return found ? bestY : (lowest < float.MaxValue ? lowest : 0f);
    }

    /// <summary>Scrap on the floor drifts to a nearby player and is collected
    /// on contact; what nobody reaches banks to the team pool when it expires.
    /// Players are walked in seat order and pickups in spawn order, so two
    /// players equidistant from the same drop always resolve the same way.</summary>
    private static void UpdatePickups(World w)
    {
        if (w.Pickups.Count == 0) return;

        foreach (var pickup in w.Pickups)
        {
            if (pickup.Dead) continue;

            // Fall first. A drop from the air lane is collectable on the way
            // down if a player happens to be under it, which is a nice thing
            // to have happen rather than a rule worth preventing.
            if (pickup.Pos.Y > pickup.GroundY)
            {
                float fallen = Balance.ScrapFallSpeed * Balance.Dt;
                float y = MathF.Max(pickup.GroundY, pickup.Pos.Y - fallen);
                pickup.Pos = new Vec3(pickup.Pos.X, y, pickup.Pos.Z);
            }

            PlayerState? nearest = null;
            float best = float.MaxValue;
            foreach (var player in w.Players.Values)
            {
                if (!player.Connected || !player.Alive) continue;
                float distance = player.Pos.DistanceTo(pickup.Pos);
                if (distance >= best) continue;
                best = distance;
                nearest = player;
            }

            if (nearest is not null && best <= Balance.ScrapCollectMeters)
            {
                nearest.Scrap[pickup.Type] = nearest.Scrap.GetValueOrDefault(pickup.Type, 0) + pickup.Amount;
                pickup.Dead = true;
                w.Emit(new SimEvent.ScrapCollected(pickup.Id, nearest.Id, pickup.Type.ToString(), pickup.Amount));
                continue;
            }

            if (nearest is not null && best <= Balance.ScrapMagnetMeters && best > 0.001f)
            {
                var toward = (nearest.Pos - pickup.Pos) * (1f / best);
                pickup.Pos += toward * (Balance.ScrapMagnetSpeed * Balance.Dt);
            }

            pickup.Life -= Balance.Dt;
            if (pickup.Life <= 0f)
            {
                w.TeamScrap[pickup.Type] = w.TeamScrap.GetValueOrDefault(pickup.Type, 0) + pickup.Amount;
                pickup.Dead = true;
                w.Emit(new SimEvent.ScrapExpired(pickup.Id, pickup.Type.ToString(), pickup.Amount));
            }
        }
    }

    /// <summary>A def's bounty as this wave pays it. Rounded away from zero so
    /// the cheapest enemy on the roster never rounds down to nothing.</summary>
    private static int ScaledBounty(int baseBounty, int waveIndex) =>
        Math.Max(1, (int)MathF.Round(baseBounty * WavePlan.BountyScale(waveIndex),
            MidpointRounding.AwayFromZero));

    private static int? SourcePlayerId(string source) =>
        source.StartsWith("player") && int.TryParse(source.AsSpan(6), out int id) ? id : null;
}
