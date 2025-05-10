#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"

// Define a log category
HIREZMCPUNREAL_API DECLARE_LOG_CATEGORY_EXTERN(LogHiRezMCP, Log, All);

class HIREZMCPUNREAL_API FHiRezMCPUnrealModule : public IModuleInterface
{
public:
    /** IModuleInterface implementation */
    virtual void StartupModule() override;
    virtual void ShutdownModule() override;
private:
	TUniquePtr<class FMCPServer> Server;
};
