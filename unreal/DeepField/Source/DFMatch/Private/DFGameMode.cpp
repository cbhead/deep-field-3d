#include "DFGameMode.h"

#include "DFGameplayTags.h"
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
	if (UDFMessageBus* Bus = UDFMessageBus::Get(this))
	{
		FDFMsg_Player Msg;
		Msg.PlayerId = NewPlayer && NewPlayer->PlayerState ? NewPlayer->PlayerState->GetPlayerId() : 0;
		Msg.Name = NewPlayer && NewPlayer->PlayerState ? FName(*NewPlayer->PlayerState->GetPlayerName()) : NAME_None;
		Bus->Broadcast(DFTags::Message_PlayerJoined, Msg);
	}
	UE_LOG(LogDFMatch, Log, TEXT("player joined: %s"), NewPlayer ? *NewPlayer->GetName() : TEXT("?"));
}

void ADFGameMode::Logout(AController* Exiting)
{
	if (UDFMessageBus* Bus = UDFMessageBus::Get(this))
	{
		FDFMsg_Player Msg;
		Msg.PlayerId = Exiting && Exiting->PlayerState ? Exiting->PlayerState->GetPlayerId() : 0;
		Bus->Broadcast(DFTags::Message_PlayerLeft, Msg);
	}
	Super::Logout(Exiting);
}
