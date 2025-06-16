# Technical Specification: UnrealMCPServer Plugin

## 1. Overview

The UnrealMCPServer plugin will implement a Model Context Protocol (MCP) server within the Unreal Engine Editor. It will allow AI agents to interact with the Unreal Engine environment by exposing HTTP endpoints according to the MCP specification (2025-03-26).

**Key Requirements:**

*   **MCP Version:** 2025-03-26
*   **Protocol:** HTTP (initially, no HTTPS)
*   **Port:** 30069
*   **Authentication:** None (initially)
*   **Streaming:** Not supported (initially)
*   **Session Support:** Not supported (initially)
*   **Platform:** Unreal Engine Editor Plugin

## 2. Core Modules and Classes

### 2.1. `FUnrealMCPServerModule` (Module Core)

*   **Responsibilities:**
    *   Implements `IModuleInterface`.
    *   Handles plugin startup (`StartupModule`) and shutdown (`ShutdownModule`).
    *   Initializes, starts, and stops the HTTP server.
    *   Manages the HTTP router instance.
*   **Key Members:**
    *   `TSharedPtr<IHttpRouter> HttpRouter`: The main HTTP router.
    *   `FHttpRouteHandle HealthCheckRouteHandle`: Handle for the `GET /health` endpoint.
    *   `FHttpRouteHandle ListResourcesRouteHandle`: Handle for the `POST /-/list-resources` endpoint.
    *   `FHttpRouteHandle ReadResourceRouteHandle`: Handle for the `POST /-/read-resource` endpoint.
    *   `FHttpRouteHandle MCPGeneralRouteHandle`: Handle for the general `POST /mcp` endpoint.
    *   `uint32 HttpServerPort = 30069;`

### 2.2. `FHiRezMCPRequestHandler` (Static or Utility Class)

*   **Responsibilities:**
    *   Contains static methods to handle incoming HTTP requests for each dedicated MCP endpoint (`/health`, `/-/list-resources`, `/-/read-resource`).
    *   Contains static methods or a dispatcher for requests to the general `/mcp` endpoint, routing based on the JSON-RPC `method` field.
    *   Parses incoming JSON request bodies.
    *   Constructs JSON response bodies.
    *   Interacts with Unreal Engine APIs to fulfill MCP requests (e.g., listing actors, getting asset properties). This part will be expanded as we define specific resources and tools.
*   **Key Methods (Examples based on MCP Spec):**
    *   `static void HandleHealthCheck(const FHttpServerRequest& Request, const TSharedRef<IHttpServerResponse, ESPMode::ThreadSafe>& Response);`
    *   `static void HandleListResourcesRequest(const FHttpServerRequest& Request, const TSharedRef<IHttpServerResponse, ESPMode::ThreadSafe>& Response); // For /-/list-resources`
    *   `static void HandleReadResourceRequest(const FHttpServerRequest& Request, const TSharedRef<IHttpServerResponse, ESPMode::ThreadSafe>& Response); // For /-/read-resource`
    *   `static void HandleGeneralMCPRequest(const FHttpServerRequest& Request, const TSharedRef<IHttpServerResponse, ESPMode::ThreadSafe>& Response); // For /mcp, will dispatch internally`
    *   // Internal dispatch methods within HandleGeneralMCPRequest or separate static methods:
    *   // static void HandleInitialize(const FMCPInitializeRequest& MCPRequest, FMCPInitializeResult& MCPResponse); 
    *   // static void HandlePing(const FMCPPingRequest& MCPRequest, FMCPPongResponse& MCPResponse);
    *   // static void HandleListTools(const FMCPListToolsRequest& MCPRequest, FMCPListToolsResponse& MCPResponse);
    *   // static void HandleCallTool(const FMCPCallToolRequest& MCPRequest, FMCPCallToolResponse& MCPResponse);
    *   // static void HandleInitialized(const FMCPInitializedParams& MCPParams); // For notifications
    *   // static void HandleShutdown(const FMCPShutdownParams& MCPParams);
    *   // static void HandleExit(const FMCPExitParams& MCPParams);

### 2.3. MCP Data Structures (UE Structs)

*   **Responsibilities:**
    *   Define USTRUCTS to represent MCP JSON-RPC request parameters and result/error bodies for easy serialization/deserialization with Unreal's JSON utilities.
*   **Examples (based on MCP Spec for chosen methods):**
    *   `FMCPErrorObject` (for the `error` field in a JSON-RPC error response)
    *   **General Lifecycle & Ping:**
        *   `FMCPInitializeRequestParams`
        *   `FMCPInitializeResult`
        *   `FMCPClientInfo` (part of `InitializeRequestParams`)
        *   `FMCPServerInfo` (part of `InitializeResult`)
        *   `FMCPInitializedParams` (for `initialized` notification)
        *   `FMCPShutdownParams` (empty or specific, for `shutdown` request)
        *   `FMCPExitParams` (empty, for `exit` notification)
        *   `FMCPPingRequestParams` (likely empty or with optional data)
        *   `FMCPPongResult` (likely empty or with server data)
    *   **Resources:**
        *   `FMCPListResourcesRequestParams` (e.g., `cursor`)
        *   `FMCPListResourcesResult` (e.g., `resources`, `nextCursor`)
        *   `FMCPResourceDescriptor` (e.g., `uri`, `name`, `description`, `mimeType`)
        *   `FMCPReadResourceRequestParams` (e.g., `uri`)
        *   `FMCPReadResourceResult` (e.g., `contents`)
        *   `FMCPResourceContent` (e.g., `uri`, `mimeType`, `text`, `binary`)
    *   **Tools:**
        *   `FMCPListToolsRequestParams` (e.g., `cursor`)
        *   `FMCPListToolsResult` (e.g., `tools`, `nextCursor`)
        *   `FMCPToolDescriptor` (e.g., `name`, `description`, `inputSchema`)
        *   `FMCPCallToolRequestParams` (e.g., `name`, `arguments` - which could be a `FJsonObjectWrapper` or a specific USTRUCT)
        *   `FMCPCallToolResult` (e.g., `content`, `isError`)
        *   `FMCPToolResultContentItem` (e.g. `type`, `text`, `json`, etc.)
    *   (More to be added as per the spec for chosen endpoints)

## 3. MCP Endpoints to Implement (Initial Scope)

Based on the MCP specification (v2025-03-26), and the "no streaming, no session" constraint:

### 3.1. Overview

The MCP server will expose the following HTTP endpoints, adhering to the MCP v2025-03-26 specification. All POST requests expect a JSON-RPC 2.0 compliant body and will return JSON-RPC 2.0 compliant responses.

### 3.2 Endpoints

**Basic Endpoints (as per MCP Basic Specification):**

- `GET /health`
  - **Description**: Standard health check.
  - **MCP Method**: N/A (Direct HTTP)

- `POST /-/list-resources`
  - **Description**: Resource discovery.
  - **MCP Method**: `resources/list` (sent in JSON-RPC body)

- `POST /-/read-resource`
  - **Description**: Retrieve resource content.
  - **MCP Method**: `resources/read` (sent in JSON-RPC body)

**General MCP Endpoint (as per Streamable HTTP Transport Guidelines for other methods):**

- `POST /mcp` (Path to be finalized, e.g., `/mcp`, `/rpc`, or `/`)
  - **Description**: Handles all other MCP JSON-RPC methods not covered by dedicated basic endpoints. The specific MCP method is defined in the `"method"` field of the JSON-RPC request body.
  - **Supported MCP Methods (Initial Scope - Request/Response only, no notifications/streaming):**
    - `initialize`: Establishes session and capabilities.
    - `initialized`: Notification from client after `initialize`.
    - `shutdown`: Client requests server shutdown.
    - `exit`: Client informs server it's exiting (server may also exit).
    - `ping`: Health check via JSON-RPC.
    - `resources/list`: (Can also be routed here if preferred over `/-/list-resources`)
    - `resources/read`: (Can also be routed here if preferred over `/-/read-resource`)
    - `tools/list`: Discover available server tools.
    - `tools/call`: Execute a server-defined tool.

**Note on Notifications and Streaming:**
For the initial implementation, the server will focus on basic request/response. Notifications (e.g., `notifications/resources/list_changed`, `notifications/tools/list_changed`) and SSE streaming for unsolicited server messages or long-running operations will **NOT** be supported. The `POST /mcp` endpoint will handle requests and return a single JSON-RPC response over HTTP, not an SSE stream.

## 4. HTTP Server Setup

*   Utilize `FHttpServerModule` from Unreal Engine.
*   Bind routes in `FUnrealMCPServerModule::StartupModule`.
*   Ensure proper error handling and HTTP status codes.

## 5. JSON Handling

*   Use Unreal's built-in JSON parsing and serialization utilities:
    *   `FJsonObjectConverter` for USTRUCT to JSON and vice-versa.
    *   `TJsonReaderFactory` and `TJsonWriterFactory`.

## 6. Future Considerations (Out of Initial Scope)

*   Authentication (API keys, tokens).
*   HTTPS support.
*   Streaming responses.
*   Session management.
*   More comprehensive resource types (e.g., specific actor properties, material parameters, console command execution).
*   `/-/create-resource`, `/-/update-resource`, `/-/delete-resource` endpoints.
