# Project Checklist: HiRezMCPUnreal Plugin

## Phase 1: Project Setup & Core Infrastructure (Current Focus)

-   [x] Initialize `.cursor` documentation folder structure (`agentnotes.md`, `notebook.md`, `project_checklist.md`, `HiRezMCPUnreal_TechSpec.md`).
-   [ ] **Planning & Design:**
    -   [x] Draft initial `HiRezMCPUnreal_TechSpec.md` with class responsibilities and initial MCP endpoints.
    -   [x] Draft initial `project_checklist.md`.
    -   [x] **USER REVIEW:** Get approval on `HiRezMCPUnreal_TechSpec.md` and `project_checklist.md`.
-   [ ] **Unreal Plugin Module Setup:**
    -   [x] Create a new Unreal Engine Editor Plugin named `HiRezMCPUnreal` (conceptual, files created manually/scripted).
    -   [x] Define `HiRezMCPUnreal.uplugin` file.
    -   [x] Create the main module class `FHiRezMCPUnrealModule` (header `HiRezMCPUnreal.h` and source `HiRezMCPUnreal.cpp` created).
    -   [x] Implement basic `StartupModule` and `ShutdownModule` logic.
    -   [x] Add `HTTPServer` module as a dependency (in `.uplugin` and `.Build.cs`).
-   [ ] **HTTP Server Implementation:**
    -   [x] Initialize and start the HTTP server on port 30069 in `StartupModule` (with fallback to ephemeral port).
    -   [x] Stop the HTTP server in `ShutdownModule`.
    -   [x] Register a basic `/health` endpoint (and other initial MCP routes).
    -   [ ] Test the `/health` endpoint using a tool like `curl` or Postman.
-   [ ] **MCP Data Structures:**
    -   [ ] Define USTRUCTs for basic MCP request/response messages (e.g., `FMCPErrorResponse`, `FMCPHealthResponse`).
    -   [ ] Implement JSON serialization/deserialization helpers for these structs.

## Phase 2: Initial MCP Endpoint Implementation

-   [ ] **Basic Endpoint: `/-/list-resources`**
    -   [ ] Define USTRUCTs for `FMCPListResourcesRequest` and `FMCPListResourcesResponse`, including `FMCPResourceDescriptor`.
    -   [ ] Implement the request handler in `FHiRezMCPRequestHandler` (or dedicated handler class).
    -   [ ] Bind the `/-/list-resources` route in `FHiRezMCPUnrealModule` to its handler.
    -   [ ] Initially, make it return a static list of example resource types (e.g., "actors", "assets").
    -   [ ] Test the endpoint.
-   [ ] **Basic Endpoint: `/-/read-resource`**
    -   [ ] Define USTRUCTs for `FMCPReadResourceRequest` and `FMCPReadResourceResponse`.
    -   [ ] Implement the request handler in `FHiRezMCPRequestHandler` (or dedicated handler class).
    -   [ ] Bind the `/-/read-resource` route in `FHiRezMCPUnrealModule` to its handler.
    -   [ ] Initially, make it return a static response for a known example resource URI.
    -   [ ] Test the endpoint.
-   [ ] **General MCP Endpoint: `POST /mcp` (Path TBD)**
    -   [ ] Implement base request handler logic for `/mcp` that parses the JSON-RPC `method` field.
    -   [ ] Design and implement a dispatch mechanism to route to specific MCP method handlers based on the `method` field.
    -   [ ] Bind the `/mcp` route in `FHiRezMCPUnrealModule` to this general handler/dispatcher.
    -   [ ] **MCP Method: `initialize`**
        -   [ ] Define USTRUCTs for `FMCPInitializeRequest` and `FMCPInitializeResult`.
        -   [ ] Implement handler logic for `initialize`.
        -   [ ] Test `initialize` method via `/mcp`.
    -   [ ] **MCP Method: `initialized` (Notification)**
        -   [ ] Implement handler logic for `initialized` notification (may just log or acknowledge).
        -   [ ] Test `initialized` method via `/mcp`.
    -   [ ] **MCP Method: `shutdown`**
        -   [ ] Implement handler logic for `shutdown`.
        -   [ ] Test `shutdown` method via `/mcp`.
    -   [ ] **MCP Method: `exit` (Notification)**
        -   [ ] Implement handler logic for `exit` notification.
        -   [ ] Test `exit` method via `/mcp`.
    -   [ ] **MCP Method: `ping` (JSON-RPC based)**
        -   [ ] Define USTRUCTs for `FMCPPingRequest` and `FMCPPongResponse` (or use generic empty result).
        -   [ ] Implement handler logic for `ping`.
        -   [ ] Test `ping` method via `/mcp`.
    -   [ ] **MCP Method: `tools/list`**
        -   [ ] Define USTRUCTs for `FMCPListToolsRequest`, `FMCPListToolsResponse`, and `FMCPToolDescriptor`.
        -   [ ] Implement handler logic for `tools/list`.
        -   [ ] Initially, return a static list of example tools.
        -   [ ] Test `tools/list` method via `/mcp`.
    -   [ ] **MCP Method: `tools/call`**
        -   [ ] Define USTRUCTs for `FMCPCallToolRequest` and `FMCPCallToolResponse`.
        -   [ ] Implement handler logic for `tools/call`.
        -   [ ] Initially, support a simple static test tool.
        -   [ ] Test `tools/call` method via `/mcp`.

## Phase 3: Basic Resource Interaction

-   [ ] **List Actors Resource:**
    -   [ ] Enhance `/-/list-resources` to identify "actors" as a resource type.
    -   [ ] Implement logic in `/-/read-resource` (or a dedicated endpoint if preferred by MCP spec for collections) to list actors in the current editor level.
        -   Each actor should have a URI (e.g., `unreal://actors/{ActorNameOrID}`).
    -   [ ] Test listing actors.
-   [ ] **Read Actor Details Resource:**
    -   [ ] Implement logic in `/-/read-resource` to fetch basic details for an actor specified by its URI (e.g., name, class, location).
    -   [ ] Test reading actor details.

## Phase 4: Further Development & Refinement

-   [ ] Implement more resource types as needed (e.g., assets, editor settings).
-   [ ] Implement `/-/create-resource`, `/-/update-resource`, `/-/delete-resource` if/when required.
-   [ ] Add comprehensive error handling and logging.
-   [ ] Create unit and integration tests.
-   [ ] Documentation within the code.
-   [ ] Consider security implications if moving beyond local-only.

---
*This checklist will be up