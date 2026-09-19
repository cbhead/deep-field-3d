#pragma once

#include "Modules/ModuleManager.h"

class FDFPlayerModule : public IModuleInterface
{
public:
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
};
