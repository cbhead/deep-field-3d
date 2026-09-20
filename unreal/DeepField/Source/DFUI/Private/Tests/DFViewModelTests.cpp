#include "DFGameplayTags.h"
#include "HAL/FileManager.h"
#include "Misc/AutomationTest.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "ViewModels/DFFakeMatchFeed.h"
#include "ViewModels/DFMatchViewModel.h"
#include "ViewModels/DFPickupViewModel.h"
#include "ViewModels/DFPlayerViewModel.h"
#include "ViewModels/DFStructureViewModel.h"
#include "ViewModels/DFVehicleViewModel.h"

#if WITH_DEV_AUTOMATION_TESTS

// DF.UI.* — C12. The view models are plain UObjects, so none of this needs a world.

namespace
{
	constexpr EAutomationTestFlags GFlags = EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter;

	/** Counts notifications of one field. */
	struct FFieldCounter
	{
		int32 Count = 0;
		FFieldCounter(UMVVMViewModelBase& ViewModel, FName FieldName)
		{
			const UE::FieldNotification::FFieldId Field = ViewModel.GetFieldNotificationDescriptor().GetField(ViewModel.GetClass(), FieldName);
			bFound = Field.IsValid();
			ViewModel.AddFieldValueChangedDelegate(Field, INotifyFieldValueChanged::FFieldValueChangedDelegate::CreateLambda(
				[this](UObject*, UE::FieldNotification::FFieldId) { ++Count; }));
		}
		bool bFound = false;
	};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDFViewModelNotifies, "DF.UI.ViewModel.NotifiesOnChangeOnly", GFlags)
bool FDFViewModelNotifies::RunTest(const FString&)
{
	UDFMatchViewModel* Match = NewObject<UDFMatchViewModel>();
	FFieldCounter Money(*Match, TEXT("Money"));
	FFieldCounter Lobby(*Match, TEXT("bLobby"));
	FFieldCounter TeamScrap(*Match, TEXT("TeamScrap"));
	FFieldCounter Boss(*Match, TEXT("Boss"));
	TestTrue(TEXT("the fields exist by their contract names"), Money.bFound && Lobby.bFound && TeamScrap.bFound && Boss.bFound);

	// A feed writes every field every frame; only a real change may wake a binding.
	Match->SetMoney(250);
	Match->SetMoney(250);
	Match->SetMoney(175);
	TestEqual(TEXT("int: two changes, one repeat"), Money.Count, 2);

	Match->SetLobby(false);
	Match->SetLobby(true);
	Match->SetLobby(true);
	TestEqual(TEXT("bool: the default is not a change"), Lobby.Count, 1);

	Match->SetTeamScrap({ { EDFScrapType::Alloy, 3 }, { EDFScrapType::Flux, 1 } });
	Match->SetTeamScrap({ { EDFScrapType::Flux, 1 }, { EDFScrapType::Alloy, 3 } });
	Match->SetTeamScrap({ { EDFScrapType::Alloy, 4 }, { EDFScrapType::Flux, 1 } });
	TestEqual(TEXT("map: order is not a change, a value is"), TeamScrap.Count, 2);

	FDFBossView View;
	Match->SetBoss(View);
	View.bActive = true;
	View.HpFrac = 0.5f;
	Match->SetBoss(View);
	Match->SetBoss(View);
	TestEqual(TEXT("struct"), Boss.Count, 1);

	// Derived fields follow what they are derived from.
	UDFPlayerViewModel* Player = NewObject<UDFPlayerViewModel>();
	FFieldCounter HpFrac(*Player, TEXT("HpFrac"));
	Player->SetMaxHp(200.f);
	Player->SetHp(50.f);
	Player->SetHp(50.f);
	TestEqual(TEXT("HpFrac notifies with Hp and MaxHp"), HpFrac.Count, 2);
	TestEqual(TEXT("HpFrac"), Player->HpFrac(), 0.25f);
	Player->SetMaxHp(0.f);
	TestEqual(TEXT("HpFrac with no max"), Player->HpFrac(), 0.f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDFViewModelChildren, "DF.UI.ViewModel.ChildrenKeepIdentity", GFlags)
bool FDFViewModelChildren::RunTest(const FString&)
{
	UDFMatchViewModel* Match = NewObject<UDFMatchViewModel>();
	FFieldCounter Players(*Match, TEXT("Players"));
	FFieldCounter Local(*Match, TEXT("Local"));
	FFieldCounter Structures(*Match, TEXT("Structures"));

	TestNull(TEXT("no local player before anyone joined"), Match->Local());
	Match->SetLocalPlayerId(2);
	UDFPlayerViewModel* One = Match->FindOrAddPlayer(1);
	UDFPlayerViewModel* Two = Match->FindOrAddPlayer(2);
	TestTrue(TEXT("found, not re-added"), Match->FindOrAddPlayer(1) == One && Match->GetPlayers().Num() == 2);
	TestEqual(TEXT("the list notifies per add"), Players.Count, 2);
	TestTrue(TEXT("Local() is the player with LocalPlayerId"), Match->Local() == Two);
	TestEqual(TEXT("Local notified for the id and for the arrival"), Local.Count, 2);

	// A feed pass: write what exists, retain what was written.
	Match->FindOrAddStructure(10)->SetSocketId(TEXT("g01"));
	Match->FindOrAddStructure(11)->SetSocketId(TEXT("g02"));
	UDFStructureViewModel* Kept = Match->AtSocket(TEXT("g02"));
	Match->RetainStructures({ 11 });
	TestEqual(TEXT("retain drops the rest"), Match->GetStructures().Num(), 1);
	TestTrue(TEXT("and keeps the survivor's identity"), Match->AtSocket(TEXT("g02")) == Kept && !Match->IsSocketOccupied(TEXT("g01")));
	const int32 Before = Structures.Count;
	Match->RetainStructures({ 11 });
	TestEqual(TEXT("a retain that removes nothing is silent"), Structures.Count, Before);
	TestNull(TEXT("no socket id never matches"), Match->AtSocket(NAME_None));

	TestTrue(TEXT("the local player leaving is a Local change"), Match->RemovePlayer(2) && Match->Local() == nullptr && Local.Count == 3);
	TestFalse(TEXT("removing twice"), Match->RemovePlayer(2));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDFViewModelFakeFeed, "DF.UI.ViewModel.FakeFeedAndHelpers", GFlags)
bool FDFViewModelFakeFeed::RunTest(const FString&)
{
	UDFMatchViewModel* Match = NewObject<UDFMatchViewModel>();
	TestFalse(TEXT("invalid until a feed has written"), Match->IsValid());
	FDFFakeMatchFeed::FillMidWave(*Match);
	TestTrue(TEXT("valid after"), Match->IsValid());

	// GameView.cs helpers.
	const UDFPlayerViewModel* Me = Match->Local();
	if (!TestNotNull(TEXT("Local()"), Me))
	{
		return true;
	}
	TestEqual(TEXT("SpendableMoney = Money - PendingSpend"), Match->SpendableMoney(), 412 - 75);
	TestEqual(TEXT("TeamScrapOf"), Match->TeamScrapOf(EDFScrapType::Alloy), 23);
	TestEqual(TEXT("TeamScrapOf, none"), Match->TeamScrapOf(EDFScrapType::Primecore), 0);
	TestEqual(TEXT("PersonalScrapOf reads the local player"), Match->PersonalScrapOf(EDFScrapType::Alloy), 11);
	TestTrue(TEXT("Faction is a tag"), Me->GetFaction() == DFTags::Faction_Forge);
	TestEqual(TEXT("PackLevelFor"), Me->PackLevelFor(TEXT("rifle")), 2);
	TestEqual(TEXT("PackLevelFor, stock weapon"), Me->PackLevelFor(TEXT("sidearm")), 0);
	TestTrue(TEXT("AmmoFor"), Me->AmmoFor(TEXT("rifle")) == FName(TEXT("ap")) && Me->AmmoFor(TEXT("sidearm")) == FName(TEXT("standard")));
	TestTrue(TEXT("AttachmentFor"), Me->AttachmentFor(TEXT("rifle"), EDFAttachmentSlot::Barrel) == FName(TEXT("longBarrel")) && Me->AttachmentFor(TEXT("rifle"), EDFAttachmentSlot::Optic).IsNone());
	TestTrue(TEXT("Owns"), Me->Owns(TEXT("rifle")) && !Me->Owns(TEXT("scattergun")));

	UDFVehicleViewModel* Vehicle = nullptr;
	int32 Seat = 0;
	TestTrue(TEXT("SeatOf finds the rider"), Match->SeatOf(3, Vehicle, Seat) && Vehicle == Match->VehicleById(TEXT("buggy_1")) && Seat == 1);
	TestTrue(TEXT("SeatOf, on foot"), !Match->SeatOf(1, Vehicle, Seat) && Vehicle == nullptr && Seat == INDEX_NONE);
	TestTrue(TEXT("an empty seat is nobody's"), Match->VehicleById(TEXT("buggy_1"))->SeatOf(0) == INDEX_NONE && Match->VehicleById(TEXT("buggy_1"))->IsSeatFree(0) && Match->VehicleById(TEXT("buggy_1"))->DriverId() == 0);

	const UDFStructureViewModel* Lance = Match->AtSocket(TEXT("g03"));
	TestTrue(TEXT("AtSocket + def tag from the content id"), Lance && Lance->GetDefTag() == DFTags::Tower_Lance && Lance->GetDefId() == FName(TEXT("lance")));
	TestTrue(TEXT("PathLevel"), Lance && Lance->PathLevel(0) == 4 && Lance->PathLevel(7) == 0);
	TestTrue(TEXT("sellable is the feed's answer"), Lance && Lance->IsSellable() && !Match->AtSocket(TEXT("g07"))->IsSellable());

	// A live feed's tick moves timers and nothing else.
	const float Bleed = Match->FindPlayer(2)->GetBleedoutSecondsLeft();
	FDFFakeMatchFeed::Tick(*Match, 0.5f);
	TestEqual(TEXT("bleedout ticks"), Match->FindPlayer(2)->GetBleedoutSecondsLeft(), Bleed - 0.5f);
	TestEqual(TEXT("money does not"), Match->GetMoney(), 412);

	// Filling again is idempotent on identity-free state and Reset really resets.
	FDFFakeMatchFeed::FillLobby(*Match);
	TestTrue(TEXT("lobby"), Match->IsLobby() && Match->GetPlayers().Num() == 2 && Match->GetStructures().Num() == 0 && Match->GetWaveIndex() == -1 && Match->GetPendingSpend() == 0);
	Match->SetConnectionState(DFTags::Message_ConnectionState);
	Match->Reset();
	TestTrue(TEXT("Reset"), !Match->IsValid() && Match->GetPlayers().Num() == 0 && Match->GetLocalPlayerId() == 0 && !Match->IsLobby());
	TestTrue(TEXT("ConnectionState survives leaving a match"), Match->GetConnectionState().IsValid());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDFUINoNetBranching, "DF.UI.NoNetBranching", GFlags)
bool FDFUINoNetBranching::RunTest(const FString&)
{
	// C12: UI never asks whether it is the host. The same check belongs in CI (WS-15); having it
	// here means a WS-12 session sees it before a PR does.
	const FString Root = FPaths::ConvertRelativePathToFull(FPaths::Combine(FPaths::ProjectDir(), TEXT("Source"), TEXT("DFUI")));
	TArray<FString> Files;
	IFileManager::Get().FindFilesRecursive(Files, *Root, TEXT("*.h"), true, false);
	IFileManager::Get().FindFilesRecursive(Files, *Root, TEXT("*.cpp"), true, false, /*bClearFileNames*/ false);
	TestTrue(FString::Printf(TEXT("DFUI sources found under %s"), *Root), Files.Num() > 0);

	// Spelled in pieces so this file does not trip its own check.
	const FString Banned[] = { FString(TEXT("HasAuth")) + TEXT("ority("), FString(TEXT("GetNet")) + TEXT("Mode("), FString(TEXT("IsNet")) + TEXT("Mode("), FString(TEXT("GetLocal")) + TEXT("Role(") };
	for (const FString& File : Files)
	{
		FString Text;
		if (!FFileHelper::LoadFileToString(Text, *File))
		{
			AddError(FString::Printf(TEXT("cannot read %s"), *File));
			continue;
		}
		for (const FString& Call : Banned)
		{
			if (Text.Contains(Call, ESearchCase::CaseSensitive))
			{
				AddError(FString::Printf(TEXT("%s calls %s) - UI reads view models, never net role (C12)"), *File, *Call));
			}
		}
	}
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
