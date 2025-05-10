// Copyright Epic Games, Inc. All Rights Reserved.

#include "HiRezMCPUnreal.h"
#include "MCPTypes.h"

#include "HttpServerModule.h"
#include "IHttpRouter.h"
#include "HttpServerResponse.h"
#include "JsonUtilities.h"
#include "Json.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"
#include "Engine/Engine.h" // For FEngineVersion

#define LOCTEXT_NAMESPACE "FHiRezMCPUnrealModule"

// Define the log category
DEFINE_LOG_CATEGORY(LogHiRezMCP);

// Define static constants
const FString FHiRezMCPUnrealModule::MCP_PROTOCOL_VERSION = TEXT("2025-03-26");
const FString FHiRezMCPUnrealModule::PLUGIN_VERSION = TEXT("0.1.0");

void FHiRezMCPUnrealModule::StartupModule()
{
	UE_LOG(LogHiRezMCP, Warning, TEXT("HiRezMCPUnrealModule has started"));
	StartHttpServer();
}

void FHiRezMCPUnrealModule::ShutdownModule()
{
	UE_LOG(LogHiRezMCP, Warning, TEXT("HiRezMCPUnrealModule has shut down"));
	StopHttpServer();
}

void FHiRezMCPUnrealModule::StartHttpServer()
{
	FHttpServerModule& HttpServerModule = FHttpServerModule::Get();
	HttpRouter = HttpServerModule.GetHttpRouter(HttpServerPort);

	if (!HttpRouter.IsValid())
	{
		UE_LOG(LogHiRezMCP, Error, TEXT("Failed to get HttpRouter on port %d. Another server might be running or port is in use."), HttpServerPort);
		return;
	}

	// Register routes
	// Keeping existing non-MCP routes for now, they can be reviewed/removed later.
	HealthCheckRouteHandle = HttpRouter->BindRoute(FHttpPath(TEXT("/health")), EHttpServerRequestVerbs::VERB_GET, 
		[](const FHttpServerRequest& Request, const FHttpResultCallback& OnComplete) -> bool {
			// FHiRezMCPUnrealModule::HandleHealthCheck(Request, OnComplete); // If it were static
			// For now, a simple inline response for health check if it's to be kept
			TUniquePtr<FHttpServerResponse> Response = FHttpServerResponse::Create(TEXT("Healthy"), TEXT("text/plain"));
			Response->Code = EHttpServerResponseCodes::Ok;
			OnComplete(MoveTemp(Response));
			return true;
		});

	// Bind the main MCP route to the non-static member function
	MCPGeneralRouteHandle = HttpRouter->BindRoute(FHttpPath(TEXT("/mcp")), EHttpServerRequestVerbs::VERB_POST, 
		[this](const FHttpServerRequest& Request, const FHttpResultCallback& OnComplete) -> bool {
			this->HandleGeneralMCPRequest(Request, OnComplete);
			return true;
		});

	UE_LOG(LogHiRezMCP, Log, TEXT("Bound /health, and /mcp to handlers."));

    // Start listening for requests
    HttpServerModule.StartAllListeners();
	UE_LOG(LogHiRezMCP, Log, TEXT("HTTP Server started on port %d"), HttpServerPort);
}

void FHiRezMCPUnrealModule::StopHttpServer()
{
	if (HttpRouter.IsValid())
	{
		// Unbind routes
		if (HealthCheckRouteHandle.IsValid()) HttpRouter->UnbindRoute(HealthCheckRouteHandle);
		if (MCPGeneralRouteHandle.IsValid()) HttpRouter->UnbindRoute(MCPGeneralRouteHandle);

		UE_LOG(LogHiRezMCP, Log, TEXT("All routes unbound."));
		HttpRouter.Reset();
	}

	// let the HttpServerModule clean itself up whenever it goes away.  Nothing to do here
}

// Helper to convert FJsonRpcResponse to JSON string
bool FHiRezMCPUnrealModule::ConvertRpcResponseToJsonString(const FJsonRpcResponse& RpcResponse, FString& OutJsonString)
{
    TSharedPtr<FJsonObject> JsonObject = FJsonObjectConverter::UStructToJsonObject(RpcResponse);
    if (JsonObject.IsValid())
    {
        return FJsonSerializer::Serialize(JsonObject.ToSharedRef(), TJsonWriterFactory<>::Create(&OutJsonString));
    }
    return false;
}

// Helper to convert FJsonRpcErrorObject and RequestId to JSON string (full JSON-RPC error response)
bool FHiRezMCPUnrealModule::ConvertRpcErrorToJsonString(const FJsonRpcErrorObject& RpcError, const FString& RequestId, FString& OutJsonString)
{
    FJsonRpcResponse ErrorResponse;
    ErrorResponse.id = RequestId;
    // Need to use MakeShared to assign to TSharedPtr for USTRUCTs within USTRUCTs if they are to be serialized properly by FJsonObjectConverter
    ErrorResponse.error = MakeShared<FJsonRpcErrorObject>(RpcError); 

    TSharedPtr<FJsonObject> JsonObject = FJsonObjectConverter::UStructToJsonObject(ErrorResponse);
    if (JsonObject.IsValid())
    {
        return FJsonSerializer::Serialize(JsonObject.ToSharedRef(), TJsonWriterFactory<>::Create(&OutJsonString));
    }
    return false;
}

// Helper to send a JSON response
void FHiRezMCPUnrealModule::SendJsonResponse(const FHttpResultCallback& OnComplete, const FString& JsonPayload, bool bSuccess)
{
    TUniquePtr<FHttpServerResponse> Response = FHttpServerResponse::Create(JsonPayload, TEXT("application/json"));
    Response->Code = bSuccess ? EHttpServerResponseCodes::Ok : EHttpServerResponseCodes::BadRequest;
    OnComplete(MoveTemp(Response));
}

// Helper to send a simple JSON-RPC error response
void FHiRezMCPUnrealModule::SendSimpleErrorResponse(const FHttpResultCallback& OnComplete, const FString& RequestId, int32 ErrorCode, const FString& ErrorMessage)
{
    FJsonRpcErrorObject RpcError;
    RpcError.code = ErrorCode;
    RpcError.message = ErrorMessage;

    FString ErrorJsonString;
    if (ConvertRpcErrorToJsonString(RpcError, RequestId, ErrorJsonString))
    {
        SendJsonResponse(OnComplete, ErrorJsonString, false);
    }
    else
    {
        // Fallback if error serialization fails (should not happen ideally)
        TUniquePtr<FHttpServerResponse> FallbackResponse = FHttpServerResponse::Create(TEXT("{\"jsonrpc\": \"2.0\", \"error\": {\"code\": -32000, \"message\": \"Server error during error serialization\"}, \"id\": null}"), TEXT("application/json"));
        FallbackResponse->Code = EHttpServerResponseCodes::ServerError;
        OnComplete(MoveTemp(FallbackResponse));
    }
}

void FHiRezMCPUnrealModule::HandleGeneralMCPRequest(const FHttpServerRequest& Request, const FHttpResultCallback& OnComplete)
{
	FUTF8ToTCHAR Convert((ANSICHAR*)Request.Body.GetData(), Request.Body.Num());
	FString RequestBody(Convert.Length(), Convert.Get());
	
    //FString RequestBody = Request.GetBodyAsString();
    UE_LOG(LogHiRezMCP, Verbose, TEXT("Received MCP request: %s"), *RequestBody);

    FJsonRpcRequest RpcRequest;
    if (!FJsonObjectConverter::JsonObjectStringToUStruct(RequestBody, &RpcRequest, 0, 0))
    {
        UE_LOG(LogHiRezMCP, Error, TEXT("Failed to parse JSON-RPC request: %s"), *RequestBody);
        SendSimpleErrorResponse(OnComplete, TEXT(""), -32700, TEXT("Parse error")); // ID might be unknown here
        return;
    }

    if (RpcRequest.jsonrpc != TEXT("2.0"))
    {
        UE_LOG(LogHiRezMCP, Error, TEXT("Invalid JSON-RPC version: %s"), *RpcRequest.jsonrpc);
        SendSimpleErrorResponse(OnComplete, RpcRequest.id, -32600, TEXT("Invalid Request - JSON-RPC version must be 2.0"));
        return;
    }

    // Method dispatching
    if (RpcRequest.method == TEXT("initialize"))
    {
        FInitializeParams InitParams;
        if (RpcRequest.params.IsValid() && FJsonObjectConverter::JsonObjectToUStruct(RpcRequest.params.ToSharedRef(), &InitParams, 0, 0))
        {
            UE_LOG(LogHiRezMCP, Log, TEXT("Handling 'initialize' method. Client protocol version: %s"), *InitParams.protocolVersion);

            FInitializeResult InitResult;
            InitResult.protocolVersion = MCP_PROTOCOL_VERSION; // Server's supported version
            
            // Populate ServerInfo
            InitResult.serverInfo->name = TEXT("HiRezMCPUnreal");
            InitResult.serverInfo->version = PLUGIN_VERSION;
            InitResult.serverInfo->HirezMCPUnreal_version = PLUGIN_VERSION;
            if (GEngine)
            {
                InitResult.serverInfo->unreal_engine_version = FEngineVersion::Current().ToString(EVersionComponent::Patch);
            }
            else
            {
                InitResult.serverInfo->unreal_engine_version = TEXT("Unknown");
            }

            // Populate ServerCapabilities (defaults are fine for now as defined in MCPTypes.h constructors)
            // Note: listChanged and subscribe capabilities are false by default due to no SSE.

            FJsonRpcResponse RpcResponse;
            RpcResponse.id = RpcRequest.id;
            RpcResponse.result = FJsonObjectConverter::UStructToJsonObject(InitResult);

            FString ResponseJsonString;
            if (ConvertRpcResponseToJsonString(RpcResponse, ResponseJsonString))
            {
                SendJsonResponse(OnComplete, ResponseJsonString, true);
            }
            else
            {
                UE_LOG(LogHiRezMCP, Error, TEXT("Failed to serialize 'initialize' response."));
                SendSimpleErrorResponse(OnComplete, RpcRequest.id, -32603, TEXT("Internal error - Failed to serialize response"));
            }
        }
        else
        {
            UE_LOG(LogHiRezMCP, Error, TEXT("Failed to parse 'initialize' params."));
            SendSimpleErrorResponse(OnComplete, RpcRequest.id, -32602, TEXT("Invalid params - Could not parse initialize parameters"));
        }
    }
    else if (RpcRequest.method == TEXT("notifications/initialized"))
    {
        // Client confirms initialization
        UE_LOG(LogHiRezMCP, Log, TEXT("Received 'notifications/initialized'. Session active."));
        // For synchronous HTTP, a simple OK or No Content is fine.
        // No actual JSON-RPC response is strictly needed for a notification, but clients might expect a valid HTTP response.
        TUniquePtr<FHttpServerResponse> Response = FHttpServerResponse::Create(TEXT(""), TEXT("application/json")); // Empty body
        Response->Code = EHttpServerResponseCodes::Ok; // Or No Content (204)
        OnComplete(MoveTemp(Response));
    }
    else if (RpcRequest.method == TEXT("ping"))
    {
         UE_LOG(LogHiRezMCP, Log, TEXT("Handling 'ping' method."));
        FJsonRpcResponse RpcResponse;
        RpcResponse.id = RpcRequest.id;
        // Per MCP spec, ping result should be an empty JSON object {}
        RpcResponse.result = MakeShared<FJsonObject>(); 

        FString ResponseJsonString;
        if (ConvertRpcResponseToJsonString(RpcResponse, ResponseJsonString))
        {
            SendJsonResponse(OnComplete, ResponseJsonString, true);
        }
        else
        {
            UE_LOG(LogHiRezMCP, Error, TEXT("Failed to serialize 'ping' response."));
            SendSimpleErrorResponse(OnComplete, RpcRequest.id, -32603, TEXT("Internal error - Failed to serialize ping response"));
        }
    }
    else
    {
        UE_LOG(LogHiRezMCP, Warning, TEXT("Unknown MCP method received: %s"), *RpcRequest.method);
        SendSimpleErrorResponse(OnComplete, RpcRequest.id, -32601, TEXT("Method not found"));
    }
}

#undef LOCTEXT_NAMESPACE
    
IMPLEMENT_MODULE(FHiRezMCPUnrealModule, HiRezMCPUnreal)
