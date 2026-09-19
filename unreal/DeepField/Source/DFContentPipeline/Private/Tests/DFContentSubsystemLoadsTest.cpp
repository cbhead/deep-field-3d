#include "Content/DFContentRows.h"
#include "Content/DFContentSubsystem.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

// The committed tables load through the one door gameplay uses, and a few known numbers come out the other
// side. UDFContentSubsystem is a GameInstance subsystem and a headless editor test has no game instance, so
// this instantiates it directly and runs the same LoadTables() that Initialize() runs.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDFContentSubsystemLoadsTest, "DF.Unit.Content.SubsystemLoads", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FDFContentSubsystemLoadsTest::RunTest(const FString& Parameters)
{
	UDFContentSubsystem* Content = NewObject<UDFContentSubsystem>(GetTransientPackage());
	TestTrue(TEXT("LoadTables() finds every table"), Content->LoadTables());
	TestTrue(TEXT("IsReady() after LoadTables()"), Content->IsReady());
	if (!Content->IsReady())
	{
		return false;
	}

	const FDFTowerRow* Lance = Content->Tower(TEXT("lance"));
	if (TestNotNull(TEXT("Tower(lance)"), Lance))
	{
		TestEqual(TEXT("Tower(lance)->Cost"), Lance->Cost, 75);
	}

	const FDFEnemyRow* Ram = Content->Enemy(TEXT("ram"));
	if (TestNotNull(TEXT("Enemy(ram)"), Ram))
	{
		TestEqual(TEXT("Enemy(ram)->EnrageSpeedFactor"), Ram->EnrageSpeedFactor, 1.6f, 1e-4f);
	}

	TestEqual(TEXT("Balance(startingMoney)"), Content->Balance(TEXT("startingMoney")), 250.f, 1e-4f);
	TestEqual(TEXT("Waves(foundry).Num()"), Content->Waves(TEXT("foundry")).Num(), 22);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
