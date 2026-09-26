#pragma once

#include "CoreMinimal.h"
#include "Engine/GameInstance.h"
#include "DFGameInstance.generated.h"

/** Project game instance: hosts the content, message-bus and (later) online subsystems. */
UCLASS()
class DFCORE_API UDFGameInstance : public UGameInstance
{
	GENERATED_BODY()

public:
	virtual void Init() override;

	/** Build label shown in the connection screen and carried in the session handshake (C14). */
	UFUNCTION(BlueprintPure, Category = "DF")
	static FString GetBuildLabel();
};
