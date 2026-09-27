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

#endif // WITH_DEV_AUTOMATION_TESTS
