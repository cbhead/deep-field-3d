#pragma once

#include "Modules/ModuleManager.h"

class FDFVfxModule : public IModuleInterface
{
public:
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
};
