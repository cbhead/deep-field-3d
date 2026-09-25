#include "Combat/DFStructure.h"

#include "Engine/World.h"
#include "GameFramework/Actor.h"

UDFStructureRegistry* UDFStructureRegistry::Get(const UObject* WorldContext)
{
	const UWorld* World = WorldContext ? WorldContext->GetWorld() : nullptr;
	return World ? World->GetSubsystem<UDFStructureRegistry>() : nullptr;
}

void UDFStructureRegistry::Register(AActor* Actor)
{
	if (!IsValid(Actor) || !Actor->GetClass()->ImplementsInterface(UDFStructure::StaticClass()))
	{
		return;
	}
	for (const TWeakObjectPtr<AActor>& Existing : Structures)
	{
		if (Existing.Get() == Actor)
		{
			return;
		}
	}
	Structures.Add(Actor);
}

void UDFStructureRegistry::Unregister(AActor* Actor)
{
	Structures.RemoveAll([Actor](const TWeakObjectPtr<AActor>& Entry) { return !Entry.IsValid() || Entry.Get() == Actor; });
}

TArray<AActor*> UDFStructureRegistry::GetStructures()
{
	TArray<AActor*> Out;
	Out.Reserve(Structures.Num());
	bool bStale = false;
	for (const TWeakObjectPtr<AActor>& Entry : Structures)
	{
		if (AActor* Actor = Entry.Get(); IsValid(Actor))
		{
			Out.Add(Actor);
		}
		else
		{
			bStale = true;
		}
	}
	if (bStale)
	{
		Structures.RemoveAll([](const TWeakObjectPtr<AActor>& Entry) { return !IsValid(Entry.Get()); });
	}
	return Out;
}

AActor* UDFStructureRegistry::FindSiegeTarget(const FVector& PositionCm, float ReachMeters)
{
	AActor* Target = nullptr;
	double Nearest = TNumericLimits<double>::Max();
	const double ReachCm = static_cast<double>(ReachMeters) * 100.0;
	for (AActor* Actor : GetStructures())
	{
		const IDFStructure* Structure = Cast<IDFStructure>(Actor);
		if (!Structure || Structure->GetStructureMaxHp() <= 0.f)
		{
			continue;   // indestructible
		}
		const double Distance = FVector::Dist(PositionCm, Structure->GetStructurePosition());
		if (Distance <= ReachCm && Distance < Nearest)
		{
			Nearest = Distance;
			Target = Actor;
		}
	}
	return Target;
}

bool UDFStructureRegistry::Siege(const FVector& PositionCm, float ReachMeters, float Dps, float DeltaSeconds, int32 AttackerId)
{
	if (Dps <= 0.f)
	{
		return false;
	}
	AActor* Target = FindSiegeTarget(PositionCm, ReachMeters);
	IDFStructure* Structure = Target ? Cast<IDFStructure>(Target) : nullptr;
	if (!Structure)
	{
		return false;
	}
	Structure->ApplySiegeDamage(Dps * DeltaSeconds, AttackerId);
	return true;
}

AActor* UDFStructureRegistry::FindRepairTarget(const FVector& PositionCm, float ReachMeters)
{
	const double ReachCm = static_cast<double>(ReachMeters) * 100.0;
	for (AActor* Actor : GetStructures())
	{
		const IDFStructure* Structure = Cast<IDFStructure>(Actor);
		if (!Structure)
		{
			continue;
		}
		const float Max = Structure->GetStructureMaxHp();
		const float Hp = Structure->GetStructureHp();
		if (Max <= 0.f || Hp >= Max || Hp <= 0.f)
		{
			continue;   // indestructible, whole, or already rubble
		}
		if (FVector::Dist(PositionCm, Structure->GetStructurePosition()) > ReachCm)
		{
			continue;
		}
		return Actor;
	}
	return nullptr;
}

bool UDFStructureRegistry::RepairNearby(const FVector& PositionCm, float ReachMeters, float Amount, int32 PlayerId)
{
	AActor* Target = FindRepairTarget(PositionCm, ReachMeters);
	IDFStructure* Structure = Target ? Cast<IDFStructure>(Target) : nullptr;
	if (!Structure)
	{
		return false;
	}
	Structure->ApplyRepair(Amount, PlayerId);
	return true;
}
