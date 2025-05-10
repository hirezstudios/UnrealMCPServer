# Agent Notes for HiRezMCPUnreal Plugin

## Project Overview

This project aims to create an Unreal Engine plugin (`HiRezMCPUnreal`) that functions as a Model Context Protocol (MCP) server. The server will expose Unreal Engine capabilities (assets, editor functions, game state) to external AI models and applications.

**Initial Approach (Superseded):** The first conceptualization of this project (see memory `584aa543-75aa-4a61-b75b-669eb9cad70e`) involved an MCP server operating via a raw TCP socket (port 30069) for JSON commands.

**Current Approach (HTTP/SSE):** Based on further research into standard MCP implementations (`.cursor/notes/MCP_Server_Implementation_Research.md`), the project will now implement the MCP server using **HTTP with Server-Sent Events (SSE)** for communication. This aligns better with common MCP practices.

## Key Documentation

*   **Technical Specification (Current):** `HiRezMCPUnreal_MCP_HTTP_Implementation_TechSpec.md` - This is the primary design document detailing the HTTP/SSE based MCP server architecture and implementation plan.
*   **Project Checklist:** `project_checklist.md` - Tracks the progress of tasks based on the HTTP/SSE technical specification.
*   **MCP Research:** `MCP_Server_Implementation_Research.md` - Comprehensive notes on MCP principles, JSON-RPC, HTTP/SSE transport, core features (Tools, Resources, Prompts), and security.
*   **Notebook:** `notebook.md` - General notes, findings, and ideas during development.
*   **Old Technical Specification (Superseded):** `HiRezMCPUnreal_TechSpec.md` - This document detailed the older TCP-based approach and is now outdated.

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
    *   Implemented as an Unreal Engine Plugin (`HiRezMCPUnreal`).
    *   Utilize `FHttpServerModule` (initially) or a third-party C++ HTTP library.
    *   Leverage Unreal's JSON parsing utilities (`FJsonObjectConverter`, `TJsonReader`/`Writer`).
    *   Define MCP objects as `USTRUCT`s.
    *   Manage threading carefully to avoid blocking the game thread.

## Development Approach

*   Follow the phased implementation plan in `HiRezMCPUnreal_MCP_HTTP_Implementation_TechSpec.md` and `project_checklist.md`.
*   Prioritize establishing the core HTTP/SSE communication and JSON-RPC handling (Phase 1).
*   Incrementally add MCP features (Tools, Resources, Prompts).
*   Focus on robust error handling and logging throughout.
*   Address security considerations (TLS/HTTPS) as a key part of later phases.

## Python Client (`hirez_mcp_client`)

A corresponding Python client library (`hirez_mcp_client`) will be developed to interact with the `HiRezMCPUnreal` server. This client will also need to implement HTTP/SSE communication to be compatible with the server.

## Agent Reminders

*   Always refer to `HiRezMCPUnreal_MCP_HTTP_Implementation_TechSpec.md` for the current design.
*   Ensure changes and progress are reflected in `project_checklist.md`.
*   Keep `notebook.md` updated with any new findings or important details encountered during development.
*   Adhere to user-defined rules, especially regarding code organization, testing, and documentation.
