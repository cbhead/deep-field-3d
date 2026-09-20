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
	bool IsWaveActive() const { return bWaveActive; }
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

private:
	void CheckCleared();

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
};
