// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"
#include "HttpServerRequest.h"  // Added for FHttpServerRequest
#include "HttpServerResponse.h" // Added for IHttpServerResponse
#include "IHttpRouter.h"        // Added for IHttpRouter and FHttpRouteHandle

class FHiRezMCPUnrealModule : public IModuleInterface
{
public:

	/** IModuleInterface implementation */
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

private:
	/** Starts the HTTP server and registers routes */
	void StartHttpServer();

	/** Stops the HTTP server */
	void StopHttpServer();

	// HTTP Handler methods
	static void HandleHealthCheck(const FHttpServerRequest& Request, const FHttpResultCallback& OnComplete);
	static void HandleListResourcesRequest(const FHttpServerRequest& Request, const FHttpResultCallback& OnComplete);
	static void HandleReadResourceRequest(const FHttpServerRequest& Request, const FHttpResultCallback& OnComplete);
	static void HandleGeneralMCPRequest(const FHttpServerRequest& Request, const FHttpResultCallback& OnComplete);

	/** HTTP router instance */
	TSharedPtr<IHttpRouter> HttpRouter;

    /** Port for the HTTP server */
    uint32 HttpServerPort = 30069; // Default port, can be made configurable

	/** Route handles to keep track of registered routes */
	FHttpRouteHandle HealthCheckRouteHandle;
	FHttpRouteHandle ListResourcesRouteHandle;
	FHttpRouteHandle ReadResourceRouteHandle;
	FHttpRouteHandle MCPGeneralRouteHandle;
};
