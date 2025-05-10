# Technical Specification: HiRezMCPUnreal - MCP Server (HTTP Implementation)

**Version:** 1.0
**Date:** 2025-05-09
**Author:** Cascade AI

## 1. Introduction

This document outlines the technical specification for implementing a Model Context Protocol (MCP) Server within the `HiRezMCPUnreal` Unreal Engine plugin. This implementation will adhere to the MCP standard, utilizing HTTP for primary communication, as detailed in the `MCP_Server_Implementation_Research.md` document.

The goal is to expose Unreal Engine capabilities (assets, editor functions, game state) as MCP Tools, Resources, and Prompts, enabling external AI models and applications to interact with the Unreal Engine environment in a standardized way.

This specification supersedes previous designs that may have focused on raw TCP communication for JSON-RPC, aligning instead with the more common HTTP transport for MCP.

## 2. Core Architecture in Unreal Engine

### 2.1. Server Component

The MCP Server will be implemented as part of an Unreal Engine plugin (`HiRezMCPUnreal`). It will run within the Unreal Editor or a packaged game instance.

*   **Plugin Module:** The core server logic will reside in a dedicated plugin module (e.g., `HiRezMCPServerModule`).
*   **Lifecycle Management:** The server will start when the plugin module loads (or on-demand via a console command/editor button) and shut down gracefully when the module unloads or the engine exits.

### 2.2. HTTP Server Implementation

An HTTP server will be embedded within the plugin to handle MCP communications.
*   **Port:** The server will listen on a configurable HTTP port (default, e.g., 30069, but clearly identified as HTTP).
*   **Communication Model:** Initial implementation will use a synchronous HTTP POST request/response model. Client sends MCP Request via POST, Server returns MCP Response in the HTTP response body. Server-Sent Events (SSE) and server-initiated notifications are **deferred**.
*   **Options for HTTP Server:**
    1.  **Unreal Engine's `FHttpServerModule`:**
        *   Leverages built-in engine capabilities (`IHttpRouter`, `FHttpServerResponse`).
        *   Pros: No external dependencies.
        *   Cons: Might be less flexible or performant than dedicated C++ HTTP libraries; stability for production use needs verification (some parts might be experimental or primarily for Web UI).
    2.  **Third-party C++ HTTP Library:** (e.g., `civetweb`, `cpp-httplib`, `Boost.Beast`)
        *   Pros: Potentially more robust, feature-rich (e.g., easier SSE handling, HTTPS support).
        *   Cons: Adds external dependency; integration effort (compilation, linking).
*   **Decision Point:** The initial implementation will attempt to use `FHttpServerModule`. If significant limitations are encountered, a switch to a third-party library will be evaluated.

### 2.3. Threading Model

All network I/O and potentially lengthy MCP request processing must occur on non-game threads to prevent blocking the Unreal Engine main thread and causing hitches.
*   **HTTP Listener Thread:** The HTTP server will have its own thread(s) for listening to and accepting incoming connections.
*   **Request Handler Threads:** Each incoming MCP request (via HTTP POST) might be processed in a separate task or thread pool.
*   **UE Main Thread Synchronization:** Any operations requiring access to UE game objects or systems (e.g., `UWorld`, `GEditor`) must be marshaled back to the game thread using `AsyncTask(ENamedThreads::GameThread, ...)` or similar mechanisms.
*   **UE Async Primitives:** Utilize `FAsyncTask`, `FTSTicker`, `Tasks` (if UE5.1+), and thread-safe data structures (`TQueue`, `FCriticalSection`).

## 3. Protocol Mechanics in UE C++

### 3.1. JSON-RPC 2.0 Handling

*   **JSON Library:** Unreal Engine's built-in JSON utilities will be used:
    *   `TJsonReader` / `TJsonWriter` for streaming.
    *   `FJsonObject` / `FJsonValue` for DOM-style manipulation.
    *   `FJsonObjectConverter` for serializing/deserializing USTRUCTs to/from JSON.
*   **Core JSON-RPC Structures:** C++ USTRUCTs will define JSON-RPC messages:
    ```cpp
    // Example (simplified)
    USTRUCT()
    struct FJsonRpcRequest
    {
        GENERATED_BODY()

        UPROPERTY() FString jsonrpc; // "2.0"
        UPROPERTY() FString method;
        UPROPERTY() TSharedPtr<FJsonObject> params; // Or a specific USTRUCT
        UPROPERTY() FString id; // Optional for notifications
    };

    USTRUCT()
    struct FJsonRpcResponse
    {
        GENERATED_BODY()

        UPROPERTY() FString jsonrpc; // "2.0"
        UPROPERTY() TSharedPtr<FJsonValue> result; // Can be any valid JSON
        UPROPERTY() TSharedPtr<FJsonObject> error; // For error responses
        UPROPERTY() FString id;
    };

    USTRUCT()
    struct FJsonRpcErrorObject
    {
        GENERATED_BODY()

        UPROPERTY() int32 code;
        UPROPERTY() FString message;
        UPROPERTY() TSharedPtr<FJsonValue> data; // Optional
    };
    ```
*   **Error Codes:** Standard JSON-RPC error codes (`-32700` to `-32000`) will be used. Custom error codes for MCP-specific issues will be defined if necessary, following the JSON-RPC specification.

### 3.2. MCP Connection Lifecycle

A C++ class (e.g., `FMcpSession`) will manage the state of each connected client.

*   **Initialization Phase (`initialize` method):**
    *   Handler for the `initialize` JSON-RPC method.
    *   Input: `FInitializeParams` USTRUCT (containing `protocolVersion`, `clientInfo`, `capabilities`).
    *   Logic:
        *   Version negotiation: Compare client `protocolVersion` with server's supported version (e.g., "2025-03-26").
        *   Capability negotiation: Store client capabilities and prepare server capabilities.
    *   Output: `FInitializeResult` USTRUCT (containing `protocolVersion`, `serverInfo`, `serverCapabilities`).
    *   This response is sent synchronously in the HTTP response to the `initialize` POST request.
*   **`notifications/initialized` Notification:**
    *   Upon receiving this notification from the client (via HTTP POST), the session is marked as fully active. (Server provides an empty HTTP 204 No Content or simple JSON-RPC success response).
*   **Operation Phase:**
    *   A dispatcher (e.g., a `TMap<FString, TFunction<void(TSharedPtr<FJsonRpcRequest>, TSharedRef<FMcpSession>)>>`) will route incoming JSON-RPC methods (from HTTP POST) to their respective handlers.
    *   Handlers will process requests and return JSON-RPC responses directly in the HTTP POST response body.
*   **Shutdown Phase:**
    *   With synchronous HTTP, explicit session shutdown is less defined by transport. The `shutdown` and `exit` MCP methods can be used.
    *   Implement `$/cancelRequest` to handle client-side cancellation of long-running operations (server will attempt to stop processing, but response for original request might have already been sent or is pending).

## 4. HTTP Transport Implementation (Synchronous POST)

### 4.1. Server-Side Endpoint

A single base path (e.g., `/mcp`) will be used for all MCP communication.

*   **HTTP POST `/mcp`:**
    *   Clients send all MCP Request or Notification messages to the server using HTTP POST to this endpoint.
    *   The body of the POST request contains the JSON-RPC message string.
    *   The server processes the JSON-RPC message.
        *   If it's an MCP Request (containing an `id`), the server generates a corresponding JSON-RPC Response. This Response is sent back as the body of the HTTP 200 OK response to this POST request.
        *   If it's an MCP Notification, the server processes it and typically returns an HTTP 204 No Content response or a generic JSON-RPC success acknowledgement if a response body is expected by the client framework for notifications (though JSON-RPC notifications strictly don't require responses).
    *   **Session Management:** While HTTP POST is stateless, MCP itself implies a stateful session established by `initialize`. The server will need to manage session state (e.g., negotiated capabilities) in memory, potentially associated with a client identifier if provided/feasible, or assume a single-client model for simplicity if no robust client identification is implemented initially.

### 4.2. Server-to-Client Message Delivery

*   All MCP JSON-RPC **Responses** from the server to the client are delivered synchronously as the body of the HTTP POST response that carried the MCP Request.
*   Server-initiated MCP JSON-RPC **Notifications** (e.g., `notifications/tools/list_changed`, `$/progress`) are **not supported** in this initial synchronous HTTP model. Their implementation is deferred and would require enabling SSE or an alternative push mechanism.

### 4.3. Security Considerations

*   **TLS (HTTPS):**
    *   MCP specification mandates TLS for production environments.
    *   **Decision:** For the initial implementation phases, TLS/HTTPS support will be **deferred** to simplify development. The server will operate over HTTP.
    *   Implementing HTTPS with `FHttpServerModule` might require engine modifications or be non-trivial.
    *   If using a third-party library, ensure it supports SSL/TLS (e.g., OpenSSL integration) for future implementation.
    *   **Warning:** Operating without TLS is insecure for production or sensitive data. This approach is for local development and testing only. Future work must address TLS implementation before any deployment outside of a trusted local environment.
*   **Origin Validation:** If possible with the chosen HTTP server, validate the `Origin` header.
*   **Authentication/Authorization:** The MCP spec largely defers this to the client/host. The server will initially trust incoming requests on the configured port. Future enhancements could include API key validation or other mechanisms if required.

## 5. MCP Features Implementation (UE C++)

USTRUCTs will be defined for all MCP data types (ToolDefinition, Resource, Prompt, etc.), mirroring the MCP JSON schema.

### 5.1. Tools

*   **`tools/list`:**
    *   Returns a list of available `FToolDefinition`s.
    *   Tools can be discovered via:
        *   A C++ interface (e.g., `IMcpTool`) implemented by UObjects.
        *   Reflection over UFUNCTIONs/UCLASSes with specific metadata.
        *   Manual registration.
*   **`tools/call`:**
    *   Input: `FToolCallParams` (`toolName`, `toolInput` which is a JSON object).
    *   Locates the tool by name.
    *   Input schema validation (if `inputSchema` is defined in `FToolDefinition`).
    *   Executes the tool:
        *   If native C++, directly call the function.
        *   Execution must be on a worker thread if potentially long-running, with results posted back. Use `$/progress` for updates.
    *   Output: `FToolCallResult` (containing `contentType`, `content`). `ContentPart`s (text, image) need to be supported.
*   **`notifications/tools/list_changed`:**
    *   Sent when the set of available tools changes. Requires server capability `tools: { "listChanged": true }`. **(Deferred with SSE)**

### 5.2. Resources

*   **`resources/list`:**
    *   Returns a list of `FResourceDefinition`s.
    *   Resources could be:
        *   Unreal Assets (Blueprints, Textures, Materials, Levels identified by path).
        *   Editor objects or game state information.
*   **`resources/get_content`:**
    *   Input: `uri` of the resource.
    *   Returns the content of the resource (e.g., T3D text for a Blueprint, pixel data for a texture).
    *   This is where the "get_blueprint_contents" feature from the initial memory fits.
*   **`resources/subscribe` & `notifications/resources/content_changed`:**
    *   Allows clients to subscribe to changes in a resource.
    *   Requires server capability `resources: { "subscribe": true }`. **(Deferred with SSE)**
    *   Mechanisms like `FCoreUObjectDelegates::OnObjectModified` or asset-specific callbacks could trigger notifications.

### 5.3. Prompts

*   **`prompts/list`:** Returns available `FPromptDefinition`s.
*   **`prompts/get_template`:** Returns the template structure for a prompt.
*   **`prompts/render`:**
    *   Input: `FPromptParameters` (`uri`, `variables`).
    *   Renders the prompt template with provided variables.
    *   Output: `FRenderedPrompt` (containing `messages` array for LLM).

### 5.4. Utilities

*   **`ping`:** Simple request/response to check connectivity.
*   **`$/progress`:** Server sends this notification for long-running tool calls. **(Deferred with SSE)**
*   **`$/cancelRequest`:** Client sends this to request cancellation of an operation. Server should attempt to honor it.

## 6. Data Structures and Schema Adherence (UE C++)

*   **Source of Truth:** The official MCP JSON schema (version "2025-03-26" or latest).
*   **USTRUCTs:** All JSON objects defined in the MCP schema will have corresponding USTRUCTs.
    *   Example: `FClientInfo`, `FServerInfo`, `FInitializeCapabilities`, `FToolDefinition`, `FResourceDefinition`, `FPromptDefinition`, `FContentPart`, etc.
*   **`FJsonObjectConverter`:** Will be heavily used for serialization/deserialization between JSON strings and USTRUCTs.
*   **Validation:**
    *   Incoming JSON will be validated by attempting to deserialize into the expected USTRUCT. Missing required fields or type mismatches will result in JSON-RPC `InvalidParams` errors.
    *   Outgoing JSON generated from USTRUCTs should inherently conform if USTRUCTs are correctly defined.

## 7. Error Handling and Logging

*   **UE Logging:** Use `UE_LOG` extensively with a dedicated log category (e.g., `LogHiRezMCP`).
*   **JSON-RPC Errors:** All errors will be reported as valid JSON-RPC error responses.
*   **Exception Handling:** C++ exceptions should be used judiciously and caught at appropriate boundaries to be converted into JSON-RPC errors.

## 8. Modularity and Extensibility

*   **Service Locator/Registry:** For tools, resources, and prompts to allow dynamic registration.
*   **Interfaces:** Define C++ interfaces for different types of MCP capabilities that can be implemented by other parts of the plugin or game code.
*   **Configuration:** Allow configuration of port, enabled features, etc., via INI files or editor settings.

## 9. Phased Implementation Plan (High-Level)

1.  **Phase 1: Core HTTP Server & JSON-RPC Framework**
    *   Set up chosen HTTP server (`FHttpServerModule` PoC) for synchronous POST request/response.
    *   Implement JSON-RPC message parsing and serialization (core USTRUCTs).
    *   Implement `initialize` handshake (synchronous response) and `notifications/initialized` handling.
    *   Implement `ping` utility.
2.  **Phase 2: Basic Tool Implementation**
    *   Implement `tools/list` and `tools/call` (synchronous responses).
    *   Create 1-2 simple example C++ tools (e.g., a tool to log a message, a tool to query basic editor state).
    *   ~~Implement `notifications/tools/list_changed`.~~ (Deferred)
3.  **Phase 3: Basic Resource Implementation**
    *   Implement `resources/list` and `resources/get_content` (synchronous responses).
    *   Implement `get_blueprint_contents` as a resource.
    *   ~~Implement `notifications/resources/list_changed`.~~ (Deferred)
4.  **Phase 4: Advanced Features & Refinements**
    *   Implement Prompts.
    *   ~~Implement resource subscriptions (`resources/subscribe`, `notifications/resources/content_changed`).~~ (Deferred)
    *   ~~Implement `$/progress` and~~ `$/cancelRequest`.
    *   Robust error handling and logging.
    *   Threading improvements and performance testing.
5.  **Phase 5: Security & Deployment**
    *   Implement TLS (HTTPS).
    *   Configuration options.
    *   Packaging and testing.

## 10. Open Questions & Risks

*   **`FHttpServerModule` Suitability:** Is it robust enough for production use, especially for ~~SSE and~~ potentially HTTPS?
*   **HTTPS Implementation:** Complexity of integrating TLS, especially if `FHttpServerModule` doesn't support it easily.
*   **Session Management without SSE:** How to effectively manage client-specific state (like negotiated capabilities) across stateless HTTP POST requests if no client identifier is consistently passed or managed.
*   **Performance:** Impact of JSON processing and network traffic on UE performance, especially with many connected clients or large data transfers.
*   **Schema Evolution:** Keeping USTRUCTs in sync with official MCP schema updates.

## 11. Future Considerations (Post-MVP)

*   **Implement Server-Sent Events (SSE):** To enable server-initiated notifications (`list_changed`, `content_changed`, `$/progress`) and support for capabilities like `tools: { "listChanged": true }` and `resources: { "subscribe": true }`.
*   Server-initiated Sampling (`sampling/createMessage`).
*   Client-exposed Roots (`roots/list`).
*   Structured output from tools (beyond simple text/binary).
*   More comprehensive authentication/authorization.
