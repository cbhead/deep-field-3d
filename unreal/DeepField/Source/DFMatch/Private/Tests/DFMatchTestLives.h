#pragma once

#include "Components/ActorComponent.h"
#include "CoreMinimal.h"
#include "Match/DFMatchSeams.h"
#include "DFMatchTestLives.generated.h"

/** A stand-in for WS-06's economy component in DF.Unit.Match tests: lives the test sets by hand. */
UCLASS(NotBlueprintable, HideDropdown)
class UDFMatchTestLives : public UActorComponent, public IDFMatchLivesSource
{
	GENERATED_BODY()

public:
	int32 Lives = 20;

	virtual int32 GetLives() const override { return Lives; }
};
