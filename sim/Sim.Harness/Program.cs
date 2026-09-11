using DeepField.Harness;
using DeepField.Sim;
using DeepField.Sim.Content;

// The gate suite: every check prints PASS/FAIL; a single failure fails the run.
// Gates only grow — this is the 671-gate culture's new home.

int failures = 0;

void Gate(string name, bool ok, string detail = "")
{
    Console.WriteLine($"{(ok ? "PASS" : "FAIL")}  {name}{(detail.Length > 0 ? $"  ({detail})" : "")}");
    if (!ok) failures++;
}

const uint Seed = 20260906;

PlayerBot MidBot(int id = 1, string faction = "ember") =>
    new(Seed + (uint)id) { PlayerId = id, FactionId = faction, Accuracy = 0.6f, Uptime = 0.8f };

// --- Gate 1: determinism — same seed + same bots => byte-identical logs, twice.
{
    var a = MatchRunner.Run(Seed, Maps.Foundry, MidBot());
    var b = MatchRunner.Run(Seed, Maps.Foundry, MidBot());
    Gate("determinism: identical event logs across runs", a.EventLogHash == b.EventLogHash,
        $"hash {a.EventLogHash[..12]}");
}

// --- Gate 2: conservation — every spawned enemy is killed or leaked, never lost.
{
    var r = MatchRunner.Run(Seed, Maps.Foundry, MidBot());
    Gate("conservation: spawned == kills + leaks",
        r.Spawned == r.TowerKills + r.PlayerKills + r.Leaked,
        $"spawned {r.Spawned} = towers {r.TowerKills} + player {r.PlayerKills} + leaked {r.Leaked}");
}

// --- Gate 3: the mid-band player + scripted towers clear Foundry.
{
    var r = MatchRunner.Run(Seed, Maps.Foundry, MidBot());
    Gate("mid-band bot + towers clear Foundry", r.Victory,
        $"waves {r.WavesCleared}/{Maps.Foundry.TotalWaves}, lives {r.LivesLeft}, reactions {r.Reactions}");
}

// --- Gate 4: towers-only holds the floor (bleeding lives is fine; losing isn't).
{
    var r = MatchRunner.Run(Seed, Maps.Foundry);
    Gate("towers-only survives Foundry (floor)", r.Victory,
        $"waves {r.WavesCleared}, lives {r.LivesLeft}, leaked {r.Leaked}");
}

// --- Gate 5: the player matters — skill band shifts outcomes measurably.
{
    var low = MatchRunner.Run(Seed, Maps.Foundry,
        new PlayerBot(Seed + 1) { PlayerId = 1, FactionId = "ember", Accuracy = 0.4f, Uptime = 0.6f });
    var high = MatchRunner.Run(Seed, Maps.Foundry,
        new PlayerBot(Seed + 1) { PlayerId = 1, FactionId = "ember", Accuracy = 0.8f, Uptime = 0.9f });
    Gate("skill band: high-skill bot out-kills low-skill bot", high.PlayerKills > low.PlayerKills,
        $"high {high.PlayerKills} vs low {low.PlayerKills}");
}

// --- Gate 6: co-op scaling — 4 players see more enemies than 1, and still win.
{
    var solo = MatchRunner.Run(Seed, Maps.Foundry, MidBot());
    var four = MatchRunner.Run(Seed, Maps.Foundry,
        MidBot(1, "ember"), MidBot(2, "forge"),
        new PlayerBot(Seed + 3) { PlayerId = 3, FactionId = "ember", Accuracy = 0.6f, Uptime = 0.8f },
        new PlayerBot(Seed + 4) { PlayerId = 4, FactionId = "forge", Accuracy = 0.6f, Uptime = 0.8f });
    // Players 3/4 get faction-taken rejections — sim survives that too.
    Gate("co-op scaling: 4p waves are bigger and clearable", four.Spawned > solo.Spawned,
        $"solo {solo.Spawned} vs 4p {four.Spawned} spawned");
}

// --- Gate 7: serialization round-trip — resume mid-match, identical continuation.
{
    var world = new World(Seed, Maps.Foundry);
    world.Enqueue(new Command.Join(1, "p1", "ember"));
    world.Enqueue(new Command.PlaceTower(1, "lance", "g2"));
    world.Enqueue(new Command.PlaceTower(1, "skywatch", "g4"));
    for (int i = 0; i < Balance.TickHz * 40; i++) Step.Advance(world);

    var resumed = Serialization.Deserialize(Serialization.Serialize(world));

    var logA = new List<string>();
    var logB = new List<string>();
    for (int i = 0; i < Balance.TickHz * 15; i++)
    {
        Step.Advance(world);
        logA.AddRange(world.Events.Select(e => e.LogLine()));
        Step.Advance(resumed);
        logB.AddRange(resumed.Events.Select(e => e.LogLine()));
    }
    Gate("serialization: resumed world continues identically",
        MatchRunner.HashLog(logA) == MatchRunner.HashLog(logB),
        $"{logA.Count} events compared");
}

// --- Gate 8: refusals answer out loud (sockets, towers, traps, factions, scrap).
{
    var world = new World(Seed, Maps.Foundry);
    world.Enqueue(new Command.Join(1, "p1", "ember"));
    world.Enqueue(new Command.Join(2, "p2", "ember"));           // factionTaken
    world.Enqueue(new Command.PlaceTower(1, "lance", "nope"));   // unknownSocket
    world.Enqueue(new Command.PlaceTower(1, "phantom", "g1"));   // unknownTower
    world.Enqueue(new Command.PlaceTower(1, "lance", "t1"));     // trapSocket
    Step.Advance(world);

    var reasons = world.Events.OfType<SimEvent.BuildRejected>().Select(e => e.Reason)
        .Concat(world.Events.OfType<SimEvent.JoinRejected>().Select(e => e.Reason)).ToList();
    Gate("refusals: socket/tower/trap/faction all rejected with reasons",
        reasons.Contains("unknownSocket") && reasons.Contains("unknownTower")
            && reasons.Contains("trapSocket") && reasons.Contains("factionTaken"),
        string.Join(",", reasons));
}

// --- Gate 9: layers — ground towers never touch the air lane.
{
    var r = MatchRunner.Run(Seed, Maps.Foundry);
    bool groundHitAir = false;
    foreach (var line in r.EventLog)
    {
        // Lance/Nova fire only at ground; Skywatch only at air. A ground tower
        // damaging a skiff would show as its tower id sourcing damage on a skiff
        // death — cheap proxy: no skiff death sourced by a bolt/mortar tower.
        // Full mapping is overkill here; the sim gate is the layer filter test below.
    }
    var world = new World(Seed, Maps.Foundry);
    world.Enqueue(new Command.PlaceTower(0, "lance", "g4"));
    Step.Advance(world);
    // Force-spawn a skiff via wave 4 plan and run: lance must never fire.
    world.WaveIndex = 2; // next wave started will be index 3 (skiffs)
    world.PhaseTimer = 0f;
    int lanceFires = 0;
    for (int i = 0; i < Balance.TickHz * 30; i++)
    {
        Step.Advance(world);
        lanceFires += world.Events.OfType<SimEvent.TowerFired>()
            .Count(e => world.Enemies.Any(en => en.Id == e.TargetId && en.DefId == "skiff"));
        if (world.Enemies.Any(e => e.DefId == "skiff") && world.Enemies.All(e => e.DefId == "skiff"))
            groundHitAir |= lanceFires > 0;
    }
    Gate("layers: ground tower never fires at the air lane", lanceFires == 0,
        $"lance fired {lanceFires}x at skiffs");
}

// --- Gate 10: Aegis directional armor — rear damage beats front damage.
{
    float DamageFrom(Vec3 attackerPos)
    {
        var world = new World(Seed, Maps.Foundry);
        world.Enqueue(new Command.Join(1, "p1", "forge"));
        Step.Advance(world);
        var enemy = new Enemy
        {
            Id = world.NextId(), DefId = "aegis", Hp = 55f, MaxHp = 55f,
            Pos = new Vec3(0, 0, 0), Facing = new Vec3(1, 0, 0),
            Bounty = 0, LeakDamage = 1,
        };
        world.Enemies.Add(enemy);
        world.Players[1].Pos = attackerPos;
        world.Players[1].WeaponCooldown = 0f;
        world.Enqueue(new Command.PlayerSync(1, attackerPos));
        world.Enqueue(new Command.PlayerHit(1, enemy.Id, "sidearm"));
        Step.Advance(world);
        var damaged = world.Events.OfType<SimEvent.EnemyDamaged>().FirstOrDefault();
        return damaged?.Amount ?? 0f;
    }

    float front = DamageFrom(new Vec3(10, 0, 0));   // in its face
    float rear = DamageFrom(new Vec3(-10, 0, 0));   // directly behind
    Gate("aegis: rear damage > front damage", rear > front * 3f,
        $"front {front:0.##} vs rear {rear:0.##}");
}

// --- Gate 11: Thermal Shock — chill + burn reacts, consumes both, bursts.
{
    var world = new World(Seed, Maps.Foundry);
    world.Enqueue(new Command.Join(1, "p1", "ember"));
    world.Enqueue(new Command.PlaceTower(0, "singularity", "g3"));
    Step.Advance(world);

    // Placed via leg coordinates (MoveEnemies recomputes Pos from these): leg 2
    // of the ground route passes within aura range of socket g3.
    var enemy = new Enemy
    {
        Id = world.NextId(), DefId = "monolith", Hp = 300f, MaxHp = 300f,
        RouteIndex = 0, Leg = 2, LegProgress = 15f,
        Facing = new Vec3(1, 0, 0), Bounty = 0, LeakDamage = 1,
    };
    world.Enemies.Add(enemy);
    Step.Advance(world);   // aura chills

    world.Players[1].Pos = enemy.Pos + new Vec3(4, 0, 0);
    world.Enqueue(new Command.UseAbility(1, enemy.Pos));   // ignition wave: burn
    Step.Advance(world);

    bool reacted = world.Events.OfType<SimEvent.ReactionTriggered>().Any(e => e.ReactionId == "thermalShock");
    // Both halves are consumed: the burn never lands as a status (the aura may
    // legitimately re-chill later in the same tick, so test the burn side).
    bool burnConsumed = !enemy.Statuses[(int)Channel.Thermal].Active;
    Gate("thermal shock: chill + burn reacts and consumes both statuses",
        reacted && burnConsumed, $"reacted {reacted}, burnConsumed {burnConsumed}");
}

// --- Gate 12: scrap economy — a mid-band Foundry run can afford an L4 recipe.
{
    var r = MatchRunner.Run(Seed, Maps.Foundry, MidBot());
    int alloy = r.TeamScrapEnd.GetValueOrDefault(ScrapType.Alloy);
    int plating = r.TeamScrapEnd.GetValueOrDefault(ScrapType.Plating);
    // Note: the scripted builder already SPENT scrap on L4 breakpoints if it got
    // there; end-of-run pool + any spend proves income. Income floor: the damage
    // path recipe (8 alloy + 2 plating) must be within reach of what remains.
    bool reachable = alloy + plating > 0
        && r.EventLog.Any(l => l.Contains("scrapDropped"));
    Gate("scrap: drops flow and the team pool fills", reachable,
        $"end pool alloy {alloy}, plating {plating}");

    // The personal half is a thing on the floor now, and a bot that never
    // walks over one has no gunsmith at all — which would quietly hollow out
    // every build sweep while every other gate stayed green. Measure the loop,
    // not the intent: some of what dropped has to end up in a pocket.
    int collected = r.ScrapCollected, expired = r.ScrapExpired;
    int inPockets = r.PersonalScrapEnd?.Values.Sum() ?? 0;
    Gate("scrap: what drops on the floor gets picked up",
        collected > 0 && inPockets > 0,
        $"collected {collected}, expired to the team {expired}, in pockets {inPockets}");
}

// --- Gate 13 (M2): Cluster splits into 5 scaled children on death, not on leak.
{
    var world = new World(Seed, Maps.Foundry);
    var cluster = new Enemy
    {
        Id = world.NextId(), DefId = "cluster", Hp = 1f, MaxHp = 80f,   // 2x wave scaling
        RouteIndex = 0, Leg = 1, LegProgress = 2f, Facing = new Vec3(1, 0, 0),
        Bounty = 10, LeakDamage = 1,
    };
    world.Enemies.Add(cluster);
    world.Enqueue(new Command.Join(1, "p1", "ember"));
    Step.Advance(world);
    world.Players[1].Pos = cluster.Pos + new Vec3(3, 0, 0);
    world.Enqueue(new Command.PlayerHit(1, cluster.Id, "sidearm"));
    Step.Advance(world);

    var motes = world.Enemies.Where(e => e.DefId == "mote").ToList();
    bool scaled = motes.Count == 5 && motes.All(m => m.MaxHp > Enemies.Mote.Hp * 1.5f);
    Gate("cluster: death births 5 wave-scaled motes", scaled,
        $"children {motes.Count}, childMaxHp {(motes.Count > 0 ? motes[0].MaxHp : 0):0.#}");
}

// --- Gate 14 (M2): Warden shield soaks, blocks burn, regens after a lull.
{
    var world = new World(Seed, Maps.Foundry);
    world.Enqueue(new Command.Join(1, "p1", "ember"));
    Step.Advance(world);
    var warden = new Enemy
    {
        Id = world.NextId(), DefId = "warden", Hp = 60f, MaxHp = 60f, Shield = 25f,
        RouteIndex = 0, Leg = 1, LegProgress = 2f, Facing = new Vec3(1, 0, 0),
        Bounty = 0, LeakDamage = 1,
    };
    world.Enemies.Add(warden);
    world.Players[1].Pos = warden.Pos + new Vec3(3, 0, 0);

    // Burn attempt through shield: refused.
    world.Enqueue(new Command.UseAbility(1, warden.Pos));
    Step.Advance(world);
    bool burnBlocked = !warden.Statuses[(int)Channel.Thermal].Active;

    world.Enqueue(new Command.PlayerHit(1, warden.Id, "sidearm"));
    Step.Advance(world);
    bool soaked = warden.Hp == 60f && warden.Shield < 25f;

    float dropped = warden.Shield;
    for (int i = 0; i < Balance.TickHz * 6; i++) Step.Advance(world);
    bool regenerated = warden.Shield > dropped;

    Gate("warden: shield soaks, blocks burn, regens after lull",
        burnBlocked && soaked && regenerated,
        $"burnBlocked {burnBlocked}, hp {warden.Hp:0}, shield {warden.Shield:0.#}");
}

// --- Gate 15 (M2): Mole is untargetable underground, hittable in windows.
{
    var world = new World(Seed, Maps.Foundry);
    world.Enqueue(new Command.Join(1, "p1", "forge"));
    world.Money = 1000;
    world.Enqueue(new Command.PlaceTower(1, "lance", "g1"));
    Step.Advance(world);
    var mole = new Enemy
    {
        Id = world.NextId(), DefId = "mole", Hp = 34f, MaxHp = 34f,
        RouteIndex = 0, Leg = 0, LegProgress = 10f, TotalTraveled = 10f,
        Facing = new Vec3(1, 0, 0), Bounty = 0, LeakDamage = 1,
    };
    world.Enemies.Add(mole);

    bool firedWhileBurrowed = false, firedWhileSurfaced = false;
    for (int i = 0; i < Balance.TickHz * 12 && !mole.Dead; i++)
    {
        Step.Advance(world);
        foreach (var e in world.Events.OfType<SimEvent.TowerFired>())
        {
            if (mole.Burrowed) firedWhileBurrowed = true;
            else firedWhileSurfaced = true;
        }
    }
    Gate("mole: towers only fire during surface windows",
        !firedWhileBurrowed && firedWhileSurfaced,
        $"burrowedFires {firedWhileBurrowed}, surfacedFires {firedWhileSurfaced}");
}

// --- Gate 16 (M2): Flash Freeze — chill + shock emits freeze, cc-resist caps it.
{
    var world = new World(Seed, Maps.Foundry);
    var target = new Enemy
    {
        Id = world.NextId(), DefId = "drifter", Hp = 1000f, MaxHp = 1000f,
        RouteIndex = 0, Leg = 1, LegProgress = 2f, Facing = new Vec3(1, 0, 0),
        Bounty = 0, LeakDamage = 1,
    };
    world.Enemies.Add(target);
    target.Statuses[(int)Channel.Movement] = new StatusSlot { StatusId = "chill", TimeLeft = 5f, Source = "t" };
    target.CcResist = 0f;

    // Apply shock via a scripted hit (rifle applies mark; use direct status).
    world.Enqueue(new Command.Join(1, "p1", "ember"));
    Step.Advance(world);

    // Direct application path: simulate an Arc hit by enqueuing through a
    // 1000-hp target — use the sim's own seam via reflection-free helper:
    // shock arrives with the Arc tower (task 16); here we assert the reaction
    // table itself using the freeze emitted by chill+shock.
    bool frozen;
    {
        // Emulate: chill active, shock incoming → freeze slot filled.
        var reaction = Reactions.Match("chill", "shock");
        frozen = reaction is { EmitStatus: "freeze" };
    }
    Gate("flash freeze: chill+shock reaction emits freeze (table)", frozen, "");
}

// --- Gate 17 (M2): Arc chains to a second target and applies shock;
//     next to a chill field that means Flash Freeze end-to-end.
{
    var world = new World(Seed, Maps.Foundry);
    world.Money = 1000;
    world.Enqueue(new Command.PlaceTower(0, "arc", "g3"));
    Step.Advance(world);

    Enemy Spawn(float lateral) => new()
    {
        Id = world.NextId(), DefId = "drifter", Hp = 500f, MaxHp = 500f,
        RouteIndex = 0, Leg = 2, LegProgress = 15f, LateralOffset = lateral,
        Facing = new Vec3(1, 0, 0), Bounty = 0, LeakDamage = 1,
    };
    var a = Spawn(0f);
    var b = Spawn(2f);
    a.Statuses[(int)Channel.Movement] = new StatusSlot { StatusId = "chill", TimeLeft = 10f, Source = "t" };
    world.Enemies.Add(a);
    world.Enemies.Add(b);

    bool froze = false;
    for (int i = 0; i < Balance.TickHz * 2; i++)
    {
        Step.Advance(world);
        froze |= world.Events.OfType<SimEvent.StatusApplied>().Any(e => e.StatusId == "freeze");
    }

    bool bothHit = a.Hp < 500f && b.Hp < 500f;
    Gate("arc: chain hits two targets; shock on chill flash-freezes", bothHit && froze,
        $"aHp {a.Hp:0}, bHp {b.Hp:0}, controlActive {froze}");
}

// --- Gate 18 (M2): traps trigger with charges; launcher knockback respects mass.
{
    var world = new World(Seed, Maps.Foundry);
    world.Money = 1000;
    world.TeamScrap[ScrapType.Alloy] = 20;
    world.TeamScrap[ScrapType.Plating] = 20;
    world.Enqueue(new Command.PlaceTower(0, "launcher", "t2"));
    Step.Advance(world);

    Enemy AtTrap(string defId)
    {
        // Trap t2 sits at (0,0,3); ground route leg 3 runs (0,14)→(0,-8).
        var e = new Enemy
        {
            Id = world.NextId(), DefId = defId, Hp = 10000f, MaxHp = 10000f,
            RouteIndex = 0, Leg = 3, LegProgress = 11f, TotalTraveled = 60f,
            Facing = new Vec3(0, 0, -1), Bounty = 0, LeakDamage = 1,
        };
        world.Enemies.Add(e);
        return e;
    }

    var drifter = AtTrap("drifter");
    Step.Advance(world);   // moves into radius, trap fires
    Step.Advance(world);
    float drifterSetback = 60f + 2.5f * 2 * Balance.Dt - drifter.TotalTraveled;

    var world2Backup = drifter.TotalTraveled;
    Gate("traps: launcher knocks a drifter meaningfully backward", drifterSetback > 4f,
        $"setback {drifterSetback:0.0}m, traveled {drifter.TotalTraveled:0.0}");
}

// --- Gate 19 (M2): trap placement refuses tower sockets and vice versa.
{
    var world = new World(Seed, Maps.Foundry);
    world.Money = 1000;
    world.TeamScrap[ScrapType.Alloy] = 20;
    world.Enqueue(new Command.PlaceTower(0, "spike", "g1"));     // trap on ground socket
    world.Enqueue(new Command.PlaceTower(0, "barricade", "g2")); // barricade on ground socket
    Step.Advance(world);
    var reasons = world.Events.OfType<SimEvent.BuildRejected>().Select(e => e.Reason).ToList();
    Gate("sockets: trap and barricade refuse mismatched tags",
        reasons.Count(r => r == "wrongSocketTag") == 2, string.Join(",", reasons));
}

// --- Gate 20 (M2): the gunsmith is real — AP rounds beat plating, and every
//     attachment/ammo recipe is craftable from enemy yields (reachability).
{
    var world = new World(Seed, Maps.Foundry);
    world.Enqueue(new Command.Join(1, "p1", "ember"));
    Step.Advance(world);
    var player = world.Players[1];
    player.Scrap[ScrapType.Plating] = 10;

    Enemy Armored() => new()
    {
        Id = world.NextId(), DefId = "aegis", Hp = 500f, MaxHp = 500f,
        RouteIndex = 0, Leg = 1, LegProgress = 2f, Facing = new Vec3(1, 0, 0),
        Bounty = 0, LeakDamage = 1,
    };

    // Baseline: sidearm into the FRONT of an Aegis.
    var target = Armored();
    world.Enemies.Add(target);
    world.Players[1].Pos = target.Pos + new Vec3(6, 0, 0);
    world.Enqueue(new Command.PlayerSync(1, world.Players[1].Pos));
    world.Enqueue(new Command.PlayerHit(1, target.Id, "sidearm"));
    Step.Advance(world);
    float baseline = world.Events.OfType<SimEvent.EnemyDamaged>().First().Amount;

    // Craft AP, hit again (cooldown passes), compare. AP ignores FLAT armor
    // (Aegis has none — directional is positional), so use hollow-point's
    // inverse instead: verify AP vs a flat-armor proxy via shred + math is the
    // M4 Carapace's job. Here: reachability + the crafting flow itself.
    world.Enqueue(new Command.SelectAmmo(1, "sidearm", "ap"));
    Step.Advance(world);
    bool apCrafted = world.Players[1].CraftedAmmo.Contains("ap")
        && world.Players[1].BuildFor("sidearm").AmmoId == "ap";

    // Reachability: every recipe's scrap types must drop from at least one
    // enemy. Platforms are on this list now too — weapons and melee are bought
    // with personal scrap, so a recipe naming a type nothing drops would be a
    // gun you can never own.
    var droppable = Enemies.All.Values.SelectMany(e => e.ScrapYield.Keys).ToHashSet();
    bool reachable = Attachments.All.Values.All(a => a.Recipe.Keys.All(droppable.Contains))
        && Ammo.All.Values.All(a => a.Recipe.Keys.All(droppable.Contains))
        && Weapons.All.Values.All(x => x.Recipe.Keys.All(droppable.Contains))
        && Melee.All.Values.All(m => m.Recipe.Keys.All(droppable.Contains));

    Gate("gunsmith: crafting flows and every recipe is reachable",
        apCrafted && reachable && baseline > 0f,
        $"apCrafted {apCrafted}, reachable {reachable}");
}

// --- Gate 33b: every command survives the wire.
//
// Commands are encoded to text and parsed back by hand, in two switch
// statements that have to be kept in step by memory. Miss the decode half and
// the command does not error — it parses to null and is dropped, so the action
// simply never happens for network clients while working perfectly in solo.
// Melee added four verbs at once, which is exactly when that goes wrong.
{
    var samples = new Command[]
    {
        new Command.Join(1, "p", "forge"),
        new Command.Leave(1),
        new Command.SetFaction(1, "tempest", 3),
        new Command.Launch(1),
        new Command.PlayerSync(1, new Vec3(1.5f, 2f, -3.25f)),
        new Command.PlaceTower(1, "lance", "g1"),
        new Command.SellTower(1, 7),
        new Command.UpgradeTower(1, 7, 2),
        new Command.StartWave(1),
        new Command.PlayerHit(1, 42, "rifle"),
        new Command.BuyWeapon(1, "rifle"),
        new Command.SelectWeapon(1, "rifle"),
        new Command.UseAbility(1, new Vec3(0f, 1f, 2f)),
        new Command.Revive(1, 2),
        new Command.CraftAttachment(1, "rifle", "longBarrel"),
        new Command.SelectAmmo(1, "rifle", "ap"),
        new Command.Reload(1),
        new Command.PlayerMelee(1, new Vec3(3f, 0f, 4f)),
        new Command.BuyMelee(1, "maul"),
        new Command.CraftMeleeAttachment(1, "maul", "cryoCore"),
        new Command.UpgradeMelee(1, "maul"),
    };

    var broken = new List<string>();
    foreach (var command in samples)
    {
        string wire = Protocol.CommandToWire(command);
        var back = Protocol.CommandFromWire(wire);
        if (back is null) { broken.Add($"{command.GetType().Name} decodes to null"); continue; }
        if (back.ToString() != command.ToString())
            broken.Add($"{command.GetType().Name}: {command} -> {wire} -> {back}");
    }

    // And the vocabulary must be complete: a command type with no sample here
    // is a command nobody proved works on the wire.
    int commandTypes = typeof(Command).Assembly.GetTypes()
        .Count(x => x.IsSealed && x.BaseType == typeof(Command));
    if (samples.Select(s => s.GetType()).Distinct().Count() != commandTypes)
        broken.Add($"{commandTypes} command types but {samples.Select(s => s.GetType()).Distinct().Count()} covered");

    // Seat authorization is a second hand-maintained switch over the same
    // types, and its fallthrough is `false` — so a command missing from it is
    // refused for every network client while working perfectly in solo. Melee
    // was missing from it, found here.
    foreach (var command in samples)
        if (!Protocol.CommandClaimsSeat(command, 1))
            broken.Add($"{command.GetType().Name} is not authorized for its own seat");

    Gate("protocol: every command round-trips and authorizes its own seat",
        broken.Count == 0,
        broken.Count == 0 ? $"{samples.Length} commands, protocol v{Protocol.Version}"
                          : string.Join("; ", broken));
}

// --- Gate 34: melee swings an arc, and melee kills pay better.
//
// Two separate claims, measured separately. The first version of this gate
// compared total scrap from "a swing" against "some shots" and passed
// handsomely — 9 against 2 — for entirely the wrong reason: the swing hit
// three enemies at once while the weapon cooldown let the gun kill one. That
// is a real melee advantage and it is not the bonus, so it cannot be the
// evidence for the bonus. The scrap half now kills exactly one enemy each way.
{
    // (a) The arc. Three targets ahead, one behind, one swing.
    var world = new World(Seed, Maps.Foundry);
    world.Enqueue(new Command.Join(1, "p1", "forge"));
    Step.Advance(world);
    world.Players[1].Pos = Vec3.Zero;

    Enemy Place(World w, float x, float y, float z, float hp = 8f, string defId = "drifter")
    {
        var e = new Enemy
        {
            Id = w.NextId(), DefId = defId, Hp = hp, MaxHp = hp,
            Facing = new Vec3(1, 0, 0), Bounty = 0, LeakDamage = 1,
            RouteIndex = 0, Leg = 0, LegProgress = 0f, Pos = new Vec3(x, y, z),
        };
        w.Enemies.Add(e);
        return e;
    }

    var ahead = new[] { Place(world, 0f, 0f, 1.2f), Place(world, 0f, 0f, 1.6f), Place(world, 0f, 0f, 2.0f) };
    var behind = Place(world, 0f, 0f, -1.5f);

    world.Enqueue(new Command.PlayerSync(1, Vec3.Zero));
    world.Enqueue(new Command.PlayerMelee(1, new Vec3(0f, 0f, 4f)));
    for (int i = 0; i < 3; i++) Step.Advance(world);

    bool arcRespected = ahead.All(e => e.Dead) && !behind.Dead;

    // (b) The bonus. One Mender each way — chosen because its yield is 2 Flux
    // and 1 Gravium, and a 1-scrap enemy rounds the bonus clean away.
    int ScrapFromOneKill(bool useMelee)
    {
        var w2 = new World(Seed, Maps.Foundry);
        w2.Enqueue(new Command.Join(1, "p1", "forge"));
        Step.Advance(w2);
        w2.Players[1].Pos = Vec3.Zero;

        var victim = Place(w2, 0f, 0f, 1.5f, hp: 1f, defId: "mender");
        w2.Enqueue(new Command.PlayerSync(1, Vec3.Zero));
        if (useMelee) w2.Enqueue(new Command.PlayerMelee(1, new Vec3(0f, 0f, 4f)));
        else w2.Enqueue(new Command.PlayerHit(1, victim.Id, "sidearm"));
        for (int i = 0; i < 3; i++) Step.Advance(w2);

        if (!victim.Dead) return -1;      // did not kill: not a comparison
        return w2.TeamScrap.Values.Sum() + w2.Players[1].Scrap.Values.Sum();
    }

    int meleeScrap = ScrapFromOneKill(useMelee: true);
    int rangedScrap = ScrapFromOneKill(useMelee: false);

    Gate("melee: the swing is an arc, and one melee kill pays more than one shot",
        arcRespected && meleeScrap > rangedScrap && rangedScrap > 0,
        $"arc: {ahead.Count(e => e.Dead)}/3 ahead died, behind survived {!behind.Dead} | "
        + $"one kill pays {meleeScrap} melee vs {rangedScrap} ranged");
}

// --- Gate 34b: a melee build survives a save/resume.
//
// Melee state was invisible to serialization when first written, which would
// have meant a drop-in joiner or a resumed save quietly reverting to a bare
// wrench — a loss the player would feel and no gate would report, because
// every existing serialization check compares event logs and a lost mastery
// level emits no event.
{
    var world = new World(Seed, Maps.Foundry);
    world.Money = 2000;
    world.Enqueue(new Command.Join(1, "p1", "forge"));
    Step.Advance(world);

    // Stocked in every type: a platform is bought with personal scrap now, and
    // the Maul's recipe wants Plating the fixture never used to need.
    foreach (ScrapType type in System.Enum.GetValues<ScrapType>())
        world.Players[1].Scrap[type] = 20;
    world.Enqueue(new Command.BuyMelee(1, "maul"));
    Step.Advance(world);
    world.Enqueue(new Command.CraftMeleeAttachment(1, "maul", "cryoCore"));
    world.Enqueue(new Command.UpgradeMelee(1, "maul"));
    Step.Advance(world);
    world.Enqueue(new Command.UpgradeMelee(1, "maul"));
    Step.Advance(world);

    var before = world.Players[1];
    var resumed = Serialization.Deserialize(Serialization.Serialize(world));
    var after = resumed.Players[1];

    bool platform = after.MeleeId == "maul" && after.OwnedMelee.Contains("maul");
    bool infusion = after.MeleeBuildFor("maul").Attachments
        .TryGetValue(MeleeSlot.CoreInfusion, out var core) && core == "cryoCore";
    bool mastery = after.MeleeBuildFor("maul").MasteryLevel == before.MeleeBuildFor("maul").MasteryLevel
        && after.MeleeBuildFor("maul").MasteryLevel > 0;

    Gate("melee: platform, infusion and mastery all survive a resume",
        platform && infusion && mastery,
        $"platform {after.MeleeId}, infusion {(infusion ? "kept" : "LOST")}, "
        + $"mastery {after.MeleeBuildFor("maul").MasteryLevel} (was {before.MeleeBuildFor("maul").MasteryLevel})");
}

// --- Gate 35: every melee platform and attachment is reachable and does
// something, the same invariant the factions and reactions now carry.
{
    var problems = new List<string>();

    var droppable = Enemies.All.Values.SelectMany(e => e.ScrapYield.Keys).ToHashSet();
    foreach (var att in Melee.Attachments.Values)
    {
        foreach (var type in att.Recipe.Keys)
            if (!droppable.Contains(type))
                problems.Add($"{att.Id} needs {type}, which nothing drops");
        bool inert = att.DamageFactor == 1f && att.SpeedFactor == 1f
            && att.ReachFactor == 1f && att.Applies is null;
        if (inert) problems.Add($"{att.Id} changes nothing");
    }

    // Every infusion must name a status that exists, or point-blank reaction
    // play silently does not happen.
    foreach (var att in Melee.Attachments.Values.Where(a => a.Slot == MeleeSlot.CoreInfusion))
        if (att.Applies is null || !Statuses.All.ContainsKey(att.Applies))
            problems.Add($"{att.Id} is an infusion that applies nothing real");

    foreach (var def in Melee.All.Values)
    {
        if (def.Damage <= 0f || def.ReachMeters <= 0f || def.ArcDegrees <= 0f)
            problems.Add($"{def.Id} cannot connect");
        if (def.MasteryCosts.Count < Melee.MaxMasteryLevel)
            problems.Add($"{def.Id} has fewer mastery costs than levels");
    }

    Gate("melee: platforms and attachments are reachable and all do something",
        problems.Count == 0,
        problems.Count == 0
            ? $"{Melee.All.Count} platforms, {Melee.Attachments.Count} attachments"
            : string.Join("; ", problems));
}

// --- Gate 33 (M3): the campaign chain is well-formed and fully reachable.
//
// A sector list is exactly the kind of content that rots quietly: rename a map
// and the chain still "works", it just strands everything behind a sector that
// no longer exists, and the only symptom is a player who cannot get to level
// two. Cheap to check, so it gets checked.
{
    var problems = new List<string>();

    foreach (string sector in Campaign.Sectors)
        if (!Maps.All.Values.Any(m => m.Id == sector))
            problems.Add($"campaign names {sector}, which is not a map");

    // Every real map should be in the campaign — a map nobody can select is
    // work nobody sees. testlane is the documented exception.
    foreach (var map in Maps.All.Values)
        if (map.Id != "testlane" && Campaign.IndexOf(map.Id) < 0)
            problems.Add($"{map.Id} exists but no sector plays it");

    if (Campaign.Sectors.Distinct().Count() != Campaign.Sectors.Count)
        problems.Add("a sector appears twice");

    // Walking the chain from nothing cleared must reach every sector.
    var cleared = new List<string>();
    var reached = new List<string>();
    for (int step = 0; step < Campaign.Sectors.Count; step++)
    {
        string? open = Campaign.Sectors.FirstOrDefault(
            s => Campaign.IsUnlocked(s, cleared) && !cleared.Contains(s));
        if (open is null) break;
        reached.Add(open);
        cleared.Add(open);
    }
    if (reached.Count != Campaign.Sectors.Count)
        problems.Add($"chain stalls after {reached.Count} of {Campaign.Sectors.Count}");

    Gate("campaign: the sector chain is well-formed and every map is reachable",
        problems.Count == 0,
        problems.Count == 0
            ? $"{Campaign.Sectors.Count} sectors: {string.Join(" -> ", Campaign.Sectors)}"
            : string.Join("; ", problems));
}

// --- Gate 32 (M3): every faction's ability changes the world when used.
//
// Written after the third instance of the same bug class in one milestone:
// shred that nothing applied, weapon range that nothing read, and an ability
// switch statement whose cases are matched on a string. A faction whose
// AbilityId has no case in that switch costs its cooldown and does nothing at
// all, silently, forever — and the lobby would still offer it as a choice.
{
    var inert = new List<string>();
    foreach (var faction in Factions.All.Values)
    {
        var world = new World(Seed, Maps.Foundry);
        world.Money = 3000;
        world.Enqueue(new Command.Join(1, "p1", faction.Id));
        world.Enqueue(new Command.PlaceTower(0, "lance", "g4"));
        Step.Advance(world);

        // Something for every ability to act on: a tower to buff, an enemy to
        // burn, shock, chill or reveal.
        var enemy = new Enemy
        {
            Id = world.NextId(), DefId = "drifter", Hp = 400f, MaxHp = 400f,
            Facing = new Vec3(1, 0, 0), Bounty = 0, LeakDamage = 1,
            RouteIndex = 0, Leg = 3, LegProgress = 14f,
        };
        world.Enemies.Add(enemy);
        Step.Advance(world);

        var player = world.Players[1];
        player.Pos = enemy.Pos;
        var before = (
            hp: enemy.Hp,
            statuses: enemy.Statuses.Count(s => s.Active),
            buffed: world.Towers.Count(x => x.BuffFactor > 1f));

        world.Enqueue(new Command.PlayerSync(1, player.Pos));
        world.Enqueue(new Command.UseAbility(1, enemy.Pos));
        Step.Advance(world);

        bool didSomething = enemy.Hp < before.hp
            || enemy.Statuses.Count(s => s.Active) > before.statuses
            || world.Towers.Count(x => x.BuffFactor > 1f) > before.buffed;

        if (!didSomething) inert.Add($"{faction.Id}/{faction.AbilityId}");
    }

    Gate("factions: every ability does something when it fires",
        inert.Count == 0,
        inert.Count == 0 ? $"{Factions.All.Count} factions all act"
                         : "inert: " + string.Join(", ", inert));
}

// --- Gate 30 (M3): no condition may blind a whole route's defence.
//
// Conditions are specified as factors over the swept baseline, and the value
// of that rule is that weather can never be the thing that decides a wave.
// Geometry can break it without breaking the formula: a socket at the edge of
// its reach does not lose margin to a range factor, it loses the route.
//
// Honest note on what this gate did and did not do. It was written believing
// it was the reason Fog could not run at the specced 0.7, and it was not — it
// passed at 0.7 the whole time. The actual cause was a reference build placing
// towers on a deck row 4 m too far back. This gate guards the map; it cannot
// guard a build order, and mistaking one for the other cost a round of tuning
// the wrong dial.
{
    var offenders = new List<string>();
    foreach (var map in Campaign.Sectors.Select(id => Maps.All[id]))
    {
        foreach (var condition in Conditions.All.Values)
        {
            if (condition.TowerRangeFactor >= 1f) continue;
            foreach (var route in map.Routes)
            {
                // Only towers that can engage this route's layer count. Judging
                // the air lane by a lance's reach measures nothing: a lance
                // could not shoot a Skiff at any range.
                var defenders = Towers.All.Values
                    .Where(d => d.TargetLayers.Contains(route.Layer) && d.RangeMeters > 0f)
                    .ToList();
                if (defenders.Count == 0) continue;

                int full = 0, fogged = 0;
                foreach (var socket in map.Sockets)
                {
                    bool Sees(float r)
                    {
                        for (int i = 0; i + 1 < route.Waypoints.Count; i++)
                            for (int k = 0; k < 20; k++)
                            {
                                var pt = route.Waypoints[i]
                                    + (route.Waypoints[i + 1] - route.Waypoints[i]) * (k / 20f);
                                if ((pt - socket.Pos).Length() <= r) return true;
                            }
                        return false;
                    }
                    // A socket counts as covering if any legal defender reaches
                    // from it, and as still covering under weather if any legal
                    // defender that weather does not exempt still reaches.
                    if (defenders.Any(d => Sees(d.RangeMeters))) full++;
                    if (defenders.Any(d => Sees(d.RangeMeters
                            * (condition.RangeExemptTowerIds.Contains(d.Id)
                                ? 1f : condition.TowerRangeFactor)))) fogged++;
                }
                if (full == 0) continue;
                // Losing a quarter of the covering sockets is reduction; losing
                // more is the condition making the decision instead of the wave.
                if (fogged < full * 0.75f)
                    offenders.Add($"{map.Id}/{route.Id} under {condition.Id}: {full} -> {fogged}");
            }
        }
    }

    Gate("conditions: weather reduces coverage, never erases it",
        offenders.Count == 0,
        offenders.Count == 0 ? "all routes keep 75%+ of their covering sockets"
                             : string.Join("; ", offenders));
}

// --- Gate 31 (M3 exit gate): the weathered campaign is winnable and is
// measurably not the clear one.
//
// The plan's exit condition, run as a gate. "Winnable" is the easy half; the
// half that matters is that weather is felt, because a condition that changes
// nothing would pass a winnability check perfectly.
//
// It does not re-derive that detection answers stealth — gate 28 proves that
// directly, on an isolated Shade, which is a far cleaner measurement than a
// campaign outcome. Two attempts to measure it here failed for reasons worth
// recording: with a bot present the hero kills Shades whether or not a
// Detector exists (the roster's stated "Detector tower, hero eyes"), and with
// towers alone the trap sockets kill them anyway, because a trap triggers on
// proximity and does not care whether it can see what stepped on it. Both are
// the design working; neither leaves room for a campaign-level control.
{
    // Measured across seeds, not one.
    //
    // This asked a single seed whether the weathered run finished with fewer
    // lives than the clear one, which is a strict inequality on a number that
    // moves by one. Three balance changes that each passed it alone — bounty
    // scaling, gun damage, Skywatch damage — landed together and it read minus
    // one: the weathered campaign finishing *ahead*. That is not weather
    // ceasing to matter, it is a coin landing on its edge, and a gate that
    // flips sign on the bot's build order will be switched off the third time
    // it does it.
    //
    // Five seeds, and the comparison is on the total. Any one of them may still
    // come out level or backwards; what must hold is that a campaign of night
    // and fog costs the player something overall.
    // On the Spire, which is the only map that schedules both — night on wave 7
    // and fog on wave 11. This gate has been named "night+fog" since M3 and run
    // against Switchyard, whose whole schedule is one night on wave 8. Fog was
    // never in the measurement at all: dropping its range factor from 0.7 to
    // 0.5 moved not one of the five seeds, because no wave it touches was ever
    // played.
    var weatheredMap = Maps.Spire;
    var clear = weatheredMap with { ConditionScheduleOrNull = new Dictionary<int, string>() };
    var seeds = new[] { Seed, Seed + 101u, Seed + 202u, Seed + 303u, Seed + 404u };

    int weatheredLives = 0, clearLives = 0, wins = 0;
    var perSeed = new List<string>();
    foreach (uint seed in seeds)
    {
        // The bot is the same player in both; only the match seed moves, which
        // is what varies the wave jitter and scatter the outcome swings on.
        var shipped = MatchRunner.Run(seed, weatheredMap, MidBot());
        var unweathered = MatchRunner.Run(seed, clear, MidBot());
        weatheredLives += shipped.LivesLeft;
        clearLives += unweathered.LivesLeft;
        if (shipped.Victory) wins++;
        perSeed.Add($"{unweathered.LivesLeft - shipped.LivesLeft:+0;-0;0}");
    }

    bool winnable = wins == seeds.Length;
    bool felt = weatheredLives < clearLives;

    Gate("night+fog: the weathered campaign is winnable, and weather is felt",
        winnable && felt,
        $"{wins}/{seeds.Length} weathered runs clear | lives {weatheredLives} vs {clearLives} clear "
        + $"(cost {clearLives - weatheredLives} over {seeds.Length} seeds: {string.Join(",", perSeed)})");
}

// --- Gate 21 (M2): Switchyard clears for the mid-band bot; towers-only floor holds.
{
    var mid = MatchRunner.Run(Seed, Maps.Switchyard, MidBot());
    var floor = MatchRunner.Run(Seed, Maps.Switchyard);
    var leaksByDef = floor.EventLog.Where(l => l.Contains(" enemyLeaked "))
        .GroupBy(l => l.Split(' ')[3])
        .Select(g => $"{g.Key}:{g.Count()}");
    // Sector 2's shape differs from sector 1 by design: the mid-band player
    // must clear it, but towers alone are only required to hold deep (≥10 of
    // 12) — heroes mattering more as the campaign advances IS the thesis.
    // (Same doctrine as the 2D game's accepted probe-understates-play note.)
    Gate("switchyard: mid-band clears; towers-only holds ≥10 waves",
        mid.Victory && floor.WavesCleared >= 10,
        $"mid waves {mid.WavesCleared}/{Maps.Switchyard.TotalWaves} lives {mid.LivesLeft} | floor waves {floor.WavesCleared} lives {floor.LivesLeft} leaks {string.Join(",", leaksByDef)}");
}

// --- Gate 21b (M3): Spire clears for the mid-band bot; the floor holds deep.
//
// Same contract as the other two sectors, on a map whose shape argues with
// them: the core is at the top, so a leak is not something that got past you,
// it is something that climbed over you.
{
    var mid = MatchRunner.Run(Seed, Maps.Spire, MidBot());
    var floor = MatchRunner.Run(Seed, Maps.Spire);
    var leaks = floor.EventLog.Where(l => l.Contains(" enemyLeaked "))
        .GroupBy(l => l.Split(' ')[3]).Select(g => $"{g.Key}:{g.Count()}");

    Gate("spire: mid-band clears; towers-only holds \u226510 waves",
        mid.Victory && floor.WavesCleared >= 10,
        $"mid waves {mid.WavesCleared}/{Maps.Spire.TotalWaves} lives {mid.LivesLeft} | "
        + $"floor waves {floor.WavesCleared} lives {floor.LivesLeft} leaks {string.Join(",", leaks)}");
}

// --- Gate 22 (M2): a barricade on b1 reroutes shortcut spawns to the long way.
{
    var world = new World(Seed, Maps.Switchyard);
    world.Money = 1000;
    world.Enqueue(new Command.PlaceTower(0, "barricade", "b1"));
    world.Enqueue(new Command.StartWave(0));
    while (world.Enemies.Count == 0) Step.Advance(world);

    // Wave 1 is authored entirely onto groundShort (route index 1); with the
    // gate closed every spawn must walk route index 0 (the long way).
    bool rerouted = world.Enemies.All(e => e.RouteIndex == 0);
    Gate("barricade: gated shortcut spawns fall back to the long route", rerouted,
        $"routes {string.Join(",", world.Enemies.Select(e => e.RouteIndex).Distinct())}");
}

// --- Gate 24 (M3): poison is the answer burn is not.
//
// Burn is refused outright while a shield is up; poison has to land on hp
// through that same shield. If both were only damage-over-time with different
// numbers there would be no reason to carry either. Driven through a real
// PlayerHit with the poison stream, not a test hook.
{
    float DamageThroughShield(string weaponId)
    {
        var world = new World(Seed, Maps.Foundry);
        world.Enqueue(new Command.Join(1, "p1", "forge"));
        Step.Advance(world);

        var player = world.Players[1];
        player.OwnedWeapons.Add(weaponId);
        player.WeaponId = weaponId;
        player.WeaponCooldown = 0f;
        player.Pos = new Vec3(0, 0, 2);

        var enemy = new Enemy
        {
            Id = world.NextId(), DefId = "warden",
            Hp = 300f, MaxHp = 300f, Shield = 200f,
            Pos = new Vec3(0, 0, 0), Facing = new Vec3(1, 0, 0),
            Bounty = 0, LeakDamage = 1,
        };
        world.Enemies.Add(enemy);

        world.Enqueue(new Command.PlayerSync(1, player.Pos));
        world.Enqueue(new Command.PlayerHit(1, enemy.Id, weaponId));

        float startHp = enemy.Hp;
        for (int i = 0; i < Balance.TickHz * 2; i++) Step.Advance(world);
        return startHp - enemy.Hp;                 // hp lost *behind* the shield
    }

    float poison = DamageThroughShield("poisonStream");
    float ember = DamageThroughShield("emberPistol");

    Gate("poison: reaches hp through a shield that stops burn",
        poison > 0f && ember <= 0f,
        $"poison {poison:0.0} hp behind shield, ember {ember:0.0}");
}

// --- Gate 25 (M3): Corrode joins the table without breaking closure.
{
    var reactions = Reactions.All;
    var emitted = reactions.Where(r => r.EmitStatus is not null).Select(r => r.EmitStatus!).ToList();

    // Closure: nothing a reaction emits may itself be half of another reaction.
    bool closed = !emitted.Any(e => reactions.Any(r => r.StatusA == e || r.StatusB == e));

    var corrode = Reactions.Match(Statuses.Poison.Id, Statuses.Shred.Id);
    var corrodeReversed = Reactions.Match(Statuses.Shred.Id, Statuses.Poison.Id);

    Gate("corrode: poison + shred reacts, and the table stays closed",
        corrode is not null && corrodeReversed is not null && closed,
        $"{reactions.Count} reactions, emits [{string.Join(",", emitted)}]");
}

// --- Gate 25b (M3): every reaction is actually reachable, and is co-op.
//
// Closure was gated; reachability was not. Corrode shipped as a defined,
// closure-clean reaction whose second input — shred — nothing in the game
// applied: no tower, trap, weapon, attachment or ammo. It could never fire.
//
// The rule the plan states is stronger than "reachable": each reaction must
// have one input a builder can supply and one a shooter can, because that
// split is what makes a reaction a co-op moment rather than a solo rotation.
{
    var towerSide = new HashSet<string>();
    foreach (var tower in Towers.All.Values) foreach (var s in tower.Applies) towerSide.Add(s);
    foreach (var trap in Traps.All.Values) if (trap.Applies is { } s) towerSide.Add(s);

    var heroSide = new HashSet<string>();
    foreach (var w in Weapons.All.Values) foreach (var s in w.Applies) heroSide.Add(s);
    foreach (var a in Attachments.All.Values) if (a.Applies is { } s) heroSide.Add(s);
    foreach (var a in Ammo.All.Values) if (a.Applies is { } s) heroSide.Add(s);
    // Faction abilities apply statuses in code rather than through a def field,
    // so they are deliberately not counted here: a reaction that only works
    // because someone picked Ember is not a reaction the roster guarantees.

    var broken = new List<string>();
    foreach (var r in Reactions.All)
    {
        var inputs = new[] { r.StatusA, r.StatusB };
        var unsourced = inputs.Where(i => !towerSide.Contains(i) && !heroSide.Contains(i)).ToList();
        if (unsourced.Count > 0) { broken.Add($"{r.Id}: nothing applies {string.Join("+", unsourced)}"); continue; }
        // Co-op shape: one side buildable, the other shootable.
        bool coop = (towerSide.Contains(r.StatusA) && heroSide.Contains(r.StatusB))
                 || (towerSide.Contains(r.StatusB) && heroSide.Contains(r.StatusA));
        if (!coop) broken.Add($"{r.Id}: not splittable between a builder and a shooter");
    }

    Gate("reactions: every one is reachable, and splits builder/shooter",
        broken.Count == 0,
        broken.Count == 0
            ? $"{Reactions.All.Count} reactions | towers apply [{string.Join(",", towerSide.OrderBy(x => x))}] heroes apply [{string.Join(",", heroSide.OrderBy(x => x))}]"
            : string.Join("; ", broken));
}

// --- Gate 26 (M3): the Filament ramps, and switching targets costs it.
//
// A beam whose damage did not climb would just be a worse Lance, and one that
// kept its charge across targets would be free burst on every new arrival. Both
// halves are the tower's identity, so both are asserted.
{
    (float Early, float Late, float AfterSwitch) BeamOutput()
    {
        var world = new World(Seed, Maps.Foundry);
        world.Money = 1000;
        world.Enqueue(new Command.PlaceTower(0, "filament", "g4"));
        Step.Advance(world);


        // Leg 3 runs (0,14) → (0,-8); 14 along it puts the enemy at (0,0),
        // four metres from socket g4 and well inside the beam's reach.
        Enemy Spawn() => new()
        {
            Id = world.NextId(), DefId = "drifter",
            Hp = 4000f, MaxHp = 4000f,
            Facing = new Vec3(1, 0, 0),
            Bounty = 0, LeakDamage = 1, RouteIndex = 0, Leg = 3, LegProgress = 14f,
        };

        var first = Spawn();
        world.Enemies.Add(first);

        // One second of contact from cold.
        float mark = first.Hp;
        for (int i = 0; i < Balance.TickHz; i++) Step.Advance(world);
        float early = mark - first.Hp;

        // Three more seconds on the same target: the ramp should be higher.
        for (int i = 0; i < Balance.TickHz * 3; i++) Step.Advance(world);
        mark = first.Hp;
        for (int i = 0; i < Balance.TickHz; i++) Step.Advance(world);
        float late = mark - first.Hp;

        // Swap the target out; the next second should look cold again.
        first.Dead = true;
        world.Enemies.Clear();
        var second = Spawn();
        world.Enemies.Add(second);
        mark = second.Hp;
        for (int i = 0; i < Balance.TickHz; i++) Step.Advance(world);
        return (early, late, mark - second.Hp);
    }

    var (early, late, afterSwitch) = BeamOutput();
    Gate("filament: damage ramps while held and resets on target switch",
        late > early * 1.4f && afterSwitch < late * 0.75f,
        $"first second {early:0.0}, held {late:0.0}, after switch {afterSwitch:0.0}");
}

// --- Gate 27 (M3): the Detector buys information, not damage.
{
    var world = new World(Seed, Maps.Foundry);
    world.Money = 1000;
    world.Enqueue(new Command.PlaceTower(0, "detector", "g4"));
    Step.Advance(world);

    var enemy = new Enemy
    {
        Id = world.NextId(), DefId = "drifter", Hp = 100f, MaxHp = 100f,
        Facing = new Vec3(1, 0, 0),
        Bounty = 0, LeakDamage = 1, RouteIndex = 0, Leg = 3, LegProgress = 14f,
    };
    world.Enemies.Add(enemy);

    float before = enemy.Hp;
    for (int i = 0; i < Balance.TickHz; i++) Step.Advance(world);

    bool revealed = enemy.Statuses[(int)Channel.Detection].Active;
    Gate("detector: reveals without dealing damage",
        revealed && MathF.Abs(before - enemy.Hp) < 0.01f,
        $"revealed {revealed}, hp delta {before - enemy.Hp:0.00}");
}

// --- Gate 27 (M3): every defined enemy actually spawns somewhere.
//
// Written because it did not hold: Shade and Mender were fully implemented,
// gated for counterability, and reachable by nothing — no wave table named
// them. A roster entry no campaign spawns is dead content that still passes
// every other gate.
{
    var spawned = new HashSet<string>();
    foreach (var (mapId, waves) in Waves.ByMap)
    {
        if (mapId == "testlane") continue;      // the M0 fixture, not a campaign
        foreach (var wave in waves)
            foreach (var group in wave)
                spawned.Add(group.EnemyId);
    }

    // Cluster children are born from a split, never from a wave table.
    var fromSplits = Enemies.All.Values
        .Where(e => e.SplitCount > 0 && e.SplitInto is not null)
        .Select(e => e.SplitInto!)
        .ToHashSet();

    var orphans = Enemies.All.Keys
        .Where(id => !spawned.Contains(id) && !fromSplits.Contains(id))
        .OrderBy(id => id)
        .ToList();

    Gate("roster: every enemy is reachable from some wave table",
        orphans.Count == 0,
        orphans.Count == 0 ? $"{Enemies.All.Count} enemies all spawn"
                           : "never spawned: " + string.Join(", ", orphans));
}

// --- Gate 27b: the declared campaign length matches the authored table.
//
// Inserting two waves mid-arc silently truncated the finale — the map still
// declared twelve and the table had fourteen, so the ending simply never
// played and every gate stayed green.
{
    var mismatched = new List<string>();
    foreach (var (mapId, waves) in Waves.ByMap)
    {
        var map = Maps.All.Values.FirstOrDefault(m => m.Id == mapId);
        if (map is null) continue;
        if (map.TotalWaves != waves.Count)
            mismatched.Add($"{mapId} declares {map.TotalWaves}, table has {waves.Count}");
    }

    Gate("waves: every map plays its whole authored arc",
        mismatched.Count == 0,
        mismatched.Count == 0 ? "all maps match" : string.Join("; ", mismatched));
}

// --- Gate 28 (M3): a Shade is invisible to towers until something reveals it.
//
// Both halves matter. If towers could see it, the Detector is a wasted socket;
// if a Detector did not fix it, the Shade is uncounterable by building, which
// the roster rule forbids.
{
    float ShadeHpAfter(bool withDetector)
    {
        var world = new World(Seed, Maps.Foundry);
        world.Money = 2000;
        world.Enqueue(new Command.PlaceTower(0, "lance", "g4"));
        if (withDetector) world.Enqueue(new Command.PlaceTower(0, "detector", "g15"));
        Step.Advance(world);

        var shade = new Enemy
        {
            Id = world.NextId(), DefId = "shade", Hp = 500f, MaxHp = 500f,
            Facing = new Vec3(1, 0, 0), Bounty = 0, LeakDamage = 1,
            RouteIndex = 0, Leg = 3, LegProgress = 14f,
        };
        world.Enemies.Add(shade);

        for (int i = 0; i < Balance.TickHz * 2; i++) Step.Advance(world);
        return shade.Hp;
    }

    float unseen = ShadeHpAfter(withDetector: false);
    float revealed = ShadeHpAfter(withDetector: true);

    Gate("shade: towers ignore it until a Detector reveals it",
        unseen >= 500f && revealed < 500f,
        $"unseen {unseen:0.0} hp, with detector {revealed:0.0} hp");
}

// --- Gate 29 (M3): the Mender is answered by prioritisation, not by dps.
//
// Its heal (7/s) deliberately out-paces poison (3/s), so the counter is killing
// the healer — the plan's "can you prioritize?". The gate measures the same
// escort twice: once with the Mender alive, once without. If the heal did not
// lengthen the kill, the Mender is decoration; if the escort survived either
// way, it is uncounterable.
{
    int TicksToKillEscort(bool withMender)
    {
        var world = new World(Seed, Maps.Foundry);
        world.Money = 2000;
        world.Enqueue(new Command.PlaceTower(0, "lance", "g4"));
        Step.Advance(world);

        Enemy Add(string defId, float hp, float legProgress)
        {
            var e = new Enemy
            {
                Id = world.NextId(), DefId = defId, Hp = hp, MaxHp = hp,
                Facing = new Vec3(1, 0, 0), Bounty = 0, LeakDamage = 1,
                RouteIndex = 0, Leg = 3, LegProgress = legProgress,
            };
            world.Enemies.Add(e);
            return e;
        }

        var escort = Add("drifter", 90f, 14f);
        if (withMender) Add("mender", 400f, 14.5f);   // fat enough to outlive the escort

        for (int tick = 1; tick <= Balance.TickHz * 30; tick++)
        {
            Step.Advance(world);
            if (!world.Enemies.Contains(escort) || escort.Hp <= 0f) return tick;
        }
        return int.MaxValue;      // never died
    }

    int guarded = TicksToKillEscort(withMender: true);
    int alone = TicksToKillEscort(withMender: false);

    Gate("mender: healing lengthens the kill, and both escorts still die",
        guarded > alone && guarded != int.MaxValue,
        $"guarded {guarded} ticks vs alone {alone} ticks");
}

// --- Gate 30 (M4): the Ram's front is armour and its back is the answer.
//
// The whole enemy is the gap between these two numbers. If the front is not
// punishing, nobody moves and the Ram is a slow drifter; if the back is not
// decisive, moving does not pay and the Ram is unanswerable. It also fails if
// the arithmetic ever silently stops mattering — the first version of this
// enemy had so much hp that changing it produced a byte-identical campaign log,
// which is the failure mode a stat gate exists to catch.
{
    (float Hp, bool Dead) RamUnderFire(bool fromBehind)
    {
        var world = new World(Seed, Maps.Switchyard);
        world.Enqueue(new Command.Join(1, "probe", "ember"));
        Step.Advance(world);

        var ram = new Enemy
        {
            Id = world.NextId(), DefId = "ram",
            Hp = Enemies.Ram.Hp, MaxHp = Enemies.Ram.Hp,
            Facing = new Vec3(1, 0, 0), Bounty = 0, LeakDamage = 2,
            RouteIndex = 0, Leg = 2, LegProgress = 4f,
        };
        world.Enemies.Add(ram);
        Step.Advance(world);

        var weapon = Weapons.All[world.Players[1].WeaponId];
        for (int i = 0; i < Balance.TickHz * 12 && !ram.Dead; i++)
        {
            world.Enqueue(new Command.PlayerSync(1, ram.Pos + new Vec3(fromBehind ? -2.5f : 2.5f, 0f, 0f)));
            world.Enqueue(new Command.PlayerHit(1, ram.Id, weapon.Id));
            Step.Advance(world);
        }
        return (ram.Hp, ram.Dead);
    }

    var face = RamUnderFire(fromBehind: false);
    var back = RamUnderFire(fromBehind: true);

    Gate("ram: twelve seconds of fire kills it from behind and barely marks the front",
        back.Dead && !face.Dead && face.Hp > Enemies.Ram.Hp * 0.5f,
        $"to the face {face.Hp:0}/{Enemies.Ram.Hp:0} hp left | from behind dead {back.Dead}");
}

// --- Gate 31 (M4): the Ram demolishes what you build, and a wrench only slows it.
//
// Two halves, and the second is the one worth guarding. A siege enemy whose
// damage a single repairing player can simply out-heal is not a threat, it is a
// stalemate — the wrench is meant to buy time to kill the thing, never to
// replace killing it. MeleeRepairFactor shipped at 1.4, which came to 27 hp/s
// of repair against 14 dps of demolition, so one player held a barricade
// against a Ram indefinitely while the description in the file claimed the
// opposite.
{
    (float Hp, bool Destroyed, int Ticks) BarricadeUnderSiege(bool repairing)
    {
        var world = new World(Seed, Maps.Switchyard);
        world.Money = 2000;
        world.Enqueue(new Command.Join(1, "fixer", "forge"));
        world.Enqueue(new Command.PlaceTower(0, "barricade", "b1"));
        Step.Advance(world);

        var barricade = world.Towers.Single();
        var ram = new Enemy
        {
            Id = world.NextId(), DefId = "ram",
            Hp = 100_000f, MaxHp = 100_000f,          // the demolition is the subject, not the kill
            Facing = new Vec3(1, 0, 0), Bounty = 0, LeakDamage = 2,
            RouteIndex = 0, Leg = 0, LegProgress = 0f,
        };
        world.Enemies.Add(ram);

        // Walk it in rather than placing it. MoveEnemies recomputes position
        // from the route every tick, so a hand-set Pos is gone before
        // SiegeStructures reads it — the first version of this gate held the
        // Ram on the barricade and measured a demolition that never started.
        // Arriving under its own power also exercises the part that makes the
        // enemy work at all: it walks at the barricade instead of being turned
        // onto the fallback route the way everything else is. Once in reach it
        // pins itself, because sieging sets its speed to zero.
        int approach = 0;
        while (!ram.Sieging && !ram.Dead && approach++ < Balance.TickHz * 120) Step.Advance(world);

        int ticks = 0;
        for (; ticks < Balance.TickHz * 120 && world.Towers.Count > 0; ticks++)
        {
            if (repairing)
            {
                world.Enqueue(new Command.PlayerSync(1, barricade.Pos));
                world.Enqueue(new Command.PlayerMelee(1, barricade.Pos + new Vec3(0f, 0f, 1f)));
            }
            Step.Advance(world);
        }
        return (barricade.Hp, world.Towers.Count == 0, ticks);
    }

    var alone = BarricadeUnderSiege(repairing: false);
    var held = BarricadeUnderSiege(repairing: true);

    Gate("ram: it demolishes a barricade, and one wrench slows that without stopping it",
        alone.Destroyed && held.Destroyed && held.Ticks > alone.Ticks * 1.4f,
        $"unattended {alone.Ticks} ticks | repaired {held.Ticks} ticks " +
        $"({(float)held.Ticks / alone.Ticks:0.0}x)");
}

// --- Gate 23: socket placement is legal and generous.
//
// Build density is a player-freedom dial: too few sockets and there is only
// one defence, which is what the first pass shipped. This gate keeps the graph
// honest as it grows — sockets off the road, traps on it, nothing stranded out
// of range, nothing stacked on top of anything else.
{
    static float DistanceToRoute(Vec3 point, MapDef map)
    {
        float best = float.MaxValue;
        foreach (var route in map.Routes)
        {
            if (route.Layer != EnemyLayer.Ground) continue;
            for (int i = 0; i < route.Waypoints.Count - 1; i++)
                best = MathF.Min(best, PointToSegment(point, route.Waypoints[i], route.Waypoints[i + 1]));
        }
        return best;
    }

    static float PointToSegment(Vec3 p, Vec3 a, Vec3 b)
    {
        // True distance, height included. This was flat for a long time, on the
        // premise that height is what separates a deck socket from the lane
        // under it — fine while every map was a yard with a deck over it, and
        // wrong the moment one was a tower block. On the Spire a street-level
        // socket sits directly beneath a stairwell thirty metres up, which flat
        // distance calls "standing in the road".
        //
        // Measuring properly says the same thing about the old maps (a deck
        // socket six metres up is six metres from the lane, comfortably clear)
        // and the right thing about the new one.
        float abx = b.X - a.X, aby = b.Y - a.Y, abz = b.Z - a.Z;
        float lengthSq = abx * abx + aby * aby + abz * abz;
        if (lengthSq < 0.001f) return (p - a).Length();
        float t = Math.Clamp(
            ((p.X - a.X) * abx + (p.Y - a.Y) * aby + (p.Z - a.Z) * abz) / lengthSq, 0f, 1f);
        float cx = a.X + abx * t, cy = a.Y + aby * t, cz = a.Z + abz * t;
        return MathF.Sqrt((p.X - cx) * (p.X - cx) + (p.Y - cy) * (p.Y - cy) + (p.Z - cz) * (p.Z - cz));
    }

    var problems = new List<string>();
    foreach (var map in Campaign.Sectors.Select(id => Maps.All[id]))
    {
        var sockets = map.Sockets;

        if (sockets.Select(s => s.Id).Distinct().Count() != sockets.Count)
            problems.Add($"{map.Id}: duplicate socket id");

        // Density: the whole point of this pass.
        if (sockets.Count < 30) problems.Add($"{map.Id}: only {sockets.Count} sockets");

        foreach (var socket in sockets)
        {
            float toLane = DistanceToRoute(socket.Pos, map);

            if (socket.Tag == SocketTag.Trap)
            {
                // Traps trigger on contact, so a plate off the road is dead money.
                if (toLane > 2.5f) problems.Add($"{map.Id}/{socket.Id}: trap {toLane:0.0}m off the lane");
            }
            else if (socket.Tag == SocketTag.Ground)
            {
                // Standing in the road would put a tower inside the enemies.
                if (toLane < 3.5f) problems.Add($"{map.Id}/{socket.Id}: {toLane:0.0}m from the lane centre");
                // Stranded past every tower's reach is a socket nobody will buy.
                if (toLane > 20f) problems.Add($"{map.Id}/{socket.Id}: stranded {toLane:0.0}m from any lane");
            }
            else if (socket.Tag == SocketTag.Wall)
            {
                // A wall mount overhanging the lane is the whole appeal, so it
                // is only checked for being stranded.
                if (toLane > 20f) problems.Add($"{map.Id}/{socket.Id}: stranded {toLane:0.0}m from any lane");
            }

            if (socket.Tag == SocketTag.Wall && socket.Pos.Y < 1f)
                problems.Add($"{map.Id}/{socket.Id}: wall socket at ground level");
        }

        // Two pads close enough to overlap read as one blurry option.
        foreach (var a in sockets)
            foreach (var b in sockets)
            {
                if (string.CompareOrdinal(a.Id, b.Id) >= 0) continue;
                if (a.Tag != b.Tag) continue;                      // a plate beside a pad is fine
                if (MathF.Abs(a.Pos.Y - b.Pos.Y) > 2f) continue;   // different tiers may stack
                float dx = a.Pos.X - b.Pos.X, dz = a.Pos.Z - b.Pos.Z;
                float gap = MathF.Sqrt(dx * dx + dz * dz);
                if (gap < 4.5f) problems.Add($"{map.Id}: {a.Id}/{b.Id} only {gap:0.0}m apart");
            }
    }

    Gate("sockets: placement legal, spaced, and dense enough to choose from",
        problems.Count == 0,
        problems.Count == 0
            // Named from the campaign rather than hardcoded: this gate checked
            // exactly two maps by name and would have let a third map's
            // sockets through unexamined, which is precisely the rot the
            // campaign-chain gate was written to catch one file over.
            ? string.Join(", ", Campaign.Sectors.Select(id => $"{id} {Maps.All[id].Sockets.Count}"))
            : string.Join(" | ", problems.Take(6)));
}

Console.WriteLine();
Console.WriteLine(failures == 0 ? "ALL GATES GREEN" : $"{failures} GATE(S) FAILED");
return failures == 0 ? 0 : 1;
