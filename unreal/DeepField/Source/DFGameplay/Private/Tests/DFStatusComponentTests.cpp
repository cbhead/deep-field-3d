#include "Abilities/DFAbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "Attributes/DFCombatSet.h"
#include "Attributes/DFHealthSet.h"
#include "Attributes/DFMovementSet.h"
#include "Cues/DFGameplayCueNotify_Base.h"
#include "DFGameplayLocalTags.h"
#include "DFGameplayTags.h"
#include "DFTestRows.h"
#include "Damage/DFDamageContext.h"
#include "Damage/DFDamageMath.h"
#include "Effects/DFGE_Damage.h"
#include "Effects/DFGE_FactionPassive.h"
#include "Effects/DFGE_Passive_Ember.h"
#include "Effects/DFGE_Passive_Forge.h"
#include "Effects/DFGE_Passive_Glacier.h"
#include "Effects/DFGE_Passive_Specter.h"
#include "Effects/DFGE_Passive_Tempest.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/WorldSettings.h"
#include "Messages/DFMessageBus.h"
#include "Messages/DFMessages.h"
#include "Misc/AutomationTest.h"
#include "Status/DFStatusComponent.h"
#include "Tint/DFTintComponent.h"
#include "Tint/DFTintLayout.h"

#if WITH_DEV_AUTOMATION_TESTS

// The world half of C5: an actor with a DF ASC, health + movement sets, a status component and a
// tint component in a standalone game world (a UGameInstance so the message bus subsystem
// exists). Rows are the Appendix A1 literals (DFTestRows), never WS-01's tables.
namespace
{
	/** Advances a standalone game world (the engine's own GAS tests tick the same way). */
	void TickWorld(UWorld* World, float Seconds)
	{
		const float Step = 0.1f;
		while (Seconds > 0.f)
		{
			World->Tick(LEVELTICK_All, FMath::Min(Seconds, Step));
			Seconds -= Step;
			++GFrameCounter;
		}
	}

	/** A bare tick function registered on the persistent level: counts the frames the world actually ran tick functions. */
	struct FDFProbeTick : public FTickFunction
	{
		int32 Count = 0;
		FDFProbeTick()
		{
			bCanEverTick = true;
			bStartWithTickEnabled = true;
			TickGroup = TG_DuringPhysics;
		}
		virtual void ExecuteTick(float, ELevelTick, ENamedThreads::Type, const FGraphEventRef&) override { ++Count; }
		virtual FString DiagnosticMessage() override { return TEXT("DFProbeTick"); }
		virtual FName DiagnosticContext(bool) override { return TEXT("DFProbeTick"); }
	};

	/** Counts damage events on a health set and checks every one is the same amount. */
	struct FDFTickCounter
	{
		int32 Ticks = 0;
		float Expected = 0.f;
		bool bEveryTickExpected = true;
		void Bind(UDFHealthSet* Health, float InExpected)
		{
			Expected = InExpected;
			Health->OnDamaged.AddLambda([this](AActor*, AActor*, const FGameplayEffectSpec*, float Magnitude, float, float)
			{
				++Ticks;
				bEveryTickExpected &= FMath::IsNearlyEqual(Magnitude, Expected, 1e-3f);
			});
		}
	};

	// A 1 s TickWorld is ten 0.1 s frames. A GAS periodic timer set outside a frame sits in the timer
	// manager's pending list through the first frame, and the on-application tick fires once on
	// that frame, so ten frames yield 1 + ~27 ticks of a 1/30 s period; in a real 60 fps match the
	// same delay is one 16 ms frame. The bands below are that arithmetic, not a loose "about".
	constexpr int32 MinTicksPerSecond = 26;
	constexpr int32 MaxTicksPerSecond = 31;

	struct FWorldFixture
	{
		UGameInstance* GI = nullptr;
		UWorld* World = nullptr;
		AActor* Enemy = nullptr;
		UDFAbilitySystemComponent* ASC = nullptr;
		UDFHealthSet* Health = nullptr;
		UDFMovementSet* Movement = nullptr;
		UDFTintComponent* Tint = nullptr;
		UDFStatusComponent* Status = nullptr;

		/** A world and one enemy with the given health row numbers. */
		bool Init(float MaxHealth, float FlatArmor = 0.f, float Shield = 0.f)
		{
			GI = NewObject<UGameInstance>(GEngine);
			GI->AddToRoot();
			GI->InitializeStandalone();
			World = GI->GetWorld();
			if (!World)
			{
				return false;
			}
			World->InitializeActorsForPlay(FURL());
			World->BeginPlay();
			// A world with no GameMode never marks itself as begun-play (that is AGameStateBase's job), and
			// an actor spawned into such a world never dispatches BeginPlay — so a component registered
			// after the spawn gets neither BeginPlay nor its tick function (AActor::HandleRegisterComponentWithWorld).
			// AWorldSettings::NotifyBeginPlay is what the game state calls; it flips the world's flag.
			World->GetWorldSettings()->NotifyBeginPlay();

			Enemy = World->SpawnActor<AActor>();
			ASC = NewObject<UDFAbilitySystemComponent>(Enemy, TEXT("ASC"));
			ASC->RegisterComponent();
			Health = NewObject<UDFHealthSet>(Enemy, TEXT("HealthSet"));
			Movement = NewObject<UDFMovementSet>(Enemy, TEXT("MovementSet"));
			ASC->AddAttributeSetSubobject(Health);
			ASC->AddAttributeSetSubobject(Movement);
			ASC->InitAbilityActorInfo(Enemy, Enemy);
			Health->InitMaxHealth(MaxHealth);
			Health->InitHealth(MaxHealth);
			Health->InitFlatArmor(FlatArmor);
			Health->InitMaxShield(Shield);
			Health->InitShield(Shield);
			Movement->InitBaseSpeed(250.f);
			Movement->InitSpeedFactor(1.f);

			Tint = NewObject<UDFTintComponent>(Enemy, TEXT("Tint"));
			Tint->RegisterComponent();

			Status = NewObject<UDFStatusComponent>(Enemy, TEXT("Status"));
			for (const TPair<FName, FDFStatusRow>& Row : DFTestRows::AllStatuses())
			{
				Status->AddStatusRowOverride(Row.Key, Row.Value);
			}
			Status->AddReactionRowOverride(TEXT("thermalShock"), DFTestRows::ThermalShock());
			Status->AddReactionRowOverride(TEXT("flashFreeze"), DFTestRows::FlashFreeze());
			Status->AddReactionRowOverride(TEXT("corrode"), DFTestRows::Corrode());
			Status->RegisterComponent();
			return true;
		}

		/** Another actor with its own ASC carrying LooseTag (an Ember hero, say). DamageFactor >= 0
		 *  gives it a UDFCombatSet at that DamageFactor — an Overclock / faction feed, the source
		 *  attribute the damage execution snapshot-captures from the instigator. */
		AActor* SpawnApplier(const FGameplayTag& LooseTag, float DamageFactor = -1.f)
		{
			AActor* Applier = World->SpawnActor<AActor>();
			UDFAbilitySystemComponent* ApplierASC = NewObject<UDFAbilitySystemComponent>(Applier, TEXT("ASC"));
			ApplierASC->RegisterComponent();
			if (DamageFactor >= 0.f)
			{
				UDFCombatSet* Combat = NewObject<UDFCombatSet>(Applier, TEXT("CombatSet"));
				ApplierASC->AddAttributeSetSubobject(Combat);
			}
			ApplierASC->InitAbilityActorInfo(Applier, Applier);
			if (DamageFactor >= 0.f)
			{
				ApplierASC->SetNumericAttributeBase(UDFCombatSet::GetDamageFactorAttribute(), DamageFactor);
			}
			if (LooseTag.IsValid())
			{
				ApplierASC->AddLooseGameplayTag(LooseTag);
			}
			return Applier;
		}

		/** A game state whose server clock runs SkewSeconds ahead of this world's — what a client
		 *  that joined SkewSeconds after the server's level load sees. ServerWorldTimeSecondsDelta
		 *  is a protected UPROPERTY, so the test writes it the way replication would. */
		AGameStateBase* SpawnGameState(float SkewSeconds)
		{
			AGameStateBase* GameState = World->SpawnActor<AGameStateBase>();
			if (!GameState)
			{
				return nullptr;
			}
			FProperty* Delta = AGameStateBase::StaticClass()->FindPropertyByName(TEXT("ServerWorldTimeSecondsDelta"));
			if (FFloatProperty* AsFloat = CastField<FFloatProperty>(Delta))
			{
				AsFloat->SetPropertyValue_InContainer(GameState, SkewSeconds);
			}
			return GameState;
		}

		void Tick(float Seconds) { TickWorld(World, Seconds); }
		float Now() const { return World->GetTimeSeconds(); }

		void Shutdown()
		{
			if (!World)
			{
				return;
			}
			if (Enemy)
			{
				Enemy->Destroy();
			}
			World->EndPlay(EEndPlayReason::Quit);   // the begun-play flag NotifyBeginPlay set; CleanupWorld warns otherwise
			GI->Shutdown();
			GEngine->DestroyWorldContext(World);
			World->DestroyWorld(false);
			GI->RemoveFromRoot();
			World = nullptr;
		}
	};
}

// chill then burn through the component must detonate thermalShock for 12% of MaxHealth on
// UDFHealthSet, remove chill's effect and tags, and broadcast DF.Message.ReactionTriggered.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDFStatusComponentThermalShockTest, "DF.Unit.Status.ComponentThermalShockInWorld", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FDFStatusComponentThermalShockTest::RunTest(const FString& Parameters)
{
	FWorldFixture F;
	if (!TestTrue(TEXT("standalone world"), F.Init(100.f)))
	{
		return false;
	}
	UDFAbilitySystemComponent* ASC = F.ASC;
	UDFHealthSet* Health = F.Health;
	UDFStatusComponent* Status = F.Status;
	UDFTintComponent* Tint = F.Tint;

	int32 Reactions = 0;
	int32 Applied = 0;
	FDFMessageHandle ReactionHandle;
	FDFMessageHandle AppliedHandle;
	UDFMessageBus* Bus = UDFMessageBus::Get(F.World);
	if (Bus)
	{
		ReactionHandle = Bus->Subscribe<FDFMsg_Status>(DFTags::Message_ReactionTriggered, [&Reactions](const FGameplayTag&, const FDFMsg_Status&) { ++Reactions; });
		AppliedHandle = Bus->Subscribe<FDFMsg_Status>(DFTags::Message_StatusApplied, [&Applied](const FGameplayTag&, const FDFMsg_Status&) { ++Applied; });
	}

	// chill: Movement effect, channel tag, slow, tint.
	TestEqual(TEXT("chill applied"), Status->Apply(DFTags::Status_Chill, nullptr), EDFStatusApplyResult::Applied);
	TestTrue(TEXT("Movement channel tag granted"), ASC->HasMatchingGameplayTag(DFTags::Status_Channel_Movement));
	TestTrue(TEXT("DF.Status.Chill granted"), ASC->HasMatchingGameplayTag(DFTags::Status_Chill));
	TestTrue(TEXT("SpeedFactor 0.65"), FMath::IsNearlyEqual(ASC->GetNumericAttribute(UDFMovementSet::GetSpeedFactorAttribute()), 0.65f, 1e-4f));
	TestTrue(TEXT("replicated slot holds chill"), Status->IsChannelActive(EDFStatusChannel::Movement));
	TestEqual(TEXT("slot tag"), Status->ActiveStatus(EDFStatusChannel::Movement), FGameplayTag(DFTags::Status_Chill));
	TestEqual(TEXT("tint shows chill"), Tint->GetStatusTintSource(), FGameplayTag(DFTags::Status_Chill));
	TestEqual(TEXT("health untouched"), Health->GetHealth(), 100.f);

	// burn: thermalShock for 12% of 100.
	TestEqual(TEXT("burn reacts"), Status->Apply(DFTags::Status_Burn, nullptr), EDFStatusApplyResult::Reacted);
	TestEqual(TEXT("reaction id"), Status->GetLastOutcome().ReactionId, FName(TEXT("thermalShock")));
	TestTrue(TEXT("health 88"), FMath::IsNearlyEqual(Health->GetHealth(), 88.f, 1e-3f));
	TestFalse(TEXT("chill gone from the slots"), Status->IsChannelActive(EDFStatusChannel::Movement));
	TestFalse(TEXT("burn never landed"), Status->IsChannelActive(EDFStatusChannel::Thermal));
	TestFalse(TEXT("Movement tag removed"), ASC->HasMatchingGameplayTag(DFTags::Status_Channel_Movement));
	TestFalse(TEXT("Thermal tag absent"), ASC->HasMatchingGameplayTag(DFTags::Status_Channel_Thermal));
	TestTrue(TEXT("SpeedFactor back to 1"), FMath::IsNearlyEqual(ASC->GetNumericAttribute(UDFMovementSet::GetSpeedFactorAttribute()), 1.f, 1e-4f));
	TestFalse(TEXT("tint cleared"), Tint->GetStatusTintSource().IsValid());
	if (Bus)
	{
		TestEqual(TEXT("one ReactionTriggered message"), Reactions, 1);
		TestEqual(TEXT("one StatusApplied message (chill)"), Applied, 1);
	}

	// burn on its own now lands; its periodic tick starts on the next timer tick (6 dps at 30 Hz = 0.2 per tick, no armor).
	FDFTickCounter BurnTicks;
	BurnTicks.Bind(Health, 0.2f);
	TestEqual(TEXT("burn applied"), Status->Apply(DFTags::Status_Burn, nullptr), EDFStatusApplyResult::Applied);
	TestTrue(TEXT("Thermal tag granted"), ASC->HasMatchingGameplayTag(DFTags::Status_Channel_Thermal));
	TestEqual(TEXT("tint shows burn"), Tint->GetStatusTintSource(), FGameplayTag(DFTags::Status_Burn));
	TestTrue(TEXT("no tick before the world ticks"), FMath::IsNearlyEqual(Health->GetHealth(), 88.f, 1e-3f));
	F.Tick(1.f);
	TestTrue(*FString::Printf(TEXT("every burn tick is 0.2 (%d ticks)"), BurnTicks.Ticks), BurnTicks.bEveryTickExpected && BurnTicks.Ticks > 0);
	TestTrue(*FString::Printf(TEXT("~30 ticks in a second (got %d)"), BurnTicks.Ticks), BurnTicks.Ticks >= MinTicksPerSecond && BurnTicks.Ticks <= MaxTicksPerSecond);
	TestTrue(TEXT("health is 88 minus the ticks"), FMath::IsNearlyEqual(Health->GetHealth(), 88.f - 0.2f * BurnTicks.Ticks, 1e-2f));

	// Shield up: a new burn is refused (the active one keeps ticking), chill is not.
	Health->InitMaxShield(25.f);
	Health->InitShield(25.f);
	Status->ClearChannel(EDFStatusChannel::Thermal);
	TestFalse(TEXT("Thermal cleared"), ASC->HasMatchingGameplayTag(DFTags::Status_Channel_Thermal));
	TestEqual(TEXT("burn refused on a shield"), Status->Apply(DFTags::Status_Burn, nullptr), EDFStatusApplyResult::RejectedShield);
	TestEqual(TEXT("chill still lands"), Status->Apply(DFTags::Status_Chill, nullptr), EDFStatusApplyResult::Applied);

	// Tether immunity from Mass (enemy row) and from an immune tag on the ASC.
	Status->Mass = 6.f;
	TestEqual(TEXT("magnetize refused at Mass 6"), Status->Apply(DFTags::Status_Magnetize, nullptr), EDFStatusApplyResult::RejectedImmune);
	Status->Mass = 1.f;
	ASC->AddLooseGameplayTag(DFTags::Enemy_State_Phased);
	TestEqual(TEXT("magnetize refused while phased"), Status->Apply(DFTags::Status_Magnetize, nullptr), EDFStatusApplyResult::RejectedImmune);
	ASC->RemoveLooseGameplayTag(DFTags::Enemy_State_Phased);
	TestEqual(TEXT("magnetize lands otherwise"), Status->Apply(DFTags::Status_Magnetize, nullptr), EDFStatusApplyResult::Applied);
	TestTrue(TEXT("Tether tag granted"), ASC->HasMatchingGameplayTag(DFTags::Status_Channel_Tether));

	// Expiry through the component tick as the world runs: everything ends, tags and slots clear.
	// The facts the expiry rests on, so a failure names its link: the component's tick is
	// registered and enabled, the world runs tick functions at all, and its clock advances.
	TestTrue(TEXT("status tick function registered"), Status->PrimaryComponentTick.IsTickFunctionRegistered());
	TestTrue(TEXT("status tick function enabled"), Status->PrimaryComponentTick.IsTickFunctionEnabled());
	TestTrue(TEXT("component tick enabled"), Status->IsComponentTickEnabled());
	FDFProbeTick Probe;
	Probe.RegisterTickFunction(F.World->PersistentLevel);
	TestTrue(TEXT("probe tick registered"), Probe.IsTickFunctionRegistered());
	const float Before = F.Now();
	F.Tick(10.f);
	TestTrue(*FString::Printf(TEXT("world clock advanced ~10 s (got %.2f)"), F.Now() - Before), F.Now() - Before >= 9.5f);
	TestTrue(*FString::Printf(TEXT("the world ran tick functions (probe ticked %d frames)"), Probe.Count), Probe.Count >= 90);
	Probe.UnRegisterTickFunction();
	TestFalse(TEXT("Movement expired"), Status->IsChannelActive(EDFStatusChannel::Movement));
	TestFalse(TEXT("Tether expired"), Status->IsChannelActive(EDFStatusChannel::Tether));
	TestFalse(TEXT("no Movement tag"), ASC->HasMatchingGameplayTag(DFTags::Status_Channel_Movement));
	TestFalse(TEXT("no Tether tag"), ASC->HasMatchingGameplayTag(DFTags::Status_Channel_Tether));

	if (Bus)
	{
		Bus->Unsubscribe(ReactionHandle);
		Bus->Unsubscribe(AppliedHandle);
	}
	F.Shutdown();
	return true;
}

// Step.cs ApplyStatus -> Damage(w, enemy, burst, source, enemy.Pos, ...): the burst goes through
// the damage order with the source at the enemy's own position — no arc test, but mark, flat
// armor and shield all apply. 12% of 100 = 12, x1.25 mark = 15, - 2 flat armor = 13, shield 5
// soaks 5, health takes 8.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDFStatusBurstIsArmoredAndShieldedTest, "DF.Unit.Status.BurstIsArmoredAndShielded", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FDFStatusBurstIsArmoredAndShieldedTest::RunTest(const FString& Parameters)
{
	FWorldFixture F;
	if (!TestTrue(TEXT("standalone world"), F.Init(100.f, /*FlatArmor*/ 2.f, /*Shield*/ 5.f)))
	{
		return false;
	}
	TestEqual(TEXT("mark applied"), F.Status->Apply(DFTags::Status_Mark, nullptr), EDFStatusApplyResult::Applied);
	TestTrue(TEXT("DamageTakenFactor 1.25 on the set"), FMath::IsNearlyEqual(F.ASC->GetNumericAttribute(UDFHealthSet::GetDamageTakenFactorAttribute()), 1.25f, 1e-4f));
	TestEqual(TEXT("chill applied through the shield"), F.Status->Apply(DFTags::Status_Chill, nullptr), EDFStatusApplyResult::Applied);

	// The shield would refuse a plain burn; the reaction scan runs first (DF.Unit.Status.ReactionScanPrecedesGates).
	TestEqual(TEXT("burn onto chill reacts despite the shield"), F.Status->Apply(DFTags::Status_Burn, nullptr), EDFStatusApplyResult::Reacted);
	TestEqual(TEXT("thermalShock"), F.Status->GetLastOutcome().ReactionId, FName(TEXT("thermalShock")));
	TestTrue(*FString::Printf(TEXT("shield emptied by the burst (got %.2f)"), F.Health->GetShield()), FMath::IsNearlyEqual(F.Health->GetShield(), 0.f, 1e-3f));
	TestTrue(*FString::Printf(TEXT("health 92: 12 x 1.25 - 2 = 13, 5 to the shield, 8 to health (got %.2f)"), F.Health->GetHealth()), FMath::IsNearlyEqual(F.Health->GetHealth(), 92.f, 1e-3f));
	TestTrue(TEXT("mark survived the reaction"), F.Status->IsChannelActive(EDFStatusChannel::Vulnerability));
	TestFalse(TEXT("chill consumed"), F.Status->IsChannelActive(EDFStatusChannel::Movement));
	TestFalse(TEXT("burn never landed"), F.Status->IsChannelActive(EDFStatusChannel::Thermal));

	// Without mark, on the same target with the shield gone: 12 - 2 = 10 straight to health.
	F.Status->ClearChannel(EDFStatusChannel::Vulnerability);
	F.Status->Apply(DFTags::Status_Chill, nullptr);
	F.Status->Apply(DFTags::Status_Burn, nullptr);
	TestTrue(*FString::Printf(TEXT("health 82 (got %.2f)"), F.Health->GetHealth()), FMath::IsNearlyEqual(F.Health->GetHealth(), 82.f, 1e-3f));

	// Corrode with shred ACTIVE: the shred is consumed before the burst, so the burst sees the
	// full 2 armor again (the Defense effect is removed first): 10% of 100 = 10 - 2 = 8 -> 74.
	TestEqual(TEXT("shred applied"), F.Status->Apply(DFTags::Status_Shred, nullptr), EDFStatusApplyResult::Applied);
	TestTrue(TEXT("shred drove FlatArmor to 0"), FMath::IsNearlyEqual(F.ASC->GetNumericAttribute(UDFHealthSet::GetFlatArmorAttribute()), 0.f, 1e-4f));
	TestEqual(TEXT("poison onto shred: corrode"), F.Status->Apply(DFTags::Status_Poison, nullptr), EDFStatusApplyResult::Reacted);
	TestTrue(TEXT("FlatArmor back to 2 before the burst"), FMath::IsNearlyEqual(F.ASC->GetNumericAttribute(UDFHealthSet::GetFlatArmorAttribute()), 2.f, 1e-4f));
	TestTrue(*FString::Printf(TEXT("health 74 (got %.2f)"), F.Health->GetHealth()), FMath::IsNearlyEqual(F.Health->GetHealth(), 74.f, 1e-3f));

	F.Shutdown();
	return true;
}

// Step.cs ApplyStatus: `if (active.Id == incoming.Id) { target.TimeLeft = duration; target.Source = source; return; }`
// — a refresh moves the end time and the source and emits no StatusApplied.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDFStatusRefreshIsSilentTest, "DF.Unit.Status.RefreshIsSilent", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FDFStatusRefreshIsSilentTest::RunTest(const FString& Parameters)
{
	FWorldFixture F;
	if (!TestTrue(TEXT("standalone world"), F.Init(100.f)))
	{
		return false;
	}
	int32 Messages = 0;
	int32 Delegates = 0;
	FDFMessageHandle AppliedHandle;
	UDFMessageBus* Bus = UDFMessageBus::Get(F.World);
	if (Bus)
	{
		AppliedHandle = Bus->Subscribe<FDFMsg_Status>(DFTags::Message_StatusApplied, [&Messages](const FGameplayTag&, const FDFMsg_Status&) { ++Messages; });
	}
	F.Status->OnStatusApplied.AddLambda([&Delegates](const FDFStatusSlot&) { ++Delegates; });

	AActor* First = F.SpawnApplier(FGameplayTag());
	AActor* Second = F.SpawnApplier(FGameplayTag());

	TestEqual(TEXT("chill applied"), F.Status->Apply(DFTags::Status_Chill, First), EDFStatusApplyResult::Applied);
	const float FirstEnd = F.Status->GetResolver().Slot(EDFStatusChannel::Movement).EndTime;
	TestTrue(TEXT("ends 1.5 s out"), FMath::IsNearlyEqual(FirstEnd, F.Now() + 1.5f, 1e-3f));
	TestEqual(TEXT("source is the first applier"), F.Status->GetResolver().Slot(EDFStatusChannel::Movement).SourceId, static_cast<int32>(First->GetUniqueID()));

	F.Tick(1.f);
	TestEqual(TEXT("same id refreshes"), F.Status->Apply(DFTags::Status_Chill, Second), EDFStatusApplyResult::Refreshed);
	const FDFStatusSlot& Slot = F.Status->GetResolver().Slot(EDFStatusChannel::Movement);
	TestTrue(TEXT("end time moved to now + 1.5"), FMath::IsNearlyEqual(Slot.EndTime, F.Now() + 1.5f, 1e-3f));
	TestTrue(TEXT("later than before"), Slot.EndTime > FirstEnd + 0.5f);
	TestEqual(TEXT("source is now the second applier"), Slot.SourceId, static_cast<int32>(Second->GetUniqueID()));
	TestTrue(TEXT("replicated slot follows"), FMath::IsNearlyEqual(F.Status->GetSlots()[static_cast<int32>(EDFStatusChannel::Movement)].EndTimeServer, Slot.EndTime, 1e-4f));
	TestTrue(TEXT("still one Movement effect: SpeedFactor 0.65, not 0.65^2"), FMath::IsNearlyEqual(F.ASC->GetNumericAttribute(UDFMovementSet::GetSpeedFactorAttribute()), 0.65f, 1e-4f));

	TestEqual(TEXT("OnStatusApplied fired once"), Delegates, 1);
	if (Bus)
	{
		TestEqual(TEXT("one StatusApplied message, not two"), Messages, 1);
	}

	// It expires once, at the refreshed time: alive at +1.4 s from the refresh, gone after.
	F.Tick(1.3f);
	TestTrue(TEXT("still chilled after the original end"), F.Status->IsChannelActive(EDFStatusChannel::Movement));
	F.Tick(0.5f);
	TestFalse(TEXT("expired at the refreshed end"), F.Status->IsChannelActive(EDFStatusChannel::Movement));

	if (Bus)
	{
		Bus->Unsubscribe(AppliedHandle);
	}
	F.Shutdown();
	return true;
}

// Step.cs UpdateStatuses: `Damage(w, enemy, def.DamagePerSecond * Balance.Dt, ...)` every tick at
// 30 Hz, through the full order. A 0.2 burn tick against the Ram's 2 flat armor floors to 0.5, so
// a burning Ram loses ~15 hp/s, not 6. Mark amplifies the tick (0.2 x 1.25 = 0.25 on an unarmored target).
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDFStatusDotTickMatchesSimTest, "DF.Unit.Status.DotTickMatchesSim", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FDFStatusDotTickMatchesSimTest::RunTest(const FString& Parameters)
{
	// The per-tick number with no world: 6 dps / 30 = 0.2 -> floor 0.5 against 2 armor; x30 = 15 dps.
	{
		FDFDamageInput Tick;
		Tick.BaseDamage = 6.f / 30.f;
		Tick.FlatArmor = 2.f;
		TestTrue(TEXT("0.2 tick floors to 0.5 on a ram"), FMath::IsNearlyEqual(FDFDamageMath::Compute(Tick).Damage, 0.5f, 1e-4f));
		TestTrue(TEXT("15 dps"), FMath::IsNearlyEqual(FDFDamageMath::Compute(Tick).Damage * 30.f, 15.f, 1e-3f));
	}

	// The Ram in the world: 70 hp, flat armor 2, burning for a second.
	FWorldFixture Ram;
	if (!TestTrue(TEXT("standalone world"), Ram.Init(70.f, /*FlatArmor*/ 2.f)))
	{
		return false;
	}
	TestTrue(TEXT("tick rate is the sim's 30 Hz"), FMath::IsNearlyEqual(Ram.Status->GetStatusTickHz(), 30.f, 1e-4f));
	FDFTickCounter RamTicks;
	RamTicks.Bind(Ram.Health, 0.5f);
	TestEqual(TEXT("burn applied"), Ram.Status->Apply(DFTags::Status_Burn, nullptr), EDFStatusApplyResult::Applied);
	Ram.Tick(1.f);
	const float Lost = 70.f - Ram.Health->GetHealth();
	TestTrue(*FString::Printf(TEXT("every tick was the 0.5 floor, not 0.2 (%d ticks)"), RamTicks.Ticks), RamTicks.bEveryTickExpected && RamTicks.Ticks > 0);
	TestTrue(*FString::Printf(TEXT("~30 ticks in a second (got %d)"), RamTicks.Ticks), RamTicks.Ticks >= MinTicksPerSecond && RamTicks.Ticks <= MaxTicksPerSecond);
	TestTrue(*FString::Printf(TEXT("a burning ram loses 0.5 x 30 = 15 hp/s (got %.2f over %d ticks)"), Lost, RamTicks.Ticks), FMath::IsNearlyEqual(Lost, 0.5f * RamTicks.Ticks, 1e-2f));
	Ram.Shutdown();

	// An unarmored, marked target: 0.2 x 1.25 = 0.25 per tick — the mark rides on the DoT.
	FWorldFixture Drifter;
	if (!TestTrue(TEXT("standalone world 2"), Drifter.Init(100.f)))
	{
		return false;
	}
	FDFTickCounter MarkedTicks;
	MarkedTicks.Bind(Drifter.Health, 0.25f);
	Drifter.Status->Apply(DFTags::Status_Mark, nullptr);
	Drifter.Status->Apply(DFTags::Status_Burn, nullptr);
	Drifter.Tick(1.f);
	TestTrue(*FString::Printf(TEXT("marked ticks are 0.25 (%d ticks)"), MarkedTicks.Ticks), MarkedTicks.bEveryTickExpected && MarkedTicks.Ticks > 0);
	TestTrue(*FString::Printf(TEXT("~30 marked ticks in a second (got %d)"), MarkedTicks.Ticks), MarkedTicks.Ticks >= MinTicksPerSecond && MarkedTicks.Ticks <= MaxTicksPerSecond);
	TestTrue(TEXT("a marked drifter loses 0.25 per tick to burn"), FMath::IsNearlyEqual(100.f - Drifter.Health->GetHealth(), 0.25f * MarkedTicks.Ticks, 1e-2f));
	Drifter.Shutdown();
	return true;
}

// Step.cs ApplyStatus: Ember's factor is applied to `duration` by the applier's faction BEFORE the
// refresh branch, so an Ember refresh also gets the x1.3 burn.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDFStatusEmberDurationOnRefreshTest, "DF.Unit.Status.EmberDurationOnRefresh", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FDFStatusEmberDurationOnRefreshTest::RunTest(const FString& Parameters)
{
	// The resolver half: a duration factor rides on a refresh.
	{
		DFTestRows::FResolverFixture R;
		const FDFStatusRow& Burn = R.Rows.FindChecked(TEXT("burn"));
		R.Resolver.Apply(TEXT("burn"), Burn, 0.f, 1, R.Target, -1.f, 1.f);
		TestEqual(TEXT("plain burn ends at 3"), R.Resolver.Slot(EDFStatusChannel::Thermal).EndTime, 3.f);
		const FDFStatusApplyOutcome Refresh = R.Resolver.Apply(TEXT("burn"), Burn, 1.f, 2, R.Target, -1.f, 1.3f);
		TestEqual(TEXT("refreshed"), Refresh.Result, EDFStatusApplyResult::Refreshed);
		TestTrue(TEXT("refresh carries the factor: 1 + 3.9"), FMath::IsNearlyEqual(R.Resolver.Slot(EDFStatusChannel::Thermal).EndTime, 4.9f, 1e-4f));
	}

	// The world half: the factor comes from the applier's faction tag and Balance("emberBurnDurationFactor").
	FWorldFixture F;
	if (!TestTrue(TEXT("standalone world"), F.Init(100.f)))
	{
		return false;
	}
	TestTrue(TEXT("factor default 1.3"), FMath::IsNearlyEqual(F.Status->GetEmberBurnDurationFactor(), 1.3f, 1e-4f));
	AActor* Ember = F.SpawnApplier(DFTags::Faction_Ember);
	AActor* Forge = F.SpawnApplier(DFTags::Faction_Forge);
	TestTrue(TEXT("ember hero recognised"), UDFStatusComponent::IsEmberApplier(Ember));
	TestFalse(TEXT("forge hero not"), UDFStatusComponent::IsEmberApplier(Forge));
	TestFalse(TEXT("no source: not ember"), UDFStatusComponent::IsEmberApplier(nullptr));

	// An Ember burn lasts 3.9 s.
	TestEqual(TEXT("ember burn applied"), F.Status->Apply(DFTags::Status_Burn, Ember), EDFStatusApplyResult::Applied);
	TestTrue(TEXT("ends 3.9 s out"), FMath::IsNearlyEqual(F.Status->GetResolver().Slot(EDFStatusChannel::Thermal).EndTime, F.Now() + 3.9f, 1e-3f));

	// An Ember refresh lasts 3.9 s from now, not 3.
	F.Tick(1.f);
	TestEqual(TEXT("ember refresh"), F.Status->Apply(DFTags::Status_Burn, Ember), EDFStatusApplyResult::Refreshed);
	TestTrue(TEXT("refresh ends 3.9 s out"), FMath::IsNearlyEqual(F.Status->GetResolver().Slot(EDFStatusChannel::Thermal).EndTime, F.Now() + 3.9f, 1e-3f));

	// A Forge refresh of the same burn gets the plain 3 s (the factor is the applier's, not the status's).
	F.Tick(0.5f);
	TestEqual(TEXT("forge refresh"), F.Status->Apply(DFTags::Status_Burn, Forge), EDFStatusApplyResult::Refreshed);
	TestTrue(TEXT("refresh ends 3 s out"), FMath::IsNearlyEqual(F.Status->GetResolver().Slot(EDFStatusChannel::Thermal).EndTime, F.Now() + 3.f, 1e-3f));

	// Ember's factor is burn-only: an Ember chill is the row's 1.5 s (the burn is cleared first — chill onto burn would react).
	F.Status->ClearChannel(EDFStatusChannel::Thermal);
	TestEqual(TEXT("ember chill applied"), F.Status->Apply(DFTags::Status_Chill, Ember), EDFStatusApplyResult::Applied);
	TestTrue(TEXT("chill ends 1.5 s out"), FMath::IsNearlyEqual(F.Status->GetResolver().Slot(EDFStatusChannel::Movement).EndTime, F.Now() + 1.5f, 1e-3f));

	// A weapon actor owned by the Ember hero counts as an Ember applier.
	AActor* Weapon = F.World->SpawnActor<AActor>();
	Weapon->SetOwner(Ember);
	TestTrue(TEXT("owned actor inherits the faction"), UDFStatusComponent::IsEmberApplier(Weapon));

	F.Shutdown();
	return true;
}

// Step.cs UpdateStatuses: `Damage(w, enemy, dps * dt, slot.Source, ...)` — every tick is credited to
// the slot's CURRENT source, and a same-id refresh moves the source. The running periodic effect
// is kept (no re-apply: its period and phase stand, no extra on-application tick) and its context
// instigator is retargeted to the refresher, so the ticks after the refresh belong to the second applier.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDFStatusRefreshRetargetsDotTest, "DF.Unit.Status.RefreshRetargetsDot", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FDFStatusRefreshRetargetsDotTest::RunTest(const FString& Parameters)
{
	FWorldFixture F;
	if (!TestTrue(TEXT("standalone world"), F.Init(100.f)))
	{
		return false;
	}
	AActor* First = F.SpawnApplier(FGameplayTag());
	AActor* Second = F.SpawnApplier(FGameplayTag());
	TArray<AActor*> Instigators;
	F.Health->OnDamaged.AddLambda([&Instigators](AActor* Instigator, AActor*, const FGameplayEffectSpec*, float, float, float) { Instigators.Add(Instigator); });

	TestEqual(TEXT("burn from the first applier"), F.Status->Apply(DFTags::Status_Burn, First), EDFStatusApplyResult::Applied);
	F.Tick(0.5f);
	const int32 TicksBeforeRefresh = Instigators.Num();
	TestTrue(TEXT("the burn ticked"), TicksBeforeRefresh > 0);
	bool bAllFirst = true;
	for (AActor* Who : Instigators)
	{
		bAllFirst &= (Who == First);
	}
	TestTrue(TEXT("every tick before the refresh is the first applier's"), bAllFirst);

	const FActiveGameplayEffectHandle Before = F.Status->GetChannelEffectHandle(EDFStatusChannel::Thermal);
	TestTrue(TEXT("a Thermal effect is running"), Before.IsValid());
	TestEqual(TEXT("same burn from the second applier refreshes"), F.Status->Apply(DFTags::Status_Burn, Second), EDFStatusApplyResult::Refreshed);
	TestEqual(TEXT("slot source moved"), F.Status->GetResolver().Slot(EDFStatusChannel::Thermal).SourceId, static_cast<int32>(Second->GetUniqueID()));
	TestEqual(TEXT("no extra tick fired by the refresh itself (the effect was not re-applied)"), Instigators.Num(), TicksBeforeRefresh);
	TestTrue(TEXT("the SAME Thermal effect still runs (retargeted, not re-applied)"), F.Status->GetChannelEffectHandle(EDFStatusChannel::Thermal) == Before);
	TestTrue(TEXT("and it is still active on the ASC"), F.ASC->GetActiveGameplayEffect(Before) != nullptr);
	TestTrue(TEXT("the channel tag is still granted"), F.ASC->HasMatchingGameplayTag(DFTags::Status_Channel_Thermal));

	F.Tick(0.5f);
	TestTrue(TEXT("the burn kept ticking"), Instigators.Num() > TicksBeforeRefresh);
	bool bAllSecond = true;
	for (int32 I = TicksBeforeRefresh; I < Instigators.Num(); ++I)
	{
		bAllSecond &= (Instigators[I] == Second);
	}
	TestTrue(TEXT("every tick after the refresh is the second applier's"), bAllSecond);
	TestTrue(*FString::Printf(TEXT("~30 ticks over the second, period untouched (got %d)"), Instigators.Num()), Instigators.Num() >= MinTicksPerSecond && Instigators.Num() <= MaxTicksPerSecond);
	F.Shutdown();
	return true;
}

// The component plumbs bHero into the resolver's hero gate (C5 / B§1.6): a hero's Thermal is
// refused before any effect is made, Movement lands, stagger controls without a gauge.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDFStatusHeroComponentTest, "DF.Unit.Status.HeroComponentRefusesThermal", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FDFStatusHeroComponentTest::RunTest(const FString& Parameters)
{
	FWorldFixture F;
	if (!TestTrue(TEXT("standalone world"), F.Init(100.f)))
	{
		return false;
	}
	F.Status->bHero = true;
	TestEqual(TEXT("burn refused on a hero"), F.Status->Apply(DFTags::Status_Burn, nullptr), EDFStatusApplyResult::RejectedHero);
	TestFalse(TEXT("no Thermal tag"), F.ASC->HasMatchingGameplayTag(DFTags::Status_Channel_Thermal));
	TestEqual(TEXT("poison refused on a hero"), F.Status->Apply(DFTags::Status_Poison, nullptr), EDFStatusApplyResult::RejectedHero);
	TestEqual(TEXT("chill lands on a hero"), F.Status->Apply(DFTags::Status_Chill, nullptr), EDFStatusApplyResult::Applied);
	TestTrue(TEXT("SpeedFactor 0.65"), FMath::IsNearlyEqual(F.ASC->GetNumericAttribute(UDFMovementSet::GetSpeedFactorAttribute()), 0.65f, 1e-4f));
	TestEqual(TEXT("stagger lands on a hero"), F.Status->Apply(DFTags::Status_Stagger, nullptr), EDFStatusApplyResult::Applied);
	TestTrue(TEXT("Control tag granted"), F.ASC->HasMatchingGameplayTag(DFTags::Status_Channel_Control));
	F.Tick(0.2f);
	TestEqual(TEXT("gauge untouched by a hero's stagger"), F.Status->GetCcResist(), 0.f);
	TestTrue(TEXT("health untouched"), FMath::IsNearlyEqual(F.Health->GetHealth(), 100.f, 1e-3f));
	F.Shutdown();
	return true;
}

// The five faction passives (PROGRAMME §5.2 WS-02) as effects: each grants its DF.Ability.Passive.<Id>
// tag, the two with an attribute write UDFCombatSet from the Balance dial (through
// UDFMMC_BalanceDial), and Ember's tag alone makes an applier an Ember applier for the status
// component — the granter (WS-07) never spells a number.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDFStatusFactionPassivesTest, "DF.Unit.Status.FactionPassives", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FDFStatusFactionPassivesTest::RunTest(const FString& Parameters)
{
	FWorldFixture F;
	if (!TestTrue(TEXT("standalone world"), F.Init(100.f)))
	{
		return false;
	}
	AActor* Hero = F.SpawnApplier(FGameplayTag());
	UDFAbilitySystemComponent* ASC = UDFAbilitySystemComponent::FindOn(Hero);
	if (!TestNotNull(TEXT("hero ASC"), ASC))
	{
		return false;
	}
	UDFCombatSet* Combat = NewObject<UDFCombatSet>(Hero, TEXT("CombatSet"));
	ASC->AddAttributeSetSubobject(Combat);
	TestEqual(TEXT("RateFactor starts at 1"), Combat->GetRateFactor(), 1.f);
	TestEqual(TEXT("ChilledBonus starts at 1"), Combat->GetChilledBonus(), 1.f);

	struct FExpected
	{
		FGameplayTag Faction;
		TSubclassOf<UDFGE_FactionPassive> Class;
		FGameplayTag Passive;
		FName Dial;
		float Magnitude;
	};
	const FExpected Table[] = {
		{ DFTags::Faction_Forge,   UDFGE_Passive_Forge::StaticClass(),   DFTags::Ability_Passive_BuildDiscount, TEXT("forgeBuildDiscount"),         0.9f  },
		{ DFTags::Faction_Ember,   UDFGE_Passive_Ember::StaticClass(),   DFTags::Ability_Passive_BurnDuration,  TEXT("emberBurnDurationFactor"),    1.3f  },
		{ DFTags::Faction_Tempest, UDFGE_Passive_Tempest::StaticClass(), DFTags::Ability_Passive_ReloadSpeed,   TEXT("tempestRateFactor"),          1.12f },
		{ DFTags::Faction_Glacier, UDFGE_Passive_Glacier::StaticClass(), DFTags::Ability_Passive_ChilledBonus,  TEXT("glacierChilledDamageFactor"), 1.25f },
		{ DFTags::Faction_Specter, UDFGE_Passive_Specter::StaticClass(), DFTags::Ability_Passive_WeakPoints,    NAME_None,                          1.f   },
	};
	TestEqual(TEXT("five passives"), UDFGE_FactionPassive::AllClasses().Num(), 5);
	TestTrue(TEXT("no passive for a non-faction tag"), UDFGE_FactionPassive::ClassForFaction(DFTags::Status_Burn) == nullptr);
	TMap<FName, FActiveGameplayEffectHandle> Handles;
	for (const FExpected& E : Table)
	{
		const FString Who = E.Faction.ToString();
		const TSubclassOf<UDFGE_FactionPassive> Class = UDFGE_FactionPassive::ClassForFaction(E.Faction);
		TestTrue(*FString::Printf(TEXT("%s -> %s"), *Who, *GetNameSafe(E.Class)), Class == E.Class);
		const UDFGE_FactionPassive* Passive = E.Class.GetDefaultObject();
		TestEqual(*FString::Printf(TEXT("%s grants %s"), *Who, *E.Passive.ToString()), Passive->GetPassiveTag(), E.Passive);
		TestEqual(*FString::Printf(TEXT("%s dial"), *Who), Passive->GetDial(), E.Dial);
		TestTrue(*FString::Printf(TEXT("%s magnitude %.2f (got %.3f)"), *Who, E.Magnitude, Passive->Magnitude(Hero)), FMath::IsNearlyEqual(Passive->Magnitude(Hero), E.Magnitude, 1e-4f));
		TestFalse(*FString::Printf(TEXT("%s tag absent before"), *Who), ASC->HasMatchingGameplayTag(E.Passive));
		const FActiveGameplayEffectHandle Handle = ASC->ApplyGameplayEffectToSelf(Passive, 1.f, ASC->MakeEffectContext());
		TestTrue(*FString::Printf(TEXT("%s applied (infinite)"), *Who), Handle.IsValid());
		TestTrue(*FString::Printf(TEXT("%s tag granted"), *Who), ASC->HasMatchingGameplayTag(E.Passive));
		Handles.Add(E.Dial.IsNone() ? FName(*Who) : E.Dial, Handle);
	}
	TestTrue(*FString::Printf(TEXT("Tempest: RateFactor x1.12 (got %.3f)"), Combat->GetRateFactor()), FMath::IsNearlyEqual(Combat->GetRateFactor(), 1.12f, 1e-4f));
	TestTrue(*FString::Printf(TEXT("Glacier: ChilledBonus x1.25 (got %.3f)"), Combat->GetChilledBonus()), FMath::IsNearlyEqual(Combat->GetChilledBonus(), 1.25f, 1e-4f));
	TestEqual(TEXT("Specter: WeakPointBonus untouched"), Combat->GetWeakPointBonus(), 1.f);
	TestEqual(TEXT("DamageFactor untouched"), Combat->GetDamageFactor(), 1.f);

	// Ember by passive: no DF.Faction.Ember on the hero, the granted tag alone stretches its burn to 3.9 s.
	TestFalse(TEXT("no faction tag"), ASC->HasMatchingGameplayTag(DFTags::Faction_Ember));
	TestTrue(TEXT("Ember passive makes an Ember applier"), UDFStatusComponent::IsEmberApplier(Hero));
	TestEqual(TEXT("burn from the passive hero"), F.Status->Apply(DFTags::Status_Burn, Hero), EDFStatusApplyResult::Applied);
	TestTrue(TEXT("burn ends 3.9 s out"), FMath::IsNearlyEqual(F.Status->GetResolver().Slot(EDFStatusChannel::Thermal).EndTime, F.Now() + 3.9f, 1e-3f));

	// Removing the passive removes the tag and the factor: the next burn is the row's 3 s.
	ASC->RemoveActiveGameplayEffect(Handles.FindChecked(TEXT("emberBurnDurationFactor")));
	TestFalse(TEXT("Ember tag gone"), ASC->HasMatchingGameplayTag(DFTags::Ability_Passive_BurnDuration));
	TestFalse(TEXT("no longer an Ember applier"), UDFStatusComponent::IsEmberApplier(Hero));
	F.Status->ClearChannel(EDFStatusChannel::Thermal);
	TestEqual(TEXT("plain burn"), F.Status->Apply(DFTags::Status_Burn, Hero), EDFStatusApplyResult::Applied);
	TestTrue(TEXT("burn ends 3 s out"), FMath::IsNearlyEqual(F.Status->GetResolver().Slot(EDFStatusChannel::Thermal).EndTime, F.Now() + 3.f, 1e-3f));
	ASC->RemoveActiveGameplayEffect(Handles.FindChecked(TEXT("tempestRateFactor")));
	TestTrue(TEXT("RateFactor back to 1"), FMath::IsNearlyEqual(Combat->GetRateFactor(), 1.f, 1e-4f));
	F.Shutdown();
	return true;
}

// The cue path of C5 without a map: UDFGameplayCueNotify_Base registers the native family handlers
// in the runtime cue set (no asset in Content), and every cue the component executes reaches
// UDFMessageBus as FDFMsg_GameplayCue keyed by the cue tag — the status Applied cue, then the
// thermalShock reaction cue carrying the 12 % burst. This is what WS-14 / WS-13 subscribe to.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDFStatusCueForwardsToBusTest, "DF.Unit.Status.CueForwardsToBus", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FDFStatusCueForwardsToBusTest::RunTest(const FString& Parameters)
{
	FWorldFixture F;
	if (!TestTrue(TEXT("standalone world"), F.Init(100.f)))
	{
		return false;
	}
	UDFMessageBus* Bus = UDFMessageBus::Get(F.World);
	if (!TestNotNull(TEXT("message bus"), Bus))
	{
		return false;
	}
	TArray<FDFMsg_GameplayCue> Seen;
	const FGameplayTag Root = FGameplayTag::RequestGameplayTag(FName(TEXT("GameplayCue.DF")), /*ErrorIfNotFound*/ false);
	TestTrue(TEXT("GameplayCue.DF exists"), Root.IsValid());
	const FDFMessageHandle Handle = Bus->Subscribe<FDFMsg_GameplayCue>(Root, [&Seen](const FGameplayTag&, const FDFMsg_GameplayCue& Msg) { Seen.Add(Msg); });

	AActor* Applier = F.SpawnApplier(FGameplayTag());
	TestEqual(TEXT("chill lands"), F.Status->Apply(DFTags::Status_Chill, Applier), EDFStatusApplyResult::Applied);
	const FGameplayTag ChillApplied = DFGameplayLocalTags::StatusCue(TEXT("chill"), TEXT("Applied"));
	TestTrue(TEXT("one cue so far: chill Applied"), Seen.Num() == 1 && Seen[0].Cue == ChillApplied);
	if (Seen.Num() == 1)
	{
		TestEqual(TEXT("handled by the native status fallback (GameplayCue.DF.Status)"), Seen[0].HandlerTag, DFGameplayLocalTags::StatusCueRoot());
		TestTrue(TEXT("target is the enemy"), Seen[0].Target == F.Enemy);
		TestTrue(TEXT("instigator is the applier"), Seen[0].Instigator == Applier);
		TestEqual(TEXT("event: Executed"), Seen[0].Event, EDFCueEvent::Executed);
		TestTrue(TEXT("RawMagnitude = the slot magnitude 0.35"), FMath::IsNearlyEqual(Seen[0].RawMagnitude, 0.35f, 1e-4f));
	}

	TestEqual(TEXT("burn reacts"), F.Status->Apply(DFTags::Status_Burn, Applier), EDFStatusApplyResult::Reacted);
	const FGameplayTag ThermalShock = DFGameplayLocalTags::ReactionCue(TEXT("thermalShock"));
	const FDFMsg_GameplayCue* Reaction = Seen.FindByPredicate([&ThermalShock](const FDFMsg_GameplayCue& M) { return M.Cue == ThermalShock; });
	if (TestNotNull(TEXT("GameplayCue.DF.Reaction.ThermalShock reached the bus"), Reaction))
	{
		TestEqual(TEXT("handled by the native reaction fallback (GameplayCue.DF.Reaction)"), Reaction->HandlerTag, DFGameplayLocalTags::ReactionCueRoot());
		TestTrue(TEXT("burst 12 of 100"), FMath::IsNearlyEqual(Reaction->RawMagnitude, 12.f, 1e-3f));
		TestTrue(TEXT("fraction 0.12"), FMath::IsNearlyEqual(Reaction->NormalizedMagnitude, 0.12f, 1e-4f));
		TestTrue(TEXT("target is the enemy"), Reaction->Target == F.Enemy);
	}
	const FDFMsg_GameplayCue* ChillRemoved = Seen.FindByPredicate([](const FDFMsg_GameplayCue& M) { return M.Cue == DFGameplayLocalTags::StatusCue(TEXT("chill"), TEXT("Removed")); });
	TestNotNull(TEXT("the consumed chill's Removed cue reached the bus"), ChillRemoved);
	TestTrue(TEXT("health 88 after the burst"), FMath::IsNearlyEqual(F.Health->GetHealth(), 88.f, 1e-3f));
	Bus->Unsubscribe(Handle);
	F.Shutdown();
	return true;
}

// Step.cs applies the shooter's build factor at the WEAPON call site (`weapon.Damage *
// build.DamageFactor(armored)`, Step.cs:558) and Damage() itself applies none — so the two
// appliers that are not shots carry the row's own number and nothing else: UpdateStatuses ticks
// `def.DamagePerSecond * Balance.Dt` (Step.cs:1041) and a reaction bursts `enemy.MaxHp *
// reaction.BurstFraction` (Step.cs:1086). A 1.2x hero's burn ticks 0.2, not 0.24, and its
// thermalShock bursts 12, not 14.4.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDFStatusDotIgnoresSourceFactorsTest, "DF.Unit.Status.DotIgnoresSourceFactors", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FDFStatusDotIgnoresSourceFactorsTest::RunTest(const FString& Parameters)
{
	// The DoT tick: 100 hp, no armor, burn 6 dps / 30 Hz = 0.2 a tick — from an Overclocked hero.
	FWorldFixture F;
	if (!TestTrue(TEXT("standalone world"), F.Init(100.f)))
	{
		return false;
	}
	AActor* Overclocked = F.SpawnApplier(FGameplayTag(), /*DamageFactor*/ 1.2f);
	UAbilitySystemComponent* ApplierASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Overclocked);
	if (!TestNotNull(TEXT("the applier has an ASC"), ApplierASC))
	{
		F.Shutdown();
		return false;
	}
	// Without this the rest proves nothing: the factor must really be on the instigator's set,
	// which is what MakeOutgoingSpec snapshot-captures.
	TestTrue(TEXT("the applier really carries DamageFactor 1.2"),
		FMath::IsNearlyEqual(ApplierASC->GetNumericAttribute(UDFCombatSet::GetDamageFactorAttribute()), 1.2f, 1e-4f));

	FDFTickCounter Ticks;
	Ticks.Bind(F.Health, 0.2f);   // 0.24 if the applier's DamageFactor reached the tick
	TestEqual(TEXT("burn applied by the Overclocked hero"), F.Status->Apply(DFTags::Status_Burn, Overclocked), EDFStatusApplyResult::Applied);
	F.Tick(1.f);
	TestTrue(*FString::Printf(TEXT("every tick is the row's 0.2, not 1.2 x 0.2 = 0.24 (%d ticks)"), Ticks.Ticks), Ticks.bEveryTickExpected && Ticks.Ticks > 0);
	TestTrue(*FString::Printf(TEXT("~30 ticks in a second (got %d)"), Ticks.Ticks), Ticks.Ticks >= MinTicksPerSecond && Ticks.Ticks <= MaxTicksPerSecond);
	TestTrue(*FString::Printf(TEXT("health lost 0.2 x %d (got %.3f)"), Ticks.Ticks, 100.f - F.Health->GetHealth()),
		FMath::IsNearlyEqual(100.f - F.Health->GetHealth(), 0.2f * Ticks.Ticks, 1e-2f));
	// The instigator is still the applier — only its factors were dropped (bounty and scrap credit).
	TestTrue(TEXT("the Overclocked hero is still the DoT's source"),
		F.Status->GetResolver().Slot(EDFStatusChannel::Thermal).SourceId == static_cast<int32>(Overclocked->GetUniqueID()));
	F.Shutdown();

	// The reaction burst: 12% of 100 hp is 12, whoever lit it.
	FWorldFixture B;
	if (!TestTrue(TEXT("standalone world 2"), B.Init(100.f)))
	{
		return false;
	}
	AActor* BurstApplier = B.SpawnApplier(FGameplayTag(), /*DamageFactor*/ 1.2f);
	TestEqual(TEXT("chill applied"), B.Status->Apply(DFTags::Status_Chill, BurstApplier), EDFStatusApplyResult::Applied);
	TestEqual(TEXT("burn onto chill reacts"), B.Status->Apply(DFTags::Status_Burn, BurstApplier), EDFStatusApplyResult::Reacted);
	TestTrue(*FString::Printf(TEXT("health 88: the burst is 12, not 1.2 x 12 = 14.4 (got %.2f)"), B.Health->GetHealth()),
		FMath::IsNearlyEqual(B.Health->GetHealth(), 88.f, 1e-3f));
	B.Shutdown();
	return true;
}

// FDFStatusSlotRep::EndTimeServer is a SERVER timestamp, written on the server and replicated
// verbatim. Subtracting it from a client's own UWorld::GetTimeSeconds() — seconds since ITS level
// load — is meaningless: a client that joined 120 s after the server's level load would read a
// 1.5 s chill as 121.5 s on every status bar. Both machines must read the one shared clock,
// AGameStateBase::GetServerWorldTimeSeconds(). The skew here is constructed, never assumed zero.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDFStatusTimeRemainingUsesServerClockTest, "DF.Unit.Status.TimeRemainingUsesServerClock", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FDFStatusTimeRemainingUsesServerClockTest::RunTest(const FString& Parameters)
{
	FWorldFixture F;
	if (!TestTrue(TEXT("standalone world"), F.Init(100.f)))
	{
		return false;
	}
	AGameStateBase* GameState = F.SpawnGameState(/*SkewSeconds*/ 120.f);
	if (!TestNotNull(TEXT("a game state to carry the server clock"), GameState))
	{
		F.Shutdown();
		return false;
	}
	TestTrue(TEXT("the world reports it"), F.World->GetGameState() == GameState);

	const float LocalNow = F.World->GetTimeSeconds();
	const float ServerNow = static_cast<float>(GameState->GetServerWorldTimeSeconds());
	if (!TestTrue(*FString::Printf(TEXT("the clocks really are 120 s apart (local %.2f, server %.2f)"), LocalNow, ServerNow),
		FMath::IsNearlyEqual(ServerNow - LocalNow, 120.f, 1e-2f)))
	{
		F.Shutdown();
		return false;
	}

	TestEqual(TEXT("chill applied"), F.Status->Apply(DFTags::Status_Chill, nullptr), EDFStatusApplyResult::Applied);
	const float EndTimeServer = F.Status->GetSlots()[static_cast<int32>(EDFStatusChannel::Movement)].EndTimeServer;

	// What goes on the wire is in the server's clock, because that is the only clock a client can
	// reconstruct. This is the assertion the old local-clock Now() fails.
	TestTrue(*FString::Printf(TEXT("EndTimeServer is a SERVER timestamp: %.2f, not the local %.2f"), EndTimeServer, LocalNow + 1.5f),
		FMath::IsNearlyEqual(EndTimeServer, ServerNow + 1.5f, 1e-2f));
	TestTrue(TEXT("and so it is not the local clock's value"), EndTimeServer > LocalNow + 100.f);
	// Stated plainly: read against the local clock this slot would claim 121.5 s left.
	TestTrue(TEXT("a local-clock read would have said 121.5 s"), FMath::IsNearlyEqual(EndTimeServer - LocalNow, 121.5f, 1e-2f));
	TestTrue(*FString::Printf(TEXT("TimeRemaining is the row's 1.5 s (got %.3f)"), F.Status->TimeRemaining(EDFStatusChannel::Movement)),
		FMath::IsNearlyEqual(F.Status->TimeRemaining(EDFStatusChannel::Movement), 1.5f, 1e-2f));

	// The skew does not disturb expiry either: the resolver ticks on the same clock it stamped.
	F.Tick(1.f);
	TestTrue(TEXT("still chilled after 1 s"), F.Status->IsChannelActive(EDFStatusChannel::Movement));
	TestTrue(*FString::Printf(TEXT("~0.5 s left (got %.3f)"), F.Status->TimeRemaining(EDFStatusChannel::Movement)),
		FMath::IsNearlyEqual(F.Status->TimeRemaining(EDFStatusChannel::Movement), 0.5f, 1.5e-1f));
	F.Tick(0.8f);
	TestFalse(TEXT("expired on time despite the skew"), F.Status->IsChannelActive(EDFStatusChannel::Movement));
	TestTrue(TEXT("an empty channel has no time left"), FMath::IsNearlyEqual(F.Status->TimeRemaining(EDFStatusChannel::Movement), 0.f, 1e-4f));
	F.Shutdown();
	return true;
}

// Statuses.cs: poison is the status that bypasses armor AND shield — its entire reason to exist
// (Step.cs:1042 `ignoreFlatArmor: def.IgnoresArmor, ignoreShield: def.IgnoresShield`). The one line
// that carries a row's identity into the damage path is UDFDamageContext::SetStatus, so this drives
// a Toxin DoT through the world on a Warden-like target: armor 2, shield 20.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDFStatusPoisonBypassesArmorAndShieldTest, "DF.Unit.Status.PoisonDotBypassesArmorAndShield", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FDFStatusPoisonBypassesArmorAndShieldTest::RunTest(const FString& Parameters)
{
	// Poison, 3 dps / 30 Hz = 0.1 a tick, on 2 flat armor behind a 20 shield: neither touches it.
	FWorldFixture Warden;
	if (!TestTrue(TEXT("standalone world"), Warden.Init(100.f, /*FlatArmor*/ 2.f, /*Shield*/ 20.f)))
	{
		return false;
	}
	FDFTickCounter PoisonTicks;
	PoisonTicks.Bind(Warden.Health, 0.1f);   // 0.5 if the armor floor bit; the row says it does not
	TestEqual(TEXT("poison lands through the shield"), Warden.Status->Apply(DFTags::Status_Poison, nullptr), EDFStatusApplyResult::Applied);
	Warden.Tick(1.f);
	TestTrue(*FString::Printf(TEXT("every poison tick is 0.1, not the 0.5 armor floor (%d ticks)"), PoisonTicks.Ticks), PoisonTicks.bEveryTickExpected && PoisonTicks.Ticks > 0);
	TestTrue(*FString::Printf(TEXT("~30 ticks in a second (got %d)"), PoisonTicks.Ticks), PoisonTicks.Ticks >= MinTicksPerSecond && PoisonTicks.Ticks <= MaxTicksPerSecond);
	TestTrue(*FString::Printf(TEXT("the shield never soaked a tick (got %.2f)"), Warden.Health->GetShield()), FMath::IsNearlyEqual(Warden.Health->GetShield(), 20.f, 1e-3f));
	TestTrue(*FString::Printf(TEXT("health took all of it: 0.1 x %d (got %.3f)"), PoisonTicks.Ticks, 100.f - Warden.Health->GetHealth()),
		FMath::IsNearlyEqual(100.f - Warden.Health->GetHealth(), 0.1f * PoisonTicks.Ticks, 1e-2f));
	Warden.Shutdown();

	// Burn on the same armor does NOT bypass: 0.2 floored to 0.5 by the 2 armor. (A shield would
	// refuse burn outright — Step.cs ApplyStatus — so the comparison target is unshielded.)
	FWorldFixture Ram;
	if (!TestTrue(TEXT("standalone world 2"), Ram.Init(100.f, /*FlatArmor*/ 2.f)))
	{
		return false;
	}
	FDFTickCounter BurnTicks;
	BurnTicks.Bind(Ram.Health, 0.5f);
	TestEqual(TEXT("burn applied"), Ram.Status->Apply(DFTags::Status_Burn, nullptr), EDFStatusApplyResult::Applied);
	Ram.Tick(1.f);
	TestTrue(*FString::Printf(TEXT("burn is armor-floored to 0.5 — it bypasses nothing (%d ticks)"), BurnTicks.Ticks), BurnTicks.bEveryTickExpected && BurnTicks.Ticks > 0);
	Ram.Shutdown();

	// The two flags are independent, and poison (which sets both) cannot tell them apart. Two
	// fixture rows that set exactly one each pin which flag SetStatus writes to which field.
	// acid: Toxin, 6 dps, ignores ARMOR only -> 0.2 a tick, and the shield soaks all of it.
	// venom: Toxin, 6 dps, ignores SHIELD only -> 0.2 floored to 0.5 by the armor, straight to health.
	FWorldFixture Acid;
	if (!TestTrue(TEXT("standalone world 3"), Acid.Init(100.f, /*FlatArmor*/ 2.f, /*Shield*/ 20.f)))
	{
		return false;
	}
	Acid.Status->AddStatusRowOverride(TEXT("acid"), DFTestRows::Status(EDFStatusChannel::Toxin, 6.f, 1.f, 6.f, 1.f, 0.f, false, /*bIgnoresArmor*/ true, /*bIgnoresShield*/ false));
	FDFTickCounter AcidTicks;
	AcidTicks.Bind(Acid.Health, 0.2f);   // 0.5 if the flags were swapped, 0.5 if SetStatus never ran
	TestEqual(TEXT("acid applied"), Acid.Status->ApplyById(TEXT("acid"), nullptr), EDFStatusApplyResult::Applied);
	Acid.Tick(1.f);
	TestTrue(*FString::Printf(TEXT("acid ticks 0.2: armor ignored, shield not (%d ticks)"), AcidTicks.Ticks), AcidTicks.bEveryTickExpected && AcidTicks.Ticks > 0);
	TestTrue(*FString::Printf(TEXT("the shield soaked every acid tick (got %.2f)"), Acid.Health->GetShield()),
		FMath::IsNearlyEqual(Acid.Health->GetShield(), 20.f - 0.2f * AcidTicks.Ticks, 1e-2f));
	TestTrue(*FString::Printf(TEXT("health untouched behind the shield (got %.2f)"), Acid.Health->GetHealth()), FMath::IsNearlyEqual(Acid.Health->GetHealth(), 100.f, 1e-3f));
	Acid.Shutdown();

	FWorldFixture Venom;
	if (!TestTrue(TEXT("standalone world 4"), Venom.Init(100.f, /*FlatArmor*/ 2.f, /*Shield*/ 20.f)))
	{
		return false;
	}
	Venom.Status->AddStatusRowOverride(TEXT("venom"), DFTestRows::Status(EDFStatusChannel::Toxin, 6.f, 1.f, 6.f, 1.f, 0.f, false, /*bIgnoresArmor*/ false, /*bIgnoresShield*/ true));
	FDFTickCounter VenomTicks;
	VenomTicks.Bind(Venom.Health, 0.5f);   // 0.2 if the flags were swapped
	TestEqual(TEXT("venom applied"), Venom.Status->ApplyById(TEXT("venom"), nullptr), EDFStatusApplyResult::Applied);
	Venom.Tick(1.f);
	TestTrue(*FString::Printf(TEXT("venom ticks the 0.5 armor floor: shield ignored, armor not (%d ticks)"), VenomTicks.Ticks), VenomTicks.bEveryTickExpected && VenomTicks.Ticks > 0);
	TestTrue(*FString::Printf(TEXT("the shield never soaked a venom tick (got %.2f)"), Venom.Health->GetShield()), FMath::IsNearlyEqual(Venom.Health->GetShield(), 20.f, 1e-3f));
	TestTrue(*FString::Printf(TEXT("health took 0.5 x %d (got %.2f)"), VenomTicks.Ticks, 100.f - Venom.Health->GetHealth()),
		FMath::IsNearlyEqual(100.f - Venom.Health->GetHealth(), 0.5f * VenomTicks.Ticks, 1e-2f));
	Venom.Shutdown();
	return true;
}

// Step.cs:545 `bool armored = enemyDef.FlatArmor > 0f || enemyDef.FrontArmorArcDegrees > 0f` — read
// off the enemy DEFINITION, so shred (which only lowers the per-hit armor) can never turn a Ram
// into a valid hollow-point target (Gunsmith.cs:144-150). It lives with the world tests because the
// end-to-end half needs a real shred on a real attribute aggregator.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDFDamageShredKeepsTargetArmoredTest, "DF.Unit.Damage.ShredKeepsTargetArmored", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FDFDamageShredKeepsTargetArmoredTest::RunTest(const FString& Parameters)
{
	// The pure-math half: the row says 2, shred took the attribute to 0, hollow point still misses out.
	{
		FDFDamageInput Shredded;
		Shredded.BaseDamage = 7.f;
		Shredded.RowFlatArmor = 2.f;        // enemyDef.FlatArmor
		Shredded.FlatArmor = 0.f;           // the attribute, after shred -2
		Shredded.bTargetShredded = true;
		Shredded.UnarmoredBonusFactor = 1.3f;
		TestTrue(TEXT("a shredded ram is still armored"), Shredded.IsArmored());
		TestTrue(TEXT("7, not 9.1: no hollow-point bonus"), FMath::IsNearlyEqual(FDFDamageMath::Compute(Shredded).Damage, 7.f, 1e-4f));

		// A genuinely unarmored target (the row says 0) still gets the bonus.
		FDFDamageInput Drifter = Shredded;
		Drifter.RowFlatArmor = 0.f;
		TestFalse(TEXT("a drifter is unarmored"), Drifter.IsArmored());
		TestTrue(TEXT("hollow point on a drifter: 9.1"), FMath::IsNearlyEqual(FDFDamageMath::Compute(Drifter).Damage, 9.1f, 1e-4f));
	}

	// The world half: a real Defense status on a real FlatArmor aggregator. Base 2, shred -2 -> 0.
	FWorldFixture F;
	if (!TestTrue(TEXT("standalone world"), F.Init(100.f, /*FlatArmor*/ 2.f)))
	{
		return false;
	}
	TestEqual(TEXT("shred applied"), F.Status->Apply(DFTags::Status_Shred, nullptr), EDFStatusApplyResult::Applied);
	TestTrue(TEXT("shred drove the FlatArmor attribute to 0"), FMath::IsNearlyEqual(F.ASC->GetNumericAttribute(UDFHealthSet::GetFlatArmorAttribute()), 0.f, 1e-4f));

	// A hollow-point hit: 10 base, UnarmoredBonusFactor 1.3 on the context, no arc.
	UDFDamageContext* Hit = UDFDamageContext::Make(F.Enemy, DFTags::Damage_Type_Kinetic, FGameplayTag());
	Hit->UnarmoredBonusFactor = 1.3f;
	FGameplayEffectContextHandle HitContext = F.ASC->MakeEffectContext();
	Hit->AttachTo(HitContext);
	FGameplayEffectSpecHandle Spec = F.ASC->MakeOutgoingSpec(UDFGE_Damage::StaticClass(), 1.f, HitContext);
	if (!TestTrue(TEXT("damage spec"), Spec.IsValid()))
	{
		F.Shutdown();
		return false;
	}
	Spec.Data->SetSetByCallerMagnitude(DFTags::SetByCaller_Damage, 10.f);
	F.ASC->ApplyGameplayEffectSpecToSelf(*Spec.Data);
	TestTrue(*FString::Printf(TEXT("health 90: 10, not 13 — the row's armor still says armored (got %.2f)"), F.Health->GetHealth()),
		FMath::IsNearlyEqual(F.Health->GetHealth(), 90.f, 1e-3f));
	F.Shutdown();
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
