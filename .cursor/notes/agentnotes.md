# Agent Notes for HiRezMCPUnreal

This document contains critical information for future agent sessions, including user preferences, project structure, and approach guidance.

## Project Overview

- **Project Name:** HiRezMCPUnreal
- **Goal:** Create an Unreal Engine Editor Plugin that exposes a Model Context Protocol (MCP) server.
- **MCP Spec:** [https://modelcontextprotocol.io/specification/2025-03-26](https://modelcontextprotocol.io/specification/2025-03-26)
- **Server Details:** HTTP on port 30069, no authentication, no streaming, no session support (initially).
- **Unreal Module:** `FHttpServerModule`

## User Preferences

- Follow the documentation and planning rules outlined in global and project-specific agent rules.
- Create detailed technical specifications and action plans before implementation.
- Update `project_checklist.md` regularly.
- Seek approval for plans before coding.

## Project Structure Guidance

- The plugin will be named `HiRezMCPUnreal`.
- Core functionality will revolve around Unreal's `FHttpServerModule`.
- MCP message handling will be a key component.

## Pointers to Other Documentation

- `project_checklist.md`: Main task checklist.
- `notebook.md`: Ongoing findings and useful information.
- `HiRezMCPUnreal_TechSpec.md`: Detailed technical specification for the plugin.
- `.cursor/rules/`: Agent operational rules.
