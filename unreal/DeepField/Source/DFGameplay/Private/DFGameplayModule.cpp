#include "DFGameplayModule.h"

#include "Modules/ModuleManager.h"

DEFINE_LOG_CATEGORY_STATIC(LogDFGameplay, Log, All);

void FDFGameplayModule::StartupModule()
{
	UE_LOG(LogDFGameplay, Log, TEXT("DFGameplay module started"));
}

void FDFGameplayModule::ShutdownModule()
{
}

IMPLEMENT_MODULE(FDFGameplayModule, DFGameplay)
