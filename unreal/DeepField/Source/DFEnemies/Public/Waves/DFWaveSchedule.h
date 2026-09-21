#pragma once

#include "CoreMinimal.h"
#include "Waves/DFWavePlan.h"

// A planned wave played back against world time. TickOffsets are sim ticks (balance dial tickHz,
// 30); the schedule converts elapsed seconds to whole ticks and releases every entry that has come
// due, in plan order. It is plain data over a pure plan, so a resumed match rebuilds it by
// re-planning the wave and skipping what was already released.
struct DFENEMIES_API FDFWaveSchedule
{
	void Reset(TArray<FDFSpawnEntry> InEntries, float InTickHz);

	/** Advance by world seconds and hand every entry now due to Emit, in plan order; returns how many
	 *  were handed over. A TickOffset of 0 is due on the first call, whatever its delta.
	 *
	 *  Emit returns whether to carry on. It is foreign code and may do anything — end the match,
	 *  destroy the actor that owns this schedule, reset it — so it gets the say over whether the rest
	 *  of this frame's entries are released. Returning false stops after the current one (which
	 *  counts as released and keeps its place in the cursor); elapsed time still advanced, so the
	 *  entries left behind are due immediately on the next Advance. */
	int32 Advance(float DeltaSeconds, TFunctionRef<bool(const FDFSpawnEntry&)> Emit);

	/** Drop the first Count entries without emitting them (resume). */
	void SkipReleased(int32 Count);

	bool IsExhausted() const { return Next >= Entries.Num(); }
	int32 NumReleased() const { return Next; }
	int32 NumRemaining() const { return Entries.Num() - Next; }
	int32 Num() const { return Entries.Num(); }

	/** Whole sim ticks since Reset. */
	int64 CurrentTick() const;

	/** World seconds until the next release; 0 when one is already due or nothing is left. */
	float SecondsUntilNext() const;

private:
	TArray<FDFSpawnEntry> Entries;
	int32 Next = 0;
	double ElapsedSeconds = 0.0;   // double: a 40-minute endless wave at 120 fps is 288 000 float adds
	float TickHz = 0.f;
};
