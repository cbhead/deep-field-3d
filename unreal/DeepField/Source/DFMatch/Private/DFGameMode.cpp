#include "DFGameMode.h"

#include "DFGameplayTags.h"
#include "Messages/DFMessageBus.h"
#include "Messages/DFMessages.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"

DEFINE_LOG_CATEGORY_STATIC(LogDFMatch, Log, All);

ADFGameMode::ADFGameMode()
{
	bUseSeamlessTravel = false;
}

void ADFGameMode::PreLogin(const FString& Options, const FString& Address, const FUniqueNetIdRepl& UniqueId, FString& ErrorMessage)
{
	// C14: version + content-hash handshake, sanctions and anti-cheat seams land here (WS-11).
	Super::PreLogin(Options, Address, UniqueId, ErrorMessage);
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
