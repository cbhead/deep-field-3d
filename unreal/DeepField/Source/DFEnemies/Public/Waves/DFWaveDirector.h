#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Info.h"
#include "Messages/DFMessages.h"
#include "Waves/DFWavePlan.h"
#include "Waves/DFWaveSchedule.h"
#include "DFWaveDirector.generated.h"

DECLARE_MULTICAST_DELEGATE_OneParam(FDFOnSpawnRequested, const FDFSpawnEntry& /*Entry*/);
DECLARE_MULTICAST_DELEGATE_OneParam(FDFOnWaveEvent, int32 /*WaveIndex*/);

/**
 * Plans a wave and releases its bodies on time. Host only: the game mode spawns one, it is never
 * replicated, and clients learn about waves from DF.Message.* and the enemies themselves (§3.3).
 *
 * The director owns *what spawns when* and *when the wave is over*. It does not own the match
 * phase (intermission, victory, lives — DFMatch), it does not broadcast DF.Message.Wave* (the
 * phase owner does, with DescribeWave's payload), and it does not know what an enemy is: a
 * released entry goes out on OnSpawnRequested, and whoever spawns the body reports it gone with
 * NotifyEnemyRemoved. Bodies that do not come from the plan (a Cluster's children, a brood vent's
 * motes) are reported with NotifyEnemyAdded so the wave does not clear under them.
 */
UCLASS(NotBlueprintable, NotPlaceable)
class DFENEMIES_API ADFWaveDirector : public AInfo
{
	GENERATED_BODY()

public:
	ADFWaveDirector();

	/** Tables from the imported content for MapId. False, with the reason, if the content is not usable. */
	bool Configure(uint32 InSeed, FName MapId, FString& OutError);

	/** Tables built by the caller (tests, PCG variants). Validated the same way. */
	bool ConfigureWithTables(uint32 InSeed, FDFWavePlanTables InTables, FString& OutError);

	/** B§1.11 streams (elite injection, boss waves, PCG variants). Order of registration only orders ties. */
	void AddInjectionStream(TSharedRef<const IDFWaveInjectionStream> Stream);

	/** Plan WaveIndex for PlayerCount seated players and start releasing it. False if not configured or a wave is running. */
	bool BeginWave(int32 WaveIndex, int32 PlayerCount);

	/** Resume: as BeginWave, with the first AlreadyReleased entries of the plan treated as released
	 *  and AliveNow bodies already in the world (the save records both; the plan is recomputed). */
	bool ResumeWave(int32 WaveIndex, int32 PlayerCount, int32 AlreadyReleased, int32 AliveNow);

	/** A body entered the world outside the plan (split children, brood). */
	void NotifyEnemyAdded(int32 Count = 1);

	/** A body left the world: killed or leaked, it is all the same to the wave. */
	void NotifyEnemyRemoved(int32 Count = 1);

	/** Stop releasing and forget the wave without clearing it (defeat, host teardown). */
	void AbortWave();

	/** The payload for DF.Message.WaveStarted / WaveCleared / Intermission: lap, threat (the hp scale
	 *  the wave spawns with — the HUD's endless readout is this number, not a copy) and condition. */
	FDFMsg_Wave DescribeWave(int32 WaveIndex, int32 PlayerCount) const;

	bool IsConfigured() const { return bConfigured; }
	/** Whether a wave is running *here and now*. Asks IsLiveWave rather than the raw flag, because a
	 *  destroyed director must not tell a caller a wave is running: DFMatch and the mutables are the
	 *  callers, and they ask this to decide whether to act. The flag alone is stale on a pending-kill
	 *  actor whose EndPlay never ran. */
	bool IsWaveActive() const { return IsLiveWave(); }
	int32 GetWaveIndex() const { return ActiveWave; }
	int32 GetAliveCount() const { return Alive; }
	const FDFWaveSchedule& GetSchedule() const { return Schedule; }
	const FDFWavePlanTables& GetTables() const { return Tables; }

	/** One per released entry, in plan order. The listener spawns the body. */
	FDFOnSpawnRequested OnSpawnRequested;
	/** The plan is fully released; bodies may still be alive. */
	FDFOnWaveEvent OnWaveSpawnsExhausted;
	/** Fully released and nothing alive. Fires once per wave. */
	FDFOnWaveEvent OnWaveCleared;

	virtual void Tick(float DeltaSeconds) override;

	/** Teardown ends the wave. Every guard in this class already asks `bWaveActive`, so clearing it
	 *  here is what makes them all correct for a destroyed director at once — rather than an
	 *  IsValid term at each entry point, which is the same mistake as guarding the callouts that
	 *  happened to bite me. It matters because `NotifyEnemyRemoved` is a *public* entry point that
	 *  reaches `CheckCleared`: destroy the director with bodies still alive and the stragglers' late
	 *  reports would drive Alive to zero and broadcast a cleared wave out of a torn-down match. No
	 *  caller does that today; WS-19's brood vents and WS-24's mutables are being sent at this very
	 *  API, and a caller reaching into a garbage actor is not going to be obvious to them. */
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;

private:
	void CheckCleared();

	/** Whether a release loop is still working on the wave it started on, and on a live actor.
	 *
	 *  Every broadcast this class makes hands control to foreign code that may destroy the director,
	 *  abort the wave, or abort and immediately begin another. `bWaveActive` cannot tell the last of
	 *  those from "nothing happened" — it is true either way — and a loop that trusted it would keep
	 *  releasing the *replacement* wave's entries against the old wave's clock. So "still this wave"
	 *  is a token the code compares rather than a state it infers, and it is re-checked after every
	 *  broadcast rather than after the ones that have bitten us. */
	bool IsStillReleasing(uint64 Generation) const { return IsLiveWave() && WaveGeneration == Generation; }

	/** A wave is running *and* this director is still a live object. Both halves are load-bearing and
	 *  neither implies the other: `Destroy()` only marks an actor pending-kill, and `EndPlay` — which
	 *  clears `bWaveActive` below — is never called at all for an actor that never began play, which
	 *  is a real configuration (a director torn down before BeginPlay dispatches, and every
	 *  automation world without a game state). Relying on the hook alone left a wave clearing out of
	 *  a destroyed director; the predicate is the thing that has to be right. */
	bool IsLiveWave() const { return IsValid(this) && bWaveActive; }

	FDFWavePlanTables Tables;
	FDFWaveSchedule Schedule;
	TArray<TSharedRef<const IDFWaveInjectionStream>> InjectionStreams;
	uint32 Seed = 0;
	int32 ActiveWave = INDEX_NONE;
	int32 Alive = 0;
	bool bConfigured = false;
	bool bWaveActive = false;
	bool bExhaustedAnnounced = false;
	bool bReleasing = false;
	uint64 WaveGeneration = 0;   // bumped by every wave that begins; never reused
};
