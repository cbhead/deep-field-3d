#include "World/DFOperatedGate.h"

#include "DFWorldCollision.h"
#include "Engine/OverlapResult.h"
#include "Engine/World.h"
#include "Net/UnrealNetwork.h"

ADFOperatedGate::ADFOperatedGate()
{
	bReplicates = true;
	bAlwaysRelevant = true;
	NetDormancy = DORM_DormantAll;   // wakes on a flip; a lever that is not pulled costs no bandwidth
}

void ADFOperatedGate::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ADFOperatedGate, bClosed);
}

float ADFOperatedGate::CooldownRemaining() const
{
	const UWorld* World = GetWorld();
	if (!World)
	{
		return 0.f;
	}
	return FMath::Max(0.f, CooldownSeconds - (World->GetTimeSeconds() - LastToggleTime));
}

bool ADFOperatedGate::HasBodyWithin() const
{
	const UWorld* World = GetWorld();
	if (!World)
	{
		return false;
	}
	TArray<FOverlapResult> Overlaps;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(DFOperatedGateBodyCheck), false, this);
	return World->OverlapMultiByObjectType(Overlaps, GetActorLocation(), FQuat::Identity,
		FCollisionObjectQueryParams(DFCollision::Enemy), FCollisionShape::MakeSphere(BodyCheckMeters * 100.f), Params);
}

bool ADFOperatedGate::CanToggle(const FVector& FromLocation, FName* OutReason) const
{
	if (FVector::Dist(FromLocation, GetActorLocation()) > ReachMeters * 100.f)
	{
		if (OutReason) { *OutReason = TEXT("tooFar"); }
		return false;
	}
	if (CooldownRemaining() > 0.f)
	{
		if (OutReason) { *OutReason = TEXT("cooldown"); }
		return false;
	}
	// Only a close is denied by a body: opening on top of one frees it, which is the point.
	if (!bClosed && HasBodyWithin())
	{
		if (OutReason) { *OutReason = TEXT("bodyInGateway"); }
		return false;
	}
	return true;
}

bool ADFOperatedGate::TryToggle(const FVector& FromLocation, FName* OutReason)
{
	if (!HasAuthority())
	{
		if (OutReason) { *OutReason = TEXT("notAuthority"); }
		return false;
	}
	if (!CanToggle(FromLocation, OutReason))
	{
		return false;
	}
	bClosed = !bClosed;
	LastToggleTime = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.f;
	FlushNetDormancy();
	OnRep_Closed();
	return true;
}

void ADFOperatedGate::OnRep_Closed()
{
	// The lane's edge state lives on the match state (FastArray, ADR-0004); the lever only owns
	// its own switch. WS-02/05 wire the edge closure and the DF.Message.LaneClosed message.
}
