#include "Effects/DFGE_Status_Thermal.h"

UDFGE_Status_Thermal::UDFGE_Status_Thermal()
{
	ConfigureChannel(EDFStatusChannel::Thermal);
	ConfigurePeriodicDamage();
}
