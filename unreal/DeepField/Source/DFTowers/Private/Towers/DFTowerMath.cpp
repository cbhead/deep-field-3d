#include "Towers/DFTowerMath.h"

#include "Determinism/DFDetMath.h"

// Pinned like the wave plan: PathFactor and the range chain must give the host's answer on every
// machine (a Windows Game target defaults to /fp:fast; DFDetMath.h explains).
DF_DET_FP_PUSH

namespace DFTowerMath
{
	namespace Reasons
	{
		const FName None = NAME_None;
		const FName UnknownSocket(TEXT("unknownSocket"));
		const FName UnknownTower(TEXT("unknownTower"));
		const FName WrongSocketTag(TEXT("wrongSocketTag"));
		const FName TrapSocket(TEXT("trapSocket"));
		const FName Occupied(TEXT("occupied"));
		const FName WouldSeal(TEXT("wouldSeal"));
		const FName InsufficientFunds(TEXT("insufficientFunds"));
		const FName InsufficientScrap(TEXT("insufficientScrap"));
		const FName UnknownPath(TEXT("unknownPath"));
		const FName MaxLevel(TEXT("maxLevel"));
	}

	namespace
	{
		const FName DamagePath(TEXT("damage"));
		const FName RatePath(TEXT("rate"));
		const FName RampPath(TEXT("ramp"));
		const FName PeakPath(TEXT("peak"));
		// TowerMath.cs RangePathIds: Detector calls it "field", Filament "optics", everything else "range".
		const FName RangePaths[] = { FName(TEXT("range")), FName(TEXT("field")), FName(TEXT("optics")) };
	}

	// ---- paths -----------------------------------------------------------------------------------

	int32 PathIndex(const FDFTowerRow& Row, FName PathId)
	{
		for (int32 i = 0; i < Row.UpgradePaths.Num(); ++i)
		{
			if (Row.UpgradePaths[i].Id == PathId)
			{
				return i;
			}
		}
		return INDEX_NONE;
	}

	int32 PurchasesOn(TConstArrayView<int32> PathLevels, int32 Index)
	{
		return PathLevels.IsValidIndex(Index) ? PathLevels[Index] : 0;
	}

	float PathFactor(const FDFTowerRow& Row, TConstArrayView<int32> PathLevels, FName PathId)
	{
		DF_DET_FP_SCOPE
		const int32 Index = PathIndex(Row, PathId);
		if (Index == INDEX_NONE)
		{
			return 1.f;
		}
		return FDFDetMath::PowInt(Row.UpgradePaths[Index].PerLevelFactor, PurchasesOn(PathLevels, Index));
	}

	int32 MaxLevel(const FDFUpgradePathRow& Path)
	{
		return Path.LevelCosts.Num() + 1;
	}

	const FDFScrapBundle* RecipeFor(const FDFUpgradePathRow& Path, int32 Level)
	{
		return Path.BreakpointRecipes.Find(Level);
	}

	// ---- the numbers a tower fights with -----------------------------------------------------------

	TConstArrayView<FName> RangePathIds()
	{
		return MakeArrayView(RangePaths);
	}

	int32 RangePathIndex(const FDFTowerRow& Row)
	{
		// TowerMath.cs RangePathIndex: the first path (in path order) whose id is a range id.
		for (int32 i = 0; i < Row.UpgradePaths.Num(); ++i)
		{
			for (const FName& Id : RangePaths)
			{
				if (Row.UpgradePaths[i].Id == Id)
				{
					return i;
				}
			}
		}
		return INDEX_NONE;
	}

	float BaseRangeMeters(const FDFTowerRow& Row, TConstArrayView<int32> PathLevels)
	{
		DF_DET_FP_SCOPE
		// TowerMath.cs BaseRange: multiplied in RangePathIds order, as the sim does.
		float Range = Row.RangeMeters;
		for (const FName& Id : RangePaths)
		{
			Range *= PathFactor(Row, PathLevels, Id);
		}
		return Range;
	}

	float RangeMeters(const FDFTowerRow& Row, FName TowerId, TConstArrayView<int32> PathLevels, const FDFConditionRow* Condition)
	{
		DF_DET_FP_SCOPE
		float Range = BaseRangeMeters(Row, PathLevels);
		if (Condition && !Condition->RangeExemptTowerIds.Contains(TowerId))
		{
			Range *= Condition->TowerRangeFactor;
		}
		return Range;
	}

	float RangeAfterUpgradeMeters(const FDFTowerRow& Row, FName TowerId, TConstArrayView<int32> PathLevels, int32 Index, const FDFConditionRow* Condition)
	{
		if (!Row.UpgradePaths.IsValidIndex(Index))
		{
			return RangeMeters(Row, TowerId, PathLevels, Condition);
		}
		const int32 Bought = PurchasesOn(PathLevels, Index);
		if (Bought + 1 >= MaxLevel(Row.UpgradePaths[Index]))
		{
			return RangeMeters(Row, TowerId, PathLevels, Condition);
		}
		TArray<int32, TInlineAllocator<4>> Next;
		Next.SetNumZeroed(Row.UpgradePaths.Num());
		for (int32 i = 0; i < Next.Num(); ++i)
		{
			Next[i] = PurchasesOn(PathLevels, i);
		}
		++Next[Index];
		return RangeMeters(Row, TowerId, Next, Condition);
	}

	float EffectiveDamage(const FDFTowerRow& Row, TConstArrayView<int32> PathLevels)
	{
		DF_DET_FP_SCOPE
		return Row.Damage * PathFactor(Row, PathLevels, DamagePath);
	}

	float EffectiveRate(const FDFTowerRow& Row, TConstArrayView<int32> PathLevels, float BuffFactor)
	{
		DF_DET_FP_SCOPE
		return Row.ShotsPerSecond * PathFactor(Row, PathLevels, RatePath) * BuffFactor;
	}

	float BeamRamp(const FDFTowerRow& Row, TConstArrayView<int32> PathLevels, float HeldSeconds, float BeamRampPerSecond, float BeamRampCap)
	{
		DF_DET_FP_SCOPE
		const float Rate = BeamRampPerSecond * PathFactor(Row, PathLevels, RampPath);
		const float Cap = BeamRampCap * PathFactor(Row, PathLevels, PeakPath);
		return FMath::Min(Cap, 1.f + Rate * HeldSeconds);
	}

	// ---- building, upgrading, selling ---------------------------------------------------------------

	int32 BuildCost(const FDFTowerRow& Row, bool bBuilderIsForge, float ForgeBuildDiscount)
	{
		DF_DET_FP_SCOPE
		// Step.cs: `cost = (int)(cost * Balance.ForgeBuildDiscount)` — a float product truncated toward zero.
		return bBuilderIsForge ? static_cast<int32>(static_cast<float>(Row.Cost) * ForgeBuildDiscount) : Row.Cost;
	}

	FName CheckPlacement(const FDFTowerRow& Row, const FDFPlacementFacts& Facts, int32 Cost)
	{
		const bool bBarricade = Row.Kind == EDFTowerKind::Barricade;
		const bool bTagOk = bBarricade
			? Facts.SocketTag == EDFSocketTag::Barricade
			: (Facts.SocketTag == EDFSocketTag::Ground || Facts.SocketTag == EDFSocketTag::Wall);
		if (!bTagOk)
		{
			return Facts.SocketTag == EDFSocketTag::Trap ? Reasons::TrapSocket : Reasons::WrongSocketTag;
		}
		if (Facts.bSocketOccupied)
		{
			return Reasons::Occupied;
		}
		// "The map can be shaped and cannot be sealed" — checked before the money moves, so a refusal costs nothing.
		if (bBarricade && Facts.bWouldSeal)
		{
			return Reasons::WouldSeal;
		}
		if (Facts.Money < Cost)
		{
			return Reasons::InsufficientFunds;
		}
		return Reasons::None;
	}

	FDFUpgradeQuote QuoteUpgrade(const FDFTowerRow& Row, TConstArrayView<int32> PathLevels, int32 Index, int32 Money, const TMap<EDFScrapType, int32>& TeamScrap)
	{
		FDFUpgradeQuote Quote;
		if (!Row.UpgradePaths.IsValidIndex(Index))
		{
			Quote.Reason = Reasons::UnknownPath;
			return Quote;
		}
		const FDFUpgradePathRow& Path = Row.UpgradePaths[Index];
		Quote.CurrentLevel = PurchasesOn(PathLevels, Index) + 1;
		if (Quote.CurrentLevel >= MaxLevel(Path))
		{
			Quote.Reason = Reasons::MaxLevel;
			return Quote;
		}
		Quote.NextLevel = Quote.CurrentLevel + 1;
		Quote.MoneyCost = Path.LevelCosts[Quote.CurrentLevel - 1];
		Quote.Recipe = RecipeFor(Path, Quote.NextLevel);
		if (Money < Quote.MoneyCost)
		{
			Quote.Reason = Reasons::InsufficientFunds;
			return Quote;
		}
		if (Quote.Recipe)
		{
			for (const TPair<EDFScrapType, int32>& Part : Quote.Recipe->Amounts)
			{
				const int32* Have = TeamScrap.Find(Part.Key);
				if ((Have ? *Have : 0) < Part.Value)
				{
					Quote.Reason = Reasons::InsufficientScrap;
					return Quote;
				}
			}
		}
		Quote.Reason = Reasons::None;
		return Quote;
	}

	int32 SellRefund(int32 Spent, int32 SellRefundPercent)
	{
		return Spent * SellRefundPercent / 100;
	}

	// ---- targeting ----------------------------------------------------------------------------------

	int32 PickTarget(const FDFTowerRow& Row, const FVector& TowerPositionCm, float Range, TConstArrayView<FDFTargetCandidate> Candidates)
	{
		return PickTarget(Row, TowerPositionCm, Range, Candidates, [Candidates](int32 Index) { return Candidates[Index].bSightBlocked; });
	}

	int32 PickTarget(const FDFTowerRow& Row, const FVector& TowerPositionCm, float Range, TConstArrayView<FDFTargetCandidate> Candidates,
		TFunctionRef<bool(int32)> IsSightBlocked)
	{
		int32 Best = INDEX_NONE;
		float BestRemaining = TNumericLimits<float>::Max();
		for (int32 i = 0; i < Candidates.Num(); ++i)
		{
			const FDFTargetCandidate& Enemy = Candidates[i];
			if (Enemy.bDead || !Row.TargetLayers.Contains(Enemy.Layer) || Enemy.bBurrowed)
			{
				continue;
			}
			if (Enemy.bStealth && !Enemy.bDetected)
			{
				continue;   // towers need stealth revealed; heroes are unaffected
			}
			const float DistanceMeters = static_cast<float>(FVector::Dist(TowerPositionCm, Enemy.PositionCm)) / 100.f;
			if (DistanceMeters > Range || DistanceMeters < Row.MinRangeMeters)
			{
				continue;
			}
			// A candidate that cannot win is not worth a trace: the sim checks sight before comparing, but
			// skipping the trace for a loser changes nothing it returns.
			if (!(Enemy.RemainingToCore < BestRemaining))
			{
				continue;
			}
			if (IsSightBlocked(i))
			{
				continue;
			}
			// Strict '<' as the sim: a stranded walker (float max) never beats anything, and ties keep the first.
			BestRemaining = Enemy.RemainingToCore;
			Best = i;
		}
		return Best;
	}

	bool IsSightBlockedByBody(const FVector& FromCm, const FVector& TargetCm, const FVector& BlockerCm, float BlockRadiusCm)
	{
		const FVector ToTarget = TargetCm - FromCm;
		const FVector ToBlocker = BlockerCm - FromCm;
		const double TargetDist = ToTarget.Size();
		const double BlockerDist = ToBlocker.Size();
		if (TargetDist <= 0.0 || BlockerDist >= TargetDist)
		{
			return false;
		}
		const FVector Dir = ToTarget / TargetDist;
		const double Along = FVector::DotProduct(ToBlocker, Dir);
		if (Along <= 0.0)
		{
			return false;
		}
		const FVector Closest = FromCm + Dir * Along;
		return FVector::Dist(Closest, BlockerCm) < BlockRadiusCm;
	}

	int32 NextChainTarget(const FDFTowerRow& Row, int32 StruckIndex, TConstArrayView<FDFTargetCandidate> Candidates, const TSet<int32>& HitIds)
	{
		if (!Candidates.IsValidIndex(StruckIndex))
		{
			return INDEX_NONE;
		}
		const FVector From = Candidates[StruckIndex].PositionCm;
		int32 Next = INDEX_NONE;
		float BestDistance = Row.ChainRange;   // metres; strictly nearer than the chain range, as the sim
		for (int32 i = 0; i < Candidates.Num(); ++i)
		{
			const FDFTargetCandidate& Candidate = Candidates[i];
			if (Candidate.bDead || Candidate.bBurrowed || HitIds.Contains(Candidate.Id) || !Row.TargetLayers.Contains(Candidate.Layer))
			{
				continue;
			}
			const float DistanceMeters = static_cast<float>(FVector::Dist(From, Candidate.PositionCm)) / 100.f;
			if (DistanceMeters < BestDistance)
			{
				BestDistance = DistanceMeters;
				Next = i;
			}
		}
		return Next;
	}

	float AcquisitionDelaySeconds(const FDFConditionRow* Condition, bool bTargetMarked)
	{
		if (!Condition || Condition->AcquisitionDelaySeconds <= 0.f)
		{
			return 0.f;
		}
		if (Condition->bAcquisitionDelayExemptsMarked && bTargetMarked)
		{
			return 0.f;
		}
		return Condition->AcquisitionDelaySeconds;
	}

	bool AdvanceShot(FDFTowerShot& Shot, const FVector& AimPointCm, float DeltaSeconds, float HitRadiusMeters)
	{
		DF_DET_FP_SCOPE
		const float StepCm = Shot.SpeedMetersPerSecond * DeltaSeconds * 100.f;
		const float DistanceCm = static_cast<float>(FVector::Dist(Shot.PositionCm, AimPointCm));
		if (DistanceCm <= StepCm + HitRadiusMeters * 100.f)
		{
			return true;
		}
		Shot.PositionCm += (AimPointCm - Shot.PositionCm).GetSafeNormal() * StepCm;
		return false;
	}

	float SplashFactor(float DistanceMeters, float SplashRadiusMeters, float SplashFalloff)
	{
		DF_DET_FP_SCOPE
		if (SplashRadiusMeters <= 0.f || DistanceMeters > SplashRadiusMeters)
		{
			return 0.f;
		}
		return 1.f - (1.f - SplashFalloff) * (DistanceMeters / SplashRadiusMeters);
	}
}

DF_DET_FP_POP
