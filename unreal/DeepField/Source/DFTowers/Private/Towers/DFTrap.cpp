#include "Towers/DFTrap.h"

#include "Combat/DFTargetable.h"
#include "Components/SceneComponent.h"
#include "Content/DFContentSubsystem.h"
#include "DFGameplayTags.h"
#include "Messages/DFMessageBus.h"
#include "Messages/DFMessages.h"
#include "Net/UnrealNetwork.h"
#include "Towers/DFTower.h"
#include "Towers/DFTowerDamage.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(DFTrap)

DEFINE_LOG_CATEGORY_STATIC(LogDFTrap, Log, All);

ADFTrap::ADFTrap()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickGroup = TG_PostPhysics;   // after the enemies have moved, as the towers
	bReplicates = true;
	SetReplicatingMovement(false);
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
}

void ADFTrap::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ADFTrap, DefId);
	DOREPLIFETIME(ADFTrap, SocketId);
	DOREPLIFETIME(ADFTrap, StructureId);
	DOREPLIFETIME(ADFTrap, OwnerSeat);
	DOREPLIFETIME(ADFTrap, ChargesLeft);
}

const FDFTrapRow* ADFTrap::GetRow() const
{
	const UDFContentSubsystem* Content = UDFContentSubsystem::Get(this);
	return (Content && !DefId.IsNone()) ? Content->Trap(DefId) : nullptr;
}

bool ADFTrap::InitializeTrap(FName InDefId, FName InSocketId, int32 InOwnerSeat)
{
	DefId = InDefId;
	const FDFTrapRow* Row = GetRow();
	if (!Row)
	{
		UE_LOG(LogDFTrap, Error, TEXT("%s: no trap row '%s'"), *GetName(), *InDefId.ToString());
		DefId = NAME_None;
		return false;
	}
	SocketId = InSocketId;
	OwnerSeat = InOwnerSeat;
	StructureId = ADFTower::AllocateStructureId();
	ChargesLeft = Row->Charges;
	RearmTimer = 0.f;
	ForceNetUpdate();
	return true;
}

void ADFTrap::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (HasAuthority())
	{
		StepTrap(DeltaSeconds);
	}
}

bool ADFTrap::StepTrap(float DeltaSeconds)
{
	if (RearmTimer > 0.f)
	{
		// Step.cs: count down and skip this step, even the step it reaches zero.
		RearmTimer -= DeltaSeconds;
		if (RearmTimer <= 0.f && ChargesLeft > 0)
		{
			Announce(DFTags::Message_TrapRearmed, TEXT("armed"));
		}
		return false;
	}
	const FDFTrapRow* Row = GetRow();
	if (!Row || ChargesLeft <= 0)
	{
		return false;
	}
	UDFTargetRegistry* Registry = UDFTargetRegistry::Get(this);
	if (!Registry)
	{
		return false;
	}

	const float RadiusCm = Row->TriggerRadius * 100.f;
	const FVector At = GetActorLocation();
	const FName Applies[] = { Row->Applies };
	bool bFired = false;
	for (AActor* Actor : Registry->GetTargets())
	{
		IDFTargetable* Body = Cast<IDFTargetable>(Actor);
		if (!Body || Body->IsTargetDead() || Body->IsTargetBurrowed() || Body->GetTargetLayer() != EDFEnemyLayer::Ground)
		{
			continue;
		}
		if (FVector::Dist(At, Body->GetTargetPosition()) > RadiusCm)
		{
			continue;
		}
		bFired = true;
		// Step.cs order: damage; then the status if it still lives; then the knockback if it still lives.
		DFTowerDamage::ApplyDamage(this, Actor, Row->Damage, DFTags::Damage_Type_Kinetic, DFTags::Damage_Source_Trap);
		if (!Row->Applies.IsNone() && !Body->IsTargetDead())
		{
			DFTowerDamage::ApplyStatuses(this, Actor, Applies);
		}
		if (Row->KnockbackMeters > 0.f && !Body->IsTargetDead())
		{
			Body->ApplyKnockback(Row->KnockbackMeters / FMath::Max(Body->GetKnockbackMass(), 0.25f));
		}
	}
	if (bFired)
	{
		--ChargesLeft;
		RearmTimer = Row->RearmSeconds;
		Announce(DFTags::Message_TrapTriggered, ChargesLeft > 0 ? FName(TEXT("triggered")) : FName(TEXT("spent")));
	}
	return bFired;
}

void ADFTrap::Announce(const FGameplayTag& Tag, FName State) const
{
	if (UDFMessageBus* Bus = UDFMessageBus::Get(this))
	{
		FDFMsg_Structure Message;
		Message.StructureId = StructureId;
		Message.PlayerId = OwnerSeat;
		Message.DefId = DefId;
		Message.SocketId = SocketId;
		Message.State = State;
		Bus->BroadcastTeam(Tag, Message);
	}
}
