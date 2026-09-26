#include "DFTestRows.h"
#include "Misc/AutomationTest.h"
#include "Status/DFStatusResolver.h"

#if WITH_DEV_AUTOMATION_TESTS

// C5 semantics, Statuses.cs / Step.cs:1070-1140, with no world attached.

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDFStatusStrongestWinsTest, "DF.Unit.Status.StrongestWins", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FDFStatusStrongestWinsTest::RunTest(const FString& Parameters)
{
	DFTestRows::FResolverFixture F;

	// chill (magnitude 0.35) lands in an empty Movement slot.
	TestEqual(TEXT("chill applied"), F.Apply(TEXT("chill")).Result, EDFStatusApplyResult::Applied);
	TestEqual(TEXT("chill magnitude = 1 - 0.65"), F.Resolver.Slot(EDFStatusChannel::Movement).Magnitude, 0.35f);

	// rubble (0.8x -> magnitude 0.2) is weaker: dropped, chill stays.
	TestEqual(TEXT("rubble dropped"), F.Apply(TEXT("rubble")).Result, EDFStatusApplyResult::DroppedWeaker);
	TestEqual(TEXT("chill still holds Movement"), F.Resolver.Slot(EDFStatusChannel::Movement).StatusId, FName(TEXT("chill")));

	// A stronger movement status (0.4x -> magnitude 0.6) replaces chill and reports what it displaced.
	F.Rows.Add(TEXT("deepChill"), DFTestRows::Status(EDFStatusChannel::Movement, 2.f, 0.4f));
	const FDFStatusApplyOutcome Stronger = F.Apply(TEXT("deepChill"));
	TestEqual(TEXT("stronger applied"), Stronger.Result, EDFStatusApplyResult::Applied);
	TestEqual(TEXT("stronger replaced chill"), Stronger.ReplacedStatusId, FName(TEXT("chill")));
	TestEqual(TEXT("Movement slot is deepChill"), F.Resolver.Slot(EDFStatusChannel::Movement).StatusId, FName(TEXT("deepChill")));

	// Now chill is the weaker one.
	TestEqual(TEXT("chill dropped against deepChill"), F.Apply(TEXT("chill")).Result, EDFStatusApplyResult::DroppedWeaker);

	// Equal magnitude does not replace either (>= keeps the incumbent).
	F.Rows.Add(TEXT("otherChill"), DFTestRows::Status(EDFStatusChannel::Movement, 9.f, 0.4f));
	TestEqual(TEXT("equal magnitude dropped"), F.Apply(TEXT("otherChill")).Result, EDFStatusApplyResult::DroppedWeaker);

	// A magnitude override is what strongest-wins compares (Swift: Movement x0.5).
	DFTestRows::FResolverFixture G;
	G.Apply(TEXT("chill"), 0.f, 1, 0.175f);
	TestEqual(TEXT("override magnitude stored"), G.Resolver.Slot(EDFStatusChannel::Movement).Magnitude, 0.175f);
	TestEqual(TEXT("rubble now beats the halved chill"), G.Apply(TEXT("rubble")).Result, EDFStatusApplyResult::Applied);

	// Channels are independent: a burn does not touch the Movement slot (no chill active in G now? rubble is; burn+rubble is no reaction).
	TestEqual(TEXT("burn applied alongside rubble"), G.Apply(TEXT("burn")).Result, EDFStatusApplyResult::Applied);
	TestTrue(TEXT("rubble still active"), G.Resolver.IsActive(EDFStatusChannel::Movement));
	TestTrue(TEXT("burn active"), G.Resolver.IsActive(EDFStatusChannel::Thermal));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDFStatusSameIdRefreshesTest, "DF.Unit.Status.SameIdRefreshes", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FDFStatusSameIdRefreshesTest::RunTest(const FString& Parameters)
{
	DFTestRows::FResolverFixture F;
	TestEqual(TEXT("first chill applied"), F.Apply(TEXT("chill"), 0.f, 1).Result, EDFStatusApplyResult::Applied);
	TestEqual(TEXT("ends at 1.5"), F.Resolver.Slot(EDFStatusChannel::Movement).EndTime, 1.5f);

	const FDFStatusApplyOutcome Again = F.Apply(TEXT("chill"), 1.0f, 2);
	TestEqual(TEXT("same id refreshes"), Again.Result, EDFStatusApplyResult::Refreshed);
	TestEqual(TEXT("end time moved to now + duration"), F.Resolver.Slot(EDFStatusChannel::Movement).EndTime, 2.5f);
	TestEqual(TEXT("source updated to the refresher"), F.Resolver.Slot(EDFStatusChannel::Movement).SourceId, 2);
	TestEqual(TEXT("never stacks: magnitude unchanged"), F.Resolver.Slot(EDFStatusChannel::Movement).Magnitude, 0.35f);

	// It expires once, at the refreshed time.
	TestEqual(TEXT("not expired at 2.4"), F.Resolver.Tick(2.4f, 0.1f), 0);
	TestTrue(TEXT("still active"), F.Resolver.IsActive(EDFStatusChannel::Movement));
	TArray<FDFStatusSlot> Expired;
	TestEqual(TEXT("expired at 2.5"), F.Resolver.Tick(2.5f, 0.1f, &Expired), 1);
	const FName ExpiredId = Expired.Num() == 1 ? Expired[0].StatusId : FName();
	TestEqual(TEXT("the expired slot is chill"), ExpiredId, FName(TEXT("chill")));
	TestFalse(TEXT("slot cleared"), F.Resolver.IsActive(EDFStatusChannel::Movement));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDFStatusBurnRejectedOnShieldTest, "DF.Unit.Status.BurnRejectedOnShield", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FDFStatusBurnRejectedOnShieldTest::RunTest(const FString& Parameters)
{
	DFTestRows::FResolverFixture F;
	F.Target.Shield = 25.f;   // a Warden with its shield up
	TestEqual(TEXT("burn rejected while shielded"), F.Apply(TEXT("burn")).Result, EDFStatusApplyResult::RejectedShield);
	TestFalse(TEXT("Thermal slot empty"), F.Resolver.IsActive(EDFStatusChannel::Thermal));

	// Poison is the answer burn is not: it passes through the shield.
	TestEqual(TEXT("poison lands through a shield"), F.Apply(TEXT("poison")).Result, EDFStatusApplyResult::Applied);
	// So do the non-damage channels.
	TestEqual(TEXT("chill lands through a shield"), F.Apply(TEXT("chill")).Result, EDFStatusApplyResult::Applied);

	// Shield popped: burn lands.
	F.Target.Shield = 0.f;
	DFTestRows::FResolverFixture G;
	G.Target.Shield = 0.f;
	TestEqual(TEXT("burn applied once the shield is down"), G.Apply(TEXT("burn")).Result, EDFStatusApplyResult::Applied);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDFStatusControlRejectedAtFullCcResistTest, "DF.Unit.Status.ControlRejectedAtFullCcResist", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FDFStatusControlRejectedAtFullCcResistTest::RunTest(const FString& Parameters)
{
	DFTestRows::FResolverFixture F;
	F.Resolver.CcResist = 1.f;
	TestEqual(TEXT("shock rejected at full gauge"), F.Apply(TEXT("shock")).Result, EDFStatusApplyResult::RejectedCcResist);
	TestEqual(TEXT("freeze rejected at full gauge"), F.Apply(TEXT("freeze")).Result, EDFStatusApplyResult::RejectedCcResist);
	TestFalse(TEXT("Control slot empty"), F.Resolver.IsActive(EDFStatusChannel::Control));
	// Soft statuses are not gated by the gauge.
	TestEqual(TEXT("chill lands at full gauge"), F.Apply(TEXT("chill")).Result, EDFStatusApplyResult::Applied);

	F.Resolver.CcResist = 0.99f;
	TestEqual(TEXT("shock lands just under full"), F.Apply(TEXT("shock")).Result, EDFStatusApplyResult::Reacted);   // chill + shock = flashFreeze
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDFStatusThermalShockBurstTest, "DF.Unit.Status.ThermalShockBurst", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FDFStatusThermalShockBurstTest::RunTest(const FString& Parameters)
{
	// Step.cs ApplyStatus: the active half is consumed (slot cleared) and the incoming one never
	// lands ("return; // incoming status consumed by the reaction"). Both inputs are consumed.
	DFTestRows::FResolverFixture F;
	TestEqual(TEXT("chill applied"), F.Apply(TEXT("chill")).Result, EDFStatusApplyResult::Applied);
	const FDFStatusApplyOutcome Out = F.Apply(TEXT("burn"));
	TestEqual(TEXT("burn reacted"), Out.Result, EDFStatusApplyResult::Reacted);
	TestEqual(TEXT("reaction is thermalShock"), Out.ReactionId, FName(TEXT("thermalShock")));
	TestEqual(TEXT("burst fraction 12%"), Out.BurstFraction, 0.12f);
	TestEqual(TEXT("consumed chill"), Out.ConsumedStatusId, FName(TEXT("chill")));
	TestEqual(TEXT("consumed from Movement"), Out.ConsumedChannel, EDFStatusChannel::Movement);
	TestTrue(TEXT("nothing emitted"), Out.EmittedStatusId.IsNone());
	TestFalse(TEXT("chill gone"), F.Resolver.IsActive(EDFStatusChannel::Movement));
	TestFalse(TEXT("burn never landed"), F.Resolver.IsActive(EDFStatusChannel::Thermal));

	// 12% of a 55 hp Aegis = 6.6.
	const float MaxHp = 55.f;
	TestEqual(TEXT("burst on an aegis"), MaxHp * Out.BurstFraction, 6.6f);

	// The pair reacts either way round (burn first, then chill).
	DFTestRows::FResolverFixture G;
	G.Apply(TEXT("burn"));
	const FDFStatusApplyOutcome Rev = G.Apply(TEXT("chill"));
	TestEqual(TEXT("chill onto burn reacts"), Rev.Result, EDFStatusApplyResult::Reacted);
	TestEqual(TEXT("consumed burn"), Rev.ConsumedStatusId, FName(TEXT("burn")));
	TestFalse(TEXT("Thermal empty"), G.Resolver.IsActive(EDFStatusChannel::Thermal));
	TestFalse(TEXT("Movement empty"), G.Resolver.IsActive(EDFStatusChannel::Movement));

	// The reaction scan precedes the shield gate (Step.cs order): a chilled, shielded target still detonates.
	DFTestRows::FResolverFixture H;
	H.Apply(TEXT("chill"));
	H.Target.Shield = 25.f;
	TestEqual(TEXT("thermalShock fires on a chilled shielded target"), H.Apply(TEXT("burn")).Result, EDFStatusApplyResult::Reacted);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDFStatusFlashFreezeEmitsFreezeTest, "DF.Unit.Status.FlashFreezeEmitsFreeze", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FDFStatusFlashFreezeEmitsFreezeTest::RunTest(const FString& Parameters)
{
	DFTestRows::FResolverFixture F;
	F.Apply(TEXT("chill"), 0.f);
	const FDFStatusApplyOutcome Out = F.Apply(TEXT("shock"), 0.f);
	TestEqual(TEXT("reacted"), Out.Result, EDFStatusApplyResult::Reacted);
	TestEqual(TEXT("flashFreeze"), Out.ReactionId, FName(TEXT("flashFreeze")));
	TestEqual(TEXT("no burst"), Out.BurstFraction, 0.f);
	TestEqual(TEXT("emitted freeze"), Out.EmittedStatusId, FName(TEXT("freeze")));
	TestEqual(TEXT("into Control"), Out.EmittedChannel, EDFStatusChannel::Control);
	TestEqual(TEXT("freeze duration 1.2"), Out.EmittedDuration, 1.2f);
	TestFalse(TEXT("chill consumed"), F.Resolver.IsActive(EDFStatusChannel::Movement));
	TestEqual(TEXT("Control slot holds freeze, not shock"), F.Resolver.Slot(EDFStatusChannel::Control).StatusId, FName(TEXT("freeze")));
	TestEqual(TEXT("freeze ends at 1.2"), F.Resolver.Slot(EDFStatusChannel::Control).EndTime, 1.2f);
	TestTrue(TEXT("freeze is hard control"), F.Resolver.IsControlled());

	// The emitted freeze is gated by cc-resist like any hard control (Step.cs: emitDef.HardControl && CcResist >= 1).
	DFTestRows::FResolverFixture G;
	G.Apply(TEXT("chill"));
	G.Resolver.CcResist = 1.f;
	// shock itself is rejected at full gauge — but the reaction scan runs first, so it reacts and the emit is refused.
	const FDFStatusApplyOutcome Gated = G.Apply(TEXT("shock"));
	TestEqual(TEXT("still reacts"), Gated.Result, EDFStatusApplyResult::Reacted);
	TestTrue(TEXT("emit refused at full gauge"), Gated.bEmitRejected);
	TestFalse(TEXT("no freeze written"), G.Resolver.IsActive(EDFStatusChannel::Control));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDFStatusCorrodeBurstTest, "DF.Unit.Status.CorrodeBurst", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FDFStatusCorrodeBurstTest::RunTest(const FString& Parameters)
{
	DFTestRows::FResolverFixture F;
	F.Target.Shield = 25.f;   // poison lands through a shield, and the reaction does not care
	TestEqual(TEXT("poison applied"), F.Apply(TEXT("poison")).Result, EDFStatusApplyResult::Applied);
	const FDFStatusApplyOutcome Out = F.Apply(TEXT("shred"));
	TestEqual(TEXT("reacted"), Out.Result, EDFStatusApplyResult::Reacted);
	TestEqual(TEXT("corrode"), Out.ReactionId, FName(TEXT("corrode")));
	TestEqual(TEXT("burst 10%"), Out.BurstFraction, 0.10f);
	TestEqual(TEXT("consumed poison"), Out.ConsumedStatusId, FName(TEXT("poison")));
	TestFalse(TEXT("Toxin empty"), F.Resolver.IsActive(EDFStatusChannel::Toxin));
	TestFalse(TEXT("Defense empty (shred consumed)"), F.Resolver.IsActive(EDFStatusChannel::Defense));
	// 10% of a 70 hp Ram = 7.
	TestEqual(TEXT("burst on a ram"), 70.f * Out.BurstFraction, 7.f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDFStatusReactionsNeverChainTest, "DF.Unit.Status.ReactionsNeverChain", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FDFStatusReactionsNeverChainTest::RunTest(const FString& Parameters)
{
	// A hypothetical table where a reaction OUTPUT is another reaction's INPUT: freeze + mark.
	// The closure rule says the emitted freeze is written straight to its slot and never
	// re-scanned, so mark survives and only flashFreeze fires.
	DFTestRows::FResolverFixture F;
	F.Resolver.Reactions.Add(TEXT("fakeChain"), DFTestRows::Reaction(TEXT("freeze"), TEXT("mark"), 0.5f));

	TestEqual(TEXT("mark applied"), F.Apply(TEXT("mark")).Result, EDFStatusApplyResult::Applied);
	TestEqual(TEXT("chill applied"), F.Apply(TEXT("chill")).Result, EDFStatusApplyResult::Applied);
	const FDFStatusApplyOutcome Out = F.Apply(TEXT("shock"));
	TestEqual(TEXT("flashFreeze fired"), Out.ReactionId, FName(TEXT("flashFreeze")));
	TestEqual(TEXT("freeze emitted"), Out.EmittedStatusId, FName(TEXT("freeze")));
	TestTrue(TEXT("mark untouched by the emitted freeze"), F.Resolver.IsActive(EDFStatusChannel::Vulnerability));
	TestTrue(TEXT("freeze sits in Control"), F.Resolver.Slot(EDFStatusChannel::Control).StatusId == FName(TEXT("freeze")));
	TestEqual(TEXT("no burst from the fake chain"), Out.BurstFraction, 0.f);

	// The same freeze arriving as an ordinary INPUT would react: closure is about outputs only.
	DFTestRows::FResolverFixture G;
	G.Resolver.Reactions.Add(TEXT("fakeChain"), DFTestRows::Reaction(TEXT("freeze"), TEXT("mark"), 0.5f));
	G.Apply(TEXT("mark"));
	const FDFStatusApplyOutcome Direct = G.Apply(TEXT("freeze"));
	TestEqual(TEXT("direct freeze onto mark reacts"), Direct.Result, EDFStatusApplyResult::Reacted);
	TestEqual(TEXT("fakeChain fired"), Direct.ReactionId, FName(TEXT("fakeChain")));

	// And the real table has no reaction that consumes a reaction output: freeze + anything = nothing.
	DFTestRows::FResolverFixture H;
	H.Apply(TEXT("chill"));
	H.Apply(TEXT("shock"));   // -> freeze
	TestEqual(TEXT("burn onto freeze: plain apply"), H.Apply(TEXT("burn")).Result, EDFStatusApplyResult::Applied);
	TestEqual(TEXT("chill onto freeze+burn: thermalShock with burn, freeze untouched"), H.Apply(TEXT("chill")).ReactionId, FName(TEXT("thermalShock")));
	TestTrue(TEXT("freeze still active"), H.Resolver.IsActive(EDFStatusChannel::Control));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDFStatusCcResistGaugeTest, "DF.Unit.Status.CcResistGauge", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FDFStatusCcResistGaugeTest::RunTest(const FString& Parameters)
{
	DFTestRows::FResolverFixture F;
	TestEqual(TEXT("fill rate default 0.45"), F.Resolver.CcResistFillPerSecond, 0.45f);
	TestEqual(TEXT("decay rate default 0.125"), F.Resolver.CcResistDecayPerSecond, 0.125f);

	// Freeze (hard control, 1.2 s): fills at 0.45/s while active.
	F.Apply(TEXT("freeze"), 0.f);
	F.Resolver.Tick(0.5f, 0.5f);
	TestTrue(TEXT("gauge after 0.5 s controlled = 0.225"), FMath::IsNearlyEqual(F.Resolver.CcResist, 0.225f, 1e-4f));
	F.Resolver.Tick(1.0f, 0.5f);
	TestTrue(TEXT("gauge after 1.0 s = 0.45"), FMath::IsNearlyEqual(F.Resolver.CcResist, 0.45f, 1e-4f));
	// Freeze expires at 1.2; the tick that expires it sees no control and decays.
	F.Resolver.Tick(1.5f, 0.5f);
	TestFalse(TEXT("freeze expired"), F.Resolver.IsActive(EDFStatusChannel::Control));
	TestTrue(TEXT("decayed by 0.125 x 0.5"), FMath::IsNearlyEqual(F.Resolver.CcResist, 0.45f - 0.0625f, 1e-4f));
	// Decays to 0, never below.
	F.Resolver.Tick(20.f, 10.f);
	TestEqual(TEXT("gauge floors at 0"), F.Resolver.CcResist, 0.f);

	// Fills to 1 and caps; then hard control is refused until it decays under 1.
	DFTestRows::FResolverFixture G;
	G.Apply(TEXT("freeze"), 0.f);
	G.Resolver.Tick(0.5f, 10.f);
	TestEqual(TEXT("gauge caps at 1"), G.Resolver.CcResist, 1.f);
	TestEqual(TEXT("shock refused at 1"), G.Apply(TEXT("shock"), 0.6f).Result, EDFStatusApplyResult::RejectedCcResist);
	G.Resolver.Tick(2.f, 1.f);   // freeze gone, decays 0.125
	TestTrue(TEXT("gauge decays"), G.Resolver.CcResist < 1.f);
	TestEqual(TEXT("shock lands again"), G.Apply(TEXT("shock"), 2.f).Result, EDFStatusApplyResult::Applied);

	// Tether fills at 0.5x (row CcResistFill 0.5) and is not hard control.
	DFTestRows::FResolverFixture T;
	T.Apply(TEXT("magnetize"), 0.f);
	TestFalse(TEXT("tether is not hard control"), T.Resolver.IsControlled());
	T.Resolver.Tick(1.f, 1.f);
	TestTrue(TEXT("tether fills at 0.225/s"), FMath::IsNearlyEqual(T.Resolver.CcResist, 0.225f, 1e-4f));

	// The strongest active scale wins: freeze (1.0) + magnetize (0.5) fills at 1.0.
	DFTestRows::FResolverFixture B;
	B.Apply(TEXT("magnetize"), 0.f);
	B.Apply(TEXT("freeze"), 0.f);
	B.Resolver.Tick(1.f, 1.f);
	TestTrue(TEXT("hard control dominates the fill"), FMath::IsNearlyEqual(B.Resolver.CcResist, 0.45f, 1e-4f));

	// Rates are data: a balance override changes the slope.
	DFTestRows::FResolverFixture R;
	R.Resolver.CcResistFillPerSecond = 1.f;
	R.Apply(TEXT("freeze"), 0.f);
	R.Resolver.Tick(0.25f, 0.25f);
	TestTrue(TEXT("override fill rate honoured"), FMath::IsNearlyEqual(R.Resolver.CcResist, 0.25f, 1e-4f));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDFStatusTetherImmunityTest, "DF.Unit.Status.TetherImmunity", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FDFStatusTetherImmunityTest::RunTest(const FString& Parameters)
{
	DFTestRows::FResolverFixture F;
	F.Target.Mass = 3.f;   // aegis
	TestEqual(TEXT("magnetize lands on Mass 3"), F.Apply(TEXT("magnetize")).Result, EDFStatusApplyResult::Applied);

	DFTestRows::FResolverFixture Ram;
	Ram.Target.Mass = 6.f;
	TestEqual(TEXT("refused at Mass 6"), Ram.Apply(TEXT("magnetize")).Result, EDFStatusApplyResult::RejectedImmune);

	DFTestRows::FResolverFixture Monolith;
	Monolith.Target.Mass = 8.f;
	TestEqual(TEXT("refused at Mass 8"), Monolith.Apply(TEXT("magnetize")).Result, EDFStatusApplyResult::RejectedImmune);

	DFTestRows::FResolverFixture Phased;
	Phased.Target.Mass = 1.f;
	Phased.Target.bTetherImmune = true;   // DF.Enemy.State.Phased / boss tag on the owner
	TestEqual(TEXT("refused on an immune target"), Phased.Apply(TEXT("magnetize")).Result, EDFStatusApplyResult::RejectedImmune);
	TestEqual(TEXT("chill still lands on an immune target"), Phased.Apply(TEXT("chill")).Result, EDFStatusApplyResult::Applied);
	return true;
}

// Step.cs ApplyStatus 1070-1131: the reaction scan runs across every active slot BEFORE the
// burn-vs-shield and hard-control-vs-CcResist gates, which apply only to non-reaction paths.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDFStatusReactionScanPrecedesGatesTest, "DF.Unit.Status.ReactionScanPrecedesGates", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FDFStatusReactionScanPrecedesGatesTest::RunTest(const FString& Parameters)
{
	// Case 1: a shielded, chilled Warden hit by burn. Burn alone is refused on a shield; with
	// chill active the scan matches first and thermalShock fires anyway.
	DFTestRows::FResolverFixture Warden;
	Warden.Target.Shield = 25.f;
	TestEqual(TEXT("burn alone refused on the shield"), Warden.Apply(TEXT("burn")).Result, EDFStatusApplyResult::RejectedShield);
	TestEqual(TEXT("chill lands through the shield"), Warden.Apply(TEXT("chill")).Result, EDFStatusApplyResult::Applied);
	const FDFStatusApplyOutcome Shock = Warden.Apply(TEXT("burn"));
	TestEqual(TEXT("burn onto chill reacts despite the shield"), Shock.Result, EDFStatusApplyResult::Reacted);
	TestEqual(TEXT("thermalShock"), Shock.ReactionId, FName(TEXT("thermalShock")));
	TestEqual(TEXT("burst 12%"), Shock.BurstFraction, 0.12f);
	TestFalse(TEXT("chill consumed"), Warden.Resolver.IsActive(EDFStatusChannel::Movement));
	TestFalse(TEXT("burn never landed"), Warden.Resolver.IsActive(EDFStatusChannel::Thermal));

	// Case 2: shock at CcResist >= 1 into a chilled target. Shock alone is refused by the gauge;
	// with chill active flashFreeze fires — and its emitted freeze is then itself gated by CcResist < 1.
	DFTestRows::FResolverFixture Full;
	Full.Resolver.CcResist = 1.f;
	TestEqual(TEXT("shock alone refused at full gauge"), Full.Apply(TEXT("shock")).Result, EDFStatusApplyResult::RejectedCcResist);
	Full.Apply(TEXT("chill"));
	const FDFStatusApplyOutcome Freeze = Full.Apply(TEXT("shock"));
	TestEqual(TEXT("shock onto chill reacts despite the gauge"), Freeze.Result, EDFStatusApplyResult::Reacted);
	TestEqual(TEXT("flashFreeze"), Freeze.ReactionId, FName(TEXT("flashFreeze")));
	TestFalse(TEXT("chill consumed"), Full.Resolver.IsActive(EDFStatusChannel::Movement));
	TestTrue(TEXT("emitted freeze refused by the gauge"), Freeze.bEmitRejected);
	TestTrue(TEXT("no emitted status recorded"), Freeze.EmittedStatusId.IsNone());
	TestFalse(TEXT("Control slot empty"), Full.Resolver.IsActive(EDFStatusChannel::Control));

	// And just under the gauge the same pair emits the freeze.
	DFTestRows::FResolverFixture Under;
	Under.Resolver.CcResist = 0.99f;
	Under.Apply(TEXT("chill"));
	const FDFStatusApplyOutcome Emit = Under.Apply(TEXT("shock"));
	TestEqual(TEXT("reacts"), Emit.Result, EDFStatusApplyResult::Reacted);
	TestFalse(TEXT("emit not refused"), Emit.bEmitRejected);
	TestEqual(TEXT("freeze written"), Under.Resolver.Slot(EDFStatusChannel::Control).StatusId, FName(TEXT("freeze")));
	return true;
}

// Step.cs ApplyStatus: `slot.StatusId = null; // consume the active half` ... `return; // incoming
// status consumed by the reaction`. The ACTIVE partner's slot is cleared; the INCOMING status is
// never written anywhere; nothing else on the target is touched.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDFStatusReactionConsumesActiveNotIncomingTest, "DF.Unit.Status.ReactionConsumesActiveNotIncoming", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FDFStatusReactionConsumesActiveNotIncomingTest::RunTest(const FString& Parameters)
{
	// poison active (Toxin), mark active (Vulnerability, a bystander), shred incoming (Defense).
	DFTestRows::FResolverFixture F;
	F.Apply(TEXT("mark"), 0.f, 7);
	F.Apply(TEXT("poison"), 0.f, 3);
	const FDFStatusApplyOutcome Out = F.Apply(TEXT("shred"), 0.5f, 9);
	TestEqual(TEXT("corrode fired"), Out.ReactionId, FName(TEXT("corrode")));
	TestEqual(TEXT("consumed the ACTIVE poison"), Out.ConsumedStatusId, FName(TEXT("poison")));
	TestEqual(TEXT("from Toxin"), Out.ConsumedChannel, EDFStatusChannel::Toxin);
	TestFalse(TEXT("Toxin slot cleared"), F.Resolver.IsActive(EDFStatusChannel::Toxin));
	TestFalse(TEXT("incoming shred NOT applied"), F.Resolver.IsActive(EDFStatusChannel::Defense));
	TestTrue(TEXT("bystander mark untouched"), F.Resolver.IsActive(EDFStatusChannel::Vulnerability));
	TestEqual(TEXT("mark keeps its source"), F.Resolver.Slot(EDFStatusChannel::Vulnerability).SourceId, 7);
	TestEqual(TEXT("mark keeps its end time"), F.Resolver.Slot(EDFStatusChannel::Vulnerability).EndTime, 4.f);
	TestTrue(TEXT("nothing emitted"), Out.EmittedStatusId.IsNone());

	// The other way round: shred active, poison incoming — shred is consumed, poison never lands.
	DFTestRows::FResolverFixture G;
	G.Apply(TEXT("shred"));
	const FDFStatusApplyOutcome Rev = G.Apply(TEXT("poison"));
	TestEqual(TEXT("corrode again"), Rev.ReactionId, FName(TEXT("corrode")));
	TestEqual(TEXT("consumed the ACTIVE shred"), Rev.ConsumedStatusId, FName(TEXT("shred")));
	TestFalse(TEXT("Defense cleared"), G.Resolver.IsActive(EDFStatusChannel::Defense));
	TestFalse(TEXT("incoming poison NOT applied"), G.Resolver.IsActive(EDFStatusChannel::Toxin));

	// The incoming status is consumed even though its own channel was free and it would have
	// been the stronger of nothing: the reaction return happens before the slot logic.
	TestTrue(TEXT("a fresh poison lands now that shred is gone"), G.Apply(TEXT("poison")).Result == EDFStatusApplyResult::Applied);
	return true;
}

// Step.cs ApplyStatus: `emitSlot.StatusId = emitDef.Id; emitSlot.TimeLeft = emitDef.MaxDurationSeconds;`
// — the EmitStatus is written straight into its slot, whatever held it, at the row's own duration.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDFStatusEmitOverwritesSlotTest, "DF.Unit.Status.EmitOverwritesSlot", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FDFStatusEmitOverwritesSlotTest::RunTest(const FString& Parameters)
{
	// A hard control far STRONGER than freeze (magnitude = duration: 5 vs 1.2) holds the Control
	// slot. Strongest-wins would keep it; the emit does not ask.
	DFTestRows::FResolverFixture F;
	F.Rows.Add(TEXT("stasis"), DFTestRows::Status(EDFStatusChannel::Control, 5.f, 0.f, 0.f, 1.f, 0.f, true));
	TestEqual(TEXT("stasis applied"), F.Apply(TEXT("stasis"), 0.f, 4).Result, EDFStatusApplyResult::Applied);
	TestEqual(TEXT("freeze as an input is dropped against stasis"), F.Apply(TEXT("freeze"), 0.f, 4).Result, EDFStatusApplyResult::DroppedWeaker);
	F.Apply(TEXT("chill"), 0.f, 1);
	const FDFStatusApplyOutcome Out = F.Apply(TEXT("shock"), 2.f, 2);
	TestEqual(TEXT("flashFreeze"), Out.ReactionId, FName(TEXT("flashFreeze")));
	TestEqual(TEXT("emitted freeze"), Out.EmittedStatusId, FName(TEXT("freeze")));
	TestEqual(TEXT("it displaced stasis"), Out.EmitReplacedStatusId, FName(TEXT("stasis")));
	const FDFStatusSlot& Control = F.Resolver.Slot(EDFStatusChannel::Control);
	TestEqual(TEXT("Control slot is freeze"), Control.StatusId, FName(TEXT("freeze")));
	TestEqual(TEXT("freeze magnitude 1.2 (weaker, still written)"), Control.Magnitude, 1.2f);
	TestEqual(TEXT("freeze duration is the row's: 2 + 1.2"), Control.EndTime, 3.2f);
	TestEqual(TEXT("freeze source is the reactor"), Control.SourceId, 2);

	// The same emit onto a slot already holding freeze is not a refresh either: it is rewritten from Now.
	DFTestRows::FResolverFixture G;
	G.Apply(TEXT("chill"), 0.f);
	G.Apply(TEXT("shock"), 0.f);   // freeze until 1.2
	TestEqual(TEXT("freeze until 1.2"), G.Resolver.Slot(EDFStatusChannel::Control).EndTime, 1.2f);
	G.Apply(TEXT("chill"), 1.f);
	const FDFStatusApplyOutcome Again = G.Apply(TEXT("shock"), 1.f);
	TestEqual(TEXT("second flashFreeze"), Again.ReactionId, FName(TEXT("flashFreeze")));
	TestEqual(TEXT("it overwrote the earlier freeze"), Again.EmitReplacedStatusId, FName(TEXT("freeze")));
	TestEqual(TEXT("freeze now until 2.2"), G.Resolver.Slot(EDFStatusChannel::Control).EndTime, 2.2f);

	// The emit takes the row's raw duration: no duration factor rides on it (Step.cs writes emitDef.MaxDurationSeconds).
	DFTestRows::FResolverFixture H;
	H.Apply(TEXT("chill"), 0.f);
	const FDFStatusApplyOutcome Scaled = H.Resolver.Apply(TEXT("shock"), H.Rows.FindChecked(TEXT("shock")), 0.f, 1, H.Target, -1.f, /*DurationFactor*/ 2.f);
	TestEqual(TEXT("emitted duration unscaled"), Scaled.EmittedDuration, 1.2f);
	return true;
}

// Step.cs ApplyStatus: `if (burst > 0f) Damage(...); if (reaction.EmitStatus is { } emitted && !enemy.Dead) { ... }`
// — a burst that kills the target suppresses the emit. The world half reports the kill through OnReaction.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDFStatusNoEmitOnKillTest, "DF.Unit.Status.NoEmitOnKill", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FDFStatusNoEmitOnKillTest::RunTest(const FString& Parameters)
{
	// A reaction that both bursts and emits (none in the shipped table; the closure rule does not care).
	DFTestRows::FResolverFixture F;
	F.Resolver.Reactions.Reset();
	F.Resolver.Reactions.Add(TEXT("lethalFreeze"), DFTestRows::Reaction(TEXT("chill"), TEXT("shock"), 1.0f, TEXT("freeze")));

	int32 Calls = 0;
	FName SeenReaction;
	bool bSeenSlotClearedBeforeBurst = false;
	F.Resolver.OnReaction = [&](const FDFStatusApplyOutcome& Partial) -> bool
	{
		++Calls;
		SeenReaction = Partial.ReactionId;
		// The consumed slot is already clear when the burst goes out (Step.cs clears it first).
		bSeenSlotClearedBeforeBurst = !F.Resolver.IsActive(Partial.ConsumedChannel);
		return false;   // the burst killed the target
	};
	F.Apply(TEXT("chill"));
	const FDFStatusApplyOutcome Out = F.Apply(TEXT("shock"));
	TestEqual(TEXT("reacted"), Out.Result, EDFStatusApplyResult::Reacted);
	TestEqual(TEXT("burst callback ran once"), Calls, 1);
	TestEqual(TEXT("with the reaction id"), SeenReaction, FName(TEXT("lethalFreeze")));
	TestTrue(TEXT("consumed slot cleared before the burst"), bSeenSlotClearedBeforeBurst);
	TestEqual(TEXT("burst fraction reported"), Out.BurstFraction, 1.0f);
	TestTrue(TEXT("emit skipped: target dead"), Out.bEmitSkippedDead);
	TestFalse(TEXT("not the cc-resist refusal"), Out.bEmitRejected);
	TestTrue(TEXT("nothing emitted"), Out.EmittedStatusId.IsNone());
	TestFalse(TEXT("Control slot empty"), F.Resolver.IsActive(EDFStatusChannel::Control));

	// Alive: the same reaction writes its freeze.
	DFTestRows::FResolverFixture G;
	G.Resolver.Reactions.Reset();
	G.Resolver.Reactions.Add(TEXT("lethalFreeze"), DFTestRows::Reaction(TEXT("chill"), TEXT("shock"), 1.0f, TEXT("freeze")));
	G.Resolver.OnReaction = [](const FDFStatusApplyOutcome&) { return true; };
	G.Apply(TEXT("chill"));
	const FDFStatusApplyOutcome Alive = G.Apply(TEXT("shock"));
	TestFalse(TEXT("emit not skipped"), Alive.bEmitSkippedDead);
	TestEqual(TEXT("freeze written"), G.Resolver.Slot(EDFStatusChannel::Control).StatusId, FName(TEXT("freeze")));

	// Unbound callback = alive (a resolver with no world attached).
	DFTestRows::FResolverFixture H;
	H.Apply(TEXT("chill"));
	TestEqual(TEXT("unbound: flashFreeze emits"), H.Apply(TEXT("shock")).EmittedStatusId, FName(TEXT("freeze")));
	return true;
}

// Step.cs ApplyStatus: `if (active.Magnitude >= incoming.Magnitude) return;` — a tie keeps the active status, untouched.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDFStatusTieKeepsActiveTest, "DF.Unit.Status.TieKeepsActive", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FDFStatusTieKeepsActiveTest::RunTest(const FString& Parameters)
{
	// Two different Movement statuses with the same magnitude (0.65x -> 0.35): the incumbent stays.
	DFTestRows::FResolverFixture F;
	F.Rows.Add(TEXT("tarChill"), DFTestRows::Status(EDFStatusChannel::Movement, 4.f, 0.65f));
	TestEqual(TEXT("chill applied"), F.Apply(TEXT("chill"), 0.f, 1).Result, EDFStatusApplyResult::Applied);
	const FDFStatusApplyOutcome Tie = F.Apply(TEXT("tarChill"), 1.f, 2);
	TestEqual(TEXT("tie dropped"), Tie.Result, EDFStatusApplyResult::DroppedWeaker);
	TestTrue(TEXT("nothing replaced"), Tie.ReplacedStatusId.IsNone());
	const FDFStatusSlot& Slot = F.Resolver.Slot(EDFStatusChannel::Movement);
	TestEqual(TEXT("chill still holds the slot"), Slot.StatusId, FName(TEXT("chill")));
	TestEqual(TEXT("its source unchanged"), Slot.SourceId, 1);
	TestEqual(TEXT("its end time unchanged (not refreshed)"), Slot.EndTime, 1.5f);
	TestEqual(TEXT("its magnitude unchanged"), Slot.Magnitude, 0.35f);

	// Ties in the Control channel too (shock 0.25 vs a 0.25 s stagger-like row).
	DFTestRows::FResolverFixture G;
	G.Rows.Add(TEXT("jolt"), DFTestRows::Status(EDFStatusChannel::Control, 0.25f, 0.f, 0.f, 1.f, 0.f, true));
	G.Apply(TEXT("jolt"), 0.f, 5);
	TestEqual(TEXT("shock ties jolt: dropped"), G.Apply(TEXT("shock"), 0.1f, 6).Result, EDFStatusApplyResult::DroppedWeaker);
	TestEqual(TEXT("jolt stays"), G.Resolver.Slot(EDFStatusChannel::Control).StatusId, FName(TEXT("jolt")));

	// Strictly stronger still wins, and the tie rule is on magnitude, not id order.
	G.Rows.Add(TEXT("stun"), DFTestRows::Status(EDFStatusChannel::Control, 0.5f, 0.f, 0.f, 1.f, 0.f, true));
	TestEqual(TEXT("stun beats jolt"), G.Apply(TEXT("stun"), 0.1f, 7).Result, EDFStatusApplyResult::Applied);
	return true;
}

// C5 / B§1.6: "Heroes may carry only Movement statuses and Stagger (0.25 s, no cc-resist), never
// Thermal/Toxin." Not a Step.cs rule (the sim's heroes carried no statuses): a hero target refuses
// everything but Movement rows and the stagger row, before the reaction scan, and its stagger
// neither fills nor consults the cc-resist gauge.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDFStatusHeroRejectsThermalAndToxinTest, "DF.Unit.Status.HeroRejectsThermalAndToxin", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FDFStatusHeroRejectsThermalAndToxinTest::RunTest(const FString& Parameters)
{
	DFTestRows::FResolverFixture Hero;
	Hero.Target.bHero = true;

	// Movement lands with the usual channel rules.
	TestEqual(TEXT("chill lands on a hero"), Hero.Apply(TEXT("chill")).Result, EDFStatusApplyResult::Applied);
	TestEqual(TEXT("rubble is the weaker Movement status: dropped"), Hero.Apply(TEXT("rubble")).Result, EDFStatusApplyResult::DroppedWeaker);
	TestEqual(TEXT("chill refreshes"), Hero.Apply(TEXT("chill"), 0.5f).Result, EDFStatusApplyResult::Refreshed);

	// Everything outside Movement / stagger is refused — Thermal and Toxin first among them.
	for (const TCHAR* Id : { TEXT("burn"), TEXT("poison"), TEXT("shred"), TEXT("mark"), TEXT("reveal"), TEXT("magnetize"), TEXT("freeze") })
	{
		TestEqual(*FString::Printf(TEXT("%s refused on a hero"), Id), Hero.Apply(Id).Result, EDFStatusApplyResult::RejectedHero);
	}
	TestFalse(TEXT("Thermal empty"), Hero.Resolver.IsActive(EDFStatusChannel::Thermal));
	TestFalse(TEXT("Toxin empty"), Hero.Resolver.IsActive(EDFStatusChannel::Toxin));
	TestFalse(TEXT("Control empty"), Hero.Resolver.IsActive(EDFStatusChannel::Control));

	// The gate precedes the reaction scan: shock onto a chilled hero is refused, not flashFreeze.
	const FDFStatusApplyOutcome Shock = Hero.Apply(TEXT("shock"));
	TestEqual(TEXT("shock refused on a chilled hero"), Shock.Result, EDFStatusApplyResult::RejectedHero);
	TestTrue(TEXT("no reaction named"), Shock.ReactionId.IsNone());
	TestTrue(TEXT("chill still active"), Hero.Resolver.IsActive(EDFStatusChannel::Movement));
	TestFalse(TEXT("no freeze emitted"), Hero.Resolver.IsActive(EDFStatusChannel::Control));

	// Stagger lands in Control, hard control, and never fills the gauge.
	const FDFStatusApplyOutcome Stagger = Hero.Apply(TEXT("stagger"), 1.f);
	TestEqual(TEXT("stagger lands on a hero"), Stagger.Result, EDFStatusApplyResult::Applied);
	const FDFStatusSlot& Control = Hero.Resolver.Slot(EDFStatusChannel::Control);
	TestEqual(TEXT("Control slot is stagger"), Control.StatusId, FName(TEXT("stagger")));
	TestTrue(TEXT("stagger is hard control"), Control.bHardControl);
	TestTrue(TEXT("hero is controlled while staggered"), Hero.Resolver.IsControlled());
	TestEqual(TEXT("stagger fills no cc-resist"), Control.CcFillScale, 0.f);
	TestEqual(TEXT("0.25 s"), Control.EndTime, 1.25f);
	Hero.Resolver.Tick(1.2f, 0.2f);
	TestEqual(TEXT("gauge still 0 after a staggered tick"), Hero.Resolver.CcResist, 0.f);
	Hero.Resolver.Tick(1.3f, 0.1f);
	TestFalse(TEXT("stagger expired"), Hero.Resolver.IsActive(EDFStatusChannel::Control));

	// ... and ignores the gauge: a full gauge (set by hand — nothing fills it on a hero) still staggers.
	DFTestRows::FResolverFixture Full;
	Full.Target.bHero = true;
	Full.Resolver.CcResist = 1.f;
	TestEqual(TEXT("stagger lands at a full gauge"), Full.Apply(TEXT("stagger")).Result, EDFStatusApplyResult::Applied);

	// Enemies are untouched by the gate.
	DFTestRows::FResolverFixture Enemy;
	TestEqual(TEXT("burn lands on an enemy"), Enemy.Apply(TEXT("burn")).Result, EDFStatusApplyResult::Applied);
	TestEqual(TEXT("stagger on an enemy fills the gauge like any hard control"), Enemy.Apply(TEXT("stagger")).Result, EDFStatusApplyResult::Applied);
	TestEqual(TEXT("enemy stagger CcFillScale 1"), Enemy.Resolver.Slot(EDFStatusChannel::Control).CcFillScale, 1.f);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
