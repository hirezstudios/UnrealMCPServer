
#pragma once

#include "CoreMinimal.h"
#include "HttpServerRequest.h"
#include "HttpServerResponse.h"
#include "IHttpRouter.h"
#include "UMCP_Types.h"

// Forward declarations for JSON types (used in helpers)
struct FUMCP_JsonRpcResponse;
struct FUMCP_JsonRpcError;
struct FUMCP_JsonRpcId;
using UMCP_JsonRpcHandler = TFunction<bool(const FUMCP_JsonRpcRequest& Request, TSharedPtr<FJsonObject> OutSuccess, FUMCP_JsonRpcError& OutError)>;

class UNREALMCPSERVER_API FUMCP_Server
{
public:
	void StartServer();
	void StopServer();

	// Method handlers should return true for success/error (indicating which object to use in the JSON RPC response)
	void RegisterRpcMethodHandler(const FString& MethodName, UMCP_JsonRpcHandler&& Handler);
	bool RegisterTool(FUMCP_ToolDefinition Tool);
private:
    void HandleStreamableHTTPMCPRequest(const FHttpServerRequest& Request, const FHttpResultCallback& OnComplete);

    static const FString MCP_PROTOCOL_VERSION;
    static const FString PLUGIN_VERSION;
	
    // Helper methods for sending responses
    static void SendJsonRpcResponse(const FHttpResultCallback& OnComplete, const FUMCP_JsonRpcResponse& Response);

	void RegisterInternalRpcMethodHandlers();
	bool Rpc_Initialize(const FUMCP_JsonRpcRequest& Request, TSharedPtr<FJsonObject> OutSuccess, FUMCP_JsonRpcError& OutError);
	bool Rpc_Ping(const FUMCP_JsonRpcRequest& Request, TSharedPtr<FJsonObject> OutSuccess, FUMCP_JsonRpcError& OutError);
	bool Rpc_ClientNotifyInitialized(const FUMCP_JsonRpcRequest& Request, TSharedPtr<FJsonObject> OutSuccess, FUMCP_JsonRpcError& OutError);
	bool Rpc_ToolsList(const FUMCP_JsonRpcRequest& Request, TSharedPtr<FJsonObject> OutSuccess, FUMCP_JsonRpcError& OutError);
	bool Rpc_ToolsCall(const FUMCP_JsonRpcRequest& Request, TSharedPtr<FJsonObject> OutSuccess, FUMCP_JsonRpcError& OutError);
    
    TSharedPtr<IHttpRouter> HttpRouter;
    uint32 HttpServerPort = 30069;
	TMap<FString, UMCP_JsonRpcHandler> JsonRpcMethodHandlers;
    FHttpRouteHandle RouteHandle_MCPStreamableHTTP;
	TMap<FString, FUMCP_ToolDefinition> Tools;
};
