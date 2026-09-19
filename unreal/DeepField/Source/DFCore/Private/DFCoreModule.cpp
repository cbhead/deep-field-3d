#include "DFCoreModule.h"

#include "Modules/ModuleManager.h"

DEFINE_LOG_CATEGORY_STATIC(LogDFCore, Log, All);

void FDFCoreModule::StartupModule()
{
	UE_LOG(LogDFCore, Log, TEXT("DFCore module started"));
}

void FDFCoreModule::ShutdownModule()
{
}

IMPLEMENT_PRIMARY_GAME_MODULE(FDFCoreModule, DFCore, "DeepField");
