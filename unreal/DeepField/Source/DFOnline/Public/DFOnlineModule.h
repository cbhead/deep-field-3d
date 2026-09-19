#pragma once

#include "Modules/ModuleManager.h"

class FDFOnlineModule : public IModuleInterface
{
public:
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
};
