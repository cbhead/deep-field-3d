#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "DFMatchFeedSubsystem.generated.h"

/**
 * Runs the real feed (FDFMatchStateFeed) once a frame in every game world: standalone, listen host,
 * client and PIE alike, with no branch on which of those it is (C12). Whatever the world's game state
 * is, if it is an ADFMatchState it is copied into the game instance's UDFMatchViewModel; a world with no
 * match state (L_Test_UI, whose fake feed owns the model) is left alone.
 *
 * When the world goes away the model is Reset, so the next map never shows the last one's match
 * (ConnectionState, the session's, survives as Reset promises).
 */
UCLASS()
class DFUI_API UDFMatchFeedSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual void Deinitialize() override;
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;

	/** One pass now. Tick calls it; a test calls it after giving the world its game state. True if a
	 *  match state was found and written. */
	bool Refresh();

protected:
	virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override;

private:
	/** This world's match has been written into the model at least once, so leaving it must clear it. */
	bool bFed = false;
};
