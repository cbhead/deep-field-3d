#include "DFMatchModule.h"

#include "Modules/ModuleManager.h"

DEFINE_LOG_CATEGORY_STATIC(LogDFMatch, Log, All);

void FDFMatchModule::StartupModule()
{
	UE_LOG(LogDFMatch, Log, TEXT("DFMatch module started"));
}

void FDFMatchModule::ShutdownModule()
{
}

IMPLEMENT_MODULE(FDFMatchModule, DFMatch)
