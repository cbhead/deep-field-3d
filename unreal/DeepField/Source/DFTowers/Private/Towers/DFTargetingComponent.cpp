#include "Towers/DFTargetingComponent.h"

#include "Combat/DFTargetable.h"
#include "DFWorldCollision.h"
#include "Engine/HitResult.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "Status/DFStatusComponent.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(DFTargetingComponent)

UDFTargetingComponent::UDFTargetingComponent()
{
	PrimaryComponentTick.bCanEverTick = false;   // the tower asks; nothing here runs on its own
	SetIsReplicatedByDefault(false);             // host-only: clients see the tower's replicated target
}

FVector UDFTargetingComponent::MuzzleLocation() const
{
	const AActor* Owner = GetOwner();
	return (Owner ? Owner->GetActorLocation() : FVector::ZeroVector) + FVector(0.f, 0.f, SightHeightCm);
}

bool UDFTargetingComponent::IsDetected(const AActor* Body)
{
	const UDFStatusComponent* Status = Body ? Body->FindComponentByClass<UDFStatusComponent>() : nullptr;
	return Status && Status->IsChannelActive(EDFStatusChannel::Detection);
}

bool UDFTargetingComponent::IsMarked(const AActor* Body)
{
	const UDFStatusComponent* Status = Body ? Body->FindComponentByClass<UDFStatusComponent>() : nullptr;
	return Status && Status->IsChannelActive(EDFStatusChannel::Vulnerability);
}

TArray<DFTowerMath::FDFTargetCandidate> UDFTargetingComponent::GatherCandidates(TArray<AActor*>& OutActors) const
{
	TArray<DFTowerMath::FDFTargetCandidate> Candidates;
	OutActors.Reset();
	UDFTargetRegistry* Registry = UDFTargetRegistry::Get(this);
	if (!Registry)
	{
		return Candidates;
	}
	for (AActor* Actor : Registry->GetTargets())
	{
		const IDFTargetable* Body = Cast<IDFTargetable>(Actor);
		if (!Body)
		{
			continue;
		}
		DFTowerMath::FDFTargetCandidate& C = Candidates.AddDefaulted_GetRef();
		C.Id = Body->GetTargetId();
		C.PositionCm = Body->GetTargetPosition();
		C.Layer = Body->GetTargetLayer();
		C.bDead = Body->IsTargetDead();
		C.bBurrowed = Body->IsTargetBurrowed();
		C.bStealth = Body->IsTargetStealthy();
		C.bDetected = C.bStealth && IsDetected(Actor);   // only asked when it matters
		C.RemainingToCore = Body->GetRemainingToCore();
		OutActors.Add(Actor);
	}
	return Candidates;
}

bool UDFTargetingComponent::IsSightBlocked(AActor* Target, TConstArrayView<AActor*> Bodies) const
{
	const IDFTargetable* TargetBody = Cast<IDFTargetable>(Target);
	UWorld* World = GetWorld();
	if (!TargetBody || !World)
	{
		return true;
	}
	const FVector From = MuzzleLocation();
	const FVector To = TargetBody->GetAimPoint();

	// Terrain and registered sight blockers (§3.2: "towers need line of sight; terrain blocks it").
	FCollisionQueryParams Params(SCENE_QUERY_STAT(DFTowerSight), /*bTraceComplex*/ false);
	Params.AddIgnoredActor(GetOwner());
	Params.AddIgnoredActor(Target);
	FHitResult Hit;
	if (World->LineTraceSingleByChannel(Hit, From, To, DFCollision::Sight, Params))
	{
		return true;
	}

	// Living cover (Step.cs SightBlocked): a sight-blocking body between the tower and the target.
	// The sim measures from the tower's position; so does this, so the geometry is the sim's.
	const FVector TowerPosition = GetOwner() ? GetOwner()->GetActorLocation() : From;
	for (AActor* Other : Bodies)
	{
		const IDFTargetable* Blocker = Cast<IDFTargetable>(Other);
		if (!Blocker || Other == Target || Blocker->IsTargetDead() || !Blocker->BlocksTowerSight())
		{
			continue;
		}
		if (DFTowerMath::IsSightBlockedByBody(TowerPosition, TargetBody->GetTargetPosition(), Blocker->GetTargetPosition()))
		{
			return true;
		}
	}
	return false;
}

AActor* UDFTargetingComponent::PickTarget(const FDFTowerRow& Row, float RangeMeters)
{
	TArray<AActor*> Actors;
	const TArray<DFTowerMath::FDFTargetCandidate> Candidates = GatherCandidates(Actors);
	const FVector TowerPosition = GetOwner() ? GetOwner()->GetActorLocation() : FVector::ZeroVector;
	const int32 Index = DFTowerMath::PickTarget(Row, TowerPosition, RangeMeters, Candidates,
		[this, &Actors](int32 i) { return IsSightBlocked(Actors[i], Actors); });
	return Actors.IsValidIndex(Index) ? Actors[Index] : nullptr;
}

float UDFTargetingComponent::NoteTarget(AActor* Target, const FDFConditionRow* Condition)
{
	const IDFTargetable* Body = Cast<IDFTargetable>(Target);
	if (!Body)
	{
		LastTargetId = INDEX_NONE;
		return 0.f;
	}
	const int32 Id = Body->GetTargetId();
	if (Id == LastTargetId)
	{
		return 0.f;
	}
	LastTargetId = Id;
	return DFTowerMath::AcquisitionDelaySeconds(Condition, IsMarked(Target));
}
