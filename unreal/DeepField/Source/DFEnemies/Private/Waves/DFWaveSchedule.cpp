#include "Waves/DFWaveSchedule.h"

void FDFWaveSchedule::Reset(TArray<FDFSpawnEntry> InEntries, float InTickHz)
{
	Entries = MoveTemp(InEntries);
	Next = 0;
	ElapsedSeconds = 0.0;
	TickHz = InTickHz;
	ensureMsgf(TickHz > 0.f || Entries.Num() == 0, TEXT("FDFWaveSchedule: %d entries and no tick rate — nothing would ever come due"), Entries.Num());
}

int64 FDFWaveSchedule::CurrentTick() const
{
	// The epsilon absorbs accumulated rounding so that N frames of exactly 1/tickHz are N ticks.
	return static_cast<int64>(FMath::FloorToDouble(ElapsedSeconds * static_cast<double>(TickHz) + 1e-6));
}

int32 FDFWaveSchedule::Advance(float DeltaSeconds, TFunctionRef<bool(const FDFSpawnEntry&)> Emit)
{
	ElapsedSeconds += FMath::Max(0.f, DeltaSeconds);
	const int64 Tick = CurrentTick();
	int32 Released = 0;
	while (Next < Entries.Num() && static_cast<int64>(Entries[Next].TickOffset) <= Tick)
	{
		// Advance the cursor first: Emit may look at the schedule (is this the last one?). A copy,
		// because Emit is foreign code and a reference into Entries would not survive a Reset.
		const FDFSpawnEntry Entry = Entries[Next++];
		const bool bContinue = Emit(Entry);
		++Released;
		if (!bContinue)
		{
			break;
		}
	}
	return Released;
}

void FDFWaveSchedule::SkipReleased(int32 Count)
{
	Next = FMath::Clamp(Next + Count, 0, Entries.Num());
}

float FDFWaveSchedule::SecondsUntilNext() const
{
	if (IsExhausted() || TickHz <= 0.f)
	{
		return 0.f;
	}
	const double Due = static_cast<double>(Entries[Next].TickOffset) / static_cast<double>(TickHz);
	return static_cast<float>(FMath::Max(0.0, Due - ElapsedSeconds));
}
