#pragma once

#include "CoreMinimal.h"

// The join code (C14): a short human-typeable token the host reads out. It identifies the lobby
// for a FindLobbies search (a searchable lobby attribute), never a player, and it never grants
// entry on its own -- the host approves every code-holder (DF.Message.JoinRequest). Codes rotate
// so a code that was shared once does not keep working: after every approval and on a timer.

struct DFONLINE_API FDFJoinCode
{
	static constexpr int32 Length = 6;

	/** 32 symbols without the look-alikes 0/O and 1/I, so a code survives being read aloud. */
	static const TCHAR* Alphabet();

	/** A fresh random code (bias-free: 5 bits per symbol from a GUID). */
	static FString Generate();

	/** Upper-cases and strips separators so "ab-cd ef" matches "ABCDEF". */
	static FString Normalize(const FString& Raw);

	/** Length and alphabet only; says nothing about whether any lobby uses it. */
	static bool IsWellFormed(const FString& Code);

	/** True if C is one of the alphabet's symbols. */
	static bool IsSymbol(TCHAR C);
};

/** The host's live code: rotates on demand (each approval) and when its lifetime runs out. */
struct DFONLINE_API FDFJoinCodeRotator
{
	/** How long one code stays valid without any approval; 0 disables the timer. */
	double LifetimeSeconds = 600.0;

	/** The current code, rotating first if it has expired. Empty until Rotate() is called once. */
	const FString& Current(double Now);

	/** Issue a new code now. */
	void Rotate(double Now);

	/** True if Candidate (normalised) is the live, unexpired code. */
	bool Matches(const FString& Candidate, double Now) const;

	bool IsActive() const { return !Code.IsEmpty(); }
	bool IsExpired(double Now) const;
	int32 GetGeneration() const { return Generation; }
	void Reset();

private:
	FString Code;
	double IssuedAt = 0.0;
	int32 Generation = 0;
};
