#pragma once

#include "Modules/ModuleManager.h"

class FDFGameplayModule : public IModuleInterface
{
public:
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
};
