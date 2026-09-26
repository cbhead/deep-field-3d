#include "Profile/DFProfileSave.h"

#include "DFGameplayTags.h"

bool FDFMatchRecord::CapXp()
{
	bool bChanged = false;
	for (TPair<FGameplayTag, int32>& Pair : XpBySource)
	{
		const int32 Capped = FMath::Clamp(Pair.Value, 0, MaxXpPerSource);
		bChanged |= Capped != Pair.Value;
		Pair.Value = Capped;
	}
	return bChanged;
}

void UDFProfileSave::ApplyMatchRecord(const FDFMatchRecord& InRecord)
{
	// The cap is applied here, not trusted from the record: the host computed it, but the
	// profile is the last line (B§5.6) and a tampered host still cannot hand out more.
	FDFMatchRecord Record = InRecord;
	Record.CapXp();

	for (const TPair<FGameplayTag, int32>& Pair : Record.XpBySource)
	{
		if (Pair.Key.IsValid())
		{
			FactionXp.FindOrAdd(Pair.Key) += Pair.Value;
		}
	}

	if (!Record.MapId.IsNone())
	{
		const FName Key = BestWaveKey(Record.MapId, Record.bEndless);
		int32& Best = BestWave.FindOrAdd(Key);
		Best = FMath::Max(Best, Record.BestWave);

		// A sector is cleared by a campaign victory; endless never clears anything.
		if (Record.bVictory && !Record.bEndless)
		{
			ClearedSectors.AddUnique(Record.MapId);
			if (Record.Tier.IsValid())
			{
				FGameplayTag& Highest = HighestTier.FindOrAdd(Record.MapId);
				if (TierRank(Record.Tier) > TierRank(Highest))
				{
					Highest = Record.Tier;
				}
			}
		}
	}

	RecentMatches.Insert(Record, 0);
	if (RecentMatches.Num() > MaxRecentMatches)
	{
		RecentMatches.SetNum(MaxRecentMatches);
	}
}

FName UDFProfileSave::BestWaveKey(FName MapId, bool bEndless)
{
	return bEndless ? FName(*(MapId.ToString() + TEXT(":endless"))) : MapId;
}

int32 UDFProfileSave::TierRank(const FGameplayTag& Tier)
{
	if (Tier == DFTags::Tier_Assault) { return 3; }
	if (Tier == DFTags::Tier_Hardened) { return 2; }
	if (Tier == DFTags::Tier_Standard) { return 1; }
	return 0;
}

bool UDFProfileSave::IsSameAs(const UDFProfileSave& O) const
{
	if (Version != O.Version || Name != O.Name || PreferredFaction != O.PreferredFaction || LastJoinCode != O.LastJoinCode)
	{
		return false;
	}
	if (bShowDamageNumbers != O.bShowDamageNumbers || MouseSensitivity != O.MouseSensitivity || FieldOfView != O.FieldOfView
		|| HudScale != O.HudScale || bScreenShake != O.bScreenShake || bReduceFlashes != O.bReduceFlashes
		|| bTeammateOutlines != O.bTeammateOutlines || MasterVolume != O.MasterVolume || MusicVolume != O.MusicVolume
		|| SfxVolume != O.SfxVolume || VoiceVolume != O.VoiceVolume || ScalabilityTier != O.ScalabilityTier
		|| TsrPercent != O.TsrPercent || bFlashlightAuto != O.bFlashlightAuto)
	{
		return false;
	}
	if (!FactionXp.OrderIndependentCompareEqual(O.FactionXp) || ClearedSectors != O.ClearedSectors
		|| !BestWave.OrderIndependentCompareEqual(O.BestWave) || !HighestTier.OrderIndependentCompareEqual(O.HighestTier)
		|| !Blueprints.OrderIndependentCompareEqual(O.Blueprints) || RecentMatches.Num() != O.RecentMatches.Num())
	{
		return false;
	}
	for (int32 i = 0; i < RecentMatches.Num(); ++i)
	{
		const FDFMatchRecord& A = RecentMatches[i];
		const FDFMatchRecord& B = O.RecentMatches[i];
		if (A.MapId != B.MapId || A.Tier != B.Tier || A.bEndless != B.bEndless || A.BestWave != B.BestWave || A.bVictory != B.bVictory
			|| !A.XpBySource.OrderIndependentCompareEqual(B.XpBySource) || A.PlayedAt != B.PlayedAt || A.HostBuild != B.HostBuild || A.HostId != B.HostId)
		{
			return false;
		}
	}
	return true;
}
