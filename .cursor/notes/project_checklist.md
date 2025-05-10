# Project Checklist: HiRezMCPUnreal - MCP Server (HTTP Implementation)

**Overall Goal:** Create an Unreal Engine plugin (`HiRezMCPUnreal`) that implements a Model Context Protocol (MCP) server using synchronous HTTP POST for requests/responses, exposing UE capabilities.

**Note:** Server-Sent Events (SSE) and server-initiated notifications are **deferred** for initial implementation. Communication will be client-initiated request (POST) and server response (in HTTP response body).

---

## Phase 1: Core HTTP Server & JSON-RPC Framework
- [X] **Task 1.1:** Set up the chosen HTTP server (e.g., `FHttpServerModule` PoC) to handle synchronous POST requests.
    - [X] Sub-task: Configure a listening port (e.g., 30069) for HTTP.
    - [X] Sub-task: Implement basic request routing for a `/mcp` endpoint.
- [X] **Task 1.2:** Implement core JSON-RPC message parsing and serialization.
    - [X] Sub-task: Define USTRUCTs for `FJsonRpcRequest`, `FJsonRpcResponse`, `FJsonRpcError`.
    - [X] Sub-task: Implement functions to convert between JSON strings and these USTRUCTs.
- [X] **Task 1.3:** Implement the `initialize` MCP method (synchronous request/response).
    - [X] Sub-task: Define `FInitializeParams` and `FInitializeResult` USTRUCTs.
    - [X] Sub-task: Define `FServerInfo` and `FServerCapabilities` USTRUCTs.
    - [X] Sub-task: Handle client `initialize` POST request and return `FInitializeResult` in HTTP response.
- [X] **Task 1.4:** Implement handling for the `notifications/initialized` MCP client notification.
    - [X] Sub-task: Server receives POST, processes, returns HTTP 204 or simple success.
- [X] **Task 1.5:** Implement a basic session/state management concept (even if single-client focused for now).
    - [X] Sub-task: Store negotiated capabilities after `initialize`.
- [X] **Task 1.6:** Implement the `ping` utility MCP method (synchronous request/response).
- [X] **Task 1.7:** Basic logging setup (e.g., `LogHiRezMCP` category).
- [X] **Status:** Completed

## Phase 2: Basic Tool Implementation
- [ ] **Task 2.1:** Implement `tools/list` JSON-RPC method (synchronous response).
    - [ ] Sub-task: Define `FToolDefinition` USTRUCT.
    - [ ] Sub-task: Implement tool discovery/registration mechanism (e.g., manual registration of C++ tools).
- [ ] **Task 2.2:** Implement `tools/call` JSON-RPC method (synchronous response).
    - [ ] Sub-task: Define `FToolCallParams` and `FToolCallResult` USTRUCTs.
    *   [ ] Sub-task: Define `FContentPart` USTRUCTs (Text, Image initially).
    - [ ] Sub-task: Implement tool execution logic (C++ focused, callable from MCP client via POST).
    - [ ] Sub-task: Ensure tool execution is asynchronous on a worker thread for long-running tasks, with the final result returned in the HTTP response.
- [ ] **Task 2.3:** Create 1-2 simple example C++ tools.
    - [ ] Example 1: C++ tool (e.g., "LogMessageToUnrealConsole").
    - [ ] Example 2: C++ tool (e.g., "GetBasicEditorInfo").
- [ ] ~~**Task 2.4:** Implement `notifications/tools/list_changed` server notification.~~ **(Deferred due to no SSE)**
- [ ] **Status:** Not Started

## Phase 3: Basic Resource Implementation
- [ ] **Task 3.1:** Implement `resources/list` JSON-RPC method (synchronous response).
    - [ ] Sub-task: Define `FResourceDefinition` USTRUCT.
    - [ ] Sub-task: Implement resource discovery/registration mechanism.
- [ ] **Task 3.2:** Implement `resources/get_content` JSON-RPC method (synchronous response).
    - [ ] Sub-task: Define `FResourceContentParams` and `FResourceContentResult` USTRUCTs.
    - [ ] Sub-task: Implement logic to retrieve resource content (e.g., T3D for a Blueprint).
- [ ] **Task 3.3:** Implement `get_blueprint_contents` as a specific resource type or via `resources/get_content`.
- [ ] ~~**Task 3.4:** Implement `notifications/resources/list_changed` server notification.~~ **(Deferred due to no SSE)**
- [ ] **Status:** Not Started

## Phase 4: Advanced Features & Refinements
- [ ] **Task 4.1:** Implement Prompts (`prompts/list`, `prompts/get_template`, `prompts/render`) (synchronous responses).
    - [ ] Sub-task: Define `FPromptDefinition`, `FPromptTemplate`, `FPromptRenderParams`, `FPromptRenderResult` USTRUCTs.
- [ ] ~~**Task 4.2:** Implement resource subscriptions (`resources/subscribe`, `notifications/resources/content_changed`).~~ **(Deferred due to no SSE)**
- [ ] ~~**Task 4.3:** Implement `$/progress` server notification for long-running tool calls.~~ **(Deferred due to no SSE)**
- [ ] **Task 4.4:** Implement `$/cancelRequest` client notification.
    - [ ] Sub-task: Server attempts to honor cancellation for ongoing asynchronous tasks.
- [ ] **Task 4.5:** Robust error handling and reporting as JSON-RPC errors in HTTP responses.
- [ ] **Task 4.6:** Threading improvements and performance testing (focus on synchronous request handling).
- [ ] **Status:** Not Started

## Phase 5: Security & Deployment
- [ ] **Task 5.1:** Implement TLS (HTTPS) for secure communication. **(Deferred for initial implementation; HTTP will be used locally)**
    - [ ] Sub-task: Evaluate `FHttpServerModule` for HTTPS or select/integrate third-party library (for future implementation).
- [ ] **Task 5.2:** Implement configuration options (port, features via INI/Editor Settings).
- [ ] **Task 5.3:** Plugin packaging and end-to-end testing with a sample MCP client.
- [ ] **Task 5.4:** Documentation for using the plugin.
- [ ] **Status:** Not Started

## Future Considerations (Post-Initial Implementation)
- [ ] Implement Server-Sent Events (SSE) to enable server-initiated notifications and related capabilities (`tools/list_changed`, `resources/content_changed`, `$/progress`, resource subscriptions).
- [ ] Implement Server-initiated Sampling (`sampling/createMessage`).
- [ ] Implement Client-exposed Roots (`roots/list`).
- [ ] Explore advanced tool output (structured content beyond text/image).
- [ ] Investigate more robust session management for multi-client scenarios.
- [ ] Performance optimizations for high-throughput scenarios.