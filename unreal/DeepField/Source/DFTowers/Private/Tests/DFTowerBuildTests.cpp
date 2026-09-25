#include "Content/DFContentSubsystem.h"
#include "DFGameplayTags.h"
#include "DFTowerTestDummy.h"
#include "DFWorldSubsystem.h"
#include "Messages/DFMessages.h"
#include "Misc/AutomationTest.h"
#include "Testing/DFTestUtils.h"
#include "Towers/DFBuildSubsystem.h"
#include "Towers/DFTower.h"
#include "Towers/DFTowerMath.h"
#include "World/DFSocket.h"

#if WITH_DEV_AUTOMATION_TESTS

// DF.Unit.Tower.Build* — UDFBuildSubsystem against Step.cs ApplyPlaceTower / ApplyUpgradeTower /
// ApplySellTower. Numbers are towers.json's: lance costs 75; each lance path costs 40, 54, 73 for its
// first three purchases, and arriving at L4 on "damage" also takes 8 Alloy + 2 Plating; a barricade
// costs 60; selling returns sellRefundPercent (70) of everything spent, integer division.

namespace DFTowerBuildTest
{
	constexpr EAutomationTestFlags Flags = EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter;

	/** A test world with the content tables, a purse, and four pads: g1 (Ground), w1 (Wall), b1 (Barricade), t1 (Trap). */
	struct FBuildFixture
	{
		FDFTestWorld World;
		UDFTowerTestWallet* Wallet = nullptr;
		UDFBuildSubsystem* Build = nullptr;

		bool Setup(FAutomationTestBase& Test)
		{
			UDFContentSubsystem* Content = World.GetSubsystem<UDFContentSubsystem>();
			if (!Test.TestNotNull(TEXT("content subsystem"), Content) || !(Content->IsReady() || Test.TestTrue(TEXT("tables load"), Content->LoadTables())))
			{
				return false;
			}
			Build = UDFBuildSubsystem::Get(World.GetWorld());
			if (!Test.TestNotNull(TEXT("build subsystem"), Build))
			{
				return false;
			}
			Wallet = NewObject<UDFTowerTestWallet>();
			Wallet->AddToRoot();
			Build->SetWalletOverride(Wallet);

			struct FPad { const TCHAR* Id; EDFSocketTag Tag; };
			const FPad Pads[] = {
				{ TEXT("g1"), EDFSocketTag::Ground }, { TEXT("w1"), EDFSocketTag::Wall },
				{ TEXT("b1"), EDFSocketTag::Barricade }, { TEXT("t1"), EDFSocketTag::Trap } };
			float X = 1000.f;
			for (const FPad& Pad : Pads)
			{
				ADFSocket* Socket = World.SpawnActor<ADFSocket>(FTransform(FVector(X, 0.f, 0.f)));
				if (!Test.TestNotNull(TEXT("socket"), Socket))
				{
					return false;
				}
				Socket->SocketId = Pad.Id;
				Socket->Tag = Pad.Tag;
				X += 500.f;
			}
			UDFWorldSubsystem::Get(World.GetWorld())->Invalidate();   // pick up the pads' ids
			return true;
		}

		~FBuildFixture()
		{
			if (Wallet)
			{
				Wallet->RemoveFromRoot();
			}
		}
	};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDFTowerBuildPlaceTest, "DF.Unit.Tower.BuildPlaceInSimOrder", DFTowerBuildTest::Flags)
bool FDFTowerBuildPlaceTest::RunTest(const FString& Parameters)
{
	using namespace DFTowerMath;
	DFTowerBuildTest::FBuildFixture F;
	if (!F.Setup(*this))
	{
		return false;
	}
	FDFMessageCapture Placed(F.World.MessageBus(), DFTags::Message_TowerPlaced);
	F.Wallet->Money = 100;

	TestEqual(TEXT("a socket that does not exist"), F.Build->PlaceTower(1, TEXT("lance"), TEXT("nowhere")).Reason, Reasons::UnknownSocket);
	TestEqual(TEXT("a tower that does not exist"), F.Build->PlaceTower(1, TEXT("bogus"), TEXT("g1")).Reason, Reasons::UnknownTower);
	TestEqual(TEXT("a tower on a barricade pad"), F.Build->PlaceTower(1, TEXT("lance"), TEXT("b1")).Reason, Reasons::WrongSocketTag);
	TestEqual(TEXT("a tower on a trap pad is named as such"), F.Build->PlaceTower(1, TEXT("lance"), TEXT("t1")).Reason, Reasons::TrapSocket);
	TestEqual(TEXT("a barricade on a ground pad"), F.Build->PlaceTower(1, TEXT("barricade"), TEXT("g1")).Reason, Reasons::WrongSocketTag);

	F.Wallet->Money = 50;
	TestEqual(TEXT("50 does not buy a 75 lance"), F.Build->PlaceTower(1, TEXT("lance"), TEXT("g1")).Reason, Reasons::InsufficientFunds);
	TestEqual(TEXT("and a refusal costs nothing"), F.Wallet->Money, 50);
	TestEqual(TEXT("and announces nothing"), Placed.Num(), 0);

	F.Wallet->Money = 100;
	const FDFBuildResult Built = F.Build->PlaceTower(2, TEXT("lance"), TEXT("g1"));
	if (!TestTrue(TEXT("100 buys the lance"), Built.Succeeded()) || !TestNotNull(TEXT("the tower"), Built.Tower))
	{
		return false;
	}
	TestEqual(TEXT("75 was taken"), F.Wallet->Money, 25);
	TestEqual(TEXT("the tower remembers what it cost"), Built.Tower->GetSpent(), 75);
	TestEqual(TEXT("built by seat 2"), Built.Tower->GetOwnerSeat(), 2);
	TestEqual(TEXT("it stands on the pad's top"), Built.Tower->GetActorLocation(), FVector(1000.f, 0.f, 2.f * ADFSocket::PadHalfHeightCm));
	TestTrue(TEXT("found by socket"), F.Build->FindTowerOnSocket(TEXT("g1")) == Built.Tower);
	TestTrue(TEXT("found by structure id"), F.Build->FindTower(Built.Tower->GetStructureId()) == Built.Tower);
	if (TestEqual(TEXT("one TowerPlaced to the team"), Placed.Num(), 1))
	{
		const FDFMsg_Structure* Message = Placed.LastPayload<FDFMsg_Structure>();
		if (TestNotNull(TEXT("an FDFMsg_Structure"), Message))
		{
			TestEqual(TEXT("its structure id"), Message->StructureId, Built.Tower->GetStructureId());
			TestEqual(TEXT("its placer"), Message->PlayerId, 2);
			TestEqual(TEXT("its def"), Message->DefId, FName(TEXT("lance")));
			TestEqual(TEXT("its socket"), Message->SocketId, FName(TEXT("g1")));
		}
	}

	F.Wallet->Money = 1000;
	TestEqual(TEXT("a second tower on the same pad"), F.Build->PlaceTower(1, TEXT("lance"), TEXT("g1")).Reason, Reasons::Occupied);
	TestTrue(TEXT("a wall pad takes a tower"), F.Build->PlaceTower(1, TEXT("lance"), TEXT("w1")).Succeeded());
	// b1 has no lane gate in this world, so there is nothing for it to seal.
	TestTrue(TEXT("a barricade pad takes a barricade"), F.Build->PlaceTower(1, TEXT("barricade"), TEXT("b1")).Succeeded());
	TestEqual(TEXT("75 + 60 more"), F.Wallet->Money, 1000 - 75 - 60);
	TestEqual(TEXT("three towers stand"), F.Build->GetTowers().Num(), 3);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDFTowerBuildUpgradeTest, "DF.Unit.Tower.BuildUpgradeAndBreakpointScrap", DFTowerBuildTest::Flags)
bool FDFTowerBuildUpgradeTest::RunTest(const FString& Parameters)
{
	using namespace DFTowerMath;
	DFTowerBuildTest::FBuildFixture F;
	if (!F.Setup(*this))
	{
		return false;
	}
	FDFMessageCapture Upgraded(F.World.MessageBus(), DFTags::Message_TowerUpgraded);
	F.Wallet->Money = 1000;
	const FDFBuildResult Built = F.Build->PlaceTower(1, TEXT("lance"), TEXT("g1"));
	if (!TestTrue(TEXT("lance placed"), Built.Succeeded()))
	{
		return false;
	}
	ADFTower* Tower = Built.Tower;
	const int32 Id = Tower->GetStructureId();

	TestEqual(TEXT("an unknown tower"), F.Build->UpgradeTower(1, Id + 1000, 0).Reason, Reasons::UnknownTower);
	TestEqual(TEXT("a path the lance does not have"), F.Build->UpgradeTower(1, Id, 7).Reason, Reasons::UnknownPath);

	TestTrue(TEXT("L1 -> L2 on damage"), F.Build->UpgradeTower(1, Id, 0).Succeeded());
	TestTrue(TEXT("L2 -> L3 on damage"), F.Build->UpgradeTower(1, Id, 0).Succeeded());
	TestEqual(TEXT("40 + 54 taken"), F.Wallet->Money, 1000 - 75 - 40 - 54);
	TestEqual(TEXT("two purchases on damage"), Tower->GetPathLevels()[0], 2);

	// L4 is a breakpoint: 73 money and 8 Alloy + 2 Plating from the team pool.
	const int32 MoneyBefore = F.Wallet->Money;
	TestEqual(TEXT("no scrap: refused"), F.Build->UpgradeTower(1, Id, 0).Reason, Reasons::InsufficientScrap);
	TestEqual(TEXT("and nothing was taken"), F.Wallet->Money, MoneyBefore);
	TestEqual(TEXT("and nothing was bought"), Tower->GetPathLevels()[0], 2);

	F.Wallet->Scrap.Add(EDFScrapType::Alloy, 8);
	F.Wallet->Scrap.Add(EDFScrapType::Plating, 2);
	TestTrue(TEXT("with the recipe in the pool: L3 -> L4"), F.Build->UpgradeTower(1, Id, 0).Succeeded());
	TestEqual(TEXT("73 taken"), F.Wallet->Money, MoneyBefore - 73);
	TestEqual(TEXT("the Alloy spent"), F.Wallet->Scrap.FindRef(EDFScrapType::Alloy), 0);
	TestEqual(TEXT("the Plating spent"), F.Wallet->Scrap.FindRef(EDFScrapType::Plating), 0);
	TestEqual(TEXT("everything spent is on the tower"), Tower->GetSpent(), 75 + 40 + 54 + 73);

	if (TestEqual(TEXT("three TowerUpgraded to the team"), Upgraded.Num(), 3))
	{
		const FDFMsg_Upgrade* Last = Upgraded.LastPayload<FDFMsg_Upgrade>();
		if (TestNotNull(TEXT("an FDFMsg_Upgrade"), Last))
		{
			TestEqual(TEXT("the path"), Last->PathId, FName(TEXT("damage")));
			TestEqual(TEXT("the level the player now sees"), Last->NewLevel, 4);
			TestTrue(TEXT("a breakpoint"), Last->bBreakpoint);
		}
		const FDFMsg_Upgrade* First = Upgraded.PayloadAt<FDFMsg_Upgrade>(0);
		if (TestNotNull(TEXT("the first upgrade"), First))
		{
			TestEqual(TEXT("L2"), First->NewLevel, 2);
			TestFalse(TEXT("not a breakpoint"), First->bBreakpoint);
		}
	}

	F.Wallet->Money = 0;
	TestEqual(TEXT("no money: refused"), F.Build->UpgradeTower(1, Id, 1).Reason, Reasons::InsufficientFunds);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDFTowerBuildSellTest, "DF.Unit.Tower.BuildSellRefundsSeventyPercent", DFTowerBuildTest::Flags)
bool FDFTowerBuildSellTest::RunTest(const FString& Parameters)
{
	using namespace DFTowerMath;
	DFTowerBuildTest::FBuildFixture F;
	if (!F.Setup(*this))
	{
		return false;
	}
	FDFMessageCapture Sold(F.World.MessageBus(), DFTags::Message_TowerSold);
	F.Wallet->Money = 1000;
	const FDFBuildResult Built = F.Build->PlaceTower(1, TEXT("lance"), TEXT("g1"));
	if (!TestTrue(TEXT("lance placed"), Built.Succeeded()))
	{
		return false;
	}
	const int32 Id = Built.Tower->GetStructureId();
	TestTrue(TEXT("one purchase"), F.Build->UpgradeTower(1, Id, 1).Succeeded());   // 75 + 40 = 115 spent

	TestEqual(TEXT("selling an unknown tower"), F.Build->SellTower(1, Id + 1000).Reason, Reasons::UnknownTower);
	TestEqual(TEXT("is silent, as the sim"), Sold.Num(), 0);

	const int32 MoneyBefore = F.Wallet->Money;
	const FDFBuildResult Sale = F.Build->SellTower(3, Id);
	TestTrue(TEXT("sold"), Sale.Succeeded());
	TestEqual(TEXT("115 x 70 / 100 = 80 (integer division)"), Sale.Refund, 80);
	TestEqual(TEXT("the refund is in the purse"), F.Wallet->Money, MoneyBefore + 80);
	TestNull(TEXT("gone by id"), F.Build->FindTower(Id));
	TestNull(TEXT("the pad is free"), F.Build->FindTowerOnSocket(TEXT("g1")));
	if (TestEqual(TEXT("one TowerSold to the team"), Sold.Num(), 1))
	{
		const FDFMsg_Structure* Message = Sold.LastPayload<FDFMsg_Structure>();
		if (TestNotNull(TEXT("an FDFMsg_Structure"), Message))
		{
			TestEqual(TEXT("the seller"), Message->PlayerId, 3);
			TestEqual(TEXT("the refund"), Message->Refund, 80);
			TestEqual(TEXT("the socket it stood on"), Message->SocketId, FName(TEXT("g1")));
		}
	}
	TestTrue(TEXT("the freed pad takes a new tower"), F.Build->PlaceTower(1, TEXT("lance"), TEXT("g1")).Succeeded());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDFTowerBuildNoWalletTest, "DF.Unit.Tower.BuildWithoutWalletRefuses", DFTowerBuildTest::Flags)
bool FDFTowerBuildNoWalletTest::RunTest(const FString& Parameters)
{
	DFTowerBuildTest::FBuildFixture F;
	if (!F.Setup(*this))
	{
		return false;
	}
	F.Build->SetWalletOverride(nullptr);   // and a test world has no game state to carry one
	AddExpectedError(TEXT("No team wallet"), EAutomationExpectedErrorFlags::Contains, 0);
	TestEqual(TEXT("with no purse, nothing is affordable"), F.Build->PlaceTower(1, TEXT("lance"), TEXT("g1")).Reason, DFTowerMath::Reasons::InsufficientFunds);
	TestEqual(TEXT("and nothing was built"), F.Build->GetTowers().Num(), 0);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
