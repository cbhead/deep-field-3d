#pragma once

#include "Modules/ModuleManager.h"

DFCONTENTPIPELINE_API DECLARE_LOG_CATEGORY_EXTERN(LogDFContentPipeline, Log, All);

class FDFContentPipelineModule : public IModuleInterface
{
public:
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
};
