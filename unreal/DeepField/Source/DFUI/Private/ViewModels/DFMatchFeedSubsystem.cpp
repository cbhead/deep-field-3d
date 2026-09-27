#include "ViewModels/DFMatchFeedSubsystem.h"

#include "DFMatchState.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "ViewModels/DFMatchStateFeed.h"
#include "ViewModels/DFMatchViewModel.h"
#include "ViewModels/DFViewModelSubsystem.h"

bool UDFMatchFeedSubsystem::DoesSupportWorldType(const EWorldType::Type WorldType) const
{
	// Worlds that play a match; never the editor's own world or a preview scene.
	return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
}

void UDFMatchFeedSubsystem::Deinitialize()
{
	if (bFed)
	{
		if (const UDFViewModelSubsystem* ViewModels = UDFViewModelSubsystem::Get(this))
		{
			if (UDFMatchViewModel* Match = ViewModels->GetMatch())
			{
				Match->Reset();
			}
		}
		bFed = false;
	}
	Super::Deinitialize();
}

void UDFMatchFeedSubsystem::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	Refresh();
}

TStatId UDFMatchFeedSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UDFMatchFeedSubsystem, STATGROUP_Tickables);
}

bool UDFMatchFeedSubsystem::Refresh()
{
	UWorld* World = GetWorld();
	const ADFMatchState* State = World ? World->GetGameState<ADFMatchState>() : nullptr;
	const UDFViewModelSubsystem* ViewModels = UDFViewModelSubsystem::Get(this);
	UDFMatchViewModel* Match = ViewModels ? ViewModels->GetMatch() : nullptr;
	if (!State || !Match)
	{
		return false;
	}
	// This machine's own player, if it has one (a dedicated server has none: Local() stays null there).
	const UGameInstance* GameInstance = World->GetGameInstance();
	const APlayerController* LocalController = GameInstance ? GameInstance->GetFirstLocalPlayerController(World) : nullptr;
	FDFMatchStateFeed::Fill(*Match, *State, LocalController ? LocalController->PlayerState.Get() : nullptr);
	bFed = true;
	return true;
}
