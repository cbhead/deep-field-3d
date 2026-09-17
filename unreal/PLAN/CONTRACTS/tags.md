# C1 — GameplayTag registry

**Canonical:** `unreal/DeepField/Source/DFCore/Public/DFGameplayTags.h` / `.cpp` (native, `UE_DECLARE_GAMEPLAY_TAG_EXTERN` / `UE_DEFINE_GAMEPLAY_TAG`). Workstream-local tags: `unreal/DeepField/Config/Tags/DF_<ws>.ini` (auto-loaded from `Config/Tags/`), append-only within the owning file. Promoting a local tag to native is an RFC. **Owner:** WS-00 (native); each WS (its ini).

Every id string in the content tables maps to a tag with PascalCase of the id (`emberPistol` → `DF.Weapon.EmberPistol`). `DF.Content.TagCoverage` fails if any content id lacks its tag.

```
DF.Faction.{Forge,Ember,Tempest,Glacier,Specter}
DF.Ability.{Overdrive,IgnitionWave,ChainSurge,CryoField,RevealPulse}
DF.Ability.Passive.{BuildDiscount,BurnDuration,ReloadSpeed,ChilledBonus,WeakPoints}
DF.Ability.Combo.{Overcharge,Shatterburn,DeepFreeze,Exposed,ForgeFire}
DF.Status.Channel.{Movement,Thermal,Toxin,Defense,Vulnerability,Control,Tether,Detection}
DF.Status.{Chill,Burn,Poison,Shred,Mark,Shock,Freeze,Reveal,Magnetize,Rubble,Tar,Stagger}
DF.Reaction.{ThermalShock,FlashFreeze,Corrode,Implosion}
DF.Tower.{Lance,Nova,Singularity,Skywatch,Arc,Barricade,Detector,Filament,Overclock}
DF.Tower.Kind.{Bolt,Mortar,Aura,Flak,Tesla,Beam,Barricade,Support}
DF.Tower.Path.{Damage,Range,Rate,Field,Analysis,Ramp,Peak,Optics,Potency,Efficiency,Network}
DF.Trap.{Spike,Tar,Launcher,Acid,Snare}
DF.Enemy.{Drifter,Mote,Monolith,Skiff,Aegis,Warden,Mole,Cluster,Shade,Mender,Ram,Broodmother,Leaper,Carapace,Skater,Nullifier,BossFrame01}
DF.Enemy.Layer.{Ground,Air}
DF.Enemy.State.{Burrowed,Stealthed,Revealed,Shielded,Enraged,Sieging,Splitting,Phased,Airborne,Tethering,Knocked}
DF.Enemy.Elite.{Gilded,Juggernaut,Voltaic,Swift,Umbral}
DF.Boss.Phase.{Armoured,Exposed,Enraged}
DF.Weapon.{Sidearm,Rifle,Scattergun,EmberPistol,PoisonStream,CryoSprayer,Lmg,BurstDmr,FlakCannon,ChargeSniper,AutoScattergun}
DF.Weapon.Slot.{Barrel,Muzzle,Optic,Magazine,Stock,Underbarrel,Infusion}
DF.Ammo.{Standard,Ap,HollowPoint,Incendiary,Cryo,Toxin,Shock,Gravi}
DF.Melee.{Wrench,Blade,Maul,Spear,Gauntlets,Chainblade}
DF.Melee.Slot.{Edge,Grip,CoreInfusion,Counterweight,ChargeCell}
DF.Scrap.{Alloy,Flux,Plating,Gravium,Primecore}
DF.Condition.{Fog,Night,Heatwave,Coldsnap,Storm,Tremor}
DF.Socket.{Ground,Wall,Trap,Barricade}
DF.Mutable.{Floodgate,Crusher,Wall,Cache,Nest,Barrel,Container}
DF.Team.{Defenders,Invaders}
DF.Tier.{Standard,Hardened,Assault}
DF.Damage.Type.{Kinetic,Thermal,Toxin,Shock,Splash,Siege,Contact,Leak,Ram,Fall}
DF.Damage.Source.{Tower,Hero,Melee,Trap,Reaction,Enemy,Vehicle,Mutable}
DF.Player.State.{Downed,Bleeding,Seated,Climbing,Ziplining,Launched,Lifting,Reloading,Sprinting,Crouching,Aiming,Sliding,Mantling,Building,Teleporting,Carrying,Dragging,InNest}
DF.Input.{Move,Look,Jump,Sprint,Crouch,Fire,AltFire,Aim,Reload,Melee,Interact,Build,Upgrade,Sell,Ability,Wheel,Ping,Scoreboard,Pause}
DF.Event.{Fire,Reload.Start,Reload.End,Melee.Swing,Hit,Interact,Ping}
DF.SetByCaller.{Damage,Duration,Magnitude,Radius,Level,Impulse}
DF.Message.<EventsCsName>                      # one per FDFMsg_* (see messages.md)
DF.Audio.State.{Lobby,Intermission,Wave,Breach,Boss,Victory,Defeat}
GameplayCue.DF.Status.<id>.{Applied,Removed,Tick}
GameplayCue.DF.Reaction.<id>
GameplayCue.DF.Tower.<id>.{Fire,Impact,Beam,Upgrade,Place,Sell,Destroyed}
GameplayCue.DF.Weapon.<id>.{Fire,Impact,Reload,Charge}
GameplayCue.DF.Ability.<id>
GameplayCue.DF.Enemy.{Spawn,Death,Leak,Teleport,Burrow,Surface,Shield.Break,Shield.Regen,Heal,Split,Leap,Phase,Tether,Knockdown}
GameplayCue.DF.Player.{Downed,Revived,Respawned,Hit,Mantle,Slide,Land.Hard}
GameplayCue.DF.Match.{WaveStart,WaveClear,Breach,Victory,Defeat,BossArrive,BossPhase}
GameplayCue.DF.Mutable.{Floodgate,Crusher,WallBreak,CacheOpen,BarrelExplode}
```
