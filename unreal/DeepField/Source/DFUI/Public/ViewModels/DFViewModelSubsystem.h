#pragma once

#include "Messages/DFMessageBus.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "DFViewModelSubsystem.generated.h"

class UDFMatchViewModel;

/** Owns the one UDFMatchViewModel of this game instance and publishes it in the MVVM global
 *  collection as "DFMatch", so a WBP_ resolves it by name ("Global Viewmodel Collection") with no
 *  code. Discrete state reaches the model from the message bus here (today: ConnectionState);
 *  continuous state is written by whichever feed is active. */
UCLASS()
class DFUI_API UDFViewModelSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	/** The name widgets use in the global view-model collection. */
	static const FName MatchViewModelName;

	static UDFViewModelSubsystem* Get(const UObject* WorldContext);

	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	UFUNCTION(BlueprintPure, Category = "DF|UI")
	UDFMatchViewModel* GetMatch() const { return Match; }

private:
	UPROPERTY(Transient)
	TObjectPtr<UDFMatchViewModel> Match;

	FDFMessageHandle ConnectionStateHandle;
};
