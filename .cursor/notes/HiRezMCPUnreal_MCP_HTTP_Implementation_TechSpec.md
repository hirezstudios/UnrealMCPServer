# Technical Specification: HiRezMCPUnreal - MCP Server (Streamable HTTP Implementation)

**Version:** 1.0
**Date:** 2025-05-09
**Author:** Cascade AI

## 1. Introduction

This document outlines the technical specification for implementing a Model Context Protocol (MCP) Server within the `HiRezMCPUnreal` Unreal Engine plugin. This implementation will adhere to the MCP standard, utilizing Streamable HTTP for primary communication.

The goal is to expose Unreal Engine capabilities (assets, editor functions, game state) as MCP Tools, Resources, and Prompts, enabling external AI models and applications to interact with the Unreal Engine environment in a standardized way.

This specification supersedes previous designs that may have focused on raw TCP communication for JSON-RPC, aligning instead with the more common Streamable HTTP transport for MCP.

## 2. Core Architecture in Unreal Engine

### 2.1. Server Component

The MCP Server will be implemented as part of an Unreal Engine plugin (`HiRezMCPUnreal`). It will run within the Unreal Editor or a packaged game instance.

*   **Plugin Module:** The core server logic will reside in a dedicated plugin module (e.g., `HiRezMCPServerModule`).
*   **Lifecycle Management:** The server will start when the plugin module loads (or on-demand via a console command/editor button) and shut down gracefully when the module unloads or the engine exits.

### 2.2. HTTP Server Implementation

An HTTP server will be embedded within the plugin to handle MCP communications using the **Streamable HTTP** transport, as per the current MCP specification.

*   **Port:** The server will listen on a configurable HTTP port (default, e.g., 30069).
*   **Communication Model (Streamable HTTP):**
    *   Clients send JSON-RPC messages via HTTP POST to a single MCP endpoint.
    *   Clients MUST include an `Accept: application/json, text/event-stream` header, signaling their ability to handle both direct JSON responses and Server-Sent Events (SSE) streams.
    *   The server responds based on the request and its capabilities:
        *   `HTTP 202 Accepted` (no body): If the POST contained only JSON-RPC responses or notifications.
        *   `Content-Type: application/json`: For a single JSON-RPC response to client requests.
        *   `Content-Type: text/event-stream`: To initiate an SSE stream for multiple/ongoing messages or server-initiated notifications related to the client's request. **(Deferred for initial implementation)**
    *   Clients MAY also use HTTP GET (with `Accept: text/event-stream`) to the MCP endpoint to request a server-initiated SSE stream. **(Deferred for initial implementation)**
    *   SSE is an *optional component* of Streamable HTTP, employed by the server when streaming is appropriate. It is not a standalone, always-on requirement. **For the initial implementation, support for all SSE-related capabilities (including `Content-Type: text/event-stream` responses and client GET for server-initiated streams) is deferred. The server will initially only support `Content-Type: application/json` responses to client POST requests.**
*   **Options for HTTP Server:**
    1.  **Unreal Engine's `FHttpServerModule`:**
        *   Leverages built-in engine capabilities (`IHttpRouter`, `FHttpServerResponse`).
        *   Pros: No external dependencies.
        *   Cons: Might be less flexible or performant than dedicated C++ HTTP libraries; stability for production use needs verification (some parts might be experimental or primarily for Web UI).
    2.  **Third-party C++ HTTP Library:** (e.g., `civetweb`, `cpp-httplib`, `Boost.Beast`)
        *   Pros: Potentially more robust, feature-rich (e.g., easier SSE handling, HTTPS support).
        *   Cons: Adds external dependency; integration effort (compilation, linking).
*   **Decision Point:** The initial implementation will attempt to use `FHttpServerModule`. If significant limitations are encountered (e.g., with SSE streaming or header manipulation for Streamable HTTP), a switch to a third-party library will be evaluated.

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

A C++ class (e.g., `FMcpSession`) will manage the state of each connected client. The `Mcp-Session-Id` HTTP header will be used for session management if the server assigns one during initialization.

*   **Initialization Phase (`initialize` method):**
    *   Handler for the `initialize` JSON-RPC method (sent via HTTP POST).
    *   Input: `FInitializeParams` USTRUCT (containing `protocolVersion`, `clientInfo`, `capabilities`).
    *   Logic:
        *   Version negotiation: Compare client `protocolVersion` with server's supported version (e.g., "2025-03-26").
        *   Capability negotiation: Store client capabilities and prepare server capabilities.
        *   The server MAY assign a session ID and include it in an `Mcp-Session-Id` header on the HTTP response.
    *   Output: `FInitializeResult` USTRUCT (containing `protocolVersion`, `serverInfo`, `serverCapabilities`).
    *   This response is sent in the HTTP response body (typically `Content-Type: application/json`) to the `initialize` POST request.
*   **`notifications/initialized` Notification:**
    *   Upon receiving this notification from the client (via HTTP POST), the session is marked as fully active. The server typically returns an HTTP 202 Accepted response.
*   **Operation Phase:**
    *   A dispatcher (e.g., a `TMap<FString, TFunction<void(TSharedPtr<FJsonRpcRequest>, TSharedRef<FMcpSession>)>>`) will route incoming JSON-RPC methods (from HTTP POST) to their respective handlers.
    *   Handlers will process requests. Responses are sent back in the HTTP POST response body (`application/json`) or via an SSE stream (`text/event-stream`) if initiated.
*   **Shutdown Phase:**
    *   The `shutdown` and `exit` MCP methods can be used by the client (via HTTP POST).
    *   Clients SHOULD send an HTTP DELETE request with the `Mcp-Session-Id` (if active) to explicitly terminate a session.
    *   Implement `$/cancelRequest` to handle client-side cancellation of long-running operations.

## 4. Streamable HTTP Transport Implementation

### 4.1. Server-Side Endpoint

A single base path (e.g., `/mcp`) will be used for all MCP communication.

*   **HTTP POST `/mcp`:**
    *   Clients send all MCP Request or Notification messages to the server using HTTP POST to this endpoint.
    *   Client MUST include `Accept: application/json, text/event-stream` header.
    *   The body of the POST request contains the JSON-RPC message string.
    *   The server processes the JSON-RPC message:
        *   If the POST body consists solely of JSON-RPC responses or notifications, and the server accepts the input, it MUST return an `HTTP 202 Accepted` with no body.
        *   If the POST body contains any JSON-RPC requests, the server MUST respond with either:
            *   `Content-Type: application/json`: The HTTP response body contains a single JSON-RPC Response.
            *   `Content-Type: text/event-stream`: The server initiates an SSE stream. This stream SHOULD eventually include one JSON-RPC response for each JSON-RPC request sent in the POST. The server MAY send other JSON-RPC requests or notifications to the client over this stream. **(Deferred for initial implementation)**
*   **HTTP GET `/mcp`:**
    *   Clients MAY issue an HTTP GET request to the MCP endpoint to open an SSE stream for server-initiated messages. **(Deferred for initial implementation)**
    *   Client MUST include `Accept: text/event-stream` header.
    *   The server MUST either respond with `Content-Type: text/event-stream` (establishing SSE) or `HTTP 405 Method Not Allowed` if it doesn't support GET for streaming.
*   **Session Management:** The server uses the `Mcp-Session-Id` HTTP header to manage sessions if implemented. If a client receives a session ID, it MUST include it on subsequent requests for that session.

### 4.2. Server-to-Client Message Delivery

*   MCP JSON-RPC **Responses** to client requests are delivered either as a single JSON object in the HTTP POST response body (`Content-Type: application/json`) or as individual messages within an SSE stream (`Content-Type: text/event-stream`) initiated in response to the POST. **(Initial implementation will only support `Content-Type: application/json` responses).**
*   Server-initiated MCP JSON-RPC **Requests** or **Notifications** (e.g., `notifications/tools/list_changed`, `$/progress`) can be sent over an active SSE stream. This stream might be initiated in response to a client's POST or GET request. **(Support for SSE streams, and thus these asynchronous messages, is deferred for the initial implementation).**

### 4.3. Security Considerations

*   **TLS (HTTPS):**
    *   MCP specification mandates TLS for production environments.
    *   **Decision:** For the initial implementation phases, TLS/HTTPS support will be **deferred** to simplify development. The server will operate over HTTP.
    *   Implementing HTTPS with `FHttpServerModule` might require engine modifications or be non-trivial.
    *   If using a third-party library, ensure it supports SSL/TLS (e.g., OpenSSL integration) for future implementation.
    *   **Warning:** Operating without TLS is insecure for production or sensitive data. This approach is for local development and testing only. Future work must address TLS implementation before any deployment outside of a trusted local environment.
*   **Origin Header Validation:** Servers MUST validate the `Origin` header on all incoming HTTP requests.
*   **Localhost Binding:** For local-only servers, bind to `localhost` (127.0.0.1).
*   **Authentication/Authorization:** The MCP spec largely defers this. Initially, trust incoming requests. Future enhancements could include API keys. `Mcp-Session-Id` should be cryptographically secure if used.

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
        *   Execution must be on a worker thread if potentially long-running. Use `$/progress` for updates if streaming.
    *   Output: `FToolCallResult` (containing `contentType`, `content`). `ContentPart`s (text, image) need to be supported.
*   **`notifications/tools/list_changed`:**
    *   Sent when the set of available tools changes. Requires server capability `tools: { "listChanged": true }`. **(Deferred as it requires SSE)**.

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
    *   Requires server capability `resources: { "subscribe": true }`. **(Deferred as it requires SSE)**.
    *   Mechanisms like `FCoreUObjectDelegates::OnObjectModified` or asset-specific callbacks could trigger notifications.

### 5.3. Prompts

*   **`prompts/list`:** Returns available `FPromptDefinition`s.
*   **`prompts/get_template`:** Returns the template structure for a prompt.
*   **`prompts/render`:**
    *   Input: `FPromptParameters` (`uri`, `variables`).
    *   Renders the prompt template with provided variables.
    *   Output: `FRenderedPrompt` (containing `messages` array for LLM).

### 5.4. Utilities

*   **`ping`:** Simple request/response to check connectivity (standard JSON-RPC response).
*   **`$/progress`:** Server sends this notification for long-running tool calls. **(Deferred as it requires SSE)**.
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
    *   Set up chosen HTTP server (`FHttpServerModule` PoC) for Streamable HTTP.
    *   Implement JSON-RPC message parsing and serialization (core USTRUCTs).
    *   Implement `initialize` handshake (synchronous response) and `notifications/initialized` handling.
    *   Implement `ping` utility.
2.  **Phase 2: Basic Tool Implementation**
    *   Implement `tools/list` and `tools/call` (synchronous responses).
    *   Create 1-2 simple example C++ tools (e.g., a tool to log a message, a tool to query basic editor state).
    *   `notifications/tools/list_changed` **(Deferred, requires SSE)**.
3.  **Phase 3: Basic Resource Implementation**
    *   Implement `resources/list` and `resources/get_content` (synchronous responses).
    *   Implement `get_blueprint_contents` as a resource.
    *   `notifications/resources/content_changed` **(Deferred, requires SSE)**.
4.  **Phase 4: Advanced Features & Refinements (Non-SSE)**
    *   Implement Prompts.
    *   Implement `$/cancelRequest`.
    *   Robust error handling and logging.
    *   Threading improvements and performance testing.
    *   Resource subscriptions (`resources/subscribe`) and `$/progress` **(Deferred, requires SSE)**.
5.  **Phase 5: Security & Deployment**
    *   Implement TLS (HTTPS).
    *   Configuration options.

## 10. Open Questions & Risks

*   **`FHttpServerModule` Suitability:** Is it robust enough for production use, especially for Streamable HTTP?
*   **HTTPS Implementation:** Complexity of integrating TLS, especially if `FHttpServerModule` doesn't support it easily.
*   **Session Management without SSE:** How to effectively manage client-specific state (like negotiated capabilities) across stateless HTTP POST requests if no client identifier is consistently passed or managed.
*   **Performance:** Impact of JSON processing and network traffic on UE performance, especially with many connected clients or large data transfers.
*   **Schema Evolution:** Keeping USTRUCTs in sync with official MCP schema updates.

## 11. Future Considerations (Post-MVP)

*   Implement SSE components of Streamable HTTP (including `Content-Type: text/event-stream` responses, client GET support for server-initiated streams, and features like `$/progress`, `notifications/tools/list_changed`, `notifications/resources/content_changed`, and `resources/subscribe`).
*   Server-initiated Sampling (`sampling/createMessage`).
*   Client-exposed Roots (`roots/list`).
*   Structured output from tools (beyond simple text/binary).
*   More comprehensive authentication/authorization.
