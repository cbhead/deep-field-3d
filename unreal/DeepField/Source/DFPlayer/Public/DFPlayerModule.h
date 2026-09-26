#pragma once

#include "Modules/ModuleManager.h"

DFPLAYER_API DECLARE_LOG_CATEGORY_EXTERN(LogDFPlayer, Log, All);

class FDFPlayerModule : public IModuleInterface
{
public:
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
};
