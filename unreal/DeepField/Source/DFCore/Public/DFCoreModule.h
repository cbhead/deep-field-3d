#pragma once

#include "Modules/ModuleManager.h"

class FDFCoreModule : public IModuleInterface
{
public:
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
};
