#include "DFAudioModule.h"

#include "Modules/ModuleManager.h"

DEFINE_LOG_CATEGORY_STATIC(LogDFAudio, Log, All);

void FDFAudioModule::StartupModule()
{
	UE_LOG(LogDFAudio, Log, TEXT("DFAudio module started"));
}

void FDFAudioModule::ShutdownModule()
{
}

IMPLEMENT_MODULE(FDFAudioModule, DFAudio)
