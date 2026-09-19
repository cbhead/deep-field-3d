#include "DFJoinCode.h"

#include "Misc/Guid.h"

const TCHAR* FDFJoinCode::Alphabet()
{
	return TEXT("ABCDEFGHJKLMNPQRSTUVWXYZ23456789");
}

FString FDFJoinCode::Generate()
{
	// 256 % 32 == 0, so masking a byte to 5 bits keeps every symbol equally likely.
	const FGuid Guid = FGuid::NewGuid();
	uint8 Bytes[sizeof(FGuid)];
	FMemory::Memcpy(Bytes, &Guid, sizeof(FGuid));
	const TCHAR* Symbols = Alphabet();
	FString Code;
	Code.Reserve(Length);
	for (int32 i = 0; i < Length; ++i)
	{
		Code.AppendChar(Symbols[Bytes[i] & 31]);
	}
	return Code;
}

FString FDFJoinCode::Normalize(const FString& Raw)
{
	FString Out;
	Out.Reserve(Raw.Len());
	for (TCHAR C : Raw)
	{
		if (FChar::IsAlnum(C))
		{
			Out.AppendChar(FChar::ToUpper(C));
		}
	}
	return Out;
}

bool FDFJoinCode::IsWellFormed(const FString& Code)
{
	if (Code.Len() != Length)
	{
		return false;
	}
	for (TCHAR C : Code)
	{
		if (!IsSymbol(C))
		{
			return false;
		}
	}
	return true;
}

bool FDFJoinCode::IsSymbol(TCHAR C)
{
	return C != TEXT('\0') && FCString::Strchr(Alphabet(), C) != nullptr;
}

const FString& FDFJoinCodeRotator::Current(double Now)
{
	if (!IsActive() || IsExpired(Now))
	{
		Rotate(Now);
	}
	return Code;
}

void FDFJoinCodeRotator::Rotate(double Now)
{
	FString Fresh = FDFJoinCode::Generate();
	// A rotation must change the code; the odds of a repeat are 1 in 2^30 but the contract is "rotates".
	while (Fresh == Code)
	{
		Fresh = FDFJoinCode::Generate();
	}
	Code = MoveTemp(Fresh);
	IssuedAt = Now;
	++Generation;
}

bool FDFJoinCodeRotator::Matches(const FString& Candidate, double Now) const
{
	if (!IsActive() || IsExpired(Now))
	{
		return false;
	}
	return FDFJoinCode::Normalize(Candidate) == Code;
}

bool FDFJoinCodeRotator::IsExpired(double Now) const
{
	return LifetimeSeconds > 0.0 && (Now - IssuedAt) >= LifetimeSeconds;
}

void FDFJoinCodeRotator::Reset()
{
	Code.Reset();
	IssuedAt = 0.0;
	Generation = 0;
}
