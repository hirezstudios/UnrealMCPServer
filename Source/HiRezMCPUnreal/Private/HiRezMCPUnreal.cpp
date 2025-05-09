// Copyright Epic Games, Inc. All Rights Reserved.

#include "HiRezMCPUnreal.h"
#include "HttpServerModule.h"
#include "HttpServerRequest.h"
#include "HttpServerResponse.h"
#include "IHttpRouter.h"
#include "JsonDomBuilder.h" // For simple JSON responses
#include "HAL/PlatformProcess.h" // For FPlatformProcess::Sleep

#define LOCTEXT_NAMESPACE "FHiRezMCPUnrealModule"

void FHiRezMCPUnrealModule::StartupModule()
{
	UE_LOG(LogTemp, Warning, TEXT("HiRezMCPUnrealModule has started"));
	StartHttpServer();
}

void FHiRezMCPUnrealModule::ShutdownModule()
{
	UE_LOG(LogTemp, Warning, TEXT("HiRezMCPUnrealModule has shut down"));
	StopHttpServer();
}

void FHiRezMCPUnrealModule::StartHttpServer()
{
	FHttpServerModule& HttpServerModule = FHttpServerModule::Get();
	HttpRouter = HttpServerModule.GetHttpRouter(HttpServerPort);

	if (!HttpRouter.IsValid())
	{
		UE_LOG(LogTemp, Error, TEXT("HiRezMCPUnreal: Failed to get HttpRouter on port %d. Another server might be running or port is in use."), HttpServerPort);
		return;
	}

	// Register routes
	HealthCheckRouteHandle = HttpRouter->BindRoute(FHttpPath(TEXT("/health")), EHttpServerRequestVerbs::VERB_GET, 
		[](const FHttpServerRequest& Request, const FHttpResultCallback& OnComplete) -> bool {
			FHiRezMCPUnrealModule::HandleHealthCheck(Request, OnComplete);
			return true;
		});

	ListResourcesRouteHandle = HttpRouter->BindRoute(FHttpPath(TEXT("/-/list-resources")), EHttpServerRequestVerbs::VERB_POST,
		[](const FHttpServerRequest& Request, const FHttpResultCallback& OnComplete) -> bool {
			FHiRezMCPUnrealModule::HandleListResourcesRequest(Request, OnComplete);
			return true;
		});

	ReadResourceRouteHandle = HttpRouter->BindRoute(FHttpPath(TEXT("/-/read-resource")), EHttpServerRequestVerbs::VERB_POST,
		[](const FHttpServerRequest& Request, const FHttpResultCallback& OnComplete) -> bool {
			FHiRezMCPUnrealModule::HandleReadResourceRequest(Request, OnComplete);
			return true;
		});

	MCPGeneralRouteHandle = HttpRouter->BindRoute(FHttpPath(TEXT("/mcp")), EHttpServerRequestVerbs::VERB_POST, 
		[](const FHttpServerRequest& Request, const FHttpResultCallback& OnComplete) -> bool {
			FHiRezMCPUnrealModule::HandleGeneralMCPRequest(Request, OnComplete);
			return true;
		});

	UE_LOG(LogTemp, Log, TEXT("HiRezMCPUnreal: Bound /health, /-/list-resources, /-/read-resource, /mcp"));

    // Start listening for requests
    HttpServerModule.StartAllListeners();
    UE_LOG(LogTemp, Log, TEXT("HiRezMCPUnreal: HTTP Server started on port %d."), HttpServerPort);
}

void FHiRezMCPUnrealModule::StopHttpServer()
{
	if (HttpRouter.IsValid())
	{
		// Unbind routes using stored handles
		if (HealthCheckRouteHandle.IsValid()) HttpRouter->UnbindRoute(HealthCheckRouteHandle);
		if (ListResourcesRouteHandle.IsValid()) HttpRouter->UnbindRoute(ListResourcesRouteHandle);
		if (ReadResourceRouteHandle.IsValid()) HttpRouter->UnbindRoute(ReadResourceRouteHandle);
		if (MCPGeneralRouteHandle.IsValid()) HttpRouter->UnbindRoute(MCPGeneralRouteHandle);

		UE_LOG(LogTemp, Log, TEXT("HiRezMCPUnreal: Routes unbound."));
	}

    // Stop all listeners associated with the module instance
    FHttpServerModule& HttpServerModule = FHttpServerModule::Get();
    HttpServerModule.StopAllListeners(); 
    UE_LOG(LogTemp, Log, TEXT("HiRezMCPUnreal: HTTP Server stopped."));

	HttpRouter.Reset();
}

// HTTP Handler method implementations
void FHiRezMCPUnrealModule::HandleHealthCheck(const FHttpServerRequest& Request, const FHttpResultCallback& OnComplete)
{
	TUniquePtr<FHttpServerResponse> Response = MakeUnique<FHttpServerResponse>();
	Response->Code = EHttpServerResponseCodes::Ok;
	Response->Headers.Add(TEXT("Content-Type"), TArray<FString>{TEXT("application/json")});
	FTCHARToUTF8 ConvertToUtf8(TEXT("{\"status\": \"OK\"}"));
	const uint8* ConvertToUtf8Bytes = (reinterpret_cast<const uint8*>(ConvertToUtf8.Get()));
	Response->Body.Append(ConvertToUtf8Bytes, ConvertToUtf8.Length());
	OnComplete(MoveTemp(Response));
}

void FHiRezMCPUnrealModule::HandleListResourcesRequest(const FHttpServerRequest& Request, const FHttpResultCallback& OnComplete)
{
	// Placeholder implementation
	TUniquePtr<FHttpServerResponse> Response = MakeUnique<FHttpServerResponse>();
	Response->Code = EHttpServerResponseCodes::Ok;
	Response->Headers.Add(TEXT("Content-Type"), TArray<FString>{TEXT("application/json")});
	FTCHARToUTF8 ConvertToUtf8(TEXT("{\"resources\": []}"));
	const uint8* ConvertToUtf8Bytes = (reinterpret_cast<const uint8*>(ConvertToUtf8.Get()));
	Response->Body.Append(ConvertToUtf8Bytes, ConvertToUtf8.Length());
	OnComplete(MoveTemp(Response));
}

void FHiRezMCPUnrealModule::HandleReadResourceRequest(const FHttpServerRequest& Request, const FHttpResultCallback& OnComplete)
{
	// Placeholder implementation
	TUniquePtr<FHttpServerResponse> Response = MakeUnique<FHttpServerResponse>();
	Response->Code = EHttpServerResponseCodes::NotFound;
	Response->Headers.Add(TEXT("Content-Type"), TArray<FString>{TEXT("application/json")});
	FTCHARToUTF8 ConvertToUtf8(TEXT("{\"error\": \"Resource not found\"}"));
	const uint8* ConvertToUtf8Bytes = (reinterpret_cast<const uint8*>(ConvertToUtf8.Get()));
	Response->Body.Append(ConvertToUtf8Bytes, ConvertToUtf8.Length());
	OnComplete(MoveTemp(Response));
}

void FHiRezMCPUnrealModule::HandleGeneralMCPRequest(const FHttpServerRequest& Request, const FHttpResultCallback& OnComplete)
{
	// Placeholder implementation for general MCP requests
	// This will eventually parse the request body to determine the MCP command and parameters
	TUniquePtr<FHttpServerResponse> Response = MakeUnique<FHttpServerResponse>();
	Response->Code = EHttpServerResponseCodes::NotSupported;
	Response->Headers.Add(TEXT("Content-Type"), TArray<FString>{TEXT("application/json")});
	FTCHARToUTF8 ConvertToUtf8(TEXT("{\"message\": \"General MCP handler not fully implemented yet.\"}"));
	const uint8* ConvertToUtf8Bytes = (reinterpret_cast<const uint8*>(ConvertToUtf8.Get()));
	Response->Body.Append(ConvertToUtf8Bytes, ConvertToUtf8.Length());
	OnComplete(MoveTemp(Response));
}

#undef LOCTEXT_NAMESPACE
	
IMPLEMENT_MODULE(FHiRezMCPUnrealModule, HiRezMCPUnreal);
