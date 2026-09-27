#include "DFGameplayTags.h"
#include "Economy/DFEconomyStateComponent.h"
#include "GameFramework/Actor.h"
#include "Messages/DFMessageBus.h"
#include "Messages/DFMessages.h"
#include "Misc/AutomationTest.h"
#include "Testing/DFTestUtils.h"

#if WITH_DEV_AUTOMATION_TESTS

// DF.Unit.Economy.* — UDFEconomyStateComponent on a host actor in a standalone game world: the
// starting purse, leaks and kills heard on the message bus, and the wallet's all-or-nothing spend.
namespace DFEconomyTest
{
	constexpr EAutomationTestFlags Flags = EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter;

	UDFEconomyStateComponent* MakeEconomy(FDFTestWorld& World)
	{
		AActor* Host = World.SpawnActor<AActor>();
		if (!Host)
		{
			return nullptr;
		}
		UDFEconomyStateComponent* Economy = NewObject<UDFEconomyStateComponent>(Host);
		Economy->RegisterComponent();   // the actor has begun play, so this runs the component's BeginPlay
		return Economy;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDFEconomyStartsFromBalanceTest, "DF.Unit.Economy.StartsFromBalance", DFEconomyTest::Flags)
bool FDFEconomyStartsFromBalanceTest::RunTest(const FString& Parameters)
{
	FDFTestWorld World;
	UDFEconomyStateComponent* Economy = DFEconomyTest::MakeEconomy(World);
	if (!TestNotNull(TEXT("economy"), Economy))
	{
		return false;
	}
	// balance.json: startingMoney 250, startingLives 20 (the component's fallbacks say the same).
	TestEqual(TEXT("starting money"), Economy->GetMoney(), 250);
	TestEqual(TEXT("starting lives"), Economy->GetLives(), 20);
	TestEqual(TEXT("no team scrap yet"), Economy->GetTeamScrap().Num(), 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDFEconomyLeaksAndKillsTest, "DF.Unit.Economy.LeaksAndKills", DFEconomyTest::Flags)
bool FDFEconomyLeaksAndKillsTest::RunTest(const FString& Parameters)
{
	FDFTestWorld World;
	UDFEconomyStateComponent* Economy = DFEconomyTest::MakeEconomy(World);
	UDFMessageBus* Bus = World.MessageBus();
	if (!TestNotNull(TEXT("economy"), Economy) || !TestNotNull(TEXT("bus"), Bus))
	{
		return false;
	}
	Economy->Reset(100, 5);
	int32 Changes = 0;
	Economy->OnEconomyChanged.AddLambda([&Changes](UDFEconomyStateComponent*) { ++Changes; });

	FDFMsg_Enemy Leak;
	Leak.DefId = TEXT("drifter");
	Leak.Phase = 2;   // the leak damage
	Bus->Broadcast(DFTags::Message_EnemyLeaked, Leak);
	TestEqual(TEXT("a leak takes its lives at once (before the director hears the body is gone)"), Economy->GetLives(), 3);

	FDFMsg_Kill Kill;
	Kill.DefId = TEXT("drifter");
	Kill.Bounty = 7;
	Bus->Broadcast(DFTags::Message_EnemyKilled, Kill);
	TestEqual(TEXT("a kill pays its bounty"), Economy->GetMoney(), 107);

	Leak.Phase = 10;
	Bus->Broadcast(DFTags::Message_EnemyLeaked, Leak);
	TestEqual(TEXT("lives stop at zero"), Economy->GetLives(), 0);
	Bus->Broadcast(DFTags::Message_EnemyLeaked, Leak);
	TestEqual(TEXT("and stay there"), Economy->GetLives(), 0);
	TestEqual(TEXT("every change was announced, the no-op leak was not"), Changes, 3);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDFEconomySpendIsAllOrNothingTest, "DF.Unit.Economy.SpendIsAllOrNothing", DFEconomyTest::Flags)
bool FDFEconomySpendIsAllOrNothingTest::RunTest(const FString& Parameters)
{
	FDFTestWorld World;
	UDFEconomyStateComponent* Economy = DFEconomyTest::MakeEconomy(World);
	if (!TestNotNull(TEXT("economy"), Economy))
	{
		return false;
	}
	Economy->Reset(100, 20);
	TestTrue(TEXT("75 of 100"), Economy->TrySpend(75, nullptr));
	TestEqual(TEXT("25 left"), Economy->GetMoney(), 25);
	TestFalse(TEXT("75 of 25 is refused"), Economy->TrySpend(75, nullptr));
	TestEqual(TEXT("and takes nothing"), Economy->GetMoney(), 25);

	FDFScrapBundle Scrap;
	Scrap.Amounts.Add(EDFScrapType::Gravium, 1);
	TestFalse(TEXT("scrap the pool does not have refuses the whole spend"), Economy->TrySpend(10, &Scrap));
	TestEqual(TEXT("the money stays"), Economy->GetMoney(), 25);

	Economy->AddMoney(50);
	TestEqual(TEXT("a refund comes back"), Economy->GetMoney(), 75);
	return true;
}

#endif
