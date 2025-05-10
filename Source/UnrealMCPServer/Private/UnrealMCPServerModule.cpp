#include "UnrealMCPServerModule.h"
#include "UMCP_Server.h"
#include "UMCP_CommonTools.h"

// Define the log category
DEFINE_LOG_CATEGORY(LogUnrealMCPServer);

void FUnrealMCPServerModule::StartupModule()
{
	UE_LOG(LogUnrealMCPServer, Warning, TEXT("FUnrealMCPServerModule has started"));
	CommonTools = MakeUnique<FUMCP_CommonTools>();
	Server = MakeUnique<FUMCP_Server>();
	if (Server)
	{
		CommonTools->RegisterTools(Server.Get());
		Server->StartServer();
	}
}

void FUnrealMCPServerModule::ShutdownModule()
{
	if (Server)
	{
		Server->StopServer();
		Server.Reset();
	}
	if (CommonTools)
	{
		CommonTools.Reset();
	}
	UE_LOG(LogUnrealMCPServer, Warning, TEXT("FUnrealMCPServerModule has shut down"));
}

IMPLEMENT_MODULE(FUnrealMCPServerModule, UnrealMCPServer)
