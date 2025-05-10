#include "MCPServer.h"
#include "MCPTypes.h"
#include "HiRezMCPUnreal.h"

#include "HttpServerModule.h"
#include "IHttpRouter.h"
#include "HttpServerResponse.h"
#include "JsonUtilities.h"
#include "Json.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"
#include "Engine/Engine.h" // For FEngineVersion

const FString FMCPServer::MCP_PROTOCOL_VERSION = TEXT("2024-11-05");//TEXT("2025-03-26");
const FString FMCPServer::PLUGIN_VERSION = TEXT("0.1.0");

void FMCPServer::StartServer()
{
	FHttpServerModule& HttpServerModule = FHttpServerModule::Get();
	HttpRouter = HttpServerModule.GetHttpRouter(HttpServerPort);
	if (!HttpRouter.IsValid())
	{
		UE_LOG(LogHiRezMCP, Error, TEXT("Failed to get HttpRouter on port %d. Another server might be running or port is in use."), HttpServerPort);
		return;
	}

	RouteHandle_MCPStreamableHTTP = HttpRouter->BindRoute(FHttpPath(TEXT("/mcp")), EHttpServerRequestVerbs::VERB_POST, 
		[this](const FHttpServerRequest& Request, const FHttpResultCallback& OnComplete) -> bool {
			this->HandleStreamableHTTPMCPRequest(Request, OnComplete);
			return true;
		});
	RegisterInternalRpcMethodHandlers();

	UE_LOG(LogHiRezMCP, Log, TEXT("Bound /mcp to handler."));

    // Start listening for requests
    HttpServerModule.StartAllListeners();
	UE_LOG(LogHiRezMCP, Log, TEXT("HTTP Server started on port %d"), HttpServerPort);
}

void FMCPServer::StopServer()
{
	if (HttpRouter.IsValid())
	{
		// Unbind routes
		if (RouteHandle_MCPStreamableHTTP.IsValid())
		{
			HttpRouter->UnbindRoute(RouteHandle_MCPStreamableHTTP);
			RouteHandle_MCPStreamableHTTP.Reset();
		}

		UE_LOG(LogHiRezMCP, Log, TEXT("All routes unbound."));
		HttpRouter.Reset();
	}
	JsonRpcMethodHandlers.Empty();

	// let the HttpServerModule clean itself up whenever it goes away.  Nothing to do here
}

void FMCPServer::RegisterRpcMethodHandler(const FString& MethodName, JsonRpcHandler&& Handler)
{
	JsonRpcMethodHandlers.Add(MethodName, Handler);
}

// Helper to send a JSON response
void FMCPServer::SendJsonRpcResponse(const FHttpResultCallback& OnComplete, const FJsonRpcResponse& RpcResponse)
{
	FString JsonPayload;
	if (!RpcResponse.ToJsonString(JsonPayload))
	{
		UE_LOG(LogHiRezMCP, Error, TEXT("Failed to serialize response."));
		JsonPayload = TEXT("{\"jsonrpc\": \"2.0\", \"id\": null, \"error\": {\"code\": -32603, \"message\": \"Internal error - Failed to serialize response\"}}");
	}

	if (JsonPayload.Len() > 1000)
	{
		UE_LOG(LogHiRezMCP, Verbose, TEXT("SendJsonResponse: Payload received (truncated): %s"), *JsonPayload.Left(1000));
	}
	else
	{
		UE_LOG(LogHiRezMCP, Verbose, TEXT("SendJsonResponse: Payload received: %s"), *JsonPayload);
	}
    TUniquePtr<FHttpServerResponse> Response = FHttpServerResponse::Create(JsonPayload, TEXT("application/json"));
    Response->Code = EHttpServerResponseCodes::Ok; 

    if (!Response.IsValid())
    {
        UE_LOG(LogHiRezMCP, Error, TEXT("SendJsonResponse: FHttpServerResponse::Create failed to create a valid response object!"));
        return; 
    }
    
    UE_LOG(LogHiRezMCP, Verbose, TEXT("SendJsonResponse: Calling OnComplete. Response Code: %d"), Response->Code);
    OnComplete(MoveTemp(Response));
}

// Main handler for MCP requests, runs on an HTTP thread
void FMCPServer::HandleStreamableHTTPMCPRequest(const FHttpServerRequest& Request, const FHttpResultCallback& OnComplete)
{
	FUTF8ToTCHAR Convert((ANSICHAR*)Request.Body.GetData(), Request.Body.Num());
	FString RequestBody(Convert.Length(), Convert.Get());
    UE_LOG(LogHiRezMCP, Verbose, TEXT("Received MCP request: %s"), *RequestBody);
	
	FJsonRpcResponse Response;

    FJsonRpcRequest RpcRequest;
    if (!FJsonRpcRequest::CreateFromJsonString(RequestBody, RpcRequest))
    {
        UE_LOG(LogHiRezMCP, Error, TEXT("Failed to parse MCP request JSON: %s"), *RequestBody);
    	Response.error = MakeShared<FJsonRpcErrorObject>(EMCPErrorCode::ParseError, TEXT("Failed to parse MCP request JSON"));
        SendJsonRpcResponse(OnComplete, Response);
        return;
    }
	Response.id = RpcRequest.id;

    if (RpcRequest.jsonrpc != TEXT("2.0"))
    {
        UE_LOG(LogHiRezMCP, Error, TEXT("Invalid JSON-RPC version: %s"), *RpcRequest.jsonrpc);
		Response.error = MakeShared<FJsonRpcErrorObject>(EMCPErrorCode::InvalidRequest, TEXT("Invalid Request - JSON-RPC version must be 2.0"));
        SendJsonRpcResponse(OnComplete, Response);
        return;
    }

	JsonRpcHandler* Handler = JsonRpcMethodHandlers.Find(RpcRequest.method);
	if (!Handler)
	{
        UE_LOG(LogHiRezMCP, Warning, TEXT("Unknown MCP method received: %s"), *RpcRequest.method);
		Response.error = MakeShared<FJsonRpcErrorObject>(EMCPErrorCode::MethodNotFound, TEXT("Method not found"));
		SendJsonRpcResponse(OnComplete, Response);
		return;
	}

	auto SuccessObject = MakeShared<FJsonObject>();
	auto ErrorObject = MakeShared<FJsonRpcErrorObject>();
	if (!(*Handler)(RpcRequest, SuccessObject, *ErrorObject))
	{
		UE_LOG(LogHiRezMCP, Warning, TEXT("Error handling '%s': (%d) %s"), *RpcRequest.method, ErrorObject->code, *ErrorObject->message);
		Response.error = MoveTemp(ErrorObject);
		SendJsonRpcResponse(OnComplete, Response);
		return;
	}
	Response.result = MakeShared<FJsonValueObject>(MoveTemp(SuccessObject));
	SendJsonRpcResponse(OnComplete, Response);
}

void FMCPServer::RegisterInternalRpcMethodHandlers()
{
	RegisterRpcMethodHandler(TEXT("initialize"), [this](const FJsonRpcRequest& Request, TSharedPtr<FJsonObject> OutSuccess, FJsonRpcErrorObject& OutError)
	{
		return Rpc_Initialize(Request, OutSuccess, OutError);
	});
	RegisterRpcMethodHandler(TEXT("ping"), [this](const FJsonRpcRequest& Request, TSharedPtr<FJsonObject> OutSuccess, FJsonRpcErrorObject& OutError)
	{
		return Rpc_Ping(Request, OutSuccess, OutError);
	});
	RegisterRpcMethodHandler(TEXT("notifications/initialized"), [this](const FJsonRpcRequest& Request, TSharedPtr<FJsonObject> OutSuccess, FJsonRpcErrorObject& OutError)
	{
		return Rpc_ClientNotifyInitialized(Request, OutSuccess, OutError);
	});
}

bool FMCPServer::Rpc_Initialize(const FJsonRpcRequest& Request, TSharedPtr<FJsonObject> OutSuccess, FJsonRpcErrorObject& OutError)
{
	if (!Request.params.IsValid())
	{
		OutError.SetError(EMCPErrorCode::InvalidParams);
		OutError.message = TEXT("Failed to parse 'initialize' params: params object is null or invalid.");
        return false;
	}
	
	FInitializeParams InitParams;
	if (!FInitializeParams::CreateFromJsonObject(Request.params, InitParams))
	{
		OutError.SetError(EMCPErrorCode::InvalidParams);
		OutError.message = TEXT("Failed to parse 'initialize' params");
        return false;
	}

	// TODO proper capabilities negotiation
	
	FInitializeResult InitResult;
	InitResult.protocolVersion = MCP_PROTOCOL_VERSION; // Server's supported version
	InitResult.serverInfo.name = TEXT("HiRezMCPUnreal");
	InitResult.serverInfo.version = PLUGIN_VERSION;
	InitResult.serverInfo.HirezMCPUnreal_version = PLUGIN_VERSION;
	InitResult.serverInfo.unreal_engine_version = FEngineVersion::Current().ToString(EVersionComponent::Patch);

	// Populate ServerCapabilities (defaults are fine for now as defined in MCPTypes.h constructors)
	// FServerCapabilities members are default-initialized. Example: InitResult.serverCapabilities.tools.inputSchema = true;

	if (!InitResult.ToJsonObject(OutSuccess))
	{
		OutError.SetError(EMCPErrorCode::InternalError);
		OutError.message = TEXT("Failed to serialize initialize result");
		return false;
	}

	return true;
}

bool FMCPServer::Rpc_Ping(const FJsonRpcRequest& Request, TSharedPtr<FJsonObject> OutSuccess, FJsonRpcErrorObject& OutError)
{
	UE_LOG(LogHiRezMCP, Verbose, TEXT("Handling ping method."));
	return true;
}

bool FMCPServer::Rpc_ClientNotifyInitialized(const FJsonRpcRequest& Request, TSharedPtr<FJsonObject> OutSuccess, FJsonRpcErrorObject& OutError)
{
	UE_LOG(LogHiRezMCP, Verbose, TEXT("Handling ClientNotifyInitialized method."));
	return true;
}
