# Notebook for HiRezMCPUnreal

This notebook is for recording potentially useful information, findings, and tidbits related to the HiRezMCPUnreal plugin project.

## Initial Thoughts & Research

- **MCP Specification:** Review [https://modelcontextprotocol.io/specification/2025-03-26](https://modelcontextprotocol.io/specification/2025-03-26) thoroughly for endpoint definitions and message structures.
- **Unreal FHttpServerModule:** Investigate usage examples and best practices for `FHttpServerModule` in Unreal Engine.
  - Key classes: `IHttpRouter`, `FHttpRouteHandle`, `FHttpServerRequest`, `FHttpServerResponse`.
- **JSON Parsing in Unreal:** Unreal uses `FJsonObject` and related classes for JSON manipulation. This will be crucial for MCP message processing.
- **Port:** 30069 (as specified by user).
- **Plugin Type:** Editor Plugin.
