#include "DFGameplayTags.h"
#include "DFMatchState.h"
#include "DFPlayerState.h"
#include "Hero/DFHeroStateComponent.h"
#include "Messages/DFMessages.h"
#include "Misc/AutomationTest.h"
#include "Testing/DFTestUtils.h"

#if WITH_DEV_AUTOMATION_TESTS

// DF.Unit.Match.* (hero state) — ADFPlayerState hosting WS-03's UDFHeroStateComponent (ADR-0024): its
// host events become PlayerDowned / PlayerRevived / PlayerRespawned on the bus with the right seats, the
// reviver is credited (Step.cs:931), and ADFMatchState respawns only the bled-out heroes at the wave
// boundary (Step.cs UpdateWaves). A test world never ticks actors, so the hero state is stepped with
// HostAdvance and the boundary is called directly (the phase machine calls the same function).

namespace DFMatchHeroStateTest
{
	constexpr EAutomationTestFlags Flags = EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter;
	constexpr float Dt = 0.25f;

	void Advance(UDFHeroStateComponent* Hero, float Seconds, bool bHeld)
	{
		for (float T = 0.f; T < Seconds - KINDA_SMALL_NUMBER; T += Dt)
		{
			Hero->HostAdvance(Dt, bHeld);
		}
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDFMatchPlayerStateHostsHeroStateTest, "DF.Unit.Match.PlayerStateHostsHeroState", DFMatchHeroStateTest::Flags)
bool FDFMatchPlayerStateHostsHeroStateTest::RunTest(const FString&)
{
	TestNotNull(TEXT("every player state carries a hero state"), GetDefault<ADFPlayerState>()->GetHeroState());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDFMatchHeroEventsTest, "DF.Unit.Match.HeroEventsBecomeMessages", DFMatchHeroStateTest::Flags)
bool FDFMatchHeroEventsTest::RunTest(const FString&)
{
	using namespace DFMatchHeroStateTest;

	FDFTestWorld World;
	ADFPlayerState* A = World.SpawnActor<ADFPlayerState>();
	ADFPlayerState* B = World.SpawnActor<ADFPlayerState>();
	if (!TestNotNull(TEXT("player A"), A) || !TestNotNull(TEXT("player B"), B))
	{
		return false;
	}
	A->SetSeat(1);
	B->SetSeat(2);
	UDFHeroStateComponent* HeroA = A->GetHeroState();
	UDFHeroStateComponent* HeroB = B->GetHeroState();
	FDFMessageCapture Seen(World.MessageBus(), DFTags::Message);

	HeroA->HostDeplete(2);
	TestEqual(TEXT("one PlayerDowned"), Seen.CountOf(DFTags::Message_PlayerDowned), 1);
	const FDFMsg_Player* Downed = Seen.LastPayload<FDFMsg_Player>();
	TestTrue(TEXT("PlayerDowned names seat 1"), Downed != nullptr && Downed->PlayerId == 1);

	TestTrue(TEXT("B starts reviving A"), HeroA->HostBeginRevive(HeroB));
	Advance(HeroA, HeroA->GetRules().ReviveSeconds, true);
	TestTrue(TEXT("A is up"), HeroA->IsUp());
	TestEqual(TEXT("one PlayerRevived"), Seen.CountOf(DFTags::Message_PlayerRevived), 1);
	const FDFMsg_Player* Revived = Seen.LastPayload<FDFMsg_Player>();
	TestTrue(TEXT("PlayerRevived: the reviver is seat 2, the target seat 1"), Revived != nullptr && Revived->PlayerId == 2 && Revived->TargetPlayerId == 1);
	TestEqual(TEXT("B is credited the revive"), B->GetRevives(), 1);
	TestEqual(TEXT("B earns the revive XP (Step.cs:931)"), B->GetMatchXp(), ADFPlayerState::ReviveMatchXp);
	TestEqual(TEXT("A is credited nothing"), A->GetRevives(), 0);

	HeroA->HostDeplete(1);
	TestEqual(TEXT("a solo down is also a PlayerDowned"), Seen.CountOf(DFTags::Message_PlayerDowned), 2);
	Advance(HeroA, HeroA->GetRules().SoloRespawnSeconds, false);
	TestTrue(TEXT("A respawned alone"), HeroA->IsUp());
	TestEqual(TEXT("one PlayerRespawned"), Seen.CountOf(DFTags::Message_PlayerRespawned), 1);
	const FDFMsg_Player* Respawned = Seen.LastPayload<FDFMsg_Player>();
	TestTrue(TEXT("PlayerRespawned names seat 1"), Respawned != nullptr && Respawned->PlayerId == 1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDFMatchBoundaryRespawnTest, "DF.Unit.Match.BledOutHeroesRespawnAtBoundary", DFMatchHeroStateTest::Flags)
bool FDFMatchBoundaryRespawnTest::RunTest(const FString&)
{
	using namespace DFMatchHeroStateTest;

	FDFTestWorld World;
	ADFMatchState* Match = World.SpawnActor<ADFMatchState>();
	ADFPlayerState* A = World.SpawnActor<ADFPlayerState>();
	ADFPlayerState* B = World.SpawnActor<ADFPlayerState>();
	if (!TestNotNull(TEXT("match state"), Match) || !TestNotNull(TEXT("player A"), A) || !TestNotNull(TEXT("player B"), B))
	{
		return false;
	}
	A->SetSeat(1);
	B->SetSeat(2);
	// A test world has no game mode, so the player states have not registered with the match state themselves.
	Match->AddPlayerState(A);
	Match->AddPlayerState(B);
	UDFHeroStateComponent* HeroA = A->GetHeroState();
	UDFHeroStateComponent* HeroB = B->GetHeroState();
	FDFMessageCapture Seen(World.MessageBus(), DFTags::Message_PlayerRespawned);

	HeroA->HostDeplete(2);
	Advance(HeroA, HeroA->GetRules().BleedoutSeconds, false);
	TestTrue(TEXT("A bled out"), HeroA->ShouldRespawnAtWaveBoundary());
	HeroB->HostDeplete(2);
	Advance(HeroB, 1.f, false);
	TestFalse(TEXT("B is still bleeding"), HeroB->ShouldRespawnAtWaveBoundary());

	Match->HostRespawnBledOutHeroes();
	TestTrue(TEXT("the bled-out hero respawns at the boundary"), HeroA->IsUp());
	TestTrue(TEXT("a hero still bleeding stays down into the next wave (Step.cs)"), HeroB->IsDowned());
	TestEqual(TEXT("one PlayerRespawned"), Seen.Num(), 1);
	return true;
}

#endif
