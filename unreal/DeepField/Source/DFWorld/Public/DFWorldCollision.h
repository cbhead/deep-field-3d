#pragma once

#include "CoreMinimal.h"
#include "Engine/EngineTypes.h"

// C16 (CONTRACTS/collision-input.md) names the channels; DefaultEngine.ini binds them to the
// engine's numbered GameTraceChannel slots. Code should say DF_Sight, not GameTraceChannel1,
// so the binding is written down once here and the ini is the only other place that knows it.
// (These belong in DFCore next to the tags; until a contract-append lands there, DFWorld is
// the lowest module that traces.)
namespace DFCollision
{
	// Trace channels
	constexpr ECollisionChannel Sight       = ECC_GameTraceChannel1;
	constexpr ECollisionChannel Weapon      = ECC_GameTraceChannel2;
	constexpr ECollisionChannel Build       = ECC_GameTraceChannel3;
	constexpr ECollisionChannel Interact    = ECC_GameTraceChannel4;
	constexpr ECollisionChannel LaneSurface = ECC_GameTraceChannel5;

	// Object channels
	constexpr ECollisionChannel Hero        = ECC_GameTraceChannel6;
	constexpr ECollisionChannel Enemy       = ECC_GameTraceChannel7;
	constexpr ECollisionChannel Structure   = ECC_GameTraceChannel8;
	constexpr ECollisionChannel Vehicle     = ECC_GameTraceChannel9;
	constexpr ECollisionChannel Prop        = ECC_GameTraceChannel10;
	constexpr ECollisionChannel Pickup      = ECC_GameTraceChannel11;

	// Profiles (DefaultEngine.ini [/Script/Engine.CollisionProfile]); functions so no FName is
	// built during static initialisation.
	inline FName GrayboxProfile()   { static const FName Name(TEXT("DF_Graybox"));   return Name; }
	inline FName StructureProfile() { static const FName Name(TEXT("DF_Structure")); return Name; }
}
