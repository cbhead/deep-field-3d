// WS-11 automation tests (PROGRAMME.md §7): run with
//   unreal/Build/editor-lock.sh unreal/Build/test.sh DF.Online
// Everything runs against the Null services, headless (-nullrhi), in a standalone game
// instance the tests create themselves; nothing here touches the disk or the network beyond
// the Null LAN beacon on the loopback interface.

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "DFContentHash.h"
#include "DFGameInstance.h"
#include "DFGameplayTags.h"
#include "DFJoinCode.h"
#include "DFOnlineSubsystem.h"
#include "Engine/DataTable.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Content/DFContentRows.h"
#include "IDFSessionBackend.h"
#include "Kismet/GameplayStatics.h"
#include "Profile/DFLocalProgressionProvider.h"
#include "Profile/DFProfileMigrations.h"
#include "Profile/DFProfileSave.h"
#include "UObject/StrongObjectPtr.h"

namespace DFOnlineTests
{
	/** A backend that records instead of travelling, so a session test never leaves the test world. */
	class FRecordingBackend : public IDFSessionBackend
	{
	public:
		int32 Hosts = 0;
		int32 Joins = 0;
		int32 Leaves = 0;
		FString LastMap;
		FString LastConnect;
		FString LastOptions;

		virtual FName GetName() const override { return TEXT("Recording"); }
		virtual bool StartHost(UWorld*, FName MapId, const FString& Options) override { ++Hosts; LastMap = MapId.ToString(); LastOptions = Options; return true; }
		virtual bool Join(UWorld*, const FString& ConnectString, const FString& Options) override { ++Joins; LastConnect = ConnectString; LastOptions = Options; return true; }
		virtual void Leave(UWorld*) override { ++Leaves; }
		virtual FName GetNetDriverClassName() const override { return TEXT("Recording"); }
	};

	/** A standalone game instance (dummy world, real subsystems) for one test, torn down by the last latent step. */
	struct FFixture : TSharedFromThis<FFixture>
	{
		FAutomationTestBase* Test = nullptr;
		TStrongObjectPtr<UGameInstance> GameInstance;
		UDFOnlineSubsystem* Online = nullptr;
		TSharedPtr<FRecordingBackend> Backend;
		double StartedAt = 0.0;

		// Outcomes the latent steps read back (RunTest's locals are gone by then).
		int32 LoginChanged = 0;
		bool bCallbackOk = false;
		FString CallbackReason;

		bool Boot(FAutomationTestBase* InTest)
		{
			Test = InTest;
			GameInstance = TStrongObjectPtr<UGameInstance>(NewObject<UDFGameInstance>(GEngine));
			GameInstance->InitializeStandalone();
			Online = GameInstance->GetSubsystem<UDFOnlineSubsystem>();
			if (!Online)
			{
				Test->AddError(TEXT("UDFOnlineSubsystem missing from the game instance"));
				return false;
			}
			Backend = MakeShared<FRecordingBackend>();
			Online->SetSessionBackend(Backend);
			StartedAt = FPlatformTime::Seconds();
			return true;
		}

		void Teardown()
		{
			if (!GameInstance)
			{
				return;
			}
			UWorld* World = GameInstance->GetWorld();
			GameInstance->Shutdown();
			if (World && GEngine)
			{
				GEngine->DestroyWorldContext(World);
				World->DestroyWorld(false);
			}
			Online = nullptr;
			GameInstance.Reset();
		}

		bool TimedOut(double Seconds) const { return FPlatformTime::Seconds() - StartedAt > Seconds; }
	};
}

using namespace DFOnlineTests;

/** Spin until Predicate is true or the fixture's timeout passes (then fails the test). */
DEFINE_LATENT_AUTOMATION_COMMAND_THREE_PARAMETER(FDFWaitUntil, TSharedPtr<FFixture>, Fixture, TFunction<bool()>, Predicate, FString, What);
bool FDFWaitUntil::Update()
{
	if (Predicate())
	{
		return true;
	}
	if (Fixture->TimedOut(20.0))
	{
		Fixture->Test->AddError(FString::Printf(TEXT("timed out waiting for: %s"), *What));
		return true;
	}
	return false;
}

/** Run a step (assertions, the next call) once; the fixture keeps the game instance alive between steps. */
DEFINE_LATENT_AUTOMATION_COMMAND_ONE_PARAMETER(FDFStep, TFunction<void()>, Step);
bool FDFStep::Update()
{
	Step();
	return true;
}

// ---- DF.Online.NullLogin ----------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDFOnlineNullLoginTest, "DF.Online.NullLogin", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FDFOnlineNullLoginTest::RunTest(const FString& Parameters)
{
	TSharedPtr<FFixture> F = MakeShared<FFixture>();
	if (!F->Boot(this))
	{
		return false;
	}
	TestEqual(TEXT("offline before login"), F->Online->GetConnectionState(), EDFConnectionState::Offline);
	TestFalse(TEXT("not logged in before login"), F->Online->IsLoggedIn());

	F->Online->OnLoginChanged.AddLambda([F](const FDFOnlineIdentity&) { ++F->LoginChanged; });
	F->Online->Login(EDFLoginMethod::Auto, FDFOnlineResult::CreateLambda([F](bool bOk, const FString& Reason) { F->bCallbackOk = bOk; F->CallbackReason = Reason; }));

	ADD_LATENT_AUTOMATION_COMMAND(FDFWaitUntil(F, [F]() { return F->Online->IsLoggedIn() || F->Online->GetConnectionState() == EDFConnectionState::Failed; }, TEXT("login")));
	ADD_LATENT_AUTOMATION_COMMAND(FDFStep([this, F]()
	{
		const FDFOnlineIdentity& Me = F->Online->GetLocalIdentity();
		TestTrue(TEXT("login callback reported success"), F->bCallbackOk);
		TestTrue(FString::Printf(TEXT("logged in (reason: %s)"), *F->CallbackReason), Me.bLoggedIn);
		TestTrue(TEXT("identity has an id"), Me.Id.IsValid());
		TestTrue(TEXT("Null ids carry the OSSV2 prefix"), Me.Id.ToString().StartsWith(TEXT("OSSV2-")));
		TestTrue(TEXT("identity has a display name"), !Me.DisplayName.IsEmpty());
		TestEqual(TEXT("provider is Null"), Me.Provider, FName(TEXT("Null")));
		TestEqual(TEXT("provider name on the facade"), F->Online->GetProviderName(), FName(TEXT("Null")));
		TestEqual(TEXT("state is LoggedIn"), F->Online->GetConnectionState(), EDFConnectionState::LoggedIn);
		TestEqual(TEXT("OnLoginChanged fired once"), F->LoginChanged, 1);
		TestTrue(TEXT("id round-trips to an FAccountId"), Me.Id.ToAccountId().IsValid());
		TestEqual(TEXT("FAccountId round-trips to the same id"), FDFOnlineId::FromAccountId(Me.Id.ToAccountId()), Me.Id);

		// Logging out is local state even where the provider cannot (Null: NotImplemented).
		F->Online->Logout();
		TestFalse(TEXT("logged out"), F->Online->IsLoggedIn());
		TestEqual(TEXT("state is Offline after logout"), F->Online->GetConnectionState(), EDFConnectionState::Offline);
		F->Teardown();
	}));
	return true;
}

// ---- DF.Online.NullSession --------------------------------------------------------------------
// Login, host a party lobby over the Null (LAN) lobbies, exercise the join-code / approval /
// ban rules against ValidateJoinOptions, then leave.

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDFOnlineNullSessionTest, "DF.Online.NullSession", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FDFOnlineNullSessionTest::RunTest(const FString& Parameters)
{
	TSharedPtr<FFixture> F = MakeShared<FFixture>();
	if (!F->Boot(this))
	{
		return false;
	}
	F->Online->Login(EDFLoginMethod::Auto);
	ADD_LATENT_AUTOMATION_COMMAND(FDFWaitUntil(F, [F]() { return F->Online->IsLoggedIn() || F->Online->GetConnectionState() == EDFConnectionState::Failed; }, TEXT("login")));
	ADD_LATENT_AUTOMATION_COMMAND(FDFStep([this, F]()
	{
		if (!F->Online->IsLoggedIn())
		{
			AddError(TEXT("login failed; cannot host"));
			F->Teardown();
			return;
		}
		F->Online->CreateInviteOnlySession(TEXT("testlane"), DFTags::Tier_Standard, /*bEndless*/ false);
	}));
	ADD_LATENT_AUTOMATION_COMMAND(FDFWaitUntil(F, [F]() { return !F->GameInstance || F->Online->IsHosting() || F->Online->GetConnectionState() == EDFConnectionState::LoggedIn; }, TEXT("session")));
	ADD_LATENT_AUTOMATION_COMMAND(FDFStep([this, F]()
	{
		if (!F->GameInstance)
		{
			return;
		}
		UDFOnlineSubsystem* Online = F->Online;
		const FDFSessionInfo& S = Online->GetSession();
		TestTrue(TEXT("hosting"), Online->IsHosting());
		TestEqual(TEXT("state is Hosting"), Online->GetConnectionState(), EDFConnectionState::Hosting);
		TestEqual(TEXT("map"), S.MapId, FName(TEXT("testlane")));
		TestEqual(TEXT("tier"), S.Tier, FGameplayTag(DFTags::Tier_Standard));
		TestFalse(TEXT("not endless"), S.bEndless);
		TestEqual(TEXT("build attribute"), S.Build, UDFOnlineSubsystem::GetBuildLabel());
		TestEqual(TEXT("content hash attribute"), S.ContentHash, Online->GetContentHash());
		TestEqual(TEXT("host is me"), S.Host, Online->GetLocalIdentity().Id);
		TestTrue(TEXT("I am a member"), S.Members.Contains(Online->GetLocalIdentity().Id));

		const FString Code = Online->GetJoinCode();
		TestTrue(TEXT("join code well-formed"), FDFJoinCode::IsWellFormed(Code));
		TestTrue(TEXT("join code validates"), Online->IsJoinCodeValid(Code));
		TestTrue(TEXT("join code validates lower-case with a dash"), Online->IsJoinCodeValid(Code.ToLower().Mid(0, 3) + TEXT("-") + Code.Mid(3)));

		// The handshake the game mode runs at PreLogin.
		const FDFOnlineId Stranger(TEXT("OSSV2-stranger-0000"), Online->GetLocalIdentity().Id.Services);
		FString Error;
		const FString Good = Online->MakeJoinOptions();
		TestTrue(TEXT("my own options carry build/hash/id"), Good.Contains(TEXT("?build=")) && Good.Contains(TEXT("?contentHash=")) && Good.Contains(TEXT("?onlineId=")));
		TestTrue(TEXT("host validates itself"), Online->ValidateJoinOptions(Good, Error));

		const FString StrangerOptions = FString::Printf(TEXT("?build=%s?contentHash=%s?onlineId=%s"), *UDFOnlineSubsystem::GetBuildLabel(), *Online->GetContentHash(), *Stranger.ToString());
		TestFalse(TEXT("unapproved stranger refused"), Online->ValidateJoinOptions(StrangerOptions, Error));
		TestEqual(TEXT("reason notInvited"), Error, FString(TEXT("notInvited")));

		Online->ApproveJoin(Stranger, true);
		TestTrue(TEXT("approved stranger accepted"), Online->ValidateJoinOptions(StrangerOptions, Error));
		TestTrue(TEXT("approval rotated the code"), Online->GetJoinCode() != Code);
		TestFalse(TEXT("old code no longer valid"), Online->IsJoinCodeValid(Code));

		TestFalse(TEXT("wrong build refused"), Online->ValidateJoinOptions(TEXT("?build=0.0.0?contentHash=x?onlineId=y"), Error));
		TestEqual(TEXT("reason versionMismatch"), Error, FString(TEXT("versionMismatch")));
		TestFalse(TEXT("wrong hash refused"), Online->ValidateJoinOptions(FString::Printf(TEXT("?build=%s?contentHash=deadbeef?onlineId=%s"), *UDFOnlineSubsystem::GetBuildLabel(), *Stranger.ToString()), Error));
		TestEqual(TEXT("reason contentMismatch"), Error, FString(TEXT("contentMismatch")));
		TestFalse(TEXT("bare dev join refused while hosting"), Online->ValidateJoinOptions(TEXT(""), Error));

		Online->Kick(Stranger, /*bBanForSession*/ true);
		TestFalse(TEXT("banned stranger refused"), Online->ValidateJoinOptions(StrangerOptions, Error));
		TestEqual(TEXT("reason banned"), Error, FString(TEXT("banned")));

		TestTrue(TEXT("LaunchMatch goes through the backend"), Online->LaunchMatch());
		TestEqual(TEXT("backend hosted the session map"), F->Backend->LastMap, FString(TEXT("testlane")));

		Online->Leave();
		TestFalse(TEXT("no session after Leave"), Online->GetSession().bValid);
		TestEqual(TEXT("state back to LoggedIn"), Online->GetConnectionState(), EDFConnectionState::LoggedIn);
		TestTrue(TEXT("join code gone after Leave"), Online->GetJoinCode().IsEmpty());
		TestFalse(TEXT("ban list is per session"), Online->IsBanned(Stranger));
		AddExpectedMessage(TEXT("join without handshake options"), ELogVerbosity::Warning);
		TestTrue(TEXT("bare dev join allowed without a session"), Online->ValidateJoinOptions(TEXT(""), Error));
		F->Teardown();
	}));
	return true;
}

// ---- DF.Online.ProfileSaveRoundTrip -------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDFOnlineProfileSaveRoundTripTest, "DF.Online.ProfileSaveRoundTrip", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FDFOnlineProfileSaveRoundTripTest::RunTest(const FString& Parameters)
{
	UDFLocalProgressionProvider* Provider = NewObject<UDFLocalProgressionProvider>();
	Provider->bInMemoryOnly = true;
	TestTrue(TEXT("load creates a profile"), Provider->Load());
	UDFProfileSave* Profile = Provider->GetProfile();
	if (!Profile)
	{
		return false;
	}
	TestEqual(TEXT("fresh profile is at the current version"), Profile->Version, UDFProfileSave::CurrentVersion);

	Profile->Name = TEXT("Chandler");
	Profile->PreferredFaction = DFTags::Faction_Ember;
	Profile->LastJoinCode = TEXT("ABCDEF");
	Profile->bShowDamageNumbers = false;
	Profile->MouseSensitivity = 0.75f;
	Profile->FieldOfView = 100;
	Profile->HudScale = 1.25f;
	Profile->bReduceFlashes = true;
	Profile->MusicVolume = 0.3f;
	Profile->ScalabilityTier = TEXT("High");
	Profile->TsrPercent = 50.f;
	Profile->FactionXp.Add(DFTags::Faction_Ember, 120);
	Profile->FactionXp.Add(DFTags::Faction_Forge, 7);
	Profile->ClearedSectors = { TEXT("foundry"), TEXT("switchyard") };
	Profile->BestWave.Add(TEXT("foundry"), 10);
	Profile->BestWave.Add(TEXT("foundry:endless"), 23);
	Profile->HighestTier.Add(TEXT("foundry"), DFTags::Tier_Hardened);
	FDFBlueprint Blueprint;
	Blueprint.Ammo = TEXT("hollowpoint");
	Blueprint.Attachments.Add(DFTags::Faction_Forge, TEXT("scope"));   // any tag key; the slot tags are WS-06's
	Profile->Blueprints.Add(TEXT("rifle"), Blueprint);
	FDFMatchRecord Record;
	Record.MapId = TEXT("foundry");
	Record.Tier = DFTags::Tier_Standard;
	Record.BestWave = 10;
	Record.bVictory = true;
	Record.XpBySource.Add(DFTags::Faction_Ember, 40);
	Record.PlayedAt = FDateTime(2026, 9, 19, 12, 0, 0);
	Record.HostBuild = TEXT("0.13.0-unreal");
	Record.HostId = TEXT("OSSV2-host");
	Profile->RecentMatches.Add(Record);

	TestTrue(TEXT("save to memory"), Provider->Save());
	TestTrue(TEXT("saved bytes are not empty"), Provider->GetLastSavedBytes().Num() > 0);

	UDFLocalProgressionProvider* Reader = NewObject<UDFLocalProgressionProvider>();
	Reader->bInMemoryOnly = true;
	TestTrue(TEXT("load from bytes"), Reader->LoadFromBytes(Provider->GetLastSavedBytes()));
	UDFProfileSave* Loaded = Reader->GetProfile();
	if (!Loaded)
	{
		return false;
	}
	TestTrue(TEXT("round trip is field-identical"), Loaded->IsSameAs(*Profile));
	TestEqual(TEXT("level from XP (120 / 100 per level)"), Reader->LevelFor(DFTags::Faction_Ember), 2);
	TestEqual(TEXT("level for a faction with no XP"), Reader->LevelFor(DFTags::Faction_Glacier), 1);

	// Migration: a v0 file walks to v1 unchanged; a newer file is refused (and never overwritten).
	UDFProfileSave* Old = NewObject<UDFProfileSave>();
	Old->Version = 0;
	Old->Name = TEXT("legacy");
	TestTrue(TEXT("v0 migrates"), UDFProfileMigrations::Migrate(Old));
	TestEqual(TEXT("v0 -> current"), Old->Version, UDFProfileSave::CurrentVersion);
	TestEqual(TEXT("migration keeps the data"), Old->Name, FString(TEXT("legacy")));
	UDFProfileSave* Future = NewObject<UDFProfileSave>();
	Future->Version = UDFProfileSave::CurrentVersion + 1;
	AddExpectedMessage(TEXT("leaving it untouched"), ELogVerbosity::Warning);
	TestFalse(TEXT("newer profile refused"), UDFProfileMigrations::Migrate(Future));
	TestEqual(TEXT("newer profile version untouched"), Future->Version, UDFProfileSave::CurrentVersion + 1);
	TestEqual(TEXT("curve: 0 xp is level 1"), UDFLocalProgressionProvider::LevelForXp(0, 100.f), 1);
	TestEqual(TEXT("curve: 99 xp is level 1"), UDFLocalProgressionProvider::LevelForXp(99, 100.f), 1);
	TestEqual(TEXT("curve: 100 xp is level 2"), UDFLocalProgressionProvider::LevelForXp(100, 100.f), 2);
	TestEqual(TEXT("curve: capped at MaxLevel"), UDFLocalProgressionProvider::LevelForXp(1000000, 100.f), UDFLocalProgressionProvider::MaxLevel);
	return true;
}

// ---- DF.Online.MatchRecordCapsXp ----------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDFOnlineMatchRecordCapsXpTest, "DF.Online.MatchRecordCapsXp", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FDFOnlineMatchRecordCapsXpTest::RunTest(const FString& Parameters)
{
	UDFLocalProgressionProvider* Provider = NewObject<UDFLocalProgressionProvider>();
	Provider->bInMemoryOnly = true;
	Provider->Load();
	UDFProfileSave* Profile = Provider->GetProfile();

	FDFMatchRecord Record;
	Record.MapId = TEXT("foundry");
	Record.Tier = DFTags::Tier_Hardened;
	Record.BestWave = 10;
	Record.bVictory = true;
	Record.XpBySource.Add(DFTags::Faction_Forge, 150);     // a lying host
	Record.XpBySource.Add(DFTags::Faction_Ember, 60);      // exactly the cap
	Record.XpBySource.Add(DFTags::Faction_Tempest, -5);    // never negative
	Record.XpBySource.Add(DFTags::Faction_Glacier, 12);

	Provider->ApplyMatchRecord(Record);
	TestEqual(TEXT("150 capped to 60"), Profile->FactionXp.FindRef(DFTags::Faction_Forge), FDFMatchRecord::MaxXpPerSource);
	TestEqual(TEXT("60 stays 60"), Profile->FactionXp.FindRef(DFTags::Faction_Ember), 60);
	TestEqual(TEXT("negative clamps to 0"), Profile->FactionXp.FindRef(DFTags::Faction_Tempest), 0);
	TestEqual(TEXT("12 stays 12"), Profile->FactionXp.FindRef(DFTags::Faction_Glacier), 12);
	TestTrue(TEXT("campaign victory clears the sector"), Profile->ClearedSectors.Contains(FName(TEXT("foundry"))));
	TestEqual(TEXT("best wave recorded"), Profile->BestWave.FindRef(TEXT("foundry")), 10);
	TestEqual(TEXT("highest tier recorded"), Profile->HighestTier.FindRef(TEXT("foundry")), FGameplayTag(DFTags::Tier_Hardened));
	TestEqual(TEXT("record kept (capped)"), Profile->RecentMatches.Num(), 1);
	TestEqual(TEXT("stored record carries the capped XP"), Profile->RecentMatches[0].XpBySource.FindRef(DFTags::Faction_Forge), FDFMatchRecord::MaxXpPerSource);
	TestTrue(TEXT("apply persisted"), Provider->GetLastSavedBytes().Num() > 0);

	// A second match adds on top; the cap is per record per source, not a lifetime ceiling.
	Provider->ApplyMatchRecord(Record);
	TestEqual(TEXT("second record adds another 60"), Profile->FactionXp.FindRef(DFTags::Faction_Forge), 2 * FDFMatchRecord::MaxXpPerSource);

	// Endless: best wave under its own key, no sector clear, no tier; a lower tier never lowers the highest.
	FDFMatchRecord Endless;
	Endless.MapId = TEXT("foundry");
	Endless.Tier = DFTags::Tier_Standard;
	Endless.bEndless = true;
	Endless.BestWave = 31;
	Endless.bVictory = true;
	Provider->ApplyMatchRecord(Endless);
	TestEqual(TEXT("endless best wave keyed separately"), Profile->BestWave.FindRef(TEXT("foundry:endless")), 31);
	TestEqual(TEXT("campaign best wave untouched"), Profile->BestWave.FindRef(TEXT("foundry")), 10);
	TestEqual(TEXT("highest tier untouched by a lower tier"), Profile->HighestTier.FindRef(TEXT("foundry")), FGameplayTag(DFTags::Tier_Hardened));
	TestEqual(TEXT("cleared sectors still one"), Profile->ClearedSectors.Num(), 1);

	// A loss records the wave but clears nothing.
	FDFMatchRecord Loss;
	Loss.MapId = TEXT("spire");
	Loss.Tier = DFTags::Tier_Assault;
	Loss.BestWave = 4;
	Provider->ApplyMatchRecord(Loss);
	TestFalse(TEXT("a loss does not clear the sector"), Profile->ClearedSectors.Contains(FName(TEXT("spire"))));
	TestEqual(TEXT("a loss still records the wave"), Profile->BestWave.FindRef(TEXT("spire")), 4);
	TestFalse(TEXT("a loss records no tier"), Profile->HighestTier.Contains(TEXT("spire")));

	// The recent list is bounded.
	for (int32 i = 0; i < 30; ++i)
	{
		Provider->ApplyMatchRecord(Loss);
	}
	TestEqual(TEXT("recent matches capped"), Profile->RecentMatches.Num(), UDFProfileSave::MaxRecentMatches);
	return true;
}

// ---- DF.Online.JoinCodeRotates ------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDFOnlineJoinCodeRotatesTest, "DF.Online.JoinCodeRotates", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FDFOnlineJoinCodeRotatesTest::RunTest(const FString& Parameters)
{
	const FString Symbols(FDFJoinCode::Alphabet());
	TestEqual(TEXT("alphabet has 32 symbols"), Symbols.Len(), 32);
	TestTrue(TEXT("alphabet has no 0/O/1/I"), !FDFJoinCode::IsSymbol(TEXT('0')) && !FDFJoinCode::IsSymbol(TEXT('O')) && !FDFJoinCode::IsSymbol(TEXT('1')) && !FDFJoinCode::IsSymbol(TEXT('I')));
	TestTrue(TEXT("alphabet symbols are recognised"), FDFJoinCode::IsSymbol(TEXT('A')) && FDFJoinCode::IsSymbol(TEXT('9')));

	TSet<FString> Seen;
	for (int32 i = 0; i < 200; ++i)
	{
		const FString Code = FDFJoinCode::Generate();
		TestTrue(TEXT("generated code is well-formed"), FDFJoinCode::IsWellFormed(Code));
		Seen.Add(Code);
	}
	TestTrue(TEXT("200 draws are (all but surely) distinct"), Seen.Num() >= 199);

	TestEqual(TEXT("normalize upper-cases and strips separators"), FDFJoinCode::Normalize(TEXT(" ab-cD ef ")), FString(TEXT("ABCDEF")));
	TestFalse(TEXT("too short is malformed"), FDFJoinCode::IsWellFormed(TEXT("ABCDE")));
	TestFalse(TEXT("look-alike symbols are malformed"), FDFJoinCode::IsWellFormed(TEXT("ABCDE0")));

	FDFJoinCodeRotator Rotator;
	Rotator.LifetimeSeconds = 100.0;
	TestFalse(TEXT("inactive before first use"), Rotator.IsActive());
	TestFalse(TEXT("nothing matches an inactive rotator"), Rotator.Matches(TEXT("ABCDEF"), 0.0));

	const FString First = Rotator.Current(1000.0);
	TestTrue(TEXT("first code issued"), FDFJoinCode::IsWellFormed(First));
	TestEqual(TEXT("generation 1"), Rotator.GetGeneration(), 1);
	TestTrue(TEXT("first code matches"), Rotator.Matches(First, 1050.0));
	TestTrue(TEXT("matches after normalisation"), Rotator.Matches(First.ToLower(), 1050.0));
	TestEqual(TEXT("stable within its lifetime"), Rotator.Current(1099.0), First);

	Rotator.Rotate(1060.0);
	const FString Second = Rotator.Current(1060.0);
	TestNotEqual(TEXT("rotation changes the code"), Second, First);
	TestEqual(TEXT("generation 2"), Rotator.GetGeneration(), 2);
	TestFalse(TEXT("old code rejected"), Rotator.Matches(First, 1061.0));
	TestTrue(TEXT("new code accepted"), Rotator.Matches(Second, 1061.0));

	TestFalse(TEXT("expired code rejected"), Rotator.Matches(Second, 1160.0));
	const FString Third = Rotator.Current(1160.0);
	TestNotEqual(TEXT("expiry rotates on read"), Third, Second);
	TestEqual(TEXT("generation 3"), Rotator.GetGeneration(), 3);

	Rotator.LifetimeSeconds = 0.0;
	TestEqual(TEXT("lifetime 0 never expires"), Rotator.Current(1.0e9), Third);

	Rotator.Reset();
	TestFalse(TEXT("reset clears"), Rotator.IsActive());
	TestEqual(TEXT("reset clears the generation"), Rotator.GetGeneration(), 0);
	return true;
}

// ---- DF.Online.ContentHashDeterministic --------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDFOnlineContentHashTest, "DF.Online.ContentHashDeterministic", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FDFOnlineContentHashTest::RunTest(const FString& Parameters)
{
	auto MakeBalance = [](const TCHAR* Name, float Dial) -> UDataTable*
	{
		UDataTable* Table = NewObject<UDataTable>(GetTransientPackage(), Name);
		Table->RowStruct = FDFBalanceRow::StaticStruct();
		FDFBalanceRow Row;
		Row.Dials.Add(TEXT("startingMoney"), Dial);
		Row.Dials.Add(TEXT("lives"), 20.f);
		Table->AddRow(TEXT("balance"), Row);
		return Table;
	};
	UDataTable* A = MakeBalance(TEXT("DT_HashA"), 500.f);
	UDataTable* B = MakeBalance(TEXT("DT_HashB"), 500.f);
	UDataTable* C = MakeBalance(TEXT("DT_HashC"), 501.f);

	const FString HashA = UDFContentHash::HashTables({ A });
	TestEqual(TEXT("sha1 hex is 40 chars"), HashA.Len(), 40);
	TestEqual(TEXT("same table hashes the same"), UDFContentHash::HashTables({ A }), HashA);
	TestNotEqual(TEXT("a changed dial changes the hash"), UDFContentHash::HashTables({ C }), HashA);
	TestNotEqual(TEXT("the table name is part of the hash"), UDFContentHash::HashTables({ B }), HashA);
	TestEqual(TEXT("order of tables does not matter"), UDFContentHash::HashTables({ A, B }), UDFContentHash::HashTables({ B, A }));
	TestEqual(TEXT("empty set is well defined"), UDFContentHash::HashTables({}), UDFContentHash::Sha1Hex(FString()));
	TestEqual(TEXT("known sha1 of empty string"), UDFContentHash::Sha1Hex(FString()), FString(TEXT("da39a3ee5e6b4b0d3255bfef95601890afd80709")));

	// The project hash: imported tables when the importer has run, the JSON source otherwise; stable across calls.
	EDFContentHashSource Source = EDFContentHashSource::None;
	const FString Project = UDFContentHash::ComputeForProjectSource(Source);
	TestEqual(TEXT("project hash is sha1 hex"), Project.Len(), 40);
	TestTrue(TEXT("project hash has a source (tables or unreal/content/json)"), Source != EDFContentHashSource::None);
	EDFContentHashSource Again = EDFContentHashSource::None;
	TestEqual(TEXT("project hash is stable"), UDFContentHash::ComputeForProjectSource(Again), Project);
	TestEqual(TEXT("project hash source is stable"), Again, Source);
	AddInfo(FString::Printf(TEXT("project content hash %s from %s"), *Project, Source == EDFContentHashSource::Tables ? TEXT("tables") : TEXT("json source")));
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
