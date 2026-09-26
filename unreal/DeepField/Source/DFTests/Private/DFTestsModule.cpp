#include "DFTestsModule.h"

#include "Modules/ModuleManager.h"

DEFINE_LOG_CATEGORY_STATIC(LogDFTests, Log, All);

void FDFTestsModule::StartupModule()
{
	UE_LOG(LogDFTests, Log, TEXT("DFTests module started"));
}

void FDFTestsModule::ShutdownModule()
{
}

IMPLEMENT_MODULE(FDFTestsModule, DFTests)
