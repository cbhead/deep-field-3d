#include "DFWorldModule.h"

#include "Modules/ModuleManager.h"

DEFINE_LOG_CATEGORY_STATIC(LogDFWorld, Log, All);

void FDFWorldModule::StartupModule()
{
	UE_LOG(LogDFWorld, Log, TEXT("DFWorld module started"));
}

void FDFWorldModule::ShutdownModule()
{
}

IMPLEMENT_MODULE(FDFWorldModule, DFWorld)
