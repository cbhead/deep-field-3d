#include "Abilities/DFAbilitySystemComponent.h"
#include "Attributes/DFHealthSet.h"
#include "Attributes/DFMovementSet.h"
#include "DFGameplayTags.h"
#include "DFTestRows.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "Messages/DFMessageBus.h"
#include "Messages/DFMessages.h"
#include "Misc/AutomationTest.h"
#include "Status/DFStatusComponent.h"
#include "Tint/DFTintComponent.h"
#include "Tint/DFTintLayout.h"

#if WITH_DEV_AUTOMATION_TESTS

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
}

// The world half: an actor with a DF ASC, health + movement sets, a status component and a tint
// component in a standalone game world (a UGameInstance so the message bus subsystem exists).
// chill then burn through the component must detonate thermalShock for 12% of MaxHealth on
// UDFHealthSet, remove chill's effect and tags, and broadcast DF.Message.ReactionTriggered.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDFStatusComponentThermalShockTest, "DF.Unit.Status.ComponentThermalShockInWorld", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FDFStatusComponentThermalShockTest::RunTest(const FString& Parameters)
{
	UGameInstance* GI = NewObject<UGameInstance>(GEngine);
	GI->AddToRoot();
	GI->InitializeStandalone();
	UWorld* World = GI->GetWorld();
	if (!TestNotNull(TEXT("standalone world"), World))
	{
		return false;
	}
	World->InitializeActorsForPlay(FURL());
	World->BeginPlay();

	int32 Reactions = 0;
	int32 Applied = 0;
	FDFMessageHandle ReactionHandle;
	FDFMessageHandle AppliedHandle;
	UDFMessageBus* Bus = UDFMessageBus::Get(World);
	if (Bus)
	{
		ReactionHandle = Bus->Subscribe<FDFMsg_Status>(DFTags::Message_ReactionTriggered, [&Reactions](const FGameplayTag&, const FDFMsg_Status&) { ++Reactions; });
		AppliedHandle = Bus->Subscribe<FDFMsg_Status>(DFTags::Message_StatusApplied, [&Applied](const FGameplayTag&, const FDFMsg_Status&) { ++Applied; });
	}

	AActor* Enemy = World->SpawnActor<AActor>();
	UDFAbilitySystemComponent* ASC = NewObject<UDFAbilitySystemComponent>(Enemy, TEXT("ASC"));
	ASC->RegisterComponent();
	UDFHealthSet* Health = NewObject<UDFHealthSet>(Enemy, TEXT("HealthSet"));
	UDFMovementSet* Movement = NewObject<UDFMovementSet>(Enemy, TEXT("MovementSet"));
	ASC->AddAttributeSetSubobject(Health);
	ASC->AddAttributeSetSubobject(Movement);
	ASC->InitAbilityActorInfo(Enemy, Enemy);
	Health->InitMaxHealth(100.f);
	Health->InitHealth(100.f);
	Movement->InitBaseSpeed(250.f);
	Movement->InitSpeedFactor(1.f);

	UDFTintComponent* Tint = NewObject<UDFTintComponent>(Enemy, TEXT("Tint"));
	Tint->RegisterComponent();

	UDFStatusComponent* Status = NewObject<UDFStatusComponent>(Enemy, TEXT("Status"));
	for (const TPair<FName, FDFStatusRow>& Row : DFTestRows::AllStatuses())
	{
		Status->AddStatusRowOverride(Row.Key, Row.Value);
	}
	Status->AddReactionRowOverride(TEXT("thermalShock"), DFTestRows::ThermalShock());
	Status->AddReactionRowOverride(TEXT("flashFreeze"), DFTestRows::FlashFreeze());
	Status->AddReactionRowOverride(TEXT("corrode"), DFTestRows::Corrode());
	Status->RegisterComponent();

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

	// burn on its own now lands and ticks on application: 6 dps x 0.25 s = 1.5.
	TestEqual(TEXT("burn applied"), Status->Apply(DFTags::Status_Burn, nullptr), EDFStatusApplyResult::Applied);
	TestTrue(TEXT("Thermal tag granted"), ASC->HasMatchingGameplayTag(DFTags::Status_Channel_Thermal));
	TestTrue(TEXT("first burn tick: 86.5"), FMath::IsNearlyEqual(Health->GetHealth(), 86.5f, 1e-3f));
	TestEqual(TEXT("tint shows burn"), Tint->GetStatusTintSource(), FGameplayTag(DFTags::Status_Burn));

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
	TickWorld(World, 10.f);
	TestFalse(TEXT("Movement expired"), Status->IsChannelActive(EDFStatusChannel::Movement));
	TestFalse(TEXT("Tether expired"), Status->IsChannelActive(EDFStatusChannel::Tether));
	TestFalse(TEXT("no Movement tag"), ASC->HasMatchingGameplayTag(DFTags::Status_Channel_Movement));
	TestFalse(TEXT("no Tether tag"), ASC->HasMatchingGameplayTag(DFTags::Status_Channel_Tether));

	if (Bus)
	{
		Bus->Unsubscribe(ReactionHandle);
		Bus->Unsubscribe(AppliedHandle);
	}
	Enemy->Destroy();
	GI->Shutdown();
	GEngine->DestroyWorldContext(World);
	World->DestroyWorld(false);
	GI->RemoveFromRoot();
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
