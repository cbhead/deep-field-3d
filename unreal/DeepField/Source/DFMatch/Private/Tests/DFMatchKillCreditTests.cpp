#include "AbilitySystemComponent.h"
#include "Attributes/DFHealthSet.h"
#include "DFGameplayTags.h"
#include "DFMatchState.h"
#include "DFPlayerState.h"
#include "Damage/DFDamageContext.h"
#include "Economy/DFEconomyStateComponent.h"
#include "Effects/DFGE_Damage.h"
#include "Enemies/DFEnemy.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "Match/DFSeatHolder.h"
#include "Misc/AutomationTest.h"
#include "Testing/DFTestUtils.h"

#if WITH_DEV_AUTOMATION_TESTS

// DF.Unit.Match.* (the seats' records) — a hero's kills and damage reach its ADFPlayerState. ADFEnemy
// (DFEnemies) names the seat behind a hit through IDFSeatHolder (DFCore) and says it in
// DF.Message.EnemyDamaged / EnemyKilled; ADFMatchState credits the seat on the host. The hero is a
// stand-in pawn that holds a player state, as ADFHeroCharacter does in play; the hit is the rifle's
// (UDFGE_Damage with the pawn as the instigator, DFHeroWeaponComponent::HostApplyShot).

namespace DFMatchCreditTest
{
	constexpr EAutomationTestFlags Flags = EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter;

	/** A test world whose actors begin play (so the match state and the enemy subscribe and bind as in a
	 *  game), and ends that play before FDFTestWorld tears it down (CleanupWorld warns otherwise). */
	struct FPlayWorld
	{
		FDFTestWorld World;

		FPlayWorld()
		{
			// No game mode: the world settings flip the begun-play flag (see DFStatusComponentTests).
			World.GetWorld()->GetWorldSettings()->NotifyBeginPlay();
		}

		~FPlayWorld()
		{
			if (UWorld* W = World.GetWorld(); W && W->HasBegunPlay())
			{
				W->EndPlay(EEndPlayReason::Quit);
			}
		}

		ADFPlayerState* Seat(int32 SeatIndex) const
		{
			ADFPlayerState* Player = World.SpawnActor<ADFPlayerState>();
			if (Player)
			{
				Player->SetSeat(SeatIndex);
			}
			return Player;
		}

		/** A hero stand-in: a pawn played by Player, the instigator its shots carry. */
		APawn* Hero(ADFPlayerState* Player) const
		{
			APawn* Pawn = World.SpawnActor<APawn>();
			if (Pawn)
			{
				Pawn->SetPlayerState(Player);
			}
			return Pawn;
		}

		ADFEnemy* Enemy(float Health) const
		{
			ADFEnemy* Body = World.SpawnActor<ADFEnemy>();
			if (Body)
			{
				Body->GetHealthSet()->InitMaxHealth(Health);
				Body->GetHealthSet()->InitHealth(Health);
			}
			return Body;
		}
	};

	/** A hero's shot as the rifle lands it: UDFGE_Damage, set by caller, the shooter as instigator. */
	void Hit(ADFEnemy& Enemy, AActor* Shooter, float Amount)
	{
		UAbilitySystemComponent* ASC = Enemy.GetAbilitySystemComponent();
		FGameplayEffectContextHandle Context = ASC->MakeEffectContext();
		Context.AddInstigator(Shooter, Shooter);
		UDFDamageContext* Damage = UDFDamageContext::Make(Shooter, DFTags::Damage_Type_Kinetic, DFTags::Damage_Source_Hero);
		Damage->AttachTo(Context);
		FGameplayEffectSpecHandle Spec = ASC->MakeOutgoingSpec(UDFGE_Damage::StaticClass(), 1.f, Context);
		if (Spec.IsValid())
		{
			Spec.Data->SetSetByCallerMagnitude(DFTags::SetByCaller_Damage, Amount);
			ASC->ApplyGameplayEffectSpecToSelf(*Spec.Data);
		}
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDFMatchSeatOfTest, "DF.Unit.Match.SeatOfFindsThePlayer", DFMatchCreditTest::Flags)
bool FDFMatchSeatOfTest::RunTest(const FString& Parameters)
{
	FDFTestWorld World;
	ADFPlayerState* Player = World.SpawnActor<ADFPlayerState>();
	ADFPlayerState* Unseated = World.SpawnActor<ADFPlayerState>();
	APawn* Pawn = World.SpawnActor<APawn>();
	APawn* Nobody = World.SpawnActor<APawn>();
	APlayerController* Controller = World.SpawnActor<APlayerController>();
	AActor* Thing = World.SpawnActor<AActor>();
	if (!TestNotNull(TEXT("player state"), Player) || !TestNotNull(TEXT("unseated"), Unseated) || !TestNotNull(TEXT("pawn"), Pawn)
		|| !TestNotNull(TEXT("pawn with nobody"), Nobody) || !TestNotNull(TEXT("controller"), Controller) || !TestNotNull(TEXT("thing"), Thing))
	{
		return false;
	}
	Player->SetSeat(3);
	Pawn->SetPlayerState(Player);
	Controller->SetPlayerState(Player);

	TestEqual(TEXT("the player state answers its own seat"), IDFSeatHolder::SeatOf(Player), 3);
	TestEqual(TEXT("through GetMatchSeat"), Cast<IDFSeatHolder>(Player) ? Cast<IDFSeatHolder>(Player)->GetMatchSeat() : -1, 3);
	TestEqual(TEXT("a pawn: its player state's seat (a hero's shot names its pawn)"), IDFSeatHolder::SeatOf(Pawn), 3);
	TestEqual(TEXT("a controller: its player state's seat"), IDFSeatHolder::SeatOf(Controller), 3);
	TestEqual(TEXT("an unseated player: 0"), IDFSeatHolder::SeatOf(Unseated), 0);
	TestEqual(TEXT("a pawn nobody plays: 0"), IDFSeatHolder::SeatOf(Nobody), 0);
	TestEqual(TEXT("anything else (a tower, a hazard): 0"), IDFSeatHolder::SeatOf(Thing), 0);
	TestEqual(TEXT("nothing: 0"), IDFSeatHolder::SeatOf(nullptr), 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDFMatchHeroKillCreditedTest, "DF.Unit.Match.HeroKillCredited", DFMatchCreditTest::Flags)
bool FDFMatchHeroKillCreditedTest::RunTest(const FString& Parameters)
{
	using namespace DFMatchCreditTest;
	FPlayWorld W;
	ADFMatchState* Match = W.World.SpawnActor<ADFMatchState>();
	ADFPlayerState* Rook = W.Seat(1);
	ADFPlayerState* Ash = W.Seat(2);
	APawn* AshHero = W.Hero(Ash);
	AActor* Hazard = W.World.SpawnActor<AActor>();
	ADFEnemy* Target = W.Enemy(100.f);
	ADFEnemy* Other = W.Enemy(10.f);
	if (!TestNotNull(TEXT("match state"), Match) || !TestNotNull(TEXT("seat 1"), Rook) || !TestNotNull(TEXT("seat 2"), Ash)
		|| !TestNotNull(TEXT("seat 2's hero"), AshHero) || !TestNotNull(TEXT("hazard"), Hazard)
		|| !TestNotNull(TEXT("enemy"), Target) || !TestNotNull(TEXT("other enemy"), Other))
	{
		return false;
	}
	TestTrue(TEXT("the seats joined the match (the player array)"),
		Match->PlayerArray.Contains(static_cast<APlayerState*>(Rook)) && Match->PlayerArray.Contains(static_cast<APlayerState*>(Ash)));
	FDFMessageCapture Damaged(W.World.MessageBus(), DFTags::Message_EnemyDamaged, /*bIncludeChildren*/ false);
	FDFMessageCapture Killed(W.World.MessageBus(), DFTags::Message_EnemyKilled, /*bIncludeChildren*/ false);

	// Two shots from seat 2's hero: each is announced with the seat, and the seat's record takes it.
	Hit(*Target, AshHero, 30.f);
	if (!TestEqual(TEXT("one EnemyDamaged"), Damaged.Num(), 1))
	{
		return false;
	}
	const FDFMsg_Damage* First = Damaged.PayloadAt<FDFMsg_Damage>(0);
	if (TestNotNull(TEXT("an FDFMsg_Damage"), First))
	{
		TestEqual(TEXT("from seat 2"), First->SourcePlayerId, 2);
		TestEqual(TEXT("not from a structure"), First->SourceStructureId, 0);
		TestEqual(TEXT("30 dealt"), First->Amount, 30.f, 0.01f);
		TestEqual(TEXT("70% left"), First->RemainingFraction, 0.7f, 0.001f);
		TestEqual(TEXT("the hero's source tag"), First->DamageSource, DFTags::Damage_Source_Hero.GetTag());
	}
	TestEqual(TEXT("seat 2's damage"), Ash->GetDamageDealt(), 30.f, 0.01f);
	TestEqual(TEXT("seat 1 did nothing"), Rook->GetDamageDealt(), 0.f);

	// The lethal shot: its whole amount counts (Step.cs adds `total`, overkill included); the body dies
	// the frame after, and the kill is the seat's.
	Hit(*Target, AshHero, 80.f);
	TestEqual(TEXT("seat 2's damage, overkill included"), Ash->GetDamageDealt(), 110.f, 0.01f);
	TestEqual(TEXT("no kill inside the hit"), Killed.Num(), 0);
	W.World.Tick();
	if (!TestEqual(TEXT("one EnemyKilled the frame after"), Killed.Num(), 1))
	{
		return false;
	}
	const FDFMsg_Kill* Kill = Killed.PayloadAt<FDFMsg_Kill>(0);
	if (TestNotNull(TEXT("an FDFMsg_Kill"), Kill))
	{
		TestEqual(TEXT("killed by seat 2"), Kill->KillerPlayerId, 2);
		TestEqual(TEXT("by no structure"), Kill->KillerStructureId, 0);
	}
	TestEqual(TEXT("seat 2 has the kill"), Ash->GetKills(), 1);
	TestEqual(TEXT("and its match XP (Step.cs:2092)"), Ash->GetMatchXp(), ADFPlayerState::KillMatchXp);
	TestEqual(TEXT("seat 1 has none"), Rook->GetKills(), 0);

	// A body killed by something with no seat behind it: announced with seat 0, credited to nobody.
	Hit(*Other, Hazard, 50.f);
	W.World.Tick();
	if (TestEqual(TEXT("the second kill"), Killed.Num(), 2))
	{
		TestEqual(TEXT("no seat behind a hazard"), Killed.PayloadAt<FDFMsg_Kill>(1)->KillerPlayerId, 0);
	}
	TestEqual(TEXT("seat 2 still has one kill"), Ash->GetKills(), 1);
	TestEqual(TEXT("and no hazard damage"), Ash->GetDamageDealt(), 110.f, 0.01f);

	// A hit on a corpse is not a hit (Step.cs Damage() returns for a dead body).
	const int32 HitsBefore = Damaged.Num();
	ADFEnemy* Corpse = W.Enemy(10.f);
	if (TestNotNull(TEXT("third enemy"), Corpse))
	{
		Hit(*Corpse, AshHero, 20.f);
		Hit(*Corpse, AshHero, 20.f);
		TestEqual(TEXT("only the lethal hit on it is announced"), Damaged.Num(), HitsBefore + 1);
		TestEqual(TEXT("and credited"), Ash->GetDamageDealt(), 130.f, 0.01f);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDFMatchNothingListensAfterTest, "DF.Unit.Match.NothingListensAfterTheMatch", DFMatchCreditTest::Flags)
bool FDFMatchNothingListensAfterTest::RunTest(const FString& Parameters)
{
	using namespace DFMatchCreditTest;
	// A new match reloads the map, but the message bus is the game instance's and outlives it. So a
	// match that has ended play must hear nothing more: this plays the reload in one world, with the old
	// match state ended (as a map change ends it) and a new one begun, on the same bus.
	FPlayWorld W;
	ADFMatchState* Old = W.World.SpawnActor<ADFMatchState>();
	ADFPlayerState* Player = W.Seat(1);
	UDFMessageBus* Bus = W.World.MessageBus();
	if (!TestNotNull(TEXT("old match"), Old) || !TestNotNull(TEXT("seat 1"), Player) || !TestNotNull(TEXT("bus"), Bus) || !TestNotNull(TEXT("old economy"), Old->GetEconomy()))
	{
		return false;
	}
	FDFMsg_Kill Kill;
	Kill.KillerPlayerId = 1;
	Kill.Bounty = 5;
	FDFMsg_Damage Damage;
	Damage.SourcePlayerId = 1;
	Damage.Amount = 12.f;

	const int32 OldMoney = Old->GetEconomy()->GetMoney();
	Bus->Broadcast(DFTags::Message_EnemyKilled, Kill);
	Bus->Broadcast(DFTags::Message_EnemyDamaged, Damage);
	TestEqual(TEXT("the running match credits the kill"), Player->GetKills(), 1);
	TestEqual(TEXT("and the damage"), Player->GetDamageDealt(), 12.f, 0.01f);
	TestEqual(TEXT("and its economy pays the bounty"), Old->GetEconomy()->GetMoney(), OldMoney + 5);

	Old->RouteEndPlay(EEndPlayReason::LevelTransition);
	Bus->Broadcast(DFTags::Message_EnemyKilled, Kill);
	Bus->Broadcast(DFTags::Message_EnemyDamaged, Damage);
	TestEqual(TEXT("an ended match credits nothing"), Player->GetKills(), 1);
	TestEqual(TEXT("no damage either"), Player->GetDamageDealt(), 12.f, 0.01f);
	TestEqual(TEXT("its economy pays nothing"), Old->GetEconomy()->GetMoney(), OldMoney + 5);

	ADFMatchState* New = W.World.SpawnActor<ADFMatchState>();
	if (!TestNotNull(TEXT("new match"), New) || !TestNotNull(TEXT("new economy"), New->GetEconomy()))
	{
		return false;
	}
	const int32 NewMoney = New->GetEconomy()->GetMoney();
	Bus->Broadcast(DFTags::Message_EnemyKilled, Kill);
	TestEqual(TEXT("the new match credits the kill once, not twice"), Player->GetKills(), 2);
	TestEqual(TEXT("its own economy pays"), New->GetEconomy()->GetMoney(), NewMoney + 5);
	TestEqual(TEXT("the old one still does not"), Old->GetEconomy()->GetMoney(), OldMoney + 5);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
