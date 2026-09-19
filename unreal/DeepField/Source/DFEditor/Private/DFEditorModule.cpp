#include "DFEditorModule.h"

#include "Modules/ModuleManager.h"

DEFINE_LOG_CATEGORY_STATIC(LogDFEditor, Log, All);

void FDFEditorModule::StartupModule()
{
	UE_LOG(LogDFEditor, Log, TEXT("DFEditor module started"));
}

void FDFEditorModule::ShutdownModule()
{
}

IMPLEMENT_MODULE(FDFEditorModule, DFEditor)
