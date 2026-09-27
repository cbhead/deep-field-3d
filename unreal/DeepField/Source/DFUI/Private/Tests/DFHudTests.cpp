#include "Attributes/DFHealthSet.h"
#include "DFHeroCharacter.h"
#include "DFMatchState.h"
#include "DFPlayerState.h"
#include "Economy/DFEconomyStateComponent.h"
#include "GameFramework/PlayerController.h"
#include "Hero/DFHeroStateComponent.h"
#include "Misc/AutomationTest.h"
#include "Screens/DFHudStyle.h"
#include "Screens/DFHudText.h"
#include "Screens/DFUIRootSubsystem.h"
#include "Testing/DFTestUtils.h"
#include "Tokens/DFUITokenNames.h"
#include "Tokens/DFUITokens.h"
#include "ViewModels/DFMatchFeedSubsystem.h"
#include "ViewModels/DFMatchStateFeed.h"
#include "ViewModels/DFMatchViewModel.h"
#include "ViewModels/DFPlayerViewModel.h"
#include "ViewModels/DFViewModelSubsystem.h"
#include "Waves/DFWaveDirector.h"
#include "Waves/DFWavePlan.h"

#if WITH_DEV_AUTOMATION_TESTS

// DF.UI.Feed.* - the real feed: ADFMatchState, its economy, the seats and the hero pawns, copied into the
// view models the way every machine sees them. A test world has no game mode, so its actors never begin
// play or tick: the match and the director are driven by hand, as DFMatchStateTests.cpp does.
// DF.UI.Hud.* - what the first HUD says and how it looks, without a widget (the widget needs a renderer,
// which the -nullrhi gate does not have; the layout is checked in the running game).

namespace DFHudTest
{
	constexpr EAutomationTestFlags Flags = EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter;

	/** Two waves on one lane: four grunts, then three grunts and two lurkers (DFMatchStateTests' fixture). */
	FDFWavePlanTables Tables()
	{
		FDFWavePlanTables T;
		T.MapId = TEXT("hudFixture");
		T.TickHz = 30.f;
		T.Dials.CountScalePerExtraPlayer = 0.6f;
		T.Dials.HpScalePerExtraPlayer = 0.15f;
		T.Dials.EndlessCountGrowthPerLap = 1.15f;
		T.Dials.HpGrowth = 1.22f;
		T.Dials.EndlessHpGrowth = 1.15f;
		T.Dials.BountyScale = 1.f;
		T.Dials.BountyGrowth = 1.15f;
		T.Dials.ScrapGrowth = 1.15f;
		T.Enemies.Add(TEXT("grunt"));
		T.Enemies.Add(TEXT("lurker"));
		T.Waves.AddDefaulted(2);
		T.Waves[0].Add({ TEXT("grunt"), 4, 20, 0, TEXT("ground") });
		T.Waves[1].Add({ TEXT("grunt"), 3, 15, 0, TEXT("ground") });
		T.Waves[1].Add({ TEXT("lurker"), 2, 30, 10, TEXT("ground") });
		return T;
	}

	FString Str(const FText& Text)
	{
		return Text.ToString();
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDFUIFeedMatchStateTest, "DF.UI.Feed.MatchStateFillsViewModel", DFHudTest::Flags)
bool FDFUIFeedMatchStateTest::RunTest(const FString&)
{
	using namespace DFHudTest;
	FDFTestWorld World;
	ADFWaveDirector* Director = World.SpawnActor<ADFWaveDirector>();
	ADFMatchState* State = World.SpawnActor<ADFMatchState>();
	if (!TestNotNull(TEXT("director"), Director) || !TestNotNull(TEXT("match state"), State))
	{
		return false;
	}
	FString Error;
	if (!TestTrue(TEXT("fixture tables configure"), Director->ConfigureWithTables(7u, Tables(), Error)))
	{
		return false;
	}
	FDFMatchSettings Settings;
	Settings.IntermissionSeconds = 2.f;
	State->ConfigureMatch(Settings);
	State->UseWaveDirector(Director);
	UDFEconomyStateComponent* Economy = State->GetEconomy();
	if (!TestNotNull(TEXT("the match state carries WS-06's economy"), Economy))
	{
		return false;
	}
	UDFMatchViewModel* Match = NewObject<UDFMatchViewModel>();

	// Before the first wave: the arc, the countdown, the starting money and lives.
	FDFMatchStateFeed::Fill(*Match, *State, nullptr);
	TestTrue(TEXT("a filled model is valid"), Match->IsValid());
	TestEqual(TEXT("no wave yet"), Match->GetWaveIndex(), -1);
	TestEqual(TEXT("the arc's length"), Match->GetTotalWaves(), 2);
	TestEqual(TEXT("intermission"), Match->GetPhase(), EDFMatchPhase::Intermission);
	TestEqual(TEXT("the countdown is the replicated end time read now"), Match->GetPhaseSecondsLeft(), 2.f, 0.01f);
	TestEqual(TEXT("money is the economy's"), Match->GetMoney(), UDFEconomyStateComponent::DefaultStartingMoney);
	TestEqual(TEXT("lives are the economy's"), Match->GetLives(), UDFEconomyStateComponent::DefaultStartingLives);
	TestNull(TEXT("no local player on a machine with none"), Match->Local());
	TestEqual(TEXT("title before the first wave"), Str(DFHudText::WaveTitle(*Match)), FString(TEXT("Wave -- / 2")));
	TestEqual(TEXT("first-wave countdown"), Str(DFHudText::PhaseLine(*Match)), FString(TEXT("First wave in 2s")));

	// Spending and a leak move the numbers; the countdown follows the clock.
	TestTrue(TEXT("a Lance's worth spent"), Economy->TrySpend(75, nullptr));
	Economy->TakeLives(3);
	State->AdvanceMatch(1.2f);
	FDFMatchStateFeed::Fill(*Match, *State, nullptr);
	TestEqual(TEXT("money after spending"), Match->GetMoney(), UDFEconomyStateComponent::DefaultStartingMoney - 75);
	TestEqual(TEXT("lives after a leak"), Match->GetLives(), UDFEconomyStateComponent::DefaultStartingLives - 3);
	TestEqual(TEXT("0.8 s left reads as 1 s"), Str(DFHudText::PhaseLine(*Match)), FString(TEXT("First wave in 1s")));

	// The first wave: running, with its bodies counted.
	State->AdvanceMatch(1.f);
	Director->Tick(60.f);
	State->AdvanceMatch(0.f);
	FDFMatchStateFeed::Fill(*Match, *State, nullptr);
	TestEqual(TEXT("wave 0"), Match->GetWaveIndex(), 0);
	TestEqual(TEXT("phase Wave"), Match->GetPhase(), EDFMatchPhase::Wave);
	TestEqual(TEXT("no countdown during a wave"), Match->GetPhaseSecondsLeft(), 0.f);
	TestEqual(TEXT("four grunts remaining"), Match->GetEnemiesRemaining(), 4);
	TestEqual(TEXT("title during wave 1"), Str(DFHudText::WaveTitle(*Match)), FString(TEXT("Wave 1 / 2")));
	TestEqual(TEXT("phase line during a wave"), Str(DFHudText::PhaseLine(*Match)), FString(TEXT("Wave in progress")));

	// Cleared: the break before the next.
	Director->NotifyEnemyRemoved(4);
	FDFMatchStateFeed::Fill(*Match, *State, nullptr);
	TestEqual(TEXT("back to intermission"), Match->GetPhase(), EDFMatchPhase::Intermission);
	TestEqual(TEXT("nothing remaining"), Match->GetEnemiesRemaining(), 0);
	TestEqual(TEXT("the next-wave countdown"), Str(DFHudText::PhaseLine(*Match)), FString(TEXT("Next wave in 2s")));

	// The last wave, and the core falls in it.
	State->AdvanceMatch(2.1f);
	Economy->TakeLives(Economy->GetLives());
	State->AdvanceMatch(0.016f);
	FDFMatchStateFeed::Fill(*Match, *State, nullptr);
	TestEqual(TEXT("defeat"), Match->GetPhase(), EDFMatchPhase::Defeat);
	TestEqual(TEXT("no lives"), Match->GetLives(), 0);
	TestEqual(TEXT("banner"), Str(DFHudText::Banner(Match->GetPhase())), FString(TEXT("DEFEAT")));
	TestEqual(TEXT("banner detail"), Str(DFHudText::BannerDetail(*Match)), FString(TEXT("The core fell on wave 2")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDFUIFeedSeatsTest, "DF.UI.Feed.SeatsAndLocalHero", DFHudTest::Flags)
bool FDFUIFeedSeatsTest::RunTest(const FString&)
{
	FDFTestWorld World;
	ADFMatchState* State = World.SpawnActor<ADFMatchState>();
	ADFPlayerState* Me = World.SpawnActor<ADFPlayerState>();
	ADFPlayerState* Mate = World.SpawnActor<ADFPlayerState>();
	ADFPlayerState* Joining = World.SpawnActor<ADFPlayerState>();
	ADFHeroCharacter* Hero = World.SpawnActor<ADFHeroCharacter>();
	if (!TestNotNull(TEXT("match state"), State) || !TestNotNull(TEXT("players"), Me) || !TestNotNull(TEXT("mate"), Mate)
		|| !TestNotNull(TEXT("joining"), Joining) || !TestNotNull(TEXT("hero"), Hero) || !TestNotNull(TEXT("hero health"), Hero->GetHealthSet()))
	{
		return false;
	}
	State->ConfigureMatch(FDFMatchSettings());
	Me->SetSeat(1);
	Me->SetPlayerName(TEXT("Rook"));
	Mate->SetSeat(2);
	// A test world has no game mode, so the player states have not registered with the match state themselves.
	State->AddPlayerState(Me);
	State->AddPlayerState(Mate);
	State->AddPlayerState(Joining);
	// The local hero, hurt: the attribute set is what replicates, so it is what the feed reads.
	Hero->SetPlayerState(Me);
	Hero->GetHealthSet()->InitMaxHealth(100.f);
	Hero->GetHealthSet()->InitHealth(64.f);

	UDFMatchViewModel* Match = NewObject<UDFMatchViewModel>();
	FDFMatchStateFeed::Fill(*Match, *State, Me);
	TestEqual(TEXT("two seated players (one not seated yet is left out)"), Match->GetPlayers().Num(), 2);
	TestEqual(TEXT("LocalPlayerId is the local seat"), Match->GetLocalPlayerId(), 1);
	UDFPlayerViewModel* Local = Match->Local();
	if (!TestNotNull(TEXT("Local()"), Local))
	{
		return false;
	}
	TestEqual(TEXT("name"), Local->GetName(), FString(TEXT("Rook")));
	TestEqual(TEXT("hp from the hero's health set"), Local->GetHp(), 64.f);
	TestEqual(TEXT("max hp"), Local->GetMaxHp(), 100.f);
	TestEqual(TEXT("HpFrac"), Local->HpFrac(), 0.64f, 1e-4f);
	TestFalse(TEXT("standing"), Local->IsDowned());

	// A teammate goes down; a hit lands on the local hero; the model keeps its children.
	Mate->GetHeroState()->HostDeplete(2);
	Hero->GetHealthSet()->InitHealth(12.f);
	FDFMatchStateFeed::Fill(*Match, *State, Me);
	TestTrue(TEXT("the teammate is downed in the model"), Match->FindPlayer(2) && Match->FindPlayer(2)->IsDowned());
	TestTrue(TEXT("the local player keeps its view model"), Match->Local() == Local);
	TestEqual(TEXT("hp follows the attribute"), Local->GetHp(), 12.f);

	// The teammate leaves.
	State->RemovePlayerState(Mate);
	FDFMatchStateFeed::Fill(*Match, *State, Me);
	TestEqual(TEXT("one player left"), Match->GetPlayers().Num(), 1);
	TestNull(TEXT("the leaver is gone"), Match->FindPlayer(2));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDFUIFeedSubsystemTest, "DF.UI.Feed.SubsystemFollowsTheWorld", DFHudTest::Flags)
bool FDFUIFeedSubsystemTest::RunTest(const FString&)
{
	FDFTestWorld World;
	UDFMatchFeedSubsystem* Feed = World.GetWorld()->GetSubsystem<UDFMatchFeedSubsystem>();
	const UDFViewModelSubsystem* ViewModels = World.GetSubsystem<UDFViewModelSubsystem>();
	UDFMatchViewModel* Match = ViewModels ? ViewModels->GetMatch() : nullptr;
	if (!TestNotNull(TEXT("a game world has the feed"), Feed) || !TestNotNull(TEXT("and the game instance its match view model"), Match))
	{
		return false;
	}

	// No match state: the model is left to whatever feeds it (L_Test_UI's fake feed).
	TestFalse(TEXT("nothing to feed from"), Feed->Refresh());
	TestFalse(TEXT("the model is untouched"), Match->IsValid());

	ADFMatchState* State = World.SpawnActor<ADFMatchState>();
	if (!TestNotNull(TEXT("match state"), State))
	{
		return false;
	}
	State->ConfigureMatch(FDFMatchSettings());
	World.GetWorld()->SetGameState(State);
	TestTrue(TEXT("the world's match state is fed"), Feed->Refresh());
	TestTrue(TEXT("into the game instance's model"), Match->IsValid() && Match->GetMoney() == UDFEconomyStateComponent::DefaultStartingMoney);

	// Every frame, by itself.
	State->GetEconomy()->AddMoney(40);
	World.Tick();
	TestEqual(TEXT("a world tick runs the feed"), Match->GetMoney(), UDFEconomyStateComponent::DefaultStartingMoney + 40);

	World.GetWorld()->SetGameState(nullptr);
	TestFalse(TEXT("the match state gone: nothing fed"), Feed->Refresh());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDFUIHudTextTest, "DF.UI.Hud.Text", DFHudTest::Flags)
bool FDFUIHudTextTest::RunTest(const FString&)
{
	using DFHudTest::Str;

	// Refusals: the one the first playable meets most has words; the rest are shown as the host sent them.
	TestEqual(TEXT("insufficientFunds"), Str(DFHudText::Refusal(TEXT("insufficientFunds"))), FString(TEXT("Not enough money")));
	TestEqual(TEXT("occupied is raw"), Str(DFHudText::Refusal(TEXT("occupied"))), FString(TEXT("occupied")));
	TestEqual(TEXT("wouldSeal is raw"), Str(DFHudText::Refusal(TEXT("wouldSeal"))), FString(TEXT("wouldSeal")));
	TestEqual(TEXT("no reason"), Str(DFHudText::Refusal(NAME_None)), FString(TEXT("Refused")));

	TestEqual(TEXT("the build hint"), Str(DFHudText::BuildHint(TEXT("lance"), 75)), FString(TEXT("Hold E on a pad: build Lance (75)   Hold U on a tower: upgrade   Hold X: sell")));

	TestEqual(TEXT("ammo"), Str(DFHudText::Ammo(18, 24, false, 0.f)), FString(TEXT("18 / 24")));
	TestEqual(TEXT("empty"), Str(DFHudText::Ammo(0, 24, false, 0.f)), FString(TEXT("0 / 24")));
	TestEqual(TEXT("reloading, whole percent down"), Str(DFHudText::Ammo(0, 24, true, 0.406f)), FString(TEXT("RELOADING 40%")));
	TestEqual(TEXT("reloading never reads 100%"), Str(DFHudText::Ammo(0, 24, true, 1.f)), FString(TEXT("RELOADING 99%")));

	UDFMatchViewModel* Match = NewObject<UDFMatchViewModel>();
	Match->SetTotalWaves(10);
	Match->SetWaveIndex(1);
	Match->SetPhase(EDFMatchPhase::Wave);
	TestEqual(TEXT("mid-arc"), Str(DFHudText::WaveTitle(*Match)), FString(TEXT("Wave 2 / 10")));
	Match->SetPhase(EDFMatchPhase::Intermission);
	Match->SetPhaseSecondsLeft(7.2f);
	TestEqual(TEXT("next wave, rounded up"), Str(DFHudText::PhaseLine(*Match)), FString(TEXT("Next wave in 8s")));
	Match->SetPhaseSecondsLeft(0.f);
	TestEqual(TEXT("clock stopped"), Str(DFHudText::PhaseLine(*Match)), FString(TEXT("Waiting for players")));

	Match->SetEndless(true);
	Match->SetLap(0);
	TestEqual(TEXT("endless, first pass: still the arc"), Str(DFHudText::WaveTitle(*Match)), FString(TEXT("Wave 2 / 10")));
	Match->SetLap(2);
	Match->SetWaveIndex(22);
	TestEqual(TEXT("endless past the arc"), Str(DFHudText::WaveTitle(*Match)), FString(TEXT("Endless, lap 2 · wave 23")));

	Match->SetEndless(false);
	Match->SetLap(0);
	Match->SetTotalWaves(0);
	Match->SetWaveIndex(-1);
	TestEqual(TEXT("a dev map"), Str(DFHudText::WaveTitle(*Match)), FString(TEXT("No waves on this map")));

	Match->SetLobby(true);
	TestEqual(TEXT("lobby title"), Str(DFHudText::WaveTitle(*Match)), FString(TEXT("Lobby")));
	TestEqual(TEXT("lobby line"), Str(DFHudText::PhaseLine(*Match)), FString(TEXT("Waiting for the host to launch")));

	Match->SetLobby(false);
	Match->SetTotalWaves(10);
	Match->SetWaveIndex(9);
	Match->SetPhase(EDFMatchPhase::Victory);
	TestEqual(TEXT("victory banner"), Str(DFHudText::Banner(EDFMatchPhase::Victory)), FString(TEXT("VICTORY")));
	TestEqual(TEXT("victory detail"), Str(DFHudText::BannerDetail(*Match)), FString(TEXT("All 10 waves held")));
	TestTrue(TEXT("no banner while the match runs"), DFHudText::Banner(EDFMatchPhase::Wave).IsEmpty() && DFHudText::Banner(EDFMatchPhase::Intermission).IsEmpty());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDFUIHudStyleTest, "DF.UI.Hud.StyleFromTokens", DFHudTest::Flags)
bool FDFUIHudStyleTest::RunTest(const FString&)
{
	// No token set (a cooked build before DA_UITokens): the built-in values, and not one error logged -
	// an error here would fail this test, which is the point.
	const FDFHudStyle BuiltIn = FDFHudStyle::From(nullptr);
	TestEqual(TEXT("source"), BuiltIn.Source, FString(TEXT("built-in")));
	const FDFHudStyle Empty = FDFHudStyle::From(NewObject<UDFUITokens>());
	TestTrue(TEXT("an empty token set falls back silently too"), Empty.Money.Equals(BuiltIn.Money));

	// The real design system: every value the HUD uses is its token, and the built-in copy is the same
	// value, so a design drop that changes one of them fails here rather than drifting unseen.
	UDFUITokens* Tokens = NewObject<UDFUITokens>();
	TArray<FString> Errors;
	if (!TestTrue(TEXT("the design system parses"), Tokens->FillFromDesignSystem(UDFUITokens::DesignSystemDir(), Errors)))
	{
		return false;
	}
	const FDFHudStyle Real = FDFHudStyle::From(Tokens);
	struct FCase { const TCHAR* What; FLinearColor FDFHudStyle::* Field; FName Token; };
	const FCase Cases[] = {
		{ TEXT("panel"), &FDFHudStyle::Panel, DFTokens::SurfaceGlass },
		{ TEXT("panel edge"), &FDFHudStyle::PanelEdge, DFTokens::BorderPanel },
		{ TEXT("scrim"), &FDFHudStyle::Scrim, DFTokens::SurfaceOverlay },
		{ TEXT("text"), &FDFHudStyle::Text, DFTokens::TextPrimary },
		{ TEXT("text secondary"), &FDFHudStyle::TextSecondary, DFTokens::TextSecondary },
		{ TEXT("accent"), &FDFHudStyle::Accent, DFTokens::TextAccent },
		{ TEXT("money"), &FDFHudStyle::Money, DFTokens::ResGold },
		{ TEXT("lives"), &FDFHudStyle::Lives, DFTokens::Lives },
		{ TEXT("danger"), &FDFHudStyle::Danger, DFTokens::StateDanger },
		{ TEXT("wave idle"), &FDFHudStyle::WaveIdle, DFTokens::WaveIdle },
		{ TEXT("wave active"), &FDFHudStyle::WaveActive, DFTokens::WaveActive },
		{ TEXT("hp bar"), &FDFHudStyle::HpBar, DFTokens::BarHp },
		{ TEXT("hp low"), &FDFHudStyle::HpLow, DFTokens::BarHpLow },
		{ TEXT("bar track"), &FDFHudStyle::BarTrack, DFTokens::BarTrack },
	};
	for (const FCase& Case : Cases)
	{
		TestTrue(FString::Printf(TEXT("%s is --%s"), Case.What, *Case.Token.ToString()), (Real.*Case.Field).Equals(Tokens->Color(Case.Token), 1e-4f));
		TestTrue(FString::Printf(TEXT("the built-in %s matches --%s"), Case.What, *Case.Token.ToString()), (BuiltIn.*Case.Field).Equals(Tokens->Color(Case.Token), 1e-4f));
	}
	TestEqual(TEXT("hud-edge"), Real.Edge, Tokens->Length(DFTokens::HudEdge));
	TestEqual(TEXT("the built-in hud-edge"), BuiltIn.Edge, Tokens->Length(DFTokens::HudEdge));
	TestEqual(TEXT("the built-in title size"), BuiltIn.SizeTitle, Tokens->Length(DFTokens::SizeDisplayMd));
	TestEqual(TEXT("the built-in banner size"), BuiltIn.SizeBanner, Tokens->Length(DFTokens::SizeDisplayXl));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDFUIHudNotInTestsTest, "DF.UI.Hud.NoHudInTestWorlds", DFHudTest::Flags)
bool FDFUIHudNotInTestsTest::RunTest(const FString&)
{
	// The landing gate plays real levels in PIE (DF.Func.*) with -nullrhi; none of them may grow a HUD.
	FDFTestWorld World;
	APlayerController* Controller = World.SpawnActor<APlayerController>();
	TestNotNull(TEXT("a controller in a game world"), Controller);
	TestFalse(TEXT("no UI for a controller during an automation run"), UDFUIRootSubsystem::ShouldShowUI(Controller));
	TestFalse(TEXT("no UI without a controller"), UDFUIRootSubsystem::ShouldShowUI(nullptr));
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
