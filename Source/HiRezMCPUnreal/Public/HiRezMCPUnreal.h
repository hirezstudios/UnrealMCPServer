// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"
#include "HttpServerRequest.h"
#include "HttpServerResponse.h"
#include "IHttpRouter.h"

// Forward declarations for JSON types (used in helpers)
struct FJsonRpcResponse;
struct FJsonRpcErrorObject;

// Define a log category
HIREZMCPUNREAL_API DECLARE_LOG_CATEGORY_EXTERN(LogHiRezMCP, Log, All);

class HIREZMCPUNREAL_API FHiRezMCPUnrealModule : public IModuleInterface
{
public:
    /** IModuleInterface implementation */
    virtual void StartupModule() override;
    virtual void ShutdownModule() override;

    /** MCP HTTP Server */
    void StartHttpServer();
    void StopHttpServer();

    /** MCP Request Handler */
    void HandleGeneralMCPRequest(const FHttpServerRequest& Request, const FHttpResultCallback& OnComplete);
    
    // Supported MCP Protocol Version
    static const FString MCP_PROTOCOL_VERSION;
    static const FString PLUGIN_VERSION;


private:
    // Helper methods for sending responses
    static bool ConvertRpcResponseToJsonString(const FJsonRpcResponse& RpcResponse, FString& OutJsonString);
    static bool ConvertRpcErrorToJsonString(const FJsonRpcErrorObject& RpcError, const FString& RequestId, FString& OutJsonString);
    static void SendJsonResponse(const FHttpResultCallback& OnComplete, const FString& JsonPayload, bool bSuccess = true);

    TSharedPtr<IHttpRouter> HttpRouter;
    uint32 HttpServerPort = 30069; // Default MCP port for this plugin

    // Route handles to unbind them on shutdown
    FHttpRouteHandle HealthCheckRouteHandle;
    FHttpRouteHandle MCPGeneralRouteHandle;
};
