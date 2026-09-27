#include "Combat/DFTargetable.h"

#include "Engine/World.h"
#include "GameFramework/Actor.h"

UDFTargetRegistry* UDFTargetRegistry::Get(const UObject* WorldContext)
{
	const UWorld* World = WorldContext ? WorldContext->GetWorld() : nullptr;
	return World ? World->GetSubsystem<UDFTargetRegistry>() : nullptr;
}

void UDFTargetRegistry::Register(AActor* Actor)
{
	if (!IsValid(Actor) || !Actor->GetClass()->ImplementsInterface(UDFTargetable::StaticClass()))
	{
		return;
	}
	for (const TWeakObjectPtr<AActor>& Existing : Targets)
	{
		if (Existing.Get() == Actor)
		{
			return;
		}
	}
	Targets.Add(Actor);
}

void UDFTargetRegistry::Unregister(AActor* Actor)
{
	Targets.RemoveAll([Actor](const TWeakObjectPtr<AActor>& Entry) { return !Entry.IsValid() || Entry.Get() == Actor; });
}

TArray<AActor*> UDFTargetRegistry::GetTargets()
{
	TArray<AActor*> Out;
	Out.Reserve(Targets.Num());
	bool bStale = false;
	for (const TWeakObjectPtr<AActor>& Entry : Targets)
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
		Targets.RemoveAll([](const TWeakObjectPtr<AActor>& Entry) { return !IsValid(Entry.Get()); });
	}
	return Out;
}
