#include "Status/DFStatusResolver.h"

FDFStatusResolver::FDFStatusResolver()
{
	for (int32 I = 0; I < NumChannels; ++I)
	{
		Slots[I].Channel = static_cast<EDFStatusChannel>(I);
	}
}

float FDFStatusResolver::MagnitudeFor(const FDFStatusRow& Row, float MagnitudeOverride)
{
	return MagnitudeOverride >= 0.f ? MagnitudeOverride : Row.Magnitude();
}

float FDFStatusResolver::DurationFor(const FDFStatusRow& Row, float DurationFactor)
{
	return Row.MaxDurationSeconds * FMath::Max(DurationFactor, 0.f);
}

float FDFStatusResolver::CcFillScaleFor(const FDFStatusRow& Row)
{
	// Hard control fills at the row's rate (1); Tether at its own (0.5); nothing else touches the gauge.
	if (Row.bHardControl || Row.Channel == EDFStatusChannel::Tether)
	{
		return FMath::Max(Row.CcResistFill, 0.f);
	}
	return 0.f;
}

void FDFStatusResolver::Write(FDFStatusSlot& Target, FName StatusId, const FDFStatusRow& Row, float Magnitude, float Now, float Duration, int32 SourceId)
{
	Target.StatusId = StatusId;
	Target.Channel = Row.Channel;
	Target.Magnitude = Magnitude;
	Target.EndTime = Now + Duration;
	Target.SourceId = SourceId;
	Target.CcFillScale = CcFillScaleFor(Row);
	Target.bHardControl = Row.bHardControl;
}

FDFStatusApplyOutcome FDFStatusResolver::Apply(FName StatusId, const FDFStatusRow& Row, float Now, int32 SourceId, const FDFStatusTargetState& Target,
	float MagnitudeOverride, float DurationFactor)
{
	FDFStatusApplyOutcome Out;
	Out.StatusId = StatusId;
	Out.Channel = Row.Channel;
	Out.Magnitude = MagnitudeFor(Row, MagnitudeOverride);
	Out.Duration = DurationFor(Row, DurationFactor);

	// 1. Reactions first (Step.cs ApplyStatus): the active partner is consumed, the incoming
	//    status never lands, and the output goes straight into its slot without a re-scan.
	for (int32 I = 0; I < NumChannels; ++I)
	{
		FDFStatusSlot& Active = Slots[I];
		if (!Active.IsActive())
		{
			continue;
		}
		const FDFReactionEntry* Reaction = Reactions.Match(Active.StatusId, StatusId);
		if (!Reaction)
		{
			continue;
		}
		Out.Result = EDFStatusApplyResult::Reacted;
		Out.ReactionId = Reaction->Id;
		Out.BurstFraction = Reaction->Row.BurstFraction;
		Out.ConsumedStatusId = Active.StatusId;
		Out.ConsumedChannel = Active.Channel;
		Active.Clear();

		const FName Emit = Reaction->Row.EmitStatus;
		if (!Emit.IsNone())
		{
			const FDFStatusRow* EmitRow = FindStatusRow ? FindStatusRow(Emit) : nullptr;
			if (EmitRow)
			{
				if (EmitRow->bHardControl && CcResist >= 1.f)
				{
					Out.bEmitRejected = true;
				}
				else
				{
					FDFStatusSlot& EmitSlot = Slot(EmitRow->Channel);
					Out.EmitReplacedStatusId = EmitSlot.StatusId;
					Out.EmittedStatusId = Emit;
					Out.EmittedChannel = EmitRow->Channel;
					Out.EmittedMagnitude = EmitRow->Magnitude();
					Out.EmittedDuration = DurationFor(*EmitRow, DurationFactor);
					Write(EmitSlot, Emit, *EmitRow, Out.EmittedMagnitude, Now, Out.EmittedDuration, SourceId);
				}
			}
			else
			{
				Out.bEmitRejected = true;
			}
		}
		return Out;
	}

	// 2. Gates.
	if (Row.Channel == EDFStatusChannel::Thermal && Target.Shield > 0.f)
	{
		Out.Result = EDFStatusApplyResult::RejectedShield;
		return Out;
	}
	if (Row.bHardControl && CcResist >= 1.f)
	{
		Out.Result = EDFStatusApplyResult::RejectedCcResist;
		return Out;
	}
	if (Row.Channel == EDFStatusChannel::Tether && (Target.bTetherImmune || Target.Mass >= TetherImmuneMass))
	{
		Out.Result = EDFStatusApplyResult::RejectedImmune;
		return Out;
	}

	// 3. One status per channel: refresh, drop, or replace.
	FDFStatusSlot& Current = Slot(Row.Channel);
	if (Current.IsActive())
	{
		if (Current.StatusId == StatusId)
		{
			Current.EndTime = Now + Out.Duration;
			Current.SourceId = SourceId;
			Current.Magnitude = Out.Magnitude;
			Out.Result = EDFStatusApplyResult::Refreshed;
			return Out;
		}
		if (Current.Magnitude >= Out.Magnitude)
		{
			Out.Result = EDFStatusApplyResult::DroppedWeaker;
			return Out;
		}
		Out.ReplacedStatusId = Current.StatusId;
	}
	Write(Current, StatusId, Row, Out.Magnitude, Now, Out.Duration, SourceId);
	Out.Result = EDFStatusApplyResult::Applied;
	return Out;
}

int32 FDFStatusResolver::Tick(float Now, float DeltaSeconds, TArray<FDFStatusSlot>* OutExpired)
{
	int32 Expired = 0;
	for (int32 I = 0; I < NumChannels; ++I)
	{
		FDFStatusSlot& S = Slots[I];
		if (S.IsActive() && S.EndTime <= Now)
		{
			if (OutExpired)
			{
				OutExpired->Add(S);
			}
			S.Clear();
			++Expired;
		}
	}

	const float Scale = ActiveCcFillScale();
	if (Scale > 0.f)
	{
		CcResist = FMath::Min(1.f, CcResist + CcResistFillPerSecond * Scale * DeltaSeconds);
	}
	else
	{
		CcResist = FMath::Max(0.f, CcResist - CcResistDecayPerSecond * DeltaSeconds);
	}
	return Expired;
}

bool FDFStatusResolver::IsControlled() const
{
	for (const FDFStatusSlot& S : Slots)
	{
		if (S.IsActive() && S.bHardControl)
		{
			return true;
		}
	}
	return false;
}

float FDFStatusResolver::ActiveCcFillScale() const
{
	float Scale = 0.f;
	for (const FDFStatusSlot& S : Slots)
	{
		if (S.IsActive())
		{
			Scale = FMath::Max(Scale, S.CcFillScale);
		}
	}
	return Scale;
}

void FDFStatusResolver::ClearAll()
{
	for (FDFStatusSlot& S : Slots)
	{
		S.Clear();
	}
}
