#include "Enemies/DFEnemy.h"

#include "Abilities/DFAbilitySystemComponent.h"
#include "Attributes/DFHealthSet.h"
#include "Attributes/DFMovementSet.h"
#include "Combat/DFStructure.h"
#include "Components/StaticMeshComponent.h"
#include "Content/DFContentRows.h"
#include "Content/DFContentSubsystem.h"
#include "DFGameplayTags.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Look/DFShapeLook.h"
#include "Materials/MaterialInterface.h"
#include "Messages/DFMessageBus.h"
#include "Messages/DFMessages.h"
#include "Movement/DFEnemyMovement.h"
#include "Movement/DFLaneWalker.h"
#include "Net/UnrealNetwork.h"
#include "Status/DFStatusComponent.h"
#include "TimerManager.h"
#include "UObject/ConstructorHelpers.h"
#include "Waves/DFWaveDirector.h"
#include "LaneGraph/DFLaneGraphAsset.h"

DEFINE_LOG_CATEGORY_STATIC(LogDFEnemy, Log, All);

namespace DFEnemyLook
{
	// The placeholder body: the engine's 1 m cylinder (pivot at its centre), scaled to the sim's enemy.
	constexpr float WidthCm = 80.f;
	constexpr float HeightCm = 160.f;
	/** How far toward the wound colour a body at 0 health has gone. */
	constexpr float WoundWeight = 0.75f;
	const FLinearColor Wound(0.79f, 0.23f, 0.16f);   // #C93B28, M_Enemy's HpFrac colour

	/** Host-wide target ids, from 1 (0 = unknown in every C15 message). */
	int32 NextTargetId = 1;
}

ADFEnemy::ADFEnemy()
{
	bReplicates = true;
	SetReplicatingMovement(true);
	PrimaryActorTick.bCanEverTick = false;   // UDFEnemyMovement ticks; the actor has nothing of its own

	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);

	Body = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Body"));
	Body->SetupAttachment(Root);
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Cylinder(TEXT("/Engine/BasicShapes/Cylinder"));
	if (Cylinder.Succeeded())
	{
		Body->SetStaticMesh(Cylinder.Object);
	}
	// The lane puts the actor's origin on the ground; the cylinder's pivot is its centre.
	Body->SetRelativeLocation(FVector(0.f, 0.f, DFEnemyLook::HeightCm * 0.5f));
	Body->SetRelativeScale3D(FVector(DFEnemyLook::WidthCm / 100.f, DFEnemyLook::WidthCm / 100.f, DFEnemyLook::HeightCm / 100.f));
	// Walking bodies do not shove the hero or each other around. Towers find them through the target
	// registry, not collision; Visibility still blocks so the hero's aim and traces see them.
	Body->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	Body->SetCollisionResponseToAllChannels(ECR_Ignore);
	Body->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);

	Movement = CreateDefaultSubobject<UDFEnemyMovement>(TEXT("Movement"));

	// C4: an enemy's ASC replicates Minimal (attributes to every machine, effects stay on the host).
	AbilitySystem = CreateDefaultSubobject<UDFAbilitySystemComponent>(TEXT("AbilitySystem"));
	AbilitySystem->SetIsReplicated(true);
	AbilitySystem->SetReplicationMode(EGameplayEffectReplicationMode::Minimal);
	HealthSet = CreateDefaultSubobject<UDFHealthSet>(TEXT("HealthSet"));
	MovementSet = CreateDefaultSubobject<UDFMovementSet>(TEXT("MovementSet"));
	Status = CreateDefaultSubobject<UDFStatusComponent>(TEXT("Status"));
}

UAbilitySystemComponent* ADFEnemy::GetAbilitySystemComponent() const
{
	return AbilitySystem;
}

void ADFEnemy::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME_CONDITION(ADFEnemy, Entry, COND_InitialOnly);
	DOREPLIFETIME_CONDITION(ADFEnemy, TargetId, COND_InitialOnly);
}

void ADFEnemy::BeginPlay()
{
	Super::BeginPlay();
	AbilitySystem->InitAbilityActorInfo(this, this);
	// Every machine: the tint follows health as it replicates.
	AbilitySystem->GetGameplayAttributeValueChangeDelegate(UDFHealthSet::GetHealthAttribute()).AddWeakLambda(this,
		[this](const FOnAttributeChangeData&) { RefreshHealthTint(); });
	if (HasAuthority())
	{
		HealthSet->OnHealthDepleted.AddUObject(this, &ADFEnemy::HandleHealthDepleted);
		if (UDFTargetRegistry* Registry = UDFTargetRegistry::Get(this))
		{
			Registry->Register(this);
			bRegistered = true;
		}
	}
}

const FDFEnemyRow* ADFEnemy::FindRow() const
{
	const UDFContentSubsystem* Content = UDFContentSubsystem::Get(GetWorld());
	return Content && !Entry.DefId.IsNone() ? Content->Enemy(Entry.DefId) : nullptr;
}

void ADFEnemy::InitAttributes(const FDFEnemyRow& Row)
{
	const float MaxHealth = FMath::Max(1.f, Row.Hp * Entry.HpFactor);
	HealthSet->InitMaxHealth(MaxHealth);
	HealthSet->InitHealth(MaxHealth);
	HealthSet->InitMaxShield(Row.Shield);
	HealthSet->InitShield(Row.Shield);
	HealthSet->InitFlatArmor(Row.FlatArmor);
	MovementSet->InitBaseSpeed(Row.SpeedMetersPerSec * 100.f);
	MovementSet->InitSpeedFactor(1.f);
	Status->TargetId = TargetId;
	Status->InitFromEnemyRow(Row);
}

float ADFEnemy::GetHealthFraction() const
{
	const float Max = HealthSet ? HealthSet->GetMaxHealth() : 0.f;
	return Max > 0.f ? FMath::Clamp(HealthSet->GetHealth() / Max, 0.f, 1.f) : 1.f;
}

EDFEnemyLayer ADFEnemy::GetTargetLayer() const
{
	const FDFEnemyRow* Row = FindRow();
	return Row ? Row->Layer : EDFEnemyLayer::Ground;
}

bool ADFEnemy::IsTargetBurrowed() const
{
	return Movement && Movement->IsBurrowed();
}

bool ADFEnemy::IsTargetStealthy() const
{
	const FDFEnemyRow* Row = FindRow();
	return Row && Row->bStealth;
}

bool ADFEnemy::BlocksTowerSight() const
{
	const FDFEnemyRow* Row = FindRow();
	return Row && Row->bBlocksSight;
}

float ADFEnemy::GetRemainingToCore() const
{
	// Opaque sort key (INT 2026-09-21): a walker with nowhere to go sorts last.
	if (!Movement || !Movement->IsOnLane())
	{
		return TNumericLimits<float>::Max();
	}
	return Movement->RemainingToCoreMeters();
}

float ADFEnemy::GetKnockbackMass() const
{
	const FDFEnemyRow* Row = FindRow();
	return Row ? Row->Mass : 1.f;
}

void ADFEnemy::ApplyKnockback(float Meters)
{
	if (HasAuthority() && Movement && !bKilled)
	{
		Movement->KnockBack(Meters);
	}
}

void ADFEnemy::HandleHealthDepleted(AActor* HitInstigator, AActor* Causer, const FGameplayEffectSpec* Spec, float Magnitude, float OldValue, float NewValue)
{
	if (bKilled)
	{
		return;
	}
	// Dead from now on for every later reader (the next tower this frame skips it; the hit that did it
	// still applies its statuses, which do not ask). The body leaves next frame, outside the hit.
	bKilled = true;
	Killer = HitInstigator ? HitInstigator : Causer;
	if (Movement)
	{
		Movement->SetComponentTickEnabled(false);   // a corpse does not walk into the core
	}
	GetWorldTimerManager().SetTimerForNextTick(FTimerDelegate::CreateUObject(this, &ADFEnemy::Die));
}

void ADFEnemy::Die()
{
	if (IsActorBeingDestroyed())
	{
		return;
	}
	UE_LOG(LogDFEnemy, Log, TEXT("%s (%s) killed by %s"), *GetName(), *Entry.DefId.ToString(), *GetNameSafe(Killer.Get()));
	if (UDFMessageBus* Bus = UDFMessageBus::Get(this))
	{
		FDFMsg_Kill Msg;
		Msg.EnemyId = TargetId;
		Msg.DefId = Entry.DefId;
		Msg.Bounty = Bounty;
		Msg.Location = GetActorLocation();
		if (const IDFStructure* Structure = Cast<IDFStructure>(Killer.Get()))
		{
			Msg.KillerStructureId = Structure->GetStructureId();
		}
		Bus->BroadcastTeam(DFTags::Message_EnemyKilled, Msg);
	}
	OnKilled.Broadcast(this, Killer.Get());
	Destroy();   // EndPlay tells the director
}

ADFEnemy* ADFEnemy::SpawnFromEntry(UWorld* World, const FDFSpawnEntry& InEntry, const UDFLaneGraphAsset* Graph,
	TSharedPtr<const FDFLaneRouting> Routing, ADFWaveDirector* InDirector, FString& OutError)
{
	// A body that never makes it onto the lane still has to leave the wave's count, or the wave waits
	// for it forever.
	auto Refuse = [&OutError, InDirector](const FString& Why) -> ADFEnemy*
	{
		OutError = Why;
		if (InDirector)
		{
			InDirector->NotifyEnemyRemoved();
		}
		return nullptr;
	};

	if (!World || !Graph)
	{
		return Refuse(TEXT("no world or no lane graph"));
	}
	const UDFContentSubsystem* Content = UDFContentSubsystem::Get(World);
	const FDFEnemyRow* Row = Content ? Content->Enemy(InEntry.DefId) : nullptr;
	if (!Row)
	{
		return Refuse(FString::Printf(TEXT("the content has no enemy '%s'"), *InEntry.DefId.ToString()));
	}

	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	Params.bDeferConstruction = true;
	ADFEnemy* Enemy = World->SpawnActor<ADFEnemy>(ADFEnemy::StaticClass(), FTransform::Identity, Params);
	if (!Enemy)
	{
		return Refuse(TEXT("SpawnActor failed"));
	}
	Enemy->Entry = InEntry;
	Enemy->Director = InDirector;
	Enemy->TargetId = DFEnemyLook::NextTargetId++;
	// Step.cs spawns with Bounty = ScaledBounty(def.Bounty, w.WaveIndex): fixed now, for this wave,
	// rounded half away from zero and never below 1.
	const int32 Wave = InDirector ? FMath::Max(0, InDirector->GetWaveIndex()) : 0;
	const float BountyScale = InDirector && InDirector->IsConfigured() ? FDFWavePlan::BountyScale(InDirector->GetTables().Dials, Wave) : 1.f;
	Enemy->Bounty = FMath::Max(1, static_cast<int32>(FMath::RoundHalfFromZero(static_cast<float>(Row->Bounty) * BountyScale)));
	Enemy->InitAttributes(*Row);
	Enemy->FinishSpawning(FTransform::Identity);

	// Listen before Configure: it relays the walker's first events.
	Enemy->Movement->OnWalkEvent.AddUObject(Enemy, &ADFEnemy::HandleWalkEvent);
	FString Why;
	if (!Enemy->Movement->Configure(Graph, InEntry.RouteId, InEntry.LateralOffset * 100.f, *Row, MoveTemp(Routing), Why))
	{
		OutError = Why;
		Enemy->Destroy();   // EndPlay tells the director
		return nullptr;
	}
	const FDFLaneWalkerState& Walker = Enemy->Movement->GetWalkerState();
	Enemy->SetActorLocationAndRotation(FDFLaneWalker::LocationOf(*Graph, Walker), FDFLaneWalker::FacingOf(*Graph, Walker).Rotation());
	Enemy->ApplyPlaceholderLook();
	return Enemy;
}

void ADFEnemy::HandleWalkEvent(const FDFWalkEvent& Event)
{
	if (Event.Kind != EDFWalkEventKind::ReachedCore || bKilled)
	{
		return;
	}
	UE_LOG(LogDFEnemy, Log, TEXT("%s (%s) reached the core"), *GetName(), *Entry.DefId.ToString());
	// The team hears it first: the economy takes the lives now, before EndPlay reports the body gone
	// to the director (DFMatchSeams.h: that order makes a last-enemy leak a Defeat).
	if (UDFMessageBus* Bus = UDFMessageBus::Get(this))
	{
		const FDFEnemyRow* Row = FindRow();
		FDFMsg_Enemy Msg;
		Msg.EnemyId = TargetId;
		Msg.DefId = Entry.DefId;
		Msg.Location = GetActorLocation();
		Msg.Phase = Row ? Row->LeakDamage : 1;   // C15: Phase carries the leak's lives
		Bus->BroadcastTeam(DFTags::Message_EnemyLeaked, Msg);
	}
	OnLeaked.Broadcast(this);
	Destroy();
}

void ADFEnemy::EndPlay(const EEndPlayReason::Type Reason)
{
	if (bRegistered)
	{
		if (UDFTargetRegistry* Registry = UDFTargetRegistry::Get(this))
		{
			Registry->Unregister(this);
		}
		bRegistered = false;
	}
	GetWorldTimerManager().ClearAllTimersForObject(this);
	// Once, on the host, and only when the body leaves a running game: a level being torn down is not
	// the wave losing an enemy.
	if (Reason == EEndPlayReason::Destroyed && HasAuthority() && !bReportedGone)
	{
		bReportedGone = true;
		if (ADFWaveDirector* D = Director.Get())
		{
			D->NotifyEnemyRemoved();
		}
	}
	Super::EndPlay(Reason);
}

void ADFEnemy::ApplyPlaceholderLook()
{
	if (!Body || Entry.DefId.IsNone())
	{
		return;
	}
	// One stable colour per enemy id, so a mixed wave reads at a glance.
	const uint32 Hash = GetTypeHash(Entry.DefId.ToString());
	BaseColour = FLinearColor::MakeFromHSV8(static_cast<uint8>(Hash & 0xFF), 200, 230);
	bTinted = true;
	RefreshHealthTint();
}

void ADFEnemy::RefreshHealthTint()
{
	if (!bTinted || !Body)
	{
		return;
	}
	// A hurt body reads as hurt: toward the wound colour and darker as health falls.
	const float Hurt = 1.f - GetHealthFraction();
	const FLinearColor Colour = FMath::Lerp(BaseColour, DFEnemyLook::Wound, Hurt * DFEnemyLook::WoundWeight) * FMath::Lerp(1.f, 0.45f, Hurt);
	DFShapeLook::Tint(Body, Colour);
}
