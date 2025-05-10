#include "HiRezMCPUnreal.h"
#include "UMCP_Server.h"

// Define the log category
DEFINE_LOG_CATEGORY(LogHiRezMCP);

void FHiRezMCPUnrealModule::StartupModule()
{
	UE_LOG(LogHiRezMCP, Warning, TEXT("HiRezMCPUnrealModule has started"));
	Server = MakeUnique<FUMCP_Server>();
	if (Server)
	{
		FUMCP_ToolDefinition TestTool;
		TestTool.name = TEXT("TestTool");
		TestTool.description = TEXT("This is a test tool");
		TestTool.DoToolCall = FUMCP_ToolCall::CreateLambda([](TSharedPtr<FJsonObject> arguments, TArray<FUMCP_CallToolResultContent>& OutContent)
		{
			UE_LOG(LogHiRezMCP, Error, TEXT("HELLO WORLD"));
			auto& Content = OutContent.Add_GetRef(FUMCP_CallToolResultContent());
			Content.type = TEXT("text");
			Content.text = TEXT("Hello World, from our MCP!");
			return true;
		});
		Server->RegisterTool(MoveTemp(TestTool));
		
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
