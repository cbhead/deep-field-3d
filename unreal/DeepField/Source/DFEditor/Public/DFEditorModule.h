#pragma once

#include "Modules/ModuleManager.h"

class FDFEditorModule : public IModuleInterface
{
public:
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
};
