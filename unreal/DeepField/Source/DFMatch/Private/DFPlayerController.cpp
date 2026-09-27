#include "DFPlayerController.h"

#include "DFMatchState.h"
#include "DFPlayerState.h"
#include "Engine/World.h"
#include "DFGameplayTags.h"
#include "Messages/DFMessageBus.h"
#include "Towers/DFBuildSubsystem.h"
#include "Towers/DFTower.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(DFPlayerController)

int32 ADFPlayerController::GetSeat() const
{
	const ADFPlayerState* Seated = GetPlayerState<ADFPlayerState>();
	return Seated ? Seated->GetSeat() : 0;
}

void ADFPlayerController::Server_Launch_Implementation()
{
	// As Step.cs ApplyLaunch: a launch from anyone but the launch seat, or outside the lobby, is
	// silently ignored — the lobby screen only offers the button to the seat that may press it.
	if (ADFMatchState* Match = GetWorld() ? GetWorld()->GetGameState<ADFMatchState>() : nullptr)
	{
		Match->ServerLaunch(GetSeat());
	}
}

void ADFPlayerController::Server_CallEarly_Implementation()
{
	if (ADFMatchState* Match = GetWorld() ? GetWorld()->GetGameState<ADFMatchState>() : nullptr)
	{
		Match->ServerCallEarly(GetSeat());
	}
}

void ADFPlayerController::Server_PlaceTower_Implementation(FName TowerId, FName SocketId)
{
	UDFBuildSubsystem* Build = UDFBuildSubsystem::Get(this);
	if (!Build)
	{
		return;
	}
	// The Forge discount waits on the builder's faction (WS-07's faction component on the player state).
	const FDFBuildResult Result = Build->PlaceTower(GetSeat(), TowerId, SocketId, /*bBuilderIsForge*/ false);
	if (!Result.Succeeded())
	{
		FDFMsg_Rejected Refusal;
		Refusal.PlayerId = GetSeat();
		Refusal.Reason = Result.Reason;
		Refusal.Subject = SocketId;
		Refusal.Detail = TowerId.ToString();
		Client_Refused(DFTags::Message_BuildRejected, Refusal);
	}
}

void ADFPlayerController::Server_UpgradeTower_Implementation(int32 StructureId, int32 PathIndex)
{
	UDFBuildSubsystem* Build = UDFBuildSubsystem::Get(this);
	if (!Build)
	{
		return;
	}
	const FDFBuildResult Result = Build->UpgradeTower(GetSeat(), StructureId, PathIndex);
	if (!Result.Succeeded())
	{
		FDFMsg_Rejected Refusal;
		Refusal.PlayerId = GetSeat();
		Refusal.Reason = Result.Reason;
		Refusal.Subject = Result.Tower ? Result.Tower->GetDefId() : NAME_None;
		Refusal.Detail = FString::FromInt(StructureId);
		Client_Refused(DFTags::Message_UpgradeRejected, Refusal);
	}
}

void ADFPlayerController::Server_SellTower_Implementation(int32 StructureId)
{
	if (UDFBuildSubsystem* Build = UDFBuildSubsystem::Get(this))
	{
		Build->SellTower(GetSeat(), StructureId);   // an unknown tower is ignored without a message (Step.cs)
	}
}

void ADFPlayerController::Client_Refused_Implementation(FGameplayTag Tag, const FDFMsg_Rejected& Payload)
{
	if (UDFMessageBus* Bus = UDFMessageBus::Get(this))
	{
		Bus->Broadcast(Tag, Payload);
	}
}
