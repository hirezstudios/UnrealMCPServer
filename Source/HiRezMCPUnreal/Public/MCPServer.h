
#pragma once

#include "CoreMinimal.h"
#include "HttpServerRequest.h"
#include "HttpServerResponse.h"
#include "IHttpRouter.h"
#include "MCPTypes.h"

// Forward declarations for JSON types (used in helpers)
struct FJsonRpcResponse;
struct FJsonRpcErrorObject;
struct FJsonRpcId;
using JsonRpcHandler = TFunction<bool(const FJsonRpcRequest& Request, TSharedPtr<FJsonObject> OutSuccess, FJsonRpcErrorObject& OutError)>;

class HIREZMCPUNREAL_API FMCPServer
{
public:
	void StartServer();
	void StopServer();

	// Method handlers should return true for success/error (indicating which object to use in the JSON RPC response)
	void RegisterRpcMethodHandler(const FString& MethodName, JsonRpcHandler&& Handler);
private:
    void HandleStreamableHTTPMCPRequest(const FHttpServerRequest& Request, const FHttpResultCallback& OnComplete);

    static const FString MCP_PROTOCOL_VERSION;
    static const FString PLUGIN_VERSION;
	
    // Helper methods for sending responses
    static bool ConvertRpcResponseToJsonString(const FJsonRpcResponse& RpcResponse, FString& OutJsonString);
    static bool ConvertRpcErrorToJsonString(const FJsonRpcErrorObject& RpcError, const FJsonRpcId& RequestId, FString& OutJsonString);
    static void SendJsonRpcResponse(const FHttpResultCallback& OnComplete, const FJsonRpcResponse& Response);

	void RegisterInternalRpcMethodHandlers();
	bool Rpc_Initialize(const FJsonRpcRequest& Request, TSharedPtr<FJsonObject> OutSuccess, FJsonRpcErrorObject& OutError);
	bool Rpc_Ping(const FJsonRpcRequest& Request, TSharedPtr<FJsonObject> OutSuccess, FJsonRpcErrorObject& OutError);
	bool Rpc_ClientNotifyInitialized(const FJsonRpcRequest& Request, TSharedPtr<FJsonObject> OutSuccess, FJsonRpcErrorObject& OutError);
    
    TSharedPtr<IHttpRouter> HttpRouter;
    uint32 HttpServerPort = 30069;
	TMap<FString, JsonRpcHandler> JsonRpcMethodHandlers;
    FHttpRouteHandle RouteHandle_MCPStreamableHTTP;
};
