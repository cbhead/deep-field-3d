using Godot;
using System.Collections.Generic;
using DeepField.Game.Ui;
using DeepField.Sim.Content;

namespace DeepField.Game;

/// <summary>Design's mesh-based effects, driven the way its brief asks
/// (docs/design/models/vfx.js, projectiles.js): every file is a static hero
/// frame the client scales, turns and fades over a short life, with named
/// sub-groups it can drive without lookups — <c>_spin</c> rotates about Y,
/// <c>_pulse</c> breathes, <c>_rise</c> lifts, <c>_fade</c> dissipates.
/// Muzzle flashes face −Z and scale from nothing; impacts scale up and fade;
/// tracers are a unit-length streak along −Z that is stretched to the hit.
///
/// Two lifetimes, because the delivered set is two kinds of thing:
///
/// * <b>Bursts</b> are one-shots with a life measured in fractions of a
///   second — a flash, an impact, a wave beat, a reaction. Fire and forget.
/// * <b>Held</b> effects last exactly as long as the fact they are drawing,
///   which nobody can know in advance: a burn lasts until the burn ends, a
///   revive column until the key is released. They are retained-mode — the
///   frame that still wants one asks for it again by the same key, and
///   anything that went unasked-for this frame is dropped. That inverts the
///   bookkeeping in the direction that cannot leak: a status the client never
///   hears the end of stops being drawn on the first frame the enemy's status
///   bits come back clear, rather than living forever on a missed event.
///
/// Everything here is cosmetic and client-side. A tracer is drawn from the
/// muzzle to wherever the shot went the instant the trigger is pulled; the
/// sim decides separately whether it hurt, and says so through
/// EnemyDamaged. Nothing waits on the server to look like a gun.</summary>
public partial class Vfx : Node3D
{
    private sealed class Burst
    {
        public Node3D Node = null!;
        public float Life, Age;
        public float ScaleFrom, ScaleTo;
        public Vector3 Stretch = Vector3.One;   // applied on top of the scale curve (tracers)
        public Node3D? Spin, Pulse, Rise, Fade;
        public bool ShrinkOut;                   // tracers thin to nothing instead of scaling out
    }

    /// <summary>A held effect, and the dials a caller re-sets each frame it
    /// keeps asking for one. The node is public because two callers need to
    /// move it themselves: the revive tether stretches to the reviver, and a
    /// status effect sits on an enemy whose own view is doing the walking.</summary>
    public sealed class Held
    {
        public Node3D Node = null!;
        internal Node3D? Spin, Pulse, Rise, Fade;
        internal float Age;
        internal bool Touched;

        /// <summary>Metres a second the <c>_rise</c> group climbs, and how
        /// long before it snaps back and climbs again. Negative for poison,
        /// whose beads drip: design authors them above the feet and says so.</summary>
        public float RiseSpeed = 0.7f;
        public float RiseLoopSeconds = 1.4f;

        /// <summary>Seconds to keep this after the last frame that asked for
        /// it. Zero for anything driven from per-frame world state, which is
        /// the safe default. A beam is driven from an *event* instead — the
        /// sim fires thirty times a second and the client draws sixty, so
        /// half the frames have no event and a zero-linger beam would strobe
        /// at 30 Hz.</summary>
        public float LingerSeconds;

        internal float SinceTouched;

        /// <summary>0..1 to drive <c>_pulse</c> as a gauge that grows with it;
        /// null to let it breathe. Design asks for the revive ring's *arc* to
        /// be the progress, which would mean rebuilding the torus every frame
        /// — the group is scaled instead, and a ring growing to full reads as
        /// the same fact at the speed a revive takes.</summary>
        public float? Progress;
    }

    private readonly List<Burst> _live = new();
    private readonly Dictionary<string, Held> _held = new();

    /// <summary>Effects on screen right now. The review probe's evidence that
    /// the calls it made actually put something in the world, as opposed to
    /// returning quietly because a file was missing.</summary>
    public int LiveCount => _live.Count + _held.Count;

    /// <summary>The keys of every held effect on screen. The status probe's
    /// evidence that a burn on an enemy is a burn on that enemy — the count
    /// alone cannot tell one held effect from another.</summary>
    public IEnumerable<string> HeldKeys => _held.Keys;

    /// <summary>Palette per ammo type, from design's tracer set: the round's
    /// hue is what tells a cryo streak from a standard one across the map.</summary>
    public static Color AmmoHue(string ammoId) => ammoId switch
    {
        "ap" => new Color("cfd7e4"),
        "hollowpoint" => new Color("e8862b"),
        "incendiary" => UiTheme.Status("burn"),
        "cryo" => UiTheme.Status("chill"),
        "shock" => UiTheme.Status("shock"),
        "toxin" => UiTheme.Status("poison"),
        _ => new Color("f4dca4"),
    };

    // ---------------------------------------------------------------------
    // Guns
    // ---------------------------------------------------------------------

    /// <summary>One shot from a hero weapon: flash at the muzzle, streak to
    /// the point the shot ended, a burst where it landed.</summary>
    public void GunShot(Vector3 muzzle, Vector3 hit, string ammoId, bool struckSomething)
    {
        var hue = AmmoHue(ammoId);
        var forward = hit - muzzle;
        if (forward.LengthSquared() < 1e-4f) return;

        Burst? flash = Spawn("vfx_muzzle_lance", muzzle, forward.Normalized(), life: 0.07f, from: 0.05f, to: 0.32f, tint: hue);
        _ = flash;

        string tracer = AssetLibrary.Has($"vfx_tracer_{ammoId}") ? $"vfx_tracer_{ammoId}" : "vfx_tracer_standard";
        // A little past the muzzle so the streak starts outside the flash.
        var start = muzzle + forward.Normalized() * 0.25f;
        if (Spawn(tracer, start, forward.Normalized(), life: 0.09f, from: 1f, to: 1f) is { } streak)
        {
            streak.Stretch = new Vector3(1f, 1f, Mathf.Max(0.1f, forward.Length() - 0.25f));
            streak.ShrinkOut = true;
        }

        if (struckSomething)
            Spawn("vfx_impact_lance", hit, -forward.Normalized(), life: 0.22f, from: 0.12f, to: 0.42f, tint: hue);
    }

    /// <summary>A teammate's shot, reconstructed from the damage it did: the
    /// command never crosses the wire, but the hit does, and a streak from
    /// their avatar to the target is what makes co-op fire legible.</summary>
    public void RemoteShot(Vector3 from, Vector3 hit)
    {
        var forward = hit - from;
        if (forward.LengthSquared() < 1e-4f) return;
        if (Spawn("vfx_tracer_standard", from, forward.Normalized(), life: 0.09f, from: 1f, to: 1f) is { } streak)
        {
            streak.Stretch = new Vector3(0.7f, 0.7f, forward.Length());
            streak.ShrinkOut = true;
        }
    }

    // ---------------------------------------------------------------------
    // Towers
    // ---------------------------------------------------------------------

    /// <summary>Design's per-tower flash at the rig's muzzle node, facing the
    /// way the barrel points. Aura towers have no muzzle and no flash.</summary>
    public void TowerFired(string towerDefId, Vector3 muzzle, Vector3 forward)
    {
        string asset = $"vfx_muzzle_{towerDefId}";
        if (!AssetLibrary.Has(asset)) return;
        Spawn(asset, muzzle, forward, life: 0.08f, from: 0.1f, to: 1f);
    }

    /// <summary>A tower that does not fire a round: the Filament's continuous
    /// beam and each leg of the Arc's chain.
    ///
    /// Neither of these tower kinds ever creates a projectile in the sim — a
    /// beam applies its damage where it stands and a tesla arc is instant — so
    /// the projectile view sync, which follows sim projectiles, has never had
    /// anything to follow for them. Both models were delivered and neither had
    /// ever been on screen. Design authors them as a unit-length streak along
    /// −Z with the note "scale Z to the hit distance", the same contract the
    /// tracers already use.
    ///
    /// Held rather than fired once, and with a linger: the Filament re-arms
    /// this every sim tick for as long as it holds a target, and a burst per
    /// tick would stack four deep at 60 fps while a zero-linger held effect
    /// would strobe on the frames between ticks.</summary>
    public void TowerBeam(string key, string towerDefId, Vector3 from, Vector3 to)
    {
        string asset = towerDefId switch
        {
            "arc" => "proj_arc_beam",
            "filament" => "proj_filament_beam",
            _ => "",
        };
        if (asset.Length == 0) return;

        var along = to - from;
        float length = along.Length();
        if (length < 0.05f) return;

        if (Hold(key, asset, from) is not { } beam) return;
        beam.LingerSeconds = 0.12f;         // three sim ticks: no gap, no stack
        beam.Node.Position = from;
        beam.Node.LookAt(to, Mathf.Abs(along.Normalized().Dot(Vector3.Up)) > 0.99f
            ? Vector3.Forward : Vector3.Up);
        // Unit length along −Z; only Z is stretched, or the beam fattens with
        // the distance it crosses.
        beam.Node.Scale = new Vector3(1f, 1f, length);
    }

    /// <summary>Where a tower's round stopped existing. The sim removes a
    /// projectile the tick it lands, so the view's last position is the hit.</summary>
    public void ProjectileLanded(string towerDefId, Vector3 at)
    {
        string asset = $"vfx_impact_{towerDefId}";
        if (!AssetLibrary.Has(asset)) return;
        // Nova's splash ring is authored flat on the ground; the rest burst
        // outward from the point.
        Spawn(asset, at, towerDefId == "nova" ? null : Vector3.Forward, life: 0.25f, from: 0.2f, to: 1f);
    }

    /// <summary>A structure going up on its pad: design's holo-cage of the
    /// chassis volume, clamps dropping onto it, the pad ring closing.</summary>
    public void TowerPlaced(Vector3 at) =>
        Spawn("vfx_tower_place", at, forward: null, life: 0.5f, from: 0.7f, to: 1.05f);

    /// <summary>Refund, in brass rather than damage colours — the effect has
    /// to say "money back", not "something just died here".</summary>
    public void TowerSold(Vector3 at) =>
        Spawn("vfx_tower_sell", at, forward: null, life: 0.6f, from: 1f, to: 1.1f);

    /// <summary>A path went up a level. Design draws three chevron tiers on
    /// the <c>_rise</c> group and says the count can be culled per tier, so
    /// the tenth level is visibly more than the second.</summary>
    public void TowerUpgraded(Vector3 at, int newLevel)
    {
        if (Spawn("vfx_tower_upgrade", at, forward: null, life: 0.7f, from: 0.9f, to: 1.05f) is not { } burst) return;
        int tiers = newLevel >= 7 ? 3 : newLevel >= 4 ? 2 : 1;
        CullChevrons(burst.Node, tiers);
    }

    /// <summary>Design's chevrons are named <c>up_chevron&lt;tier&gt;_&lt;k&gt;</c>;
    /// hide the tiers this level has not earned.</summary>
    private static void CullChevrons(Node root, int tiers)
    {
        if (root is Node3D n3)
        {
            string name = n3.Name.ToString();
            if (name.StartsWith("up_chevron") && name.Length > 10
                && int.TryParse(name[10].ToString(), out int tier) && tier >= tiers)
                n3.Visible = false;
        }
        foreach (var child in root.GetChildren()) CullChevrons(child, tiers);
    }

    /// <summary>The Detector's reveal sweep. Design authors it at the full
    /// 13 m radius and asks the client to scale 0 → 1 over the period, so a
    /// Detector with the field path bought sweeps wider as well as the same.</summary>
    public void DetectorPulse(Vector3 at, float radiusMeters, float periodSeconds)
    {
        const float AuthoredRadius = 13f;   // Towers.Detector.RangeMeters, as delivered
        float full = radiusMeters / AuthoredRadius;
        Spawn("vfx_detector_pulse", at + Vector3.Up * 0.05f, forward: null,
            life: periodSeconds, from: 0.05f * full, to: full);
    }

    // ---------------------------------------------------------------------
    // The wave, and the thing it is walking at
    // ---------------------------------------------------------------------

    /// <summary>The portal opening. Yawed rather than aimed: design's ground
    /// chevrons run along the model's **+X**, and they are the half of this
    /// effect that says which way the trouble is coming. Everything else in
    /// it — the iris flare, the siren cones, the shock ring — stands upright,
    /// so a LookAt down the lane would put the sirens on their side.</summary>
    public void WaveStart(Vector3 at, Vector3 downLane)
    {
        if (Spawn("vfx_wave_start", at, forward: null, life: 1.5f, from: 0.6f, to: 1.1f) is not { } burst)
            return;
        if (downLane.LengthSquared() < 1e-6f) return;
        burst.Node.Rotation = new Vector3(0f, Mathf.DegToRad(MapKit.YawAlongX(downLane)), 0f);
    }

    /// <summary>All clear, at the core. The one effect in the set that is
    /// allowed to be slow.</summary>
    public void WaveClear(Vector3 at) =>
        Spawn("vfx_wave_clear", at, forward: null, life: 2f, from: 0.5f, to: 1.15f);

    /// <summary>Something got through. The core already swaps to its struck
    /// model; this is the alarm around it.</summary>
    public void CoreBreach(Vector3 at) =>
        Spawn("vfx_core_breach", at, forward: null, life: 1f, from: 0.35f, to: 1.2f);

    // ---------------------------------------------------------------------
    // What happens to enemies
    // ---------------------------------------------------------------------

    /// <summary>A Warden's bubble bursting. Parented to the enemy so the
    /// shards go where the shield was, not where it was half a second ago.</summary>
    public void ShieldPopped(Node3D on) =>
        SpawnOn(on, "vfx_shield_pop", life: 0.4f, from: 0.6f, to: 1.15f);

    /// <summary>The bubble coming back. Held rather than fired once: regen is
    /// a lull, not an instant, and the seams are drawn climbing for as long as
    /// the shield is actually climbing.</summary>
    public void ShieldRegen(string key, Node3D on) => HoldOn(key, on, "vfx_shield_regen");

    /// <summary>The Mole going under or coming up — the same fountain of dirt
    /// either way, because from outside it is the same event.</summary>
    public void BurrowSpray(Vector3 at) =>
        Spawn("vfx_burrow_spray", at, forward: null, life: 0.5f, from: 0.5f, to: 1.1f);

    /// <summary>A Cluster bursting into its Motes. Fired on the death rather
    /// than on the spawns: the sac opening is the moment, and the Motes walk
    /// out of it under their own models.</summary>
    public void ClusterSplit(Vector3 at) =>
        Spawn("vfx_cluster_split", at, forward: null, life: 0.5f, from: 0.5f, to: 1.15f);

    /// <summary>The co-op payoff. Thermal Shock is a burst; Flash Freeze is
    /// the moment the column comes down, and the lock it leaves behind is
    /// drawn by the held control effect for as long as the freeze lasts.</summary>
    public void Reaction(string reactionId, Node3D on)
    {
        string asset = $"vfx_reaction_{reactionId.ToLowerInvariant()}";
        if (!AssetLibrary.Has(asset)) return;
        SpawnOn(on, asset, life: reactionId == "flashFreeze" ? 0.5f : 0.6f, from: 0.35f, to: 1.1f);
    }

    /// <summary>Design's status set, one model per channel, held on the enemy
    /// for as long as the sim says the channel is occupied.
    ///
    /// The channel is what the snapshot carries and the status id is what the
    /// event carries, and both halves are needed: the Control channel holds
    /// either a 0.25 s stagger or a 1.2 s hard lock and they are not the same
    /// picture, while nothing but the bits can say when either one ended.</summary>
    public void StatusHeld(string key, Node3D on, string statusId, float scale)
    {
        string asset = StatusAsset(statusId);
        if (asset.Length == 0) return;
        if (HoldOn(key, on, asset) is not { } held) return;
        held.Node.Scale = Vector3.One * scale;
        // Poison beads fall. Design authors them mid-body and says to drive
        // the group downward — a rising drip reads as steam.
        held.RiseSpeed = statusId == "poison" ? -0.5f : 0.7f;
    }

    /// <summary>Which of design's files draws a status. Freeze is deliberately
    /// the stun model rather than a freeze one: design ships no held freeze
    /// effect, ships a stun whose note is the dazed lock with a cc-resist
    /// gauge filling under it, and that is exactly what a frozen enemy is in
    /// this sim. The flash-freeze ice column is the reaction burst that starts
    /// it, and it is drawn once, on the reaction.</summary>
    public static string StatusAsset(string statusId) => statusId switch
    {
        "chill" => "vfx_status_chill",
        "burn" => "vfx_status_burn",
        "poison" => "vfx_status_poison",
        "shred" => "vfx_status_shred",
        "mark" => "vfx_status_mark",
        "shock" => "vfx_status_shock",
        "freeze" => "vfx_status_stun",
        "reveal" => "vfx_status_reveal",
        _ => "",
    };

    // ---------------------------------------------------------------------
    // What players do
    // ---------------------------------------------------------------------

    /// <summary>A faction ability, at the place it happens — which for three
    /// of the five is the aim point and not the hero.
    ///
    /// Overdrive is the exception that needs two positions: design ships the
    /// buff ring for the hero and a <c>overdrive_tower_crown</c> sub-group to
    /// put on each tower the surge reached, so the player can see what they
    /// bought rather than reading a number off the HUD.</summary>
    public void Ability(string abilityId, Vector3 heroPos, Vector3 aimPos)
    {
        switch (abilityId)
        {
            case "overdrive":
                if (Spawn("vfx_ability_overdrive", heroPos, forward: null, life: 1.6f, from: 0.25f, to: 1f) is { } od)
                    HideCrown(od.Node);
                break;
            case "ignitionWave":
                Spawn("vfx_ability_ignitionwave", aimPos, forward: null, life: 1.1f, from: 0.2f, to: 1f);
                break;
            case "chainSurge":
                Spawn("vfx_ability_chainsurge", aimPos, forward: null, life: 0.9f, from: 0.2f, to: 1f);
                break;
            case "revealPulse":
                // Design has not drawn this one; the Detector's sweep is the
                // same picture at map scale, which is what the ability is.
                Spawn(AssetLibrary.Has("vfx_ability_revealpulse") ? "vfx_ability_revealpulse" : "vfx_detector_pulse",
                    heroPos, forward: null, life: 1.4f, from: 0.1f, to: 4f);
                break;
            case "cryoField":
                // Nothing delivered draws a chill dome; ask for it by name so
                // it lands the day design ships it (docs/FORWARD-MANIFEST-vfx.md).
                Spawn("vfx_ability_cryofield", aimPos, forward: null, life: 1.1f, from: 0.2f, to: 1f);
                break;
        }
    }

    /// <summary>One tower's share of an Overdrive: design's crown ring, lifted
    /// out of the ability model and parked on the chassis for the buff.</summary>
    public void OverdriveCrown(Vector3 at, float seconds)
    {
        var scene = AssetLibrary.TryInstantiate("vfx_ability_overdrive");
        if (scene is null) return;
        var crown = FindNamed(scene, "overdrive_tower_crown");
        if (crown is null) { scene.QueueFree(); return; }

        crown.GetParent().RemoveChild(crown);
        scene.QueueFree();

        MapKit.NoShadow(crown);
        crown.Position = at;
        AddChild(crown);
        _live.Add(new Burst
        {
            Node = crown, Life = seconds, ScaleFrom = 0.4f, ScaleTo = 1f,
            Spin = crown, Pulse = FindSuffix(crown, "_pulse"), Rise = FindSuffix(crown, "_rise"),
        });
    }

    /// <summary>The hold-R channel over a downed hero. Held, with the progress
    /// ring driven from the sim's own revive clock rather than a guess — the
    /// arc is the thing the player is waiting on.</summary>
    public void ReviveBeam(string key, Vector3 at, float progress)
    {
        if (Hold(key, "vfx_revive_beam", at) is not { } held) return;
        held.Node.Position = at;
        held.Progress = Mathf.Clamp(progress, 0f, 1f);
    }

    // ---------------------------------------------------------------------
    // Going somewhere else
    // ---------------------------------------------------------------------

    /// <summary>A player arriving or leaving on the pad network. Teal, and the
    /// same column at both ends, because what the effect has to say is "that
    /// person is now over there" — two different effects would read as two
    /// different things happening.</summary>
    public void Teleport(Vector3 at) => Warp(at, UiTheme.Status("chill"));

    /// <summary>An enemy crossing a warp gate. The same shape in the colour
    /// everything hostile is drawn in: a player has to be able to tell, from
    /// the far side of a field, which of the two just happened.</summary>
    public void EnemyWarp(Vector3 at) => Warp(at, new Color("e9614c"));

    private void Warp(Vector3 at, Color hue)
    {
        // Design's teleport burst if it has landed; the flat impact ring is
        // the stand-in, which is authored lying on the ground and is the
        // closest thing in the delivered set to a column of light.
        string asset = AssetLibrary.Has("vfx_teleport_burst") ? "vfx_teleport_burst" : "vfx_impact_nova";
        Spawn(asset, at + Vector3.Up * 0.2f, forward: null, life: 0.45f, from: 0.3f, to: 1.6f, tint: hue);
    }

    // =====================================================================
    // Plumbing
    // =====================================================================

    /// <summary>Puts one of design's effects in the world.
    ///
    /// <paramref name="forward"/> is <c>null</c> for everything authored the
    /// way it stands — a ground ring, a light column, a portal beat. Those
    /// want **no rotation at all**, and the way to get one wrong is to pass
    /// <c>Vector3.Up</c> meaning "it points up": LookAt turns the model's −Z
    /// toward the target, so an up vector lays an upright effect on its back.
    /// That is what the wave-clear beat did on the core — the success rings
    /// rose sideways out of it. Only things that genuinely aim — muzzle
    /// flashes, tracers, beams — pass a direction.</summary>
    private Burst? Spawn(string asset, Vector3 at, Vector3? forward, float life, float from, float to, Color? tint = null)
    {
        var node = AssetLibrary.TryInstantiate(asset);
        if (node is null) return null;
        MapKit.NoShadow(node);
        node.Position = at;
        AddChild(node);
        if (forward is { } aim && aim.LengthSquared() > 1e-6f)
        {
            var up = Mathf.Abs(aim.Normalized().Dot(Vector3.Up)) > 0.99f ? Vector3.Forward : Vector3.Up;
            node.LookAt(at + aim, up);
        }
        node.Scale = Vector3.One * from;
        if (tint is { } hue) Tint(node, hue);

        var burst = new Burst
        {
            Node = node, Life = life, ScaleFrom = from, ScaleTo = to,
            Spin = FindSuffix(node, "_spin"), Pulse = FindSuffix(node, "_pulse"),
            Rise = FindSuffix(node, "_rise"), Fade = FindSuffix(node, "_fade"),
        };
        _live.Add(burst);
        return burst;
    }

    /// <summary>A burst parented to something that moves. Shield shards and
    /// reaction columns belong to the body they came off; drawn in world space
    /// they trail half a metre behind a walking enemy.</summary>
    private Burst? SpawnOn(Node3D parent, string asset, float life, float from, float to)
    {
        var node = AssetLibrary.TryInstantiate(asset);
        if (node is null) return null;
        MapKit.NoShadow(node);
        parent.AddChild(node);
        node.Position = Vector3.Zero;
        node.Scale = Vector3.One * from;

        var burst = new Burst
        {
            Node = node, Life = life, ScaleFrom = from, ScaleTo = to,
            Spin = FindSuffix(node, "_spin"), Pulse = FindSuffix(node, "_pulse"),
            Rise = FindSuffix(node, "_rise"), Fade = FindSuffix(node, "_fade"),
        };
        _live.Add(burst);
        return burst;
    }

    /// <summary>Ask for a held effect by key. Returns the same one every frame
    /// until a frame goes by without asking, and marks it wanted either way.
    /// Null means design has not shipped that file — callers treat a held
    /// effect as a flourish, never as the thing itself.</summary>
    public Held? Hold(string key, string asset, Vector3 at) => HoldInternal(key, asset, null, at);

    /// <summary>The same, parented to something that walks.</summary>
    public Held? HoldOn(string key, Node3D parent, string asset) => HoldInternal(key, asset, parent, Vector3.Zero);

    private Held? HoldInternal(string key, string asset, Node3D? parent, Vector3 at)
    {
        if (_held.TryGetValue(key, out var existing))
        {
            if (IsInstanceValid(existing.Node))
            {
                existing.Touched = true;
                existing.SinceTouched = 0f;
                return existing;
            }
            _held.Remove(key);
        }

        var node = AssetLibrary.TryInstantiate(asset);
        if (node is null) return null;
        MapKit.NoShadow(node);
        (parent ?? (Node3D)this).AddChild(node);
        node.Position = parent is null ? at : Vector3.Zero;

        var held = new Held
        {
            Node = node, Touched = true,
            Spin = FindSuffix(node, "_spin"), Pulse = FindSuffix(node, "_pulse"),
            Rise = FindSuffix(node, "_rise"), Fade = FindSuffix(node, "_fade"),
        };
        _held[key] = held;
        return held;
    }

    /// <summary>Drops every held effect nothing asked for this frame. Called
    /// once, at the end of the client's view sync — the frame that stopped
    /// wanting an effect is the frame it goes away.</summary>
    public void SweepHeld()
    {
        if (_held.Count == 0) return;
        List<string>? dead = null;
        foreach (var (key, held) in _held)
        {
            if (IsInstanceValid(held.Node)
                && (held.Touched || held.SinceTouched < held.LingerSeconds))
            {
                held.Touched = false;
                continue;
            }
            (dead ??= new List<string>()).Add(key);
            if (IsInstanceValid(held.Node)) held.Node.QueueFree();
        }
        if (dead is not null) foreach (string key in dead) _held.Remove(key);
    }

    /// <summary>Drops everything, held and burning. A match ending or a level
    /// rebuild takes its effects with it.</summary>
    public void Clear()
    {
        foreach (var burst in _live) if (IsInstanceValid(burst.Node)) burst.Node.QueueFree();
        _live.Clear();
        foreach (var held in _held.Values) if (IsInstanceValid(held.Node)) held.Node.QueueFree();
        _held.Clear();
    }

    private static Node3D? FindSuffix(Node root, string suffix)
    {
        if (root is Node3D n3 && n3.Name.ToString().EndsWith(suffix)) return n3;
        foreach (var child in root.GetChildren())
            if (FindSuffix(child, suffix) is { } found) return found;
        return null;
    }

    private static Node3D? FindNamed(Node root, string name)
    {
        if (root is Node3D n3 && n3.Name.ToString() == name) return n3;
        foreach (var child in root.GetChildren())
            if (FindNamed(child, name) is { } found) return found;
        return null;
    }

    private static void HideCrown(Node root)
    {
        if (FindNamed(root, "overdrive_tower_crown") is { } crown) crown.Visible = false;
    }

    /// <summary>Design's effect meshes carry their tower's energy hue; a gun's
    /// round carries its ammo's. Only the glowing parts take the tint — the
    /// chrome core and shards stay what they are.</summary>
    private static void Tint(Node node, Color hue)
    {
        if (node is MeshInstance3D mesh)
        {
            for (int i = 0; i < mesh.GetSurfaceOverrideMaterialCount(); i++)
            {
                if (mesh.GetActiveMaterial(i) is not StandardMaterial3D material || !material.EmissionEnabled) continue;
                var copy = (StandardMaterial3D)material.Duplicate();
                copy.AlbedoColor = hue;
                copy.Emission = hue;
                mesh.SetSurfaceOverrideMaterial(i, copy);
            }
        }
        foreach (var child in node.GetChildren()) Tint(child, hue);
    }

    public override void _Process(double delta)
    {
        for (int i = _live.Count - 1; i >= 0; i--)
        {
            var b = _live[i];
            b.Age += (float)delta;
            if (b.Age >= b.Life || !IsInstanceValid(b.Node))
            {
                if (IsInstanceValid(b.Node)) b.Node.QueueFree();
                _live.RemoveAt(i);
                continue;
            }
            float t = b.Age / b.Life;
            // Fast in, ease out: the flash is there at once and lingers a beat.
            float s = Mathf.Lerp(b.ScaleFrom, b.ScaleTo, 1f - (1f - t) * (1f - t));
            var scale = Vector3.One * s;
            if (b.ShrinkOut)
            {
                float thin = 1f - t;
                scale = new Vector3(thin, thin, 1f);
            }
            b.Node.Scale = scale * b.Stretch;
            if (b.Spin is not null) b.Spin.RotateY((float)delta * 6f);
            if (b.Pulse is not null) b.Pulse.Scale = Vector3.One * (1f + 0.25f * Mathf.Sin(t * Mathf.Pi * 2f));
            if (b.Rise is not null) b.Rise.Position += Vector3.Up * (float)delta * 0.8f;
            // Design's _fade groups are domes and residual shells — the part of
            // an effect that is meant to be gone before the rest. Scaled out
            // rather than faded out on purpose: alpha would mean duplicating a
            // material per instance, and a shell shrinking into its own burst
            // reads the same at the speed these things live at.
            if (b.Fade is not null) b.Fade.Scale = Vector3.One * Mathf.Max(0.001f, 1f - t);
        }

        foreach (var h in _held.Values)
        {
            if (!IsInstanceValid(h.Node)) continue;
            h.Age += (float)delta;
            if (h.Spin is not null) h.Spin.RotateY((float)delta * 2.5f);
            if (h.Pulse is not null)
                h.Pulse.Scale = Vector3.One * (h.Progress is { } p
                    ? Mathf.Max(0.02f, p)                                  // a gauge, growing
                    : 1f + 0.18f * Mathf.Sin(h.Age * Mathf.Pi * 1.6f));    // breathing
            if (h.Rise is not null)
            {
                // A held rise loops: flames lick up and start again. Driving it
                // the way a burst does would have the burn walk off the top of
                // the enemy inside two seconds.
                float phase = h.RiseLoopSeconds <= 0f ? 0f : Mathf.PosMod(h.Age, h.RiseLoopSeconds);
                h.Rise.Position = Vector3.Up * (phase * h.RiseSpeed);
            }
            if (h.Fade is not null)
                h.Fade.Scale = Vector3.One * (0.85f + 0.15f * Mathf.Sin(h.Age * Mathf.Pi));
            h.SinceTouched += (float)delta;
        }
    }

    /// <summary>Every effect name this class can ask for, so the content audit
    /// can request them all without a match that happens to burn a Warden.
    ///
    /// This exists because the usage measurement is honest and therefore
    /// blind in one direction: a solo run builds a level and walks a wave, and
    /// never sells a tower, never triggers a reaction, never gets anything
    /// frozen. Listing the names here is how effects that only a played match
    /// reaches stop reading as unused art (docs/ASSET-USAGE.md).</summary>
    public static IEnumerable<string> Catalogue()
    {
        foreach (var status in Statuses.All.Values)
        {
            string asset = StatusAsset(status.Id);
            if (asset.Length > 0) yield return asset;
        }
        foreach (var reaction in Reactions.All) yield return $"vfx_reaction_{reaction.Id.ToLowerInvariant()}";
        foreach (var faction in Factions.All.Values) yield return $"vfx_ability_{faction.AbilityId.ToLowerInvariant()}";
        yield return "vfx_tower_place";
        yield return "vfx_tower_sell";
        yield return "vfx_tower_upgrade";
        yield return "vfx_detector_pulse";
        yield return "vfx_wave_start";
        yield return "vfx_wave_clear";
        yield return "vfx_core_breach";
        yield return "vfx_shield_pop";
        yield return "vfx_shield_regen";
        yield return "vfx_burrow_spray";
        yield return "vfx_cluster_split";
        yield return "vfx_revive_beam";
        yield return "vfx_teleport_burst";
    }
}
