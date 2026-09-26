#include "DFVfxModule.h"

#include "Modules/ModuleManager.h"

DEFINE_LOG_CATEGORY_STATIC(LogDFVfx, Log, All);

void FDFVfxModule::StartupModule()
{
	UE_LOG(LogDFVfx, Log, TEXT("DFVfx module started"));
}

void FDFVfxModule::ShutdownModule()
{
}

IMPLEMENT_MODULE(FDFVfxModule, DFVfx)
