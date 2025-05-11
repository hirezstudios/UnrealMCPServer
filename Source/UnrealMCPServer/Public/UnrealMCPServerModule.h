#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"

// Define a log category
UNREALMCPSERVER_API DECLARE_LOG_CATEGORY_EXTERN(LogUnrealMCPServer, Log, All);

class UNREALMCPSERVER_API FUnrealMCPServerModule : public IModuleInterface
{
public:
    /** IModuleInterface implementation */
    virtual void StartupModule() override;
    virtual void ShutdownModule() override;
private:
	TUniquePtr<class FUMCP_Server> Server;
	TUniquePtr<class FUMCP_CommonTools> CommonTools;
	TUniquePtr<class FUMCP_CommonResources> CommonResources;
};
