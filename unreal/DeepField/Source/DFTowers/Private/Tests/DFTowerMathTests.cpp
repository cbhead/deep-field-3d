#include "Content/DFContentSubsystem.h"
#include "Misc/AutomationTest.h"
#include "Testing/DFTestUtils.h"
#include "Towers/DFTowerMath.h"

#if WITH_DEV_AUTOMATION_TESTS

// DF.Unit.Tower.* — DFTowerMath against the sim (TowerMath.cs, Step.cs tower half). Expected numbers
// are the sim's float32 arithmetic, reproduced step for step (DetMath.PowInt by squaring, each product
// rounded to single precision). The last test runs the same functions on the committed content tables.

namespace DFTowerMathTest
{
	constexpr EAutomationTestFlags Flags = EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter;
	constexpr float Tol = 1e-4f;

	FDFUpgradePathRow Path(const TCHAR* Id, float PerLevel)
	{
		FDFUpgradePathRow P;
		P.Id = Id;
		P.PerLevelFactor = PerLevel;
		P.LevelCosts = { 40, 54, 73, 98, 132, 178, 240, 324, 438 };   // Towers.cs StdCosts
		FDFScrapBundle Four;
		Four.Amounts.Add(EDFScrapType::Alloy, 8);
		Four.Amounts.Add(EDFScrapType::Plating, 2);
		FDFScrapBundle Seven;
		Seven.Amounts.Add(EDFScrapType::Plating, 6);
		Seven.Amounts.Add(EDFScrapType::Flux, 3);
		FDFScrapBundle Ten;
		Ten.Amounts.Add(EDFScrapType::Gravium, 4);
		Ten.Amounts.Add(EDFScrapType::Plating, 6);
		P.BreakpointRecipes.Add(4, Four);
		P.BreakpointRecipes.Add(7, Seven);
		P.BreakpointRecipes.Add(10, Ten);
		return P;
	}

	/** Towers.cs Lance: Bolt, 75, 12 m, 8 dmg, 1.6/s, ground only, damage/range/rate paths. */
	FDFTowerRow Lance()
	{
		FDFTowerRow R;
		R.Kind = EDFTowerKind::Bolt;
		R.Cost = 75;
		R.RangeMeters = 12.f;
		R.Damage = 8.f;
		R.ShotsPerSecond = 1.6f;
		R.StructureHp = 120.f;
		R.TargetLayers = { EDFEnemyLayer::Ground };
		R.UpgradePaths = { Path(TEXT("damage"), 1.10f), Path(TEXT("range"), 1.12f), Path(TEXT("rate"), 1.10f) };
		return R;
	}

	FDFConditionRow Fog()
	{
		FDFConditionRow C;
		C.TowerRangeFactor = 0.7f;
		C.RangeExemptTowerIds = { TEXT("detector"), TEXT("skywatch") };
		return C;
	}

	FDFConditionRow Night()
	{
		FDFConditionRow C;
		C.AcquisitionDelaySeconds = 0.2f;
		C.bAcquisitionDelayExemptsMarked = true;
		return C;
	}

	DFTowerMath::FDFTargetCandidate Enemy(int32 Id, float XMeters, float Remaining)
	{
		DFTowerMath::FDFTargetCandidate C;
		C.Id = Id;
		C.PositionCm = FVector(XMeters * 100.f, 0.f, 0.f);
		C.RemainingToCore = Remaining;
		return C;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDFTowerPathFactorsTest, "DF.Unit.Tower.PathFactorsMatchSim", DFTowerMathTest::Flags)
bool FDFTowerPathFactorsTest::RunTest(const FString& Parameters)
{
	using namespace DFTowerMathTest;
	const FDFTowerRow Row = Lance();
	const TArray<int32> Fresh = { 0, 0, 0 };
	const TArray<int32> Bought = { 4, 3, 2 };   // damage x4, range x3, rate x2

	TestEqual(TEXT("an untouched path is 1"), DFTowerMath::PathFactor(Row, Fresh, TEXT("damage")), 1.f, Tol);
	TestEqual(TEXT("a path the tower lacks is 1"), DFTowerMath::PathFactor(Row, Bought, TEXT("ramp")), 1.f, Tol);
	TestEqual(TEXT("1.12^3 by squaring"), DFTowerMath::PathFactor(Row, Bought, TEXT("range")), 1.4049279689788818f, Tol);
	TestEqual(TEXT("a short levels array reads as no purchases"), DFTowerMath::PathFactor(Row, TArray<int32>{ 4 }, TEXT("rate")), 1.f, Tol);

	TestEqual(TEXT("damage: 8 x 1.1^4"), DFTowerMath::EffectiveDamage(Row, Bought), 11.712800979614258f, Tol);
	TestEqual(TEXT("rate: 1.6 x 1.1^2 x a 1.5 buff"), DFTowerMath::EffectiveRate(Row, Bought, 1.5f), 2.9040002822875977f, Tol);
	TestEqual(TEXT("rate unbuffed and fresh is the row's"), DFTowerMath::EffectiveRate(Row, Fresh), 1.6f, Tol);

	TestEqual(TEXT("ten levels on every shipped path"), DFTowerMath::MaxLevel(Row.UpgradePaths[0]), 10);
	TestNotNull(TEXT("a recipe at 4"), DFTowerMath::RecipeFor(Row.UpgradePaths[0], 4));
	TestNull(TEXT("none at 5"), DFTowerMath::RecipeFor(Row.UpgradePaths[0], 5));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDFTowerRangeTest, "DF.Unit.Tower.RangeMatchesSim", DFTowerMathTest::Flags)
bool FDFTowerRangeTest::RunTest(const FString& Parameters)
{
	using namespace DFTowerMathTest;
	const FDFTowerRow Row = Lance();
	const FDFConditionRow FogRow = Fog();
	const TArray<int32> Fresh = { 0, 0, 0 };
	const TArray<int32> Range3 = { 0, 3, 0 };

	TestEqual(TEXT("the range path is index 1"), DFTowerMath::RangePathIndex(Row), 1);
	TestEqual(TEXT("fresh, clear weather: the row's 12 m"), DFTowerMath::RangeMeters(Row, TEXT("lance"), Fresh, nullptr), 12.f, Tol);
	TestEqual(TEXT("three range purchases: 12 x 1.12^3"), DFTowerMath::RangeMeters(Row, TEXT("lance"), Range3, nullptr), 16.859134674072266f, Tol);
	TestEqual(TEXT("fog shrinks it: x0.7 on top of the grown range"), DFTowerMath::RangeMeters(Row, TEXT("lance"), Range3, &FogRow), 11.80139446258545f, Tol);
	TestEqual(TEXT("fog on a fresh lance: 8.4 m"), DFTowerMath::RangeMeters(Row, TEXT("lance"), Fresh, &FogRow), 8.399999618530273f, Tol);
	TestEqual(TEXT("a fog-exempt tower keeps its reach"), DFTowerMath::RangeMeters(Row, TEXT("skywatch"), Fresh, &FogRow), 12.f, Tol);

	TestEqual(TEXT("the upgrade panel's preview: one more range level"),
		DFTowerMath::RangeAfterUpgradeMeters(Row, TEXT("lance"), TArray<int32>{ 0, 8, 0 }, 1, nullptr), 33.27694320678711f, Tol);
	TestEqual(TEXT("a path that does not move range previews the same range"),
		DFTowerMath::RangeAfterUpgradeMeters(Row, TEXT("lance"), Range3, 0, nullptr), 16.859134674072266f, Tol);
	const TArray<int32> Maxed = { 0, 9, 0 };
	TestEqual(TEXT("a maxed range path previews the same range"),
		DFTowerMath::RangeAfterUpgradeMeters(Row, TEXT("lance"), Maxed, 1, nullptr), DFTowerMath::RangeMeters(Row, TEXT("lance"), Maxed, nullptr), Tol);

	// Filament grows reach on a path it calls "optics"; Detector on "field".
	FDFTowerRow Filament = Row;
	Filament.UpgradePaths = { Path(TEXT("ramp"), 1.12f), Path(TEXT("peak"), 1.15f), Path(TEXT("optics"), 1.10f) };
	TestEqual(TEXT("optics is Filament's range path"), DFTowerMath::RangePathIndex(Filament), 2);
	TestEqual(TEXT("two optics purchases: 12 x 1.1^2"), DFTowerMath::RangeMeters(Filament, TEXT("filament"), TArray<int32>{ 0, 0, 2 }, nullptr), 14.520000457763672f, Tol);

	FDFTowerRow Barricade;
	Barricade.Kind = EDFTowerKind::Barricade;
	TestEqual(TEXT("a barricade has no range path"), DFTowerMath::RangePathIndex(Barricade), (int32)INDEX_NONE);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDFTowerBeamRampTest, "DF.Unit.Tower.BeamRamp", DFTowerMathTest::Flags)
bool FDFTowerBeamRampTest::RunTest(const FString& Parameters)
{
	using namespace DFTowerMathTest;
	FDFTowerRow Filament = Lance();
	Filament.Kind = EDFTowerKind::Beam;
	Filament.UpgradePaths = { Path(TEXT("ramp"), 1.12f), Path(TEXT("peak"), 1.15f), Path(TEXT("optics"), 1.10f) };
	// Balance dials beamRampPerSecond 0.6, beamRampCap 3.
	TestEqual(TEXT("the moment it locks on: x1"), DFTowerMath::BeamRamp(Filament, TArray<int32>{ 0, 0, 0 }, 0.f, 0.6f, 3.f), 1.f, Tol);
	TestEqual(TEXT("held 2 s: 1 + 0.6 x 2"), DFTowerMath::BeamRamp(Filament, TArray<int32>{ 0, 0, 0 }, 2.f, 0.6f, 3.f), 2.2f, Tol);
	TestEqual(TEXT("held long: the cap"), DFTowerMath::BeamRamp(Filament, TArray<int32>{ 0, 0, 0 }, 60.f, 0.6f, 3.f), 3.f, Tol);
	TestEqual(TEXT("two ramp purchases: 1 + 0.6 x 1.12^2 x 2"), DFTowerMath::BeamRamp(Filament, TArray<int32>{ 2, 0, 0 }, 2.f, 0.6f, 3.f), 2.505280017852783f, Tol);
	TestEqual(TEXT("peak purchases raise the cap: 3 x 1.15"), DFTowerMath::BeamRamp(Filament, TArray<int32>{ 0, 1, 0 }, 60.f, 0.6f, 3.f), 3.45f, Tol);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDFTowerBuildRulesTest, "DF.Unit.Tower.BuildRulesMatchSim", DFTowerMathTest::Flags)
bool FDFTowerBuildRulesTest::RunTest(const FString& Parameters)
{
	using namespace DFTowerMathTest;
	using namespace DFTowerMath;
	const FDFTowerRow Row = Lance();
	FDFTowerRow Barricade;
	Barricade.Kind = EDFTowerKind::Barricade;
	Barricade.Cost = 60;

	TestEqual(TEXT("a non-Forge builder pays the row"), BuildCost(Row, false, 0.9f), 75);
	TestEqual(TEXT("Forge pays (int)(75 x 0.9) = 67, truncated as the sim casts"), BuildCost(Row, true, 0.9f), 67);

	FDFPlacementFacts Facts;
	Facts.Money = 250;
	Facts.SocketTag = EDFSocketTag::Ground;
	TestEqual(TEXT("a tower on a free ground socket with money: allowed"), CheckPlacement(Row, Facts, 75), Reasons::None);
	Facts.SocketTag = EDFSocketTag::Wall;
	TestEqual(TEXT("a wall socket takes a tower too"), CheckPlacement(Row, Facts, 75), Reasons::None);
	Facts.SocketTag = EDFSocketTag::Trap;
	TestEqual(TEXT("a trap socket is named as such"), CheckPlacement(Row, Facts, 75), Reasons::TrapSocket);
	Facts.SocketTag = EDFSocketTag::Barricade;
	TestEqual(TEXT("a tower on a barricade socket: wrong tag"), CheckPlacement(Row, Facts, 75), Reasons::WrongSocketTag);
	TestEqual(TEXT("a barricade on its socket: allowed"), CheckPlacement(Barricade, Facts, 60), Reasons::None);
	Facts.bWouldSeal = true;
	TestEqual(TEXT("a barricade that would seal a spawn: refused"), CheckPlacement(Barricade, Facts, 60), Reasons::WouldSeal);
	Facts.Money = 0;
	TestEqual(TEXT("sealing is refused before money is even looked at"), CheckPlacement(Barricade, Facts, 60), Reasons::WouldSeal);
	Facts.SocketTag = EDFSocketTag::Ground;
	Facts.bWouldSeal = true;
	Facts.Money = 250;
	TestEqual(TEXT("wouldSeal is a barricade rule only"), CheckPlacement(Row, Facts, 75), Reasons::None);
	Facts.bSocketOccupied = true;
	TestEqual(TEXT("occupied"), CheckPlacement(Row, Facts, 75), Reasons::Occupied);
	Facts.bSocketOccupied = false;
	Facts.Money = 74;
	TestEqual(TEXT("one short"), CheckPlacement(Row, Facts, 75), Reasons::InsufficientFunds);

	TestEqual(TEXT("sell: 70% of everything spent, integer division"), SellRefund(75 + 40, 70), 80);
	TestEqual(TEXT("sell a fresh 75: 52"), SellRefund(75, 70), 52);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDFTowerUpgradeQuoteTest, "DF.Unit.Tower.UpgradeQuoteMatchesSim", DFTowerMathTest::Flags)
bool FDFTowerUpgradeQuoteTest::RunTest(const FString& Parameters)
{
	using namespace DFTowerMathTest;
	using namespace DFTowerMath;
	const FDFTowerRow Row = Lance();
	TMap<EDFScrapType, int32> NoScrap;

	FDFUpgradeQuote Q = QuoteUpgrade(Row, TArray<int32>{ 0, 0, 0 }, 0, 1000, NoScrap);
	TestTrue(TEXT("a fresh tower may buy level 2"), Q.IsAllowed());
	TestEqual(TEXT("it sees level 1"), Q.CurrentLevel, 1);
	TestEqual(TEXT("and is buying level 2"), Q.NextLevel, 2);
	TestEqual(TEXT("for the first cost, 40"), Q.MoneyCost, 40);
	TestNull(TEXT("no scrap below a breakpoint"), Q.Recipe);

	Q = QuoteUpgrade(Row, TArray<int32>{ 2, 0, 0 }, 0, 1000, NoScrap);
	TestEqual(TEXT("level 3 -> 4 costs the third price, 73"), Q.MoneyCost, 73);
	TestNotNull(TEXT("and level 4 is a breakpoint"), Q.Recipe);
	TestEqual(TEXT("without the scrap: refused"), Q.Reason, Reasons::InsufficientScrap);

	TMap<EDFScrapType, int32> Pool;
	Pool.Add(EDFScrapType::Alloy, 8);
	Pool.Add(EDFScrapType::Plating, 1);
	TestEqual(TEXT("one plating short: still refused"), QuoteUpgrade(Row, TArray<int32>{ 2, 0, 0 }, 0, 1000, Pool).Reason, Reasons::InsufficientScrap);
	Pool.Add(EDFScrapType::Plating, 2);
	TestTrue(TEXT("with Alloy 8 + Plating 2: allowed"), QuoteUpgrade(Row, TArray<int32>{ 2, 0, 0 }, 0, 1000, Pool).IsAllowed());
	TestEqual(TEXT("money is checked before scrap"), QuoteUpgrade(Row, TArray<int32>{ 2, 0, 0 }, 0, 72, NoScrap).Reason, Reasons::InsufficientFunds);

	TestEqual(TEXT("level 10 is the top"), QuoteUpgrade(Row, TArray<int32>{ 9, 0, 0 }, 0, 100000, NoScrap).Reason, Reasons::MaxLevel);
	TestEqual(TEXT("level 9 -> 10 costs the last price, 438"), QuoteUpgrade(Row, TArray<int32>{ 8, 0, 0 }, 0, 100000, NoScrap).MoneyCost, 438);
	TestEqual(TEXT("an unknown path"), QuoteUpgrade(Row, TArray<int32>{ 0, 0, 0 }, 3, 1000, NoScrap).Reason, Reasons::UnknownPath);
	TestEqual(TEXT("a negative path index"), QuoteUpgrade(Row, TArray<int32>{ 0, 0, 0 }, -1, 1000, NoScrap).Reason, Reasons::UnknownPath);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDFTowerPickTargetTest, "DF.Unit.Tower.PickTargetMatchesSim", DFTowerMathTest::Flags)
bool FDFTowerPickTargetTest::RunTest(const FString& Parameters)
{
	using namespace DFTowerMathTest;
	using namespace DFTowerMath;
	const FDFTowerRow Row = Lance();
	const FVector Tower = FVector::ZeroVector;

	TArray<FDFTargetCandidate> C = { Enemy(1, 5.f, 40.f), Enemy(2, 6.f, 20.f), Enemy(3, 7.f, 30.f) };
	TestEqual(TEXT("closest to hurting you (least left to walk) wins, not closest to the tower"), PickTarget(Row, Tower, 12.f, C), 1);

	C[1].bDead = true;
	TestEqual(TEXT("the dead are skipped"), PickTarget(Row, Tower, 12.f, C), 2);
	C[1].bDead = false;
	C[1].Layer = EDFEnemyLayer::Air;
	TestEqual(TEXT("a ground-only tower ignores a flyer"), PickTarget(Row, Tower, 12.f, C), 2);
	C[1].Layer = EDFEnemyLayer::Ground;
	C[1].bBurrowed = true;
	TestEqual(TEXT("burrowed is untargetable"), PickTarget(Row, Tower, 12.f, C), 2);
	C[1].bBurrowed = false;
	C[1].bStealth = true;
	TestEqual(TEXT("stealth needs detection"), PickTarget(Row, Tower, 12.f, C), 2);
	C[1].bDetected = true;
	TestEqual(TEXT("revealed stealth is fair game"), PickTarget(Row, Tower, 12.f, C), 1);
	C[1].bSightBlocked = true;
	TestEqual(TEXT("no line of sight, no shot"), PickTarget(Row, Tower, 12.f, C), 2);
	C[1].bSightBlocked = false;
	TestEqual(TEXT("out of range (5.5 m reach)"), PickTarget(Row, Tower, 5.5f, C), 0);
	TestEqual(TEXT("nothing in a 4 m reach"), PickTarget(Row, Tower, 4.f, C), (int32)INDEX_NONE);

	// Nova's minimum range: 5 m.
	FDFTowerRow Nova = Row;
	Nova.Kind = EDFTowerKind::Mortar;
	Nova.MinRangeMeters = 5.f;
	TArray<FDFTargetCandidate> Close = { Enemy(1, 4.f, 1.f), Enemy(2, 9.f, 50.f) };
	TestEqual(TEXT("a mortar cannot hit what is inside its minimum range"), PickTarget(Nova, Tower, 16.f, Close), 1);

	// INT 2026-09-21: RemainingToCore is an opaque key. A stranded walker (float max) sorts last; a
	// sieging one reports a small number and so is shot first, with no special case.
	TArray<FDFTargetCandidate> Siege = { Enemy(1, 5.f, TNumericLimits<float>::Max()), Enemy(2, 6.f, 2.f), Enemy(3, 7.f, 25.f) };
	TestEqual(TEXT("the Ram at the wall is the priority target"), PickTarget(Row, Tower, 12.f, Siege), 1);
	TArray<FDFTargetCandidate> OnlyStranded = { Enemy(1, 5.f, TNumericLimits<float>::Max()) };
	TestEqual(TEXT("a lone stranded walker is never chosen (the sim's strict < against float max)"), PickTarget(Row, Tower, 12.f, OnlyStranded), (int32)INDEX_NONE);
	TArray<FDFTargetCandidate> Tie = { Enemy(7, 5.f, 10.f), Enemy(8, 6.f, 10.f) };
	TestEqual(TEXT("a tie keeps the first"), PickTarget(Row, Tower, 12.f, Tie), 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDFTowerSightAndChainTest, "DF.Unit.Tower.SightChainAndNight", DFTowerMathTest::Flags)
bool FDFTowerSightAndChainTest::RunTest(const FString& Parameters)
{
	using namespace DFTowerMathTest;
	using namespace DFTowerMath;
	const FVector From = FVector::ZeroVector;
	const FVector Target(1000.f, 0.f, 0.f);
	TestTrue(TEXT("a Monolith 1 m off the line, halfway: blocked"), IsSightBlockedByBody(From, Target, FVector(500.f, 100.f, 0.f)));
	TestFalse(TEXT("2 m off the line: clear"), IsSightBlockedByBody(From, Target, FVector(500.f, 200.f, 0.f)));
	TestFalse(TEXT("behind the target: clear"), IsSightBlockedByBody(From, Target, FVector(1500.f, 0.f, 0.f)));
	TestFalse(TEXT("behind the tower: clear"), IsSightBlockedByBody(From, Target, FVector(-500.f, 0.f, 0.f)));

	FDFTowerRow Arc = Lance();
	Arc.Kind = EDFTowerKind::Tesla;
	Arc.ChainRange = 4.f;
	Arc.TargetLayers = { EDFEnemyLayer::Ground, EDFEnemyLayer::Air };
	TArray<FDFTargetCandidate> C = { Enemy(1, 10.f, 0.f), Enemy(2, 13.f, 0.f), Enemy(3, 12.f, 0.f), Enemy(4, 20.f, 0.f) };
	TSet<int32> Hit;
	Hit.Add(1);
	TestEqual(TEXT("the arc jumps to the nearest within 4 m"), NextChainTarget(Arc, 0, C, Hit), 2);
	Hit.Add(3);
	TestEqual(TEXT("never to one already struck"), NextChainTarget(Arc, 0, C, Hit), 1);
	Hit.Add(2);
	TestEqual(TEXT("nothing else within range: the chain ends"), NextChainTarget(Arc, 0, C, Hit), (int32)INDEX_NONE);
	C[2].bStealth = true;
	Hit.Reset();
	Hit.Add(1);
	TestEqual(TEXT("a hop ignores stealth: the arc jumps, it does not aim"), NextChainTarget(Arc, 0, C, Hit), 2);

	const FDFConditionRow NightRow = Night();
	const FDFConditionRow FogRow = Fog();
	TestEqual(TEXT("night: a beat before a new target"), AcquisitionDelaySeconds(&NightRow, false), 0.2f, Tol);
	TestEqual(TEXT("a marked target is acquired instantly"), AcquisitionDelaySeconds(&NightRow, true), 0.f, Tol);
	TestEqual(TEXT("fog has no delay"), AcquisitionDelaySeconds(&FogRow, false), 0.f, Tol);
	TestEqual(TEXT("clear weather has no delay"), AcquisitionDelaySeconds(nullptr, false), 0.f, Tol);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDFTowerContentTest, "DF.Unit.Tower.ContentTowersMatchSim", DFTowerMathTest::Flags)
bool FDFTowerContentTest::RunTest(const FString& Parameters)
{
	// The same functions on the committed tables (towers.json, conditions.json via DT_Towers / DT_Conditions):
	// the numbers the sim's Towers.cs and Conditions.cs give.
	FDFTestWorld World;
	UDFContentSubsystem* Content = World.GetSubsystem<UDFContentSubsystem>();
	if (!TestNotNull(TEXT("content subsystem"), Content))
	{
		return false;
	}
	if (!Content->IsReady() && !TestTrue(TEXT("tables load"), Content->LoadTables()))
	{
		return false;
	}
	const FDFTowerRow* Lance = Content->Tower(TEXT("lance"));
	const FDFTowerRow* Skywatch = Content->Tower(TEXT("skywatch"));
	const FDFTowerRow* Filament = Content->Tower(TEXT("filament"));
	const FDFConditionRow* Fog = Content->Condition(TEXT("fog"));
	const FDFConditionRow* Night = Content->Condition(TEXT("night"));
	if (!TestNotNull(TEXT("lance"), Lance) || !TestNotNull(TEXT("skywatch"), Skywatch) || !TestNotNull(TEXT("filament"), Filament)
		|| !TestNotNull(TEXT("fog"), Fog) || !TestNotNull(TEXT("night"), Night))
	{
		return false;
	}
	const TArray<int32> Fresh = { 0, 0, 0 };
	TestEqual(TEXT("lance: 12 m"), DFTowerMath::RangeMeters(*Lance, TEXT("lance"), Fresh, nullptr), 12.f, 1e-4f);
	TestEqual(TEXT("lance in fog: 8.4 m"), DFTowerMath::RangeMeters(*Lance, TEXT("lance"), Fresh, Fog), 8.4f, 1e-4f);
	TestEqual(TEXT("skywatch in fog keeps 15 m (exempt)"), DFTowerMath::RangeMeters(*Skywatch, TEXT("skywatch"), Fresh, Fog), 15.f, 1e-4f);
	TestEqual(TEXT("filament grows reach on optics"), DFTowerMath::RangePathIndex(*Filament), 2);
	TestEqual(TEXT("night delays acquisition 0.2 s"), DFTowerMath::AcquisitionDelaySeconds(Night, false), 0.2f, 1e-4f);
	TestEqual(TEXT("lance level 1 -> 2 costs 40"), DFTowerMath::QuoteUpgrade(*Lance, Fresh, 0, 1000, TMap<EDFScrapType, int32>()).MoneyCost, 40);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
