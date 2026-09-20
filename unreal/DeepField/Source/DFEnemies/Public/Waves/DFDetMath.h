#pragma once

#include "CoreMinimal.h"

// Deterministic float helpers, ported from sim/Sim.Core/Math/DetMath.cs. "Deterministic" means one
// specific IEEE-754 single-precision result on every compiler: no fused multiply-add, no
// reassociation, no libm. UBT compiles Editor targets with precise FP semantics, but Game targets
// default to imprecise on Microsoft platforms (/fp:fast), and a host migrating from a Mac to a
// Windows machine must re-plan the same wave — so functions that chain float operations open with
// DF_DET_FP_SCOPE, and files that define them sit between DF_DET_FP_PUSH / _POP.

#if defined(_MSC_VER) && !defined(__clang__)
	#define DF_DET_FP_PUSH __pragma(float_control(precise, on, push)) __pragma(fp_contract(off))
	#define DF_DET_FP_POP __pragma(float_control(pop))
	#define DF_DET_FP_SCOPE
#else
	#define DF_DET_FP_PUSH
	#define DF_DET_FP_POP
	#define DF_DET_FP_SCOPE _Pragma("clang fp contract(off) reassociate(off)")
#endif

DF_DET_FP_PUSH

struct FDFDetMath
{
	/** x^n by squaring with a fixed association order (DetMath.PowInt). Not FMath::Pow: that differs
	 *  in the last ulp for some inputs, and hp scales compound over forty waves of it. */
	static float PowInt(float X, int32 N)
	{
		DF_DET_FP_SCOPE
		if (N < 0)
		{
			return 1.f / PowInt(X, -N);
		}
		float Result = 1.f;
		float Basis = X;
		while (N > 0)
		{
			if ((N & 1) != 0)
			{
				Result *= Basis;
			}
			Basis *= Basis;
			N >>= 1;
		}
		return Result;
	}

	/** MathF.Round's default: nearest, ties to even (2.5 -> 2, 3.5 -> 4). FMath::RoundToInt rounds
	 *  ties up, which puts one more enemy in some co-op waves than the sim does. */
	static float RoundHalfToEven(float X)
	{
		const float Floor = FMath::FloorToFloat(X);
		const float Fraction = X - Floor;   // exact for any float with a fractional part
		if (Fraction > 0.5f)
		{
			return Floor + 1.f;
		}
		if (Fraction < 0.5f)
		{
			return Floor;
		}
		return FMath::Fmod(Floor, 2.f) == 0.f ? Floor : Floor + 1.f;
	}
};

DF_DET_FP_POP
