#include "Content/DFContentRows.h"
#include "DFPlayerController.h"
#include "Misc/AutomationTest.h"
#include "World/DFSocket.h"

#if WITH_DEV_AUTOMATION_TESTS

// DF.Unit.Match.PadHighlightRules: the ring under the crosshair's pad tells the truth about hold E.
// ADFPlayerController::DecidePadHighlight asks DFTowerMath::CheckPlacement (the host's own rule), so a
// green ring is a build the host will take (money aside) and a red one is a refusal. Testlane has
// only ground pads, so the other tags are covered here, with rows built by hand.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDFPadHighlightRulesTest, "DF.Unit.Match.PadHighlightRules",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FDFPadHighlightRulesTest::RunTest(const FString& Parameters)
{
	FDFTowerRow Lance;
	Lance.Kind = EDFTowerKind::Bolt;
	FDFTowerRow Barricade;
	Barricade.Kind = EDFTowerKind::Barricade;
	using E = EDFPadHighlight;
	auto Decide = [](EDFSocketTag Tag, const FDFTowerRow* Choice, bool bTower, bool bTrap)
	{
		return ADFPlayerController::DecidePadHighlight(Tag, Choice, bTower, bTrap);
	};

	TestTrue(TEXT("a Lance on a free ground pad: green"), Decide(EDFSocketTag::Ground, &Lance, false, false) == E::Free);
	TestTrue(TEXT("a Lance on a free wall pad: green"), Decide(EDFSocketTag::Wall, &Lance, false, false) == E::Free);
	TestTrue(TEXT("a Lance on a trap pad: red (TrapSocket)"), Decide(EDFSocketTag::Trap, &Lance, false, false) == E::Blocked);
	TestTrue(TEXT("a Lance on a barricade pad: red (WrongSocketTag)"), Decide(EDFSocketTag::Barricade, &Lance, false, false) == E::Blocked);
	TestTrue(TEXT("a barricade on a barricade pad: green"), Decide(EDFSocketTag::Barricade, &Barricade, false, false) == E::Free);
	TestTrue(TEXT("a barricade on a ground pad: red"), Decide(EDFSocketTag::Ground, &Barricade, false, false) == E::Blocked);
	TestTrue(TEXT("a tower on the pad: gold (hold U / hold X), whatever the choice"), Decide(EDFSocketTag::Ground, &Barricade, true, false) == E::Occupied);
	TestTrue(TEXT("a trap on the pad: red (hold E is refused, and U / X act on towers)"), Decide(EDFSocketTag::Trap, &Lance, false, true) == E::Blocked);
	TestTrue(TEXT("no choice (content missing): red"), Decide(EDFSocketTag::Ground, nullptr, false, false) == E::Blocked);
	return true;
}

// DF.Unit.Match.RangePreviewRules: the range ring round a free pad is the reach the choice would have if
// it went up now, by DFTowerMath::RangeMeters (the host's own number, weather included), and a mortar's
// dead zone is an inner ring that weather does not move. A built pad's ring is the standing tower's
// GetRangeMeters(), which DF.Func.Build.PadHighlightFollowsAim checks on a real tower.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDFRangePreviewRulesTest, "DF.Unit.Match.RangePreviewRules",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FDFRangePreviewRulesTest::RunTest(const FString& Parameters)
{
	FDFTowerRow Lance;
	Lance.Kind = EDFTowerKind::Bolt;
	Lance.RangeMeters = 12.f;
	FDFTowerRow Nova;
	Nova.Kind = EDFTowerKind::Mortar;
	Nova.RangeMeters = 16.f;
	Nova.MinRangeMeters = 5.f;
	FDFTowerRow Barricade;
	Barricade.Kind = EDFTowerKind::Barricade;
	FDFConditionRow Fog;
	Fog.TowerRangeFactor = 0.7f;
	Fog.RangeExemptTowerIds = { TEXT("detector") };
	const FName LanceId(TEXT("lance"));
	const FName NovaId(TEXT("nova"));
	using E = EDFPadHighlight;
	auto Decide = [](E Highlight, const FDFTowerRow* Choice, FName Id, const FDFConditionRow* Condition)
	{
		return ADFPlayerController::DecideRangePreview(Highlight, Choice, Id, Condition, /*Tower*/ nullptr);
	};

	const FDFRangePreview Clear = Decide(E::Free, &Lance, LanceId, nullptr);
	TestEqual(TEXT("a Lance on a free pad: its 12 m"), Clear.RangeMeters, 12.f);
	TestEqual(TEXT("a Lance has no dead zone"), Clear.MinRangeMeters, 0.f);
	TestEqual(TEXT("in fog: 70 % of it, as the tower would fight"), Decide(E::Free, &Lance, LanceId, &Fog).RangeMeters, 8.4f, 1e-4f);
	TestEqual(TEXT("a tower fog exempts keeps its reach"), Decide(E::Free, &Lance, TEXT("detector"), &Fog).RangeMeters, 12.f);

	const FDFRangePreview Mortar = Decide(E::Free, &Nova, NovaId, &Fog);
	TestEqual(TEXT("a Nova in fog: 11.2 m"), Mortar.RangeMeters, 11.2f, 1e-4f);
	TestEqual(TEXT("its dead zone is the row's 5 m, fog or not (PickTarget)"), Mortar.MinRangeMeters, 5.f);

	TestEqual(TEXT("a barricade reaches nothing: no ring"), Decide(E::Free, &Barricade, TEXT("barricade"), nullptr).RangeMeters, 0.f);
	TestEqual(TEXT("blocked: no ring (the host would refuse the build)"), Decide(E::Blocked, &Lance, LanceId, nullptr).RangeMeters, 0.f);
	TestEqual(TEXT("not aimed: no ring"), Decide(E::None, &Lance, LanceId, nullptr).RangeMeters, 0.f);
	TestEqual(TEXT("no choice: no ring"), Decide(E::Free, nullptr, NAME_None, nullptr).RangeMeters, 0.f);
	TestEqual(TEXT("occupied with no tower to ask: no ring (the choice is not what stands there)"), Decide(E::Occupied, &Lance, LanceId, nullptr).RangeMeters, 0.f);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
