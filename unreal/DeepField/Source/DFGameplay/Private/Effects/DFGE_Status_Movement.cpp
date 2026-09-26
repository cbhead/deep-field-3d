#include "Effects/DFGE_Status_Movement.h"

#include "Attributes/DFMovementSet.h"

UDFGE_Status_Movement::UDFGE_Status_Movement()
{
	ConfigureChannel(EDFStatusChannel::Movement);
	AddMagnitudeModifier(UDFMovementSet::GetSpeedFactorAttribute(), EGameplayModOp::MultiplyCompound);
}
