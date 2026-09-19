#include "DFEnemiesModule.h"

#include "Modules/ModuleManager.h"

DEFINE_LOG_CATEGORY_STATIC(LogDFEnemies, Log, All);

void FDFEnemiesModule::StartupModule()
{
	UE_LOG(LogDFEnemies, Log, TEXT("DFEnemies module started"));
}

void FDFEnemiesModule::ShutdownModule()
{
}

IMPLEMENT_MODULE(FDFEnemiesModule, DFEnemies)
