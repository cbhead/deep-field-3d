#include "Movement/DFEnemyMovement.h"

#include "AbilitySystemComponent.h"
#include "Attributes/DFMovementSet.h"
#include "Content/DFContentRows.h"
#include "Content/DFContentSubsystem.h"
#include "GameFramework/Actor.h"
#include "LaneGraph/DFLaneGraphAsset.h"
#include "Status/DFStatusComponent.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(DFEnemyMovement)

DEFINE_LOG_CATEGORY_STATIC(LogDFEnemyMove, Log, All);

UDFEnemyMovement::UDFEnemyMovement()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = false;   // nothing to drive until Configure succeeds
	PrimaryComponentTick.TickGroup = TG_PrePhysics;
	SetIsReplicatedByDefault(false);                      // ADFEnemy replicates the position, not this
}

float UDFEnemyMovement::ResolveSpeed(const FDFEnemySpeedInputs& In)
{
	float Speed = FMath::Max(0.f, In.RowSpeedMetersPerSec);

	// A Shade nobody has revealed moves faster.
	if (In.bStealth && !In.bRevealed && In.StealthSpeedBonus > 0.f)
	{
		Speed *= In.StealthSpeedBonus;
	}

	// Whatever the Movement channel wrote (chill, tar, rubble).
	Speed *= FMath::Max(0.f, In.StatusSpeedFactor);

	// Enrage, from the row, and *before* the two that zero it — so a frozen Ram is still frozen
	// however badly it is losing. Panic does not beat being frozen solid.
	if (In.EnrageBelowHpFraction > 0.f && In.HpFraction <= In.EnrageBelowHpFraction)
	{
		Speed *= In.EnrageSpeedFactor;
	}

	if (In.bSieging || In.bHardControlled)
	{
		return 0.f;
	}
	return FMath::Max(0.f, Speed);
}

bool UDFEnemyMovement::IsBurrowedAt(float TotalTraveledMeters, float CycleMeters, float BurrowedMeters)
{
	if (CycleMeters <= 0.f || BurrowedMeters <= 0.f)
	{
		return false;
	}
	const float IntoCycle = FMath::Fmod(FMath::Max(0.f, TotalTraveledMeters), CycleMeters);
	return IntoCycle < BurrowedMeters;
}

void UDFEnemyMovement::BeginPlay()
{
	Super::BeginPlay();
	Status = GetOwner() ? GetOwner()->FindComponentByClass<UDFStatusComponent>() : nullptr;
}

bool UDFEnemyMovement::Configure(const UDFLaneGraphAsset* InGraph, FName InItineraryId, float LateralOffsetCm,
	const FDFEnemyRow& Row, TSharedPtr<const FDFLaneRouting> InRouting, FString& OutError)
{
	auto Fail = [&OutError](const FString& Why)
	{
		OutError = FString::Printf(TEXT("enemy movement: %s"), *Why);
		return false;
	};

	if (!InGraph)
	{
		return Fail(TEXT("no lane graph"));
	}
	if (!InRouting.IsValid())
	{
		return Fail(TEXT("no routing tables — they are shared per map and rebuilt when an edge changes, not built here"));
	}
	Itinerary = InGraph->FindItinerary(InItineraryId);
	if (!Itinerary)
	{
		// The wave table named a route this map has no itinerary for. DF.Map.Validate is meant to
		// catch this at authoring time (WS-09); refusing here is the runtime half of that rule.
		return Fail(FString::Printf(TEXT("map '%s' has no itinerary '%s'"), *InGraph->MapId.ToString(), *InItineraryId.ToString()));
	}

	Graph = InGraph;
	Routing = MoveTemp(InRouting);
	ItineraryId = InItineraryId;

	RowSpeedMetersPerSec = Row.SpeedMetersPerSec;
	bStealth = Row.bStealth;
	StealthSpeedBonus = Row.StealthSpeedBonus;
	EnrageBelowHpFraction = Row.EnrageBelowHpFraction;
	EnrageSpeedFactor = Row.EnrageSpeedFactor;
	bBurrower = Row.bBurrower;

	Params = FDFLaneWalkerParams();
	Params.SpeedMetersPerSec = Row.SpeedMetersPerSec;
	Params.RowSpeedMetersPerSec = Row.SpeedMetersPerSec;   // siege prices with the row, never the effective speed
	Params.SlopeSpeedUp = Row.SlopeSpeedUp;
	Params.SlopeSpeedDown = Row.SlopeSpeedDown;
	Params.StructureDps = Row.StructureDps;

	if (const UDFContentSubsystem* Content = UDFContentSubsystem::Get(this))
	{
		BurrowCycleMeters = Content->Balance(TEXT("burrowCycleMeters"), 0.f);
		BurrowedMeters = Content->Balance(TEXT("burrowedMeters"), 0.f);
		Params.SiegeBreachBias = Content->Balance(TEXT("siegeBreachBias"), Params.SiegeBreachBias);
	}

	TArray<FDFWalkEvent> Events;
	if (!FDFLaneWalker::Begin(*Graph, *Itinerary, Params, *Routing, LateralOffsetCm, Walker, Events))
	{
		return Fail(FString::Printf(TEXT("nothing open out of itinerary '%s' — do not spawn a body that cannot start"), *InItineraryId.ToString()));
	}
	for (const FDFWalkEvent& Event : Events)
	{
		OnWalkEvent.Broadcast(Event);
	}

	bBurrowed = bBurrower && IsBurrowedAt(0.f, BurrowCycleMeters, BurrowedMeters);
	SetComponentTickEnabled(true);
	return true;
}

FDFEnemySpeedInputs UDFEnemyMovement::GatherSpeedInputs() const
{
	FDFEnemySpeedInputs In;
	In.RowSpeedMetersPerSec = RowSpeedMetersPerSec;
	In.bStealth = bStealth;
	In.StealthSpeedBonus = StealthSpeedBonus;
	In.EnrageBelowHpFraction = EnrageBelowHpFraction;
	In.EnrageSpeedFactor = EnrageSpeedFactor;
	In.bSieging = bSieging;

	if (Status)
	{
		In.bRevealed = Status->IsChannelActive(EDFStatusChannel::Detection);
		In.bHardControlled = Status->IsControlled();
	}

	// The Movement channel writes UDFMovementSet::SpeedFactor (C4), so that attribute is the one
	// authority on how much a status has slowed this body — not the status row, which does not know
	// about stacking, and not this component.
	const AActor* Owner = GetOwner();
	const UAbilitySystemComponent* Asc = Owner ? Owner->FindComponentByClass<UAbilitySystemComponent>() : nullptr;
	if (const UDFMovementSet* Movement = Asc ? Cast<UDFMovementSet>(Asc->GetAttributeSet(UDFMovementSet::StaticClass())) : nullptr)
	{
		In.StatusSpeedFactor = Movement->GetSpeedFactor();
	}
	// No ASC is a legitimate state, not a fault: a fixture drives the rules directly, and a body
	// spawns a frame before its attributes are granted. The row's speed is the honest default.
	return In;
}

void UDFEnemyMovement::TickComponent(float DeltaSeconds, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaSeconds, TickType, ThisTickFunction);

	AActor* Owner = GetOwner();
	if (!Owner || !Owner->HasAuthority() || !Graph || !Itinerary || !Routing.IsValid() || !Walker.IsWalking())
	{
		return;
	}

	Params.SpeedMetersPerSec = ResolveSpeed(GatherSpeedInputs());

	TArray<FDFWalkEvent> Events;
	FDFLaneWalker::Advance(*Graph, *Itinerary, Params, DeltaSeconds, *Routing, Walker, Events);

	if (bBurrower)
	{
		bBurrowed = IsBurrowedAt(Walker.TotalTraveledCm / 100.f, BurrowCycleMeters, BurrowedMeters);
	}

	// Position before events: a listener handling ReachedCore will destroy this actor, and it should
	// see the body where it actually ended rather than where it was a frame ago.
	if (Walker.IsWalking())
	{
		Owner->SetActorLocationAndRotation(FDFLaneWalker::LocationOf(*Graph, Walker),
			FDFLaneWalker::FacingOf(*Graph, Walker).Rotation());
	}

	for (const FDFWalkEvent& Event : Events)
	{
		OnWalkEvent.Broadcast(Event);
		if (!IsValid(Owner) || !IsValid(this))
		{
			return;   // a listener removed the body mid-relay (a leak, a kill)
		}
	}
}

float UDFEnemyMovement::KnockBack(float Meters)
{
	if (!Graph || !Walker.IsWalking())
	{
		return 0.f;
	}
	const float Given = FDFLaneWalker::KnockBack(*Graph, Walker, Meters);
	if (bBurrower)
	{
		bBurrowed = IsBurrowedAt(Walker.TotalTraveledCm / 100.f, BurrowCycleMeters, BurrowedMeters);
	}
	if (AActor* Owner = GetOwner(); Owner && Given > 0.f)
	{
		Owner->SetActorLocationAndRotation(FDFLaneWalker::LocationOf(*Graph, Walker),
			FDFLaneWalker::FacingOf(*Graph, Walker).Rotation());
	}
	return Given;
}

float UDFEnemyMovement::RemainingToCoreMeters() const
{
	if (!Graph || !Itinerary || !Routing.IsValid())
	{
		return 0.f;
	}
	return FDFLaneWalker::RemainingToCoreMeters(*Graph, *Itinerary, Walker, Routing->EdgeOpen);
}
