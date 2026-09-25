#include "DFGameMode.h"

#include "DFEventRelay.h"
#include "DFGameplayTags.h"
#include "DFPlayerController.h"
#include "DFPlayerState.h"
#include "Messages/DFMessageBus.h"
#include "Messages/DFMessages.h"
#include "Engine/GameInstance.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"
#include "Kismet/GameplayStatics.h"
#include "Online/DFJoinSeams.h"
#include "Subsystems/GameInstanceSubsystem.h"

DEFINE_LOG_CATEGORY_STATIC(LogDFMatch, Log, All);

ADFGameMode::ADFGameMode()
{
	bUseSeamlessTravel = false;
	GameStateClass = ADFMatchState::StaticClass();
	PlayerStateClass = ADFPlayerState::StaticClass();
	PlayerControllerClass = ADFPlayerController::StaticClass();
}

void ADFGameMode::InitGame(const FString& MapName, const FString& Options, FString& ErrorMessage)
{
	Super::InitGame(MapName, Options, ErrorMessage);
	MatchSettings = FDFMatchSettings();
	MatchSettings.Seed = static_cast<uint32>(FMath::Max(0, UGameplayStatics::GetIntOption(Options, TEXT("seed"), 0)));
	MatchSettings.bLobby = UGameplayStatics::HasOption(Options, TEXT("lobby"));
	MatchSettings.bEndless = UGameplayStatics::HasOption(Options, TEXT("endless"));
	MatchSettings.bWaitForPlayers = IsRunningDedicatedServer() || UGameplayStatics::HasOption(Options, TEXT("waitforplayers"));
	const FString WavesMap = UGameplayStatics::ParseOption(Options, TEXT("wavesmap"));
	if (!WavesMap.IsEmpty())
	{
		MatchSettings.MapId = FName(*WavesMap);
	}
	const FString Intermission = UGameplayStatics::ParseOption(Options, TEXT("intermission"));
	if (!Intermission.IsEmpty())
	{
		MatchSettings.IntermissionSeconds = FMath::Max(0.f, FCString::Atof(*Intermission));
	}
}

void ADFGameMode::InitGameState()
{
	Super::InitGameState();
	if (ADFMatchState* Match = GetGameState<ADFMatchState>())
	{
		Match->ConfigureMatch(MatchSettings);
	}
}

int32 ADFGameMode::FindFreeSeat() const
{
	TSet<int32> Taken;
	if (GameState)
	{
		for (const APlayerState* PlayerState : GameState->PlayerArray)
		{
			if (const ADFPlayerState* Seated = Cast<ADFPlayerState>(PlayerState); Seated && Seated->GetSeat() > 0)
			{
				Taken.Add(Seated->GetSeat());
			}
		}
	}
	// Not yet: holding a disconnected player's seat for their rejoin (World.cs holds seat and faction).
	// AGameModeBase keeps no inactive player states (that is AGameMode's); it arrives with WS-11's
	// rejoin/resume flow, keyed by online id.
	for (int32 Seat = 1; Seat <= MaxSeats; ++Seat)
	{
		if (!Taken.Contains(Seat))
		{
			return Seat;
		}
	}
	return 0;
}

void ADFGameMode::PreLogin(const FString& Options, const FString& Address, const FUniqueNetIdRepl& UniqueId, FString& ErrorMessage)
{
	Super::PreLogin(Options, Address, UniqueId, ErrorMessage);
	if (!ErrorMessage.IsEmpty())
	{
		return;
	}
	// C14 join seams (DFCore/Public/Online/DFJoinSeams.h, ruling R3): DFMatch cannot call DFOnline (a
	// higher layer), so every game-instance subsystem that implements a seam is asked, and the first
	// refusal wins. The handshake (version, content hash, approval, ban) is UDFOnlineSubsystem's;
	// sanctions and anti-cheat have no implementer at Level 1, which is the pass-through.
	UGameInstance* GameInstance = GetGameInstance();
	if (!GameInstance)
	{
		return;
	}
	const FString OnlineId = UGameplayStatics::ParseOption(Options, DFJoinSeams::OptOnlineId);
	for (UGameInstanceSubsystem* Subsystem : GameInstance->GetSubsystemArray<UGameInstanceSubsystem>())
	{
		FString Reason;
		if (IDFJoinValidator* Validator = Cast<IDFJoinValidator>(Subsystem); Validator && !Validator->ValidateJoin(Options, Reason))
		{
			ErrorMessage = Reason.IsEmpty() ? TEXT("joinRefused") : Reason;
			break;
		}
		if (IDFSanctionsCheck* Sanctions = Cast<IDFSanctionsCheck>(Subsystem); Sanctions && !Sanctions->PassesSanctions(OnlineId, Reason))
		{
			ErrorMessage = Reason.IsEmpty() ? TEXT("sanctioned") : Reason;
			break;
		}
		if (IDFAntiCheatCheck* AntiCheat = Cast<IDFAntiCheatCheck>(Subsystem); AntiCheat && !AntiCheat->PassesAntiCheat(OnlineId, Reason))
		{
			ErrorMessage = Reason.IsEmpty() ? TEXT("antiCheat") : Reason;
			break;
		}
	}
	if (!ErrorMessage.IsEmpty())
	{
		UE_LOG(LogDFMatch, Log, TEXT("join refused (%s): %s"), *ErrorMessage, *Address);
	}
}

void ADFGameMode::PostLogin(APlayerController* NewPlayer)
{
	Super::PostLogin(NewPlayer);
	ADFPlayerState* Seated = NewPlayer ? NewPlayer->GetPlayerState<ADFPlayerState>() : nullptr;
	if (Seated && Seated->GetSeat() == 0 && !Seated->IsOnlyASpectator())
	{
		const int32 Seat = FindFreeSeat();
		if (Seat == 0)
		{
			UE_LOG(LogDFMatch, Warning, TEXT("no free seat for %s (%d seats)"), *NewPlayer->GetName(), MaxSeats);
		}
		Seated->SetSeat(Seat);
	}
	FDFMsg_Player Msg;
	Msg.PlayerId = Seated ? Seated->GetSeat() : 0;
	Msg.Name = NewPlayer && NewPlayer->PlayerState ? FName(*NewPlayer->PlayerState->GetPlayerName()) : NAME_None;
	ADFEventRelay::Publish(this, DFTags::Message_PlayerJoined, Msg);
	// The smoke (smoke-listen.sh) counts this line: keep its wording.
	UE_LOG(LogDFMatch, Log, TEXT("player joined: %s (seat %d)"), NewPlayer ? *NewPlayer->GetName() : TEXT("?"), Msg.PlayerId);
}

void ADFGameMode::Logout(AController* Exiting)
{
	FDFMsg_Player Msg;
	const ADFPlayerState* Seated = Exiting ? Exiting->GetPlayerState<ADFPlayerState>() : nullptr;
	Msg.PlayerId = Seated ? Seated->GetSeat() : 0;
	ADFEventRelay::Publish(this, DFTags::Message_PlayerLeft, Msg);
	Super::Logout(Exiting);
}
