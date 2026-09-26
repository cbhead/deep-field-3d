#include "ViewModels/DFViewModelSubsystem.h"

#include "DFGameplayTags.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "MVVMGameSubsystem.h"
#include "Messages/DFMessages.h"
#include "Types/MVVMViewModelContext.h"
#include "ViewModels/DFMatchViewModel.h"

const FName UDFViewModelSubsystem::MatchViewModelName(TEXT("DFMatch"));

UDFViewModelSubsystem* UDFViewModelSubsystem::Get(const UObject* WorldContext)
{
	const UWorld* World = WorldContext ? WorldContext->GetWorld() : nullptr;
	const UGameInstance* GameInstance = World ? World->GetGameInstance() : nullptr;
	return GameInstance ? GameInstance->GetSubsystem<UDFViewModelSubsystem>() : nullptr;
}

void UDFViewModelSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	Match = NewObject<UDFMatchViewModel>(this);

	if (const UMVVMGameSubsystem* Mvvm = Collection.InitializeDependency<UMVVMGameSubsystem>())
	{
		FMVVMViewModelContext Context;
		Context.ContextClass = UDFMatchViewModel::StaticClass();
		Context.ContextName = MatchViewModelName;
		Mvvm->GetViewModelCollection()->AddViewModelInstance(Context, Match);
	}

	if (UDFMessageBus* Bus = Collection.InitializeDependency<UDFMessageBus>())
	{
		ConnectionStateHandle = Bus->Subscribe<FDFMsg_Tagged>(DFTags::Message_ConnectionState,
			[WeakThis = TWeakObjectPtr<UDFViewModelSubsystem>(this)](const FGameplayTag&, const FDFMsg_Tagged& Msg)
			{
				if (WeakThis.IsValid() && WeakThis->Match)
				{
					WeakThis->Match->SetConnectionState(Msg.Tag);
				}
			});
	}
}

void UDFViewModelSubsystem::Deinitialize()
{
	if (const UGameInstance* GameInstance = GetGameInstance())
	{
		if (UDFMessageBus* Bus = GameInstance->GetSubsystem<UDFMessageBus>())
		{
			Bus->Unsubscribe(ConnectionStateHandle);
		}
		if (const UMVVMGameSubsystem* Mvvm = GameInstance->GetSubsystem<UMVVMGameSubsystem>())
		{
			if (UMVVMViewModelCollectionObject* Models = Mvvm->GetViewModelCollection())
			{
				FMVVMViewModelContext Context;
				Context.ContextClass = UDFMatchViewModel::StaticClass();
				Context.ContextName = MatchViewModelName;
				Models->RemoveViewModel(Context);
			}
		}
	}
	Match = nullptr;
	Super::Deinitialize();
}
