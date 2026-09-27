#pragma once

#include "CoreMinimal.h"
#include "DFGameMode.h"
#include "Engine/EngineBaseTypes.h"
#include "DFTestHostGameMode.generated.h"

/**
 * ADFGameMode on a host that starts listening between InitGame and StartPlay, as a PIE listen host does
 * (UGameInstance::StartPlayInEditorGameInstance: InitializeActorsForPlay, EnableListenServer, BeginPlay).
 * A test world has no net driver, so until bListening it answers as the world is (standalone: 0), and
 * from then as a listen server whose net driver has ServerTravelPause (BaseEngine.ini: 4 s). DF.Unit.Match.
 */
UCLASS(NotBlueprintable, NotPlaceable, HideDropdown)
class ADFTestHostGameMode : public ADFGameMode
{
	GENERATED_BODY()

public:
	/** Stands in for EnableListenServer. */
	bool bListening = false;
	float ServerTravelPause = 4.f;

protected:
	virtual float CurrentRestartLead() const override
	{
		return bListening ? RestartLeadFor(NM_ListenServer, ServerTravelPause) : Super::CurrentRestartLead();
	}
};
