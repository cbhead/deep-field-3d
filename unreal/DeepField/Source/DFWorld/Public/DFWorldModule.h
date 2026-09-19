#pragma once

#include "Modules/ModuleManager.h"

class FDFWorldModule : public IModuleInterface
{
public:
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
};
