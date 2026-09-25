#pragma once

#include "Content/DFContentRows.h"
#include "CoreMinimal.h"

/**
 * The rules of a tower as pure functions (WS-04). These are ports of the sim's `TowerMath.cs` and of the tower
 * half of `Step.cs` (`ApplyPlaceTower`, `ApplyUpgradeTower`, `ApplySellTower`, `FireTowers`,
 * `PickTarget`, `SightBlocked`, `AcquisitionDelay`, `EffectiveDamage/Rate`). The sim is the spec
 * (ADR-0005).
 *
 * Nothing here touches a world. The tower actors, the targeting component and the build subsystem
 * gather the facts (positions, sight traces, statuses, money) and ask these functions what the rules
 * say, so the host, the HUD's range ring and the tests all get one answer. `TowerMath.cs` exists
 * because "a ring drawn from the def's range is right on the frame the tower is built and wrong from
 * the first Range purchase onward"; that is why range lives here, and only here.
 *
 * Units: the rows are in metres, as the sim is. World positions are in centimetres, as Unreal is.
 * Every function taking an `FVector` says which it wants.
 *
 * Path levels count purchases, so a fresh tower is at 0 on every path and the level a player sees
 * is one more than that (design's L1, the chassis). This matches `Tower.PathLevels`.
 */
namespace DFTowerMath
{
	// ---- paths -----------------------------------------------------------------------------------

	/** Index of the path named PathId, or INDEX_NONE. */
	DFTOWERS_API int32 PathIndex(const FDFTowerRow& Row, FName PathId);

	/** Purchases made on path i (0 when PathLevels is shorter than the path list). */
	DFTOWERS_API int32 PurchasesOn(TConstArrayView<int32> PathLevels, int32 Index);

	/** The multiplier path PathId has earned: PerLevelFactor^purchases, computed with FDFDetMath::PowInt
	 *  (bit-identical to the sim's DetMath). 1 for an untouched path or a path the tower does not have. */
	DFTOWERS_API float PathFactor(const FDFTowerRow& Row, TConstArrayView<int32> PathLevels, FName PathId);

	/** The highest level a path reaches: level 1 plus one purchase per cost. Ten for every shipped path. */
	DFTOWERS_API int32 MaxLevel(const FDFUpgradePathRow& Path);

	/** The scrap recipe charged on ARRIVING at Level (4, 7, 10), or null. */
	DFTOWERS_API const FDFScrapBundle* RecipeFor(const FDFUpgradePathRow& Path, int32 Level);

	// ---- the numbers a tower fights with -----------------------------------------------------------

	/** The path ids that grow reach. Every tower has at most one; multiplying all three is a lookup, not a stack. */
	DFTOWERS_API TConstArrayView<FName> RangePathIds();

	/** Index of the path that grows this tower's range, or INDEX_NONE (a Barricade has no reach to grow). */
	DFTOWERS_API int32 RangePathIndex(const FDFTowerRow& Row);

	/** Reach before weather, in metres: the row's range grown by whichever range path it has. */
	DFTOWERS_API float BaseRangeMeters(const FDFTowerRow& Row, TConstArrayView<int32> PathLevels);

	/**
	 * Reach as the tower uses it this wave, in metres. Condition is the condition scheduled for this
	 * wave (null for none). Weather rides on top of the baseline and never replaces it; a tower whose
	 * id is in the condition's RangeExemptTowerIds keeps its full reach.
	 */
	DFTOWERS_API float RangeMeters(const FDFTowerRow& Row, FName TowerId, TConstArrayView<int32> PathLevels, const FDFConditionRow* Condition);

	/** What RangeMeters would be after one more purchase on PathIndex: the number a player is deciding
	 *  about with the upgrade panel open. Unchanged for a path that does not move range, or one at its cap. */
	DFTOWERS_API float RangeAfterUpgradeMeters(const FDFTowerRow& Row, FName TowerId, TConstArrayView<int32> PathLevels, int32 Index, const FDFConditionRow* Condition);

	/** Damage per shot (per second for a Beam, before the ramp): Damage x the "damage" path. */
	DFTOWERS_API float EffectiveDamage(const FDFTowerRow& Row, TConstArrayView<int32> PathLevels);

	/** Shots per second: ShotsPerSecond x the "rate" path x BuffFactor (Overdrive, Overclock; 1 when unbuffed). */
	DFTOWERS_API float EffectiveRate(const FDFTowerRow& Row, TConstArrayView<int32> PathLevels, float BuffFactor = 1.f);

	/** The Beam's damage multiplier after holding one target for HeldSeconds:
	 *  min(cap, 1 + rate x held), with rate = beamRampPerSecond x "ramp" and cap = beamRampCap x "peak".
	 *  Switching target resets HeldSeconds; that is the caller's (the sim resets it on switch). */
	DFTOWERS_API float BeamRamp(const FDFTowerRow& Row, TConstArrayView<int32> PathLevels, float HeldSeconds, float BeamRampPerSecond, float BeamRampCap);

	// ---- building, upgrading, selling ---------------------------------------------------------------

	/** The refusal vocabulary of DF.Message.BuildRejected / UpgradeRejected (Step.cs spells the same strings). */
	namespace Reasons
	{
		DFTOWERS_API extern const FName None;
		DFTOWERS_API extern const FName UnknownSocket;      // "unknownSocket"
		DFTOWERS_API extern const FName UnknownTower;       // "unknownTower"
		DFTOWERS_API extern const FName WrongSocketTag;     // "wrongSocketTag"
		DFTOWERS_API extern const FName TrapSocket;         // "trapSocket"
		DFTOWERS_API extern const FName Occupied;           // "occupied"
		DFTOWERS_API extern const FName WouldSeal;          // "wouldSeal"
		DFTOWERS_API extern const FName InsufficientFunds;  // "insufficientFunds"
		DFTOWERS_API extern const FName InsufficientScrap;  // "insufficientScrap"
		DFTOWERS_API extern const FName UnknownPath;        // "unknownPath"
		DFTOWERS_API extern const FName MaxLevel;           // "maxLevel"
	}

	/** What a placement costs this player: the row's cost, x forgeBuildDiscount for a Forge builder,
	 *  truncated to whole money as the sim's `(int)(cost * discount)` does. */
	DFTOWERS_API int32 BuildCost(const FDFTowerRow& Row, bool bBuilderIsForge, float ForgeBuildDiscount);

	/** The facts ApplyPlaceTower checks, gathered by the build subsystem. */
	struct FDFPlacementFacts
	{
		EDFSocketTag SocketTag = EDFSocketTag::Ground;
		bool bSocketOccupied = false;
		/** Barricades only: closing this socket's lane gate would seal a spawn from the core (UDFLaneGraphAsset::WouldSeal). */
		bool bWouldSeal = false;
		int32 Money = 0;
	};

	/**
	 * Step.cs ApplyPlaceTower's checks for a tower (traps have their own path), in the sim's order:
	 * socket tag (a Barricade needs a Barricade socket; anything else a Ground or Wall socket, with a
	 * Trap socket named as such), occupied, wouldSeal (Barricade only, before any money moves), funds.
	 * Returns Reasons::None when the build may go ahead at Cost.
	 */
	DFTOWERS_API FName CheckPlacement(const FDFTowerRow& Row, const FDFPlacementFacts& Facts, int32 Cost);

	/** An upgrade, priced: what it costs and whether it may be bought now. */
	struct FDFUpgradeQuote
	{
		FName Reason;                              // Reasons::None when it may be bought
		int32 CurrentLevel = 0;                    // the level the player sees (purchases + 1)
		int32 NextLevel = 0;
		int32 MoneyCost = 0;
		const FDFScrapBundle* Recipe = nullptr;    // scrap charged on arriving at NextLevel (4, 7, 10), else null
		bool IsAllowed() const { return Reason.IsNone(); }
	};

	/**
	 * Step.cs ApplyUpgradeTower's checks, in order: unknownPath, maxLevel, insufficientFunds,
	 * insufficientScrap (a breakpoint recipe against the team pool). Pure: the caller takes the money
	 * and the scrap and bumps PathLevels when the quote is allowed.
	 */
	DFTOWERS_API FDFUpgradeQuote QuoteUpgrade(const FDFTowerRow& Row, TConstArrayView<int32> PathLevels, int32 Index, int32 Money, const TMap<EDFScrapType, int32>& TeamScrap);

	/** What selling returns: everything spent on the tower (build + upgrades, money only) x sellRefundPercent / 100, integer division. */
	DFTOWERS_API int32 SellRefund(int32 Spent, int32 SellRefundPercent);

	// ---- targeting ----------------------------------------------------------------------------------

	/** What the targeting component knows about one enemy when choosing. */
	struct FDFTargetCandidate
	{
		int32 Id = INDEX_NONE;
		FVector PositionCm = FVector::ZeroVector;
		EDFEnemyLayer Layer = EDFEnemyLayer::Ground;
		bool bDead = false;
		bool bBurrowed = false;
		/** The def is stealthy (EnemyRow.bStealth) ... */
		bool bStealth = false;
		/** ... and is it revealed right now (the Detection channel is active)? */
		bool bDetected = false;
		/** The line from the tower to it is blocked: terrain, a registered sight blocker, or a
		 *  sight-blocking enemy in the way (IsSightBlockedByBody). Gathered by the caller's traces. */
		bool bSightBlocked = false;
		/** UDFLaneGraphAsset::RemainingToCore along its itinerary. An OPAQUE SORT KEY, not a distance:
		 *  a walker with nowhere to go reports TNumericLimits<float>::Max() so it sorts last, and a
		 *  sieging enemy reports a genuinely small value so it sorts first. Compare only (INT, 2026-09-21). */
		float RemainingToCore = 0.f;
	};

	/**
	 * Step.cs PickTarget: the index in Candidates of the enemy this tower shoots, or INDEX_NONE.
	 * Filters, in the sim's order: dead, not in the row's TargetLayers, burrowed, stealthy and not
	 * detected, farther than RangeMeters or nearer than the row's MinRangeMeters (3D distance from the
	 * tower's position), sight blocked. Of the rest, the lowest RemainingToCore wins; ties keep the
	 * earlier candidate, as the sim's strict `<` does. No special case for anything; the sim has none.
	 */
	DFTOWERS_API int32 PickTarget(const FDFTowerRow& Row, const FVector& TowerPositionCm, float RangeMeters, TConstArrayView<FDFTargetCandidate> Candidates);

	/** As PickTarget, but the sight test is asked lazily, only for a candidate that passed every cheaper
	 *  filter. Sight is the sim's last check and the expensive one (a world trace), so a tower that sees
	 *  forty enemies traces only the few in range. The flag version is this with `bSightBlocked` read back. */
	DFTOWERS_API int32 PickTarget(const FDFTowerRow& Row, const FVector& TowerPositionCm, float RangeMeters, TConstArrayView<FDFTargetCandidate> Candidates,
		TFunctionRef<bool(int32 /*CandidateIndex*/)> IsSightBlocked);

	/** Step.cs SightBlocked's geometry, for one sight-blocking body (a Monolith): it stands between the
	 *  tower and the target, nearer than the target, within BlockRadiusCm of the line of fire. */
	DFTOWERS_API bool IsSightBlockedByBody(const FVector& FromCm, const FVector& TargetCm, const FVector& BlockerCm, float BlockRadiusCm = 160.f);

	/**
	 * Step.cs FireTowers' Tesla hop: after striking StruckIndex, the nearest other candidate within
	 * ChainRange (metres) that is alive, not burrowed, on a target layer and not already hit. INDEX_NONE
	 * when the chain ends. Stealth and sight are NOT checked for a hop: the arc jumps, it does not aim.
	 */
	DFTOWERS_API int32 NextChainTarget(const FDFTowerRow& Row, int32 StruckIndex, TConstArrayView<FDFTargetCandidate> Candidates, const TSet<int32>& HitIds);

	/** Step.cs AcquisitionDelay: seconds before a tower settles on a NEW target this wave (night), 0 when
	 *  there is no condition or no delay, and 0 for a marked target (Vulnerability active) when the
	 *  condition exempts marked targets. */
	DFTOWERS_API float AcquisitionDelaySeconds(const FDFConditionRow* Condition, bool bTargetMarked);

	// ---- shots in flight (Step.cs StepTowerProjectiles) --------------------------------------------

	/** A Bolt / Mortar / Flak round: it homes on its target, as the sim's do. */
	struct FDFTowerShot
	{
		int32 TargetId = INDEX_NONE;
		FVector PositionCm = FVector::ZeroVector;
		float SpeedMetersPerSecond = 0.f;
		float Damage = 0.f;
		float SplashRadiusMeters = 0.f;
		float SplashFalloff = 1.f;
	};

	/** The sim aims 0.8 m above an enemy's position (`target.Pos + (0, 0.8, 0)`). */
	constexpr float ShotAimHeightCm = 80.f;
	/** The sim fires from 1.5 m above the tower (`tower.Pos + (0, 1.5, 0)`). */
	constexpr float ShotMuzzleHeightCm = 150.f;

	/**
	 * One step of a shot toward AimPointCm. True when it lands this step: within the step's travel plus
	 * the balance dial projectileHitRadius (0.4 m), exactly the sim's test, and then the position is not
	 * moved. Otherwise it advances SpeedMetersPerSecond x DeltaSeconds straight at the aim point. A shot
	 * whose target is gone is the caller's to drop (the sim kills it; it does not land anywhere).
	 */
	DFTOWERS_API bool AdvanceShot(FDFTowerShot& Shot, const FVector& AimPointCm, float DeltaSeconds, float HitRadiusMeters);

	/** Splash damage factor at DistanceMeters from the struck target: 1 - (1 - falloff) x (d / radius),
	 *  so 1 at the centre and SplashFalloff at the rim; 0 beyond the radius (not hit at all). */
	DFTOWERS_API float SplashFactor(float DistanceMeters, float SplashRadiusMeters, float SplashFalloff);
}
