#pragma once

#include "Modules/ModuleManager.h"

class FDFTowersModule : public IModuleInterface
{
public:
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
};
