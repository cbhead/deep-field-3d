#include "Effects/DFGE_Status_Defense.h"

#include "Attributes/DFHealthSet.h"

UDFGE_Status_Defense::UDFGE_Status_Defense()
{
	ConfigureChannel(EDFStatusChannel::Defense);
	AddMagnitudeModifier(UDFHealthSet::GetFlatArmorAttribute(), EGameplayModOp::AddBase);
}
