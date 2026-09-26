#include "DFContentPipelineModule.h"

#include "Modules/ModuleManager.h"

DEFINE_LOG_CATEGORY(LogDFContentPipeline);

void FDFContentPipelineModule::StartupModule()
{
	UE_LOG(LogDFContentPipeline, Log, TEXT("DFContentPipeline module started"));
}

void FDFContentPipelineModule::ShutdownModule()
{
}

IMPLEMENT_MODULE(FDFContentPipelineModule, DFContentPipeline)
