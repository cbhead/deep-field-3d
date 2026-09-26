#include "DFOnlineTypes.h"

UE::Online::FAccountId FDFOnlineId::ToAccountId() const
{
	if (!IsValid())
	{
		return UE::Online::FAccountId();
	}
	// The registry of the minting provider turns the string back into a handle; a provider that
	// is not loaded in this process yields an invalid id, which every caller treats as "nobody".
	return UE::Online::FOnlineIdRegistryRegistry::Get().ToAccountId(static_cast<UE::Online::EOnlineServices>(Services), Id);
}

FDFOnlineId FDFOnlineId::FromAccountId(const UE::Online::FAccountId& AccountId)
{
	FDFOnlineId Result;
	if (AccountId.IsValid())
	{
		Result.Id = UE::Online::FOnlineIdRegistryRegistry::Get().ToString(AccountId);
		Result.Services = static_cast<uint8>(AccountId.GetOnlineServicesType());
	}
	return Result;
}
