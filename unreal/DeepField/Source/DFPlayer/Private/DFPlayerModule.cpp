#include "DFPlayerModule.h"

#include "Modules/ModuleManager.h"

DEFINE_LOG_CATEGORY_STATIC(LogDFPlayer, Log, All);

void FDFPlayerModule::StartupModule()
{
	UE_LOG(LogDFPlayer, Log, TEXT("DFPlayer module started"));
}

void FDFPlayerModule::ShutdownModule()
{
}

IMPLEMENT_MODULE(FDFPlayerModule, DFPlayer)
