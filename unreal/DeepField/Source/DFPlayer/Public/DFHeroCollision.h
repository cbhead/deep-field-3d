#pragma once

#include "CoreMinimal.h"

// The hero capsule's collision profile (C16, DefaultEngine.ini). It belongs beside
// DFCollision::GrayboxProfile() once DFCollision moves to DFCore by contract-append; until then
// DFPlayer spells it here, once.
namespace DFHeroCollision
{
	inline FName Profile() { static const FName Name(TEXT("DF_Hero")); return Name; }
}
