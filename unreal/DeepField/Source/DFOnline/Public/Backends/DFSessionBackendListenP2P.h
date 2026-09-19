#pragma once

#include "CoreMinimal.h"
#include "IDFSessionBackend.h"

// The Level 1 backend: the host opens its map as a listen server and clients travel to the
// resolved connect string. Which socket layer carries the packets is the GameNetDriver definition
// in DefaultEngine.ini (IpNetDriver now, NetDriverEOS with ForceRelays once the product exists).
class DFONLINE_API FDFSessionBackendListenP2P : public IDFSessionBackend
{
public:
	virtual FName GetName() const override { return TEXT("ListenP2P"); }
	virtual bool StartHost(UWorld* World, FName MapId, const FString& Options) override;
	virtual bool Join(UWorld* World, const FString& ConnectString, const FString& Options) override;
	virtual void Leave(UWorld* World) override;
	virtual FName GetNetDriverClassName() const override;

	/** "foundry" -> "L_Foundry" (the level naming rule, C7); an asset path is passed through. */
	static FString MapIdToLevelName(FName MapId);

	/** The map the game returns to after a session (GameDefaultMap). */
	static FString FrontEndMap();
};
