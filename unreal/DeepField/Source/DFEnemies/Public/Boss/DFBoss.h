#pragma once

#include "Content/DFContentRows.h"
#include "CoreMinimal.h"
#include "Damage/DFArmorProfile.h"
#include "GameplayTagContainer.h"
#include "DFBoss.generated.h"

struct FDFDamageInput;

// Boss Frame01 (B§2.4) as pure state and functions: which phase it is in, what a hit on it lands on,
// what its plates and vents are doing, and when its attack and its brood vent go off. No actor, no
// world, no GAS — the boss body gathers the facts and asks, as ADFEnemy asks FDFLaneWalker where it
// is going, so the rules can be tested without either.
//
// The sim has no boss, so there is nothing to hold this to for parity: B§2.4 is the whole spec. Where
// it is silent this file makes one reading, says so at the declaration, and ws-19-boss.md carries each
// as an open question. The readings are:
//   - hp: the body is RawHp x FDFWavePlan::HpScale (the wave curve and co-op in one number) and each
//     plate is PlateHp on the same scale. Phases are fractions of the BODY; plate hp is its own pool.
//   - a phase is entered when the body falls to its HpFrom or below, and never left: a heal does not
//     re-arm the boss, because plates that have dropped cannot come back.
//   - the hit that breaks a plate is spent on the plate; nothing carries through to the body.
//   - a plate is armour: no arc, no flat armour, no weak point, x PlateShredFactor under shred, and
//     hollow point never gets its unarmoured bonus against one.
//   - a hit that crosses two thresholds enters both, in order, even when it also kills.

/** Where a hit on the boss landed, as the physics asset names it (B§1.2: body / weak_n / plate_n). */
UENUM()
enum class EDFBossZone : uint8
{
	Body,    // the hull
	Plate,   // plate_n
	Vent,    // weak_n, the vent under plate_n
};

/** Which pool a hit comes out of, and the weak-point factor to compute it with. */
struct FDFBossHitTarget
{
	/** INDEX_NONE: the body takes it. Otherwise the plate that does. */
	int32 PlateIndex = INDEX_NONE;
	/** For FDFDamageInput::WeakPointFactor: the phase's factor for an open vent, 1 for anything else. */
	float WeakPointFactor = 1.f;

	bool IsPlate() const { return PlateIndex != INDEX_NONE; }
};

/** What a call did, in the order it happened. The caller turns these into messages, cues and spawns. */
UENUM()
enum class EDFBossEventKind : uint8
{
	PhaseEntered,       // Phase = the index entered. DF.Message.BossPhase; +10 XP each (B§1.7)
	PlatesDropped,      // Count = plates still on when a phase without plates began; they all fall now
	PlateRemoved,       // Index = the plate damage broke. DF.Message.PlateRemoved
	AttackTelegraphed,  // Detail = attack id; it lands TelegraphSeconds later unless the phase changes first
	AttackCancelled,    // Detail = attack id; the phase changed while it was telegraphed (stop the warning)
	AttackLanded,       // Detail = attack id; the caller resolves who is inside Radius
	BroodVented,        // Count bodies of Detail (enemy id) to spawn now. Each MUST be reported to
	                    // ADFWaveDirector::NotifyEnemyAdded, or the wave clears with them alive (WS-05)
	Died,
};

USTRUCT()
struct DFENEMIES_API FDFBossEvent
{
	GENERATED_BODY()

	UPROPERTY() EDFBossEventKind Kind = EDFBossEventKind::PhaseEntered;
	UPROPERTY() int32 Phase = INDEX_NONE;     // the phase it happened in (for PhaseEntered, the one entered)
	UPROPERTY() int32 Index = INDEX_NONE;     // PlateRemoved: the plate
	UPROPERTY() int32 Count = 0;              // PlatesDropped, BroodVented
	UPROPERTY() FName Detail;                 // attack id, enemy id
	/** How far into the Advance that produced it, in seconds (0 for anything damage produced). A long frame
	 *  runs every due event in order; this says when each one was due. */
	UPROPERTY() float SecondsIntoStep = 0.f;
};

/** The boss-wide numbers from B§2.4's header line: its enemy row's hp, and the plates. */
struct FDFBossBody
{
	float RawHp = 0.f;             // FDFEnemyRow::Hp, before the wave curve
	int32 PlateCount = 0;
	float PlateHp = 0.f;           // each, before the wave curve
	float PlateShredFactor = 1.f;  // what shred does to a plate (B§2.4 "shred x2 on plates")
};

/** Everything the boss reads, as plain data: built from content, or by hand in a test. */
struct DFENEMIES_API FDFBossTables
{
	TArray<FName> PhaseIds;              // boss.json row names, in phase order; DF.Boss.Phase.<Id>
	TArray<FDFBossPhaseRow> Phases;      // the same order: HpFrom descending
	FDFBossBody Body;

	/** Structural bounds, not balance numbers: they keep a typo from sizing an array. */
	static constexpr int32 MaxPhases = 8;
	static constexpr int32 MaxPlates = 32;

	/**
	 * Phases: 1..MaxPhases, one id each (set, unique); the first starts at 1, the last ends at 0, each ends
	 * where the next starts and runs downward; arc in [0, 360], factors and speed positive, armour and
	 * siege non-negative. An attack (IntervalSeconds > 0) has an id and a telegraph that fits in its
	 * interval. A vent (Count > 0) has an enemy id, an interval, and a MaxAlive of at least one vent's
	 * worth. Plates, once off, stay off: no phase holds plates after one that does not. Body: hp positive;
	 * plates 0..MaxPlates with positive hp and shred factor. NaN fails every rule.
	 */
	bool Validate(FString& OutError) const;
};

/** Where the fight is. Plain and small: this is what replicates to the boss bar and what a save writes. */
USTRUCT()
struct DFENEMIES_API FDFBossState
{
	GENERATED_BODY()

	UPROPERTY() float Hp = 0.f;
	UPROPERTY() float MaxHp = 0.f;
	UPROPERTY() TArray<float> PlateHp;        // one per plate; <= 0 = off
	UPROPERTY() float PlateMaxHp = 0.f;
	UPROPERTY() int32 PhaseIndex = INDEX_NONE; // INDEX_NONE until Begin
	/** Seconds since the phase began or its attack last landed. */
	UPROPERTY() double AttackClock = 0.0;
	UPROPERTY() bool bTelegraphing = false;
	/** Seconds since the phase began or its vent last vented. */
	UPROPERTY() double VentClock = 0.0;
	UPROPERTY() bool bDead = false;

	bool IsActive() const { return PhaseIndex != INDEX_NONE && !bDead; }
	int32 PlatesHeld() const;
	/** Body hp over max hp (the boss bar); 0 before Begin. */
	float HpFrac() const;
};

struct DFENEMIES_API FDFBoss
{
	/** Full hp, every plate on, first phase, clocks at zero. HpScale is FDFWavePlan::HpScale for the wave the
	 *  boss walks in; the plates ride the same curve. Tables must have passed Validate. Emits nothing: the
	 *  spawner announces the arrival (DF.Message.BossArrived) with the phase this leaves it in. */
	static void Begin(FDFBossState& State, const FDFBossTables& Tables, float HpScale);

	/** The phase row in force, or null before Begin. */
	static const FDFBossPhaseRow* CurrentPhase(const FDFBossState& State, const FDFBossTables& Tables);

	/** DF.Boss.Phase.<Id> for a phase index (the boss bar, the message); invalid when out of range. */
	static FGameplayTag PhaseTag(const FDFBossTables& Tables, int32 PhaseIndex);

	/**
	 * Which pool a hit on Zone takes, for plate/vent Index. A plate that is on takes a hit on itself or on
	 * the vent under it (the vent is covered). Once it is off, either reaches the body through the open
	 * vent at the phase's WeakPointFactor. A body hit is the body at 1. An Index outside the plates is the
	 * body at 1: a mis-authored zone earns no bonus.
	 */
	static FDFBossHitTarget ResolveHit(const FDFBossState& State, const FDFBossTables& Tables, EDFBossZone Zone, int32 Index);

	/**
	 * Fills the target half of a damage input for a hit on Target, so FDFDamageMath::Compute prices it in
	 * the sim's order. The body takes the phase's arc (no rear factor: the boss has none), the phase's
	 * flat armour as its row armour, and the target's weak-point factor. It does NOT write FlatArmor: that
	 * is the attribute, which the body sets from FlatArmor() on every PhaseEntered and shred then lowers.
	 * A plate takes none of it — no arc, no flat armour, factor 1 — but counts as armoured. The source half
	 * (BaseDamage, factors, HitDot, DamageTakenFactor, bTargetShredded) stays the caller's.
	 */
	static void ProfileHit(const FDFBossState& State, const FDFBossTables& Tables, const FDFBossHitTarget& Target, FDFDamageInput& InOut);

	/** The arc of a body hit this phase, for IDFArmorProfileSource. Forward is the caller's to set. */
	static FDFArmorProfile ArmorProfile(const FDFBossState& State, const FDFBossTables& Tables);

	/** The body's flat armour this phase: the FlatArmor attribute's base value. */
	static float FlatArmor(const FDFBossState& State, const FDFBossTables& Tables);

	/** The phase's multiplier on the row speed (B§2.4 P3: x1.4). 1 before Begin. */
	static float SpeedFactor(const FDFBossState& State, const FDFBossTables& Tables);

	/**
	 * Takes Amount (FDFDamageMath's result) from Target's pool and returns what the pool lost. A plate
	 * takes x PlateShredFactor when bTargetShredded and breaks at 0 (PlateRemoved); what it could not
	 * absorb is lost. The body enters every phase whose HpFrom it has fallen to, in order — dropping the
	 * plates when a phase without them begins, cancelling a telegraph the old phase had started, and
	 * restarting both clocks — then Died at 0, after cancelling any telegraph still up. A plate already
	 * off, a dead boss, or Amount <= 0 is 0.
	 * Resolve and apply one hit at a time: a target resolved before an earlier hit changed the phase is stale.
	 */
	static float ApplyDamage(FDFBossState& State, const FDFBossTables& Tables, const FDFBossHitTarget& Target, float Amount,
		bool bTargetShredded, TArray<FDFBossEvent>& OutEvents);

	/** Restores body hp up to max and returns what was restored. The phase does not go back. */
	static float Heal(FDFBossState& State, float Amount);

	/**
	 * Time passes. The phase's attack telegraphs at IntervalSeconds - TelegraphSeconds and lands at
	 * IntervalSeconds, counted from the phase's start and then from each landing, whether or not anyone
	 * is in reach (the rhythm is the thing players learn). The vent vents every IntervalSeconds from the
	 * phase's start, Count bodies at most, never taking this boss's live brood past MaxAlive. BroodAlive is
	 * how many it has vented that are alive now; bodies vented earlier in the same call count against the
	 * cap. A vent at the cap still resets its clock and says nothing. A long step runs every due event in
	 * time order, so one 30 s step and three hundred 0.1 s steps produce the same events at the same times.
	 */
	static void Advance(FDFBossState& State, const FDFBossTables& Tables, float DeltaSeconds, int32 BroodAlive, TArray<FDFBossEvent>& OutEvents);
};

/** B§2.4 / WS-27's gate: under a mid-band defence the boss dies between 55 % and 85 % of its walk. */
struct DFENEMIES_API FDFBossKillWindow
{
	float MinFraction = 0.55f;
	float MaxFraction = 0.85f;

	/** How much of the route it had walked, clamped to [0, 1]; a leak is 1. A route of no length reads as 1,
	 *  so a broken measurement fails the gate instead of passing it. */
	static float WalkFraction(float WalkedMeters, float RouteMeters);

	/** Inside the window, both ends inclusive. */
	bool Contains(float Fraction) const { return Fraction >= MinFraction && Fraction <= MaxFraction; }
};
