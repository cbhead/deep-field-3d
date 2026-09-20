#pragma once

#include "CoreMinimal.h"
#include "Waves/DFDetMath.h"   // DF_DET_FP_*

// Deterministic randomness, ported bit for bit from sim/Sim.Core/Util/Rng.cs. Gameplay that must
// be a pure function of (seed, stream, index) draws from here and never from FMath::Rand or
// FRandomStream: wave N's content is identical whatever waves 1..N-1 drew, which is what resume
// under a new host, join-in-progress and the balance sweeps rely on (B§1.11).
// Header-only and free of DF dependencies so it can move to DFCore when a second module needs it.

DF_DET_FP_PUSH

/** mulberry32. One stream; obtain it from FDFRngStreams::StreamFor. */
struct FDFDetRng
{
	explicit FDFDetRng(uint32 Seed) : State(Seed) {}

	uint32 NextUInt()
	{
		State += 0x6D2B79F5u;
		uint32 T = State;
		T = (T ^ (T >> 15)) * (T | 1u);
		T ^= T + ((T ^ (T >> 7)) * (T | 61u));
		return T ^ (T >> 14);
	}

	/** Uniform in [0, 1). */
	double NextDouble()
	{
		DF_DET_FP_SCOPE
		return static_cast<double>(NextUInt()) / 4294967296.0;   // a division, never a multiply by the reciprocal
	}

	/** NextDouble narrowed to float, as the sim does; a draw near 1 can round up to 1.0f there too. */
	float NextFloat()
	{
		DF_DET_FP_SCOPE
		return static_cast<float>(NextDouble());
	}

	/** Uniform integer in [MinInclusive, MaxExclusive). Modulo bias included — it is the sim's. */
	int32 NextInt(int32 MinInclusive, int32 MaxExclusive)
	{
		check(MaxExclusive > MinInclusive);
		return MinInclusive + static_cast<int32>(NextUInt() % static_cast<uint32>(MaxExclusive - MinInclusive));
	}

private:
	uint32 State;
};

/** Streams are isolated by purpose: FNV-1a over seed, stream name and index seeds the generator. */
struct FDFRngStreams
{
	static constexpr const TCHAR* Wave = TEXT("wave");
	static constexpr const TCHAR* Spawn = TEXT("spawn");
	static constexpr const TCHAR* Combat = TEXT("combat");
	/** Conditions draw here and nowhere else, so turning one on cannot shift an authored spawn. */
	static constexpr const TCHAR* Condition = TEXT("condition");

	static FDFDetRng StreamFor(uint32 Seed, const TCHAR* Stream, uint32 Index = 0)
	{
		uint32 H = 2166136261u;
		auto MixUInt = [&H](uint32 V)
		{
			for (int32 I = 0; I < 4; ++I)
			{
				H ^= (V >> (I * 8)) & 0xFFu;
				H *= 16777619u;
			}
		};

		MixUInt(Seed);
		for (const TCHAR* C = Stream; *C; ++C)
		{
			H ^= static_cast<uint32>(static_cast<uint16>(*C));   // the sim mixes whole UTF-16 units
			H *= 16777619u;
		}
		MixUInt(Index);
		return FDFDetRng(H);
	}
};

DF_DET_FP_POP
