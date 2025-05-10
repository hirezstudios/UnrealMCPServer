#include "HiRezMCPUnreal.h"
#include "MCPServer.h"

// Define the log category
DEFINE_LOG_CATEGORY(LogHiRezMCP);

void FHiRezMCPUnrealModule::StartupModule()
{
	UE_LOG(LogHiRezMCP, Warning, TEXT("HiRezMCPUnrealModule has started"));
	Server = MakeUnique<FMCPServer>();
	if (Server)
	{
		Server->StartServer();
	}
}

void FHiRezMCPUnrealModule::ShutdownModule()
{
	UE_LOG(LogHiRezMCP, Warning, TEXT("HiRezMCPUnrealModule has shut down"));
	if (Server)
	{
		Server->StopServer();
		Server.Reset();
	}
}

IMPLEMENT_MODULE(FHiRezMCPUnrealModule, HiRezMCPUnreal)
