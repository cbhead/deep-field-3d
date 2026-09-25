#include "DFGameplayTags.h"
#include "GameFramework/PlayerState.h"
#include "GameplayTagContainer.h"
#include "Hero/DFHeroLife.h"
#include "Hero/DFHeroStateComponent.h"
#include "Misc/AutomationTest.h"
#include "Testing/DFTestUtils.h"

#if WITH_DEV_AUTOMATION_TESTS

// DF.Unit.Player.Life* — DFHeroLife against the sim's hero half (Step.cs ApplyRevive, the downed and
// respawn branch of the player update, RespawnPlayer, UpdateWaves' wave-boundary respawn) and B§1.14.
// Steps are 0.25 s, which binary floating point adds exactly, so "revived at 4 s" is an exact count;
// one test also runs at the sim's 30 Hz. DF.Unit.Player.HeroStateHostFlow drives the component the
// way the host will, in a test world (which never ticks it, so the test steps it with HostAdvance).

namespace DFHeroLifeTest
{
	constexpr EAutomationTestFlags Flags = EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter;
	constexpr float Tol = 1e-4f;
	constexpr float Dt = 0.25f;

	/** Steps of Dt until Pred holds, or MaxSteps. */
	template <typename TPred>
	int32 StepUntil(FDFHeroLifeState& State, const FDFHeroLifeRules& Rules, bool bHeld, int32 MaxSteps, TPred Pred)
	{
		for (int32 Step = 1; Step <= MaxSteps; ++Step)
		{
			DFHeroLife::Tick(State, Rules, Dt, bHeld);
			if (Pred(State))
			{
				return Step;
			}
		}
		return INDEX_NONE;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDFHeroLifeRulesTest, "DF.Unit.Player.LifeRulesMatchSim", DFHeroLifeTest::Flags)
bool FDFHeroLifeRulesTest::RunTest(const FString&)
{
	using namespace DFHeroLifeTest;
	// Balance.cs: ReviveSeconds 4, BleedoutSeconds 30, SoloRespawnSeconds 10, ReviveRangeMeters 2.5;
	// Step.cs: decay Dt × 0.5, revived at PlayerMaxHp × 0.5.
	const FDFHeroLifeRules Rules;
	TestEqual(TEXT("revive 4 s"), Rules.ReviveSeconds, 4.f, Tol);
	TestEqual(TEXT("bleedout 30 s"), Rules.BleedoutSeconds, 30.f, Tol);
	TestEqual(TEXT("solo respawn 10 s"), Rules.SoloRespawnSeconds, 10.f, Tol);
	TestEqual(TEXT("revive range 2.5 m"), Rules.ReviveRangeCm, 2.5f * 100.f, Tol);
	TestEqual(TEXT("decay at half rate"), Rules.ReviveDecayRate, 0.5f, Tol);
	TestEqual(TEXT("revived at half health"), Rules.RevivedHpFraction, 0.5f, Tol);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDFHeroLifeDepleteTest, "DF.Unit.Player.LifeDeplete", DFHeroLifeTest::Flags)
bool FDFHeroLifeDepleteTest::RunTest(const FString&)
{
	using namespace DFHeroLifeTest;
	const FDFHeroLifeRules Rules;

	FDFHeroLifeState Team;
	TestTrue(TEXT("two players: downed"), DFHeroLife::Deplete(Team, Rules, 2) == EDFHeroDown::Downed);
	TestTrue(TEXT("downed"), Team.Life == EDFHeroLife::Downed);
	TestEqual(TEXT("bleedout starts full"), Team.BleedoutLeft, 30.f, Tol);
	TestTrue(TEXT("bleeding"), DFHeroLife::IsBleeding(Team));
	TestFalse(TEXT("not up"), DFHeroLife::IsUp(Team));
	TestTrue(TEXT("already down: ignored"), DFHeroLife::Deplete(Team, Rules, 2) == EDFHeroDown::Ignored);

	FDFHeroLifeState Solo;
	TestTrue(TEXT("alone: solo respawn"), DFHeroLife::Deplete(Solo, Rules, 1) == EDFHeroDown::SoloRespawn);
	TestTrue(TEXT("respawning"), Solo.Life == EDFHeroLife::Respawning);
	TestEqual(TEXT("respawn timer starts full"), Solo.RespawnLeft, 10.f, Tol);
	TestFalse(TEXT("a solo hero is not bleeding"), DFHeroLife::IsBleeding(Solo));
	TestTrue(TEXT("respawning: ignored"), DFHeroLife::Deplete(Solo, Rules, 1) == EDFHeroDown::Ignored);

	FDFHeroLifeState Nobody;
	TestTrue(TEXT("no one connected counts as alone"), DFHeroLife::Deplete(Nobody, Rules, 0) == EDFHeroDown::SoloRespawn);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDFHeroLifeReviveTest, "DF.Unit.Player.LifeReviveServerTimed", DFHeroLifeTest::Flags)
bool FDFHeroLifeReviveTest::RunTest(const FString&)
{
	using namespace DFHeroLifeTest;
	const FDFHeroLifeRules Rules;

	FDFHeroLifeState Hero;
	DFHeroLife::Deplete(Hero, Rules, 2);
	const int32 Steps = StepUntil(Hero, Rules, /*bHeld*/ true, 100, [](const FDFHeroLifeState& S) { return S.Life == EDFHeroLife::Up; });
	TestEqual(TEXT("a held revive takes exactly 4 s"), Steps, 16);
	TestEqual(TEXT("revive progress cleared"), Hero.ReviveProgress, 0.f, Tol);
	TestEqual(TEXT("bleedout cleared"), Hero.BleedoutLeft, 0.f, Tol);

	// At the sim's 30 Hz the float sum of 1/30 lands within a tick of 4 s.
	FDFHeroLifeState AtThirty;
	DFHeroLife::Deplete(AtThirty, Rules, 2);
	int32 Ticks = 0;
	bool bRevived = false;
	while (!bRevived && Ticks < 200)
	{
		bRevived = DFHeroLife::Tick(AtThirty, Rules, 1.f / 30.f, true).bRevived;
		++Ticks;
	}
	TestTrue(TEXT("at 30 Hz: revived within a tick of 4 s"), bRevived && Ticks >= 120 && Ticks <= 121);

	// B§1.14's change from the sim: while held there is no decay, so progress is server time.
	FDFHeroLifeState Held;
	DFHeroLife::Deplete(Held, Rules, 2);
	StepUntil(Held, Rules, true, 8, [](const FDFHeroLifeState&) { return false; });
	TestEqual(TEXT("2 s held is 2 s of progress"), Held.ReviveProgress, 2.f, Tol);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDFHeroLifeDecayTest, "DF.Unit.Player.LifeReviveDecay", DFHeroLifeTest::Flags)
bool FDFHeroLifeDecayTest::RunTest(const FString&)
{
	using namespace DFHeroLifeTest;
	const FDFHeroLifeRules Rules;

	FDFHeroLifeState Hero;
	DFHeroLife::Deplete(Hero, Rules, 2);
	StepUntil(Hero, Rules, true, 8, [](const FDFHeroLifeState&) { return false; });    // 2 s held
	StepUntil(Hero, Rules, false, 8, [](const FDFHeroLifeState&) { return false; });   // 2 s released
	TestEqual(TEXT("released, progress decays at half rate"), Hero.ReviveProgress, 1.f, Tol);
	StepUntil(Hero, Rules, false, 16, [](const FDFHeroLifeState&) { return false; });  // 4 s more
	TestEqual(TEXT("and stops at zero"), Hero.ReviveProgress, 0.f, Tol);
	TestTrue(TEXT("still down"), Hero.Life == EDFHeroLife::Downed);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDFHeroLifeBleedoutTest, "DF.Unit.Player.LifeBleedoutAndWaveBoundary", DFHeroLifeTest::Flags)
bool FDFHeroLifeBleedoutTest::RunTest(const FString&)
{
	using namespace DFHeroLifeTest;
	const FDFHeroLifeRules Rules;

	FDFHeroLifeState Hero;
	DFHeroLife::Deplete(Hero, Rules, 2);
	StepUntil(Hero, Rules, false, 119, [](const FDFHeroLifeState&) { return false; });   // 29.75 s
	TestTrue(TEXT("still bleeding at 29.75 s"), DFHeroLife::IsBleeding(Hero));
	TestFalse(TEXT("not yet for the wave boundary"), DFHeroLife::ShouldRespawnAtWaveBoundary(Hero));

	DFHeroLife::Tick(Hero, Rules, Dt, false);   // 30 s
	TestEqual(TEXT("bled out"), Hero.BleedoutLeft, 0.f, Tol);
	TestTrue(TEXT("a bled-out hero stays down (Step.cs)"), Hero.Life == EDFHeroLife::Downed);
	TestFalse(TEXT("no longer bleeding"), DFHeroLife::IsBleeding(Hero));
	TestTrue(TEXT("respawns at the wave boundary"), DFHeroLife::ShouldRespawnAtWaveBoundary(Hero));

	StepUntil(Hero, Rules, false, 40, [](const FDFHeroLifeState&) { return false; });
	TestTrue(TEXT("and waits there, down"), Hero.Life == EDFHeroLife::Downed && Hero.BleedoutLeft == 0.f);

	// Step.cs ApplyRevive only asks Downed, so a bled-out hero can still be revived before the boundary.
	const int32 Steps = StepUntil(Hero, Rules, true, 100, [](const FDFHeroLifeState& S) { return S.Life == EDFHeroLife::Up; });
	TestEqual(TEXT("a bled-out hero can still be revived"), Steps, 16);

	FDFHeroLifeState Other;
	DFHeroLife::Deplete(Other, Rules, 2);
	DFHeroLife::Respawn(Other);
	TestTrue(TEXT("respawn: up with every timer cleared"), Other.Life == EDFHeroLife::Up && Other.BleedoutLeft == 0.f && Other.RespawnLeft == 0.f && Other.ReviveProgress == 0.f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDFHeroLifeSoloTest, "DF.Unit.Player.LifeSoloRespawn", DFHeroLifeTest::Flags)
bool FDFHeroLifeSoloTest::RunTest(const FString&)
{
	using namespace DFHeroLifeTest;
	const FDFHeroLifeRules Rules;

	FDFHeroLifeState Hero;
	DFHeroLife::Deplete(Hero, Rules, 1);
	int32 RespawnedAt = INDEX_NONE;
	for (int32 Step = 1; Step <= 100 && RespawnedAt == INDEX_NONE; ++Step)
	{
		// A hold does nothing to a solo hero: no one can be reviving them.
		if (DFHeroLife::Tick(Hero, Rules, Dt, /*bHeld*/ true).bRespawned)
		{
			RespawnedAt = Step;
		}
		TestFalse(TEXT("never revived"), Hero.Life == EDFHeroLife::Downed);
	}
	TestEqual(TEXT("respawned at 10 s"), RespawnedAt, 40);
	TestTrue(TEXT("up"), Hero.Life == EDFHeroLife::Up);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDFHeroLifeRangeTest, "DF.Unit.Player.LifeReviveRange", DFHeroLifeTest::Flags)
bool FDFHeroLifeRangeTest::RunTest(const FString&)
{
	const FDFHeroLifeRules Rules;
	TestTrue(TEXT("touching"), DFHeroLife::InReviveRange(0.f, Rules));
	TestTrue(TEXT("exactly 2.5 m (the sim refuses only beyond it)"), DFHeroLife::InReviveRange(250.f, Rules));
	TestFalse(TEXT("beyond 2.5 m"), DFHeroLife::InReviveRange(250.5f, Rules));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDFHeroRegenTest, "DF.Unit.Player.RegenMatchesSim", DFHeroLifeTest::Flags)
bool FDFHeroRegenTest::RunTest(const FString&)
{
	using namespace DFHeroLifeTest;
	// Balance.cs: PlayerRegenDelaySeconds 6, PlayerRegenPerSecond 10, PlayerMaxHp 100.
	FDFHeroRegen Regen;
	float Health = 50.f;
	DFHeroLife::NoteDamaged(Regen, 6.f);
	for (int32 Step = 1; Step <= 23; ++Step)
	{
		Health = DFHeroLife::RegenStep(Regen, Health, 100.f, 10.f, Dt);
	}
	TestEqual(TEXT("nothing for the first 5.75 s"), Health, 50.f, Tol);
	Health = DFHeroLife::RegenStep(Regen, Health, 100.f, 10.f, Dt);
	TestEqual(TEXT("regen starts on the step the delay ends (Step.cs)"), Health, 52.5f, Tol);
	for (int32 Step = 1; Step <= 4; ++Step)
	{
		Health = DFHeroLife::RegenStep(Regen, Health, 100.f, 10.f, Dt);
	}
	TestEqual(TEXT("10 hp per second"), Health, 62.5f, Tol);

	DFHeroLife::NoteDamaged(Regen, 6.f);
	Health = DFHeroLife::RegenStep(Regen, Health, 100.f, 10.f, Dt);
	TestEqual(TEXT("damage restarts the delay"), Health, 62.5f, Tol);

	FDFHeroRegen Quiet;
	TestEqual(TEXT("capped at max"), DFHeroLife::RegenStep(Quiet, 99.f, 100.f, 10.f, Dt), 100.f, Tol);
	FDFHeroRegen AtZero;
	TestEqual(TEXT("no regen from 0: getting up is a revive or a respawn"), DFHeroLife::RegenStep(AtZero, 0.f, 100.f, 10.f, Dt), 0.f, Tol);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDFHeroStateHostFlowTest, "DF.Unit.Player.HeroStateHostFlow", DFHeroLifeTest::Flags)
bool FDFHeroStateHostFlowTest::RunTest(const FString&)
{
	using namespace DFHeroLifeTest;

	FDFTestWorld World;
	APlayerState* PlayerA = World.SpawnActor<APlayerState>();
	APlayerState* PlayerB = World.SpawnActor<APlayerState>();
	APlayerState* PlayerC = World.SpawnActor<APlayerState>();
	if (!TestNotNull(TEXT("player A"), PlayerA) || !TestNotNull(TEXT("player B"), PlayerB) || !TestNotNull(TEXT("player C"), PlayerC))
	{
		return false;
	}
	auto Attach = [](APlayerState* Owner)
	{
		UDFHeroStateComponent* State = NewObject<UDFHeroStateComponent>(Owner);
		State->RegisterComponent();
		return State;
	};
	UDFHeroStateComponent* A = Attach(PlayerA);
	UDFHeroStateComponent* B = Attach(PlayerB);
	UDFHeroStateComponent* C = Attach(PlayerC);
	TestTrue(TEXT("Find on the player state"), UDFHeroStateComponent::Find(PlayerA) == A);

	struct FSeen
	{
		EDFHeroLifeEvent Event;
		UDFHeroStateComponent* By;
	};
	TArray<FSeen> Seen;
	A->OnHeroLifeEvent.AddLambda([&Seen](EDFHeroLifeEvent Event, UDFHeroStateComponent* By) { Seen.Add(FSeen{ Event, By }); });

	const FDFHeroLifeRules& Rules = A->GetRules();
	auto Advance = [A](float Seconds, bool bHeld)
	{
		for (float T = 0.f; T < Seconds - KINDA_SMALL_NUMBER; T += Dt)
		{
			A->HostAdvance(Dt, bHeld);
		}
	};

	// Seat, then down: the seat is left.
	TestTrue(TEXT("a standing hero takes a seat"), A->HostTakeSeat(PlayerC, 0));
	FGameplayTagContainer Tags;
	A->GetStateTags(Tags);
	TestTrue(TEXT("seated tag"), Tags.HasTagExact(DFTags::Player_State_Seated));

	TestTrue(TEXT("down with teammates"), A->HostDeplete(3) == EDFHeroDown::Downed);
	TestTrue(TEXT("downed"), A->IsDowned());
	TestEqual(TEXT("left the seat"), A->GetSeatIndex(), static_cast<int32>(INDEX_NONE));
	TestTrue(TEXT("event: downed"), Seen.Num() == 1 && Seen[0].Event == EDFHeroLifeEvent::Downed);
	Tags.Reset();
	A->GetStateTags(Tags);
	TestTrue(TEXT("downed and bleeding tags, not seated"), Tags.HasTagExact(DFTags::Player_State_Downed) && Tags.HasTagExact(DFTags::Player_State_Bleeding) && !Tags.HasTagExact(DFTags::Player_State_Seated));
	TestFalse(TEXT("a downed hero cannot take a seat"), A->HostTakeSeat(PlayerC, 0));
	float Bleedout = 0.f;
	TestTrue(TEXT("the host knows its own clock"), A->TryGetBleedoutSecondsLeft(Bleedout));
	TestEqual(TEXT("bleedout full"), Bleedout, Rules.BleedoutSeconds, Tol);

	// One reviver (B§1.14).
	TestTrue(TEXT("B starts reviving"), A->HostBeginRevive(B));
	TestTrue(TEXT("the reviver is B's player state"), A->GetReviver() == PlayerB);
	TestFalse(TEXT("C cannot join in"), A->HostBeginRevive(C));
	TestFalse(TEXT("nobody revives a standing hero"), B->HostBeginRevive(C));
	A->HostEndRevive(B);
	TestTrue(TEXT("after B lets go, C can"), A->HostBeginRevive(C));

	const int32 Steps = FMath::CeilToInt(Rules.ReviveSeconds / Dt);
	for (int32 Step = 1; Step < Steps; ++Step)
	{
		A->HostAdvance(Dt, true);
	}
	TestTrue(TEXT("still down a step before ReviveSeconds"), A->IsDowned());
	TestTrue(TEXT("progress is visible"), A->GetReviveFraction() > 0.9f);
	A->HostAdvance(Dt, true);
	TestTrue(TEXT("revived at ReviveSeconds"), A->IsUp());
	TestTrue(TEXT("event: revived by C"), Seen.Num() == 2 && Seen[1].Event == EDFHeroLifeEvent::Revived && Seen[1].By == C);
	TestTrue(TEXT("no reviver afterwards"), A->GetReviver() == nullptr);

	// Alone: the solo timer, then a respawn.
	TestTrue(TEXT("alone: solo respawn"), A->HostDeplete(1) == EDFHeroDown::SoloRespawn);
	TestTrue(TEXT("event: solo downed"), Seen.Num() == 3 && Seen[2].Event == EDFHeroLifeEvent::SoloDowned);
	float Respawn = 0.f;
	TestTrue(TEXT("respawn countdown known"), A->TryGetRespawnSecondsLeft(Respawn));
	TestEqual(TEXT("respawn countdown full"), Respawn, Rules.SoloRespawnSeconds, Tol);
	Advance(Rules.SoloRespawnSeconds, false);
	TestTrue(TEXT("respawned"), A->IsUp());
	TestTrue(TEXT("event: respawned"), Seen.Num() == 4 && Seen[3].Event == EDFHeroLifeEvent::Respawned);

	// Bled out: waits for WS-28's wave-boundary respawn.
	A->HostDeplete(2);
	TestFalse(TEXT("bleeding: not for the boundary yet"), A->ShouldRespawnAtWaveBoundary());
	Advance(Rules.BleedoutSeconds, false);
	TestTrue(TEXT("bled out: for the boundary"), A->ShouldRespawnAtWaveBoundary());
	TestTrue(TEXT("still down"), A->IsDowned() && !A->IsBleeding());
	A->HostRespawn();
	TestTrue(TEXT("respawned at the boundary"), A->IsUp());
	TestTrue(TEXT("event: respawned"), Seen.Num() == 6 && Seen[5].Event == EDFHeroLifeEvent::Respawned);
	A->HostRespawn();
	TestEqual(TEXT("respawning a standing hero does nothing"), Seen.Num(), 6);

	// The hero passes its UDFHeroSet BleedoutSeconds, which passives may have scaled.
	A->HostDeplete(2, 45.f);
	TestTrue(TEXT("bleedout override: bleedout left"), A->TryGetBleedoutSecondsLeft(Bleedout));
	TestEqual(TEXT("bleedout override: 45 s"), Bleedout, 45.f, Tol);

	A->OnHeroLifeEvent.Clear();   // Seen goes out of scope before the world tears A down
	return true;
}

#endif
