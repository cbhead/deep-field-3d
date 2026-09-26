#include "DFUIModule.h"

#include "Modules/ModuleManager.h"

DEFINE_LOG_CATEGORY_STATIC(LogDFUI, Log, All);

void FDFUIModule::StartupModule()
{
	UE_LOG(LogDFUI, Log, TEXT("DFUI module started"));
}

void FDFUIModule::ShutdownModule()
{
}

IMPLEMENT_MODULE(FDFUIModule, DFUI)
