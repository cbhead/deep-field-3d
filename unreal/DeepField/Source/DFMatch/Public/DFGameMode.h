#pragma once

#include "CoreMinimal.h"
#include "DFMatchState.h"
#include "GameFramework/GameModeBase.h"
#include "DFGameMode.generated.h"

// Server-only match rules (ADR-0004). WS-28 (ADR-0024) since 2026-09-25, from WS-00's skeleton.
// It picks the match classes (ADFMatchState, ADFPlayerState, ADFPlayerController), reads the match
// settings from the travel URL, seats players, and runs the C14 join seams at PreLogin.
//
// URL options (all optional): ?seed=<n> the plan seed · ?lobby the party assembles until the launch
// seat sends Launch · ?endless · ?waitforplayers (implied on a dedicated server) · ?wavesmap=<id> the
// wave tables to play (default: the level's lane graph) · ?intermission=<seconds> · ?restart=<seconds>
// from Victory / Defeat to a new match (default ADFMatchState::RestartDelaySeconds, 15; 0 = never).
//
// A new match is this map again with these options: when the match state's restart clock runs out
// (ADFMatchState::OnRestartRequested) the game mode server-travels to "?Restart", which the engine
// resolves to the last URL (AGameMode::RestartGame's travel; this class is AGameModeBase, which has none).
// It is a hard travel (bUseSeamlessTravel is off): a listen host's clients are told to follow and
// reconnect, and every actor, the economy and the seats' records come back from nothing. A ?lobby match
// returns to its lobby; -DFDemo spawns its director again with the new match state.
UCLASS()
class DFMATCH_API ADFGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	ADFGameMode();

	virtual void InitGame(const FString& MapName, const FString& Options, FString& ErrorMessage) override;
	virtual void InitGameState() override;
	virtual void PreLogin(const FString& Options, const FString& Address, const FUniqueNetIdRepl& UniqueId, FString& ErrorMessage) override;
	virtual void PostLogin(APlayerController* NewPlayer) override;
	virtual void Logout(AController* Exiting) override;

	/** Seats a match holds (C14: 1-4 players). */
	static constexpr int32 MaxSeats = 4;

	/** The settings parsed from the URL (host). */
	const FDFMatchSettings& GetMatchSettings() const { return MatchSettings; }

	/** The lowest seat no connected player occupies; 0 if all are taken. */
	int32 FindFreeSeat() const;

	/** Start a new match: reload this map with the same options. False if one is already under way or
	 *  the session or the engine refused it. Once per map: the travel replaces this game mode. */
	bool RestartMatch();

private:
	void HandleRestartRequested(ADFMatchState* Match);

	FDFMatchSettings MatchSettings;
	bool bRestarting = false;
};
