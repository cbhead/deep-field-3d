#include "Combat/DFStructure.h"
#include "Combat/DFTargetable.h"
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
#include "Towers/DFTrap.h"
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

// ---- siege and repair (Step.cs SiegeStructures, ApplyPlayerMelee) -------------------------------------
// The Ram: structureDps 14, structureReach 6 m. Lances have 120 structure hp, the barricade 300.

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDFTowerSiegeTest, "DF.Unit.Tower.SiegeBreaksThenRemovesAtFrameEnd", DFTowerBuildTest::Flags)
bool FDFTowerSiegeTest::RunTest(const FString& Parameters)
{
	DFTowerBuildTest::FBuildFixture F;
	if (!F.Setup(*this))
	{
		return false;
	}
	F.Wallet->Money = 1000;
	ADFTower* Ground = F.Build->PlaceTower(1, TEXT("lance"), TEXT("g1")).Tower;      // x 10 m
	ADFTower* Wall = F.Build->PlaceTower(1, TEXT("lance"), TEXT("w1")).Tower;        // x 15 m
	ADFTower* Barricade = F.Build->PlaceTower(1, TEXT("barricade"), TEXT("b1")).Tower;  // x 20 m
	UDFStructureRegistry* Structures = UDFStructureRegistry::Get(F.World.GetWorld());
	if (!TestNotNull(TEXT("towers"), Ground) || !TestNotNull(TEXT("towers"), Wall) || !TestNotNull(TEXT("towers"), Barricade)
		|| !TestNotNull(TEXT("structure registry"), Structures))
	{
		return false;
	}
	const float PadZ = 2.f * ADFSocket::PadHalfHeightCm;
	TestTrue(TEXT("the nearest in reach: 2 m to the wall lance beats 3 m to the ground one"),
		Structures->FindSiegeTarget(FVector(1300.f, 0.f, PadZ), 6.f) == Wall);
	TestNull(TEXT("nothing within 6 m"), Structures->FindSiegeTarget(FVector(10000.f, 0.f, PadZ), 6.f));
	TestFalse(TEXT("so that enemy is not sieging"), Structures->Siege(FVector(10000.f, 0.f, PadZ), 6.f, 14.f, 1.f / 30.f, 7));
	TestFalse(TEXT("an enemy with no structure dps never sieges"), Structures->Siege(FVector(2100.f, 0.f, PadZ), 6.f, 0.f, 1.f / 30.f, 7));

	FDFMessageCapture Damaged(F.World.MessageBus(), DFTags::Message_StructureDamaged);
	FDFMessageCapture Barricades(F.World.MessageBus(), DFTags::Message_BarricadeState);
	FDFMessageCapture Destroyed(F.World.MessageBus(), DFTags::Message_TowerDestroyed);

	const FVector AtBarricade(2100.f, 0.f, PadZ);   // 1 m from it; the wall lance is exactly 6 m away (d <= reach) but farther
	TestTrue(TEXT("a Ram at the barricade sieges"), Structures->Siege(AtBarricade, 6.f, 14.f, 1.f / 30.f, 7));
	TestEqual(TEXT("one frame: 300 - 14/30"), Barricade->GetStructureHp(), 300.f - 14.f / 30.f, 1e-3f);
	TestEqual(TEXT("the lance is untouched"), Wall->GetStructureHp(), 120.f, 1e-4f);
	TestEqual(TEXT("StructureDamaged on the host"), Damaged.Num(), 1);
	if (TestEqual(TEXT("the barricade reports damaged once"), Barricades.Num(), 1))
	{
		const FDFMsg_Structure* State = Barricades.LastPayload<FDFMsg_Structure>();
		TestTrue(TEXT("damaged"), State && State->State == FName(TEXT("damaged")));
	}
	Structures->Siege(AtBarricade, 6.f, 14.f, 1.f / 30.f, 7);
	TestEqual(TEXT("still damaged: no second state message"), Barricades.Num(), 1);

	// Two Rams in one frame, the second swing past zero: both land, as Step.cs removes after every enemy has swung.
	Structures->Siege(AtBarricade, 6.f, 14.f, 30.f, 7);   // 420 damage
	TestTrue(TEXT("broken"), Barricade->IsBroken());
	TestTrue(TEXT("but still the nearest target this frame"), Structures->FindSiegeTarget(AtBarricade, 6.f) == Barricade);
	TestTrue(TEXT("and the second Ram's swing lands on it"), Structures->Siege(AtBarricade, 6.f, 14.f, 1.f / 30.f, 8));
	if (TestEqual(TEXT("broken is announced"), Barricades.Num(), 2))
	{
		const FDFMsg_Structure* State = Barricades.LastPayload<FDFMsg_Structure>();
		TestTrue(TEXT("broken"), State && State->State == FName(TEXT("broken")));
	}
	const FDFMsg_Damage* Last = Damaged.LastPayload<FDFMsg_Damage>();
	TestTrue(TEXT("the ring never shows less than empty"), Last && Last->RemainingFraction == 0.f);
	TestEqual(TEXT("nothing is removed mid-frame"), Destroyed.Num(), 0);

	TestEqual(TEXT("rubble cannot be sold for a refund before it is removed"), F.Build->SellTower(1, Barricade->GetStructureId()).Reason, DFTowerMath::Reasons::UnknownTower);
	TestEqual(TEXT("end of frame: one removed"), F.Build->RemoveBroken(), 1);
	TestNull(TEXT("the pad is free"), F.Build->FindTowerOnSocket(TEXT("b1")));
	if (TestEqual(TEXT("one TowerDestroyed to the team"), Destroyed.Num(), 1))
	{
		const FDFMsg_Structure* Message = Destroyed.LastPayload<FDFMsg_Structure>();
		TestTrue(TEXT("a barricade is breached (its lane reopens)"), Message && Message->State == FName(TEXT("breached")));
		TestTrue(TEXT("on its socket"), Message && Message->SocketId == FName(TEXT("b1")));
	}
	TestTrue(TEXT("with the barricade gone, the Ram turns on the lance 6 m away"), Structures->FindSiegeTarget(AtBarricade, 6.f) == Wall);
	TestEqual(TEXT("nothing else was broken"), F.Build->RemoveBroken(), 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDFTowerBrokenSilentTest, "DF.Unit.Tower.BrokenTowerDoesNotFire", DFTowerBuildTest::Flags)
bool FDFTowerBrokenSilentTest::RunTest(const FString& Parameters)
{
	DFTowerBuildTest::FBuildFixture F;
	if (!F.Setup(*this))
	{
		return false;
	}
	F.Wallet->Money = 1000;
	ADFTower* Tower = F.Build->PlaceTower(1, TEXT("lance"), TEXT("g1")).Tower;
	ADFTowerTestDummy* Enemy = F.World.SpawnActor<ADFTowerTestDummy>(FTransform(FVector(1500.f, 0.f, 0.f)));
	if (!TestNotNull(TEXT("tower"), Tower) || !TestNotNull(TEXT("enemy"), Enemy))
	{
		return false;
	}
	Enemy->Id = 1;
	Enemy->Remaining = 10.f;
	UDFTargetRegistry::Get(F.World.GetWorld())->Register(Enemy);
	int32 Fired = 0;
	Tower->OnFired.AddLambda([&Fired](ADFTower*, AActor*) { ++Fired; });

	Tower->ApplySiegeDamage(1000.f, 7);
	TestTrue(TEXT("broken"), Tower->IsBroken());
	Tower->StepTower(1.f / 30.f);
	TestEqual(TEXT("rubble does not fire, even with a body 5 m away"), Fired, 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDFTowerRepairTest, "DF.Unit.Tower.RepairFirstHurtInReach", DFTowerBuildTest::Flags)
bool FDFTowerRepairTest::RunTest(const FString& Parameters)
{
	DFTowerBuildTest::FBuildFixture F;
	if (!F.Setup(*this))
	{
		return false;
	}
	F.Wallet->Money = 1000;
	ADFTower* First = F.Build->PlaceTower(1, TEXT("lance"), TEXT("g1")).Tower;    // x 10 m, built first
	ADFTower* Second = F.Build->PlaceTower(1, TEXT("lance"), TEXT("w1")).Tower;   // x 15 m
	UDFStructureRegistry* Structures = UDFStructureRegistry::Get(F.World.GetWorld());
	if (!TestNotNull(TEXT("towers"), First) || !TestNotNull(TEXT("towers"), Second) || !TestNotNull(TEXT("registry"), Structures))
	{
		return false;
	}
	const FVector Between(1250.f, 0.f, 2.f * ADFSocket::PadHalfHeightCm);   // 2.5 m from each
	TestFalse(TEXT("nothing hurt: the swing is not spent on a repair"), Structures->RepairNearby(Between, 3.f, 10.f, 2));

	FDFMessageCapture Repaired(F.World.MessageBus(), DFTags::Message_StructureRepaired);
	First->ApplySiegeDamage(50.f, 7);
	Second->ApplySiegeDamage(50.f, 7);
	TestTrue(TEXT("both hurt and in reach: the first built is mended"), Structures->RepairNearby(Between, 3.f, 10.f, 2));
	TestEqual(TEXT("70 + 10"), First->GetStructureHp(), 80.f, 1e-4f);
	TestEqual(TEXT("the other waits"), Second->GetStructureHp(), 70.f, 1e-4f);
	if (TestEqual(TEXT("StructureRepaired to the team"), Repaired.Num(), 1))
	{
		const FDFMsg_Structure* Message = Repaired.LastPayload<FDFMsg_Structure>();
		TestTrue(TEXT("by seat 2, to 80/120"), Message && Message->PlayerId == 2 && FMath::IsNearlyEqual(Message->HpFraction, 80.f / 120.f, 1e-4f));
	}
	Structures->RepairNearby(Between, 3.f, 1000.f, 2);
	TestEqual(TEXT("capped at the row's 120"), First->GetStructureHp(), 120.f, 1e-4f);
	TestTrue(TEXT("once whole it is skipped: the next hurt one is mended"), Structures->RepairNearby(Between, 3.f, 10.f, 2));
	TestEqual(TEXT("70 + 10"), Second->GetStructureHp(), 80.f, 1e-4f);
	TestFalse(TEXT("out of reach"), Structures->RepairNearby(FVector(10000.f, 0.f, 0.f), 3.f, 10.f, 2));

	Second->ApplySiegeDamage(1000.f, 7);
	TestNull(TEXT("rubble is not repaired"), Structures->FindRepairTarget(Between, 3.f));
	return true;
}

// ---- traps (Step.cs ApplyPlaceTrap, TriggerTraps) ---------------------------------------------------
// traps.json: spike 45 + 3 Alloy, radius 1.8 m, 3 charges, rearm 6 s, 18 damage, applies shred;
// launcher 55 + 2 Alloy + 1 Plating, radius 1.8 m, 2 charges, rearm 8 s, knockback 8 m.

namespace DFTowerBuildTest
{
	ADFTowerTestDummy* Body(FBuildFixture& F, int32 Id, const FVector& At, EDFEnemyLayer Layer = EDFEnemyLayer::Ground)
	{
		ADFTowerTestDummy* Dummy = F.World.SpawnActor<ADFTowerTestDummy>(FTransform(At));
		if (Dummy)
		{
			Dummy->Id = Id;
			Dummy->Layer = Layer;
			UDFTargetRegistry::Get(F.World.GetWorld())->Register(Dummy);
		}
		return Dummy;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDFTowerTrapPlaceTest, "DF.Unit.Tower.TrapPlaceInSimOrder", DFTowerBuildTest::Flags)
bool FDFTowerTrapPlaceTest::RunTest(const FString& Parameters)
{
	using namespace DFTowerMath;
	DFTowerBuildTest::FBuildFixture F;
	if (!F.Setup(*this))
	{
		return false;
	}
	FDFMessageCapture Placed(F.World.MessageBus(), DFTags::Message_TowerPlaced);
	F.Wallet->Money = 1000;
	TestEqual(TEXT("a trap on a ground pad"), F.Build->PlaceTower(1, TEXT("spike"), TEXT("g1")).Reason, Reasons::WrongSocketTag);
	F.Wallet->Money = 40;
	TestEqual(TEXT("40 does not buy a 45 spike"), F.Build->PlaceTower(1, TEXT("spike"), TEXT("t1")).Reason, Reasons::InsufficientFunds);
	F.Wallet->Money = 100;
	TestEqual(TEXT("money but no Alloy"), F.Build->PlaceTower(1, TEXT("spike"), TEXT("t1")).Reason, Reasons::InsufficientScrap);
	TestEqual(TEXT("refusals cost nothing"), F.Wallet->Money, 100);

	F.Wallet->Scrap.Add(EDFScrapType::Alloy, 5);
	const FDFBuildResult Built = F.Build->PlaceTower(3, TEXT("spike"), TEXT("t1"));
	if (!TestTrue(TEXT("placed"), Built.Succeeded()) || !TestNotNull(TEXT("the trap"), Built.Trap))
	{
		return false;
	}
	TestNull(TEXT("it is not a tower"), Built.Tower);
	TestEqual(TEXT("45 taken"), F.Wallet->Money, 55);
	TestEqual(TEXT("3 Alloy taken"), F.Wallet->Scrap.FindRef(EDFScrapType::Alloy), 2);
	TestEqual(TEXT("three charges"), Built.Trap->GetChargesLeft(), 3);
	TestTrue(TEXT("armed"), Built.Trap->IsArmed());
	TestEqual(TEXT("on the pad top"), Built.Trap->GetActorLocation(), FVector(2500.f, 0.f, 2.f * ADFSocket::PadHalfHeightCm));
	TestTrue(TEXT("found by socket"), F.Build->FindTrapOnSocket(TEXT("t1")) == Built.Trap);
	TestEqual(TEXT("TowerPlaced to the team"), Placed.Num(), 1);
	TestEqual(TEXT("a second trap on the pad"), F.Build->PlaceTower(1, TEXT("tar"), TEXT("t1")).Reason, Reasons::Occupied);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDFTowerTrapTriggerTest, "DF.Unit.Tower.TrapTriggersRearmsAndIsSpent", DFTowerBuildTest::Flags)
bool FDFTowerTrapTriggerTest::RunTest(const FString& Parameters)
{
	using namespace DFTowerBuildTest;
	FBuildFixture F;
	if (!F.Setup(*this))
	{
		return false;
	}
	F.Wallet->Money = 100;
	F.Wallet->Scrap.Add(EDFScrapType::Alloy, 3);
	ADFTrap* Trap = F.Build->PlaceTower(1, TEXT("spike"), TEXT("t1")).Trap;   // at (25 m, 0, pad top)
	if (!TestNotNull(TEXT("spike"), Trap))
	{
		return false;
	}
	const float Z = 2.f * ADFSocket::PadHalfHeightCm;
	FDFMessageCapture Triggered(F.World.MessageBus(), DFTags::Message_TrapTriggered);
	FDFMessageCapture Rearmed(F.World.MessageBus(), DFTags::Message_TrapRearmed);
	FDFMessageCapture Removed(F.World.MessageBus(), DFTags::Message_TowerDestroyed);

	Body(F, 1, FVector(2900.f, 0.f, Z));                               // 4 m: outside 1.8 m
	Body(F, 2, FVector(2550.f, 0.f, Z), EDFEnemyLayer::Air);           // a flyer overhead: not Ground
	ADFTowerTestDummy* Mole = Body(F, 3, FVector(2520.f, 0.f, Z));
	Mole->bBurrowed = true;                                             // underground
	TestFalse(TEXT("nothing it may hit is in range"), Trap->StepTrap(1.f / 30.f));
	TestEqual(TEXT("so no charge spent"), Trap->GetChargesLeft(), 3);

	Body(F, 4, FVector(2600.f, 0.f, Z));                               // 1 m: inside
	TestTrue(TEXT("a surfaced ground body 1 m away sets it off"), Trap->StepTrap(1.f / 30.f));
	TestEqual(TEXT("one charge spent"), Trap->GetChargesLeft(), 2);
	TestFalse(TEXT("rearming"), Trap->IsArmed());
	TestEqual(TEXT("TrapTriggered to the team"), Triggered.Num(), 1);

	TestFalse(TEXT("5.9 s later: still rearming"), Trap->StepTrap(5.9f));
	TestFalse(TEXT("the step the rearm runs out does not fire (Step.cs)"), Trap->StepTrap(0.2f));
	TestEqual(TEXT("TrapRearmed once"), Rearmed.Num(), 1);
	TestTrue(TEXT("armed again"), Trap->IsArmed());
	TestTrue(TEXT("the next step fires"), Trap->StepTrap(1.f / 30.f));

	Trap->StepTrap(6.1f);
	TestTrue(TEXT("the last charge"), Trap->StepTrap(1.f / 30.f));
	TestEqual(TEXT("none left"), Trap->GetChargesLeft(), 0);
	const FDFMsg_Structure* Last = Triggered.LastPayload<FDFMsg_Structure>();
	TestTrue(TEXT("the last trigger says spent"), Last && Last->State == FName(TEXT("spent")));
	TestFalse(TEXT("not removed while its rearm runs (the sim removes at charges 0 AND rearm 0)"), Trap->IsSpent());
	TestEqual(TEXT("nothing to remove yet"), F.Build->RemoveSpentTraps(), 0);

	Trap->StepTrap(8.f);
	TestTrue(TEXT("spent"), Trap->IsSpent());
	TestEqual(TEXT("no rearm message with no charges"), Rearmed.Num(), 2);
	TestEqual(TEXT("removed at the end of the frame"), F.Build->RemoveSpentTraps(), 1);
	TestNull(TEXT("the pad is free"), F.Build->FindTrapOnSocket(TEXT("t1")));
	const FDFMsg_Structure* Gone = Removed.LastPayload<FDFMsg_Structure>();
	TestTrue(TEXT("TowerDestroyed, State spent"), Gone && Gone->State == FName(TEXT("spent")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDFTowerTrapKnockbackTest, "DF.Unit.Tower.TrapKnockbackByMass", DFTowerBuildTest::Flags)
bool FDFTowerTrapKnockbackTest::RunTest(const FString& Parameters)
{
	using namespace DFTowerBuildTest;
	FBuildFixture F;
	if (!F.Setup(*this))
	{
		return false;
	}
	F.Wallet->Money = 100;
	F.Wallet->Scrap.Add(EDFScrapType::Alloy, 2);
	F.Wallet->Scrap.Add(EDFScrapType::Plating, 1);
	ADFTrap* Launcher = F.Build->PlaceTower(1, TEXT("launcher"), TEXT("t1")).Trap;
	if (!TestNotNull(TEXT("launcher"), Launcher))
	{
		return false;
	}
	const float Z = 2.f * ADFSocket::PadHalfHeightCm;
	ADFTowerTestDummy* Heavy = Body(F, 1, FVector(2600.f, 0.f, Z));
	ADFTowerTestDummy* Light = Body(F, 2, FVector(2400.f, 0.f, Z));
	Heavy->Mass = 2.f;
	Light->Mass = 0.1f;
	TestTrue(TEXT("fired"), Launcher->StepTrap(1.f / 30.f));
	TestEqual(TEXT("8 m / mass 2 = 4 m"), Heavy->KnockedBackMeters, 4.f, 1e-4f);
	TestEqual(TEXT("mass is floored at 0.25: 8 / 0.25 = 32 m"), Light->KnockedBackMeters, 32.f, 1e-4f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDFTowerWaveConditionTest, "DF.Unit.Tower.WaveConditionReachesEveryTower", DFTowerBuildTest::Flags)
bool FDFTowerWaveConditionTest::RunTest(const FString& Parameters)
{
	// Step.cs reads Conditions.ForWave every step: the weather is the wave's, for every tower standing
	// and every tower built during it. The match flow's WaveStarted carries it (DF.Condition.<Id>).
	DFTowerBuildTest::FBuildFixture F;
	if (!F.Setup(*this))
	{
		return false;
	}
	TestEqual(TEXT("DF.Condition.Fog names fog"), UDFBuildSubsystem::ConditionIdFromTag(DFTags::Condition_Fog), FName(TEXT("fog")));
	TestEqual(TEXT("DF.Condition.Night names night"), UDFBuildSubsystem::ConditionIdFromTag(DFTags::Condition_Night), FName(TEXT("night")));
	TestTrue(TEXT("no tag: clear weather"), UDFBuildSubsystem::ConditionIdFromTag(FGameplayTag()).IsNone());

	F.Wallet->Money = 1000;
	ADFTower* Before = F.Build->PlaceTower(1, TEXT("lance"), TEXT("g1")).Tower;
	if (!TestNotNull(TEXT("lance"), Before))
	{
		return false;
	}
	TestEqual(TEXT("clear weather: 12 m"), Before->GetRangeMeters(), 12.f, 1e-4f);

	FDFMsg_Wave Foggy;
	Foggy.WaveIndex = 3;
	Foggy.Condition = DFTags::Condition_Fog;
	F.World.MessageBus()->Broadcast(DFTags::Message_WaveStarted, Foggy);
	TestEqual(TEXT("the wave starts in fog: the standing lance sees 8.4 m"), Before->GetRangeMeters(), 8.4f, 1e-4f);
	ADFTower* During = F.Build->PlaceTower(1, TEXT("lance"), TEXT("w1")).Tower;
	TestTrue(TEXT("a lance built mid-wave is in the fog too"), During && FMath::IsNearlyEqual(During->GetRangeMeters(), 8.4f, 1e-4f));

	FDFMsg_Wave Clear;
	Clear.WaveIndex = 4;
	F.World.MessageBus()->Broadcast(DFTags::Message_WaveStarted, Clear);
	TestEqual(TEXT("the next wave is clear"), Before->GetRangeMeters(), 12.f, 1e-4f);
	TestTrue(TEXT("for every tower"), During && FMath::IsNearlyEqual(During->GetRangeMeters(), 12.f, 1e-4f));
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
