#include "DFOnlineModule.h"

#include "Modules/ModuleManager.h"

DEFINE_LOG_CATEGORY_STATIC(LogDFOnline, Log, All);

void FDFOnlineModule::StartupModule()
{
	UE_LOG(LogDFOnline, Log, TEXT("DFOnline module started"));
}

void FDFOnlineModule::ShutdownModule()
{
}

IMPLEMENT_MODULE(FDFOnlineModule, DFOnline)
