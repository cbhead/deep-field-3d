#include "DFVehiclesModule.h"

#include "Modules/ModuleManager.h"

DEFINE_LOG_CATEGORY_STATIC(LogDFVehicles, Log, All);

void FDFVehiclesModule::StartupModule()
{
	UE_LOG(LogDFVehicles, Log, TEXT("DFVehicles module started"));
}

void FDFVehiclesModule::ShutdownModule()
{
}

IMPLEMENT_MODULE(FDFVehiclesModule, DFVehicles)
