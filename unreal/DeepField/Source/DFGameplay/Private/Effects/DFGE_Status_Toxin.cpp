#include "Effects/DFGE_Status_Toxin.h"

UDFGE_Status_Toxin::UDFGE_Status_Toxin()
{
	ConfigureChannel(EDFStatusChannel::Toxin);
	ConfigurePeriodicDamage();
}
