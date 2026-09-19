#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "DFGameMode.generated.h"

// Server-only match rules (ADR-0004). Skeleton for WS-00's L_Dev_Empty smoke; WS-03/05/09
// and DFMatch's owners fill in seats, factions, waves, lives and end conditions.
UCLASS()
class DFMATCH_API ADFGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	ADFGameMode();

	virtual void PreLogin(const FString& Options, const FString& Address, const FUniqueNetIdRepl& UniqueId, FString& ErrorMessage) override;
	virtual void PostLogin(APlayerController* NewPlayer) override;
	virtual void Logout(AController* Exiting) override;
};
