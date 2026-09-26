#include "DFTowersModule.h"

#include "Modules/ModuleManager.h"

DEFINE_LOG_CATEGORY_STATIC(LogDFTowers, Log, All);

void FDFTowersModule::StartupModule()
{
	UE_LOG(LogDFTowers, Log, TEXT("DFTowers module started"));
}

void FDFTowersModule::ShutdownModule()
{
}

IMPLEMENT_MODULE(FDFTowersModule, DFTowers)
