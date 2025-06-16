# Agent Notes for UnrealMCPServer Plugin

## Project Overview

This project aims to create an Unreal Engine plugin (`UnrealMCPServer`) that functions as a Model Context Protocol (MCP) server. The server will expose Unreal Engine capabilities (assets, editor functions, game state) to external AI models and applications.

**Initial Approach (Superseded):** The first conceptualization of this project (see memory `584aa543-75aa-4a61-b75b-669eb9cad70e`) involved an MCP server operating via a raw TCP socket (port 30069) for JSON commands.

**Current Approach (HTTP/SSE):** Based on further research into standard MCP implementations (`.cursor/notes/MCP_Server_Implementation_Research.md`), the project will now implement the MCP server using **HTTP with Server-Sent Events (SSE)** for communication. This aligns better with common MCP practices.

## Key Documentation

*   **Technical Specification (Current):** `UnrealMCPServer_MCP_HTTP_Implementation_TechSpec.md` - This is the primary design document detailing the HTTP/SSE based MCP server architecture and implementation plan.
*   **Project Checklist:** `project_checklist.md` - Tracks the progress of tasks based on the HTTP/SSE technical specification.
*   **Search Blueprints Tool Specification:** `search_blueprints_tool_techspec.md` - Detailed technical specification for the new Blueprint search capability.
*   **MCP Research:** `MCP_Server_Implementation_Research.md` - Comprehensive notes on MCP principles, JSON-RPC, HTTP/SSE transport, core features (Tools, Resources, Prompts), and security.
*   **Notebook:** `notebook.md` - General notes, findings, and ideas during development.
*   **Old Technical Specification (Superseded):** `UnrealMCPServer_TechSpec.md` - This document detailed the older TCP-based approach and is now outdated.

## Current Project Status

### Phase 1 & 2: Core Framework ✅ COMPLETED
- HTTP server with JSON-RPC handling
- Basic MCP protocol implementation (initialize, ping, notifications)
- Tools framework with `tools/list` and `tools/call` methods
- Example tool: `export_blueprint_to_t3d`

### Phase 2B: Blueprint Search Tool 🔄 IN PROGRESS
**Next Major Feature:** `search_blueprints` tool implementation

**Current Status:** ✅ **Phase 1 COMPLETED** - Basic Asset Discovery
- ✅ Implemented Blueprint search by name pattern matching
- ✅ Implemented Blueprint search by parent class inheritance  
- ✅ Added package path filtering and recursive search
- ✅ Created comprehensive JSON input/output schema
- ✅ Added tool registration to MCP server

**Implementation Details:**
- Added AssetRegistry and BlueprintGraph module dependencies
- Created `SearchBlueprints()` function in UMCP_CommonTools
- Uses Unreal's Asset Registry API for fast metadata-based searches
- Returns structured JSON results with match details and context
- Supports three search types: "name", "parent_class", and "all"

**Next Phase:** Content Analysis within Blueprint graphs (variables, functions, asset references)

## Core Technical Details (HTTP/SSE Implementation)

*   **Protocol:** Model Context Protocol (MCP) using JSON-RPC 2.0 messages.
*   **Transport:** HTTP with Server-Sent Events (SSE).
    *   Client-to-Server: HTTP POST requests (for MCP Requests/Notifications).
    *   Server-to-Client: HTTP GET request establishes an SSE stream (for MCP Responses/Notifications).
*   **Primary MCP Capabilities to Expose:**
    *   Tools: Callable functions within Unreal Engine.
    *   Resources: Access to Unreal Engine assets and data (e.g., Blueprint T3D, texture data).
    *   Prompts: Templated interactions for LLMs.
*   **Unreal Engine Integration:**
    *   Implemented as an Unreal Engine Plugin (`UnrealMCPServer`).
    *   Utilize `FHttpServerModule` (initially) or a third-party C++ HTTP library.
    *   Leverage Unreal's JSON parsing utilities (`FJsonObjectConverter`, `TJsonReader`/`Writer`).
    *   Define MCP objects as `USTRUCT`s.
    *   Manage threading carefully to avoid blocking the game thread.

## Development Approach

*   Follow the phased implementation plan in `UnrealMCPServer_MCP_HTTP_Implementation_TechSpec.md` and `project_checklist.md`.
*   **CURRENT PRIORITY:** Implement the `search_blueprints` tool as defined in `search_blueprints_tool_techspec.md`.
*   Prioritize establishing core functionality before advanced features.
*   Incrementally add MCP features (Tools, Resources, Prompts).
*   Focus on robust error handling and logging throughout.
*   Address security considerations (TLS/HTTPS) as a key part of later phases.

## Blueprint Search Tool Implementation Strategy

**Phase 1 (High Priority):** Basic asset discovery using AssetRegistry API
- Find Blueprints by name pattern matching
- Find Blueprint subclasses by parent class
- Package path filtering and recursive search

**Phase 2 (Medium Priority):** Content analysis within Blueprint graphs
- Variable reference search
- Function call reference search  
- Asset reference search
- String literal search within nodes

**Phase 3 (Lower Priority):** Advanced features
- Cross-Blueprint dependency analysis
- Regex pattern support
- Performance optimizations and caching

## Key Unreal Engine APIs for Blueprint Search

**Asset Discovery:**
- `FAssetRegistryModule::Get().GetAssets()` - Find assets by filters
- `FAssetRegistryModule::Get().GetDerivedClassNames()` - Find subclasses
- `UAssetRegistryHelpers::GetBlueprintAssets()` - UE5+ Blueprint-specific search

**Content Analysis:**
- `UBlueprint::GetBlueprintGraph()` - Access Blueprint graphs
- `UEdGraph::GetAllNodesOfClass()` - Find specific node types
- Asset tagging system for metadata searches

## Agent Reminders

*   Always refer to `UnrealMCPServer_MCP_HTTP_Implementation_TechSpec.md` for the current design.
*   Reference `search_blueprints_tool_techspec.md` for Blueprint search implementation details.
*   Ensure changes and progress are reflected in `project_checklist.md`.
*   Keep `notebook.md` updated with any new findings or important details encountered during development.
*   Adhere to user-defined rules, especially regarding code organization, testing, and documentation.
*   **IMMEDIATE FOCUS:** Begin implementation of Phase 1 of the `search_blueprints` tool - basic asset discovery functionality.
